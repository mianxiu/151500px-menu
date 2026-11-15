import asyncio
import json
from copy import deepcopy
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock, Mock, patch

from backend.gradient_acceptance import MAKE_GRADIENT
from backend.gradient_editor import GradientEditor, VIEW_SCRIPT, checked_draft, checked_view, draft_path, load_draft

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


class GradientWindowLifecycleTests(unittest.IsolatedAsyncioTestCase):
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

    async def test_explicit_mode_choice_survives_new_editor_and_view_failure(self):
        with tempfile.TemporaryDirectory() as root:
            app = SimpleNamespace(runtime=root, desktop=SimpleNamespace(sync_native=Mock()))
            async def run_window(*events):
                stream = AsyncMock(side_effect=[json.dumps(e).encode() for e in events] + [b''])
                process = SimpleNamespace(stdout=SimpleNamespace(readline=stream), stdin=Mock())
                editor = GradientEditor(app)
                editor.process = process
                editor.request_view = AsyncMock()  # A failed view leaves the window independent.
                await editor.read_window(process)
                return editor

            self.assertFalse(GradientEditor(app).preferred_overlay())
            view = {'type':'gradient.view','serial':1,'refresh':False,'entering':True}
            editor = await run_window({'type':'gradient.preference','overlay':True}, view)
            editor.request_view.assert_awaited_once_with(view)
            self.assertTrue(GradientEditor(app).preferred_overlay())
            await run_window({'type':'gradient.preference','overlay':False})
            self.assertFalse(GradientEditor(app).preferred_overlay())
            self.assertEqual(json.loads((Path(root)/'gradient-mode.json').read_text('utf-8')), {'overlay':False})

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
        editor.preview.update.assert_called_once_with({**value,'revision':'fresh:2'}, commit=False)

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

    async def test_temporary_hand_tool_keeps_overlay_but_settled_tool_change_closes_it(self):
        editor = GradientEditor(SimpleNamespace(operation_lock=asyncio.Lock()))
        editor.phase = 'editing'
        editor.require_focus = Mock()
        editor.send = AsyncMock()
        editor.request = AsyncMock()
        editor.read_view = AsyncMock(side_effect=[
            {'tool':'moveTool'}, {'tool':'moveTool'}, {'tool':'cropTool'},
        ])
        editor.resume_preview_after_view = False
        with patch('backend.gradient_editor.require_available'):
            await editor.request_view({'serial':1,'refresh':False,'entering':True})
            await editor.request_view({'serial':2,'refresh':True,'tracking':True})
            self.assertEqual(editor.send.await_args.args[0],{'type':'gradient.view','serial':2,'tracking':True})
            self.assertEqual(editor.read_view.await_count,1)
            await editor.request_view({'serial':3,'refresh':True})
            self.assertEqual(editor.send.await_args.args[0]['view']['tool'],'moveTool')
            editor.resume_preview_after_view = True
            await editor.request_view({'serial':4,'refresh':True})
        self.assertEqual(editor.send.await_args.args[0],{'type':'gradient.view','serial':4,'tool_changed':True})
        editor.request.assert_not_awaited()
        self.assertFalse(editor.resume_preview_after_view)

    async def test_view_motion_waits_for_settled_view_before_resuming_preview(self):
        app = SimpleNamespace(operation_lock=asyncio.Lock())
        editor = GradientEditor(app)
        editor.phase = 'preview'
        preview = editor.preview = Mock()
        editor.base, editor.draft = {}, {'edited': True}
        editor.require_focus = Mock()
        editor.send = AsyncMock()
        editor.request = AsyncMock()
        editor.read_view = AsyncMock(return_value={'matrix':[1,0,0,1,4,6]})

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
        editor.request.assert_awaited_once_with('gradient.update', {'draft':editor.draft})
        self.assertEqual(editor.send.await_args.args[0]['view']['matrix'],[1,0,0,1,4,6])

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
                inspect=Mock(return_value={'gradient_target':target,'shape_editor':shape})
                desktop=SimpleNamespace(visible=True,menu_epoch=9,target=77,color_editor_active=True,
                    menu_layer_context={**target,'shape_editor':source},panel_context=SimpleNamespace(current={}),
                    preview_input_guard=None,require_action_focus=Mock(),hide_if_epoch=Mock())
                app=SimpleNamespace(desktop=desktop,operation_lock=asyncio.Lock(),runtime=root,
                    bridge=SimpleNamespace(transport=SimpleNamespace(call=inspect),_status_snapshot=lambda:{}),
                    dispatch=AsyncMock(return_value={'result':json.dumps({'verified':True})}))
                editor=GradientEditor(app)
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
                editor.start_window.assert_awaited_once_with(desktop,9)
                self.assertEqual((editor.channel,editor.draft['stroke']['stops'][0]['rgb']),('stroke',[190,25,65]))

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
