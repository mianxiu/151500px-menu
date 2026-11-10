"""Fixed text-object acceptance inside the service COM apartment, never file-open scripts."""
import asyncio
import hashlib
import json
import re
import uuid
from . import text_object
from .paths import ROOT
from .text_plan import compute
from .settings import atomic_json

def compile_rejection(record):
    """A returned top-level parser error cannot have entered this fixed wrapper."""
    script=record.get('script','');identity=record.get('id','');error=record.get('error','')
    if (record.get('phase')!='unknown' or not isinstance(script,str) or not re.fullmatch('[a-f0-9]{32}',identity) or
            hashlib.sha256(script.encode()).hexdigest()!=record.get('sha256') or
            not script.startswith('(function(){') or not script.endswith(')()') or
            ('try{return encode({id:'+json.dumps(identity)+',ok:true') not in script or
            ('catch(e){return encode({id:'+json.dumps(identity)+',ok:false') not in script or
            re.search(r'\b(?:eval|Function)\s*\(',script) or
            not re.search(r'Adobe Photoshop.*Error 25: Expected:',error,re.S)):
        return None
    return {'phase':'not-started','previous_phase':'unknown',
        'execution_exit':{'id':identity,'sha256':record['sha256'],'engine':'photoshop-top-level-parser',
                          'not_started':True,'exited':True,'error':error}}


async def native(test,code):
    bridge=test.app.bridge
    source=(ROOT/'jsx/text_object.jsx').read_text('utf-8')
    encoder=source[source.index('    function encode(v)'):source.index('    function name(id)')]
    if test.report.get('cleanup_phase')=='running':
        test.focus_check()
        if test.stop_requested:raise RuntimeError('已停止尚未执行的原生收尾')
    else:test.check()
    if bridge.busy or bridge.transport.pending.value or bridge.transport.pending.error:
        raise RuntimeError('原请求仍待核对，未执行原生验收步骤')
    identity=uuid.uuid4().hex
    script='(function(){'+encoder+'try{return encode({id:'+json.dumps(identity)+',ok:true,result:(function(){'+code+'})()});}catch(e){return encode({id:'+json.dumps(identity)+',ok:false,error:String(e),line:e.line||0});}})()'
    record={'id':identity,'phase':'submitted','script':script,'sha256':hashlib.sha256(script.encode()).hexdigest()}
    test.report['native_request']=record
    test.report.setdefault('native_requests',[]).append(record)
    try:await test.save()
    except Exception as error:
        record.update(phase='not-started',preflight_error=repr(error),execution_exit={'id':identity,'sha256':record['sha256'],'engine':'backend-preflight','not_started':True,'exited':True})
        try:
            await asyncio.to_thread(atomic_json,test.folder/(test.report['run_id']+'.json'),test.report)
            await asyncio.to_thread(atomic_json,test.folder/'latest.json',test.report)
        except Exception:
            record['phase']='unknown'
            raise
        raise
    try:
        value=await asyncio.get_running_loop().run_in_executor(bridge.executor,lambda:bridge._application().DoJavaScript(script))
        record['raw_response']=value
        value=json.loads(value)
        if value.get('id')!=identity:raise RuntimeError('原生验收返回身份不一致')
    except Exception as error:
        record.update(phase='unknown',error=repr(error))
        proof=compile_rejection(record)
        if proof:record.update(proof)
        await test.save()
        raise
    record.update(phase='returned',response=value)
    await test.save()
    if value.get('ok') is not True:raise RuntimeError(value.get('error','原生验收失败'))
    return value['result']


async def run(test):
    bridge=test.app.bridge
    async def read():
        test.check()
        return (await test.test_read(text_inspect=True))['text']

    async def write(snapshot,draft):
        model=text_object.read(snapshot['object_xmp']) if 'object_xmp' in snapshot else None
        original=model['layer']['text']['textKey'] if model else snapshot['text']
        plan=compute(original,{'revision':snapshot['revision'],'draft':draft,'operation':'edit'})
        params={k:plan[k] for k in ('value','origins','count')}
        params['revision']=snapshot['revision']
        if model:params.update(object_source=model,object_render=text_object.render_hash(snapshot['object_xmp']))
        await asyncio.to_thread(test.app.desktop.focus_search_result)
        value=await bridge.execute_text_object(snapshot['target'],params,before_dispatch=test.before_dispatch)
        if json.loads(value['result']).get('verified') is not True:raise RuntimeError('文字对象生成核验失败')
        return value

    for index,dpi in enumerate((72,300),1):
        test.report['round']=index
        name='151500px text acceptance '+test.report['run_id']+' '+str(dpi)
        test.report['text_fixture']={'creation_unconfirmed':True,'name':name}
        test.report['cleanup']='retained-text-object-fixture'
        await test.save()
        fixture=await test.step('text-object-fixture',lambda:native(test,'''
            var s=stringIDToTypeID,r=new ActionReference();r.putProperty(s('property'),s('modalToolLevel'));r.putEnumerated(s('application'),s('ordinal'),s('targetEnum'));
            if(executeActionGet(r).getInteger(s('modalToolLevel'))!==0)throw Error('Native modal edit active');
            var ids=[],original=app.documents.length?app.activeDocument.id:null,tool=app.currentTool;
            for(var i=0;i<app.documents.length;i++)ids.push(app.documents[i].id);
            var d=app.documents.add(UnitValue(1500,'px'),UnitValue(1200,'px'),'''+str(dpi)+','+json.dumps(name)+''',NewDocumentMode.RGB,DocumentFill.TRANSPARENT,1,BitsPerChannelType.EIGHT,'sRGB IEC61966-2.1');
            var l=d.artLayers.add();l.kind=LayerKind.TEXT;var t=l.textItem;t.font='MicrosoftYaHei';t.size=UnitValue(20,'pt');
            '''+("t.kind=TextType.PARAGRAPHTEXT;t.width=UnitValue(1000,'px');t.height=UnitValue(900,'px');" if dpi==300 else '')+'''
            t.position=[UnitValue(30,'px'),UnitValue(100,'px')];t.contents='Alpha old \\u4e2d\\u6587\\rSecond paragraph';
            app.currentTool='moveTool';app.bringToFront();return {document_id:d.id,layer_id:l.id,original:original,documents:ids,tool:tool};
        '''))
        test.report['text_fixture']=fixture
        test.report['cleanup']='retained-text-object-fixture'
        await test.save()
        owned='if(!app.documents.length||app.activeDocument.id!=='+str(fixture['document_id'])+'||app.activeDocument.name!=='+json.dumps(name)+")throw Error('Owned fixture target changed');var d=app.activeDocument;"
        snap=await read()
        if snap['target']['document_id']!=fixture['document_id']:raise RuntimeError('隔离文字目标不一致')
        await test.step('text-object-create',lambda:write(snap,snap['text']))
        created=await read();model=text_object.read(created['object_xmp'])
        if model['schema']!=1 or model['layer']['text']['textKey']!=snap['text']:raise RuntimeError('PS原生文字源不一致')
        original_id=created['target']['active_layer_id'];original_xmp=created['object_xmp']
        scaled=await test.step('text-object-scale',lambda:native(test,owned+"var l=d.activeLayer.duplicate();d.activeLayer=l;l.resize(125,125,AnchorPosition.MIDDLECENTER);l.translate(UnitValue(17,'px'),UnitValue(9,'px'));return {copy_id:l.id};"))
        copied=await read()
        if copied['target']['active_layer_id']!=scaled['copy_id']:raise RuntimeError('缩放副本选择不一致')
        await test.step('text-object-update',lambda:write(copied,snap['text'].replace('old','new')))
        updated=await read();updated_id=updated['target']['active_layer_id']
        if text_object.read(updated['object_xmp'])['layer']['text']['textKey']!=snap['text'].replace('old','new'):raise RuntimeError('更新正文不一致')
        path=test.folder/(test.report['run_id']+'-'+str(dpi)+'.psd')
        await test.step('text-object-undo-save-close',lambda:native(test,owned+'''
            var s=stringIDToTypeID;
            function metadata(){var r=new ActionReference();r.putProperty(s('property'),s('XMPMetadataAsUTF8'));r.putIdentifier(s('layer'),d.activeLayer.id);return executeActionGet(r).getString(s('XMPMetadataAsUTF8'));}
            function select(id){for(var i=0;i<d.layers.length;i++)if(d.layers[i].id===id){d.activeLayer=d.layers[i];return;}throw Error('Owned layer missing');}
            var after=metadata(),newest=d.activeHistoryState;d.activeHistoryState=d.historyStates[d.historyStates.length-2];
            if(metadata()!=='''+json.dumps(original_xmp)+''')throw Error('One undo source mismatch');
            d.activeHistoryState=newest;if(metadata()!==after)throw Error('Redo mismatch');
            select('''+str(original_id)+''');if(metadata()!=='''+json.dumps(original_xmp)+''')throw Error('Original instance changed');select('''+str(updated_id)+''');
            var file=new File('''+json.dumps(path.as_posix())+''');if(file.exists)throw Error('Persistence identity reused');
            var options=new PhotoshopSaveOptions();options.layers=true;d.saveAs(file,options,true,Extension.LOWERCASE);d.close(SaveOptions.DONOTSAVECHANGES);
            d=app.open(file);select('''+str(updated_id)+''');if(metadata()!==after)throw Error('Saved source mismatch');d.close(SaveOptions.DONOTSAVECHANGES);
            var ids=[];for(var i=0;i<app.documents.length;i++)ids.push(app.documents[i].id);
            if(encode(ids)!=='''+json.dumps(json.dumps(fixture['documents'],separators=(',',':')))+''')throw Error('Original document inventory changed');
            '''+("for(var i=0;i<app.documents.length;i++)if(app.documents[i].id==="+str(fixture['original'])+"){app.activeDocument=app.documents[i];break;}" if fixture['original'] is not None else '')+'''
            app.currentTool='''+json.dumps(fixture['tool'])+''';return {one_undo:true,redo:true,original_instance_unchanged:true,psd_reopened:true,owned_closed:true};
        '''))
        test.report['cleanup']='verified-closed-and-original-restored'
        test.report.pop('text_fixture',None)
        await test.save()


async def cleanup(test):
    fixture=test.report['text_fixture'];run_id=test.report['run_id']
    if len(run_id)!=32 or any(c not in '0123456789abcdef' for c in run_id):raise ValueError('Invalid fixture owner')
    doc_id=fixture.get('document_id');round_number=test.report.get('round')
    if type(doc_id) is not int or doc_id<=0 or round_number not in (1,2):raise ValueError('Unconfirmed fixture identity')
    documents=fixture.get('documents');original=fixture.get('original');tool=fixture.get('tool')
    if (not isinstance(documents,list) or any(type(i) is not int or i<=0 for i in documents) or
            original is not None and (type(original) is not int or original not in documents) or not isinstance(tool,str)):
        raise ValueError('Original document identity missing')
    name='151500px text acceptance '+run_id+' '+str((72,300)[round_number-1])
    restore=("for(var i=0;i<app.documents.length;i++)if(app.documents[i].id==="+str(original)+"){app.activeDocument=app.documents[i];break;}" if original is not None else '')
    result=await native(test,"if(!app.documents.length||app.activeDocument.id!=="+str(doc_id)+"||app.activeDocument.name!=="+json.dumps(name)+")throw Error('Owned fixture target changed');app.activeDocument.close(SaveOptions.DONOTSAVECHANGES);var ids=[];for(var i=0;i<app.documents.length;i++)ids.push(app.documents[i].id);if(encode(ids)!=="+json.dumps(json.dumps(documents,separators=(',',':')))+")throw Error('Original inventory mismatch');"+restore+"app.currentTool="+json.dumps(tool)+";return {closed_owned_document:true,original_active_restored:true};")
    test.report.pop('text_fixture')
    return result
