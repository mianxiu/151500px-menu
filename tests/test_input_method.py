import ast
import json
import tempfile
import threading
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock,patch
from backend.input_method import ImePolicy,ImeContext,InputMethod
from tests import test_input_language as legacy_tests


def sample(state='non-input',opened=1):
    return {'root':1,'identity':[1,2,'created',3],'focus':10,'layout':0x8040804,
            'ime':20,'ime_open':opened,'state':state}


class ImePolicyTests(unittest.TestCase):
    def test_first_key_confirmation_requires_actual_protection_and_preserves_unknown(self):
        guard=SimpleNamespace(shortcut_context=lambda:{'state':'non-input','root':1,'focus':10,'epoch':4})
        c=InputMethod(SimpleNamespace(input_focus=guard));c.tick=Mock(return_value=100)
        request={'verify_id':1,'root':1,'hwnd':10,'input_epoch':4,'expires_tick_ms':200,'need_ime':True}
        reply=Mock();c.policy.owned=sample();c.verify(request,reply)
        c.complete_verification({**sample(opened=0),'tsf_disabled':True},0);reply.assert_not_called();self.assertEqual(c.ready_context(),{})
        c.complete_verification({**sample(opened=0),'tsf_disabled':False});reply.assert_not_called()
        c.complete_verification({**sample(opened=0),'tsf_disabled':True})
        self.assertEqual(reply.call_args.args,(request,2));self.assertEqual(c.ready_context(),{'root':1,'focus':10,'epoch':4})
        unknown={'identity':'original-unknown'};c.policy.pending=unknown;c.verify(request,reply);reply.reset_mock()
        c.complete_verification({**sample(opened=0),'tsf_disabled':True});reply.assert_not_called()
        c.tick.return_value=201;c.complete_verification({**sample(opened=0),'tsf_disabled':True})
        self.assertEqual(reply.call_args.args,(request,0));self.assertEqual(c.policy.pending,unknown)

    def test_disable_readback_then_input_restore_without_layout_change(self):
        p=ImePolicy();request=p.step(sample(),7,0)
        self.assertEqual(request['wanted'],0);self.assertIsNone(p.owned)
        self.assertIsNone(p.step(sample(opened=0),7,.1));self.assertIsNotNone(p.owned)
        request=p.step(sample('input',0),None,.2);self.assertEqual(request['wanted'],1)
        p.step(sample('input'),None,.3);self.assertIsNone(p.owned)
        self.assertEqual((p.confirmed_disables,p.confirmed_restores),(1,1))
        self.assertIsNotNone(p.step(sample(),7,.4))

    def test_unknown_input_outside_and_already_closed_do_not_disable(self):
        for s in (sample('unknown'),sample('input'),sample('outside'),sample(opened=0),{**sample(),'ime_open':None}):
            self.assertIsNone(ImePolicy().step(s,7,0))

    def test_unconfirmed_request_is_retained_without_reissue(self):
        p=ImePolicy();request=p.step(sample(),7,0)
        for tick in (1,2,100):self.assertIsNone(p.step(sample(),8,tick))
        self.assertEqual(p.pending,request);self.assertEqual(p.state,'request-not-confirmed')

    def test_unknown_restores_once_and_does_not_autonomously_toggle(self):
        p=ImePolicy();p.step(sample(),7,0);p.step(sample(opened=0),7,.1)
        self.assertEqual(p.step(sample('unknown',0),None,.2)['wanted'],1)
        p.step(sample(),7,.3)
        self.assertIsNone(p.step(sample(),7,2));self.assertIsNotNone(p.step(sample(),8,3))

    def test_background_restore_targets_the_owned_ps_context_only(self):
        p=ImePolicy();p.step(sample(),7,0);p.step(sample(opened=0),7,.1)
        for field,value in (('identity',[4,5,'other',6]),('ime',99),('layout',0x4090409)):
            self.assertIsNone(p.step({**sample('outside',0),field:value},None,.2))
        self.assertEqual(p.step(sample('outside',0),None,.3)['ime'],20)

    def test_manual_reopen_rearms_non_input_without_waiting_for_an_edit_cycle(self):
        p=ImePolicy();p.step(sample(),7,0);p.step(sample(opened=0),7,.1)
        request=p.step(sample(),7,.2)
        self.assertEqual(request['wanted'],0);self.assertEqual(p.pending,request)
        self.assertIsNone(p.step(sample(),8,2));self.assertEqual(p.state,'request-not-confirmed')

    def test_ps_reopening_ime_for_input_is_not_a_manual_override(self):
        p=ImePolicy();p.step(sample(),7,0);p.step(sample(opened=0),7,.1)
        self.assertIsNone(p.step(sample('input'),None,.2));self.assertIsNone(p.owned)
        self.assertIsNotNone(p.step(sample(),7,.3))

    def test_native_input_restore_does_not_permanently_disable_eng_as_manual_change(self):
        p=ImePolicy();p.step(sample(),7,0);p.step(sample(opened=0),7,.1)
        request=p.step({**sample(),'native_restored':True},8,.2)
        self.assertEqual(request['wanted'],0)
        self.assertEqual(p.confirmed_restores,1)


class ImeJournalTests(unittest.TestCase):
    def test_native_journal_binds_the_control_nonce_and_preserves_unknown(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'input-method-owned.json';c=InputMethod(SimpleNamespace());c.context_schema=2
            c.policy.step({**sample(),'context_nonce':123},7,0);path.write_text(json.dumps(c.saved()),'utf-8')
            other=InputMethod(SimpleNamespace());other.load(path)
            self.assertEqual(other.context_schema,2);self.assertIsNotNone(other.policy.pending)
            self.assertIsNone(other.policy.step({**sample(opened=0),'context_nonce':456},8,3))
            self.assertIsNotNone(other.policy.pending)

    def test_unknown_journal_roundtrip_and_wrong_namespace_rejection(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'input-method-owned.json';c=InputMethod(SimpleNamespace(runtime=Path(folder)))
            c.policy.step(sample(),7,0);p.write_text(json.dumps(c.saved()),'utf-8')
            restored=InputMethod(c.desktop);restored.load(p)
            self.assertEqual(restored.policy.pending['identity'],sample()['identity'])
            self.assertIsNotNone(restored.policy.pending);self.assertIsNone(restored.policy.owned)
            p.write_text(json.dumps({'owned':None,'pending':None}),'utf-8')
            with self.assertRaises(ValueError):InputMethod(c.desktop).load(p)

    def test_untrusted_fields_never_enter_the_restore_path(self):
        c=InputMethod(SimpleNamespace());c.policy.step(sample(),7,0);valid=c.saved()
        for key,value in (('wanted',2),('wanted',False),('original',0),('disable',False),('time',float('nan')),('ime',0),('identity',[1,0,'',3])):
            with tempfile.TemporaryDirectory() as folder:
                path=Path(folder)/'input-method-owned.json';bad=json.loads(json.dumps(valid));bad['pending'][key]=value;path.write_text(json.dumps(bad),'utf-8')
                target=InputMethod(SimpleNamespace())
                with self.assertRaises(ValueError):target.load(path)
                self.assertIsNone(target.policy.owned);self.assertIsNone(target.policy.pending)

    def test_confirmed_exit_archives_but_unknown_pending_is_never_retired(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'input-method-owned.json';c=InputMethod(SimpleNamespace());c.policy.step(sample(),7,0)
            c.retire(path,'original-process-exited');self.assertIsNotNone(c.policy.pending);self.assertFalse(path.exists())
            c.policy.step(sample(opened=0),7,.1);owned=dict(c.policy.owned);c.retire(path,'original-process-exited')
            self.assertEqual(json.loads(path.read_text()),{'schema':1,'owned':None,'pending':None})
            archive=list(Path(folder).glob('input-method-retired-*.json'))
            self.assertEqual(json.loads(archive[0].read_text())['owned'],owned)
            self.assertFalse(json.loads(archive[0].read_text())['restore_sent'])


class ImeTransportTests(unittest.TestCase):
    def test_attached_menu_focus_reads_only_original_ps_ime_context(self):
        context=ImeContext.__new__(ImeContext)
        def gui_info(thread,pointer):pointer._obj.focus=99;return 1
        context.user=SimpleNamespace(GetGUIThreadInfo=gui_info,GetKeyboardLayout=lambda _:0x8040804)
        context.read=Mock(side_effect=lambda value:value)
        handle=Mock()
        with patch('win32gui.IsWindow',return_value=True),patch('win32api.OpenProcess',return_value=handle),patch('win32process.GetProcessTimes',return_value={'CreationTime':'created'}),patch('win32process.GetWindowThreadProcessId',side_effect=lambda hwnd:[8,9] if hwnd==99 else [3,2]):
            value=context.observe_owned(sample())
            self.assertEqual(value['focus'],10);self.assertEqual(value['identity'],sample()['identity'])
            self.assertEqual(value['state'],'outside');handle.Close.assert_called_once()

    def test_only_ime_control_messages_are_used(self):
        context=ImeContext.__new__(ImeContext);context.imm=SimpleNamespace(ImmGetDefaultIMEWnd=Mock(return_value=20))
        with patch('win32gui.IsWindow',return_value=True),patch('win32gui.GetClassName',return_value='IME'),patch('win32process.GetWindowThreadProcessId',return_value=[3,2]),patch('win32gui.SendMessageTimeout',return_value=(1,1)) as message:
            self.assertEqual(context.read(sample())['ime_open'],1)
            context.send({'ime':20,'wanted':0});context.send({'ime':20,'wanted':1})
            self.assertEqual([c.args[1:4] for c in message.call_args_list],[(0x283,5,0),(0x283,6,0),(0x283,6,1)])
        with patch('win32gui.IsWindow',return_value=True),patch('win32gui.GetClassName',return_value='IME'),patch('win32process.GetWindowThreadProcessId',return_value=[3,99]),patch('win32gui.SendMessageTimeout') as message:
            self.assertIsNone(context.read(sample())['ime']);message.assert_not_called()

    def test_production_controller_cannot_send_a_new_keyboard_layout_switch(self):
        from backend.desktop import Desktop
        import inspect
        self.assertIn('InputMethod(self)',inspect.getsource(Desktop.start))
        tree=ast.parse(inspect.getsource(__import__('backend.input_method',fromlist=['x'])))
        calls=[n for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute) and n.func.attr=='PostMessageW']
        self.assertEqual(len(calls),1)
        self.assertIn("if request['switch']:raise",inspect.getsource(InputMethod.legacy_step))


class ImeWorkerTests(unittest.TestCase):
    def start(self,acknowledge=True,legacy=None,tool=None,reconcile=None,created='created',reject_once=False,held=False,legacy_exited=False,start_outside=False,policy=False,unknown=False):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        self.desktop=SimpleNamespace(runtime=Path(temp.name),visible=False,settings={'enabled':True},stop=threading.Event(),input_language_busy=lambda:False,input_language_tool=lambda **_:{'classID':'moveTool','isModal':False})
        if tool is not None:self.desktop.input_language_tool=tool
        if reconcile is not None:self.desktop.input_language_reconcile=reconcile
        self.c=InputMethod(self.desktop);self.layout=0x8040804;self.native_restored=False;self.opened=1;self.kind=50033;self.focus=10;self.revision=1;self.outside=start_outside;self.posts=[]
        if legacy is not None:(self.desktop.runtime/'input-language-owned.json').write_text(json.dumps(legacy),'utf-8')
        def observe():
            if self.outside:return {'state':'outside'}
            value={**sample(), 'identity':[1,2,created,3],'focus':self.focus,'layout':self.layout,'state':'unknown','workspace':True,'pid':2,'class':'PSViewC','accessibility':legacy_tests.FocusAccessibilityTests().row(self.kind)}
            if unknown:value.update(workspace=False,accessibility={})
            return value
        self.c.sample=observe
        def read(value):return {**value,'layout':self.layout,'ime':20,'ime_open':self.opened,'native_restored':self.native_restored,'context_revision':self.revision}
        def owned(_):return read({**sample('outside'),'identity':[1,2,created,3],'focus':self.focus})
        def send(value):
            self.posts.append(value['wanted'])
            if reject_once and len(self.posts)==1:return {'rejected':True}
            if acknowledge:self.opened=value['wanted']
        def layouts(count,values):values[0]=0x8040804;values[1]=0x4090409;return 2
        self.user=SimpleNamespace(GetKeyboardLayout=Mock(return_value=0x8040804),GetKeyboardLayoutList=Mock(side_effect=layouts),PostMessageW=Mock(),GetAsyncKeyState=Mock(side_effect=lambda key:0x8000 if held and key==65 else 0),GetTickCount64=Mock(side_effect=lambda:int(time.monotonic()*1000)))
        self.policy_calls=[]
        native=SimpleNamespace(read=lambda value:{**read(value),'context_nonce':123},send=send,observe_owned=lambda value:{**owned(value),'context_nonce':123},
                                set_canvas_tool=Mock(return_value=1),canvas_correct=Mock(return_value=1),
                                canvas_diagnostics=Mock(return_value={'available':True}))
        self.native_mock=native
        if policy:
            def set_policy(window,enabled):
                self.policy_calls.append(bool(enabled))
                return (1|0x100|(1<<16)) if enabled else 1
            def policy_state(window,status):
                return {'focus_policy':bool(status&0x100),'policy_windows':(status>>16)&0xff,
                        'policy_reason':9 if status&0x100 else 7,'policy_residual':bool(status&0x200),
                        'policy_tick':int(time.monotonic()*1000) if status&0x100 else 0}
            native.set_policy=set_policy;native.policy_state=policy_state
        patches=[patch('backend.input_method.ctypes.WinDLL',return_value=self.user),
                 patch('backend.input_method.ImeContext',return_value=SimpleNamespace(read=read,send=send,observe_owned=owned)),
                 patch('backend.input_method.NativeImeContext',return_value=native),
                 patch('backend.input_method.FocusAccessibility',return_value=object()),
                 patch('backend.input_method.process_exited',side_effect=lambda identity:legacy_exited and identity[2]=='old-process')]
        if policy:patches.append(patch('win32gui.IsWindow',return_value=True))
        for p in patches:
            p.start();self.addCleanup(p.stop)
        self.c.thread.start()
        def stop():self.desktop.stop.set();self.c.wake.set();self.c.thread.join(2);self.assertFalse(self.c.thread.is_alive())
        self.addCleanup(stop)

    def wait(self,predicate):
        end=time.monotonic()+3
        while not predicate():self.assertLess(time.monotonic(),end,self.c.report);time.sleep(.005)

    def test_non_input_input_outside_and_shutdown_restore_without_changing_hkl(self):
        self.start();self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.kind=50004;self.c.activity();self.wait(lambda:self.c.policy.confirmed_restores==1)
        self.kind=50033;self.c.activity();self.wait(lambda:self.c.policy.confirmed_disables==2)
        self.outside=True;self.c.activity();self.wait(lambda:self.c.policy.confirmed_restores==2)
        self.assertEqual(self.posts,[0,1,0,1]);self.assertEqual(self.c.report['layout_requests'],0);self.user.PostMessageW.assert_not_called()
        self.outside=False;self.c.activity();self.wait(lambda:self.c.policy.confirmed_disables==3)
        self.desktop.stop.set();self.c.wake.set();self.c.thread.join(2)
        self.assertEqual(self.posts[-2:],[0,1]);self.assertIsNone(self.c.policy.pending)

    def first_key(self):
        self.desktop.input_focus=SimpleNamespace(shortcut_context=lambda:{'state':'non-input','root':1,'focus':10,'epoch':4})
        original=self.c.ime.read;self.c.ime.read=lambda value:{**original(value),'tsf_disabled':self.opened==0}
        request={'verify_id':1,'root':1,'hwnd':10,'input_epoch':4,'expires_tick_ms':int(time.monotonic()*1000)+200,'need_ime':True}
        reply=Mock();self.c.verify(request,reply);return reply

    def test_verified_first_key_can_protect_while_an_ordinary_key_is_held(self):
        self.start(held=True);self.wait(lambda:self.c.report.get('tool_modal') is False)
        self.assertEqual(self.posts,[])
        reply=self.first_key();self.wait(lambda:reply.called)
        self.assertEqual(reply.call_args.args[1],2);self.assertEqual(self.posts,[0])
        self.assertTrue(self.c.report['first_key_gate']['keys_held']);self.assertTrue(self.c.report['first_key_gate']['verified'])

    def test_verified_first_key_does_not_wait_out_an_old_known_rejection(self):
        self.start(reject_once=True);self.wait(lambda:self.c.report.get('state')=='request-rejected')
        reply=self.first_key();self.wait(lambda:reply.called)
        self.assertEqual(reply.call_args.args[1],2);self.assertEqual(self.posts,[0,0])

    def test_manual_layout_change_rechecks_tool_and_rearms_confirmed_native_restore(self):
        provider=Mock(return_value={'classID':'moveTool','isModal':False})
        self.start(tool=provider);self.wait(lambda:self.c.policy.confirmed_disables==1)
        calls=provider.call_count
        self.layout=0x4090409;self.native_restored=True;self.opened=1;self.c.wake.set()
        self.wait(lambda:self.c.policy.confirmed_disables==2)
        self.assertGreater(provider.call_count,calls);self.assertEqual(self.posts,[0,0])
        self.assertEqual(self.c.policy.owned['layout'],self.layout)
        self.assertIsNone(self.c.policy.pending);self.user.PostMessageW.assert_not_called()
        self.kind=50004;self.c.activity();self.wait(lambda:self.posts==[0,0,1])

    def test_unknown_close_is_not_retried_even_during_shutdown(self):
        self.start(acknowledge=False);self.wait(lambda:len(self.posts)==1)
        self.c.policy.pending['time']=time.monotonic()-2;self.c.activity()
        self.wait(lambda:self.c.policy.state=='request-not-confirmed')
        self.desktop.stop.set();self.c.wake.set();self.c.thread.join(2)
        self.assertEqual(self.posts,[0]);self.assertIsNotNone(self.c.policy.pending)

    def test_paused_or_disabled_product_restores_its_ime_ownership(self):
        self.start();self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.desktop.settings['enabled']=False;self.c.activity();self.wait(lambda:self.c.policy.confirmed_restores==1)
        self.assertEqual(self.posts,[0,1])

    def test_focus_policy_starts_before_uxp_evidence_without_a_lease(self):
        self.start(policy=True,unknown=True)
        self.wait(lambda:self.c.focus_policy)
        self.assertEqual(self.posts,[])
        self.wait(lambda:self.c.report.get('focus_policy'))
        self.assertGreaterEqual(self.c.report.get('policy_windows',0),1)
        self.desktop.settings['enabled']=False;self.c.activity()
        self.wait(lambda:not self.c.focus_policy)
        self.assertEqual(self.policy_calls,[True,False])
        self.assertEqual(self.posts,[])

    def test_canvas_tool_state_is_pushed_to_the_native_context(self):
        self.start(policy=True,unknown=True)
        self.wait(lambda:self.c.focus_policy)
        self.desktop.input_tool_state=lambda:{'classID':'typeCreateOrEditTool','isModal':False,
                                              'sequence':1,'created_at_ms':time.time()*1000}
        self.c.wake.set()
        self.wait(lambda:self.native_mock.set_canvas_tool.called)
        self.assertEqual(self.native_mock.set_canvas_tool.call_args.args,(self.c.policy_window,True))
        self.desktop.input_tool_state=lambda:{'classID':'moveTool','isModal':False,
                                              'sequence':2,'created_at_ms':time.time()*1000}
        self.c.wake.set()
        self.wait(lambda:self.native_mock.set_canvas_tool.call_count>=2)
        self.assertEqual(self.native_mock.set_canvas_tool.call_args.args,(self.c.policy_window,False))

    def test_focus_policy_is_restored_and_confirmed_on_shutdown(self):
        self.start(policy=True)
        self.wait(lambda:self.c.focus_policy)
        self.desktop.stop.set();self.c.wake.set();self.c.thread.join(2)
        self.assertFalse(self.c.thread.is_alive())
        self.assertEqual(self.policy_calls,[True,False])
        self.assertFalse(self.c.focus_policy)
        self.assertEqual(self.posts,[])

    def test_legacy_unknown_layout_request_blocks_new_ime_changes_without_reissue(self):
        owner=legacy_tests.ExitedLanguageOwnerTests().owner();owner['identity'][2]='old-process'
        self.start(legacy={'owned':owner,'pending':{**owner,'switch':False}})
        self.wait(lambda:self.c.report.get('legacy_pending'))
        self.assertEqual(self.posts,[]);self.user.PostMessageW.assert_not_called()
        self.desktop.stop.set();self.c.wake.set();self.c.thread.join(2)
        saved=json.loads((self.desktop.runtime/'input-language-owned.json').read_text())
        self.assertEqual(saved['pending']['identity'],owner['identity']);self.assertEqual(self.posts,[])

    def test_exited_legacy_pending_stays_unknown_and_intact_while_new_process_gets_ime(self):
        owner=legacy_tests.ExitedLanguageOwnerTests().owner();owner['identity'][2]='old-process'
        original={'owned':owner,'pending':{**owner,'switch':False}}
        self.start(legacy=original,legacy_exited=True)
        self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.assertEqual(json.loads((self.desktop.runtime/'input-language-owned.json').read_text()),original)
        self.assertIsNotNone(self.c.legacy.policy.pending);self.assertTrue(self.c.report['legacy_unknown_preserved'])
        archives=list(self.desktop.runtime.glob('input-language-unknown-exited-*.json'));self.assertEqual(len(archives),1)
        evidence=json.loads(archives[0].read_text())
        self.assertEqual(evidence['result_status'],'unknown');self.assertFalse(evidence['original_slot_released'])
        self.assertFalse(evidence['marked_restored']);self.assertEqual(json.loads(evidence['original_journal_utf8']),original)
        self.assertEqual(self.posts,[0]);self.user.PostMessageW.assert_not_called()
        self.kind=50004;self.c.activity();self.wait(lambda:self.c.policy.confirmed_restores==1)
        self.assertEqual(json.loads((self.desktop.runtime/'input-language-owned.json').read_text()),original)

    def test_legacy_archive_failure_preserves_block_and_original_record(self):
        owner=legacy_tests.ExitedLanguageOwnerTests().owner();owner['identity'][2]='old-process'
        original={'owned':owner,'pending':{**owner,'switch':False}}
        with patch('backend.input_method.atomic_json',side_effect=OSError('archive unavailable')):
            self.start(legacy=original,legacy_exited=True)
            self.wait(lambda:self.c.report.get('error_type')=='OSError')
            self.assertEqual(self.posts,[]);self.assertIsNone(self.c.legacy_exited)
            self.assertEqual(json.loads((self.desktop.runtime/'input-language-owned.json').read_text()),original)

    def test_legacy_unknown_bytes_survive_outside_startup_and_new_process_handoff(self):
        owner=legacy_tests.ExitedLanguageOwnerTests().owner();owner['identity'][2]='old-process'
        original={'owned':owner,'pending':{**owner,'switch':False}}
        self.start(legacy=original,legacy_exited=True,start_outside=True)
        path=self.desktop.runtime/'input-language-owned.json';original_bytes=json.dumps(original).encode('utf-8')
        self.wait(lambda:self.c.report.get('legacy_pending'))
        self.assertEqual(path.read_bytes(),original_bytes);self.assertEqual(self.posts,[])
        self.outside=False;self.c.wake.set();self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.assertEqual(path.read_bytes(),original_bytes)
        archive=next(self.desktop.runtime.glob('input-language-unknown-exited-*.json'))
        self.assertEqual(json.loads(archive.read_text())['original_journal_utf8'].encode('utf-8'),original_bytes)

    def test_legacy_pending_same_process_and_changed_journal_never_bypass_protection(self):
        from backend.input_language import InputLanguage
        with tempfile.TemporaryDirectory() as folder:
            desktop=SimpleNamespace(runtime=Path(folder));c=InputMethod(desktop);c.user=Mock()
            c.legacy=InputLanguage(desktop);owner=legacy_tests.ExitedLanguageOwnerTests().owner()
            c.legacy.policy.owned=owner;c.legacy.policy.pending={**owner,'switch':False,'wanted':owner['original']}
            c.legacy.retire_exited_owner=Mock()
            original={'owned':owner,'pending':c.legacy.policy.pending}
            path=desktop.runtime/'input-language-owned.json';path.write_text(json.dumps(original),'utf-8')
            with patch('backend.input_method.process_exited',return_value=True) as exited:
                self.assertFalse(c.legacy_step({**sample('unknown'),'identity':owner['identity'],'layout':owner['wanted']}))
                exited.assert_not_called();self.assertIsNone(c.legacy_exited)
                path.write_text(json.dumps({**original,'pending':{**original['pending'],'wanted':owner['wanted']}}),'utf-8')
                with self.assertRaisesRegex(RuntimeError,'身份已变化'):c.legacy_step({**sample(),'identity':[1,2,'new-process',3]})
            self.assertIsNone(c.legacy_exited);c.user.PostMessageW.assert_not_called()

    def test_known_read_settlement_can_reobserve_without_a_first_keystroke(self):
        value={'classID':'moveTool','isModal':False}
        provider=Mock(side_effect=lambda **_:None if provider.call_count==1 else value)
        self.start(tool=provider,reconcile=lambda:True)
        self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.assertGreaterEqual(provider.call_count,3);self.assertEqual(self.c.snapshot()[0],0);self.assertEqual(self.posts,[0])

    def test_host_edit_exit_invalidates_modal_evidence_without_keyboard_activity(self):
        provider=Mock(return_value={'classID':'typeCreateOrEditTool','isModal':True})
        self.start(tool=provider);self.wait(lambda:self.c.report.get('tool_modal') is True)
        provider.return_value={'classID':'typeCreateOrEditTool','isModal':False}
        self.desktop.input_tool_event=lambda:('session',2,'exit')
        self.c.wake.set();self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.assertEqual(self.c.report['tool_events'],1)
        self.assertGreater(self.c.report['edit_end_events'],0)
        self.assertEqual(self.posts,[0])

    def test_cold_start_readiness_recovers_with_no_keyboard_or_focus_activity(self):
        from backend.input_tool import ToolObservation
        from backend.settings import atomic_json
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup);folder=Path(temp.name)
        config=folder/'config.json';atomic_json(config,{'token':'startup-test'})
        ready=(folder,{'session':'3000-startup','probe_version':'0.3.90'})
        observe=Mock(side_effect=[RuntimeError('UXP not ready'),*([ready]*30)])
        reader=ToolObservation(SimpleNamespace(config=config,observe=observe))
        def publish(source,destination):
            request=json.loads(source.read_text('utf-8'));source.replace(destination)
            atomic_json(folder/'input-tool-response.json',{'schema':1,'id':request['id'],'session':request['session'],'token':'startup-test','phase':'completed','classID':'moveTool','isModal':False,'completed_at_ms':time.time()*1000})
        with patch('backend.input_tool.publish_request',side_effect=publish):
            self.start(tool=reader.read,reconcile=reader.reconcile,created='1970-01-01 00:00:02+00:00')
            self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.assertEqual(self.c.snapshot()[0],0);self.assertEqual(self.posts,[0])
        self.assertFalse(reader.path.exists());self.user.PostMessageW.assert_not_called()

    def test_unknown_read_never_starts_another_host_observation(self):
        provider=Mock(side_effect=RuntimeError('original unknown'))
        self.start(tool=provider,reconcile=lambda:False)
        self.wait(lambda:provider.call_count==1);time.sleep(.12)
        self.assertEqual(provider.call_count,1);self.assertEqual(self.posts,[])

    def test_native_not_armed_rechecks_without_keyboard_or_focus_activity(self):
        provider=Mock(return_value={'classID':'moveTool','isModal':False})
        self.start(tool=provider,reject_once=True)
        self.wait(lambda:self.c.report.get('state')=='request-rejected')
        calls=provider.call_count;time.sleep(.12)
        self.assertEqual(self.posts,[0])
        self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.assertGreater(provider.call_count,calls)
        self.assertEqual(self.c.snapshot()[0],0);self.assertEqual(self.posts,[0,0])
        self.assertEqual(len(list(self.desktop.runtime.glob('input-method-rejected-*.json'))),1)

    def test_focus_roundtrip_drops_previous_tool_evidence_without_key_activity(self):
        provider=Mock(return_value={'classID':'moveTool','isModal':False})
        self.start(tool=provider);self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.focus=11;self.kind=None;self.c.wake.set()
        self.wait(lambda:self.c.policy.confirmed_restores==1)
        calls=provider.call_count
        self.focus=10;self.kind=50033;self.c.wake.set()
        self.wait(lambda:self.c.policy.confirmed_disables==2)
        self.assertGreaterEqual(provider.call_count-calls,2)
        self.assertEqual(self.c.snapshot()[0],0);self.assertEqual(self.posts,[0,1,0])

    def test_rejected_disable_rechecks_input_instead_of_reusing_idle_tool(self):
        provider=Mock(return_value={'classID':'moveTool','isModal':False})
        self.start(tool=provider,reject_once=True)
        self.wait(lambda:self.c.report.get('state')=='request-rejected')
        provider.return_value={'classID':'typeCreateOrEditTool','isModal':True}
        self.wait(lambda:self.c.report.get('state')=='input')
        time.sleep(1.1)
        self.assertEqual(self.posts,[0]);self.assertIsNone(self.c.policy.pending)
        self.assertIsNone(self.c.policy.owned);self.assertEqual(self.c.snapshot()[0],0)

    def test_fresh_successful_sample_clears_stale_native_startup_error(self):
        self.start();self.wait(lambda:self.c.policy.confirmed_disables==1)
        read=self.c.ime.read;error=OSError('startup timeout');error.winerror=1460
        self.c.ime.read=Mock(side_effect=error);self.c.wake.set()
        self.wait(lambda:self.c.report.get('native_error_code')==1460)
        self.c.ime.read=read;self.c.wake.set()
        self.wait(lambda:self.c.report.get('state')=='non-input' and 'native_error_code' not in self.c.report)
        self.assertNotIn('error_type',self.c.report);self.assertEqual(self.posts,[0])

    def test_transient_unknown_recovers_in_same_focus_without_activity(self):
        provider=Mock(return_value={'classID':'moveTool','isModal':False})
        self.start(tool=provider);self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.kind=None;self.c.wake.set();self.wait(lambda:self.c.policy.confirmed_restores==1)
        calls=provider.call_count;self.kind=50033;self.c.wake.set()
        time.sleep(.15);self.assertEqual(self.posts,[0,1])
        self.wait(lambda:self.c.policy.confirmed_disables==2)
        self.assertGreater(provider.call_count,calls)
        self.assertEqual(self.c.snapshot()[0],0);self.assertEqual(self.focus,10)

    def test_modal_tool_end_is_reobserved_in_same_focus_without_activity(self):
        provider=Mock(return_value={'classID':'typeCreateOrEditTool','isModal':True})
        self.start(tool=provider);self.wait(lambda:self.c.report.get('state')=='input')
        provider.return_value={'classID':'moveTool','isModal':False}
        self.wait(lambda:self.c.policy.confirmed_disables==1)
        calls=provider.call_count;time.sleep(.2)
        self.assertEqual(provider.call_count,calls,'Healthy protection must not poll the host')
        self.assertEqual(self.c.snapshot()[0],0);self.assertEqual(self.posts,[0])

    def test_native_revision_invalidates_cached_tool_before_reprotection(self):
        provider=Mock(return_value={'classID':'moveTool','isModal':False})
        self.start(tool=provider);self.wait(lambda:self.c.policy.confirmed_disables==1)
        provider.return_value={'classID':'typeCreateOrEditTool','isModal':True}
        self.revision+=1;self.native_restored=True;self.opened=1;self.c.wake.set()
        self.wait(lambda:self.c.report.get('state')=='input')
        self.assertEqual(self.posts,[0]);self.assertIsNone(self.c.policy.pending)
        provider.return_value={'classID':'moveTool','isModal':False}
        self.wait(lambda:self.c.policy.confirmed_disables==2)
        self.assertEqual(self.c.snapshot()[0],0)

    def test_unstable_or_input_evidence_cannot_rearm_after_unknown(self):
        self.start();self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.kind=None;self.c.wake.set();self.wait(lambda:self.c.policy.confirmed_restores==1)
        for _ in range(5):
            self.kind=50033;self.c.wake.set();time.sleep(.08)
            self.kind=None;self.c.wake.set();time.sleep(.08)
        self.kind=50004;self.c.wake.set();self.wait(lambda:self.c.report.get('state')=='input')
        self.assertEqual(self.posts,[0,1]);self.assertIsNone(self.c.policy.pending)

    def test_broken_accessibility_client_is_rebuilt_without_focus_activity(self):
        self.start();self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.kind=None;self.c.report['uia_error_type']='COMError';self.c.wake.set()
        self.wait(lambda:self.c.policy.confirmed_restores==1)
        replacement=object()
        with patch('backend.input_method.FocusAccessibility',return_value=replacement) as factory:
            self.wait(lambda:self.c.accessibility is replacement)
            self.c.report.pop('uia_error_type');self.kind=50033;self.c.wake.set()
            self.wait(lambda:self.c.policy.confirmed_disables==2)
            self.assertEqual(factory.call_count,1)
        self.assertEqual(self.c.snapshot()[0],0);self.assertEqual(self.posts,[0,1,0])

    def test_context_change_during_final_read_rejects_old_non_input_evidence(self):
        provider=Mock(return_value={'classID':'moveTool','isModal':False})
        self.start(tool=provider);self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.kind=None;self.c.wake.set();self.wait(lambda:self.c.policy.confirmed_restores==1)
        read=self.c.ime.read;changed=threading.Event()
        def race(value):
            if self.c.policy.pending and self.c.policy.pending['disable'] and not changed.is_set():
                self.revision+=1;provider.return_value={'classID':'typeCreateOrEditTool','isModal':True};changed.set()
            return read(value)
        self.c.ime.read=race;self.kind=50033;self.c.wake.set()
        self.wait(changed.is_set);self.wait(lambda:self.c.report.get('state')=='input')
        self.assertEqual(self.posts,[0,1]);self.assertIsNone(self.c.policy.pending)

    def test_canvas_editing_protects_search_then_edit_exit_rearms_ime(self):
        provider=Mock(return_value={'classID':'typeCreateOrEditTool','isModal':True})
        self.start(tool=provider)
        self.wait(lambda:self.c.report.get('state')=='input')
        self.assertEqual(self.posts,[])
        provider.return_value={'classID':'typeCreateOrEditTool','isModal':False}
        self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.assertEqual(self.c.report['layout_requests'],0)

    def test_commit_and_mouse_exit_recheck_a_still_modal_first_reply(self):
        modal={'classID':'typeCreateOrEditTool','isModal':True}
        idle={'classID':'typeCreateOrEditTool','isModal':False}
        provider=Mock(return_value=modal)
        self.start(tool=provider);self.wait(lambda:self.c.report.get('state')=='input')
        provider.side_effect=[modal,idle,idle];started=time.monotonic();self.c.edit_end()
        self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.assertLess(time.monotonic()-started,.9,'Commit must not wait for the one-second modal cache')
        provider.side_effect=None;provider.return_value=modal;self.c.protect()
        self.wait(lambda:self.c.policy.confirmed_restores==1)
        provider.side_effect=[modal,idle,idle];started=time.monotonic();self.c.click(10,1)
        self.wait(lambda:self.c.policy.confirmed_disables==2)
        self.assertLess(time.monotonic()-started,.9)
        self.assertEqual(self.posts,[0,1,0])

    def test_commit_with_transient_unknown_uses_fresh_idle_proof_without_fault_delay(self):
        provider=Mock(return_value={'classID':'typeCreateOrEditTool','isModal':True})
        self.start(tool=provider);self.wait(lambda:self.c.report.get('state')=='input')
        self.kind=None;self.c.edit_end();self.wait(lambda:self.c.report.get('state')=='unknown')
        self.assertEqual(self.posts,[])
        provider.return_value={'classID':'typeCreateOrEditTool','isModal':False};self.kind=50033;started=time.monotonic();self.c.wake.set()
        self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.assertLess(time.monotonic()-started,.9)

    def test_slow_modal_reply_cannot_expire_commit_revalidation(self):
        modal={'classID':'typeCreateOrEditTool','isModal':True}
        idle={'classID':'typeCreateOrEditTool','isModal':False}
        provider=Mock(return_value=modal)
        self.start(tool=provider);self.wait(lambda:self.c.report.get('state')=='input')
        def slow():
            time.sleep(.6);return modal
        replies=iter([slow,lambda:idle,lambda:idle])
        provider.side_effect=lambda **_:next(replies)()
        started=time.monotonic();self.c.edit_end()
        self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.assertLess(time.monotonic()-started,1,'Do not add the one-second modal cache after a slow reply')

    def test_verified_own_menu_keeps_ps_lease_but_foreign_focus_and_text_restore_it(self):
        self.start();self.wait(lambda:self.c.policy.confirmed_disables==1)
        observe=self.c.ime.observe_owned
        held=True
        self.c.ime.observe_owned=lambda value:{**observe(value),'native_menu_held':held}
        self.desktop.input_focus=SimpleNamespace(shortcut_context=lambda:{'state':'non-input','root':1,'focus':10})
        self.outside=True;self.desktop.visible=True;self.c.wake.set()
        self.wait(lambda:self.c.report.get('own_menu_held'))
        self.assertEqual(self.posts,[0]);self.assertTrue(self.c.policy.owned)
        held=False;self.c.wake.set();self.wait(lambda:self.c.policy.confirmed_restores==1)
        self.assertEqual(self.posts,[0,1])

    def test_immediate_rejection_persists_cleared_pending_even_when_state_returns_to_empty(self):
        self.start(reject_once=True)
        self.wait(lambda:self.c.report.get('state')=='request-rejected')
        self.desktop.stop.set();self.c.wake.set();self.c.thread.join(2)
        self.assertFalse(self.c.thread.is_alive())
        path=self.desktop.runtime/'input-method-owned.json'
        saved=json.loads(path.read_text('utf-8'))
        self.assertIsNone(saved['pending']);self.assertIsNone(saved['owned'])

    def test_nonactivating_caps_close_and_known_busy_end_rearm_without_a_keystroke(self):
        self.start();self.wait(lambda:self.c.policy.confirmed_disables==1)
        self.desktop.visible=True;self.wait(lambda:self.c.policy.confirmed_restores==1)
        self.desktop.visible=False;self.wait(lambda:self.c.policy.confirmed_disables==2)
        self.desktop.input_language_busy=lambda:True;self.wait(lambda:self.c.policy.confirmed_restores==2)
        self.desktop.input_language_busy=lambda:False;self.wait(lambda:self.c.policy.confirmed_disables==3)
        self.assertEqual(self.c.snapshot()[0],0);self.assertEqual(self.posts,[0,1,0,1,0])
