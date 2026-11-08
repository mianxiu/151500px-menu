import json
import tempfile
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
from backend.input_tool import ToolObservation,ToolReadError,previous_tool_producer_exited
from backend.settings import atomic_json


class ToolObservationTests(unittest.TestCase):
    def old_session(self,**changes):
        self.identity=[10,20,'2026-11-04 15:00:00+00:00',30]
        self.heartbeat['session']='1793804500000-beef'
        request=self.pending(session='1700000000000-dead',expires_at_ms=1700000001000,**changes)
        wire={**request,'token':'private-test-token'}
        atomic_json(self.folder/'input-tool-request.json',wire)
        (self.folder/('input-tool-request-'+request['id']+'.tmp')).write_bytes(b'original temporary evidence')
        self.reply(request,completed_at_ms=1700000000100)
        return request

    def test_previous_session_is_archived_unknown_before_an_independent_read(self):
        old=self.old_session();raw=self.reader.path.read_bytes();published=[]
        def publish(source,destination):
            archive=list((self.root/'input-tool-unknown-history').glob('*.json'));self.assertEqual(len(archive),1)
            evidence=json.loads(archive[0].read_text());self.assertEqual(evidence['state'],'unknown')
            self.assertFalse(evidence['original_republished']);self.assertFalse(evidence['marked_successful'])
            self.assertEqual(evidence['request'],old)
            saved={row['name']:bytes.fromhex(row['bytes_hex']) for row in evidence['files']}
            self.assertEqual(saved['input-tool-pending.json'],raw)
            self.assertIn('input-tool-response.json',saved);self.assertIn('input-tool-request-'+old['id']+'.tmp',saved)
            wire=json.loads(source.read_text());self.assertNotEqual(wire['id'],old['id']);self.assertEqual(wire['session'],self.heartbeat['session'])
            source.replace(destination);published.append(wire['id']);self.reply(wire)
        with patch('backend.input_tool.previous_tool_producer_exited',return_value=True),patch('backend.input_tool.publish_request',side_effect=publish):
            result=self.reader.read(identity=self.identity)
        self.assertEqual(result['isModal'],False);self.assertEqual(len(published),1)
        self.assertEqual(self.reader.snapshot()['last_retired_unknown']['state'],'unknown')
        self.assertNotIn('private-test-token',json.dumps(self.reader.snapshot()))

    def test_reconciliation_can_retire_only_the_cached_current_identity_without_publishing(self):
        old=self.old_session();self.reader.identity=self.identity
        with patch('backend.input_tool.previous_tool_producer_exited',return_value=True),patch('backend.input_tool.publish_request') as publish:
            self.assertTrue(self.reader.reconcile());publish.assert_not_called()
        self.assertFalse(self.reader.path.exists())
        self.assertEqual(self.reader.snapshot()['last_retired_unknown']['request_id'],old['id'])

    def test_unconfirmed_producer_and_non_readonly_envelope_keep_the_old_slot(self):
        old=self.old_session();raw=self.reader.path.read_bytes()
        with patch('backend.input_tool.time.monotonic',return_value=1),patch('backend.input_tool.previous_tool_producer_exited',return_value=False),patch('backend.input_tool.publish_request') as publish:
            with self.assertRaises(ToolReadError):self.reader.read(identity=self.identity)
            publish.assert_not_called();self.assertEqual(self.reader.path.read_bytes(),raw)
        wire=json.loads((self.folder/'input-tool-request.json').read_text());wire['operation']='document.write'
        atomic_json(self.folder/'input-tool-request.json',wire)
        with patch('backend.input_tool.time.monotonic',return_value=3),patch('backend.input_tool.previous_tool_producer_exited',return_value=True),patch('backend.input_tool.publish_request') as publish:
            with self.assertRaises(ToolReadError):self.reader.read(identity=self.identity)
            publish.assert_not_called();self.assertEqual(self.reader.path.read_bytes(),raw)

    def test_archive_failure_and_changed_producer_never_release_unknown(self):
        self.old_session();raw=self.reader.path.read_bytes()
        with patch('backend.input_tool.time.monotonic',return_value=1),patch('backend.input_tool.previous_tool_producer_exited',return_value=True),patch('backend.input_tool.atomic_json',side_effect=OSError('archive failed')),patch('backend.input_tool.publish_request') as publish:
            with self.assertRaises(OSError):self.reader.read(identity=self.identity)
            publish.assert_not_called();self.assertEqual(self.reader.path.read_bytes(),raw)
        with patch('backend.input_tool.time.monotonic',return_value=3),patch('backend.input_tool.previous_tool_producer_exited',side_effect=[True,False]),patch('backend.input_tool.publish_request') as publish:
            with self.assertRaises(ToolReadError):self.reader.read(identity=self.identity)
            publish.assert_not_called();self.assertEqual(self.reader.path.read_bytes(),raw)
        # A new heartbeat is a diagnostic update, not a change to the original request.
        atomic_json(self.folder/'probe-last.json',{'session':self.heartbeat['session'],'tick':123})
        with patch('backend.input_tool.time.monotonic',return_value=5),patch('backend.input_tool.previous_tool_producer_exited',return_value=True),patch('backend.input_tool.publish_request') as publish:
            self.reader.identity=self.identity;self.assertTrue(self.reader.reconcile());publish.assert_not_called()

    def test_waiting_for_producer_exit_is_bounded_and_rechecks_without_a_new_key(self):
        self.old_session();self.reader.identity=self.identity
        with patch('backend.input_tool.previous_tool_producer_exited',return_value=False) as proof,patch('backend.input_tool.time.monotonic',return_value=10),patch('backend.input_tool.publish_request') as publish:
            self.assertFalse(self.reader.reconcile());self.assertFalse(self.reader.reconcile())
            self.assertEqual(proof.call_count,1);publish.assert_not_called()
        with patch('backend.input_tool.previous_tool_producer_exited',return_value=True),patch('backend.input_tool.time.monotonic',return_value=12),patch('backend.input_tool.publish_request') as publish:
            self.assertTrue(self.reader.reconcile());publish.assert_not_called()

    def test_legacy_producer_proof_requires_one_pinned_newer_ps_process(self):
        identity=[10,20,'2026-11-04 15:00:00+00:00',30]
        handle=SimpleNamespace(Close=lambda:None)
        with patch('backend.uxp_management.photoshop_process_ids',return_value=(20,)),patch('win32api.OpenProcess',return_value=handle),patch('win32process.GetProcessTimes',return_value={'CreationTime':identity[2]}),patch('win32event.WaitForSingleObject',return_value=258):
            self.assertTrue(previous_tool_producer_exited('1700000000000-dead',identity))
            self.assertFalse(previous_tool_producer_exited('1793804500000-dead',identity))
            self.assertFalse(previous_tool_producer_exited('not-a-session',identity))
            with patch('backend.uxp_management.photoshop_process_ids',return_value=(20,21)):
                self.assertFalse(previous_tool_producer_exited('1700000000000-dead',identity))
            with patch('win32process.GetProcessTimes',return_value={'CreationTime':'reused PID'}):
                self.assertFalse(previous_tool_producer_exited('1700000000000-dead',identity))
            with patch('backend.uxp_management.photoshop_process_ids',side_effect=OSError('inventory denied')):
                self.assertFalse(previous_tool_producer_exited('1700000000000-dead',identity))

    def test_corrupt_archive_keeps_unknown_slot_and_all_host_requests_unchanged(self):
        from backend.input_tool import atomic_json as write
        self.old_session();raw=self.reader.path.read_bytes()
        def corrupt(path,value):
            if path.parent.name=='input-tool-unknown-history':
                value=json.loads(json.dumps(value));value['files'][0]['bytes_hex']='00'
            write(path,value)
        with patch('backend.input_tool.previous_tool_producer_exited',return_value=True),patch('backend.input_tool.atomic_json',side_effect=corrupt),patch('backend.input_tool.publish_request') as publish:
            with self.assertRaises(ToolReadError) as raised:self.reader.read(identity=self.identity)
            self.assertEqual(raised.exception.reason,'retirement-blocked')
            publish.assert_not_called();self.assertEqual(self.reader.path.read_bytes(),raw)

    def test_ambiguous_original_mailbox_is_not_retired(self):
        self.old_session();raw=self.reader.path.read_bytes()
        self.reader.transport.storage=self.root/'storage'
        duplicate=self.reader.transport.storage/'other/External/com.15100px.menu.offline-probe/PluginData'
        duplicate.mkdir(parents=True)
        (duplicate/'input-tool-request.json').write_bytes((self.folder/'input-tool-request.json').read_bytes())
        with patch('backend.input_tool.previous_tool_producer_exited',return_value=True),patch('backend.input_tool.publish_request') as publish:
            with self.assertRaises(ToolReadError):self.reader.read(identity=self.identity)
            publish.assert_not_called();self.assertEqual(self.reader.path.read_bytes(),raw)
        self.assertEqual(self.reader.snapshot()['retirement']['reason'],'original-readonly-envelope-ambiguous')

    def test_trusted_host_failure_is_classified_without_exporting_raw_errors(self):
        request=self.pending()
        error='Error: Unsupported tool state: '+json.dumps({'classID':'moveTool','isModal':'undefined','fields':['SECRET'], 'token':'SECRET'})
        self.reply(request,phase='failed',error=error)
        with self.assertRaises(ToolReadError) as raised:self.reader.read()
        self.assertEqual(raised.exception.reason,'unsupported-tool-state')
        report=self.reader.snapshot();self.assertEqual(report['last_failure']['isModal'],'undefined')
        self.assertNotIn('SECRET',json.dumps(report));self.assertNotIn('private-test-token',json.dumps(report))
        saved=self.root/'input-tool-last-failure.json';self.assertTrue(saved.is_file())
        self.assertNotIn('SECRET',saved.read_text());self.assertFalse(self.reader.path.exists())

    def test_unknown_request_reports_reason_without_republishing_or_erasing_it(self):
        request=self.pending(session='previous');before=self.reader.path.read_bytes()
        with self.assertRaises(ToolReadError):self.reader.read()
        self.assertEqual(self.reader.snapshot()['reason'],'pending-session-changed')
        self.assertEqual(self.reader.path.read_bytes(),before)
        self.assertFalse((self.folder/'input-tool-request.json').exists())

    def test_failure_evidence_write_failure_keeps_original_pending_identity(self):
        from backend.input_tool import atomic_json as write
        request=self.pending();self.reply(request,phase='failed',error='SECRET native error')
        before=self.reader.path.read_bytes()
        def save(path,value):
            if path.name=='input-tool-last-failure.json':raise OSError('evidence unavailable')
            write(path,value)
        with patch('backend.input_tool.atomic_json',side_effect=save),self.assertRaises(OSError):self.reader.read()
        self.assertEqual(self.reader.path.read_bytes(),before)

    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.folder=self.root/'plugin';self.folder.mkdir()
        config=self.root/'uxp-config.json';atomic_json(config,{'token':'private-test-token'})
        self.heartbeat={'probe_version':'0.3.66','session':'owned-session'}
        self.reader=ToolObservation(SimpleNamespace(config=config,observe=lambda:(self.folder,self.heartbeat)))

    def pending(self,**changes):
        value={'schema':1,'id':'a'*32,'session':'owned-session','expires_at_ms':time.time()*1000+1000,**changes}
        atomic_json(self.reader.path,value);return value

    def test_edit_events_authenticate_and_never_publish_or_settle_requests(self):
        original=self.pending();path=self.folder/'input-tool-event.json'
        event={'schema':1,'session':'owned-session','token':'private-test-token','sequence':2,'state':'exit',
               'tool':'txBx','kind':'tool','created_at_ms':time.time()*1000}
        atomic_json(path,event)
        self.assertEqual(self.reader.event(),('owned-session',2,'exit'))
        detail=self.reader.event_detail()
        self.assertEqual(detail['sequence'],2);self.assertEqual(detail['state'],'exit')
        self.assertAlmostEqual(detail['created_at_ms'],event['created_at_ms'],places=3)
        for changes in ({'schema':True},{'token':'other'},{'session':'old'},{'sequence':True},{'state':'unknown'},
                        {'tool':'arwT'},{'tool':None},{'kind':'mouse'},{'kind':None},
                        {'created_at_ms':time.time()*1000-3000},{'created_at_ms':float('nan')}):
            atomic_json(path,{**event,**changes});self.assertIsNone(self.reader.event())
        self.assertEqual(json.loads(self.reader.path.read_text()),original)
        self.assertFalse((self.folder/'input-tool-request.json').exists())

    def test_tool_state_is_authenticated_fresh_and_cached_by_file_version(self):
        path=self.folder/'input-tool-state.json'
        state={'schema':1,'session':'owned-session','token':'private-test-token','sequence':4,
               'tool':'typeCreateOrEditTool','isModal':False,'created_at_ms':time.time()*1000}
        atomic_json(path,state)
        self.assertEqual(self.reader.state(),{'classID':'typeCreateOrEditTool','isModal':False,
                                               'sequence':4,'created_at_ms':state['created_at_ms']})
        writes=path.stat().st_mtime_ns
        self.assertEqual(self.reader.state()['sequence'],4);self.assertEqual(path.stat().st_mtime_ns,writes)
        self.heartbeat['session']='new-session'
        self.assertIsNone(self.reader.state())
        self.heartbeat['session']='owned-session'
        atomic_json(self.reader.transport.config,{'token':'rotated-token'})
        self.assertIsNone(self.reader.state())
        atomic_json(self.reader.transport.config,{'token':'private-test-token'})
        with patch('backend.input_tool.time.time',return_value=time.time()+25*60*60):
            self.assertIsNone(self.reader.state())
        for change in ({'session':'other'},{'token':'other'},{'isModal':'false'},{'tool':''},
                       {'sequence':0},{'created_at_ms':time.time()*1000-25*60*60*1000}):
            time.sleep(.002)
            atomic_json(path,{**state,**change,'created_at_ms':change.get('created_at_ms',time.time()*1000)})
            self.assertIsNone(self.reader.state(),change)
        time.sleep(.002)
        atomic_json(path,{**state,'sequence':6,'created_at_ms':time.time()*1000-5000})
        self.assertEqual(self.reader.state()['sequence'],6)  # durable until the tool changes
        time.sleep(.002)
        atomic_json(path,{**state,'sequence':5,'isModal':True,'created_at_ms':time.time()*1000})
        self.assertEqual(self.reader.state()['sequence'],5);self.assertTrue(self.reader.state()['isModal'])

    def reply(self,request,**changes):
        atomic_json(self.folder/'input-tool-response.json',{
            'schema':1,'id':request['id'],'session':request['session'],'token':'private-test-token',
            'phase':'completed','classID':'typeCreateOrEditTool','isModal':False,
            'completed_at_ms':time.time()*1000,**changes})

    def test_matched_idle_and_editing_tool_responses_consume_only_original_identity(self):
        for modal in (False,True):
            original=self.pending();self.reply(original,isModal=modal)
            self.assertEqual(self.reader.read(),{'classID':'typeCreateOrEditTool','isModal':modal})
            self.assertFalse(self.reader.path.exists())
            self.assertFalse((self.folder/'input-tool-request.json').exists(),'Original observation must never be republished')

    def test_new_request_is_authenticated_and_identity_is_saved_before_publication(self):
        from backend.input_tool import atomic_json as write
        from backend.input_tool import publish_request as rename
        rows=[]
        def save(path,value):rows.append(value.get('publication'));write(path,value)
        def publish(temporary,destination):
            value=json.loads(temporary.read_text('utf-8'));rows.append('request')
            pending=json.loads(self.reader.path.read_text());self.assertEqual(pending['publication'],'prepared');self.assertNotIn('token',pending)
            self.assertEqual(value['token'],'private-test-token');self.assertEqual(value['schema'],1)
            rename(temporary,destination);self.reply(value)
        with patch('backend.input_tool.atomic_json',side_effect=save),patch('backend.input_tool.publish_request',side_effect=publish):self.reader.read()
        self.assertEqual(rows,['prepared','request','published'])

    def test_cold_start_before_publication_rechecks_ready_host_without_input(self):
        with patch.object(self.reader.transport,'observe',side_effect=RuntimeError('UXP starting')),patch('backend.input_tool.time.monotonic',return_value=10):
            with self.assertRaisesRegex(RuntimeError,'starting'):self.reader.read()
        self.assertFalse(self.reader.path.exists());self.assertEqual(self.reader.retry_after,11)
        with patch('backend.input_tool.time.monotonic',return_value=10.9):self.assertFalse(self.reader.reconcile())
        with patch('backend.input_tool.time.monotonic',return_value=11):self.assertTrue(self.reader.reconcile())
        def publish(source,destination):
            request=json.loads(source.read_text('utf-8'));source.replace(destination);self.reply(request)
        with patch('backend.input_tool.publish_request',side_effect=publish) as sent:
            self.assertEqual(self.reader.read()['classID'],'typeCreateOrEditTool')
        self.assertEqual(sent.call_count,1);self.assertFalse(self.reader.path.exists())

    def test_startup_failure_with_existing_unknown_never_schedules_republication(self):
        original=self.pending();before=self.reader.path.read_bytes()
        with patch.object(self.reader.transport,'observe',side_effect=RuntimeError('UXP starting')),patch('backend.input_tool.publish_request') as sent:
            with self.assertRaises(RuntimeError):self.reader.read()
            self.assertFalse(self.reader.reconcile());sent.assert_not_called()
        self.assertIsNone(self.reader.retry_after);self.assertEqual(self.reader.path.read_bytes(),before)

    def test_previous_process_heartbeat_is_not_current_readiness(self):
        identity=[1,2,'1970-01-01 00:00:02+00:00',3]
        self.heartbeat['session']='1000-previous'
        with patch('backend.input_tool.publish_request') as sent:
            with self.assertRaisesRegex(RuntimeError,'当前PS进程'):self.reader.read(identity=identity)
            sent.assert_not_called()
        self.assertFalse(self.reader.path.exists());self.assertIsNotNone(self.reader.retry_after)
        self.heartbeat['session']='3000-current'
        def publish(source,destination):
            request=json.loads(source.read_text('utf-8'));source.replace(destination);self.reply(request)
        with patch('backend.input_tool.publish_request',side_effect=publish):self.reader.read(identity=identity)
        self.assertFalse(self.reader.path.exists())

    def test_previous_session_pending_remains_unknown_when_ps_restarts(self):
        self.heartbeat['session']='1000-previous';request=self.pending(session='1000-previous');before=self.reader.path.read_bytes()
        with patch('backend.input_tool.publish_request') as sent:
            with self.assertRaisesRegex(RuntimeError,'当前PS进程'):self.reader.read(identity=[1,2,'1970-01-01 00:00:02+00:00',3])
            sent.assert_not_called()
        self.assertIsNone(self.reader.retry_after);self.assertEqual(self.reader.path.read_bytes(),before)

    def test_failed_publication_archives_before_release_and_never_waits_for_host(self):
        error=PermissionError('sharing');error.winerror=32
        with patch('backend.input_tool.publish_request',side_effect=error):
            with self.assertRaisesRegex(RuntimeError,'未发布'):self.reader.read()
        self.assertFalse(self.reader.path.exists());self.assertFalse((self.folder/'input-tool-request.json').exists())
        archive=list(self.root.glob('input-tool-unpublished-*.json'));self.assertEqual(len(archive),1)
        record=json.loads(archive[0].read_text());self.assertFalse(record['host_request_published']);self.assertEqual(record['request']['publication'],'prepared')
        self.assertIsNotNone(self.reader.retry_after)

    def test_sharing_retry_keeps_one_identity_and_one_envelope(self):
        import os
        real=os.replace;attempts=[]
        error=PermissionError('sharing');error.winerror=32
        def replace(source,destination):
            if Path(destination).name=='input-tool-request.json':
                value=json.loads(Path(source).read_text('utf-8'));attempts.append(value)
                if len(attempts)<3:raise error
                result=real(source,destination);self.reply(value);return result
            return real(source,destination)
        with patch('backend.uxp_bridge.os.replace',side_effect=replace):self.reader.read()
        self.assertEqual(len(attempts),3);self.assertTrue(all(row==attempts[0] for row in attempts))
        self.assertFalse(self.reader.path.exists());self.assertFalse(list(self.root.glob('input-tool-unpublished-*.json')))

    def test_archive_failure_keeps_unpublished_identity_and_does_not_claim_exit(self):
        from backend.input_tool import atomic_json as write
        def save(path,value):
            if 'unpublished-' in path.name:raise OSError('archive failed')
            write(path,value)
        with patch('backend.input_tool.atomic_json',side_effect=save),patch('backend.input_tool.publish_request',side_effect=PermissionError('sharing')):
            with self.assertRaises(OSError):self.reader.read()
        self.assertEqual(json.loads(self.reader.path.read_text())['publication'],'prepared')

    def test_readonly_reconcile_consumes_only_matching_terminal_and_never_publishes(self):
        original=self.pending();self.reply(original,id='b'*32)
        with patch('backend.input_tool.publish_request') as publish:
            self.assertFalse(self.reader.reconcile());publish.assert_not_called();self.assertTrue(self.reader.path.exists())
            self.reply(original,completed_at_ms=time.time()*1000-2000)
            self.assertTrue(self.reader.reconcile());publish.assert_not_called();self.assertFalse(self.reader.path.exists())

    def test_prepared_after_publication_crash_is_not_republished(self):
        original=self.pending(schema=2,publication='prepared');self.reply(original)
        with patch('backend.input_tool.publish_request') as publish:
            self.assertEqual(self.reader.read()['classID'],'typeCreateOrEditTool');publish.assert_not_called()

    def test_unknown_wrong_id_token_and_session_are_never_republished(self):
        for changes in ({'id':'b'*32},{'token':'other'},{'session':'other'}):
            original=self.pending();self.reply(original,**changes);before=self.reader.path.read_bytes()
            with patch('backend.input_tool.time.monotonic',side_effect=[0,1]),self.assertRaisesRegex(RuntimeError,'不重发'):
                self.reader.read()
            self.assertEqual(self.reader.path.read_bytes(),before)
            self.assertFalse((self.folder/'input-tool-request.json').exists())

    def test_malformed_authenticated_response_keeps_pending_identity(self):
        for changes in ({'completed_at_ms':float('nan')},{'completed_at_ms':True},{'classID':None},{'isModal':0},{'phase':'running'}):
            original=self.pending();self.reply(original,**changes);before=self.reader.path.read_bytes()
            with self.assertRaises(RuntimeError):self.reader.read()
            self.assertEqual(self.reader.path.read_bytes(),before)

    def test_failed_or_stale_trusted_terminal_is_consumed_but_cannot_grant_eng(self):
        for changes in ({'phase':'failed'},{'completed_at_ms':time.time()*1000-2000}):
            original=self.pending();self.reply(original,**changes)
            with self.assertRaisesRegex(RuntimeError,'失败／失鲜'):self.reader.read()
            self.assertFalse(self.reader.path.exists())

    def test_old_executor_changed_session_and_damaged_pending_never_publish(self):
        self.heartbeat['probe_version']='0.3.65'
        with self.assertRaisesRegex(RuntimeError,'0.3.66'):self.reader.read()
        self.heartbeat['probe_version']='0.3.66'
        self.pending(session='previous-session')
        with self.assertRaisesRegex(RuntimeError,'会话变化'):self.reader.read()
        for value in ([],{'schema':1},{**self.pending(),'expires_at_ms':float('inf')}):
            atomic_json(self.reader.path,value)
            with self.assertRaisesRegex(RuntimeError,'身份损坏'):self.reader.read()
        self.assertFalse((self.folder/'input-tool-request.json').exists())
