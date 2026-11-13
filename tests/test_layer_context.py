import time,unittest,json,tempfile,threading
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock,patch
from backend.layer_context import validate,flags,require_menu_target,unavailable
from backend.tasks import TaskCancelled
from backend.desktop import Desktop
from backend.uxp_bridge import UxpBridge

class LayerContextTests(unittest.TestCase):
    def setUp(self):
        self.now=time.time()*1000
        self.value={'schema':1,'session':'one','document_id':12,'layer_ids':[2],
                    'kind':'pixel','mask':15,'plugin_time_ms':self.now,'same_parent':True}
    def test_rejects_stale_future_session_and_partial_selection(self):
        for changes in ({'session':'old'},{'plugin_time_ms':self.now-2100},
                        {'plugin_time_ms':self.now+1},{'layer_ids':[2,2]},
                        {'layer_ids':[True]},{'mask':16},{'kind':'none'},
                        {'background':True},{'kind':'unknown'}):
            self.assertEqual(validate({**self.value,**changes},'one',self.now)['mask'],0)
        self.assertEqual(validate(self.value,'one',self.now)['mask'],15)
    def test_packed_metadata_preserves_old_panel_bits(self):
        value=validate(self.value,'one',self.now)
        packed=flags(value)
        self.assertEqual(packed&63,0)
        self.assertTrue(packed&64);self.assertEqual((packed>>8)&15,15)
        self.assertEqual((packed>>12)&15,2);self.assertEqual((packed>>17)&255,1)
        self.assertEqual((flags(unavailable())>>8)&15,0)
    def test_quick_capabilities_use_high_bits_and_frozen_selection(self):
        value={**self.value,'quick_mask':127}
        self.assertEqual(flags(value)>>32,127)
        require_menu_target(value,'layer.new',value)
        require_menu_target(value,'adjustment.blackWhite',value)
        with self.assertRaisesRegex(RuntimeError,'不可用'):
            require_menu_target({**value,'quick_mask':3},'layer.group',value)
        with self.assertRaisesRegex(RuntimeError,'已改变'):
            require_menu_target(value,'layer.solid',{**value,'document_id':99})
        self.assertEqual(validate({**value,'quick_mask':128},'one',self.now)['mask'],0)
    def test_frozen_target_rejects_document_session_and_selection_changes(self):
        require_menu_target(self.value,'label.red',self.value)
        for changes in ({'session':'two'},{'document_id':99},{'layer_ids':[3]}):
            with self.assertRaisesRegex(RuntimeError,'已改变'):
                require_menu_target(self.value,'label.red',{**self.value,**changes})
        with self.assertRaisesRegex(RuntimeError,'不可用'):
            require_menu_target({**self.value,'mask':2},'moveToTop',self.value)
    def test_dispatch_uses_frozen_snapshot_without_reading_provider(self):
        desktop=Desktop('http://localhost',Mock())
        desktop.layer_context_provider=Mock(side_effect=AssertionError('No IPC in menu dispatch'))
        desktop.menu_layer_context=dict(self.value)
        desktop.layer_context={**self.value,'layer_ids':[3]}
        desktop.require_menu_layer_target('label.blue',self.value)
        desktop.layer_context_provider.assert_not_called()
    def test_open_freezes_memory_snapshot_and_adds_no_provider_read(self):
        d=Desktop('http://localhost',Mock());d.hwnd=2
        foreground=[1]
        d.layer_context_provider=Mock(side_effect=AssertionError('No disk/UXP wait in open'))
        d.layer_context=dict(self.value)
        d.gui=SimpleNamespace(GetForegroundWindow=lambda:foreground[0],IsWindow=lambda h:True,
            IsIconic=lambda h:False,IsWindowVisible=lambda h:True,IsWindowEnabled=lambda h:True,
            GetClassName=lambda h:'Photoshop' if h==1 else 'PhotoshopMenuImGui',
            GetWindowText=lambda h:'test @ 100%',GetWindowRect=lambda h:(0,0,1000,1000),
            GetAncestor=lambda h,mode:h,WindowFromPoint=lambda p:1,SetWindowLong=Mock(),
            SetWindowPos=Mock(),SendMessage=Mock(),ShowWindow=Mock(),
            SetForegroundWindow=lambda h:foreground.__setitem__(0,h))
        with patch('win32process.GetWindowThreadProcessId',side_effect=lambda h:(1,10 if h==1 else 20)), \
             patch('win32api.GetCursorPos',return_value=(500,500)), \
             patch('win32api.GetMonitorInfo',return_value={'Work':(0,0,1000,1000)}), \
             patch('win32api.MonitorFromPoint',return_value=1):
            d._open(hold=True)
        d.layer_context_provider.assert_not_called()
        sent=next(c.args for c in d.gui.SendMessage.call_args_list if c.args[1]==0x8011)
        self.assertEqual((sent[2]>>8)&15,15)
        d.layer_context={'kind':'background','mask':0}
        self.assertEqual(d.menu_layer_context['kind'],'pixel')
    def test_partial_overwrite_keeps_only_fresh_same_session_cache(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);bridge=UxpBridge(task_history=folder/'tasks.json',config=folder/'config.json')
            bridge.transport=SimpleNamespace(folder=folder)
            heartbeat=folder/'probe-last.json';heartbeat.write_text(json.dumps({'session':'one'}))
            source=folder/'layer-context.json';source.write_text(json.dumps(self.value))
            self.assertEqual(bridge.read_layer_context()['mask'],15)
            source.write_text('{')
            self.assertEqual(bridge.read_layer_context()['mask'],15)
            heartbeat.write_text(json.dumps({'session':'two'}))
            self.assertEqual(bridge.read_layer_context()['mask'],0)
            bridge.layer_context={**self.value,'plugin_time_ms':self.now-2100}
            self.assertEqual(bridge.read_layer_context()['mask'],0)
            bridge.executor.shutdown(wait=True)
    def test_type_cache_updates_while_ui_window_lock_is_busy(self):
        d=Desktop('http://localhost',Mock());updated=threading.Event()
        d.gui=SimpleNamespace(GetForegroundWindow=lambda:42)
        d._is_photoshop=lambda hwnd:hwnd==42
        def provider():updated.set();return dict(self.value)
        d.layer_context_provider=provider
        worker=threading.Thread(target=d._monitor_layer_context)
        try:
            with d.window_lock:
                worker.start()
                self.assertTrue(updated.wait(.5),'Type cache blocked behind window/UIA lock')
                deadline=time.monotonic()+.5
                while not d.layer_context and time.monotonic()<deadline:time.sleep(.001)
                self.assertEqual(d.layer_context['mask'],15)
        finally:d.stop.set();d.layer_wake.set();worker.join(1)
        self.assertFalse(worker.is_alive())
    def test_type_monitor_skips_uxp_discovery_outside_photoshop(self):
        d=Desktop('http://localhost',Mock())
        d.gui=SimpleNamespace(GetForegroundWindow=Mock(side_effect=[17,42,17]))
        d._is_photoshop=lambda hwnd:hwnd==42
        d.stop=Mock()
        d.stop.is_set.side_effect=[False,False,False,True]
        d.layer_wake=Mock()
        d.layer_context_provider=Mock(return_value=dict(self.value))
        d._monitor_layer_context()
        d.layer_context_provider.assert_called_once_with()
        self.assertEqual([call.args[0] for call in d.layer_wake.wait.call_args_list],[2,.04,2])
        self.assertEqual(d.layer_context['kind'],'unknown')

    def test_native_focus_transitions_wake_sleeping_observers(self):
        d=Desktop('http://localhost',Mock())
        d.gui=SimpleNamespace(GetForegroundWindow=Mock(side_effect=[42,42,17]))
        d._is_photoshop=lambda hwnd:hwnd==42
        d.foreground_changed=Mock()
        d.layer_wake=Mock()
        d.window_wake=Mock()
        d.input_language=Mock()
        d.input_focus=Mock()
        d.native=SimpleNamespace(stdout=[json.dumps({'type':'input.language.activity','hwnd':hwnd})
                                         for hwnd in (42,43,17)])
        d._read_native_events()
        self.assertEqual(d.foreground_changed.call_count,2)
        self.assertEqual(d.layer_wake.set.call_count,2)
        self.assertEqual(d.window_wake.set.call_count,2)
        self.assertEqual(d.input_language.activity.call_count,3)
        self.assertEqual(d.input_focus.focus_changed.call_count,3)
        self.assertFalse(d.photoshop_foreground)

if __name__=='__main__':unittest.main()

class DocumentSortTargetTests(unittest.TestCase):
    def test_individual_smart_allows_cross_parent_but_rejects_locks_and_stale_target(self):
        value={'session':'s','document_id':1,'layer_ids':[2,3],'kind':'mixed','same_parent':False}
        require_menu_target(value,'convertSmartObjectsIndividually',value)
        for change in ({'locked':True},{'background':True},{'kind':'unknown'}):
            with self.assertRaises(TaskCancelled):require_menu_target({**value,**change},'convertSmartObjectsIndividually',value)
        with self.assertRaises(TaskCancelled):require_menu_target(value,'convertSmartObjectsIndividually',{**value,'layer_ids':[2]})
    def test_document_sort_rejects_locked_background_cross_parent_and_stale_selection(self):
        base={'session':'s','document_id':1,'layer_ids':[2],'kind':'pixel'}
        for name in ('moveDocumentTop','moveDocumentBottom'):
            require_menu_target(base,name,base)
            for value in (None,{**base,'locked':True},{**base,'background':True},{**base,'same_parent':False}):
                with self.assertRaises(TaskCancelled):require_menu_target(value,name,base)
            with self.assertRaises(TaskCancelled):require_menu_target(base,name,{**base,'layer_ids':[3]})
