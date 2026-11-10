"""Fixed native-channel acceptance on owned documents; no caller-supplied scripts."""
import asyncio
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import time
import uuid
from .text_object_acceptance import native as execute_native
from .stability_metrics import host_scope
from .uxp_bridge import read_heartbeat

HISTORY=r'''
function history(d){var active=app.activeDocument;try{app.activeDocument=d;var state=d.activeHistoryState;for(var i=0;i<d.historyStates.length;i++)if(d.historyStates[i]===state)return {index:i,name:state.name,count:d.historyStates.length};throw Error('Native history identity unavailable');}finally{app.activeDocument=active;}}
'''

async def native(test,code):
    return await execute_native(test,HISTORY+code)


async def reload_plugin(test):
    """Back up originals before the authorized normal PS restart needed for UXP."""
    folder=test.folder/('color-originals-'+test.report['run_id'])
    folder.mkdir(exist_ok=False)
    test.report['original_backup_folder']=str(folder)
    test.report['scope']['original_documents_saved_or_closed']=True
    test.report['scope']['temporary_document_only']=False
    await test.save()
    value=await native(test,r'''
var s=stringIDToTypeID,r=new ActionReference();r.putProperty(s('property'),s('modalToolLevel'));r.putEnumerated(s('application'),s('ordinal'),s('targetEnum'));
if(executeActionGet(r).getInteger(s('modalToolLevel'))!==0)throw Error('Native edit active; originals retained');
function stamp(d){var rows=[],profile=null;try{profile=d.colorProfileName;}catch(_){}function walk(ls){for(var j=0;j<ls.length;j++){var l=ls[j];rows.push([l.name,l.typename,l.typename==='ArtLayer'?String(l.kind):'group',l.typename==='ArtLayer'&&l.kind===LayerKind.TEXT?l.textItem.contents:null]);if(l.typename==='LayerSet')walk(l.layers);}}walk(d.layers);return {id:d.id,name:d.name,saved:d.saved,history:history(d),rows:rows,mode:String(d.mode),depth:String(d.bitsPerChannel),profile:profile};}
var initial=[],files=[],active=app.documents.length?app.activeDocument.id:null,tool=app.currentTool;
for(var i=0;i<app.documents.length;i++)initial.push(stamp(app.documents[i]));
'''+'var folder=new Folder('+json.dumps(folder.as_posix())+');'+r'''
for(var i=0;i<initial.length;i++){var d=app.documents[i];if(d.id!==initial[i].id)throw Error('Document set changed');app.activeDocument=d;
if(d.width.as('px')>30000||d.height.as('px')>30000)throw Error('Original needs PSB backup; no documents closed');
var sub=new Folder(folder+'/'+i);if(!sub.create())throw Error('Backup folder unavailable');
var safe=initial[i].name,invalid=[92,47,58,42,63,34,60,62,124];for(var k=0;k<invalid.length;k++)safe=safe.split(String.fromCharCode(invalid[k])).join('_');var ext=safe.substr(-4).toLowerCase();if(ext==='.psd'||ext==='.psb')safe=safe.substr(0,safe.length-4);
var file=new File(sub+'/'+safe+'.psd'),opt=new PhotoshopSaveOptions();opt.layers=true;opt.embedColorProfile=true;
d.saveAs(file,opt,true,Extension.LOWERCASE);if(!file.exists||file.length<=0)throw Error('Layered backup unavailable');files.push(file.fsName);}
for(var i=0;i<initial.length;i++)if(encode(stamp(app.documents[i]))!==encode(initial[i]))throw Error('Original changed during backup');
for(var i=0;i<app.documents.length;i++)if(app.documents[i].id===active)app.activeDocument=app.documents[i];
return {originals:initial,files:files,active:active,tool:tool};
''')
    for name in value['files']:
        path=Path(name)
        if not path.is_file() or not path.stat().st_size:raise RuntimeError('原稿分层备份未落盘；未退出PS')
    test.report['original_backups']=[{'path':p,'sha256':hashlib.sha256(Path(p).read_bytes()).hexdigest()} for p in value['files']]
    test.report['original_before_reload']=value
    await test.save()
    # The original identities and histories are checked again immediately before closing.
    await native(test,'var expected='+json.dumps(value['originals'])+',paths='+json.dumps(value['files'])+r''';
var s=stringIDToTypeID,r=new ActionReference();r.putProperty(s('property'),s('modalToolLevel'));r.putEnumerated(s('application'),s('ordinal'),s('targetEnum'));if(executeActionGet(r).getInteger(s('modalToolLevel'))!==0)throw Error('Native edit active; no originals closed');
if(app.documents.length!==expected.length)throw Error('Original document set changed; no close');
for(var i=0;i<expected.length;i++)if(app.documents[i].id!==expected[i].id||encode(history(app.documents[i]))!==encode(expected[i].history))throw Error('Original changed; no close');
for(var i=app.documents.length-1;i>=0;i--){app.activeDocument=app.documents[i];var opt=new PhotoshopSaveOptions();opt.layers=true;opt.embedColorProfile=true;app.activeDocument.saveAs(new File(paths[i]),opt,true,Extension.LOWERCASE);if(!new File(paths[i]).exists||new File(paths[i]).length<=0)throw Error('Latest layered backup missing; no close');app.activeDocument.close(SaveOptions.DONOTSAVECHANGES);}
return {closed:true,count:expected.length};''')
    test.report['original_backups']=[{'path':p,'sha256':hashlib.sha256(Path(p).read_bytes()).hexdigest()} for p in value['files']]
    test.report['original_restore_pending']=True
    await test.save()
    # Quit is a normal COM call, with its own returned execution record.
    record={'id':uuid.uuid4().hex,'phase':'submitted','operation':'normal-photoshop-quit','original_pid':test.host_pid}
    test.report['normal_quit']=record;await test.save()
    try:await asyncio.get_running_loop().run_in_executor(test.app.bridge.executor,lambda:test.app.bridge._application().Quit())
    except Exception:
        record['phase']='unknown';await test.save();raise
    record['phase']='returned';await test.save()
    import win32api,win32event
    import win32process
    try:handle=win32api.OpenProcess(0x100000,False,test.host_pid)
    except Exception:
        if test.host_pid in win32process.EnumProcesses():raise
        handle=None
    try:
        if handle and await asyncio.to_thread(win32event.WaitForSingleObject,handle,15000)!=0:raise RuntimeError('PS正常退出未确认；未重启')
    finally:
        if handle:handle.Close()
    record['process_exit_verified']=True
    await test.save()
    await asyncio.to_thread(test.app.uxp.install,None)
    await test.app.photoshop_launcher.start(True)
    deadline=time.monotonic()+75
    while time.monotonic()<deadline:
        try:
            heartbeat=await asyncio.to_thread(read_heartbeat,test.heartbeat_folder)
            if heartbeat.get('probe_version')=='0.3.103' and heartbeat.get('session')!=test.report['versions']['session']:
                test.host_pid,test.focus_check=await asyncio.to_thread(host_scope)
                break
        except (OSError,ValueError,RuntimeError):pass
        await asyncio.sleep(.5)
    else:raise RuntimeError('新UXP会话未就绪；原稿分层备份保留')
    test.report['versions_before_reload']=dict(test.report['versions'])
    test.report['versions']={k:heartbeat.get(k) for k in test.report['versions']}
    test.report['restoration_session']=heartbeat['session'];await test.save()
    restored=await native(test,'var paths='+json.dumps(value['files'])+',expected='+json.dumps(value['originals'])+',oldActive='+json.dumps(value['active'])+',tool='+json.dumps(value['tool'])+r''';
if(app.documents.length)throw Error('New session contains documents; automatic restore refused');
var rows=[],active=null;for(var i=0;i<paths.length;i++){var d=app.open(new File(paths[i])),layers=[],profile=null;try{profile=d.colorProfileName;}catch(_){}function walk(ls){for(var j=0;j<ls.length;j++){var l=ls[j];layers.push([l.name,l.typename,l.typename==='ArtLayer'?String(l.kind):'group',l.typename==='ArtLayer'&&l.kind===LayerKind.TEXT?l.textItem.contents:null]);if(l.typename==='LayerSet')walk(l.layers);}}walk(d.layers);
if(encode(layers)!==encode(expected[i].rows)||String(d.mode)!==expected[i].mode||String(d.bitsPerChannel)!==expected[i].depth||profile!==expected[i].profile)throw Error('Restored layered contents or profile mismatch');rows.push({id:d.id,name:d.name});if(expected[i].id===oldActive)active=d;}
if(active)app.activeDocument=active;app.currentTool=tool;
return {documents:rows,verified:true};''')
    test.report['original_restore_pending']=False
    test.report['restored_originals']=restored
    await test.save()


async def run(test):
    if test.report['versions']['probe_version']!='0.3.103':
        await test.step('color-native-reload',lambda:reload_plugin(test))
    from tools.quiet_ps_session import ResourceLease
    from contextlib import nullcontext
    original=await native(test,r'''var out=[];for(var i=0;i<app.documents.length;i++){var d=app.documents[i];out.push({id:d.id,history:history(d)});}return {documents:out,active:app.documents.length?app.activeDocument.id:null,tool:app.currentTool};''')
    test.report['color_originals']=original;await test.save()
    await asyncio.to_thread(test.app.desktop.focus_search_result)
    for mode in ('low','normal'):
        test.report['round']=1 if mode=='low' else 2
        journal=test.folder/(test.report['run_id']+'-'+mode+'-resources.json')
        with ResourceLease(test.host_pid,journal,seconds=240) if mode=='low' else nullcontext():
            for model,psmode,kind,keys,values in (
                ('cmyk','CMYK','CMYKColorClass',['cyan','magenta','yellowColor','black'],[12.345,20,30,4]),
                ('lab','LAB','labColor',['luminance','a','b'],[55.5,-12.75,27]),
                ('gray','GRAYSCALE','grayscale',['gray'],[62.5])):
                for depth in (8,16):
                    async def exercise():
                        bridge=test.app.bridge
                        test.report['color_fixture']={'creation_unconfirmed':True,'model':model,'depth':depth}
                        test.report['cleanup']='retained-color-fixture';await test.save()
                        color={**dict(zip(keys,values))}
                        settings='var model='+json.dumps(psmode)+',depth='+str(depth)+',color='+json.dumps(color)+',kind='+json.dumps(kind)+';'
                        create=settings+r'''
var before=[];for(var i=0;i<app.documents.length;i++)before.push(app.documents[i].id);
var d=app.documents.add(UnitValue(128,'px'),UnitValue(128,'px'),72,'151500px native color '+model,NewDocumentMode[model],DocumentFill.TRANSPARENT,1,depth===8?BitsPerChannelType.EIGHT:BitsPerChannelType.SIXTEEN);
for(var j=0;j<before.length;j++)if(before[j]===d.id)throw Error('Test document identity unavailable');
return {id:d.id};'''
                        fixture=await native(test,create)
                        test.report['color_fixture']=fixture;await test.save()
                        guard='if(app.activeDocument.id!=='+str(fixture['id'])+')throw Error("Owned color target changed");'
                        fixture=await native(test,settings+guard+r'''var d=app.activeDocument;
var s=stringIDToTypeID,a=new ActionDescriptor(),r=new ActionReference(),using=new ActionDescriptor(),fill=new ActionDescriptor(),c=new ActionDescriptor(),shape=new ActionDescriptor();r.putClass(s('contentLayer'));a.putReference(s('null'),r);
for(var k in color)if(color.hasOwnProperty(k))c.putDouble(s(k),color[k]);fill.putObject(s('color'),s(kind),c);using.putObject(s('type'),s('solidColorLayer'),fill);
shape.putUnitDouble(s('top'),s('pixelsUnit'),8);shape.putUnitDouble(s('left'),s('pixelsUnit'),8);shape.putUnitDouble(s('bottom'),s('pixelsUnit'),100);shape.putUnitDouble(s('right'),s('pixelsUnit'),100);using.putObject(s('shape'),s('rectangle'),shape);a.putObject(s('using'),s('contentLayer'),using);executeAction(s('make'),a,DialogModes.NO);
var profile=null;try{profile=d.colorProfileName;}catch(_){}return {id:d.id,layer:d.activeLayer.id,history:history(d),mode:String(d.mode),depth:String(d.bitsPerChannel),profile:profile};''')
                        test.report['color_fixture']=fixture;await test.save()
                        before=await test.test_read(shape_inspect=True)
                        shape=before.get('shape_editor',{})
                        if not shape.get('available') or shape.get('native_model')!=model or shape.get('live_preview'):
                            raise RuntimeError('原生颜色面板读取／本地草稿能力不符：'+str(shape))
                        if shape['fill'].get('native',{}).get('model')!=model:raise RuntimeError('PS原生颜色模型读回不符')
                        after_read=await native(test,guard+'return history(app.activeDocument);')
                        if after_read!=fixture['history']:raise RuntimeError('只读颜色检查改变了PS历史')
                        params={k:deepcopy(shape[k]) for k in ('revision','fill','stroke')}
                        params['fill']['native']['values'][0]+=1
                        result=await bridge.execute_shape(params,before_dispatch=test.before_dispatch)
                        after=(await test.test_read(shape_inspect=True))['shape_editor']
                        actual,wanted=after['fill']['native'],params['fill']['native']
                        test.report['color_readback']={'actual':actual,'wanted':wanted};await test.save()
                        if (actual['model']!=wanted['model'] or len(actual['values'])!=len(wanted['values']) or
                                any(abs(a-b)>.01 for a,b in zip(actual['values'],wanted['values']))):raise RuntimeError('原生通道写入读回不符')
                        restored=await native(test,guard+r'''var d=app.activeDocument,s=stringIDToTypeID;executeAction(charIDToTypeID('undo'),undefined,DialogModes.NO);var profile=null;try{profile=d.colorProfileName;}catch(_){}return {history:history(d),mode:String(d.mode),depth:String(d.bitsPerChannel),profile:profile};''')
                        if (any(restored[k]!=fixture[k] for k in ('mode','depth','profile')) or
                                any(restored['history'][k]!=fixture['history'][k] for k in ('index','name'))):raise RuntimeError('一次撤销或文档模式／ICC不符')
                        undo=(await test.test_read(shape_inspect=True))['shape_editor']
                        if undo['fill']!=shape['fill'] or undo['stroke']!=shape['stroke']:raise RuntimeError('一次撤销未恢复原生颜色')
                        if depth==8:
                            params={k:deepcopy(undo[k]) for k in ('revision','fill','stroke')}
                            first=deepcopy(shape['fill']['native']);last=deepcopy(first);last['values'][0]+=2
                            params['fill']={'kind':'gradient','enabled':True,'method':'classic','stops':[{'native':first,'location':0},{'native':last,'location':4096}]}
                            params['stroke']={'kind':'solid','enabled':True,'width':2.5,'native':first}
                            await bridge.execute_shape(params,before_dispatch=test.before_dispatch)
                            gradient=(await test.test_read(shape_inspect=True))['shape_editor']
                            if (gradient['fill']['kind']!='gradient' or len(gradient['fill']['stops'])!=2 or not gradient['stroke']['enabled'] or
                                    gradient['stroke'].get('native',{}).get('model')!=model):raise RuntimeError('原生渐变／描边读回不符')
                            await native(test,guard+"executeAction(charIDToTypeID('undo'),undefined,DialogModes.NO);return {undone:true};")
                            undo=(await test.test_read(shape_inspect=True))['shape_editor']
                            if undo['fill']!=shape['fill'] or undo['stroke']!=shape['stroke']:raise RuntimeError('原生渐变／描边一次撤销未恢复')
                        await native(test,guard+'app.activeDocument.close(SaveOptions.DONOTSAVECHANGES);return {closed:true};')
                        test.report.pop('color_fixture',None);test.report['cleanup']='verified-closed-and-original-restored';await test.save()
                        return {'mode':mode,'model':model,'depth':depth,'fixture':fixture,'task_id':result['task_id'],'native_values':after['fill']['native'],'single_undo':True,'mode_profile_preserved':True,'gradient_and_stroke':depth==8}
                    await test.step('color-native-'+mode+'-'+model+'-'+str(depth),exercise)
    restored=await native(test,'var original='+json.dumps(original)+r''';
for(var i=0;i<original.documents.length;i++){var found=false;for(var j=0;j<app.documents.length;j++)if(app.documents[j].id===original.documents[i].id){if(encode(history(app.documents[j]))!==encode(original.documents[i].history))throw Error('Original history changed');if(original.active===original.documents[i].id)app.activeDocument=app.documents[j];found=true;}if(!found)throw Error('Original document missing');}
if(app.documents.length!==original.documents.length)throw Error('Test document retained');app.currentTool=original.tool;return {originals_preserved:true,owned_documents_closed:true};''')
    test.report['color_postflight']=restored
    await test.save()


async def cleanup(test):
    fixture=test.report.get('color_fixture',{})
    if type(fixture.get('id')) is not int:raise RuntimeError('缺少已确认的测试文档身份')
    original=test.report['color_originals']
    value=await native(test,'var owned='+str(fixture['id'])+',original='+json.dumps(original)+r''';
if(app.activeDocument.id!==owned||app.documents.length!==original.documents.length+1)throw Error('Owned color cleanup target changed');
for(var i=0;i<original.documents.length;i++){var found=false;for(var j=0;j<app.documents.length;j++)if(app.documents[j].id===original.documents[i].id){if(encode(history(app.documents[j]))!==encode(original.documents[i].history))throw Error('Original history changed');found=true;}if(!found)throw Error('Original document missing');}
app.activeDocument.close(SaveOptions.DONOTSAVECHANGES);
for(var i=0;i<app.documents.length;i++)if(app.documents[i].id===original.active)app.activeDocument=app.documents[i];app.currentTool=original.tool;
return {closed_owned_document:true,original_active_restored:true};''')
    test.report.pop('color_fixture',None)
    return value
