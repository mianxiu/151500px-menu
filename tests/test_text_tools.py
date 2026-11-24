import asyncio
import unittest
from types import SimpleNamespace
from unittest.mock import Mock,AsyncMock,patch
from backend.text_tools import TextTools,replace_parameters,split_parameters,typography


class TextToolsTests(unittest.IsolatedAsyncioTestCase):
    async def test_extended_styles_require_capability_and_preserve_plan(self):
        from backend.text_plan import compute
        tools=TextTools(SimpleNamespace());tools.available=Mock()
        tools.snapshot={'revision':'r','text':'abc','typography_supported':True,'style_scope_supported':True,
                        'paragraph_alignment_supported':True}
        fields={'align':'center','baselineShift':-2.5,'horizontalScale':80,'verticalScale':120,'leading':'auto'}
        args={'revision':'r','operation':'edit','draft':'abc','typography':{'range':[0,1],**fields}}
        with patch('backend.text_tools.evaluate',side_effect=compute):
            with self.assertRaisesRegex(ValueError,'更新插件'):await tools.preview(args)
            tools.snapshot['extended_typography']=True
            await tools.preview(args)
            self.assertEqual(tools.plan[1]['typography'],args['typography'])
        for invalid in ({'align':'other'},{'horizontalScale':0},{'verticalScale':1001},{'baselineShift':-1297}):
            with self.assertRaises(ValueError):typography(invalid)

    async def test_paragraph_indents_are_capability_and_list_bounded(self):
        from backend.text_list import write
        from backend.text_plan import compute
        from tests.test_text_list import OPTIONS
        tools=TextTools(SimpleNamespace());tools.available=Mock()
        tools.snapshot={'revision':'r','text':'abc','typography_supported':True,'style_scope_supported':True}
        args={'operation':'edit','revision':'r','draft':'abc','typography':{'range':[0,2],'startIndent':12,'endIndent':9,'firstLineIndent':'-3.5'}}
        with patch('backend.text_tools.evaluate',side_effect=compute):
            with self.assertRaisesRegex(ValueError,'支持段落缩进'):await tools.preview(args)
            tools.snapshot['paragraph_indents_supported']=True
            await tools.preview(args)
            self.assertEqual(tools.plan[1]['typography'],{**args['typography'],'firstLineIndent':-3.5})
            tools.snapshot['list_xmp']=write('',OPTIONS)
            with self.assertRaisesRegex(ValueError,'列表使用自身的段落对齐、缩进与断字设置'):await tools.preview(args)
            await tools.preview({**args,'typography':{'endIndent':5}})
            self.assertEqual(tools.plan[1]['typography'],{'endIndent':5})
        for values in ({'startIndent':-1},{'endIndent':-1},{'firstLineIndent':-1297},{'endIndent':'nan'}):
            with self.assertRaises(ValueError):typography(values)

    def test_advanced_typography_accepts_only_supported_descriptor_values(self):
        style={'autoKern':'opticalKern','color':[0,130,255],'align':'justifyLeft','hyphenate':False,
               'baseline':'superScript','fontCaps':'smallCaps','underline':'underlineOnLeftInVertical'}
        self.assertEqual(typography(style),style)
        for invalid in ({'color':[0,256,0]},{'color':[True,0,0]},{'autoKern':'manualKern'},
                        {'hyphenate':1},{'align':'justifyOther'}):
            with self.assertRaises(ValueError):typography(invalid)

    async def test_style_only_kerning_and_opentype_plan_keeps_the_same_origins(self):
        tools=TextTools(SimpleNamespace());tools.available=Mock()
        tools.snapshot={'revision':'r','text':'AV','typography_supported':True,
                        'advanced_typography':True,'opentype_supported':True,'kerning_supported':True}
        args={'operation':'edit','revision':'r','draft':'AV','kerning':[[1,0]],
              'typography':{'fractions':True,'figureStyle':'tabularOldStyle'}}
        with patch('backend.text_tools.evaluate') as worker:
            await tools.preview(args)
            worker.assert_not_called()
        self.assertEqual(tools.plan[1]['origins'],[0,1,2])
        self.assertEqual(replace_parameters(tools.plan[1])['kerning'],[[1,0]])
        for invalid in ([[1,True]],[[0,0]],[[1,0],[1,None]],[[1,1001]]):
            with self.assertRaises(ValueError):await tools.preview({**args,'kerning':invalid})

    async def test_processed_result_keeps_scoped_styles_between_replacements(self):
        from backend.text_plan import compute
        tools=TextTools(SimpleNamespace());tools.available=Mock()
        tools.snapshot={'revision':'r','text':'x😀BxC','typography_supported':True,
                        'typography_runs_supported':True,'style_scope_supported':True}
        args={'operation':'replace','revision':'r','draft':'x😀BxC','find':'x','replacement':'ZZ','regex':False,'ignore_case':False,
              'typography_runs':[{'range':[1,4],'size':18},{'range':[5,7],'tracking':52}]}
        with patch('backend.text_tools.evaluate',side_effect=compute):
            result=await tools.preview(args)
            self.assertEqual(result['value'],'ZZ😀BZZC')
            self.assertEqual(result['draft_origins'],[0,0,1,2,3,4,4,5,6])
            self.assertEqual(tools.plan[1]['typography_runs'],[
                {'range':[2,5],'size':18},{'range':[7,9],'tracking':52}])
            await tools.preview({**args,'replacement':'','typography_runs':[{'range':[0,1],'size':18}]})
            self.assertNotIn('typography_runs',tools.plan[1])
            with self.assertRaisesRegex(ValueError,'排版选区已改变'):
                await tools.preview({**args,'typography_runs':[{'range':[2,4],'size':18}]})
            self.assertIsNone(tools.plan)

    async def test_hovered_font_plan_cannot_be_written_or_converted(self):
        tools=TextTools(SimpleNamespace());tools.available=Mock()
        tools.snapshot={'revision':'r','text':'AV','typography_supported':True,'proof_available':True}
        draft={'operation':'edit','revision':'r','draft':'AV','typography':{'fontPostScriptName':'Arial-BoldMT'}}
        preview=await tools.preview({**draft,'font_preview':True})
        target={'revision':'r','plan_id':preview['plan_id']}
        with self.assertRaises(ValueError):await tools.apply(target)
        with self.assertRaises(ValueError):await tools.vector(target)
        selected=await tools.preview(draft)
        self.assertNotEqual(preview['plan_id'],selected['plan_id'])
        self.assertIsNone(tools.transient_plan)

    async def test_style_plan_survives_write_validation(self):
        from backend.text_plan import compute
        from backend.text_tools import style_summary
        app=SimpleNamespace(operation_lock=asyncio.Lock(),desktop=SimpleNamespace(focus_search_result=Mock()),
            bridge=SimpleNamespace(execute_text=AsyncMock(return_value={}),busy=False,layer_context={}),broadcast=AsyncMock())
        tools=TextTools(app);tools.available=Mock()
        tools.snapshot={'revision':'r','text':'ab','target':{},'typography_supported':True}
        raw={'textStyleRange':[{'from':0,'to':1,'textStyle':{'size':{'_unit':'pixelsUnit','_value':50},'fontPostScriptName':'Alpha'}},
                               {'from':1,'to':2,'textStyle':{'size':{'_unit':'pointsUnit','_value':12},'fontPostScriptName':'Beta'}}],
             'paragraphStyleRange':[{'from':0,'to':2,'paragraphStyle':{}}]}
        summary=style_summary(raw,300)
        self.assertEqual(summary['size'],'12');self.assertEqual(summary['fontPostScriptName'],'混合')
        with patch('backend.text_tools.evaluate',side_effect=compute):
            args={'operation':'edit','revision':'r','draft':'ab','typography':{'tracking':'-15'}}
            with self.assertRaises(ValueError):await tools.preview({**args,'typography':{'size':'nan'}})
            plan=await tools.preview(args)
            await tools.apply({'revision':'r','plan_id':plan['plan_id']})
        self.assertEqual(replace_parameters(app.bridge.execute_text.call_args.args[1])['typography'],{'tracking':-15})

    async def test_list_plan_is_private_persistent_and_derived_from_current_read(self):
        from backend.text_list import read
        from backend.text_plan import compute
        from tests.test_text_list import OPTIONS,FOREIGN
        app=SimpleNamespace(operation_lock=asyncio.Lock(),desktop=SimpleNamespace(focus_search_result=Mock()),
            bridge=SimpleNamespace(execute_text=AsyncMock(return_value={}),busy=False,layer_context={}),broadcast=AsyncMock())
        tools=TextTools(app);tools.available=Mock();target={'session':'s','document_id':1,'layer_ids':[2],'active_layer_id':2}
        tools.snapshot={'revision':'r','text':'current PS text','target':target,'list_xmp':FOREIGN,'list_paragraphs':[{'from':0,'to':16,'start':12,'first':3}]}
        with patch('backend.text_tools.evaluate',side_effect=compute):
            result=await tools.preview({'revision':'r','draft':'current PS text','operation':'list',**OPTIONS})
        self.assertNotIn('list_xmp',result);app.bridge.execute_text.assert_not_called()
        await tools.apply({'revision':'r','plan_id':result['plan_id']})
        payload=app.bridge.execute_text.call_args.args[1]
        self.assertEqual(read(payload['list_xmp']),OPTIONS);self.assertNotIn('list_options',payload)
        self.assertEqual(replace_parameters(payload,allow_xmp=True),payload)
        with self.assertRaises(ValueError):replace_parameters(payload)
        tools.snapshot={'revision':'r','text':'current PS text','target':target,'list_xmp':FOREIGN,'list_paragraphs':[{'from':0,'to':16,'start':12,'first':3}]}
        with patch('backend.text_tools.evaluate',side_effect=compute):
            await tools.write({'revision':'r','draft':'continue editing','list_options':{**OPTIONS,'start':'4'}})
        carried=app.bridge.execute_text.call_args.args[1]
        self.assertEqual(carried['value'],'continue editing');self.assertEqual(read(carried['list_xmp']),{**OPTIONS,'start':4})
        tools.snapshot={'revision':'r','text':'current PS text','target':target}
        with patch('backend.text_tools.evaluate',side_effect=compute):
            with self.assertRaisesRegex(ValueError,'0.3.81'):await tools.preview({'revision':'r','draft':'new','operation':'list',**OPTIONS})
        self.assertIsNone(tools.plan)

    async def test_write_current_draft_is_independent_of_an_invalid_split_and_is_one_shot(self):
        from backend.text_plan import compute
        app=SimpleNamespace(operation_lock=asyncio.Lock(),desktop=SimpleNamespace(focus_search_result=Mock()),
            bridge=SimpleNamespace(execute_text=AsyncMock(return_value={'message':'done'}),busy=False,layer_context={}),broadcast=AsyncMock())
        tools=TextTools(app);tools.available=Mock();target={'session':'s','document_id':1,'layer_ids':[2],'active_layer_id':2}
        tools.snapshot={'revision':'r','text':'original','target':target};draft='one\ntwo\nthree\nfour'
        with patch('backend.text_tools.evaluate',side_effect=compute):
            with self.assertRaises(ValueError):await tools.preview({'revision':'r','draft':draft,'operation':'split','mode':'lines','groups':'1','separator':'','layout':'original'})
            self.assertIsNone(tools.plan);app.bridge.execute_text.assert_not_called()
            guard=AsyncMock();await tools.write({'revision':'r','draft':draft},menu_guard=guard)
            self.assertEqual(app.bridge.execute_text.call_args.args[2],'text.replace')
            self.assertEqual(app.bridge.execute_text.call_args.args[1]['value'],'one\rtwo\rthree\rfour')
            self.assertNotIn('breaks',app.bridge.execute_text.call_args.args[1])
            guard.assert_awaited_once();app.desktop.focus_search_result.assert_called_once_with(keep_menu=True)
            with self.assertRaises(ValueError):await tools.write({'revision':'r','draft':draft})
            app.bridge.execute_text.assert_awaited_once()

    async def test_direct_write_validates_draft_and_honors_the_menu_guard(self):
        from backend.text_plan import compute
        app=SimpleNamespace(operation_lock=asyncio.Lock(),desktop=SimpleNamespace(focus_search_result=Mock()),
            bridge=SimpleNamespace(execute_text=AsyncMock(),busy=False),broadcast=AsyncMock())
        tools=TextTools(app);tools.available=Mock();tools.snapshot={'revision':'r','text':'a','target':{}}
        with patch('backend.text_tools.evaluate',side_effect=compute):
            for args in ({'revision':'r','draft':''},{'revision':'r','draft':'x','operation':'split'},{'revision':'old','draft':'x'}):
                with self.assertRaises(ValueError):await tools.write(args)
            with self.assertRaisesRegex(RuntimeError,'closed'):await tools.write({'revision':'r','draft':'x'},menu_guard=AsyncMock(side_effect=RuntimeError('closed')))
        app.bridge.execute_text.assert_not_called();app.desktop.focus_search_result.assert_not_called();self.assertIsNone(tools.plan)

    async def test_preview_is_local_and_apply_consumes_exact_validated_plan_once(self):
        app=SimpleNamespace(operation_lock=asyncio.Lock(),desktop=SimpleNamespace(focus_search_result=Mock()),
            bridge=SimpleNamespace(execute_text=AsyncMock(return_value={'message':'done'}),busy=False,layer_context={}),broadcast=AsyncMock())
        tools=TextTools(app);tools.available=Mock();target={'session':'s','document_id':1,'layer_ids':[2],'active_layer_id':2}
        tools.snapshot={'revision':'r','text':'a','target':target}
        params={'revision':'r','draft':'猫','operation':'edit'}
        from backend.text_plan import compute
        with patch('backend.text_tools.evaluate',side_effect=compute):result=await tools.preview(params)
        app.bridge.execute_text.assert_not_called();self.assertEqual(result['preview'],'猫');self.assertEqual(result['matches'],[])
        with self.assertRaises(ValueError):await tools.apply({'revision':'r','plan_id':'old'})
        request={'revision':'r','plan_id':result['plan_id']}
        await tools.apply(request);app.bridge.execute_text.assert_awaited_once()
        self.assertEqual(app.bridge.execute_text.call_args.args[1]['value'],'猫')
        with self.assertRaises(ValueError):await tools.apply(request)

    async def test_failed_or_late_preview_cannot_leave_an_applicable_plan(self):
        tools=TextTools(SimpleNamespace());tools.available=Mock();saved={'revision':'r','text':'a'};tools.snapshot=saved
        tools.plan=('old',{},'text.replace')
        with patch('backend.text_tools.evaluate',side_effect=ValueError('bad regex')):
            with self.assertRaisesRegex(ValueError,'bad regex'):await tools.preview({'revision':'r','draft':'b','operation':'edit'})
        self.assertIsNone(tools.plan)
        def changed(*_):tools.snapshot=None;return {}
        with patch('backend.text_tools.evaluate',side_effect=changed):
            with self.assertRaisesRegex(ValueError,'已改变'):await tools.preview({'revision':'r','draft':'b','operation':'edit'})
        self.assertIsNone(tools.plan)

    def test_validated_replacement_is_bounded(self):
        params={'revision':'s:1','find':'猫','replacement':''}
        self.assertEqual(replace_parameters(params),params)
        for value in ({},[],{**params,'find':''},{**params,'find':'x'*201},{**params,'replacement':'x'*501},{**params,'extra':True}):
            with self.assertRaises(ValueError):replace_parameters(value)

    async def test_read_clears_stale_preview_and_releases_after_failure(self):
        app=SimpleNamespace(bridge=SimpleNamespace(transport=SimpleNamespace(call=Mock(side_effect=RuntimeError('failed')))))
        tools=TextTools(app);tools.available=Mock();tools.snapshot={'revision':'old'}
        with self.assertRaisesRegex(RuntimeError,'failed'):await tools.read({})
        self.assertIsNone(tools.snapshot);self.assertFalse(tools.reading)

    async def test_replace_binds_original_read_target_and_rejects_repeat(self):
        app=SimpleNamespace(operation_lock=asyncio.Lock(),desktop=SimpleNamespace(focus_search_result=Mock()),
            bridge=SimpleNamespace(execute_text=AsyncMock(return_value={'message':'done'}),busy=False,layer_context={}),broadcast=AsyncMock())
        tools=TextTools(app);tools.available=Mock();target={'session':'s','document_id':1,'layer_ids':[2],'active_layer_id':2}
        tools.snapshot={'revision':'s:1','target':target}
        args={'revision':'s:1','find':'a','replacement':'b'}
        await tools.replace(args);app.bridge.execute_text.assert_awaited_once_with(target,args,'text.replace')
        with self.assertRaises(ValueError):await tools.replace(args)
        self.assertFalse(app.operation_lock.locked());self.assertIsNone(tools.snapshot)

    async def test_proof_pins_history_without_focusing_or_consuming_plan(self):
        memory=Mock()
        bridge=SimpleNamespace(execute_text=AsyncMock(side_effect=[{'frame':(memory,{'history':72})},
            {'frame':(memory,{'history':72,'scale':4})},{}]),busy=False,layer_context={})
        app=SimpleNamespace(operation_lock=asyncio.Lock(),desktop=SimpleNamespace(focus_search_result=Mock()),bridge=bridge,broadcast=AsyncMock())
        tools=TextTools(app);tools.available=Mock();guard=AsyncMock()
        tools.snapshot={'revision':'r','text':'one','target':{},'proof_available':True,'proof_size':[600,440]}
        from backend.text_plan import compute
        with patch('backend.text_tools.evaluate',side_effect=compute):
            plan=await tools.preview({'revision':'r','draft':'two','operation':'edit'})
        args={'revision':'r','plan_id':plan['plan_id']}
        with self.assertRaises(ValueError):await tools.proof({**args,'plan_id':'stale'},guard)
        with self.assertRaisesRegex(ValueError,'原尺寸预览'):await tools.proof({**args,'scale':4},guard)
        self.assertEqual(await tools.proof(args,guard),(memory,{'history':72,'plan_id':args['plan_id']}))
        app.desktop.focus_search_result.assert_not_called()
        self.assertEqual(bridge.execute_text.await_args.args[2],'text.proof')
        params=bridge.execute_text.await_args.args[1]
        _,frame=await tools.proof({**args,'scale':4},guard)
        self.assertEqual(frame['scale'],4)
        bridge.execute_text.assert_awaited_with({},params,'text.proof',before_dispatch=guard,proof_history=72,proof_scale=4)
        with self.assertRaisesRegex(ValueError,'分辨率'):await tools.proof({**args,'scale':8},guard)
        await tools.apply(args,guard)
        bridge.execute_text.assert_awaited_with({},params,'text.replace',before_dispatch=guard,proof_history=72)

    async def test_unverified_target_never_submits_proof(self):
        tools=TextTools(SimpleNamespace());tools.available=Mock()
        tools.snapshot={'text':'one','revision':'r','target':{}}
        from backend.text_plan import compute
        with patch('backend.text_tools.evaluate',side_effect=compute):
            plan=await tools.preview({'revision':'r','draft':'two','operation':'edit'})
        with self.assertRaises(ValueError):
            await tools.proof({'revision':'r','plan_id':plan['plan_id']},AsyncMock())

    async def test_table_proof_binds_cells_before_zoom_and_commit(self):
        memory=Mock()
        bridge=SimpleNamespace(execute_text_object=AsyncMock(side_effect=[
            {'frame':(memory,{'history':72})},{'frame':(memory,{'history':72,'scale':4})},{}]),
            busy=False,layer_context={})
        app=SimpleNamespace(operation_lock=asyncio.Lock(),desktop=SimpleNamespace(focus_search_result=Mock()),
            bridge=bridge,broadcast=AsyncMock())
        tools=TextTools(app);tools.available=Mock();guard=AsyncMock()
        tools.snapshot={'revision':'r','text':'','target':{},'object_proof':True,'proof_size':[600,440],
                        'table':[['original']],'vector_object':True,'object_source':{},'object_render':'a'*64}
        draft={'revision':'r','cells':[['updated']]}
        self.assertEqual(await tools.proof(draft,guard),(memory,{'history':72,'table':True}))
        with self.assertRaisesRegex(ValueError,'原尺寸预览'):
            await tools.proof({'revision':'r','cells':[['changed']],'scale':4},guard)
        await tools.proof({**draft,'scale':4},guard)
        app.desktop.focus_search_result.assert_not_called()
        payload={**draft,'object_source':{},'object_render':'a'*64}
        bridge.execute_text_object.assert_awaited_with({},payload,before_dispatch=guard,proof_scale=4,proof_history=72)
        await tools.table(draft,guard)
        bridge.execute_text_object.assert_awaited_with({},payload,before_dispatch=guard,proof_history=72)

    def test_split_offsets_are_bounded_and_ordered(self):
        self.assertEqual(split_parameters({'revision':'s:1','breaks':[3,8]})['breaks'],[3,8])
        for ends in ([1],[True,3],[3,3],[3,2],[0,3],[3,20001],list(range(1,66))):
            with self.assertRaises(ValueError):split_parameters({'revision':'s:1','breaks':ends})

    async def test_read_rejects_wrong_session_before_exposing_preview(self):
        value={'session':'new','text':{'text':'a','name':'source','revision':'x','target':{'session':'old','document_id':1,'active_layer_id':2,'layer_ids':[2]}}}
        tools=TextTools(SimpleNamespace(bridge=SimpleNamespace(transport=SimpleNamespace(call=Mock(return_value=value)))));tools.available=Mock()
        with self.assertRaisesRegex(RuntimeError,'会话不一致'):await tools.read({})
        self.assertIsNone(tools.snapshot)
