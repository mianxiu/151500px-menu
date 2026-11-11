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
from backend.gradient_editor import GradientEditor, checked_draft, draft_path, load_draft

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
                             input=json.dumps(['(function(){'+MAKE_GRADIENT+'})()',legacy]),capture_output=True,text=True,timeout=10,creationflags=0x08000000)
        self.assertEqual(check.returncode,0,check.stderr)


class GradientWindowLifecycleTests(unittest.IsolatedAsyncioTestCase):
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
