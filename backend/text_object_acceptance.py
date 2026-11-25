"""Fixed text-object acceptance inside the service COM apartment, never file-open scripts."""
import asyncio
import hashlib
import json
import math
import re
import tempfile
import uuid
from pathlib import Path
from . import text_object
from .paths import ROOT
from .text_plan import compute
from .settings import atomic_json

FIXTURE_LAYER_STATE='''
function fixtureLayers(root){
    var rows=[];
    function walk(group,parent){for(var i=0;i<group.layers.length;i++){
        var l=group.layers[i],isGroup=l.typename==='LayerSet';
        rows.push([l.id,parent,l.visible,isGroup?'group':String(l.kind),
            !isGroup&&l.kind===LayerKind.TEXT?l.textItem.contents:null]);
        if(isGroup)walk(l,l.id);
    }}
    walk(root,0);return encode(rows);
}
function fixtureState(d){
    var s=stringIDToTypeID,r=new ActionReference(),selection=s('targetLayersIDs'),ids=[];
    r.putProperty(s('property'),selection);r.putEnumerated(s('document'),s('ordinal'),s('targetEnum'));
    var selected=executeActionGet(r).getList(selection);
    for(var i=0;i<selected.count;i++)ids.push(selected.getReference(i).getIdentifier());
    r=new ActionReference();r.putEnumerated(s('historyState'),s('ordinal'),s('targetEnum'));
    return {saved:d.saved,layer:d.activeLayer.id,selected:ids,history:executeActionGet(r).getInteger(s('ID')),
        history_count:d.historyStates.length,layers:fixtureLayers(d)};
}
'''

def object_frame(snapshot):
    frame=snapshot.get('object_frame')
    if not isinstance(frame,list) or len(frame)!=8 or any(type(value) not in (float,int) or not math.isfinite(value) for value in frame):
        raise RuntimeError('文字对象的实际置入四角无效')
    return frame


def check_frame(actual,expected):
    if any(abs(value-wanted)>.02 for value,wanted in zip(actual,expected)):
        raise RuntimeError('文字对象置入位置不一致：'+json.dumps({'actual':actual,'expected':expected}))


def source_frame(snapshot):
    frame=object_frame(snapshot)
    xml=snapshot['object_xmp']
    canvas=text_object.read(xml)['canvas']
    bounds=text_object.render_bounds(xml,canvas)
    if bounds is None:return frame
    left,top,right,bottom=bounds
    result=[]
    for x,y in ((0,0),(canvas['width'],0),(canvas['width'],canvas['height']),(0,canvas['height'])):
        horizontal=(x-left)/(right-left)
        vertical=(y-top)/(bottom-top)
        result.extend(frame[axis]+horizontal*(frame[axis+2]-frame[axis])+vertical*(frame[axis+6]-frame[axis]) for axis in (0,1))
    return result


def owned_document(doc_id,names):
    name_changed='&&'.join('app.activeDocument.name!=='+json.dumps(name) for name in names)
    return (f'if(!app.documents.length||app.activeDocument.id!=={doc_id}||({name_changed}))'
            "throw Error('Owned fixture target changed');var d=app.activeDocument;")


def undo_fixture(owned, baseline, placed_layer):
    return owned+FIXTURE_LAYER_STATE+f'''
        var before={json.dumps(baseline)},current=fixtureState(d);
        if(current.layer!=={placed_layer}||current.history_count!==before.history_count+1||current.saved||
            d.activeHistoryState.name!=='Vector text object')throw Error('Vector placement did not create exactly one owned history step');
        executeAction(stringIDToTypeID('undoEvent'),new ActionDescriptor(),DialogModes.NO);
        var restored=fixtureState(d);
        if(encode(restored)!==encode(before))throw Error('Native undo did not restore the baseline: '+encode({{
            history:restored.history,layer:restored.layer,selected:restored.selected,saved:restored.saved,
            layers_restored:restored.layers===before.layers}}));
        return {{one_undo:true,history:restored.history,original_selection_restored:true,
            original_saved_restored:true,restored_saved:restored.saved}};
    '''


def native_test_files(run):
    root=Path(tempfile.gettempdir()).resolve()
    longest=root/'pmt-12345678'/f'151500px text acceptance {run} 300.psd'
    if len(str(longest))>240:
        raise RuntimeError('Photoshop 原生验收需要较短的 Windows 临时目录')
    return Path(tempfile.mkdtemp(prefix='pmt-',dir=root))


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
    in_place=test.report.get('profile')=='text-object-in-place'
    files=native_test_files(test.report['run_id'])
    test.report['native_test_files']=str(files)
    async def read():
        test.check()
        return (await test.test_read(text_inspect=True))['text']

    async def write(snapshot,draft,style=None):
        model=text_object.read(snapshot['object_xmp']) if 'object_xmp' in snapshot else None
        original=model['layer']['text']['textKey'] if model else snapshot['text']
        plan=compute(original,{'revision':snapshot['revision'],'draft':draft,'operation':'edit'})
        params={k:plan[k] for k in ('value','origins','count')}
        params['revision']=snapshot['revision']
        if style:params['typography']=style
        if model:params.update(object_source=model,object_render=text_object.render_hash(snapshot['object_xmp']))
        target=snapshot['target']
        def verify_fixture():
            test.check()
            if (target['document_id']!=test.report['text_fixture']['document_id'] or
                    target['session']!=test.report['versions']['session']):
                raise RuntimeError('隔离文字目标或 Photoshop 会话已变化')
        value=await bridge.execute_text_object(target,params,before_dispatch=test.before_dispatch,_fixture_guard=verify_fixture,
                                               **({'in_place_fixture':name} if in_place else {}))
        receipt=json.loads(value['result'])
        if receipt.get('verified') is not True:raise RuntimeError('文字对象生成核验失败')
        if receipt.get('source_workspace')!='in-place' or not receipt.get('retained_history_count'):
            raise RuntimeError('原位导出或历史凭据缺失')
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
            app.currentTool='moveTool';return {document_id:d.id,layer_id:l.id,original:original,documents:ids,tool:tool};
        '''))
        test.report['text_fixture']=fixture
        test.report['cleanup']='retained-text-object-fixture'
        await test.save()
        names=[name,name+'.psd']
        owned=owned_document(fixture['document_id'],names)
        if not in_place:
            unsaved=await native(test,owned+FIXTURE_LAYER_STATE+'''
                if(d.saved)throw Error('Expected a never-saved text fixture');
                return fixtureState(d);
            ''')
            fresh_text=await read()
            if fresh_text['target']['document_id']!=fixture['document_id']:raise RuntimeError('未保存的隔离文字目标不一致')
            await test.step('text-object-unsaved-create',lambda:write(fresh_text,fresh_text['text']))
            unsaved_object=await read()
            await test.step('text-object-unsaved-undo',lambda:native(test,undo_fixture(owned,unsaved,unsaved_object['target']['active_layer_id'])))
            if (await read())['target']!=fresh_text['target']:raise RuntimeError('未保存文字的撤销选层不一致')
        # Only the verified fixture is saved to this fresh path.
        saved_path=files/(name+'.psd')
        baseline=await test.step('text-object-saved-baseline',lambda:native(test,owned+FIXTURE_LAYER_STATE+'''
            var text=d.activeLayer,group=d.layerSets.add();
            var visible=group.artLayers.add(),hidden=group.artLayers.add();hidden.visible=false;d.activeLayer=text;
            var file=new File('''+json.dumps(saved_path.as_posix())+''');
            if(file.exists)throw Error('Original fixture path already exists');
            var options=new PhotoshopSaveOptions(),dialogs=app.displayDialogs;options.layers=true;
            try{app.displayDialogs=DialogModes.NO;d.saveAs(file,options,false,Extension.LOWERCASE);}
            finally{app.displayDialogs=dialogs;}
            if(d.activeLayer.id!==text.id||!file.exists||!d.saved||!visible.visible||hidden.visible)throw Error('Isolated saved baseline failed');
            return fixtureState(d);
        '''))
        snap=await read()
        if snap['target']['document_id']!=fixture['document_id']:raise RuntimeError('隔离文字目标不一致')
        await test.step('text-object-create',lambda:write(snap,snap['text'],{'tracking':35}))
        created=await read();model=text_object.read(created['object_xmp'])
        if model['schema']!=1 or model['layer']['text']['textKey']!=snap['text'] or any(row['textStyle'].get('tracking')!=35 for row in model['layer']['text']['textStyleRange']):raise RuntimeError('PS原生文字源或字距不一致')
        original_frame=object_frame(created)
        original_source_frame=source_frame(created)
        if text_object.render_bounds(created['object_xmp'],model['canvas']) is None:
            raise RuntimeError('未生成带原稿坐标的紧凑 PDF')
        await test.step('text-object-create-undo',lambda:native(test,undo_fixture(owned,baseline,created['target']['active_layer_id'])))
        restored=await read()
        if restored['target']!=snap['target'] or restored['text']!=snap['text']:raise RuntimeError('撤销后文字源目标不一致')
        # The scripting undo event removes the scripted step from the history.
        # Make a new, identified request from the verified state for the update test.
        await test.step('text-object-create-again',lambda:write(restored,snap['text'],{'tracking':35}))
        created=await read()
        check_frame(object_frame(created),original_frame)
        original_id=created['target']['active_layer_id'];original_xmp=created['object_xmp']
        original_baseline=await native(test,owned+FIXTURE_LAYER_STATE+'return fixtureState(d);')
        unique=await test.step('text-object-update-unique',lambda:write(created,snap['text'].replace('old','solo')))
        unique_receipt=json.loads(unique['result'])
        unique_snapshot=await read()
        if (unique_receipt.get('placement')!='original-layer' or unique_snapshot['target']!=created['target'] or
                text_object.read(unique_snapshot['object_xmp'])['layer']['text']['textKey']!=snap['text'].replace('old','solo')):
            raise RuntimeError('未共享的矢量文字更新未保留原图层')
        check_frame(source_frame(unique_snapshot),original_source_frame)
        await test.step('text-object-update-unique-undo',lambda:native(test,undo_fixture(owned,original_baseline,original_id)))
        unique_undo=await read()
        if unique_undo['target']!=created['target'] or unique_undo['object_xmp']!=original_xmp:
            raise RuntimeError('原图层更新撤销后文字源不一致')
        # Scale about the placement centre and translate the copy. Layer.resize
        # may anchor on visible layer bounds, so measure translation separately.
        scaled=await test.step('text-object-scale',lambda:native(test,owned+'''
            var l=d.activeLayer.duplicate();d.activeLayer=l;
            var s=stringIDToTypeID;
            function frame(){var r=new ActionReference();r.putProperty(s('property'),s('smartObjectMore'));r.putIdentifier(s('layer'),l.id);
                var values=executeActionGet(r).getObjectValue(s('smartObjectMore')).getList(s('transform')),points=[];
                for(var i=0;i<values.count;i++)points.push(values.getDouble(i));return points;}
            l.resize(200,200,AnchorPosition.MIDDLECENTER);var before=frame();
            l.translate(UnitValue(17,'px'),UnitValue(9,'px'));return {copy_id:l.id,before_move:before};
        '''))
        copied=await read()
        if copied['target']['active_layer_id']!=scaled['copy_id']:raise RuntimeError('缩放副本选择不一致')
        copied_frame=object_frame(copied)
        copied_source_frame=source_frame(copied)
        before_move=object_frame({'object_frame':scaled['before_move']})
        for index in range(0,8,2):
            if (abs((before_move[index]-before_move[0])-2*(original_frame[index]-original_frame[0]))>.02 or
                abs((before_move[index+1]-before_move[1])-2*(original_frame[index+1]-original_frame[1]))>.02):
                raise RuntimeError('文字对象副本的实际缩放比例与 200% 不一致')
        check_frame(copied_frame,[value+offset for value,offset in zip(before_move,[17,9]*4)])
        copied_baseline=await native(test,owned+FIXTURE_LAYER_STATE+'return fixtureState(d);')
        shared=await test.step('text-object-update',lambda:write(copied,snap['text'].replace('old','new'),{'tracking':-10}))
        if json.loads(shared['result']).get('placement')!='independent-copy':
            raise RuntimeError('共享文字对象未创建独立内容')
        updated=await read();updated_id=updated['target']['active_layer_id']
        if updated_id==scaled['copy_id']:raise RuntimeError('共享文字对象被原位更新')
        check_frame(source_frame(updated),copied_source_frame)
        updated_text=text_object.read(updated['object_xmp'])['layer']['text']
        if updated_text['textKey']!=snap['text'].replace('old','new') or any(row['textStyle'].get('tracking')!=-10 for row in updated_text['textStyleRange']):raise RuntimeError('更新正文或字距不一致')
        await test.step('text-object-update-undo',lambda:native(test,undo_fixture(owned,copied_baseline,updated_id)))
        restored_copy=await read()
        if restored_copy['target']!=copied['target'] or restored_copy['object_xmp']!=copied['object_xmp']:raise RuntimeError('撤销后矢量文字源不一致')
        check_frame(object_frame(restored_copy),copied_frame)
        await test.step('text-object-update-again',lambda:write(restored_copy,snap['text'].replace('old','new'),{'tracking':-10}))
        updated=await read();updated_id=updated['target']['active_layer_id']
        check_frame(source_frame(updated),copied_source_frame)
        if text_object.read(updated['object_xmp'])['layer']['text']!=updated_text:raise RuntimeError('再次更新文字源不一致')
        path=files/('reopen-'+str(dpi)+'.psd')
        await test.step('text-object-save-close',lambda:native(test,owned+'''
            var s=stringIDToTypeID;
            function metadata(){var r=new ActionReference();r.putProperty(s('property'),s('XMPMetadataAsUTF8'));r.putIdentifier(s('layer'),d.activeLayer.id);return executeActionGet(r).getString(s('XMPMetadataAsUTF8'));}
            function select(id){for(var i=0;i<d.layers.length;i++)if(d.layers[i].id===id){d.activeLayer=d.layers[i];return;}throw Error('Owned layer missing');}
            var after=metadata();
            select('''+str(original_id)+''');if(metadata()!=='''+json.dumps(original_xmp)+''')throw Error('Original instance changed');select('''+str(updated_id)+''');
            var file=new File('''+json.dumps(path.as_posix())+''');if(file.exists)throw Error('Persistence identity reused');
            var options=new PhotoshopSaveOptions(),dialogs=app.displayDialogs;options.layers=true;
            try{app.displayDialogs=DialogModes.NO;d.saveAs(file,options,true,Extension.LOWERCASE);}
            finally{app.displayDialogs=dialogs;}
            d.close(SaveOptions.DONOTSAVECHANGES);
            d=app.open(file);select('''+str(updated_id)+''');if(metadata()!==after)throw Error('Saved source mismatch');d.close(SaveOptions.DONOTSAVECHANGES);
            var ids=[];for(var i=0;i<app.documents.length;i++)ids.push(app.documents[i].id);
            if(encode(ids)!=='''+json.dumps(json.dumps(fixture['documents'],separators=(',',':')))+''')throw Error('Original document inventory changed');
            '''+("for(var i=0;i<app.documents.length;i++)if(app.documents[i].id==="+str(fixture['original'])+"){app.activeDocument=app.documents[i];break;}" if fixture['original'] is not None else '')+'''
            app.currentTool='''+json.dumps(fixture['tool'])+''';return {original_instance_unchanged:true,psd_reopened:true,owned_closed:true};
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
    names=[name,name+'.psd']
    name_changed='&&'.join('app.activeDocument.name!=='+json.dumps(item) for item in names)
    result=await native(test,"if(!app.documents.length||app.activeDocument.id!=="+str(doc_id)+"||("+name_changed+"))throw Error('Owned fixture target changed');app.activeDocument.close(SaveOptions.DONOTSAVECHANGES);var ids=[];for(var i=0;i<app.documents.length;i++)ids.push(app.documents[i].id);if(encode(ids)!=="+json.dumps(json.dumps(documents,separators=(',',':')))+")throw Error('Original inventory mismatch');"+restore+"app.currentTool="+json.dumps(tool)+";return {closed_owned_document:true,original_active_restored:true};")
    test.report.pop('text_fixture')
    return result
