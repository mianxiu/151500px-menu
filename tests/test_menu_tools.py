import asyncio
import io
import json
import os
import subprocess
from tempfile import TemporaryDirectory
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import AsyncMock, Mock, patch

from backend.menu_tools import MenuTools
from backend.text_tools import TextTools
from backend.layer_search import LayerSearch
from backend.desktop import Desktop, typeset_bounds
from backend.text_canvas import CanvasUnavailable, connection, valid_text_frame
from backend.paths import ROOT


class CanvasGeometryTests(unittest.TestCase):
    def test_generator_connection_uses_limited_process_query_and_verifies_both_paths(self):
        code='a'*32
        ready={'version':3,'token':code,'photoshopPid':11,'pid':12}
        host=r'C:\Photoshop\Photoshop.exe'
        worker=r'C:\Photoshop\node.exe'
        with TemporaryDirectory() as runtime:
            state=Path(runtime)/'text-canvas';state.mkdir()
            (state/'ready.json').write_text(json.dumps(ready),'utf-8')
            (state/'pairing').write_text(code,'ascii')
            with patch('win32process.GetWindowThreadProcessId',return_value=(4,11)), \
                 patch('win32api.OpenProcess') as opened, \
                 patch('win32process.GetModuleFileNameEx',side_effect=(host,worker,host,r'C:\Other\node.exe')):
                self.assertEqual(connection(runtime,51),ready)
                self.assertIsNone(connection(runtime,51))
                self.assertEqual([call.args for call in opened.call_args_list],[(0x1000,False,11),(0x1000,False,12)]*2)

    def test_high_dpi_window_can_move_when_the_initial_size_fills_photoshop(self):
        rect=(1920,0,3840,1080)
        work=(1920,0,3840,1040)
        self.assertEqual(typeset_bounds(rect,work,2),(2040,65,1680,910))

    def test_document_geometry_never_accepts_invalid_coordinates_or_colors(self):
        frame={'box':[.25,-.1,.9,.4],'wrap':True,'em':.03,'color':[0,0,255]}
        self.assertTrue(valid_text_frame(frame))
        self.assertFalse(valid_text_frame({**frame,'em':float('inf')}))
        self.assertFalse(valid_text_frame({**frame,'box':[.9,0,.25,1]}))
        self.assertFalse(valid_text_frame({**frame,'color':[255,0,-1]}))


class MenuToolsTests(unittest.IsolatedAsyncioTestCase):
    async def test_canvas_history_uses_existing_revision_and_rejects_an_edit(self):
        self.app.runtime='test';self.app.desktop.target=1
        self.tools.text.snapshot={'target':{},'text':'before','revision':'first','canvas_history':34}
        call=Mock()
        self.app.bridge=SimpleNamespace(transport=SimpleNamespace(call=call))
        memory=Mock()
        frame={'history':34}
        plan=self.tools.text.plan=object()
        with patch('backend.menu_tools.text_canvas.read',new=AsyncMock(return_value=(memory,frame))):
            accepted=await self.tools.request(self.request('text.canvas',args={'revision':'first'}))
            self.assertEqual(accepted['revision'],'first')
            self.assertIs(self.tools.text.plan,plan)
            frame['history']=35
            refused=await self.tools.request(self.request('text.canvas',request=2,args={'revision':'first'}))
            self.assertIn('error',refused)
            self.assertEqual(self.app.desktop.event.call_args.args[0]['canvas_state'],'stale')
            self.assertIsNone(self.tools.text.snapshot)
            self.assertIsNone(self.tools.text.plan)
        call.assert_not_called()
        memory.close.assert_called_once()

    async def test_typeset_can_open_without_generator(self):
        self.app.runtime=Path('test')
        with patch('backend.menu_tools.text_canvas.connection',return_value=None), \
             patch('backend.menu_tools.typeset_settings.load',return_value={}), \
             patch('backend.menu_tools.typeset_settings.load_styles',return_value={}):
            result=await self.tools.open({'menu_epoch':6,'params':{'mode':'typeset'}})
        self.assertEqual(result['epoch'],7)
        self.assertEqual(self.app.desktop.event.call_args.args[0]['mode'],'typeset')

    async def test_failed_background_retries_without_rereading_draft_and_old_epoch_is_rejected(self):
        self.app.runtime=Path('test')
        self.app.desktop.target=1
        saved=self.tools.text.snapshot={'target':{},'revision':'r','canvas_history':34}
        plan=self.tools.text.plan=object()
        memory=Mock()
        read=AsyncMock(side_effect=[CanvasUnavailable('background missing'),(memory,{'history':34})])
        with patch('backend.menu_tools.text_canvas.read',new=read):
            result=await self.tools.request(self.request('text.canvas',args={'revision':'r'}))
            self.assertIn('error',result)
            self.assertEqual(self.app.desktop.event.call_args.args[0]['canvas_state'],'failed')
            self.assertIs(self.tools.text.snapshot,saved)
            self.assertIs(self.tools.text.plan,plan)
            result=await self.tools.request(self.request('text.canvas',request=2,args={'revision':'r'}))
            self.assertEqual(result['revision'],'r')
            self.assertIs(self.tools.text.plan,plan)
            self.tools.epoch=8
            self.tools.last_request=0
            result=await self.tools.request(self.request('text.canvas',request=3,args={'revision':'r'}))
            self.assertIn('error',result)
            self.assertEqual(read.await_count,2)

    async def test_original_canvas_rebinds_revision_only_for_unchanged_text(self):
        target={'session':'s','document_id':1,'active_layer_id':2,'layer_ids':[2]}
        self.tools.epoch=7
        self.app.runtime='test';self.app.desktop.target=1
        saved={'target':target,'text':'before','revision':'first','style_source':{}}
        self.tools.text.snapshot=saved
        fresh={**saved,'revision':'second'}
        self.app.bridge=SimpleNamespace(transport=SimpleNamespace(call=Mock(return_value={'text':fresh})))
        memory=Mock()
        with patch('backend.menu_tools.text_canvas.read',new=AsyncMock(return_value=(memory,{}))):
            def request(i,revision):
                return self.tools.request({'menu_epoch':7,'params':{'request':i,'action':'text.canvas','args':{'revision':revision}}})
            self.assertEqual((await request(1,'first'))['revision'],'second')
            fresh['text']='external edit'
            self.assertIn('error',await request(2,'second'))
            memory.close.assert_called_once()

    def setUp(self):
        self.app=SimpleNamespace(panel_lock=asyncio.Lock(),desktop=SimpleNamespace(
            require_menu_tools=Mock(),open_menu_tools=Mock(return_value=7),event=Mock(),
            menu_layer_context={'session':'s','document_id':1,'layer_ids':[2]},resize_menu_tools=Mock()),broadcast=AsyncMock())
        self.tools=MenuTools(self.app);self.tools.epoch=7
        self.available=patch('backend.menu_tools.require_available');self.available.start();self.addCleanup(self.available.stop)

    def request(self,action='layers.search',request=1,epoch=7,args=None):
        return {'menu_epoch':epoch,'params':{'request':request,'action':action,'args':args or {}}}

    async def test_direct_write_routes_through_the_same_fresh_menu_guard(self):
        async def write(args,menu_guard):
            await menu_guard();return {'message':'written'}
        self.tools.text.write=AsyncMock(side_effect=write)
        args={'revision':'r','draft':'one\ntwo'}
        result=await self.tools.request(self.request('text.write',args=args))
        self.assertEqual(result,{'message':'written'});self.assertTrue(self.app.desktop.event.call_args.args[0]['ok'])
        self.assertEqual(self.tools.text.write.call_args.args[0],args)
        self.assertEqual(self.app.desktop.require_menu_tools.call_count,2)
        self.app.broadcast.assert_awaited_once_with({'type':'notice','scope':'operation','ok':True,'message':'written'})

    async def test_table_follows_only_the_verified_new_layer_in_the_same_document(self):
        desktop=self.app.desktop
        desktop.visible=True;desktop.menu_epoch=7
        desktop.layer_context={**desktop.menu_layer_context,'layer_ids':[3],'plugin_time_ms':int(time.time()*1000)}
        self.tools.text.table=AsyncMock(return_value={'message':'OK','result':'{"verified":true,"layer_id":3}'})
        result=await self.tools.request(self.request('text.table'))
        self.assertTrue(result['continue_editing'])
        self.assertEqual(desktop.menu_layer_context['layer_ids'],[3])
        desktop.layer_context={**desktop.layer_context,'document_id':2}
        result=await self.tools.request(self.request('text.table',request=2))
        self.assertFalse(result['continue_editing'])
        self.assertEqual(desktop.menu_layer_context['document_id'],1)

    async def test_open_is_epoch_bound_and_never_queues(self):
        await self.tools.open({'menu_epoch':6,'params':{'mode':'split'}})
        self.app.desktop.open_menu_tools.assert_called_once_with('split',6)
        self.app.desktop.event.assert_called_once_with({'type':'menu.tools.opened','epoch':7,'mode':'split'})
        await self.app.panel_lock.acquire()
        with self.assertRaisesRegex(RuntimeError,'未接受'):await self.tools.open({})
        self.app.panel_lock.release()
        self.assertEqual(self.app.desktop.open_menu_tools.call_count,1)

    async def test_stale_menu_and_duplicate_requests_never_call_host(self):
        self.tools.search.search=AsyncMock(return_value={'message':'done','items':[]})
        await self.tools.request(self.request(epoch=6))
        self.tools.search.search.assert_not_called()
        await self.tools.request(self.request())
        await self.tools.request(self.request())
        self.tools.search.search.assert_awaited_once()
        self.assertFalse(self.tools.pending)
        self.assertFalse(self.app.desktop.event.call_args.args[0]['ok'])

    async def test_closed_menu_is_checked_before_reads(self):
        self.app.desktop.require_menu_tools.side_effect=RuntimeError('closed')
        self.tools.search.search=AsyncMock()
        await self.tools.request(self.request())
        self.tools.search.search.assert_not_called()
        self.assertIn('closed',self.app.desktop.event.call_args.args[0]['error'])

    async def test_changed_text_target_invalidates_snapshot(self):
        self.tools.text.read=AsyncMock(return_value={'target':{'session':'s','document_id':1,'layer_ids':[999]}})
        self.tools.text.snapshot={'revision':'old'}
        await self.tools.request(self.request('text.read'))
        self.assertIsNone(self.tools.text.snapshot)
        self.assertIn('目标已改变',self.app.desktop.event.call_args.args[0]['error'])

    async def test_preview_does_not_take_the_write_slot_and_drops_old_epoch(self):
        entered=asyncio.Event();release=asyncio.Event()
        async def read(args,menu_guard=None):entered.set();await release.wait();return {'available':False,'reason':'empty'}
        self.tools.search.preview=AsyncMock(side_effect=read)
        args={'menu_epoch':7,'params':{'request':1,'search_id':'q','index':0}}
        task=asyncio.create_task(self.tools.preview(args));await entered.wait()
        self.assertFalse(self.tools.pending)
        await self.tools.preview(args)
        self.assertEqual(self.tools.search.preview.await_count,1)
        self.assertEqual(self.tools.preview_request,(7,1))
        self.app.desktop.event.assert_not_called()
        self.tools.epoch=8;self.app.desktop.event.reset_mock();release.set();await task
        self.app.desktop.event.assert_not_called();self.assertIsNone(self.tools.preview_request)

    async def test_preview_checks_closed_and_busy_menu_before_host(self):
        self.tools.search.preview=AsyncMock()
        args={'menu_epoch':7,'params':{'request':1,'search_id':'q','index':0}}
        self.tools.pending=True;await self.tools.preview(args)
        self.tools.search.preview.assert_not_called();self.tools.pending=False
        self.app.desktop.require_menu_tools.side_effect=RuntimeError('closed');await self.tools.preview(args)
        self.tools.search.preview.assert_not_called()

    async def test_write_rechecks_menu_after_durable_capture(self):
        async def split(args,menu_guard):
            self.tools.epoch=8
            await menu_guard({'document_id':1,'layer_ids':[2]})
        self.tools.text.split=AsyncMock(side_effect=split)
        await self.tools.request(self.request('text.split'))
        self.assertFalse(self.app.desktop.event.call_args.args[0]['ok'])
        self.assertIn('工具会话已改变',self.app.desktop.event.call_args.args[0]['error'])
        self.assertFalse(self.app.broadcast.call_args.args[0]['ok'])

    async def test_executor_target_argument_reaches_menu_guard(self):
        async def focus(args,menu_guard):
            await menu_guard({'document_id':1,'layer_ids':[2]})
            return {'message':'done'}
        self.tools.search.focus=AsyncMock(side_effect=focus)
        result=await self.tools.request(self.request('layers.focus'))
        self.assertEqual(result,{'message':'done'})
        self.assertTrue(self.app.desktop.event.call_args.args[0]['ok'])
        self.app.desktop.require_menu_tools.assert_called_with(7)

    async def test_pending_request_is_rejected_without_queue(self):
        self.tools.pending=True;self.tools.search.search=AsyncMock()
        await self.tools.request(self.request())
        self.tools.search.search.assert_not_called()
        self.assertTrue(self.tools.pending)
        self.tools.pending=False;self.tools.preview_request=(7,9)
        await self.tools.request(self.request())
        self.tools.search.search.assert_not_called()
        self.assertEqual(self.tools.preview_request,(7,9))

    async def test_duplicate_in_flight_cannot_finish_the_original_ui_request(self):
        self.tools.pending=True;self.tools.last_request=1
        await self.tools.request(self.request())
        self.app.desktop.event.assert_not_called();self.assertTrue(self.tools.pending)

    async def test_split_and_focus_keep_native_feedback_and_forward_guard(self):
        app=SimpleNamespace(operation_lock=asyncio.Lock(),desktop=SimpleNamespace(focus_search_result=Mock()),
            bridge=SimpleNamespace(execute_text=AsyncMock(return_value={'message':'done'}),execute_focus=AsyncMock(return_value={'message':'done'}),busy=False,layer_context={}),broadcast=AsyncMock())
        guard=AsyncMock()
        text=TextTools(app);text.available=Mock();target={'session':'s','document_id':1,'active_layer_id':2,'layer_ids':[2]}
        text.snapshot={'revision':'r','target':target};params={'revision':'r','breaks':[2,3]}
        await text.split(params,menu_guard=guard)
        app.desktop.focus_search_result.assert_called_once_with(keep_menu=True)
        app.bridge.execute_text.assert_awaited_once_with(target,params,'text.split',before_dispatch=guard)
        app.desktop.focus_search_result.reset_mock()
        search=LayerSearch(app);search.available=Mock();search.valid_until=float('inf');row={'layer_id':3}
        search.result={'search_id':'x','session':'s','items':[row]}
        await search.focus({'search_id':'x','index':0},menu_guard=guard)
        app.desktop.focus_search_result.assert_called_once_with(keep_menu=True)
        app.bridge.execute_focus.assert_awaited_once_with('s',row,before_dispatch=guard)


class MenuToolsDesktopTests(unittest.TestCase):
    def test_preview_result_reaches_native_transport_intact(self):
        desktop=Desktop('http://localhost',Mock());stream=io.StringIO()
        desktop.process=SimpleNamespace(poll=lambda:None,stdin=stream)
        for payload in ({'ok':True,'result':{'available':True,'width':1,'height':1,'pixels':[255,0,0,255]}},
                        {'ok':True,'result':{'available':False,'reason':'empty'}},
                        {'ok':False,'error':'unknown'}):
            event={'type':'menu.tools.preview.result','epoch':7,'request':9,**payload}
            desktop.event(event)
            self.assertEqual(json.loads(stream.getvalue()),event)
            stream.seek(0);stream.truncate()
        desktop.event({'type':'not-a-menu-event'})
        self.assertEqual(stream.getvalue(),'')

    def test_native_menu_hands_focus_to_its_verified_owner(self):
        desktop=Desktop('http://localhost',Mock());desktop.hwnd=2;desktop.menu_epoch=7;desktop.visible=desktop.menu_tools_active=True
        foreground=[2];desktop.gui=Mock();desktop.gui.GetForegroundWindow.side_effect=lambda:foreground[0]
        desktop._is_photoshop=Mock(return_value=True);desktop._workspace_root=lambda h:1 if h==1 else None
        desktop._is_menu_window=lambda h:h==2
        desktop.gui.EnumWindows.side_effect=lambda callback,arg:callback(1,arg)
        desktop.gui.PostMessage.side_effect=lambda *args:foreground.__setitem__(0,1)
        desktop.gui.SendMessageTimeout.side_effect=TimeoutError("(1460, 'SendMessageTimeout')")
        desktop.require_action_focus=Mock();desktop.focus_search_result(keep_menu=True)
        args=desktop.gui.PostMessage.call_args.args
        self.assertEqual(args[:3],(2,0x8019,1));self.assertEqual(args[3]>>32,7)
        desktop.gui.SendMessageTimeout.assert_not_called()
        desktop.gui.SetForegroundWindow.assert_not_called()

    def test_delayed_focus_is_confirmed_without_resending_and_close_or_foreign_cancels(self):
        for transition in ('ready','closed','tools-closed','new-window','new-session','foreign','timeout','post-error'):
            with self.subTest(transition=transition):
                desktop=Desktop('http://localhost',Mock());desktop.hwnd=2;desktop.menu_epoch=7;desktop.visible=desktop.menu_tools_active=True
                foreground=[2];elapsed=[0.];desktop.gui=Mock();desktop.gui.GetForegroundWindow.side_effect=lambda:foreground[0]
                desktop._is_photoshop=Mock(return_value=True);desktop._workspace_root=lambda h:1 if h==1 else None;desktop._is_menu_window=lambda h:h==2
                desktop.gui.EnumWindows.side_effect=lambda callback,arg:callback(1,arg);desktop.require_action_focus=Mock()
                if transition=='post-error':desktop.gui.PostMessage.side_effect=OSError('message queue unavailable')
                def wait(seconds):
                    elapsed[0]+=.1
                    if elapsed[0]>=.3:
                        if transition=='ready':foreground[0]=1
                        elif transition=='closed':desktop.visible=False
                        elif transition=='tools-closed':desktop.menu_tools_active=False
                        elif transition=='new-window':desktop.hwnd=4
                        elif transition=='new-session':desktop.menu_epoch=8
                        elif transition=='foreign':foreground[0]=3
                with patch('backend.desktop.time.monotonic',side_effect=lambda:elapsed[0]),patch('backend.desktop.time.sleep',side_effect=wait):
                    if transition=='ready':desktop.focus_search_result(keep_menu=True)
                    else:
                        with self.assertRaisesRegex(RuntimeError,'未执行'):desktop.focus_search_result(keep_menu=True)
                desktop.gui.PostMessage.assert_called_once();desktop.gui.SendMessageTimeout.assert_not_called();desktop.gui.SetForegroundWindow.assert_not_called()
                self.assertEqual(desktop.require_action_focus.call_count,1 if transition=='ready' else 0)

    def test_foreign_foreground_is_not_activated_over_for_menu_tools(self):
        desktop=Desktop('http://localhost',Mock());desktop.hwnd=2;desktop.gui=Mock()
        desktop.gui.GetForegroundWindow.return_value=3
        desktop._is_photoshop=Mock(return_value=True);desktop._workspace_root=lambda h:1 if h==1 else None
        desktop._is_menu_window=lambda h:h==2
        desktop.gui.EnumWindows.side_effect=lambda callback,arg:callback(1,arg)
        with self.assertRaisesRegex(RuntimeError,'前台已改变'):desktop.focus_search_result(keep_menu=True)
        desktop.gui.SetForegroundWindow.assert_not_called();desktop.gui.SendMessageTimeout.assert_not_called()

    def test_open_rotates_epoch_and_waits_for_native_input_protection(self):
        desktop=Desktop('http://localhost',Mock())
        desktop.visible=True;desktop.hwnd=2;desktop.target=1;desktop.menu_epoch=6
        desktop.menu_layer_context={'kind':'text','layer_ids':[3]}
        desktop.preview_hint_anchor=[500,400];desktop.gui=Mock();desktop.gui.GetWindowRect.return_value=(0,0,1920,1080)
        desktop.icons=Mock();desktop.icons.u.GetDpiForWindow.return_value=96
        desktop.require_action_focus=Mock();desktop._native_config=Mock(return_value=9);desktop.native_applied_sequence=9
        with patch('win32api.GetCursorPos',return_value=(1100,600)),patch('win32api.GetMonitorInfo',return_value={'Work':(0,0,1920,1040)}):
            self.assertEqual(desktop.open_menu_tools('split',6),7)
        self.assertTrue(desktop.menu_tools_active);self.assertFalse(desktop.hold_active)
        self.assertEqual(desktop.text_tool_owner,1)
        self.assertEqual(desktop.gui.SendMessage.call_args_list[0].args,(2,0x800d,1,7))
        desktop.gui.SetWindowPos.assert_called_once_with(2,0,1112,260,632,780,0x4|0x10)
        self.assertTrue(all(type(v) is int for v in desktop.gui.SetWindowPos.call_args.args[2:6]))
        self.assertEqual(desktop.preview_hint_anchor,[1428,650])
        with self.assertRaisesRegex(RuntimeError,'重新打开'):desktop.require_menu_tools(6)
        desktop.gui.GetWindowRect.return_value=(-1800,100,-100,1000)
        with patch('win32api.GetCursorPos',return_value=(-1000,500)),patch('win32api.MonitorFromWindow',return_value=1),patch('win32api.GetMonitorInfo',return_value={'Work':(-1920,0,0,1040)}):
            self.assertEqual(desktop.open_menu_tools('search',7),8)
        self.assertIsNone(desktop.text_tool_owner)
        self.assertEqual(desktop.gui.SetWindowPos.call_args.args,(2,0,-988,512,432,274,0x4|0x10))
        self.assertEqual(desktop.preview_hint_anchor,[-772,649])
        with patch('win32api.GetCursorPos',return_value=(2500,2000)),patch('win32api.GetMonitorInfo',return_value={'Work':(-1920,0,0,1040)}):
            desktop.open_menu_tools('search',8)
        self.assertEqual(desktop.gui.SetWindowPos.call_args.args,(2,0,-532,726,432,274,0x4|0x10))

    def test_text_tool_passes_host_click_and_raises_above_owned_dock_without_activation(self):
        desktop=Desktop('http://localhost',Mock());desktop.gui=Mock()
        desktop.visible=True;desktop.target=desktop.text_tool_owner=1;desktop.hwnd=2
        desktop._workspace_root=lambda h:{1:1,3:1,5:5}.get(h)
        desktop.gui.GetWindowRect.return_value=(100,100,400,300)
        desktop.gui.WindowFromPoint.return_value=3
        desktop.gui.GetForegroundWindow.return_value=3
        self.assertFalse(desktop.outside_click((10,10)))
        self.assertTrue(desktop.visible)
        desktop.notify.assert_not_called()
        desktop._keep_text_tool_above_workspace()
        desktop._keep_text_tool_above_workspace()
        desktop.gui.SetWindowPos.assert_called_once_with(2,0,0,0,0,0,0x213)
        desktop.gui.SetForegroundWindow.assert_not_called()
        self.assertTrue(desktop._scope())
        desktop.gui.GetForegroundWindow.return_value=5
        self.assertFalse(desktop._scope())
        self.assertEqual(desktop.target,1)
        desktop._keep_text_tool_above_workspace()
        self.assertEqual(desktop.gui.SetWindowPos.call_count,1)

    def test_typeset_retains_session_across_foreign_clicks_without_granting_host_scope(self):
        desktop=Desktop('http://localhost',Mock());desktop.gui=Mock()
        desktop.visible=desktop.menu_tools_active=desktop.typeset_open=True
        desktop.target=desktop.text_tool_owner=1;desktop.hwnd=2;desktop.menu_epoch=7
        desktop.gui.GetClassName.return_value='Photoshop';desktop.gui.GetWindow.return_value=1
        desktop.gui.GetForegroundWindow.return_value=5
        desktop._workspace_root=lambda hwnd:1 if hwnd in (1,3) else 0
        desktop._is_menu_window=lambda hwnd:hwnd==2
        self.assertTrue(desktop.keep_typeset())
        self.assertFalse(desktop.outside_click((10,10)))
        self.assertIn('保留',desktop.dismiss()['message'])
        desktop.gui.ShowWindow.assert_not_called();desktop.gui.SetForegroundWindow.assert_not_called()
        with self.assertRaisesRegex(RuntimeError,'仅在 Photoshop'):desktop.require_menu_tools(7)
        desktop.gui.GetForegroundWindow.return_value=3
        desktop.require_menu_tools(7)
        self.assertTrue(desktop.visible);self.assertEqual(desktop.menu_epoch,7)
        desktop.gui.GetWindow.return_value=4
        self.assertFalse(desktop.keep_typeset())

    def test_search_results_resize_within_ps_and_closed_epoch_rejects(self):
        desktop=Desktop('http://localhost',Mock());desktop.hwnd=2;desktop.target=1;desktop.gui=Mock()
        desktop.require_menu_tools=Mock();desktop.icons=Mock();desktop.icons.u.GetDpiForWindow.return_value=96
        desktop.gui.GetWindowRect.side_effect=lambda h:(1500,800,1932,1100) if h==2 else (100,100,1900,1100)
        with patch('win32api.GetMonitorInfo',return_value={'Work':(0,0,1920,1040)}):desktop.resize_menu_tools(7,10)
        desktop.gui.SetWindowPos.assert_called_once_with(2,0,1468,556,432,484,0x4|0x10)
        desktop.require_menu_tools.side_effect=RuntimeError('closed')
        with self.assertRaisesRegex(RuntimeError,'closed'):desktop.resize_menu_tools(8,0)
        self.assertEqual(desktop.gui.SetWindowPos.call_count,1)

    def test_tools_caps_closes_instead_of_confirming_a_shape(self):
        desktop=Desktop('http://localhost',Mock());desktop.visible=True;desktop.hwnd=2
        desktop.menu_tools_active=desktop.color_editor_active=True
        desktop._scope=Mock(return_value=True);desktop.hide=Mock(return_value={'message':'closed'});desktop.gui=Mock()
        desktop.toggle();desktop.hide.assert_called_once();desktop.gui.SendMessage.assert_not_called()

    def test_split_rejects_non_text_unknown_locked_and_multiple_targets_before_opening(self):
        for value in ({'kind':'pixel','layer_ids':[3]}, {'kind':'unknown','layer_ids':[3]},
                      {'kind':'text','layer_ids':[3,4]}, {'kind':'text','layer_ids':[3],'locked':True}):
            with self.subTest(value=value):
                desktop=Desktop('http://localhost',Mock());desktop.visible=True;desktop.menu_epoch=6
                desktop.menu_layer_context=value;desktop.gui=Mock();desktop.require_action_focus=Mock()
                with self.assertRaisesRegex(RuntimeError,'未锁定的文字图层'):desktop.open_menu_tools('split',6)
                desktop.gui.SendMessage.assert_not_called();self.assertFalse(desktop.menu_tools_active)


@unittest.skipUnless(os.name=='nt' and (ROOT/'native/MenuHost.exe').is_file(),'Windows native menu')
class NativeMenuToolsTests(unittest.TestCase):
    def test_setwindowpos_and_session_change_keep_tool_ready(self):
        import queue,threading,win32gui,win32con as c
        ready=queue.Queue()
        process=subprocess.Popen([str(ROOT/'native/MenuHost.exe')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,text=True,encoding='utf-8',creationflags=0x08000000)
        threading.Thread(target=lambda:[ready.put(json.loads(line)) for line in process.stdout],daemon=True).start()
        try:
            hwnd=ready.get(timeout=10)['hwnd']
            message=lambda code,wp=0,lp=0:win32gui.SendMessageTimeout(hwnd,code,wp,lp,c.SMTO_ABORTIFHUNG,1000)[1]
            def event(value):
                process.stdin.write(json.dumps(value)+'\n');process.stdin.flush()
            def request():
                deadline=time.monotonic()+4
                while time.monotonic()<deadline:
                    value=ready.get(timeout=max(.01,deadline-time.monotonic()))
                    if value.get('action')=='menu.tools.request':return value
                self.fail('No automatic native search request')
            event({'type':'status','data':{'photoshop':{'connected':True,'uxp_ready':True,'uxp_version':'0.3.65','busy':False},'settings':{'enabled':True,'shortcuts':{}}}})
            for epoch in range(101,104):
                message(0x8011,16|2|64,epoch-1)
                win32gui.SetWindowPos(hwnd,c.HWND_TOP,100,100,620,500,c.SWP_SHOWWINDOW|c.SWP_NOACTIVATE)
                message(0x800d,1,epoch);message(0x8010,1)
                win32gui.SetWindowPos(hwnd,0,100,100,920,620,c.SWP_NOZORDER|c.SWP_NOACTIVATE)
                for event_epoch in (epoch-1,epoch):
                    process.stdin.write(json.dumps({'type':'menu.tools.opened','epoch':event_epoch,'mode':'search'})+'\n')
                    process.stdin.flush()
                deadline=time.monotonic()+3
                while (message(0x801d)&3!=3 or message(0x8012)!=1000) and time.monotonic()<deadline:time.sleep(.01)
                self.assertEqual(message(0x801d)&3,3)
                self.assertEqual(message(0x8012),1000,'A ready tool must also render visible content')
                first=request()
                self.assertEqual(first['session'],epoch)
                self.assertEqual(first['params']['args']['name'],'','Reopen restored a previous input buffer')
                event({'type':'menu.tools.result','epoch':epoch,'request':first['params']['request'],'action':'layers.search','ok':True,'result':{'items':[]}})
                query='中文'+str(epoch)
                time.sleep(.05)
                for char in query:message(0x286,ord(char))
                typed=request()
                self.assertEqual(typed['params']['args']['name'],query)
                event({'type':'menu.tools.result','epoch':epoch,'request':typed['params']['request'],'action':'layers.search','ok':True,'result':{'items':[]}})
                deadline=time.monotonic()+2
                while message(0x801d)&4 and time.monotonic()<deadline:time.sleep(.01)
                self.assertFalse(message(0x801d)&4)
                win32gui.ShowWindow(hwnd,c.SW_HIDE)
        finally:
            process.stdin.close()
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
            process.stdout.close()
