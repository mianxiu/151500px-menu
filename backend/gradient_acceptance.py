"""Fixed Photoshop gradient transactions on owned documents."""
import asyncio
from contextlib import nullcontext
from copy import deepcopy
import json
import time

from .color_native_acceptance import native, reload_plugin
from .gradient_editor import checked_draft, draft_path, load_draft
from .mask_preview import MaskPreview
from .settings import atomic_json
from .shape_paint import parameters


MAKE_GRADIENT = r'''
var d=app.activeDocument,s=stringIDToTypeID,make=new ActionDescriptor(),r=new ActionReference();
r.putClass(s('contentLayer'));make.putReference(s('null'),r);
var layer=new ActionDescriptor(),fill=new ActionDescriptor(),g=new ActionDescriptor(),colors=new ActionList(),alpha=new ActionList();
g.putString(s('name'),'Gradient acceptance');g.putEnumerated(s('gradientForm'),s('gradientForm'),s('customStops'));
g.putInteger(s('interfaceIconFrameDimmed'),3072);
for(var i=0;i<3;i++){
 var node=new ActionDescriptor(),c=new ActionDescriptor();c.putDouble(s('red'),30+60*i);c.putDouble(s('grain'),70+i*40);c.putDouble(s('blue'),140-i*30);
 node.putObject(s('color'),s('RGBColor'),c);node.putEnumerated(s('type'),s('colorStopType'),s('userStop'));
 node.putInteger(s('location'),i*2048);node.putInteger(s('midpoint'),40+i*5);colors.putObject(s('colorStop'),node);
 var stop=new ActionDescriptor();stop.putInteger(s('location'),i*2048);stop.putInteger(s('midpoint'),45+i*5);
 stop.putUnitDouble(s('opacity'),s('percentUnit'),100-i*25);alpha.putObject(s('transferSpec'),stop);
}
g.putList(s('colors'),colors);g.putList(s('transparency'),alpha);fill.putObject(s('gradient'),s('gradientClassEvent'),g);
fill.putEnumerated(s('type'),s('gradientType'),s('linear'));fill.putUnitDouble(s('angle'),s('angleUnit'),15);fill.putUnitDouble(s('scale'),s('percentUnit'),100);
fill.putBoolean(s('reverse'),false);fill.putBoolean(s('dither'),false);fill.putBoolean(s('align'),true);
layer.putObject(s('type'),s('gradientLayer'),fill);
if(vector){var rect=new ActionDescriptor();rect.putUnitDouble(s('top'),s('pixelsUnit'),8);rect.putUnitDouble(s('left'),s('pixelsUnit'),8);rect.putUnitDouble(s('bottom'),s('pixelsUnit'),112);rect.putUnitDouble(s('right'),s('pixelsUnit'),112);layer.putObject(s('shape'),s('rectangle'),rect);}
make.putObject(s('using'),s('contentLayer'),layer);executeAction(s('make'),make,DialogModes.NO);
return {id:d.id,layer:d.activeLayer.id,history:history(d)};
'''


async def preview_frames(test, target, first, last, cancel):
    preview = test.preview = MaskPreview(len(test.report['steps']), first, trace=True)
    task = asyncio.create_task(test.app.bridge.execute_shape(
        first, frozen_target=target, preview=preview, before_dispatch=test.before_dispatch, focus_guard=test.check))

    async def pump():
        while not task.done():
            await asyncio.to_thread(preview.publish)
            await asyncio.sleep(.08)

    publisher = asyncio.create_task(pump())
    frames = []
    try:
        for value in (first, last):
            if value is last:
                assert preview.update(value)
            deadline = time.monotonic() + 25
            while time.monotonic() < deadline:
                test.check()
                if publisher.done():
                    await publisher
                if task.done():
                    await task
                    raise RuntimeError('渐变预览在确认前结束')
                progress = await asyncio.to_thread(preview.progress)
                if progress and progress['stage'] == 'ready' and progress['frame'] >= preview.seq:
                    frames.append(progress['frame'])
                    break
                await asyncio.sleep(.08)
            else:
                raise RuntimeError('未收到渐变帧读回')
        if cancel:
            preview.cancel()
        else:
            assert preview.update(last, commit=True)
        result = await asyncio.wait_for(asyncio.shield(task), 100)
        proof = json.loads(result['result'])
        if proof.get('verified') is not True or bool(proof.get('cancelled')) != cancel:
            raise RuntimeError('渐变事务终态不符')
        if cancel and proof.get('rollback_verified') is not True:
            raise RuntimeError('渐变预览恢复未核对')
        return {'frames':frames, 'task_id':result['task_id'], 'preview':proof.get('preview')}
    except Exception:
        preview.cancel()
        try:
            await asyncio.wait_for(asyncio.shield(task), 100)
        except Exception:
            pass
        raise
    finally:
        publisher.cancel()
        await asyncio.gather(publisher, return_exceptions=True)
        preview.close()
        test.preview = None
        if not task.done():
            task.add_done_callback(lambda future:future.exception() if not future.cancelled() else None)


async def run(test):
    from tools.quiet_ps_session import ResourceLease
    wanted = (await asyncio.to_thread(test.app.uxp.status))['bundled_version']
    if test.report['versions']['probe_version'] != wanted:
        await test.step('gradient-reload', lambda:reload_plugin(test,wanted))
    originals = await native(test, r'''
var s=stringIDToTypeID,r=new ActionReference();r.putProperty(s('property'),s('modalToolLevel'));r.putEnumerated(s('application'),s('ordinal'),s('targetEnum'));
if(executeActionGet(r).getInteger(s('modalToolLevel'))!==0)throw Error('Native edit active; no fixture created');
var out=[];for(var i=0;i<app.documents.length;i++)out.push({id:app.documents[i].id,history:history(app.documents[i])});return {documents:out,active:app.documents.length?app.activeDocument.id:null,tool:app.currentTool};''')
    test.report['color_originals'] = originals
    await test.save()
    for round_number, mode in enumerate(('low', 'normal'), 1):
        test.report['round'] = round_number
        resource = test.folder / (test.report['run_id']+'-'+mode+'-resources.json')
        with ResourceLease(test.host_pid, resource, seconds=240) if mode=='low' else nullcontext():
            for vector in (False, True):
                async def exercise():
                    return await case(test, vector)
                await test.step('gradient-'+mode+('-shape' if vector else '-fill'), exercise)
    restored = await native(test, 'var expected='+json.dumps(originals)+r''';
if(app.documents.length!==expected.documents.length)throw Error('Test document retained');
for(var i=0;i<expected.documents.length;i++){var found=false;for(var j=0;j<app.documents.length;j++)if(app.documents[j].id===expected.documents[i].id){if(encode(history(app.documents[j]))!==encode(expected.documents[i].history))throw Error('Original history changed');found=true;if(expected.active===app.documents[j].id)app.activeDocument=app.documents[j];}if(!found)throw Error('Original document missing');}
app.currentTool=expected.tool;return {originals_preserved:true,owned_documents_closed:true};''')
    test.report['gradient_postflight'] = restored
    await test.save()


async def case(test, vector):
    test.report['color_fixture'] = {'creation_unconfirmed':True}
    test.report['cleanup'] = 'retained-color-fixture'
    await test.save()
    owned = await native(test, r'''var d=app.documents.add(UnitValue(128,'px'),UnitValue(128,'px'),72,'151500px gradient acceptance',NewDocumentMode.RGB,DocumentFill.TRANSPARENT,1,BitsPerChannelType.EIGHT,'sRGB IEC61966-2.1');return {id:d.id};''')
    test.report['color_fixture'] = owned
    await test.save()
    guard = 'if(app.activeDocument.id!=='+str(owned['id'])+')throw Error("Owned gradient target changed");'
    fixture = await native(test, guard+'var vector='+json.dumps(vector)+';'+MAKE_GRADIENT)
    test.report['color_fixture'] = fixture
    await test.save()

    async def read():
        result = await test.test_read(shape_inspect=True, gradient_details=True)
        shape = result.get('shape_editor',{})
        target = result.get('gradient_target',{})
        if not shape.get('available') or target.get('document_id') != owned['id'] or target.get('layer_ids') != [fixture['layer']]:
            raise RuntimeError('渐变测试目标读回不符')
        return shape, target

    shape, target = await read()
    if shape['vector'] is not vector or not shape['live_preview']:
        raise RuntimeError('隔离渐变类型或实时预览能力不符')
    if vector:
        value = parameters({k:shape[k] for k in ('revision','fill','stroke')})
        value['stroke'] = {**deepcopy(value['fill']),'width':3,'enabled':True}
        await test.app.bridge.execute_shape(value,frozen_target=target,focus_guard=test.check,before_dispatch=test.before_dispatch)
        shape, target = await read()
    initial = await native(test, guard+'return history(app.activeDocument);')
    key = 'stroke' if vector else 'fill'
    base = parameters({k:shape[k] for k in ('revision','fill','stroke')})
    first = deepcopy(base)
    first[key]['stops'][1].update(location=1792,midpoint=57)
    first[key]['stops'].insert(2, {'location':3200,'rgb':[112,137,92],'source':'userStop','midpoint':50})
    first[key]['opacity_stops'][1].update(opacity=58,midpoint=44)
    first[key]['options'].update(style='radial',angle=42,scale=120,reverse=True,dither=True,smoothness=3300)
    last = deepcopy(first)
    last[key]['opacity_stops'][1]['opacity'] = 63
    last[key]['stops'][1]['location'] = 1960
    last[key]['stops'][1]['rgb'] = [166,48,93]
    last[key]['options'].update(style='reflected' if vector else 'radial',angle=-45 if vector else 90)
    checked_draft(base,last)
    draft = draft_path(test.folder/('draft-'+test.report['run_id']),target,shape)
    await asyncio.to_thread(atomic_json,draft,last)
    assert await asyncio.to_thread(load_draft,draft,base) == last
    test.report['gradient_expected'] = {'options':last[key]['options'],'source':shape[key],'target':target}
    await test.save()
    cancelled = await preview_frames(test,target,first,last,True)
    after_cancel, target = await read()
    if after_cancel['baseline'] != shape['baseline']:
        raise RuntimeError('取消后原渐变数据有变化')
    for value in (first,last):
        value['revision'] = after_cancel['revision']
    applied = await preview_frames(test,target,first,last,False)
    actual, _ = await read()
    opacity = actual[key]['opacity_stops'][1]['opacity']
    added = actual[key]['stops'][2]
    if (actual[key]['options'] != last[key]['options'] or
            actual[key]['stops'][1]['location'] != 1960 or actual[key]['stops'][1]['rgb'] != [166,48,93] or
            added['location'] != 3200 or added['rgb'] != [112,137,92] or
            min(abs(opacity-63), abs(opacity-round(63*255/100)*100/255))>.01):
        raise RuntimeError('已确认渐变末值读回不符')
    undo = await native(test,guard+r'''executeAction(charIDToTypeID('undo'),undefined,DialogModes.NO);return history(app.activeDocument);''')
    restored, _ = await read()
    if restored['baseline']!=shape['baseline'] or any(undo[k]!=initial[k] for k in ('index','name')):
        raise RuntimeError('渐变一次撤销未恢复原值')
    await native(test,guard+'app.activeDocument.close(SaveOptions.DONOTSAVECHANGES);return {closed:true};')
    test.report.pop('color_fixture',None)
    test.report['cleanup'] = 'verified-closed-and-original-restored'
    await test.save()
    return {'vector':vector,'cancelled':cancelled,'applied':applied,'draft_restored':True,'one_undo':True,'actual':actual[key]}
