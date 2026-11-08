import tempfile
import threading
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import ANY, Mock

from backend.input_method import InputMethod, type_tool


class CanvasControlTests(unittest.TestCase):
    def make(self):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        desktop=SimpleNamespace(runtime=Path(temp.name),visible=False,settings={'enabled':True},
                                stop=threading.Event(),input_language_busy=lambda:False)
        controller=InputMethod(desktop)
        controller.focus_policy=True;controller.policy_window=20
        controller.ime=SimpleNamespace(set_canvas_tool=Mock(return_value=1),
                                       canvas_correct=Mock(return_value=1),
                                       command=Mock(return_value=1),
                                       canvas_diagnostics=Mock(return_value={'available':True}))
        return controller

    def test_type_tool_names_cover_horizontal_vertical_and_mask(self):
        for name in ('typeCreateOrEditTool','typeVerticalCreateOrEditTool','typeMaskTool'):
            self.assertTrue(type_tool(name),name)
        for name in ('moveTool','typewriterTool','typeToolExtra','TypeTool',''):
            self.assertFalse(type_tool(name),name)

    def test_type_tool_push_only_on_change_and_retries_after_failure(self):
        controller=self.make()
        controller.push_type_tool({'classID':'typeCreateOrEditTool'})
        controller.push_type_tool({'classID':'typeCreateOrEditTool'})
        controller.ime.set_canvas_tool.assert_called_once_with(20,True)
        controller.ime.set_canvas_tool.side_effect=RuntimeError('busy')
        controller.push_type_tool({'classID':'moveTool'})
        self.assertTrue(controller.canvas_tool)  # failed push is not recorded
        controller.ime.set_canvas_tool.side_effect=None
        controller.push_type_tool({'classID':'moveTool'})
        self.assertEqual(controller.ime.set_canvas_tool.call_args_list[-1].args,(20,False))
        self.assertFalse(controller.canvas_tool)
        controller.push_type_tool({'classID':'typeMaskTool'})
        self.assertTrue(controller.canvas_tool)

    def test_modal_events_are_ordered_and_timestamped(self):
        controller=self.make()
        controller.canvas_event({'session':'s','sequence':2,'state':'enter','created_at_ms':111}, {})
        controller.ime.canvas_correct.assert_called_once_with(20,True,111)
        controller.canvas_event({'session':'s','sequence':2,'state':'enter','created_at_ms':111}, {})
        self.assertEqual(controller.ime.canvas_correct.call_count,1)
        controller.canvas_event({'session':'s','sequence':3,'state':'exit','created_at_ms':222}, {})
        self.assertEqual(controller.ime.canvas_correct.call_args_list[-1].args,(20,False,222))
        controller.canvas_event({'session':'s','sequence':2,'state':'enter','created_at_ms':333}, {})
        self.assertEqual(controller.ime.canvas_correct.call_count,2)
        self.assertFalse(controller.canvas_modal_state)

    def test_new_nonmodal_snapshot_after_800ms_rolls_back_once(self):
        controller=self.make();controller.modal_events_seen=1
        controller.canvas_state=True;controller.canvas_modal_seen=False
        controller.canvas_entry_at=time.monotonic()-1;controller.canvas_entry_tool_seq=0
        controller.canvas_entry_wall_ms=2
        sample={'canvas_text_editing':True,'canvas_entry_pending':True,
                'type_tool_active':True,'type_tool_known':True}
        state={'classID':'typeCreateOrEditTool','isModal':False,'sequence':1,'created_at_ms':3}
        controller.canvas_tick(sample,state)
        controller.ime.canvas_correct.assert_called_once_with(20,False,ANY)
        self.assertEqual(controller.canvas_timeouts,1)
        self.assertIsNone(controller.canvas_entry_at)
        controller.canvas_tick(sample,state)
        self.assertEqual(controller.ime.canvas_correct.call_count,1)

    def test_newer_modal_tool_state_prevents_the_timeout(self):
        controller=self.make();controller.modal_events_seen=1
        controller.canvas_state=True;controller.canvas_modal_seen=False
        controller.canvas_entry_at=time.monotonic()-1;controller.canvas_entry_tool_seq=3
        sample={'canvas_text_editing':True,'canvas_entry_pending':True,
                'type_tool_active':True,'type_tool_known':True}
        state={'classID':'typeCreateOrEditTool','isModal':True,'sequence':4,'created_at_ms':2}
        controller.canvas_tick(sample,state)
        controller.ime.canvas_correct.assert_not_called()
        self.assertTrue(controller.canvas_modal_seen)

    def test_missed_entry_restores_a_modal_type_edit_once(self):
        controller=self.make();controller.canvas_modal_state=True
        sample={'canvas_text_editing':False,'canvas_entry_pending':False,
                'type_tool_active':True,'type_tool_known':True}
        state={'classID':'typeCreateOrEditTool','isModal':True,'sequence':5,'created_at_ms':555}
        controller.canvas_tick(sample,state)
        controller.ime.canvas_correct.assert_called_once_with(20,True,555)
        controller.canvas_tick(sample,state)
        self.assertEqual(controller.ime.canvas_correct.call_count,1)

    def test_stale_tool_modal_after_exit_does_not_reopen_the_canvas(self):
        controller=self.make();controller.canvas_modal_state=False
        sample={'canvas_text_editing':False,'canvas_entry_pending':False,
                'type_tool_active':True,'type_tool_known':True}
        state={'classID':'typeCreateOrEditTool','isModal':True,'sequence':9,'created_at_ms':900}
        controller.canvas_tick(sample,state)
        controller.ime.canvas_correct.assert_not_called()

    def test_missing_events_and_unchanged_state_cannot_disable_an_edit(self):
        controller=self.make();controller.modal_events_seen=1
        controller.canvas_state=True;controller.canvas_modal_seen=False
        controller.canvas_entry_at=time.monotonic()-1
        sample={'canvas_text_editing':True,'canvas_entry_pending':True,
                'type_tool_active':True,'type_tool_known':True}
        controller.canvas_tick(sample,{'classID':'moveTool','isModal':False,'sequence':0,'created_at_ms':1})
        controller.canvas_tick(sample,None)
        controller.canvas_entry_wall_ms=2
        controller.canvas_tick(sample,{'classID':'moveTool','isModal':False,'sequence':1,'created_at_ms':1})
        controller.ime.canvas_correct.assert_not_called()

    def test_plugin_session_restart_accepts_a_new_sequence(self):
        controller=self.make()
        controller.canvas_event({'session':'old','sequence':20,'state':'exit','created_at_ms':111}, {})
        controller.canvas_event({'session':'new','sequence':1,'state':'enter','created_at_ms':222}, {})
        self.assertEqual(controller.ime.canvas_correct.call_args_list[-1].args,(20,True,222))
        self.assertTrue(controller.canvas_modal_state)

    def test_diagnostics_merge_native_trace_and_counts(self):
        controller=self.make()
        controller.ime.command.return_value=0x1_00000001
        controller.ime.canvas_diagnostics.return_value={'available':True,'entry_trigger':1,'policy_trace':[{'action':1}]}
        row=controller.canvas_diagnostics()
        self.assertTrue(row['available']);self.assertEqual(row['entry_trigger'],1)
        self.assertEqual(row['modal_events'],0);self.assertEqual(row['timeouts'],0)
        controller.ime.command.side_effect=RuntimeError('gone')
        self.assertEqual(controller.canvas_diagnostics()['available'],False)


if __name__=='__main__':
    unittest.main()
