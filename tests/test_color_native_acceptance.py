import ast
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from backend.text_object_acceptance import compile_rejection

ROOT=Path(__file__).resolve().parents[1]

class NativeColorAcceptanceTests(unittest.TestCase):
    def test_native_history_reads_inactive_document_and_restores_active(self):
        from backend.color_native_acceptance import HISTORY
        script="""const assert=require('node:assert/strict');let state={name:'original'};
const original={historyStates:[state],get activeHistoryState(){if(app.activeDocument!==this)throw Error('not frontmost');return state;}};
const owned={},app={activeDocument:owned};
"""+HISTORY+"""
assert.deepEqual(history(original),{index:0,name:'original',count:1});assert.equal(app.activeDocument,owned);
original.historyStates=[];assert.throws(()=>history(original));assert.equal(app.activeDocument,owned);
"""
        result=subprocess.run(['node','-e',script],capture_output=True,text=True,timeout=10,creationflags=0x08000000)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_fixed_extendscript_fragments_parse_before_host_execution(self):
        source=(ROOT/'backend/color_native_acceptance.py').read_text('utf-8')
        fragments=[n.value for n in ast.walk(ast.parse(source)) if isinstance(n,ast.Constant) and isinstance(n.value,str) and n.value.count('\n')>1]
        result=subprocess.run(['node','-e',"const vm=require('node:vm');for(const [i,s] of JSON.parse(require('node:fs').readFileSync(0,'utf8')).entries()){try{new vm.Script('(function(){'+s+'})()');}catch(e){throw Error('fragment '+i+': '+e.message);}}"],
            input=json.dumps(fragments),capture_output=True,text=True,timeout=10,creationflags=0x08000000)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_only_authenticated_top_level_parser_rejection_proves_not_started(self):
        identity='a'*32
        script='(function(){try{return encode({id:'+json.dumps(identity)+',ok:true});}catch(e){return encode({id:'+json.dumps(identity)+',ok:false});}})()'
        record={'id':identity,'phase':'unknown','script':script,'sha256':hashlib.sha256(script.encode()).hexdigest(),
                'error':"com_error Adobe Photoshop Error 25: Expected: ). Line: 22"}
        self.assertEqual(compile_rejection(record)['phase'],'not-started')
        for patch in ({'error':'COM call timed out'},{'error':'Adobe Photoshop Error 1: unavailable'},
                      {'id':'b'*32},{'sha256':'0'*64},{'phase':'submitted'}):
            self.assertIsNone(compile_rejection({**record,**patch}))

    def test_compile_rejection_archives_unknown_before_persisting_not_started(self):
        from backend.stability import StabilityTest
        identity='a'*32
        script='(function(){try{return encode({id:'+json.dumps(identity)+',ok:true});}catch(e){return encode({id:'+json.dumps(identity)+',ok:false});}})()'
        record={'id':identity,'phase':'unknown','script':script,'sha256':hashlib.sha256(script.encode()).hexdigest(),
                'error':'Adobe Photoshop Error 25: Expected: ).'}
        original={'schema':1,'run_id':'b'*32,'phase':'failed','finished_at':1,'native_request':record,'native_requests':[record], 'original_request_pending':True}
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory)/'stability-reports';folder.mkdir()
            (folder/'latest.json').write_text(json.dumps(original),'utf-8')
            state=StabilityTest(SimpleNamespace(runtime=directory))
            archived=json.loads(Path(state.report['native_compile_rejection_archive']).read_text('utf-8'))
            self.assertEqual(archived,original)
            self.assertEqual(state.report['phase'],'failed')
            self.assertEqual(state.report['native_request']['phase'],'not-started')
            self.assertEqual(json.loads((folder/'latest.json').read_text())['native_request']['phase'],'not-started')
