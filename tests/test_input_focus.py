import ctypes
import inspect
import threading
import time
import unittest
from types import SimpleNamespace
from unittest.mock import Mock,patch
from backend.input_focus import InputFocus
from tests import test_input_language as fixtures


class InputFocusTests(unittest.TestCase):
    def start(self):
        self.desktop=SimpleNamespace(stop=threading.Event(),visible=False,
            input_language_tool=Mock(return_value={'classID':'moveTool','isModal':False}),
            input_language_busy=lambda:False,_is_menu_window=lambda _:self.menu,
            _is_photoshop=lambda _:not self.foreign,input_language=Mock())
        self.menu=False;self.foreign=False;self.kind=50033
        self.guard=InputFocus(self.desktop)
        self.guard.sample=lambda:{'root':10,'identity':[10,20,'created',30],'focus':11,'layout':1,
            'state':'unknown','workspace':True,'pid':20,'class':'PSViewC',
            'accessibility':fixtures.FocusAccessibilityTests().row(self.kind,**{'30002':20,'30020':11})}
        self.guard.native_sample=lambda:self.guard.sample()
        user=Mock();user.GetAsyncKeyState.return_value=0;user.GetTickCount64.side_effect=lambda:int(time.monotonic()*1000)
        self.dll=Mock(return_value=user)
        for p in (patch('backend.input_focus.ctypes.WinDLL',self.dll),
                  patch('backend.input_focus.FocusAccessibility',return_value=object()),
                  patch('win32gui.GetForegroundWindow',return_value=10)):
            p.start();self.addCleanup(p.stop)
        self.guard.thread.start()
        def stop():
            self.desktop.stop.set();self.guard.wake.set();self.guard.thread.join(2)
            self.assertFalse(self.guard.thread.is_alive())
        self.addCleanup(stop)

    def wait(self,test):
        end=time.monotonic()+2
        while not test():self.assertLess(time.monotonic(),end,self.guard.report);time.sleep(.005)

    def test_guard_does_not_depend_on_ime_or_expire_during_tool_keys(self):
        self.start();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        first=self.guard.shortcut_context()
        self.desktop.input_language.report={'pending':True,'state':'request-not-confirmed'}
        for _ in range(1000):self.assertEqual(self.guard.shortcut_context(),first)
        time.sleep(.12);self.assertEqual(self.guard.shortcut_context(),first)
        self.assertEqual(self.desktop.input_language_tool.call_count,1)
        self.assertIn('kernel32',[call.args[0] for call in self.dll.call_args_list])
        self.assertNotIn('input_method',inspect.getsource(__import__('backend.input_focus',fromlist=['x'])))

    def test_text_and_hard_intent_protect_immediately_and_return_can_be_reobserved(self):
        self.start();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        self.guard.protect();self.assertEqual(self.guard.shortcut_context()['state'],'unknown')
        self.desktop.input_language_tool.return_value={'classID':'typeCreateOrEditTool','isModal':True}
        self.guard.refresh();self.wait(lambda:self.guard.shortcut_context()['state']=='input')
        self.desktop.input_language_tool.return_value={'classID':'typeCreateOrEditTool','isModal':False}
        self.guard.edit_end();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        self.kind=50004;self.guard.wake.set();self.wait(lambda:self.guard.shortcut_context()['state']=='input')
        self.kind=50033;self.guard.wake.set();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')

    def test_own_menu_and_same_focus_notification_preserve_guard_but_foreign_clears_it(self):
        self.start();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        first=self.guard.shortcut_context();self.menu=True;self.desktop.visible=True
        self.guard.focus_changed(11);time.sleep(.08);self.assertEqual(self.guard.shortcut_context(),first)
        self.menu=False;self.desktop.visible=False;self.guard.focus_changed(11)
        self.wait(lambda:self.desktop.input_language_tool.call_count>=2)
        self.assertEqual(self.guard.shortcut_context(),first)
        self.foreign=True;self.guard.focus_changed(99)
        self.assertEqual(self.guard.shortcut_context()['state'],'unknown')

    def test_late_read_cannot_overwrite_an_edit_intent(self):
        guard=InputFocus(SimpleNamespace());guard.publish('non-input',{'root':10,'focus':11},0)
        guard.protect();guard.publish('non-input',{'root':10,'focus':11},0)
        self.assertEqual(guard.shortcut_context()['state'],'unknown')

    def request(self,need_ime=False,**changes):
        return {'verify_id':1,'root':10,'hwnd':11,'input_epoch':self.guard.native_epoch,
                'expires_tick_ms':int(time.monotonic()*1000)+1000,'need_ime':need_ime,**changes}

    def test_first_f_verification_is_fresh_and_never_waits_for_ime(self):
        self.start();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        first=self.desktop.input_language_tool.call_count;reply=Mock()
        self.desktop.input_language.report={'pending':True}
        self.guard.verify(self.request(),reply);self.wait(lambda:reply.called)
        self.assertEqual(reply.call_args.args[1],2)
        self.assertGreater(self.desktop.input_language_tool.call_count,first)
        self.desktop.input_language.verify.assert_not_called()

    def test_input_guard_returns_letter_state_without_touching_ime(self):
        self.start();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        letter=Mock();self.guard.verify(self.request(True),letter)
        self.wait(lambda:letter.called);self.assertEqual(letter.call_args.args[1],2)
        self.desktop.input_language.verify.assert_not_called()
        search=Mock();self.guard.verify(self.request(verify_id=2),search);self.wait(lambda:search.called)
        self.assertEqual(search.call_args.args[1],2)

    def test_coordinator_requires_ime_only_for_letters_and_never_for_f(self):
        from backend.desktop import Desktop
        desktop=Desktop.__new__(Desktop);desktop.input_language=Mock();desktop._input_verify_reply=Mock()
        search={'need_ime':False};Desktop._input_focus_verified(desktop,search,2)
        desktop._input_verify_reply.assert_called_once_with(search,2);desktop.input_language.verify.assert_not_called()
        letter={'need_ime':True};Desktop._input_focus_verified(desktop,letter,2)
        desktop.input_language.verify.assert_called_once_with(letter,desktop._input_verify_reply)

    def test_changed_or_expired_verification_never_gets_permission(self):
        self.start();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        for changes in ({'hwnd':99},{'input_epoch':99},{'expires_tick_ms':0}):
            reply=Mock();self.guard.verify(self.request(**changes),reply);self.wait(lambda:reply.called)
            self.assertEqual(reply.call_args.args[1],0)

    def test_read_started_before_first_key_cannot_approve_it(self):
        self.start();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        started=threading.Event();release=threading.Event();self.addCleanup(release.set)
        calls=[]
        def read(**_):
            calls.append(1)
            if len(calls)==1:started.set();release.wait(1);return {'classID':'moveTool','isModal':False}
            return {'classID':'typeCreateOrEditTool','isModal':True}
        self.desktop.input_language_tool.side_effect=read;self.guard.refresh();self.assertTrue(started.wait(1))
        reply=Mock();self.guard.verify(self.request(),reply)
        self.assertEqual(self.guard.shortcut_context()['state'],'unknown')
        release.set();self.wait(lambda:reply.called)
        self.assertEqual(reply.call_args.args[1],1);self.assertGreaterEqual(len(calls),2)

    def test_host_exit_event_discards_modal_cache_without_a_key_or_focus_change(self):
        self.start()
        self.desktop.input_language_tool.return_value={'classID':'typeCreateOrEditTool','isModal':True}
        self.guard.protect();self.wait(lambda:self.guard.shortcut_context()['state']=='input')
        self.desktop.input_language_tool.return_value={'classID':'typeCreateOrEditTool','isModal':False}
        self.desktop.input_tool_event=lambda:('session',2,'exit')
        self.guard.wake.set();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        self.assertEqual(self.guard.report['tool_events'],1)
        count=self.desktop.input_language_tool.call_count
        time.sleep(.1);self.assertEqual(self.desktop.input_language_tool.call_count,count)

    def test_unavailable_tool_remains_unknown_without_repeated_reads_until_reconciled(self):
        self.start();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        self.desktop.input_language_tool.side_effect=RuntimeError('UXP not ready')
        self.guard.refresh();self.wait(lambda:self.guard.report.get('tool_error_type')=='RuntimeError')
        calls=self.desktop.input_language_tool.call_count
        time.sleep(.16);self.assertEqual(self.desktop.input_language_tool.call_count,calls)
        self.assertEqual(self.guard.shortcut_context()['state'],'unknown')
        self.desktop.input_language_tool.side_effect=None
        self.desktop.input_language_reconcile=Mock(side_effect=lambda:True)
        self.guard.wake.set();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        self.assertEqual(self.desktop.input_language_tool.call_count,calls+1)
        self.assertNotIn('tool_error_type',self.guard.report)

    def test_uia_unavailable_canvas_path_reads_tool_and_still_requires_current_click(self):
        self.start();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        original=self.guard.sample
        self.guard.sample=lambda:{**original(),'accessibility':{}}
        self.desktop.panel_context=SimpleNamespace(describe=lambda hwnd,root:'canvas')
        self.guard.accessibility=None
        self.guard.click(11,10)
        self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        self.assertGreater(self.desktop.input_language_tool.call_count,1)
        self.desktop.input_language_tool.return_value={'classID':'typeCreateOrEditTool','isModal':True}
        self.guard.refresh();self.wait(lambda:self.guard.shortcut_context()['state']=='input')
        self.guard.protect();self.assertIsNone(self.guard.snapshot()[3])
        self.wait(lambda:self.guard.shortcut_context()['state']=='unknown')

    def test_uia_initialization_recovers_without_a_new_key_or_focus_notification(self):
        self.start();self.wait(lambda:self.guard.shortcut_context()['state']=='non-input')
        self.guard.accessibility=None;self.guard.report['uia_error_type']='COMError'
        self.guard.wake.set()
        self.wait(lambda:self.guard.accessibility is not None)
        self.assertNotIn('uia_error_type',self.guard.report)

    def test_native_generation_change_and_hook_restart_require_new_observation(self):
        guard=InputFocus(SimpleNamespace())
        guard.publish('non-input',{'root':10,'focus':11},0)
        guard.protect(5)
        self.assertEqual(guard.shortcut_context(),{'state':'unknown','root':0,'focus':0,'epoch':5})
        guard.publish('non-input',{'root':10,'focus':11},0)
        self.assertEqual(guard.shortcut_context()['state'],'unknown')
        guard.publish('non-input',{'root':10,'focus':11},guard.snapshot()[0])
        self.assertEqual(guard.shortcut_context()['epoch'],5)
        guard.reconcile_epoch(0)
        self.assertEqual(guard.shortcut_context(),{'state':'unknown','root':0,'focus':0,'epoch':0})

    def test_search_origin_survives_only_same_context_return_and_never_a_new_edit(self):
        guard=InputFocus(SimpleNamespace());guard.publish('non-input',{'root':10,'focus':11},0)
        guard.menu_open('search')
        self.assertTrue(guard.menu_return({'root':10,'focus':11,'state':'unknown'},0))
        self.assertFalse(guard.menu_return({'root':10,'focus':12,'state':'unknown'},0))
        self.assertFalse(guard.menu_return({'root':10,'focus':11,'state':'input'},0))
        guard.protect(1)
        self.assertFalse(guard.menu_return({'root':10,'focus':11,'state':'unknown'},guard.snapshot()[0]))
        guard.publish('non-input',{'root':10,'focus':11},guard.snapshot()[0]);guard.menu_open('split')
        self.assertFalse(guard.menu_return({'root':10,'focus':11,'state':'unknown'},guard.snapshot()[0]))

    def test_search_return_publishes_before_a_late_worker_and_keeps_input_protection(self):
        desktop=SimpleNamespace(_is_photoshop=lambda hwnd:hwnd==10,_is_menu_window=lambda hwnd:False)
        guard=InputFocus(desktop)
        guard.native_sample=Mock(return_value={'root':10,'focus':11,'state':'unknown'})
        guard.publish('non-input',{'root':10,'focus':11},0);guard.menu_open('search')
        guard.publish('unknown',{},0)
        guard.focus_changed(10,0)
        self.assertEqual(guard.shortcut_context(),{'state':'non-input','root':10,'focus':11,'epoch':0})
        guard.publish('unknown',{},0)
        self.assertEqual(guard.shortcut_context()['state'],'non-input')
        guard.native_sample.return_value={'root':10,'focus':11,'state':'input'}
        guard.publish('unknown',{},guard.snapshot()[0]);guard.focus_changed(10,0)
        self.assertEqual(guard.shortcut_context()['state'],'unknown')
        guard.native_sample.return_value={'root':10,'focus':12,'state':'unknown'}
        guard.focus_changed(10,0)
        self.assertEqual(guard.shortcut_context()['state'],'unknown')
        guard.native_sample.return_value={'root':10,'focus':11,'state':'unknown'}
        guard.focus_changed(10,1)
        self.assertEqual(guard.shortcut_context()['state'],'unknown')
