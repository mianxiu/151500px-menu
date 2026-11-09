import asyncio
import json
import os
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch

from backend.uxp_bridge import UxpBridge, UxpResultUnknown, publish_request
from backend.uxp_recovery import PendingUxp


class RecoveryTests(unittest.IsolatedAsyncioTestCase):
    async def test_frame_exit_requires_original_modal_receipt(self):
        pending=self.bridge.transport.pending
        pending.begin({'id':'a'*32,'session':'one','operation':'set_label','command':'layer.frame'},self.folder,'b'*32)
        self.assertFalse(pending.execution_has_exited())
        self.receipt(probe_version='0.3.98',execution_exit={'schema':1,'engine':'uxp-modal','exited':True},result_unknown=True)
        self.assertTrue(pending.execution_has_exited())
        self.assertIsNotNone(pending.value)
        self.assertIsNone(pending.read_result())
        pending.clear()

    async def test_thumbnail_receipt_limit_is_bounded_and_does_not_expand_other_commands(self):
        pending=self.bridge.transport.pending
        request={'id':'a'*32,'session':'one','operation':'set_label','command':'layer.thumbnail'}
        pending.begin(request,self.folder,'b'*32)
        self.receipt(result={'available':True,'width':128,'height':128,'pixels':[255]*65536})
        self.assertTrue(pending.read_result()['result']['available'])
        restored=PendingUxp(pending.path,pending.storage);self.assertIs(restored.value['thumbnail'],True)
        self.receipt(result={'pixels':[255]*150000});self.assertIsNone(pending.read_result())
        pending.clear()
        pending.begin({**request,'command':'label'},self.folder,'c'*32)
        self.receipt(result={'pixels':[255]*65536});self.assertIsNone(pending.read_result())
        with self.assertRaises(ValueError):pending.validate({**pending.value,'thumbnail':False})
        pending.clear()

    async def test_direct_caps_late_receipt_requires_bound_target_before_releasing_busy(self):
        self.heartbeat()
        heartbeat=json.loads((self.folder/'probe-last.json').read_text('utf-8'));heartbeat['probe_version']='0.3.34'
        (self.folder/'probe-last.json').write_text(json.dumps(heartbeat),'utf-8')
        with patch('backend.uxp_bridge.publish_request',wraps=publish_request) as publish:
            with self.assertRaises(UxpResultUnknown):await self.bridge.execute('selectAbove',capture_current=True)
            task=self.bridge.tasks.snapshot()['items'][0]['id'];original=(self.folder/'probe-request.json').read_bytes()
            self.assertTrue(self.bridge.transport.pending.value['capture_current'])
            self.receipt()
            self.assertTrue((await self.bridge.reconcile_result())['active'])
            self.assertEqual(self.bridge.tasks.get(task)['state'],'result-unknown')
            for target in ({'session':'other','document_id':1,'active_layer_id':2,'layer_ids':[2]},
                           {'session':'one','document_id':1,'active_layer_id':2,'layer_ids':[3]}):
                self.receipt(captured_target=target);self.assertTrue((await self.bridge.reconcile_result())['active'])
            self.receipt(captured_target={'session':'one','document_id':1,'active_layer_id':2,'layer_ids':[2]})
            self.assertFalse((await self.bridge.reconcile_result())['active'])
            self.assertEqual(self.bridge.tasks.get(task)['target']['layer_ids'],[2])
            self.assertEqual(self.bridge.tasks.get(task)['state'],'succeeded')
            self.assertEqual(publish.call_count,1);self.assertEqual(original,(self.folder/'probe-request.json').read_bytes())

    async def asyncSetUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.env = patch.dict(os.environ, {'APPDATA':str(self.root)})
        self.env.start()
        self.folder = self.root/'Adobe/UXP/PluginsStorage/PHSP/24/External/com.15100px.menu.offline-probe/PluginData'
        self.folder.mkdir(parents=True)
        self.heartbeat()
        self.config = self.root/'config.json'
        self.config.write_text(json.dumps({'token':'private-token'}))
        self.bridge = self.make_bridge()

    def make_bridge(self):
        bridge = UxpBridge(self.root/'tasks.json', self.config)
        bridge.operation_timeout = 1
        bridge.transport.response_timeout = .025
        def capture():
            bridge._captured_target={'session':'one','document_id':1,'active_layer_id':2,'layer_ids':[2]}
            return dict(bridge._captured_target)
        bridge._capture_target = capture
        return bridge

    def heartbeat(self, session='one', age=0):
        (self.folder/'probe-last.json').write_text(json.dumps({'probe_version':'0.3.8','session':session,'photoshop_version':'24.0.1','plugin_time_ms':time.time()*1000-age}))

    async def asyncTearDown(self):
        await self.bridge.close()
        self.env.stop()
        self.temp.cleanup()

    async def unknown(self):
        with self.assertRaises(UxpResultUnknown):await self.bridge.execute('label.red')
        await asyncio.sleep(0)
        self.assertTrue(self.bridge.busy)
        return self.bridge.tasks.snapshot()['items'][0]['id']

    async def test_pending_session_freshness_is_metadata_only_and_never_clears_unknown(self):
        await self.unknown()
        pending=self.bridge.transport.pending;original=pending.path.read_bytes()
        for session,age,expected in [('one',0,True),('other',0,False),('one',16000,False)]:
            self.heartbeat(session,age)
            self.assertIs(pending.snapshot()['session_current'],expected)
            self.assertEqual(pending.path.read_bytes(),original)
            self.assertTrue(self.bridge.busy)
        (self.folder/'probe-last.json').write_text('{')
        self.assertFalse(pending.snapshot()['session_current'])

    def receipt(self, **changes):
        pending = self.bridge.transport.pending.value
        data = {key:pending[key] for key in ('id','session','operation')}
        data.update(schema=1,phase='completed',result={'count':1,'verified':True})
        data.update(changes)
        (self.folder/('result-'+pending['id']+'.json')).write_text(json.dumps(data))

    async def test_late_success_updates_same_task_without_republication(self):
        with patch('backend.uxp_bridge.publish_request',wraps=publish_request) as publish:
            task = await self.unknown()
            original = (self.folder/'probe-request.json').read_bytes()
            for _ in range(3):
                self.assertTrue((await self.bridge.reconcile_result())['active'])
            with self.assertRaises(RuntimeError):await self.bridge.execute('label.blue')
            self.receipt()
            result = await self.bridge.reconcile_result()
            self.assertFalse(result['active']);self.assertFalse(self.bridge.busy)
            self.assertEqual(self.bridge.tasks.get(task)['state'],'succeeded')
            self.assertTrue(self.bridge.tasks.get(task)['reconciled'])
            self.assertEqual(len(self.bridge.tasks.records),1)
            self.assertEqual(publish.call_count,1)
            self.assertEqual((self.folder/'probe-request.json').read_bytes(),original)
            self.assertFalse(self.bridge.transport.pending.path.exists())

    async def test_restart_then_final_failure_is_confirmed_without_execution(self):
        task = await self.unknown()
        self.receipt(phase='failed',error='target changed')
        await self.bridge.close()
        self.bridge = self.make_bridge()
        self.assertTrue(self.bridge.busy)
        self.assertTrue(self.bridge.tasks.get(task)['restored'])
        with patch.object(self.bridge.transport,'call',side_effect=AssertionError('No request during recovery')):
            await self.bridge.reconcile_result()
        self.assertFalse(self.bridge.busy)
        self.assertEqual(self.bridge.tasks.get(task)['state'],'failed')
        self.assertEqual(self.bridge.tasks.get(task)['error'],'target changed')
        self.assertTrue(self.bridge.tasks.get(task)['restored'])
        with self.assertRaises(ValueError):self.bridge.tasks.update(task,'running')

    async def test_session_change_and_stale_heartbeat_do_not_clear_unknown(self):
        task = await self.unknown()
        self.heartbeat('two')
        self.assertIn('会话已改变',self.bridge._status_snapshot()['uxp_recovery']['message'])
        await self.bridge.reconcile_result();self.assertTrue(self.bridge.busy)
        self.heartbeat(age=30000)
        self.assertIn('通讯中断',self.bridge._status_snapshot()['uxp_recovery']['message'])
        self.receipt(session='two')
        await self.bridge.reconcile_result()
        self.assertEqual(self.bridge.tasks.get(task)['state'],'result-unknown')
        # Original final receipt remains authoritative after the host restarts.
        self.receipt()
        await self.bridge.reconcile_result();self.assertFalse(self.bridge.busy)

    async def test_accepted_corrupt_wrong_id_and_unknown_failure_are_not_terminal_proof(self):
        task = await self.unknown()
        for changes in ({'phase':'accepted'},{'id':'f'*32},{'operation':'new_document'},
                        {'phase':'failed','error':'rollback uncertain','result_unknown':True}):
            self.receipt(**changes)
            await self.bridge.reconcile_result();self.assertTrue(self.bridge.busy)
        p=self.bridge.transport.pending.value
        (self.folder/('result-'+p['id']+'.json')).write_text('{')
        await self.bridge.reconcile_result();self.assertTrue(self.bridge.busy)
        (self.folder/('result-'+p['id']+'.json')).write_text('[]')
        await self.bridge.reconcile_result();self.assertTrue(self.bridge.busy)
        self.assertEqual(self.bridge.tasks.get(task)['state'],'result-unknown')

    async def test_legacy_rollback_failure_without_native_flag_remains_unknown(self):
        task=await self.unknown()
        for text in ('预览回滚结果待核对','Error: 预览回滚结果待核对'):
            self.receipt(phase='failed',error=text,result_unknown=False)
            pending=self.bridge.transport.pending
            original=(self.folder/('result-'+pending.value['id']+'.json')).read_bytes()
            self.assertIsNone(pending.read_result())
            self.assertTrue(pending.read_result(allow_unknown=True)['result_unknown'])
            self.assertTrue((await self.bridge.reconcile_result())['active'])
            self.assertTrue(self.bridge.busy)
            self.assertEqual(self.bridge.tasks.get(task)['state'],'result-unknown')
            self.assertEqual((self.folder/('result-'+pending.value['id']+'.json')).read_bytes(),original)

    async def test_misclassified_failed_history_cannot_clear_unknown_rollback_receipt(self):
        task=await self.unknown()
        self.receipt(phase='failed',error='Error: 预览回滚结果待核对',result_unknown=False)
        self.bridge.tasks.update(task,'failed',error='Error: 预览回滚结果待核对')
        self.assertTrue((await self.bridge.reconcile_result())['active'])
        self.assertTrue(self.bridge.busy)
        self.assertTrue(self.bridge.transport.pending.path.exists())

    async def test_other_known_failure_still_reconciles_normally(self):
        task=await self.unknown()
        self.receipt(phase='failed',error='Error: unsupported setter',result_unknown=False)
        await self.bridge.reconcile_result()
        self.assertFalse(self.bridge.busy)
        self.assertEqual(self.bridge.tasks.get(task)['state'],'failed')

    async def test_pending_contains_no_token_and_bad_journal_blocks_after_restart(self):
        await self.unknown()
        text=self.bridge.transport.pending.path.read_text()
        self.assertNotIn('private-token',text);self.assertNotIn('token',text)
        await self.bridge.close()
        self.bridge.transport.pending.path.write_text('broken')
        self.bridge=self.make_bridge()
        self.assertTrue(self.bridge.busy)
        self.assertEqual(self.bridge._status_snapshot()['uxp_recovery']['state'],'invalid')
        with self.assertRaises(RuntimeError):await self.bridge.execute('label.blue')

    async def test_failed_journal_write_prevents_request_publication(self):
        with patch.object(self.bridge.transport.pending,'begin',side_effect=OSError('disk full')):
            with self.assertRaises(RuntimeError):await self.bridge.execute('label.red')
        self.assertFalse((self.folder/'probe-request.json').exists())
        self.assertFalse(self.bridge.busy)

    async def test_live_worker_keeps_ownership_until_it_finishes(self):
        self.bridge.operation_timeout=.008
        self.bridge.transport.response_timeout=.2
        with self.assertRaises(RuntimeError):await self.bridge.execute('label.red')
        self.receipt()
        await self.bridge.reconcile_result()
        self.assertTrue(self.bridge.busy)
        await asyncio.sleep(.05)
        await self.bridge.reconcile_result()
        self.assertFalse(self.bridge.busy)
        self.assertEqual(self.bridge.tasks.snapshot()['items'][0]['state'],'succeeded')

    async def test_restored_missing_history_never_unblocks(self):
        await self.unknown()
        self.receipt()
        self.bridge.tasks.records.clear()
        result=await self.bridge.reconcile_result()
        self.assertIn('历史记录缺失',result['message']);self.assertTrue(self.bridge.busy)

    async def test_terminal_history_is_flushed_before_removing_pending(self):
        task=await self.unknown();self.receipt()
        flush=self.bridge.tasks.flush
        async def blocked():raise RuntimeError('history disk full')
        self.bridge.tasks.flush=blocked
        with self.assertRaises(RuntimeError):await self.bridge.reconcile_result()
        self.assertTrue(self.bridge.transport.pending.path.exists());self.assertTrue(self.bridge.busy)
        self.bridge.tasks.flush=flush
        await self.bridge.reconcile_result()
        self.assertFalse(self.bridge.busy)
        self.assertEqual(self.bridge.tasks.get(task)['state'],'succeeded')

    async def test_pending_path_outside_plugin_storage_is_rejected(self):
        await self.unknown()
        value=dict(self.bridge.transport.pending.value,folder=str(self.root/'elsewhere/PluginData'))
        self.bridge.transport.pending.path.write_text(json.dumps(value))
        restored=PendingUxp(self.bridge.transport.pending.path,self.bridge.transport.storage)
        self.assertTrue(restored.error);self.assertIsNone(restored.value)
