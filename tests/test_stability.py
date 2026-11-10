import asyncio
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock,patch

from backend.stability import StabilityTest,Stopped,summary
from backend.uxp_bridge import UxpBridge
from backend.commands import command_contracts


class StabilityTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.bridge=UxpBridge(self.root/'tasks.json',self.root/'uxp-config.json')
        self.app=SimpleNamespace(runtime=self.root,bridge=self.bridge,operation_lock=asyncio.Lock(),
            broadcast=AsyncMock(),require_uxp_enabled=lambda:None,desktop=None,photoshop_launcher=SimpleNamespace(active=False))
        self.test=StabilityTest(self.app)
    async def asyncTearDown(self):
        await self.bridge.close();self.tmp.cleanup()
    async def test_reserves_before_worker_and_rejects_second_start(self):
        gate=asyncio.Event()
        async def job():await gate.wait()
        with patch.object(self.test,'run',job):
            await self.test.start();self.assertTrue(self.app.operation_lock.locked())
            with self.assertRaisesRegex(RuntimeError,'不排队'):await self.test.start()
            self.assertEqual(len(list(self.test.folder.glob('*.json'))),2)
            self.test.stop();self.assertTrue(self.test.stop_requested)
            self.assertFalse(self.test.task.cancelled())
            gate.set();await self.test.task
            self.test.active=False;self.app.operation_lock.release()
    async def test_restore_is_interrupted_never_resumes(self):
        self.test.folder.mkdir();(self.test.folder/'latest.json').write_text(json.dumps({'schema':1,'phase':'running','run_id':'a'*32}),'utf-8')
        restored=StabilityTest(self.app)
        self.assertEqual(restored.snapshot()['phase'],'interrupted');self.assertFalse(restored.active);self.assertIsNone(restored.task)

    async def test_text_object_profile_reserves_existing_lock_without_external_scripts(self):
        gate=asyncio.Event()
        with patch.object(self.test,'run',gate.wait):
            result=await self.test.start('text-object')
            self.assertEqual(result['rounds'],2);self.assertTrue(self.app.operation_lock.locked())
            self.assertFalse(self.test.report['scope']['native_menu_lifecycle'])
            gate.set();await self.test.task
        self.test.active=False;self.app.operation_lock.release()

    async def test_text_object_native_unknown_and_retained_fixture_block_restarts(self):
        for record in ({'native_request':{'phase':'unknown'}},{'text_fixture':{'document_id':7}}):
            self.test.report=record
            with self.assertRaises(RuntimeError):await self.test.start('text-object')
            self.assertFalse(self.app.operation_lock.locked())

    async def test_native_color_unknown_quit_restore_and_owned_fixture_block_new_runs(self):
        for record in ({'normal_quit':{'phase':'submitted'}},{'normal_quit':{'phase':'unknown'}},
                       {'original_restore_pending':True},{'color_fixture':{'id':7}}):
            self.test.report=record
            with self.assertRaises(RuntimeError):await self.test.start('color-native')
            self.assertFalse(self.app.operation_lock.locked())
        from backend.main import Application
        for record in ({'original_restore_pending':True},{'normal_quit':{'phase':'unknown'}}):
            app=SimpleNamespace(updating=False,uxp=SimpleNamespace(enabled=True),stability=SimpleNamespace(report=record))
            with self.assertRaises(RuntimeError):Application.require_uxp_enabled(app)
            with self.assertRaises(RuntimeError):Application.require_update_idle(app)

    async def test_text_object_native_failure_preserves_unknown_without_cleanup(self):
        from backend.text_object_acceptance import run
        from unittest.mock import Mock
        self.test.report={'run_id':'a'*32,'round':0,'steps':[],'summary':{}}
        self.test.check=Mock();self.test.save=AsyncMock()
        with patch.object(self.bridge,'_application',side_effect=RuntimeError('COM result unknown')):
            with self.assertRaisesRegex(RuntimeError,'COM result unknown'):await run(self.test)
        self.assertEqual(self.test.report['native_request']['phase'],'unknown')
        self.assertTrue(self.test.report['text_fixture']['creation_unconfirmed'])

    async def test_text_object_native_explicit_rejection_is_a_returned_result(self):
        from backend.text_object_acceptance import run
        from unittest.mock import Mock
        import re
        self.test.report={'run_id':'a'*32,'round':0,'steps':[],'summary':{}}
        self.test.check=Mock();self.test.save=AsyncMock()
        def reject(script):
            identity=re.search(r'id:"([a-f0-9]{32})"',script)[1]
            self.assertNotIn('.jsx"',script);self.assertNotIn('.execute()',script)
            return json.dumps({'id':identity,'ok':False,'error':'Native modal edit active'})
        with patch.object(self.bridge,'_application',return_value=SimpleNamespace(DoJavaScript=reject)):
            with self.assertRaisesRegex(RuntimeError,'Native modal edit active'):await run(self.test)
        self.assertEqual(self.test.report['native_request']['phase'],'returned')

    def test_unknown_native_acceptance_blocks_other_host_operations(self):
        from backend.main import Application
        app=SimpleNamespace(updating=False,uxp=SimpleNamespace(enabled=True),
                            stability=SimpleNamespace(report={'native_request':{'phase':'unknown'}}))
        with self.assertRaisesRegex(RuntimeError,'退出仍未知'):Application.require_uxp_enabled(app)
        app.stability.report['native_request']['phase']='returned'
        Application.require_uxp_enabled(app)

    async def test_text_object_cleanup_uses_its_recorded_owner_and_returns_no_file_prompt(self):
        from backend.text_object_acceptance import cleanup
        self.test.report={'run_id':'a'*32,'round':1,'text_fixture':{'document_id':7,'documents':[], 'original':None,'tool':'moveTool'}}
        with patch('backend.text_object_acceptance.native',AsyncMock(return_value={'closed_owned_document':True,'original_active_restored':True})) as send:
            await cleanup(self.test)
        script=send.call_args.args[1]
        self.assertIn('app.activeDocument.id!==7',script)
        self.assertIn('151500px text acceptance '+'a'*32+' 72',script)
        self.assertNotIn('.execute()',script);self.assertNotIn('text_fixture',self.test.report)

    async def test_text_object_read_uses_normal_inspection_and_preserves_pending_guard(self):
        self.test.report={'profile':'text-object','run_id':'a'*32}
        with patch.object(self.bridge.transport,'call',return_value={'text':{}}) as call:
            await self.test.test_read(text_inspect=True)
        call.assert_called_once_with('inspect',text_inspect=True)
        self.assertFalse(self.test.report['read_request_pending'])

    def test_nonfixture_json_result_does_not_break_status_or_become_cleanup_owner(self):
        self.test.report={'cleanup':'retained-text-object-fixture','steps':[
            {'stage':'text-object-create','state':'passed','evidence':{'result':'{"verified":true}'}}]}
        self.assertIsNone(self.test.retained_fixture())
        self.assertFalse(self.test.snapshot()['cleanup_available'])

    async def test_native_preflight_publish_failure_proves_no_ps_call_was_started(self):
        from backend.text_object_acceptance import native
        from unittest.mock import Mock
        self.test.report={'run_id':'a'*32};self.test.check=Mock()
        self.test.save=AsyncMock(side_effect=RuntimeError('report publish failed'))
        with patch.object(self.bridge,'_application') as application:
            with self.assertRaisesRegex(RuntimeError,'report publish failed'):await native(self.test,'return true;')
        application.assert_not_called()
        result=json.loads((self.test.folder/'latest.json').read_text('utf-8'))['native_request']
        self.assertEqual(result['phase'],'not-started');self.assertTrue(result['execution_exit']['not_started'])
        self.assertEqual(result['execution_exit']['id'],result['id'])

    def retained(self):
        self.test.report={'schema':1,'run_id':'a'*32,'phase':'stopped','cleanup':'retained-for-inspection-no-automatic-cleanup',
            'original_request_pending':False,'steps':[{'stage':'fixture','state':'passed','evidence':{'result':{
                'verified':True,'target':{'session':'original-session','document_id':7}}}}]}

    async def test_retained_owner_blocks_new_run_without_overwriting_report(self):
        self.retained();original=self.test.export()
        with patch.object(type(self.bridge.transport),'folder',new_callable=__import__('unittest.mock',fromlist=['PropertyMock']).PropertyMock,return_value=self.root),patch('backend.stability.read_heartbeat',return_value={'session':'original-session'}):
            with self.assertRaisesRegex(RuntimeError,'临时文档仍保留'):await self.test.start()
        self.assertEqual(self.test.export(),original);self.assertFalse(self.app.operation_lock.locked())
        self.assertTrue(self.test.snapshot()['cleanup_available'])

    async def test_unknown_never_allows_cleanup_or_start(self):
        self.retained();self.test.report['original_request_pending']=True
        with patch.object(self.bridge,'execute_stability',new_callable=AsyncMock) as execute:
            with self.assertRaisesRegex(RuntimeError,'禁止清理'):await self.test.cleanup()
            self.assertFalse(self.test.snapshot()['cleanup_available']);execute.assert_not_called()
        self.bridge.transport.pending.error='corrupt original record'
        with self.assertRaisesRegex(RuntimeError,'原请求仍待核对'):await self.test.start()
        self.bridge.transport.pending.error=None

    async def test_cleanup_uses_original_identity_and_requires_complete_proof(self):
        self.retained();await self.app.operation_lock.acquire();self.test.active=True
        async def execute(run,step,before):
            self.assertEqual((run,step),('a'*32,'finish'));await before(None)
            return {'task_id':'original-cleanup-task','result':json.dumps({'result':{'closed_owned_document':True,'original_active_restored':True}})}
        with patch('backend.stability.host_scope',return_value=(1,lambda:None)),patch.object(type(self.bridge.transport),'folder',new_callable=__import__('unittest.mock',fromlist=['PropertyMock']).PropertyMock,return_value=self.root),patch('backend.stability.read_heartbeat',return_value={'session':'original-session'}),patch.object(self.bridge,'execute_stability',side_effect=execute) as call:
            await self.test.run_cleanup()
        call.assert_awaited_once();self.assertEqual(self.test.report['phase'],'cleaned')
        self.assertFalse(self.test.snapshot()['cleanup_available']);self.assertFalse(self.app.operation_lock.locked())
        self.assertEqual(self.test.report['steps'][0]['state'],'passed')

    async def test_cleanup_session_change_submits_nothing_and_preserves_owner(self):
        self.retained();await self.app.operation_lock.acquire();self.test.active=True
        with patch('backend.stability.host_scope',return_value=(1,lambda:None)),patch.object(type(self.bridge.transport),'folder',new_callable=__import__('unittest.mock',fromlist=['PropertyMock']).PropertyMock,return_value=self.root),patch('backend.stability.read_heartbeat',return_value={'session':'different-session'}),patch.object(self.bridge,'execute_stability',new_callable=AsyncMock) as call:
            await self.test.run_cleanup()
        call.assert_not_awaited();self.assertEqual(self.test.report['phase'],'stopped')
        self.assertIn('原UXP会话',self.test.report['cleanup_problem']['detail'])
        self.assertIn('retained',self.test.report['cleanup']);self.assertFalse(self.app.operation_lock.locked())

    async def test_cleanup_remains_active_until_evidence_saved_and_lock_released(self):
        self.retained();await self.app.operation_lock.acquire();self.test.active=True
        entered=asyncio.Event();release=asyncio.Event()
        async def save():entered.set();await release.wait()
        result={'task_id':'cleanup','result':json.dumps({'result':{'closed_owned_document':True,'original_active_restored':True}})}
        with patch('backend.stability.host_scope',return_value=(1,lambda:None)),patch.object(type(self.bridge.transport),'folder',new_callable=__import__('unittest.mock',fromlist=['PropertyMock']).PropertyMock,return_value=self.root),patch('backend.stability.read_heartbeat',return_value={'session':'original-session'}),patch.object(self.bridge,'execute_stability',new_callable=AsyncMock,return_value=result),patch.object(self.test,'save',side_effect=save):
            task=asyncio.create_task(self.test.run_cleanup());await entered.wait()
            self.assertTrue(self.test.snapshot()['active']);self.assertTrue(self.app.operation_lock.locked())
            with self.assertRaisesRegex(RuntimeError,'不排队'):await self.test.start()
            release.set();await task
        self.assertFalse(self.test.snapshot()['active']);self.assertFalse(self.app.operation_lock.locked())

    def test_english_failure_keeps_raw_detail_and_never_authorizes_unknown_retry(self):
        from backend.stability_failures import describe
        raw='An owned test document is still retained'
        self.assertEqual(describe(raw,'fixture')['code'],'fixture-retained')
        self.assertEqual(describe(raw,'fixture')['detail'],raw)
        self.assertEqual(describe(raw,'fixture',True)['code'],'result-pending')

    async def test_read_timeout_cannot_authorize_cleanup_but_matching_terminal_failure_can(self):
        from backend.uxp_bridge import UxpResultUnknown,UxpRequestFailed
        self.retained()
        with patch.object(self.bridge.transport,'call',side_effect=UxpResultUnknown('timeout')):
            with self.assertRaises(UxpResultUnknown):await self.test.test_read()
        self.assertTrue(self.test.report['read_request_pending']);self.assertFalse(self.test.snapshot()['cleanup_available'])
        with self.assertRaisesRegex(RuntimeError,'禁止清理'):await self.test.cleanup()
        self.test.report['read_request_pending']=False
        with patch.object(self.bridge.transport,'call',side_effect=UxpRequestFailed('known failed')):
            with self.assertRaises(UxpRequestFailed):await self.test.test_read()
        self.assertFalse(self.test.report['read_request_pending']);self.assertTrue(self.test.snapshot()['cleanup_available'])
    async def test_stop_does_not_cancel_inflight_and_partial_report_persists(self):
        self.test.report={'run_id':'a'*32,'started_at':__import__('time').time(),'steps':[],'round':1,'skipped':[]}
        self.test.focus_check=lambda:None
        started=asyncio.Event();finish=asyncio.Event()
        async def op():started.set();await finish.wait();return {'verified':True}
        worker=asyncio.create_task(self.test.step('host',op));await started.wait()
        self.test.active=True;self.test.stop()
        self.assertFalse(worker.done());finish.set();await worker
        with self.assertRaises(Stopped):await self.test.step('next',op)
        report=json.loads((self.test.folder/'latest.json').read_text('utf-8'))
        self.assertEqual(len(report['steps']),1);self.assertEqual(report['steps'][0]['state'],'passed')
    async def test_unknown_does_not_cleanup_or_replay(self):
        self.test.report={'run_id':'a'*32,'started_at':__import__('time').time(),'rounds':1,'round':0,'steps':[],'skipped':[]}
        self.test.active=True;await self.app.operation_lock.acquire()
        calls=[]
        async def command(step,**kw):
            calls.append(step)
            if step=='fixture':return {'verified':True}
            self.bridge.busy=True;raise RuntimeError('original pending')
        async def read():return {}
        async def monitor():pass
        with patch('backend.stability.host_scope',return_value=(1,lambda:None)),patch('backend.stability.read_heartbeat',return_value={'probe_version':'0.3.47'}),patch.object(type(self.bridge.transport),'folder',new_callable=__import__('unittest.mock',fromlist=['PropertyMock']).PropertyMock,return_value=self.root),patch('backend.stability.machine_info',return_value={}),patch.object(self.test,'monitor',monitor),patch.object(self.test,'command',command),patch.object(self.test,'menu_read',read),patch.object(self.test,'native_selection_probe',read):
            await self.test.run()
        self.assertEqual(calls,['fixture','label']);self.assertTrue(self.test.report['original_request_pending'])
        self.assertEqual(self.test.report['cleanup'],'retained-for-inspection-no-automatic-cleanup')
        self.assertFalse(self.app.operation_lock.locked());self.bridge.busy=False
    async def test_skip_excluded_from_success_statistics(self):
        rows=[{'stage':'x','state':'passed','elapsed_ms':1},{'stage':'x','state':'skipped','elapsed_ms':1000}]
        self.assertEqual(summary(rows)['x']['samples'],1)
        self.assertNotIn('stability.internal',[x['id'] for x in command_contracts()])
    async def test_menu_requires_native_visible_content_capabilities_and_closes(self):
        import time
        target={'document_id':2,'layer_ids':[3]}
        context={'schema':1,'session':'s','plugin_time_ms':time.time()*1000,'kind':'pixel','mask':15,'quick_mask':127,**target}
        desktop=SimpleNamespace(begin_stability_menu=lambda *x:7,
            stability_menu_state=lambda epoch:{'visible':True,'delivery':{'phase':'applied','menu_phase':'completed','request_id':'r'},'context':context,'native':0xffff,'frames':5},
            end_stability_menu=lambda:None,visible=False,hwnd=9,gui=SimpleNamespace(IsWindowVisible=lambda hwnd:False))
        self.app.desktop=desktop;self.test.host_pid=1;self.test.report={};self.test.check=lambda:None
        with patch.object(self.test,'inspect',AsyncMock(return_value={'stability':{'target':target}})):
            result=await self.test.menu_read()
        self.assertTrue(result['native_content_visible']);self.assertTrue(result['native_buttons_enabled']);self.assertEqual(result['epoch'],7)
        desktop.stability_menu_state=lambda epoch:{'visible':False}
        with patch.object(self.test,'inspect',AsyncMock(return_value={'stability':{'target':target}})):
            with self.assertRaisesRegex(RuntimeError,'意外关闭'):await self.test.menu_read()

    async def test_menu_waits_for_native_frame_and_enabled_controls(self):
        import time
        target={'document_id':2,'layer_ids':[3]}
        context={'schema':1,'session':'s','plugin_time_ms':time.time()*1000,'kind':'pixel','mask':15,'quick_mask':127,**target}
        base={'visible':True,'delivery':{'phase':'applied','menu_phase':'completed'},'context':context,'frames_start':10,'frames':11}
        snapshots=[{**base,'native':0},{**base,'native':0xffff}]
        state=__import__('unittest.mock',fromlist=['Mock']).Mock(side_effect=snapshots)
        close=__import__('unittest.mock',fromlist=['Mock']).Mock()
        self.app.desktop=SimpleNamespace(begin_stability_menu=lambda *x:7,stability_menu_state=state,end_stability_menu=close,
            visible=False,hwnd=9,gui=SimpleNamespace(IsWindowVisible=lambda hwnd:False))
        self.test.host_pid=1;self.test.report={};self.test.check=lambda:None
        with patch.object(self.test,'inspect',AsyncMock(return_value={'stability':{'target':target}})):
            result=await self.test.menu_read()
        self.assertEqual(state.call_count,2);close.assert_called_once();self.assertTrue(result['native_buttons_enabled'])

    async def test_internal_completion_updates_fresh_layer_feedback(self):
        import time
        context={'schema':1,'session':'s','plugin_time_ms':time.time()*1000,'kind':'pixel','mask':15,'document_id':7,'layer_ids':[8]}
        async def execute(action,before_dispatch,capture,execute):return execute(action)
        with patch.object(self.bridge,'_execute_task',execute),patch.object(self.bridge.transport,'call',return_value={'session':'s','result':{'verified':True},'layer_context':context}):
            await self.bridge.execute_stability('a'*32,'finish')
        self.assertEqual(self.bridge.layer_context['document_id'],7)
        self.assertEqual(self.bridge.layer_context['layer_ids'],[8])
    async def test_late_counter_sample_cannot_write_into_a_new_run(self):
        import threading
        entered=threading.Event();release=threading.Event();closed=[]
        class Collector:
            def __init__(self,pid):pass
            def sample(self):entered.set();release.wait(4);return {'time_ms':1}
            def close(self):closed.append(True)
        self.test.active=True;self.test.host_pid=1;self.test.report={'system_samples':[]}
        with patch('backend.stability.Metrics',Collector):
            task=asyncio.create_task(self.test.monitor())
            self.assertTrue(await asyncio.to_thread(entered.wait,3))
            newer={'system_samples':[]};self.test.report=newer;release.set();await task
        self.assertEqual(newer,{'system_samples':[]});self.assertTrue(closed)
