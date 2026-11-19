import asyncio
import json
from copy import deepcopy
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock, Mock, patch

from backend.gradient_acceptance import MAKE_GRADIENT
from backend.gradient_editor import GradientEditor, VIEW_SCRIPT, checked_draft, checked_view, draft_path, load_draft
from backend.settings import DEFAULTS, DEFAULT_GRADIENT_TINT, validate

BASE = {
    'revision':'initial:1',
    'fill':{'kind':'gradient','enabled':True,'method':'classic',
            'stops':[{'rgb':[11,22,33],'location':0,'origin':0,'midpoint':42,'source':'userStop'},
                     {'rgb':[60,70,80],'location':4096,'origin':1,'midpoint':50,'source':'userStop'}],
            'opacity_stops':[{'opacity':90,'location':0,'midpoint':53},{'opacity':20,'location':4096,'midpoint':50}],
            'options':{'style':'linear','angle':12,'scale':100,'reverse':False,'dither':False,'align':True,'smoothness':3072}},
    'stroke':{'kind':'solid','enabled':False,'rgb':[0,0,0],'width':1},
    'mask':{'feather':0,'density':100},
}

class GradientEditorTests(unittest.TestCase):
    def test_tint_setting_migrates_and_only_accepts_rgb_or_off(self):
        old = {key:value for key,value in DEFAULTS.items() if key != 'gradient_layer_tint'}
        self.assertEqual(validate(old)['gradient_layer_tint'],DEFAULT_GRADIENT_TINT)
        self.assertEqual(validate({**old,'gradient_layer_tint':'#ABC456'})['gradient_layer_tint'],'#abc456')
        self.assertEqual(validate({**old,'gradient_layer_tint':''})['gradient_layer_tint'],'')
        with self.assertRaises(ValueError):
            validate({**old,'gradient_layer_tint':'red'})

    def test_gradient_hover_preference_defaults_to_simple_and_persists(self):
        from backend.settings import Settings
        old = {key:value for key,value in DEFAULTS.items() if key != 'gradient_direct_overlay'}
        self.assertFalse(validate(old)['gradient_direct_overlay'])
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'settings.json'
            settings = Settings(path)
            settings.save({**old, 'gradient_direct_overlay':True})
            with self.assertRaises(ValueError):
                settings.save({**old, 'gradient_direct_overlay':'true'})
            self.assertTrue(Settings(path).get()['gradient_direct_overlay'])

    def test_overlay_refuses_unusable_photoshop_geometry(self):
        view={'matrix':[0, .5, -.5, 0, -75, 60],'viewport':[-960,30,0,900],
              'document':[600,400],'layer':[100,100,400,350]}
        self.assertEqual(checked_view(view),view)
        for invalid in ({'matrix':[1,2,1,2,0,0]},{'viewport':[0,0,-20,50]},
                        {'layer':[1,2,3,float('inf')]},{'document':[True,10]}):
            with self.assertRaises(ValueError):checked_view({**view,**invalid})

    def test_old_draft_inherits_only_new_center_fields(self):
        base=deepcopy(BASE)
        base['fill']['options'].update(offset_x=25.5,offset_y=-37.25)
        with tempfile.TemporaryDirectory() as root:
            path=Path(root)/'draft.json'
            old=deepcopy(BASE)
            old['fill']['options']['scale']=125
            path.write_text(json.dumps(old),'utf-8')
            actual=load_draft(path,base)
            self.assertEqual(actual['fill']['options'],{**base['fill']['options'],'scale':125})
            for value in (1001,-1001,True,'0',None,float('nan')):
                invalid=deepcopy(actual)
                invalid['fill']['options']['offset_x']=value
                with self.assertRaises(ValueError):checked_draft(base,invalid)
            old['fill']['options'].pop('angle')
            path.write_text(json.dumps(old),'utf-8')
            with self.assertRaises(ValueError):load_draft(path,base)

    def test_resume_matches_saved_target_and_original_style(self):
        target={'session':'first','document_id':5,'active_layer_id':4}
        snapshot={'baseline':'unchanged','document_path':r'C:\Art','document_name':'same.psd'}
        with tempfile.TemporaryDirectory() as root:
            path=draft_path(root,target,snapshot)
            self.assertEqual(path,draft_path(root,{**target,'session':'second','document_id':80},snapshot))
            self.assertNotEqual(path,draft_path(root,target,{**snapshot,'baseline':'modified'}))
            self.assertNotEqual(path,draft_path(root,target,{**snapshot,'document_name':'other.psd'}))
            value=deepcopy(BASE)
            value['fill']['opacity_stops'][0]['opacity']=24.5
            path.parent.mkdir()
            path.write_text(json.dumps(value),'utf-8')
            self.assertEqual(load_draft(path,{**BASE,'revision':'fresh:2'}),{**value,'revision':'fresh:2'})
            path.write_text('{bad json','utf-8')
            with self.assertRaises(ValueError):load_draft(path,BASE)
            self.assertEqual(path.read_text('utf-8'),'{bad json')

    def test_gradient_draft_rejects_unrelated_changes_and_invalid_values(self):
        for change in (
            lambda v:v['stroke'].update(width=9),
            lambda v:v['mask'].update(density=30),
            lambda v:v['fill']['opacity_stops'][0].update(opacity=101),
            lambda v:v['fill']['options'].update(smoothness=4097),
            lambda v:v['fill']['stops'][0].update(origin=32),
            lambda v:v['fill']['stops'][0].update(midpoint=50.25),
            lambda v:v['fill'].pop('opacity_stops'),
        ):
            value=deepcopy(BASE)
            change(value)
            with self.assertRaises(ValueError):checked_draft(BASE,value)
        self.assertEqual(checked_draft(BASE,BASE),BASE)

    def test_fixed_fixture_and_legacy_preview_parse(self):
        legacy=(Path(__file__).resolve().parents[1]/'jsx/legacy_shape_preview.jsx').read_text('utf-8')
        check=subprocess.run(['node','-e',"for(const s of JSON.parse(require('node:fs').readFileSync(0,'utf8')))new (require('node:vm').Script)(s);"],
                             input=json.dumps(['(function(){'+MAKE_GRADIENT+'})()',legacy,VIEW_SCRIPT]),capture_output=True,text=True,timeout=10,creationflags=0x08000000)
        self.assertEqual(check.returncode,0,check.stderr)


class GradientInputRegistrationTests(unittest.TestCase):
    def test_registration_requires_hook_ack_before_editor_opens(self):
        import threading
        from backend.desktop import Desktop
        desktop=SimpleNamespace(native_applied=threading.Condition(),native_applied_sequence=4,_native_config=Mock(return_value=4))
        Desktop.register_gradient_window(desktop,101,202)
        self.assertEqual(desktop.gradient_window,(101,202))
        desktop._native_config.return_value=0
        with self.assertRaisesRegex(RuntimeError,'输入保护未就绪'):
            Desktop.register_gradient_window(desktop,303,202)


class GradientWindowLifecycleTests(unittest.IsolatedAsyncioTestCase):
    async def test_tint_follows_only_the_verified_target(self):
        target = {'session':'s','document_id':2,'layer_ids':[4]}
        desktop = SimpleNamespace(layer_context={**target,'plugin_time_ms':1})
        editor = GradientEditor(SimpleNamespace(desktop=desktop))
        editor.target = target
        editor.phase = 'editing'
        editor.process = SimpleNamespace(returncode=None)
        editor.send = AsyncMock()
        contexts = iter(({}, {}, {**desktop.layer_context,'document_id':3}))
        async def step(_):
            desktop.layer_context = next(contexts,None)
            if desktop.layer_context is None:
                editor.process.returncode = 0
            elif not desktop.layer_context and editor.send.await_count==2:
                editor.phase = 'preview'
                editor.preview = object()
        with (patch('backend.gradient_editor.fresh',side_effect=bool),
              patch('backend.gradient_editor.asyncio.sleep',side_effect=step)):
            await editor.watch_tint_target(editor.process)
        self.assertEqual([c.args[0]['verified'] for c in editor.send.await_args_list],[True,False,True,False])

    async def test_opening_view_waits_for_editor_setup(self):
        editor = GradientEditor(None)
        event = {'type':'gradient.view','serial':1,'refresh':False,'entering':True}
        process = SimpleNamespace(stdout=SimpleNamespace(readline=AsyncMock(side_effect=[json.dumps(event).encode(), b''])), stdin=Mock())
        editor.request_view = AsyncMock()
        async with editor.lock:
            reader = asyncio.create_task(editor.read_window(process))
            await asyncio.sleep(0)
            editor.request_view.assert_not_awaited()
        await reader
        editor.request_view.assert_awaited_once_with(event)

    async def test_unavailable_overlay_saves_draft_and_closes_without_mode_fallback(self):
        app = SimpleNamespace(desktop=SimpleNamespace(sync_native=Mock(), _send_menu_event=Mock(), menu_epoch=42))
        event = {'type':'gradient.unavailable','message':'画布空间不足','draft':deepcopy(BASE)}
        stream = AsyncMock(side_effect=[json.dumps(event).encode(), b''])
        process = SimpleNamespace(stdout=SimpleNamespace(readline=stream), stdin=Mock())
        editor = GradientEditor(app)
        editor.process = process
        editor.send = AsyncMock()
        editor.request = AsyncMock()
        await editor.read_window(process)
        editor.request.assert_awaited_once_with('gradient.cancel', event)
        self.assertEqual(editor.message, '画布空间不足')

    async def test_cancel_does_not_wait_for_view_read_or_resume_preview(self):
        desktop = SimpleNamespace(sync_native=Mock())
        editor = GradientEditor(SimpleNamespace(desktop=desktop, operation_lock=asyncio.Lock()))
        editor.phase = 'editing'
        editor.base, editor.draft = {}, {'edited': True}
        editor.require_focus = Mock()
        read_started, release_read, closed = asyncio.Event(), asyncio.Event(), asyncio.Event()

        async def read_view():
            read_started.set()
            await release_read.wait()
            return {'tool':'moveTool'}

        async def stream():
            yield b'{"type":"gradient.view","serial":1,"refresh":true}'
            await read_started.wait()
            yield b'{"type":"gradient.cancel"}'
            await closed.wait()
            yield b''

        async def send(event):
            if event.get('phase') == 'closed':
                closed.set()

        editor.read_view = read_view
        editor.send = AsyncMock(side_effect=send)
        editor.request = AsyncMock(wraps=editor.request)
        events = stream()
        process = editor.process = SimpleNamespace(stdout=SimpleNamespace(readline=lambda:anext(events)), stdin=Mock())
        with patch('backend.gradient_editor.require_available'):
            task = asyncio.create_task(editor.read_window(process))
            try:
                await asyncio.wait_for(closed.wait(), 1)
                self.assertFalse(release_read.is_set())
            finally:
                release_read.set()
                await asyncio.wait_for(task, 1)
        editor.request.assert_awaited_once_with('gradient.cancel', {'type':'gradient.cancel'})

    async def test_verified_rollback_refreshes_host_revision_without_replacing_draft_identity(self):
        shape = {**deepcopy(BASE), 'available':True, 'revision':'fresh:2', 'baseline':'raw-original'}
        call = Mock(return_value={'shape_editor':shape, 'gradient_target':{'document_id':5}})
        editor = GradientEditor(SimpleNamespace(bridge=SimpleNamespace(transport=SimpleNamespace(call=call))))
        editor.base = deepcopy(BASE)
        editor.draft = deepcopy(BASE)
        editor.target = {'document_id':5}
        editor.baseline = 'raw-original'
        editor.revision = BASE['revision']
        editor.require_focus = Mock()
        editor.hwnd = 1
        editor.phase = 'preview'
        await editor.finished({'result':json.dumps({'verified':True,'cancelled':True,'rollback_verified':True})})
        await editor.refresh_restored_revision()
        self.assertEqual(editor.revision, 'fresh:2')
        self.assertEqual(editor.base['revision'], BASE['revision'])
        self.assertEqual(editor.draft['revision'], BASE['revision'])
        self.assertFalse(editor.refresh_revision)
        editor.phase = 'preview'
        editor.preview = Mock()
        editor.in_scope = Mock(return_value=True)
        editor.save = AsyncMock()
        value = deepcopy(BASE)
        value['fill']['options']['angle'] = 25
        await editor.request('gradient.update', {'draft':value})
        editor.preview.update.assert_called_once_with({**value,'revision':'fresh:2'}, commit=False, immediate=True)

    async def test_revision_refresh_refuses_external_changes_and_target_switches(self):
        shape = {**deepcopy(BASE), 'available':True, 'revision':'fresh:2', 'baseline':'raw-original'}
        target = {'document_id':5}
        call = Mock()
        editor = GradientEditor(SimpleNamespace(bridge=SimpleNamespace(transport=SimpleNamespace(call=call))))
        editor.base, editor.target = deepcopy(BASE), target
        editor.baseline, editor.revision = 'raw-original', BASE['revision']
        editor.require_focus = Mock()
        editor.refresh_revision = True
        changed = deepcopy(shape)
        changed['fill']['options']['angle'] = 90
        for observed, actual in (({**shape,'baseline':'external'}, target),
                                 (shape, {'document_id':6}), (changed, target)):
            call.return_value = {'shape_editor':observed, 'gradient_target':actual}
            with self.assertRaises(RuntimeError):
                await editor.refresh_restored_revision()
            self.assertTrue(editor.refresh_revision)
            self.assertEqual(editor.revision, BASE['revision'])

    async def test_only_transient_com_rejection_retries_the_view(self):
        editor = GradientEditor(SimpleNamespace(operation_lock=asyncio.Lock()))
        editor.phase = 'editing'
        editor.require_focus = Mock()
        editor.send = AsyncMock()
        busy = OSError('Photoshop is busy')
        busy.hresult = -2147417846
        view = {'tool':'moveTool'}
        editor.read_view = AsyncMock(side_effect=[busy, view, RuntimeError('target changed')])
        with patch('backend.gradient_editor.require_available'):
            await editor.request_view({'serial':1,'refresh':False,'entering':True})
            busy_result = editor.send.await_args.args[0]
            self.assertTrue(busy_result['retry'])
            self.assertIsNone(busy_result['view'])
            await editor.request_view({'serial':2,'refresh':True,'entering':True})
            self.assertEqual(editor.send.await_args.args[0]['view'], view)
            await editor.request_view({'serial':3,'refresh':True})
            self.assertNotIn('retry', editor.send.await_args.args[0])
            self.assertIsNone(editor.send.await_args.args[0]['view'])

    async def test_com_unavailable_uses_view_from_the_single_owned_uxp_host(self):
        target={'session':'selected-uxp','document_id':3,'active_layer_id':7,'layer_ids':[7]}
        view={'matrix':[1,0,0,1,0,0],'viewport':[100,100,800,600],
              'document':[700,500],'layer':None,'tool':'moveTool'}
        from backend.photoshop_com_default import PhotoshopComUnavailable
        unavailable=PhotoshopComUnavailable('Operation unavailable')
        transport=SimpleNamespace(observe=Mock(return_value=(None,{'session':target['session']})),
                                  call=Mock(return_value={'session':target['session'],'gradient_view':view}))
        bridge=SimpleNamespace(executor=None,transport=transport,_application=Mock(side_effect=unavailable))
        editor=GradientEditor(SimpleNamespace(bridge=bridge))
        editor.target,editor.owner=target,777
        editor.require_focus=Mock()
        process=SimpleNamespace(GetWindowThreadProcessId=lambda hwnd:(4,9))
        with patch.dict(sys.modules,win32process=process), patch('backend.gradient_editor.photoshop_process_ids') as pids, \
                patch('backend.gradient_editor.running_application',side_effect=unavailable) as attach:
            pids.return_value=(9,)
            self.assertEqual(await editor.read_view(),view)
            attach.assert_called_once_with(bridge._application,9)
            transport.call.assert_called_once_with('inspect',gradient_view_target=target)
            transport.call.reset_mock()
            transport.observe.return_value[1]['probe_version']='0.3.110'
            attach.reset_mock()
            self.assertEqual(await editor.read_view(focus_return=True),view)
            attach.assert_not_called()
            transport.call.assert_called_once_with('inspect',gradient_view_target=target)
            transport.call.reset_mock()
            pids.return_value=(9,12)
            with self.assertRaisesRegex(RuntimeError,'唯一'):
                await editor.read_view()
            transport.call.assert_not_called()

    async def test_temporary_hand_tool_keeps_overlay_but_settled_tool_change_closes_it(self):
        editor = GradientEditor(SimpleNamespace(operation_lock=asyncio.Lock()))
        editor.phase = 'editing'
        editor.require_focus = Mock()
        editor.send = AsyncMock()
        editor.request = AsyncMock()
        editor.read_view = AsyncMock(side_effect=[
            {'tool':'moveTool'}, {'tool':'moveTool'}, {'tool':'cropTool'},
        ])
        with patch('backend.gradient_editor.require_available'):
            await editor.request_view({'serial':1,'refresh':False,'entering':True})
            await editor.request_view({'serial':2,'refresh':True,'tracking':True})
            self.assertEqual(editor.send.await_args.args[0],{'type':'gradient.view','serial':2,'tracking':True})
            self.assertEqual(editor.read_view.await_count,1)
            await editor.request_view({'serial':3,'refresh':True})
            self.assertEqual(editor.send.await_args.args[0]['view']['tool'],'moveTool')
            await editor.request_view({'serial':4,'refresh':True})
        self.assertEqual(editor.send.await_args.args[0],{'type':'gradient.view','serial':4,'tool_changed':True})
        editor.request.assert_not_awaited()

    async def test_view_motion_rechecks_settled_view_without_restarting_preview(self):
        app = SimpleNamespace(operation_lock=asyncio.Lock())
        editor = GradientEditor(app)
        editor.phase = 'preview'
        preview = editor.preview = Mock()
        editor.base, editor.draft = {}, {'edited': True}
        editor.require_focus = Mock()
        editor.send = AsyncMock()
        editor.request = AsyncMock()
        editor.read_view = AsyncMock(return_value={'matrix':[1,0,0,1,4,6]})
        editor.overlay_tool = 'moveTool'

        async def rollback():
            editor.preview = None
            editor.phase = 'editing'

        editor.task = asyncio.create_task(rollback())
        with patch('backend.gradient_editor.require_available'):
            await editor.request_view({'serial':1,'refresh':True,'tracking':True})
            preview.cancel.assert_called_once_with()
            editor.request.assert_not_awaited()
            editor.read_view.assert_not_awaited()
            self.assertEqual(editor.send.await_args.args[0],{'type':'gradient.view','serial':1,'tracking':True})
            await editor.request_view({'serial':2,'refresh':True,'tracking':False})
        editor.request.assert_not_awaited()
        editor.read_view.assert_awaited_once_with('moveTool')

    async def test_preview_feedback_requires_the_latest_verified_host_frame(self):
        editor=GradientEditor(None);editor.phase='preview';editor.send_state=AsyncMock()
        preview=editor.preview=SimpleNamespace(phase='preview',seq=4)
        await editor.preview_progress(preview,{'stage':'ready','frame':3})
        self.assertEqual(editor.message,'正在更新 PS 画布…')
        await editor.preview_progress(preview,{'stage':'ready','frame':4})
        self.assertEqual(editor.message,'已更新 PS 画布')
        preview.phase='cancel';editor.message='保留取消状态'
        await editor.preview_progress(preview,{'stage':'ready','frame':5})
        self.assertEqual(editor.message,'保留取消状态')
        self.assertEqual(editor.send_state.await_count,2)

    async def test_owned_dropdown_keeps_preview_scope_and_foreign_windows_are_rejected(self):
        gui=SimpleNamespace(GetClassName=Mock(return_value='ImGui Platform'),GetAncestor=Mock(return_value=11))
        editor=GradientEditor(SimpleNamespace(desktop=SimpleNamespace(gui=gui)))
        editor.hwnd=11;editor.process=SimpleNamespace(pid=22)
        process=SimpleNamespace(GetWindowThreadProcessId=Mock(return_value=(33,22)))
        with patch.dict(sys.modules,win32process=process):
            self.assertTrue(editor.owns_foreground(44))
            gui.GetAncestor.return_value=99
            self.assertFalse(editor.owns_foreground(44))
            gui.GetAncestor.return_value=11;process.GetWindowThreadProcessId.return_value=(33,99)
            self.assertFalse(editor.owns_foreground(44))

    async def test_returning_to_photoshop_rechecks_view_without_replaying_saved_preview(self):
        editor=GradientEditor(SimpleNamespace(operation_lock=asyncio.Lock()))
        editor.phase='editing';editor.live=True;editor.base={};editor.draft={'edited':True}
        editor.refresh_revision=True
        editor.require_focus=Mock();editor.send=AsyncMock();editor.read_view=AsyncMock(return_value={'tool':'moveTool'})
        editor.overlay_tool='moveTool';editor.request=AsyncMock()
        with patch('backend.gradient_editor.require_available'):
            await editor.request_view({'serial':1,'refresh':True,'focus_return':True})
        editor.read_view.assert_awaited_once_with(focus_return=True)
        editor.request.assert_not_awaited()

    async def test_opening_saved_draft_previews_after_view_verification(self):
        editor = GradientEditor(SimpleNamespace(operation_lock=asyncio.Lock()))
        editor.phase = 'editing'
        editor.live = True
        editor.base, editor.draft = {}, {'edited': True}
        editor.require_focus = Mock()
        editor.send = AsyncMock()
        editor.request = AsyncMock()
        editor.read_view = AsyncMock(return_value={'tool':'moveTool'})
        with patch('backend.gradient_editor.require_available'):
            await editor.request_view({'serial':1, 'refresh':False, 'entering':True})
            await editor.request_view({'serial':2, 'refresh':True})
            await editor.request_view({'serial':3, 'refresh':True, 'focus_return':True})
            await editor.request_view({'serial':4, 'refresh':True, 'entering':True})
        editor.request.assert_awaited_once_with('gradient.update',{'draft':editor.draft})

    async def test_mode_switch_keeps_live_preview_and_uses_same_view(self):
        editor=GradientEditor(None)
        editor.phase='preview'
        editor.preview=Mock()
        editor.view={'matrix':[1,0,0,1,0,0]}
        editor.require_focus=Mock()
        editor.send=AsyncMock()
        editor.read_view=AsyncMock()
        await editor.request_view({'serial':1,'refresh':False})
        editor.preview.cancel.assert_not_called()
        editor.read_view.assert_not_awaited()
        editor.send.assert_awaited_once_with({'type':'gradient.view','serial':1,'view':editor.view,'message':''})

    async def test_caps_adjustment_opens_only_after_verified_confirmation(self):
        for live, verified in ((False,True),(True,True),(True,False)):
            with self.subTest(live=live,verified=verified), tempfile.TemporaryDirectory() as root:
                source=deepcopy(BASE)
                source['stroke']={**deepcopy(source['fill']),'width':2}
                adjusted=deepcopy(source)
                adjusted['stroke']['stops'][0]['rgb']=[190,25,65]
                target={'session':'s','document_id':5,'layer_ids':[4],'active_layer_id':4}
                shape={**adjusted,'available':True,'baseline':'confirmed','revision':'confirmed:2'}
                warming = threading.Event()
                window = SimpleNamespace(stdin=Mock())
                def read_target(*_, **__):
                    self.assertTrue(warming.wait(2), 'Native window did not warm alongside target inspection')
                    return {'gradient_target':target,'shape_editor':shape}
                inspect=Mock(side_effect=read_target)
                desktop=SimpleNamespace(visible=True,menu_epoch=9,target=77,color_editor_active=True,
                    menu_layer_context={**target,'shape_editor':source},panel_context=SimpleNamespace(current={}),
                    preview_input_guard=None,require_action_focus=Mock(),hide_if_epoch=Mock())
                app=SimpleNamespace(desktop=desktop,operation_lock=asyncio.Lock(),runtime=root,
                    bridge=SimpleNamespace(transport=SimpleNamespace(call=inspect),_status_snapshot=lambda:{}),
                    dispatch=AsyncMock(return_value={'result':json.dumps({'verified':True})}))
                editor=GradientEditor(app)
                async def prepare_window(_, owner):
                    self.assertEqual(owner,77)
                    warming.set()
                    return window, 99
                editor.prepare_window=AsyncMock(side_effect=prepare_window)
                editor.start_window=AsyncMock()
                released=asyncio.Event()
                committed=asyncio.Event()
                async def terminal():
                    await released.wait()
                    return verified
                async def commit(*_):committed.set()
                if live:
                    app._mask_preview=SimpleNamespace(epoch=9)
                    app._mask_preview_task=asyncio.create_task(terminal())
                args={'menu_epoch':9,'params':{'channel':'stroke','draft':adjusted,'preview':live}}
                with (patch('backend.gradient_editor.require_available'),
                      patch('backend.gradient_editor.dispatch_preview',side_effect=commit) as preview):
                    task=asyncio.create_task(editor.open(args))
                    if live:
                        await asyncio.wait_for(committed.wait(),2)
                        inspect.assert_not_called()
                        editor.prepare_window.assert_not_called()
                        editor.start_window.assert_not_called()
                        released.set()
                        preview.assert_awaited_once_with(app,'mask.preview.commit',{
                            'menu_epoch':9,'params':adjusted,'return_to_menu':True})
                        app.dispatch.assert_not_called()
                    if not verified:
                        with self.assertRaisesRegex(RuntimeError,'结果未确认'):await task
                        inspect.assert_not_called()
                        editor.start_window.assert_not_called()
                        continue
                    await task
                if not live:
                    app.dispatch.assert_awaited_once_with('action.execute',{
                        'name':'shape.paint','menu_epoch':9,'params':adjusted,'return_to_menu':True})
                launcher, epoch, warmup=editor.start_window.await_args.args
                self.assertEqual((launcher, epoch),(desktop,9))
                self.assertEqual(warmup.result(),(window,99))
                window.stdin.close.assert_called_once()
                self.assertEqual((editor.channel,editor.draft['stroke']['stops'][0]['rgb']),('stroke',[190,25,65]))

    async def test_overlay_reuses_hidden_window_and_disposes_it_when_disabled(self):
        editor = GradientEditor(None)
        pending = SimpleNamespace(stdin=Mock(), returncode=None)
        replacement = SimpleNamespace(stdin=Mock(), returncode=None)
        desktop = SimpleNamespace(settings={'gradient_direct_overlay':True}, hwnd=3, visible=True,
                                  gui=SimpleNamespace(GetForegroundWindow=lambda:11), _workspace_root=lambda hwnd:hwnd)
        editor.window_key = lambda desktop, owner:(owner, desktop.hwnd)
        editor.prepare_window = AsyncMock(side_effect=[(pending, 21), (replacement, 22)])
        editor.prewarm(desktop)
        self.assertIsNone(editor.standby)
        editor.prewarm(desktop, 11)
        standby = editor.standby[1]
        await standby
        desktop.visible = False
        editor.prewarm(desktop)
        self.assertIs(editor.standby[1], standby)
        editor.owner = 11
        async with editor.warming_window(desktop,editor.owner) as warmup:
            self.assertIs(warmup, standby)
        pending.stdin.close.assert_called_once_with()
        editor.prewarm(desktop)
        standby = editor.standby[1]
        desktop.settings['gradient_direct_overlay'] = False
        editor.prewarm(desktop)
        await standby
        await asyncio.sleep(0)
        replacement.stdin.close.assert_called_once_with()

    async def test_focus_loss_keeps_window_after_verified_rollback(self):
        editor=GradientEditor(None)
        editor.hwnd=1
        editor.phase='preview'
        result={'result':json.dumps({'verified':True,'cancelled':True,'rollback_verified':True})}
        await editor.finished(result)
        self.assertEqual(editor.phase,'editing')
        editor.phase='closing'
        await editor.finished(result)
        self.assertEqual(editor.phase,'closed')

    async def test_preview_cancelled_before_dispatch_keeps_editor_and_draft(self):
        from backend.mask_preview import MaskPreview, run_preview
        editor = GradientEditor(None)
        preview = editor.preview = MaskPreview(7, deepcopy(BASE))
        editor.hwnd, editor.phase = 1, 'preview'
        editor.require_focus = Mock()
        editor.in_scope = Mock(return_value=True)
        editor.require_target = Mock()
        desktop = SimpleNamespace(menu_epoch=7, mask_preview=preview, event=Mock())

        async def execute(*_, before_dispatch, **__):
            preview.cancel()
            await before_dispatch({})
            self.fail('Cancelled gradient reached the host write')

        lock = asyncio.Lock()
        await lock.acquire()
        app = SimpleNamespace(bridge=SimpleNamespace(execute_shape=execute, busy=False), desktop=desktop,
                              operation_lock=lock, broadcast=AsyncMock(), _mask_preview=preview)
        await run_preview(app, preview, {}, editor)
        self.assertEqual(editor.phase, 'editing')
        self.assertFalse(editor.refresh_revision)
        self.assertIsNone(editor.preview)
        self.assertFalse(lock.locked())
