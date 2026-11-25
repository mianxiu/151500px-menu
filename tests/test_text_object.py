import copy
from io import BytesIO
import unittest
import asyncio
import hashlib,json,subprocess
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock,AsyncMock,patch
from xml.dom import minidom
from backend import text_object,text_list
from tests.test_text_list import FOREIGN,OPTIONS

SOURCE={'schema':1,'canvas':{'width':600,'height':400,'resolution':300,'mode':'RGB','depth':8,'profile':'sRGB IEC61966-2.1'},
        'layer':{'name':'中文列表','xmp':'','text':{'_obj':'textLayer','textKey':'中😀文\rItem','textStyleRange':[{'from':0,'to':10,'textStyle':{'fontPostScriptName':'MicrosoftYaHei','size':{'_unit':'pointsUnit','_value':14},'tracking':20}}],
                 'paragraphStyleRange':[{'from':0,'to':10,'paragraphStyle':{'startIndent':{'_unit':'pointsUnit','_value':12},'futureProperty':True}}]}}}


class TextObjectTests(unittest.TestCase):
    def test_pdf_crop_changes_page_bounds_without_changing_vector_content(self):
        from pypdf import PdfReader, PdfWriter
        from pypdf.generic import DecodedStreamObject, NameObject
        from backend.text_object_native import crop_pdf
        writer=PdfWriter()
        page=writer.add_blank_page(width=600,height=400)
        stream=DecodedStreamObject()
        paint=b'q 0 0 0 rg 40 30 10 14 re f Q'
        stream.set_data(paint)
        page[NameObject('/Contents')]=stream
        original=BytesIO()
        writer.write(original)
        canvas={'width':600,'height':400,'resolution':72}
        result=PdfReader(BytesIO(crop_pdf(original.getvalue(),[32,330,75,379],canvas))).pages[0]
        self.assertEqual(list(result.mediabox),[0,0,43,49])
        self.assertEqual(list(result.cropbox),[0,0,43,49])
        commands=result.get_contents().operations
        self.assertIn(([1,0,0,1,-32,-21],b'cm'),commands)
        self.assertIn(([40,30,10,14],b're'),commands)

    def test_translated_double_size_point_and_paragraph_frames(self):
        from backend.text_tools import object_text_geometry
        canvas={'width':600,'height':400,'resolution':300}
        frame=[100,120,1300,120,1300,920,100,920]
        doc=[1600,1200]
        style={'size':'12','color':'#AA20FF'}
        raw={'textClickPoint':{'horizontal':{'_unit':'percentUnit','_value':25},
                               'vertical':{'_unit':'percentUnit','_value':50}},
             'textShape':[{'char':{'_value':'paint'}}]}
        size,point=object_text_geometry(raw,canvas,frame,doc,style)
        self.assertIsNone(size)
        self.assertFalse(point['wrap'])
        self.assertAlmostEqual(point['box'][0]*doc[0],400)
        self.assertAlmostEqual(point['box'][1]*doc[1],420)
        self.assertAlmostEqual(point['em']*doc[1],100)
        self.assertEqual(point['color'],[170,32,255])
        raw['textShape'][0]={'char':{'_value':'box'},'bounds':{'left':10,'top':20,'right':210,'bottom':100}}
        size,paragraph=object_text_geometry(raw,canvas,frame,doc,style)
        self.assertTrue(paragraph['wrap'])
        self.assertEqual(size,[200,80])
        self.assertAlmostEqual(paragraph['box'][0]*doc[0],420)
        self.assertAlmostEqual((paragraph['box'][2]-paragraph['box'][0])*doc[0],400)
        self.assertAlmostEqual(paragraph['em']*doc[1],100)
        crop=[120,160,360,320]
        cropped_frame=[340,440,820,440,820,760,340,760]
        self.assertEqual(object_text_geometry(raw,canvas,cropped_frame,doc,style,crop),(size,paragraph))
        self.assertIsNone(object_text_geometry(raw,canvas,[100,120,1300,121,1300,920,100,920],doc,style))

    def test_render_bounds_include_origin_and_reject_invalid_pages(self):
        raw=text_object.write('',SOURCE)
        doc=minidom.parseString(raw)
        node=doc.createElementNS(text_object.NAMESPACE,'pxText:pageBounds')
        doc.getElementsByTagNameNS(text_list.RDF,'Description')[0].appendChild(node)
        node.setAttribute('xmlns:pxText',text_object.NAMESPACE)
        self.assertIsNone(text_object.render_bounds(raw,SOURCE['canvas']))
        node.appendChild(doc.createTextNode('[12,25,400,380]'))
        self.assertEqual(text_object.render_bounds(doc.toxml(),SOURCE['canvas']),[12,25,400,380])
        for value in ('[240,0,200,9]','[0,0,601,10]','[0,0,100,2.5]','[true,0,100,30]'):
            node.firstChild.data=value
            with self.subTest(value=value),self.assertRaises(ValueError):
                text_object.render_bounds(doc.toxml(),SOURCE['canvas'])

    def test_resolved_font_metadata_validates_utf16_ranges(self):
        raw=text_object.write('',SOURCE);doc=minidom.parseString(raw)
        node=doc.createElementNS(text_object.NAMESPACE,'pxText:fontFallbacks')
        node.setAttribute('xmlns:pxText',text_object.NAMESPACE)
        doc.getElementsByTagNameNS(text_list.RDF,'Description')[0].appendChild(node)
        text=SOURCE['layer']['text']['textKey']
        self.assertEqual(text_object.font_fallbacks(raw,text),[])
        node.appendChild(doc.createTextNode('[[1,3,"MicrosoftYaHei"]]'))
        self.assertEqual(text_object.font_fallbacks(doc.toxml(),text),[[1,3,'MicrosoftYaHei']])
        for value in ('[[1,999,"Wrong"]]','[[1,3,"Alpha"],[2,4,"Beta"]]','[[0,2,true]]'):
            node.firstChild.data=value
            with self.subTest(value=value),self.assertRaises(ValueError):text_object.font_fallbacks(doc.toxml(),text)

    def test_table_source_round_trip_and_cell_bounds(self):
        model={'schema':3,'canvas':SOURCE['canvas'],'table':[['项目','单价'],['A&B','']]}
        self.assertEqual(text_object.read(text_object.write('',model)),model)
        for cells in ([],[[]],[['a'],[]],[['x']*9],[['']]*13,[['a\nb']],[['a'*129]],[[False]]):
            with self.subTest(cells=cells), self.assertRaises(ValueError):
                text_object.source({**model,'table':cells})

    def test_single_packet_roundtrip_preserves_full_styles_list_data_and_foreign_metadata(self):
        value=copy.deepcopy(SOURCE);value['layer']['xmp']=text_list.write(FOREIGN,OPTIONS)
        raw=text_object.write(FOREIGN,value);self.assertEqual(text_object.read(raw),value)
        old=minidom.parseString(FOREIGN).getElementsByTagNameNS(text_list.RDF,'Description')[0].toxml()
        self.assertEqual(minidom.parseString(raw).getElementsByTagNameNS(text_list.RDF,'Description')[0].toxml(),old)
        value['layer']['text']['textKey']='新😀文\rItem';self.assertEqual(text_object.read(text_object.write(raw,value)),value)

    def test_corrupt_unsupported_unsafe_and_oversized_sources_are_rejected(self):
        raw=text_object.write('',SOURCE)
        for bad in ('',raw.replace('MicrosoftYaHei','ArialMT'),raw.replace('&quot;schema&quot;:1','&quot;schema&quot;:2'),raw+'x','<!DOCTYPE x [<!ENTITY e "x">]>'+raw):
            with self.subTest(raw=bad[:30]):
                with self.assertRaises(ValueError):text_object.read(bad)
        for key,value in [('schema',True),('canvas',{}),('layer',{})]:
            changed=copy.deepcopy(SOURCE);changed[key]=value
            with self.assertRaises(ValueError):text_object.write('',changed)
        for key,value in [('__proto__',{}),('_path','C:/private'),('size',float('nan')),('large','a'*text_object.MAX_SOURCE)]:
            changed=copy.deepcopy(SOURCE);changed['layer']['text'][key]=value
            with self.assertRaises(ValueError):text_object.write('',changed)

    def test_utf16_range_boundaries_and_duplicate_packets(self):
        raw=text_object.write('',SOURCE);doc=minidom.parseString(raw)
        root=doc.getElementsByTagNameNS(text_list.RDF,'RDF')[0];root.appendChild(root.firstChild.cloneNode(True))
        with self.assertRaises(ValueError):text_object.read(doc.toxml())
        changed=copy.deepcopy(SOURCE);changed['layer']['text']['textStyleRange'][0]['to']=999
        with self.assertRaises(ValueError):text_object.source(changed)

    def test_embedded_vector_checksum_matches_standard_sha256_for_binary_and_padding_boundaries(self):
        root=Path(__file__).resolve().parents[1];code=(root/'jsx/text_object_sha256.jsx').read_text('utf-8')
        samples=[b'',b'abc',b'a'*55,b'a'*56,bytes(range(256))*5]
        runner=code+';const a=JSON.parse(require("fs").readFileSync(0,"utf8"));process.stdout.write(JSON.stringify(a.map(v=>sha256(String.fromCharCode(...v)))));'
        actual=json.loads(subprocess.run(['node','-e',runner],input=json.dumps([list(v) for v in samples]),text=True,capture_output=True,check=True).stdout)
        self.assertEqual(actual,[hashlib.sha256(v).hexdigest() for v in samples])


class TextObjectFlowTests(unittest.IsolatedAsyncioTestCase):
    async def test_object_read_is_private_and_write_uses_one_reserved_native_task(self):
        from backend.text_tools import TextTools
        from backend.text_plan import compute
        target={'session':'s','document_id':1,'active_layer_id':2,'layer_ids':[2]}
        xml=text_object.write('',SOURCE).replace('</rdf:Description>','<pxText:render xmlns:pxText="'+text_object.NAMESPACE+'">'+'a'*64+'</pxText:render></rdf:Description>')
        app=SimpleNamespace(operation_lock=asyncio.Lock(),desktop=SimpleNamespace(focus_search_result=Mock()),broadcast=AsyncMock(),
            bridge=SimpleNamespace(transport=SimpleNamespace(call=Mock(return_value={'session':'s','text':{'target':target,'revision':'r','name':'Object','object_xmp':xml}})),execute_text_object=AsyncMock(return_value={'message':'done'}),execute_text=AsyncMock(),busy=False,layer_context={}))
        tools=TextTools(app);tools.available=Mock();public=await tools.read({})
        self.assertTrue(public['vector_object']);self.assertEqual(public['text'],SOURCE['layer']['text']['textKey'])
        for key in ('object_source','object_xmp','list_xmp','object_render'):self.assertNotIn(key,public)
        with patch('backend.text_tools.evaluate',side_effect=compute):
            await tools.write({'revision':'r','draft':public['text']+'!'})
        app.bridge.execute_text_object.assert_awaited_once();app.bridge.execute_text.assert_not_called()
        self.assertEqual(app.bridge.execute_text_object.call_args.args[1]['object_source'],SOURCE)
        with self.assertRaises(ValueError):await tools.write({'revision':'r','draft':'again'})

    async def test_generate_consumes_only_current_replace_preview_and_preserves_menu_guard(self):
        from backend.text_tools import TextTools
        from backend.text_plan import compute
        target={'session':'s','document_id':1,'active_layer_id':2,'layer_ids':[2]}
        app=SimpleNamespace(operation_lock=asyncio.Lock(),desktop=SimpleNamespace(focus_search_result=Mock()),broadcast=AsyncMock(),bridge=SimpleNamespace(execute_text_object=AsyncMock(return_value={'message':'done'}),busy=False,layer_context={}))
        tools=TextTools(app);tools.available=Mock();tools.snapshot={'target':target,'revision':'r','text':'one'}
        with patch('backend.text_tools.evaluate',side_effect=compute):preview=await tools.preview({'revision':'r','draft':'two','operation':'edit'})
        with self.assertRaises(ValueError):await tools.vector({'revision':'r','plan_id':'stale'})
        guard=AsyncMock()
        with patch('backend.text_object_render.edge_path',side_effect=AssertionError('Native text must not require Edge')):
            await tools.vector({'revision':'r','plan_id':preview['plan_id']},menu_guard=guard)
        guard.assert_awaited_once();app.desktop.focus_search_result.assert_called_once_with(keep_menu=True)
        app.bridge.execute_text_object.assert_awaited_once();self.assertEqual(app.bridge.execute_text_object.call_args.args[1]['value'],'two')
