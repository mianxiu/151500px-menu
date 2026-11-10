import copy
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
