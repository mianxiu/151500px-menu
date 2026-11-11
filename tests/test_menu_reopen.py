import asyncio
import io
import json
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import Mock, patch
from backend.main import Application
from backend.desktop import Desktop, pie_layout
from backend.photoshop_bridge import PhotoshopBridge


def pie_bounds(*args, **kwargs):
    return pie_layout(*args, **kwargs)[0]


class MenuFocusTests(unittest.TestCase):
    def test_floating_document_popup_uses_main_owner_and_survives_document_hide(self):
        desktop = Desktop('http://localhost', lambda action:None)
        desktop.hwnd = 2
        foreground, visible, enabled = [4], {1,2,3,4}, {1,2,3,4}
        names = {1:'Photoshop',2:'PhotoshopMenuImGui',3:'OtherApp',4:'OWL.DocumentWindow'}
        desktop.gui = SimpleNamespace(
            GetForegroundWindow=lambda:foreground[0], IsWindow=lambda h:h in names,
            IsIconic=lambda h:False, IsWindowVisible=lambda h:h in visible,
            IsWindowEnabled=lambda h:h in enabled, GetClassName=lambda h:names[h],
            GetAncestor=lambda h,mode:1 if h==4 and mode==3 else h,
            GetWindowText=lambda h:'Untitled-1 @ 100%',
            GetWindowRect=lambda h:(0,0,1000,1000), WindowFromPoint=lambda p:4,
            SetWindowLong=Mock(), SetWindowPos=Mock(), SendMessage=Mock(),
            SetForegroundWindow=lambda h:foreground.__setitem__(0,h), ShowWindow=Mock())
        desktop.panel_context=SimpleNamespace(snapshot=lambda h:{'kind':'layers'},current={'kind':'layers'})
        desktop.menu_context_requester=Mock()
        with patch('win32process.GetWindowThreadProcessId',side_effect=lambda h:(1,20 if h==2 else 10)), \
             patch('win32api.GetCursorPos',return_value=(500,500)), \
             patch('win32api.GetMonitorInfo',return_value={'Work':(0,0,1000,1000)}), \
             patch('win32api.MonitorFromPoint',return_value=1):
            desktop._open(hold=True)
            # The production SWP_SHOWWINDOW path has completed before any UXP
            # publication; the layer worker owns sharing retries, never open().
            desktop.gui.SetWindowPos.assert_called_once()
            desktop.menu_context_requester.assert_not_called()
            self.assertTrue(desktop.menu_context_pending['unpublished'])
            self.assertEqual(desktop.target,1)
            self.assertEqual(desktop.gui.SetWindowLong.call_args.args[2],1)
            visible.remove(4)  # PS hides/replaces the original floating editor.
            self.assertTrue(desktop._scope())
            desktop.hide(restore=False)
            self.assertTrue(desktop._scope(), 'A neutral hide cannot strand an owned foreground')
            foreground[0]=1
            self.assertTrue(desktop._scope())
            foreground[0]=2  # PS/COM foreground can bounce back to hidden menu.
            self.assertTrue(desktop._scope())
            enabled.remove(1)
            desktop.hide(restore=False)
            self.assertFalse(desktop._scope(), 'Disabled PS still blocks actions')
            enabled.add(1)
            self.assertTrue(desktop._scope(), 'PS becoming ready restores the existing owned session')
            foreground[0]=3
            self.assertFalse(desktop._scope())
            foreground[0]=2
            self.assertFalse(desktop._scope(), 'Foreign app revokes the hidden session')

    def test_menu_publication_wait_does_not_lock_input_or_replace_a_reopened_menu(self):
        import threading,time
        desktop=Desktop('http://localhost',Mock());desktop.visible=True;desktop.menu_epoch=7
        old={'epoch':7,'unpublished':True,'deadline':time.monotonic()+10}
        desktop.menu_context_pending=old
        entered,release=threading.Event(),threading.Event()
        def publish(epoch):
            entered.set()
            if not release.wait(2):raise TimeoutError('test barrier')
            return {'id':'old','epoch':epoch,'deadline':time.monotonic()+10}
        desktop.menu_context_requester=Mock(side_effect=publish)
        worker=threading.Thread(target=desktop._publish_menu_context);worker.start()
        try:
            self.assertTrue(entered.wait(1))
            acquired=desktop.window_lock.acquire(blocking=False)
            self.assertTrue(acquired,'UXP publication must not hold the input/window lock')
            if acquired:
                try:
                    desktop.visible=False
                    desktop._finish_menu_context()
                    self.assertIsNone(desktop.menu_context_pending)
                    desktop.visible=True;desktop.menu_epoch=8
                    reopened={'epoch':8,'unpublished':True,'deadline':time.monotonic()+10}
                    desktop.menu_context_pending=reopened
                finally:desktop.window_lock.release()
        finally:release.set();worker.join(2)
        self.assertFalse(worker.is_alive())
        self.assertIs(desktop.menu_context_pending,reopened)
        desktop.menu_context_requester.assert_called_once_with(7)

    def test_deferred_publication_is_once_and_keeps_original_deadline(self):
        import time
        desktop=Desktop('http://localhost',Mock());desktop.visible=True;desktop.menu_epoch=7
        deadline=time.monotonic()+8
        desktop.menu_context_pending={'epoch':7,'unpublished':True,'deadline':deadline}
        desktop._finish_menu_context()  # No result ID exists while unpublished.
        desktop.menu_context_requester=Mock(return_value={'id':'one','epoch':7,'deadline':deadline+2})
        desktop._publish_menu_context();desktop._publish_menu_context()
        desktop.menu_context_requester.assert_called_once_with(7)
        self.assertEqual(desktop.menu_context_pending['deadline'],deadline)

    def test_read_cancel_is_outside_window_lock_and_bound_to_original_request(self):
        desktop=Desktop('http://localhost',Mock());desktop.menu_epoch=8;desktop.visible=False
        original={'id':'old','epoch':7};desktop.menu_context_active=original
        desktop.menu_context_canceller=Mock()
        desktop._cancel_closed_menu_context()
        desktop.menu_context_canceller.assert_called_once_with(original)
        self.assertIsNone(desktop.menu_context_active)
        desktop._cancel_closed_menu_context();self.assertEqual(desktop.menu_context_canceller.call_count,1)
        desktop.visible=True;desktop.menu_context_active={'id':'new','epoch':8}
        desktop._cancel_closed_menu_context();self.assertEqual(desktop.menu_context_canceller.call_count,1)

    def test_closed_or_expired_menu_never_publishes_and_failure_never_retries(self):
        import time
        for visible,deadline in [(False,time.monotonic()+10),(True,time.monotonic()-1)]:
            desktop=Desktop('http://localhost',Mock());desktop.visible=visible;desktop.menu_epoch=7
            desktop.menu_context_pending={'epoch':7,'unpublished':True,'deadline':deadline}
            desktop.menu_context_requester=Mock();desktop._publish_menu_context()
            desktop.menu_context_requester.assert_not_called();self.assertIsNone(desktop.menu_context_pending)
        desktop.visible=True
        desktop.menu_context_pending={'epoch':7,'unpublished':True,'deadline':time.monotonic()+10}
        desktop.menu_context_requester=Mock(side_effect=PermissionError('read sharing'))
        with self.assertLogs(level='ERROR'):desktop._publish_menu_context()
        desktop._publish_menu_context();desktop.menu_context_requester.assert_called_once_with(7)
        self.assertIsNone(desktop.menu_context_pending)

    def test_hidden_owned_foreground_is_scoped_but_foreign_window_is_not(self):
        desktop = Desktop('http://localhost', lambda action:None)
        desktop.hwnd, desktop.target, desktop.visible = 2, 1, False
        desktop.restore_pending = True  # An owned hide requested PS restoration.
        foreground = [2]
        desktop.gui = SimpleNamespace(GetForegroundWindow=lambda:foreground[0])
        desktop._is_menu_window = lambda hwnd:hwnd == 2
        desktop._is_photoshop = lambda hwnd:hwnd == 1
        desktop.require_action_focus()
        foreground[0] = 3
        with self.assertRaisesRegex(RuntimeError, '仅在 Photoshop'):
            desktop.require_action_focus()
        desktop.target = 0
        foreground[0] = 2
        self.assertFalse(desktop._scope())

    def test_capslock_decision_is_logged_without_submitting_an_action(self):
        notify = Mock()
        desktop = Desktop('http://localhost', notify)
        event = {'type':'caps.trace','message':256,'process_scope':True,'blocked':True}
        desktop.native = SimpleNamespace(stdout=io.StringIO(json.dumps(event)+'\n'))
        with self.assertLogs(level='INFO') as logs:
            desktop._read_native_events()
        self.assertTrue(any('CapsLock hook decision' in item for item in logs.output))
        notify.assert_not_called()

    def test_hook_health_is_logged_without_submitting_an_action(self):
        notify = Mock()
        desktop = Desktop('http://localhost', notify)
        event = {'type':'hook.health','scan_us':12000,'renew_ok':True,'dropped_total':3}
        desktop.native = SimpleNamespace(stdout=io.StringIO(json.dumps(event)+'\n'))
        with self.assertLogs(level='INFO') as logs:
            desktop._read_native_events()
        self.assertTrue(any('Native hook health' in item and 'dropped_total' in item for item in logs.output))
        notify.assert_not_called()

    def test_activation_click_is_logged_without_submitting_an_action(self):
        notify = Mock()
        desktop = Desktop('http://localhost', notify)
        event = {'type':'activation.trace','message':513,'clicked_ps':True,'direct_switching':True}
        desktop.native = SimpleNamespace(stdout=io.StringIO(json.dumps(event)+'\n'))
        with self.assertLogs(level='INFO') as logs:
            desktop._read_native_events()
        self.assertTrue(any('Photoshop activation click' in item for item in logs.output))
        notify.assert_not_called()

    def test_caps_state_is_logged_without_submitting_an_action(self):
        notify = Mock()
        desktop = Desktop('http://localhost', notify)
        event = {'type':'caps.state','toggle_on':True,'previous':False,'tick_ms':123}
        desktop.native = SimpleNamespace(stdout=io.StringIO(json.dumps(event)+'\n'))
        with self.assertLogs(level='INFO') as logs:
            desktop._read_native_events()
        self.assertTrue(any('CapsLock system state' in item and 'toggle_on' in item for item in logs.output))
        notify.assert_not_called()

    def test_pie_is_centered_on_invocation_and_clamped_to_work_area(self):
        self.assertEqual(pie_bounds((0,0,1920,1080),(0,0,1920,1040),(960,520),1), (650,270,620,500))
        self.assertEqual(pie_bounds((0,0,1920,1080),(0,0,1920,1040),(1910,1030),1), (1300,540,620,500))
        self.assertEqual(pie_bounds((-1920,0,0,1080),(-1920,0,0,1040),(-960,520),1.5), (-1425,145,930,750))

    def test_color_editor_reserves_left_panel_without_displacing_radial_anchor(self):
        self.assertEqual(pie_bounds((0,0,1920,1080),(0,0,1920,1040),(960,520),1,color_editor=True),(310,210,920,620))
        self.assertEqual(pie_bounds((0,0,1920,1080),(0,0,1920,1040),(1330,520),2,color_editor=True),(30,0,1840,1040))
        self.assertEqual(pie_bounds((0,0,1920,1080),(0,0,1920,1040),(1910,1030),1,color_editor=True),(1000,420,920,620))

    def test_left_dock_keeps_radial_anchor_near_cursor_on_scaled_secondary_monitor(self):
        ps=(-1920,0,0,1080)
        work=(-1920,0,0,1040)
        bounds, anchor, right_panel=pie_layout(ps,work,(-1530,520),1.5,color_editor=True)
        self.assertTrue(right_panel)
        self.assertEqual(bounds[0]+anchor,-1530)
        self.assertEqual(bounds[1]+bounds[3]//2,520)
        bounds, anchor, right_panel=pie_layout(ps,work,(-1860,520),1.5,color_editor=True)
        self.assertTrue(right_panel)
        self.assertEqual(bounds[0]+anchor,-1620)

    def test_left_monitor_seam_preserves_original_layout_and_pointer_anchor(self):
        ps=(3827,-29,7693,2061)
        work=(3840,-16,7680,2048)
        left_work=(0,0,3840,2064)
        for x in (3839,3877,3889,3947,3981,4129):
            for color_editor in (False,True):
                bounds, anchor, right_panel=pie_layout(ps,work,(x,744),2,color_editor=color_editor,left_work=left_work,right_inset=181)
                self.assertEqual(anchor,1300 if color_editor else 620)
                self.assertEqual(bounds[0]+anchor,x)
                self.assertFalse(right_panel)

    def test_left_monitor_gap_and_taskbar_keep_menu_in_photoshop_work_area(self):
        ps=(3827,-29,7693,2061)
        work=(3840,-16,7680,2048)
        for left_work in (None,(0,0,3800,2064),(0,300,3840,2064),(0,0,3840,700)):
            bounds, anchor, _=pie_layout(ps,work,(3889,744),2,left_work=left_work)
            self.assertEqual(bounds,(3840,244,1240,1000))
            self.assertEqual(anchor,400)

    def test_right_edge_uses_native_controls_and_shadow_width(self):
        ps=(3827,-29,7693,2061)
        work=(3840,-16,7680,2048)
        for color_editor in (False,True):
            for x, expected in ((7230,7230),(7654,7318)):
                bounds, anchor, editor_right=pie_layout(ps,work,(x,985),2,color_editor=color_editor,right_inset=181)
                self.assertEqual(bounds[0]+bounds[2],7680)
                self.assertEqual(bounds[0]+anchor,expected)
                self.assertEqual(bounds[1]+bounds[3]//2,985)
                self.assertFalse(editor_right)

    def test_home_grid_uses_wider_popup_and_clamps_at_monitor_edge(self):
        self.assertEqual(pie_bounds((0,0,1920,1080),(0,0,1920,1040),(960,520),1,home=True),(690,300,820,440))
        self.assertEqual(pie_bounds((0,0,1920,1080),(0,0,1920,1040),(1910,1030),1,home=True),(1100,600,820,440))
        self.assertEqual(pie_bounds((0,0,3840,2160),(0,0,3840,2080),(1920,1040),2,home=True),(1380,600,1640,880))

    def test_hold_begin_requests_radial_layout_before_opening(self):
        desktop = Desktop('http://localhost', lambda action:None)
        desktop.visible = False
        desktop.open = Mock(return_value={'message':'opened'})
        desktop.hold_begin()
        desktop.open.assert_called_once_with(hold=True)
        self.assertTrue(desktop.hold_active)

    def test_hold_release_is_once_and_hidden_menu_does_not_execute(self):
        desktop = Desktop('http://localhost', lambda action:None)
        desktop.hwnd, desktop.visible, desktop.hold_active = 2, True, True
        desktop._scope = lambda:True
        desktop.gui = SimpleNamespace(SendMessage=Mock())
        desktop.hold_release()
        desktop.hold_release()
        desktop.gui.SendMessage.assert_called_once_with(2, 0x800e, 0, 0)
        desktop.gui.SendMessage.reset_mock()
        desktop.hold_active, desktop.visible = True, False
        desktop.hold_release()
        desktop.gui.SendMessage.assert_not_called()

    def test_short_tap_pins_in_place_and_ignores_later_release(self):
        desktop = Desktop('http://localhost', lambda action:None)
        desktop.hwnd, desktop.visible, desktop.hold_active = 2, True, True
        desktop._scope = lambda:True
        def opened(**kwargs):
            desktop.visible = True
            return {'message':'opened'}
        desktop.open = Mock(side_effect=opened)
        desktop.gui = SimpleNamespace(SendMessage=Mock())
        desktop.hold_pin()
        desktop.open.assert_not_called()
        self.assertFalse(desktop.hold_active)
        desktop.gui.SendMessage.assert_called_once_with(2, 0x8010, 1, 0)
        desktop.hold_release()
        self.assertEqual(desktop.gui.SendMessage.call_count, 1)

    def test_release_uses_frozen_keyup_coordinates_including_negative_monitor(self):
        desktop = Desktop('http://localhost', lambda action:None)
        desktop.hwnd, desktop.visible, desktop.hold_active = 2, True, True
        desktop._scope = lambda:True
        desktop.gui = SimpleNamespace(SendMessage=Mock())
        desktop.hold_release(cursor=[-250, 730])
        desktop.gui.SendMessage.assert_called_once_with(2, 0x8015, -250, 730)
        desktop.hold_release(cursor=[-100, 900])
        self.assertEqual(desktop.gui.SendMessage.call_count, 1)
    def test_deferred_begin_keeps_the_original_physical_press_position(self):
        desktop=Desktop('http://localhost',lambda action:None);desktop.open=Mock(return_value={'message':'opened'})
        desktop.hold_begin(cursor=[-400,200])
        desktop.open.assert_called_once_with(hold=True,origin=[-400,200])

    def test_hold_release_after_focus_change_cancels_without_execution(self):
        desktop = Desktop('http://localhost', lambda action:None)
        desktop.hwnd, desktop.visible, desktop.hold_active = 2, True, True
        desktop._scope = lambda:False
        desktop.hide = Mock(return_value={'message':'closed'})
        desktop.gui = SimpleNamespace(SendMessage=Mock())
        desktop.hold_release()
        desktop.hide.assert_called_once_with(restore=False)
        desktop.gui.SendMessage.assert_not_called()

    def test_capslock_closes_input_paused_menu(self):
        desktop = Desktop('http://localhost', lambda action:None)
        desktop.visible = True
        desktop._scope = lambda:True
        desktop.require_action_focus = Mock(side_effect=RuntimeError('input paused'))
        desktop.hide = Mock(return_value={'message':'closed'})
        self.assertEqual(desktop.toggle()['message'], 'closed')
        desktop.require_action_focus.assert_not_called()

    def test_preview_close_hides_immediately_and_preserves_cancel_transaction(self):
        from backend.mask_preview import MaskPreview
        from tests.test_mask_preview import paint
        for in_scope in (True,False):
            desktop=Desktop('http://localhost',lambda action:None)
            desktop.hwnd,desktop.target,desktop.visible=2,1,True
            desktop._scope=lambda:in_scope
            desktop._is_menu_window=lambda h:False
            desktop._is_photoshop=lambda h:h==1
            desktop.gui=SimpleNamespace(GetForegroundWindow=lambda:1 if in_scope else 3,
                IsWindow=lambda h:True,ShowWindow=Mock(),SetForegroundWindow=Mock())
            desktop.event=Mock()
            preview=MaskPreview(desktop.menu_epoch,paint());desktop.mask_preview=preview
            desktop.hide()
            self.assertEqual(preview.phase,'cancel')
            self.assertFalse(desktop.visible)
            self.assertTrue(preview.feedback_shown)
            desktop.event.assert_not_called()
            desktop.gui.ShowWindow.assert_called_once_with(2,0)
            desktop.gui.SetForegroundWindow.assert_not_called()

    def test_hide_restores_ps_even_if_showwindow_changes_foreground(self):
        for initial in (2, 4):  # Main menu and floating ImGui submenu.
            desktop = Desktop('http://localhost', lambda action:None)
            desktop.hwnd, desktop.target, desktop.visible = 2, 1, True
            foreground = [initial]
            desktop._is_menu_window = lambda hwnd: hwnd in (2, 4)
            desktop._is_photoshop = lambda hwnd: hwnd == 1
            desktop.gui = SimpleNamespace(
                GetForegroundWindow=lambda:foreground[0], IsWindow=lambda hwnd:True,
                ShowWindow=lambda *args:foreground.__setitem__(0, 3),
                SetForegroundWindow=lambda hwnd:foreground.__setitem__(0, hwnd))
            desktop.hide()
            self.assertEqual(foreground[0], 1)
            self.assertFalse(desktop.visible)

    def test_right_close_route_restores_focus_and_allows_fresh_hold(self):
        desktop = Desktop('http://localhost', lambda action:None)
        desktop.hwnd, desktop.target = 2, 1
        desktop.visible = desktop.hold_active = True
        foreground = [2]
        desktop._is_menu_window = lambda hwnd: hwnd == 2
        desktop._is_photoshop = lambda hwnd: hwnd == 1
        desktop.gui = SimpleNamespace(
            GetForegroundWindow=lambda:foreground[0], IsWindow=lambda hwnd:True,
            ShowWindow=lambda *args:foreground.__setitem__(0, 3),
            SetForegroundWindow=lambda hwnd:foreground.__setitem__(0, hwnd))
        # Native right cancellation now sends Hide, not focus-neutral Dismiss.
        desktop.hide()
        self.assertEqual(foreground[0], 1)
        self.assertFalse(desktop.hold_active)
        def reopen(**kwargs):
            self.assertEqual(foreground[0], desktop.target)
            desktop.visible = True
            return {'message':'opened'}
        desktop.open = Mock(side_effect=reopen)
        desktop.hold_begin()
        self.assertTrue(desktop.visible and desktop.hold_active)
        desktop.open.assert_called_once_with(hold=True)

    def test_close_never_restores_over_another_application(self):
        desktop = Desktop('http://localhost', lambda action:None)
        desktop.hwnd, desktop.target, desktop.visible = 2, 1, True
        desktop._is_menu_window = lambda hwnd: hwnd == 2
        desktop._is_photoshop = lambda hwnd: hwnd == 1
        desktop.gui = SimpleNamespace(GetForegroundWindow=lambda:3,
            IsWindow=lambda hwnd:True, ShowWindow=Mock(), SetForegroundWindow=Mock())
        desktop.hide()
        desktop.gui.SetForegroundWindow.assert_not_called()


class MenuReopenTests(unittest.IsolatedAsyncioTestCase):
    async def test_busy_feedback_precedes_target_read_and_menu_can_reopen(self):
        with tempfile.TemporaryDirectory() as folder:
            started, release = asyncio.Event(), asyncio.Event()
            events = []
            class Bridge:
                busy = False
                async def execute(self, name, before_dispatch=None):
                    self.busy = True
                    self.event_seen = any(e.get('busy') and e.get('action') == name for e in events)
                    started.set()
                    await release.wait()
                    try:
                        await before_dispatch({})
                    finally:
                        self.busy = False
            bridge = Bridge()
            app = Application(15190, bridge=bridge, runtime=folder)
            desktop = SimpleNamespace(menu_epoch=1, visible=False, require_action_focus=lambda:None, hide=Mock())
            def reopen():
                desktop.menu_epoch += 1
                desktop.visible = True
            desktop.hold_begin = reopen
            app.desktop = desktop
            async def broadcast(event):events.append(event)
            app.broadcast = broadcast
            operation = asyncio.create_task(app.dispatch('action.execute', {'name':'label.red', 'menu_epoch':1}))
            await asyncio.wait_for(started.wait(), 2)
            self.assertTrue(bridge.event_seen)
            await asyncio.wait_for(app.dispatch('panel.hold_begin', {}), 1)
            self.assertTrue(desktop.visible)
            release.set()
            result = await operation
            self.assertTrue(result['cancelled'])
            self.assertIn('重新打开', result['message'])
            self.assertFalse(any(e.get('type')=='notice' and e.get('ok') is False for e in events))
            desktop.hide.assert_not_called()
            self.assertEqual(events[-1], {'type':'operation','busy':False})
            app.control.close()

    async def test_old_action_waiting_on_panel_lock_cannot_execute_after_reopen(self):
        with tempfile.TemporaryDirectory() as folder:
            bridge=SimpleNamespace(busy=False,execute=Mock())
            app=Application(15190,bridge=bridge,runtime=folder)
            app.desktop=SimpleNamespace(menu_epoch=1,require_action_focus=lambda:None,hide=Mock())
            async def broadcast(event):pass
            app.broadcast=broadcast
            await app.panel_lock.acquire()
            old=asyncio.create_task(app.dispatch('action.execute',{'name':'select','menu_epoch':1}))
            await asyncio.sleep(0)
            app.desktop.menu_epoch=2
            app.panel_lock.release()
            result=await old
            self.assertIn('旧菜单操作',result['message'])
            bridge.execute.assert_not_called()
            app.desktop.hide.assert_not_called()

    async def test_old_hide_waiting_on_panel_lock_does_not_close_new_pin(self):
        with tempfile.TemporaryDirectory() as folder:
            app = Application(15190, bridge=SimpleNamespace(busy=False), runtime=folder)
            app.desktop = SimpleNamespace(menu_epoch=1, hide=Mock())
            await app.panel_lock.acquire()
            stale = asyncio.create_task(app.dispatch('panel.hide', {'menu_epoch':1}))
            await asyncio.sleep(0)
            app.desktop.menu_epoch = 2
            app.panel_lock.release()
            await stale
            app.desktop.hide.assert_not_called()

    async def test_fast_reopen_survives_previous_operation_completion(self):
        with tempfile.TemporaryDirectory() as folder:
            started, release = asyncio.Event(), asyncio.Event()
            class Bridge:
                busy = False
                async def execute(self, name, before_dispatch=None):
                    self.busy = True
                    if before_dispatch:
                        await before_dispatch({'document_id':1,'active_layer_id':2,'layer_ids':[2]})
                    started.set()
                    await release.wait()
                    self.busy = False
                    return {'message':'done'}
            desktop = SimpleNamespace(visible=True, require_action_focus=lambda:None)
            def hide():
                desktop.visible = False
            desktop.hide = Mock(side_effect=hide)
            app = Application(15190, bridge=Bridge(), runtime=folder)
            app.desktop = desktop
            async def broadcast(event):
                pass
            app.broadcast = broadcast
            for _ in range(100):
                desktop.visible = True
                started.clear(); release.clear()
                operation = asyncio.create_task(app.dispatch('action.execute', {'name':'color'}))
                await asyncio.wait_for(started.wait(), 2)
                self.assertFalse(desktop.visible, 'Old menu must close before COM starts')
                desktop.visible = True  # A fresh CapsLock menu during COM.
                release.set()
                await operation
                self.assertTrue(desktop.visible, 'Completion must not close the newly opened menu')
            self.assertEqual(desktop.hide.call_count, 100)
            app.control.close()

    async def test_busy_status_preserves_connection_without_waiting_for_com(self):
        bridge = PhotoshopBridge()
        try:
            bridge._status = lambda:{'connected':True, 'document':'test.psd', 'message':'connected'}
            await bridge.status()
            bridge.busy = True
            bridge._status = Mock(side_effect=AssertionError('Busy status must not query COM'))
            status = await bridge.status()
            self.assertTrue(status['connected'])
            self.assertEqual(status['document'], 'test.psd')
            self.assertTrue(status['busy'])
            bridge._status.assert_not_called()
        finally:
            bridge.busy = False
            await bridge.close()
