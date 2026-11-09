import sys
import unittest
from types import SimpleNamespace
from unittest.mock import Mock,patch
from backend.input_context import NativeImeContext


class NativeContextTransportTests(unittest.TestCase):
    def test_both_thread_hooks_are_owned_and_partial_install_is_unwound(self):
        import os
        for queued in (0,222):
            value=NativeImeContext.__new__(NativeImeContext);value.controls={};value.hooks={}
            value.library=SimpleNamespace(_handle=7,InputContextHook=1,InputContextQueueHook=2)
            windows={'reads':0}
            def published(hwnd,name):
                if name.endswith('Owner.v1'):return os.getpid()
                if name.endswith('Window.v1'):
                    windows['reads']+=1
                    return 0 if windows['reads']==1 else 123
                return 0
            value.user=SimpleNamespace(SetWindowsHookExW=Mock(side_effect=[111,queued]),UnhookWindowsHookEx=Mock(),
                GetPropW=Mock(side_effect=published))
            value.attach=9;sample={'root':1,'identity':[1,2,'created',3],'state':'unknown'}
            with patch('backend.input_context.ctypes.cast',return_value=8),patch('backend.input_context.ctypes.get_last_error',return_value=5),patch('win32gui.SendMessageTimeout'),patch('win32gui.IsWindow',return_value=True),patch('win32gui.GetClassName',return_value='PhotoshopMenu.InputContext.v1'),patch('win32process.GetWindowThreadProcessId',return_value=[3,2]):
                if queued:
                    self.assertEqual(value.control(sample),123)
                    self.assertEqual([c.args[0] for c in value.user.SetWindowsHookExW.call_args_list],[4,3])
                    value.close();self.assertEqual([c.args[0] for c in value.user.UnhookWindowsHookEx.call_args_list],[111,222])
                else:
                    with self.assertRaises(OSError):value.control(sample)
                    value.user.UnhookWindowsHookEx.assert_called_once_with(111)
                self.assertEqual(value.hooks,{})

    def test_native_profile_command_has_bounded_cold_start_budget(self):
        value=NativeImeContext.__new__(NativeImeContext)
        value.user=SimpleNamespace(GetPropW=Mock(return_value=123));value.message=99
        send=Mock(return_value=(1,35))
        with patch.dict(sys.modules,{'win32gui':SimpleNamespace(SendMessageTimeout=send)}):
            self.assertEqual(value.command(7,2,8),35)
        send.assert_called_once_with(7,99,(123<<8)|2,8,2,1000)

    def context(self):
        value=NativeImeContext.__new__(NativeImeContext)
        value.user=SimpleNamespace(GetPropW=Mock(return_value=123));value.command=Mock(return_value=3)
        return value

    def test_nonce_mismatch_cannot_change_another_native_lease(self):
        value=self.context()
        with self.assertRaisesRegex(RuntimeError,'会话已变化'):value.send({'ime':1,'context_nonce':456,'wanted':0,'focus':2})
        value.command.assert_not_called()

    def test_synchronous_native_readback_confirms_only_matching_result(self):
        value=self.context();request={'ime':1,'context_nonce':123,'wanted':0,'focus':2,'input_restore':False}
        self.assertEqual(value.send(request)['ime_open'],0)
        value.command.return_value=9;self.assertEqual(value.send(request),{'rejected':True})
        value.command.return_value=0;self.assertIsNone(value.send(request))
        value.command.return_value=21;request.update(wanted=1,input_restore=True)
        self.assertEqual(value.send(request)['state'],'input')

    def test_read_exposes_event_subscription_and_revision_without_mutation(self):
        value=self.context();value.control=Mock(return_value=7);value.command.return_value=99
        sample={'root':1,'focus':2,'state':'unknown'}
        result=value.read(sample)
        self.assertEqual(result['ime_open'],0);self.assertTrue(result['native_events'])
        self.assertTrue(result['tsf_disabled']);self.assertEqual(result['context_revision'],123)
        value.command.assert_called_once_with(7,1,2)

    def test_frame_endpoint_binding_is_confirmed_once_and_rebound_after_restart(self):
        value=self.context();value.control=Mock(return_value=7);value.frame_host=lambda:77;value.frame_bindings={}
        value.user.GetPropW=Mock(side_effect=lambda window,name:77 if name.endswith('FrameKey.Host.v1') else 123)
        value.read({'root':1,'focus':2,'state':'unknown'});value.read({'root':1,'focus':2,'state':'unknown'})
        self.assertEqual([c.args for c in value.command.call_args_list],[(7,9,77),(7,1,2),(7,1,2)])
        value.frame_host=lambda:88
        with self.assertRaisesRegex(RuntimeError,'接管未确认'):value.read({'root':1,'focus':2,'state':'unknown'})
        self.assertEqual(value.frame_bindings[7],77)

    def test_frame_binding_timeout_is_not_republished(self):
        value=self.context();value.control=Mock(return_value=7);value.frame_host=lambda:77;value.frame_bindings={}
        value.command.side_effect=OSError('timeout');value.user.GetPropW.return_value=0
        with self.assertRaises(OSError):value.read({'root':1,'focus':2})
        self.assertEqual(value.frame_bindings[7],('pending',77))
        with self.assertRaisesRegex(RuntimeError,'不重复发送'):value.read({'root':1,'focus':2})
        self.assertEqual(value.command.call_count,1)

    def test_reading_or_changed_layout_cannot_renew_unowned_context(self):
        value=self.context();owned={'identity':[1,2,'time',3],'layout':4,'ime':5,'focus':6,'context_nonce':7}
        for sample in ({**owned,'state':'outside'},{**owned,'state':'unknown'},{**owned,'state':'non-input','layout':8},{**owned,'state':'non-input','context_nonce':8}):value.renew(owned,sample)
        value.command.assert_not_called();value.renew(owned,{**owned,'state':'non-input'})
        value.command.assert_called_once_with(5,4,6)
