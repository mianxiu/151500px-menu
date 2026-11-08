import threading,unittest
from types import SimpleNamespace
from unittest.mock import Mock
from backend.desktop import Desktop
class LayerFrameGuardTests(unittest.TestCase):
 def setUp(self):
  self.context={'state':'non-input','root':10,'focus':11,'epoch':3};self.guard={'root':10,'focus':11,'epoch':3}
  self.desktop=Desktop.__new__(Desktop);self.desktop.window_lock=threading.RLock();self.desktop.require_action_focus=Mock();self.desktop.input_focus=SimpleNamespace(shortcut_context=lambda:dict(self.context))
 def test_current_identity_is_required(self):
  self.desktop.require_layer_frame(self.guard);self.context['epoch']=4
  with self.assertRaisesRegex(RuntimeError,'上下文已改变'):self.desktop.require_layer_frame(self.guard)
 def test_input_unknown_and_foreign_focus_are_rejected(self):
  for state in ('input','unknown'):
   self.context['state']=state
   with self.assertRaisesRegex(RuntimeError,'未确认'):self.desktop.require_layer_frame(self.guard)
  self.context['state']='non-input';self.desktop.require_action_focus.side_effect=RuntimeError('foreign')
  with self.assertRaisesRegex(RuntimeError,'foreign'):self.desktop.require_layer_frame(self.guard)
 def test_reserved_operation_uses_fresh_native_identity(self):
  accepted=self.desktop.require_layer_frame(self.guard);self.context['state']='unknown'
  self.desktop.input_focus.native_epoch=3;sample={'root':10,'focus':11,'workspace':True,'blocked':False,'state':'unknown'};self.desktop.input_focus.native_sample=lambda:dict(sample)
  self.desktop.require_layer_frame(accepted,reserved=True)
  for changed in ({'focus':12},{'state':'input'},{'blocked':True}):
   old=dict(sample);sample.update(changed)
   with self.assertRaisesRegex(RuntimeError,'原生焦点'):self.desktop.require_layer_frame(accepted,reserved=True)
   sample=old
  self.desktop.input_focus.native_epoch=4
  with self.assertRaises(RuntimeError):self.desktop.require_layer_frame(accepted,reserved=True)
 def test_malformed_identity_is_rejected(self):
  for value in ({'root':10},{**self.guard,'epoch':True},{**self.guard,'focus':0}):
   with self.assertRaises(ValueError):self.desktop.require_layer_frame(value)
if __name__=='__main__':unittest.main()
