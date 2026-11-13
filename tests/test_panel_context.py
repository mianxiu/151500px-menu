import unittest
from unittest.mock import Mock, patch
from backend.panel_context import PanelContext, classify_chain, panel_flags, LAYER_CONTROLS, LAYER_CONTROLS_PS25


class PanelContextTests(unittest.TestCase):
    def test_stale_native_f_cannot_open_or_close_a_newer_menu(self):
        from backend.desktop import Desktop
        from types import SimpleNamespace
        d=Desktop('http://localhost',lambda action:None);d.visible=True;d.menu_epoch=9
        d.input_focus=SimpleNamespace(shortcut_context=lambda:{'state':'non-input','root':10,'focus':11,'epoch':2})
        d.require_action_focus=Mock();d.hide=Mock()
        with self.assertRaisesRegex(RuntimeError,'上下文已改变'):
            d.open_menu_tools('search',input_guard={'root':10,'focus':11,'epoch':1})
        d.hide.assert_not_called();self.assertTrue(d.visible)
        d.hide_if_epoch(8);d.hide.assert_not_called()
        d.hide_if_epoch(9);d.hide.assert_called_once_with(restore=True)

    def test_search_close_revalidates_ime_only_on_own_photoshop_handoff(self):
        from backend.desktop import Desktop
        for restore,own,tools in ((True,True,True),(False,True,True),(True,False,True),(True,True,False)):
            with self.subTest(restore=restore,own=own,tools=tools):
                d=Desktop('http://localhost',lambda action:None)
                d.hwnd=10;d.target=20;d.visible=True;d.menu_tools_active=tools
                d.gui=Mock();d.gui.GetForegroundWindow.return_value=10;d.gui.IsWindow.return_value=True
                d._is_menu_window=Mock(return_value=own);d._is_photoshop=Mock(return_value=True)
                d._native_config=Mock();d.preview_pending_feedback=Mock();d.input_language=Mock()
                d.hide(restore=restore)
                self.assertFalse(d.menu_tools_active)
                if restore and own and tools:d.input_language.edit_end.assert_called_once_with()
                else:d.input_language.edit_end.assert_not_called()

    def test_delayed_search_request_is_rejected_after_canvas_edit_starts(self):
        from types import SimpleNamespace
        from backend.desktop import Desktop
        d=Desktop('http://localhost',lambda action:None);d.require_action_focus=Mock()
        d.input_focus=SimpleNamespace(shortcut_context=lambda:{'state':'input'})
        with self.assertRaisesRegex(RuntimeError,'输入状态'):d.open_menu_tools('search')
        self.assertFalse(d.visible)

    def test_native_commit_event_only_requests_input_revalidation(self):
        from io import StringIO
        from types import SimpleNamespace
        from backend.desktop import Desktop
        d=Desktop('http://localhost',lambda action:self.fail('Commit revalidation is not a document command'))
        d.stop.set();d.input_language=Mock();d.native=SimpleNamespace(stdout=StringIO('{"type":"input.language.edit_end"}\n'))
        d._read_native_events();d.input_language.edit_end.assert_called_once_with()

    def test_canvas_search_uses_shared_input_evidence_instead_of_panel_class(self):
        import json
        from types import SimpleNamespace
        from backend.desktop import Desktop
        d=Desktop('http://localhost',lambda action:None)
        d.native=Mock();d.native.poll.return_value=None
        d.panel_context=SimpleNamespace(current={'kind':'canvas','input_focused':False})
        proof={'state':'input','root':10,'focus':11,'epoch':100}
        d.input_focus=SimpleNamespace(shortcut_context=lambda:dict(proof))
        d.input_language=SimpleNamespace(shortcut_context=lambda:self.fail('F must not read IME policy or recovery'))
        d._native_config();value=json.loads(d.native.stdin.write.call_args.args[0])
        self.assertTrue(value['text_input']);self.assertFalse(value['search_non_input'])
        proof['state']='non-input';d._native_config();value=json.loads(d.native.stdin.write.call_args.args[0])
        self.assertTrue(value['search_non_input']);self.assertFalse(value['text_input']);self.assertEqual(value['search_root'],10)
        self.assertEqual(value['search_focus'],11);self.assertEqual(value['search_epoch'],100)

    def test_ps2551_report_signature_and_unrelated_palettes(self):
        chain=[('PSViewC',''),('OWL.Palette',''),('OWL.Dock',''),('Photoshop','')]
        self.assertEqual(classify_chain(chain,[],LAYER_CONTROLS_PS25),'layers')
        for item in LAYER_CONTROLS_PS25:
            self.assertEqual(classify_chain(chain,[],LAYER_CONTROLS_PS25-{item}),'generic')
        self.assertEqual(classify_chain(chain,[],{('PSViewC',0),('ScrollBar',6143),*[('Edit',i) for i in range(6316,6321)]}),'generic')
        self.assertEqual(classify_chain([('Edit',''),*chain],[],LAYER_CONTROLS_PS25),'text')
        self.assertEqual(classify_chain([('PSViewC',''),('OWL.Document','')],[],LAYER_CONTROLS_PS25),'canvas')
        self.assertEqual(classify_chain([('PSViewC',''),('PSFloatC','')],[],LAYER_CONTROLS_PS25),'generic')

    def test_visible_menu_freezes_hover_even_if_photoshop_keeps_foreground(self):
        from backend.desktop import Desktop
        from types import SimpleNamespace
        for visible in (True, False):
            desktop = Desktop('http://localhost', lambda action:None)
            desktop.visible = visible
            desktop.target = 10
            desktop.stop = Mock(is_set=Mock(side_effect=[False,False,True]))
            desktop.window_wake = Mock()
            desktop._scope = Mock(return_value=True)
            desktop._is_photoshop = Mock(return_value=True)
            desktop._native_config = Mock()
            desktop.gui = SimpleNamespace(GetForegroundWindow=lambda:10)
            desktop.panel_context = Mock()
            desktop._monitor()
            if visible:
                desktop.panel_context.snapshot.assert_not_called()
            else:
                self.assertEqual(desktop.panel_context.snapshot.call_args.args,(10,))
                self.assertTrue(desktop.panel_context.snapshot.call_args.kwargs['accept']())

    def test_slow_background_read_cannot_block_fresh_open_or_overwrite_it(self):
        import threading
        detector=PanelContext(Mock())
        entered,release,done=threading.Event(),threading.Event(),threading.Event()
        def read(root,clicked,previous):
            if threading.current_thread().name=='slow-context':
                entered.set();release.wait(2)
                return ({'kind':'layers'},clicked,1)
            return ({'kind':'text','input_focused':True},clicked,2)
        detector._read_snapshot=read
        slow=threading.Thread(target=detector.snapshot,args=(10,),name='slow-context')
        slow.start();self.assertTrue(entered.wait(1))
        fast=threading.Thread(target=lambda:(detector.snapshot(10),done.set()))
        fast.start()
        try:self.assertTrue(done.wait(.5),'Fresh open must not wait for background native reads')
        finally:release.set();slow.join(2);fast.join(2)
        self.assertEqual(detector.current['kind'],'text')
        self.assertTrue(detector.current['input_focused'])

    def test_background_publish_is_discarded_when_menu_has_become_visible(self):
        detector=PanelContext(Mock());original=dict(detector.current)
        detector._read_snapshot=lambda *args:({'kind':'layers'},None,2)
        self.assertEqual(detector.snapshot(10,accept=lambda:False)['kind'],'layers')
        self.assertEqual(detector.current,original)

    def test_monitor_native_panel_read_runs_outside_menu_window_lock(self):
        from backend.desktop import Desktop
        from types import SimpleNamespace
        import threading
        desktop=Desktop('http://localhost',lambda action:None)
        desktop.window_lock=threading.Lock();desktop.target=10
        desktop.stop=Mock(is_set=Mock(side_effect=[False,False,True]));desktop.window_wake=Mock();desktop._scope=Mock(return_value=True)
        desktop._is_photoshop=Mock(return_value=True);desktop._native_config=Mock()
        desktop.gui=SimpleNamespace(GetForegroundWindow=lambda:10)
        desktop.panel_context=Mock()
        def read(*args,**kwargs):
            acquired=desktop.window_lock.acquire(blocking=False)
            self.assertTrue(acquired,'Panel enumeration must not own the menu transition lock')
            if acquired:desktop.window_lock.release()
        desktop.panel_context.snapshot.side_effect=read
        desktop._monitor();desktop.panel_context.snapshot.assert_called_once()

    def test_monitor_publishes_fresh_input_state_on_the_same_pass(self):
        from backend.desktop import Desktop
        from types import SimpleNamespace
        desktop=Desktop('http://localhost',lambda action:None)
        desktop.target=10;desktop.stop=Mock(is_set=Mock(side_effect=[False,False,True]));desktop.window_wake=Mock()
        desktop._scope=Mock(return_value=True);desktop._is_photoshop=Mock(return_value=True)
        desktop.gui=SimpleNamespace(GetForegroundWindow=lambda:10)
        desktop.panel_context=Mock();desktop.panel_context.current={'kind':'generic'}
        states=[];desktop._native_config=lambda:states.append(desktop.panel_context.current['kind'])
        def read(*args,**kwargs):desktop.panel_context.current={'kind':'text','input_focused':True}
        desktop.panel_context.snapshot.side_effect=read
        desktop._monitor();self.assertEqual(states[-1],'text')

    def test_panel_signatures_and_unknown_palette(self):
        self.assertEqual(classify_chain([('PSViewC',''),('OWL.WelcomeScreenView','')]), 'home')
        self.assertEqual(classify_chain([('Edit',''),('OWL.WelcomeScreenView','')]), 'text')
        self.assertEqual(classify_chain([('PSViewC',''),('OWL.Document','')]), 'canvas')
        self.assertEqual(classify_chain([('PSViewC',''),('OWL.Palette','')],
                                       ['Fill:', 'Lock:', 'Opacity:']), 'layers')
        self.assertEqual(classify_chain([('PSViewC',''),('OWL.Palette','')],
                                       ['填充：', '锁定：', '不透明度：']), 'layers')
        self.assertEqual(classify_chain([('PSViewC',''),('OWL.Palette','')], ['Opacity:']), 'generic')
        self.assertEqual(classify_chain([('Edit',''),('OWL.Palette','')],
                                       ['Fill:', 'Lock:', 'Opacity:']), 'text')

    def test_localized_access_keys_and_spacing_keep_strict_signature(self):
        palette = [('PSViewC',''), ('OWL.Palette','')]
        for labels in (['填充（F）：', '锁定（L）：', '不透明度（O）：'],
                       ['&Fill :', 'Loc&k:', '\u200eOpacity\u00a0:'],
                       ['填 充：', '锁定:', '不透明度 (&O) :']):
            self.assertEqual(classify_chain(palette, labels), 'layers')
        for labels in (['填充（F）：', '不透明度（O）：'],
                       ['填充颜色', '锁定', '不透明度'],
                       ['Fill opacity:', 'Lock:', 'Opacity:']):
            self.assertEqual(classify_chain(palette, labels), 'generic')
        self.assertEqual(classify_chain([('Edit',''), *palette],
                                       ['填充（F）：', '锁定（L）：', '不透明度（O）：']), 'text')

    def panel_gui(self, dock_owner=100, nested=False):
        names = {1:'PSViewC', 2:'OWL.Palette', 3:'OWL.Dock', 100:'Photoshop',
                 4:'Static', 5:'Static', 6:'Static', 7:'Edit', 8:'OWL.Palette'}
        parents = {1:2, 7:2, 2:8 if nested else 3, 8:3, 3:0, 100:0, 4:2, 5:2, 6:2}
        gui = Mock()
        gui.IsWindow.side_effect = lambda h:h in names
        gui.GetClassName.side_effect = names.__getitem__
        gui.GetParent.side_effect = parents.__getitem__
        gui.IsWindowVisible.return_value = True
        gui.GetWindowText.side_effect = {4:'填充（F）：', 5:'锁定：', 6:'不透明度（O）：'}.__getitem__
        gui.EnumChildWindows.side_effect = lambda h, callback, _: [callback(c, None) for c in ((4,5,6) if h==2 else ())]
        return gui

    def test_owned_floating_layers_and_input_are_distinguished(self):
        gui = self.panel_gui()
        with patch('backend.panel_context.workspace_root', return_value=100):
            detector = PanelContext(gui)
            self.assertEqual(detector.describe(1,100), 'layers')
            self.assertEqual(detector.describe(7,100), 'text')
        with patch('backend.panel_context.workspace_root', return_value=0):
            self.assertEqual(PanelContext(gui).describe(1,100), 'generic')

    def test_nearest_palette_owns_signature_not_outer_dock(self):
        gui = self.panel_gui(nested=True)
        with patch('backend.panel_context.workspace_root', return_value=100):
            self.assertEqual(PanelContext(gui).describe(1,100), 'layers')
        gui.EnumChildWindows.assert_called_once()
        self.assertEqual(gui.EnumChildWindows.call_args.args[0], 2)

    def test_empty_labels_need_complete_native_identity_not_only_counts(self):
        chain=[('PSViewC',''), ('OWL.Palette','')]
        self.assertEqual(classify_chain(chain, [], LAYER_CONTROLS), 'layers')
        for item in LAYER_CONTROLS:
            self.assertEqual(classify_chain(chain, [], LAYER_CONTROLS-{item}), 'generic')
        same_counts=[(name,0) for name,_ in LAYER_CONTROLS]
        self.assertEqual(classify_chain(chain, [], same_counts), 'generic')
        self.assertEqual(classify_chain([('Edit',''), *chain], [], LAYER_CONTROLS), 'text')
        self.assertEqual(classify_chain([('PSViewC',''), ('OWL.Document','')], [], LAYER_CONTROLS), 'canvas')

    def test_real_control_signature_is_collected_without_reading_input_text(self):
        gui=self.panel_gui()
        classes={1:'PSViewC',2:'OWL.Palette',3:'OWL.Dock',100:'Photoshop',
                 7:'Edit',10:'ComboBox',11:'ComboBox',12:'Edit',13:'ScrollBar'}
        parents={1:2,7:2,10:2,11:2,12:2,13:2,2:3,3:0}
        ids={1:0,7:6093,10:2490,11:2493,12:6097,13:6098}
        gui.GetClassName.side_effect=classes.__getitem__
        gui.GetParent.side_effect=parents.__getitem__
        gui.GetDlgCtrlID.side_effect=ids.__getitem__
        gui.EnumChildWindows.side_effect=lambda h, cb, _: [cb(c,None) for c in ids]
        with patch('backend.panel_context.workspace_root', return_value=100):
            self.assertEqual(PanelContext(gui).describe(1,100), 'layers')
            self.assertEqual(PanelContext(gui).describe(7,100), 'text')
        gui.GetWindowText.assert_not_called()

    def detector(self):
        detector = PanelContext(Mock(IsWindow=lambda hwnd: hwnd != 99))
        detector.describe = lambda hwnd, root: {1:'canvas',2:'layers',3:'text'}.get(hwnd,'generic')
        detector.focus_window = Mock(return_value=1)
        detector.hover_window = Mock(return_value=0)
        detector.home_visible = Mock(return_value=False)
        return detector

    def test_home_overrides_retained_canvas_and_palette_but_preserves_input(self):
        detector = self.detector()
        detector.record_click(2, 10)
        detector.hover_window.return_value = 2
        detector.home_visible.return_value = True
        self.assertEqual(detector.snapshot(10)['kind'], 'home')
        detector.focus_window.return_value = 3
        self.assertTrue(detector.snapshot(10)['input_focused'])
        detector.hover_window.return_value = 3
        self.assertEqual(detector.snapshot(10)['kind'], 'text')
        detector.home_visible.return_value = False
        detector.focus_window.return_value = 1
        detector.hover_window.return_value = 1
        self.assertEqual(detector.snapshot(10)['kind'], 'canvas')

    def test_home_protocol_does_not_alias_input_or_layout(self):
        for kind, bits in [('generic',0),('canvas',1),('layers',2),('text',3),('home',32)]:
            self.assertEqual(panel_flags({'kind':kind}), bits)
            self.assertEqual(panel_flags({'kind':kind,'input_focused':True},True,True),bits|4|8|16)

    def test_only_visible_nonempty_welcome_view_marks_home(self):
        gui = Mock()
        gui.EnumChildWindows.side_effect = lambda root, collect, unused: [collect(h,None) for h in (1,2)]
        gui.GetClassName.side_effect = lambda h:'OWL.WelcomeScreenView' if h==2 else 'OWL.Document'
        gui.GetWindowRect.return_value = (0,0,800,600)
        gui.IsWindowVisible.return_value = False
        detector = PanelContext(gui)
        self.assertFalse(detector.home_visible(10))
        gui.IsWindowVisible.return_value = True
        self.assertTrue(detector.home_visible(10))
        gui.GetWindowRect.return_value = (0,0,0,0)
        self.assertFalse(detector.home_visible(10))

    def test_clicked_layers_override_retained_canvas_focus(self):
        detector = self.detector()
        detector.record_click(2, 10)
        self.assertEqual(detector.snapshot(10)['kind'], 'layers')
        self.assertEqual(detector.snapshot(10)['source'], 'last-click')
        detector.record_click(1, 10)
        self.assertEqual(detector.snapshot(10)['kind'], 'canvas')

    def test_real_keyboard_focus_change_and_input_take_priority(self):
        detector = self.detector()
        detector.record_click(2, 10)
        detector.snapshot(10)
        detector.focus_window.return_value = 3
        self.assertEqual(detector.snapshot(10)['kind'], 'text')
        detector.focus_window.return_value = 1
        self.assertEqual(detector.snapshot(10)['kind'], 'canvas')

    def test_foreign_destroyed_and_unknown_click_fall_back(self):
        detector = self.detector()
        detector.record_click(2, 11)
        self.assertEqual(detector.snapshot(10)['kind'], 'canvas')
        detector.record_click(99, 10)
        self.assertEqual(detector.snapshot(10)['kind'], 'canvas')
        detector.record_click(4, 10)
        self.assertEqual(detector.snapshot(10)['source'], 'fallback')

    def test_text_context_blocks_direct_script_dispatch(self):
        from backend.desktop import Desktop
        desktop = Desktop('http://localhost', lambda action:None)
        desktop._scope = lambda:True
        desktop.visible = True
        desktop.panel_context = self.detector()
        desktop.panel_context.current = {'kind':'text'}
        with self.assertRaisesRegex(RuntimeError, '输入'):
            desktop.require_action_focus()

    def test_open_generic_menu_blocks_all_reorders_without_changing_chords(self):
        from backend.desktop import Desktop
        desktop = Desktop('http://localhost', lambda action:None)
        desktop.panel_context = self.detector()
        desktop.visible = True
        for kind in ('generic','canvas'):
            desktop.panel_context.current = {'kind':kind}
            for action in ('moveToTop','moveToBottom','moveUp','moveDown','moveDocumentTop','moveDocumentBottom'):
                with self.assertRaisesRegex(RuntimeError, '通用操作不提供图层移动'):
                    desktop.require_move_context(action)
        for action in ('select','selectAbove','selectBelow','copyStyle','pasteStyle','editStyle'):
            desktop.require_move_context(action)
        desktop.panel_context.current = {'kind':'layers'}
        desktop.require_move_context('moveUp')
        desktop.panel_context.current = {'kind':'canvas'}
        desktop.visible = False
        desktop.require_move_context('moveUp')
        desktop.panel_context.current = {'kind':'generic'}
        desktop.require_move_context('moveUp')

    def test_home_blocks_layer_shortcuts_even_with_open_documents(self):
        from backend.desktop import Desktop
        desktop = Desktop('http://localhost', lambda action:None)
        desktop._scope = lambda:True
        desktop.visible = True
        desktop.panel_context = self.detector()
        desktop.panel_context.current = {'kind':'home','input_focused':False}
        with self.assertRaisesRegex(RuntimeError,'首页'):
            desktop.require_action_focus()
        desktop.require_action_focus(allow_home=True)
        desktop.panel_context.current['input_focused'] = True
        with self.assertRaisesRegex(RuntimeError,'输入'):
            desktop.require_action_focus(allow_home=True)

    def test_control_recreation_failure_uses_generic_menu(self):
        detector = self.detector()
        detector.focus_window.side_effect = RuntimeError('window destroyed')
        self.assertEqual(detector.snapshot(10)['source'], 'fallback')
        self.assertEqual(detector.current['kind'], 'generic')

    def test_hover_routes_without_click_and_overrides_last_click(self):
        detector = self.detector()
        detector.record_click(1, 10)
        detector.hover_window.return_value = 2
        self.assertEqual(detector.snapshot(10)['kind'], 'layers')
        self.assertEqual(detector.current['source'], 'hover')
        detector.hover_window.return_value = 1
        self.assertEqual(detector.snapshot(10)['kind'], 'canvas')
        detector.hover_window.return_value = 0
        self.assertEqual(detector.snapshot(10)['source'], 'last-click')

    def test_hover_keeps_input_guard_and_unknown_panel_fallback(self):
        detector = self.detector()
        detector.focus_window.return_value = 3
        detector.hover_window.return_value = 1
        context = detector.snapshot(10)
        self.assertEqual(context['kind'], 'canvas')
        self.assertTrue(context['input_focused'])
        detector.hover_window.return_value = 4
        self.assertEqual(detector.snapshot(10)['source'], 'fallback')

