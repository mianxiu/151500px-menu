const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('uxp/offline-probe/labels.js','utf8');
const clone=value=>JSON.parse(JSON.stringify(value));
{
 const renderer=fs.readFileSync('jsx/text_object.jsx','utf8');
 const part=(begin,end)=>renderer.slice(renderer.indexOf(`    function ${begin}(`),renderer.indexOf(`    function ${end}(`));
 const check=vm.runInNewContext(`${part('encode','name')}${part('signature','checkSourceFonts')}${part('near','drawTable')}checkText`);
 const sparse={textKey:'AB',textStyleRange:[{from:0,to:3,textStyle:{fontPostScriptName:'MyriadPro-Regular'}}],
  paragraphStyleRange:[{from:0,to:3,paragraphStyle:{textComposerEngine:{_enum:'textComposerEngine',_value:'textOptycaComposer'},
   directionType:{_enum:'directionType',_value:'dirLeftToRight'}}}]};
 const expanded=clone(sparse),paragraph=expanded.paragraphStyleRange[0].paragraphStyle;
 Object.assign(paragraph,{align:{_enum:'alignmentType',_value:'left'},autoLeadingPercentage:Math.fround(1.2),
  textEveryLineComposer:true,hyphenate:false,justificationWordMaximum:Math.fround(1.33)});
 assert.doesNotThrow(()=>check(expanded,sparse));
 paragraph.hyphenateWordSize=8;
 assert.doesNotThrow(()=>check(expanded,sparse));
 paragraph.hyphenate=true;
 const enabled=clone(sparse);enabled.paragraphStyleRange[0].paragraphStyle.hyphenate=true;
 assert.throws(()=>check(expanded,enabled),/hyphenateWordSize/);
 delete paragraph.hyphenateWordSize;
 paragraph.hyphenate=false;
 paragraph.textEveryLineComposer=false;
 assert.throws(()=>check(expanded,sparse),/style readback mismatch/);
 paragraph.textEveryLineComposer=true;
 expanded.textStyleRange[0].textStyle.fontPostScriptName='Wrong';
 assert.throws(()=>check(expanded,sparse),/style readback mismatch/);
 const wanted=clone(sparse);wanted.textKey='A猫';
 const native=clone(wanted);native.textStyleRange=[
  {from:0,to:1,textStyle:{fontPostScriptName:'MyriadPro-Regular'}},
  {from:1,to:3,textStyle:{fontPostScriptName:'AdobeHeitiStd-Regular',fontAvailable:true}}];
 const origins=[0,1,2];
 assert.equal(JSON.stringify(check(native,wanted,sparse,{origins})),JSON.stringify([[1,2,'AdobeHeitiStd-Regular']]));
 assert.throws(()=>check(native,wanted,sparse,{origins,typography:{fontPostScriptName:'MyriadPro-Regular',range:[1,2]}}),/style readback mismatch/);
 assert.throws(()=>check(native,wanted,wanted,{origins}),/style readback mismatch/);
 assert.equal(JSON.stringify(check(native,wanted,wanted,{origins,previous_fallbacks:[[1,2,'AdobeHeitiStd-Regular']]})),JSON.stringify([[1,2,'AdobeHeitiStd-Regular']]));
 native.textStyleRange[1].textStyle.tracking=99;
 assert.throws(()=>check(native,wanted,sparse,{origins}),/style readback mismatch/);
}
const original={layerID:11,name:'mixed',layerKind:3,layerLocking:{protectAll:false},textKey:{_obj:'textLayer',textKey:'A猫😀\rBC猫',
 textStyleRange:[{_obj:'textStyleRange',from:0,to:2,textStyle:{fontPostScriptName:'Alpha',size:12,color:{red:1}}},{_obj:'textStyleRange',from:2,to:5,textStyle:{fontPostScriptName:'Beta',size:16}},{_obj:'textStyleRange',from:5,to:9,textStyle:{fontPostScriptName:'Gamma',size:20}}],
 paragraphStyleRange:[{_obj:'paragraphStyleRange',from:0,to:5,paragraphStyle:{align:'left'}},{_obj:'paragraphStyleRange',from:5,to:9,paragraphStyle:{align:'center'}}],
 antiAlias:{_value:'antiAliasCrisp'},textClickPoint:{x:1,y:2},orientation:'horizontal',textShape:[{char:{_value:'paint'},transform:{xx:1,yy:1}}],warp:{style:'none'},kerningRange:[]}};
let row=clone(original),target={session:'s',document_id:1,active_layer_id:11,layer_ids:[11]},before,history=0,commits=0,writes=0,failWrite=false,wrongStyle=false,rollbackFail=false,materializeParagraphs=false,wrongXmp=false,fallbackGlyph=false;
let sceneHistory=72, savedProof=null, proofSaveError=false, normalizeFontTags=false, changeSmartHistory=false;
const ps={app:{activeDocument:{resolution:72}},core:{getActiveTool:async()=>({classID:"moveTool",isModal:false}),executeAsModal:async fn=>fn({isCancelled:false,hostControl:{suspendHistory:async()=>{history++;before=clone(row);return 'h';},resumeHistory:async(_,commit)=>{if(commit)commits++;else{if(rollbackFail)throw Error('rollback');row=before;}}}})},action:{batchPlay:async commands=>commands.map(c=>{
 assert.equal(c._options.dialogOptions,'silent');
 if(c._obj==='get'&&c._target[0]._ref==='historyState')return {ID:sceneHistory};
 if(c._obj==='hide'||c._obj==='show'){
  const id=c._target[0]._id,roots=ps.app.activeDocument.layers;
  const layer=[...roots,...roots[0].layers].find(l=>l.id===id);
  assert.ok(layer);
  layer.visible=c._obj==='show';
  return {};
 }
 if(c._obj==='multiGet')return clone(row);
 if(c._obj==='get'&&c._target[0]._property==='XMPMetadataAsUTF8')return {XMPMetadataAsUTF8:row.XMPMetadataAsUTF8||''};
 if(c._obj==='get'&&c._target[0]._property==='smartObject'){
  if(changeSmartHistory)sceneHistory++;
  return {smartObject:row.smartObject};
 }
 if(c._obj==='get'&&c._target[0]._property==='smartObjectMore')return {smartObjectMore:row.smartObjectMore};
 if(c._obj==='set'&&c.to?._obj==='layer'){assert.equal(c._target[0]._property,'XMPMetadataAsUTF8');if(!wrongXmp)row.XMPMetadataAsUTF8=c.to.XMPMetadataAsUTF8;return {};}
 if(c._obj==='set'){
  assert.equal(c._target.length,1);assert.equal(c._target[0]._ref,'textLayer');assert.equal(c._target[0]._value,'targetEnum');
  writes++;if(failWrite)return {_obj:'error'};
  row.textKey={...row.textKey,antiAlias:{_value:'antiAliasSharp'},...clone(c.to)};
  if(c.to.kerningRange)row.textKey.kerningRange.sort((a,b)=>a.from-b.from);
  if(!c.to.paragraphStyleRange)row.textKey.paragraphStyleRange=[{...clone(row.textKey.paragraphStyleRange[0]),from:0,to:row.textKey.textKey.length+1}];
  if(materializeParagraphs && c.to.paragraphStyleRange)for(const range of row.textKey.paragraphStyleRange)range.paragraphStyle={align:'left',hyphenate:false,...range.paragraphStyle};
  if(normalizeFontTags)for(const {textStyle} of row.textKey.textStyleRange)if(textStyle.fontPostScriptName==='Sarasa-Gothic-SC-Regular'){
   textStyle.fontScript=25;textStyle.fontTechnology=2;
  }
  if(wrongStyle)row.textKey.textStyleRange[0].textStyle.fontPostScriptName='Wrong';
  if(fallbackGlyph){const run=row.textKey.textStyleRange[0];row.textKey.textStyleRange=[{...clone(run),to:1,textStyle:{...run.textStyle,fontPostScriptName:'AvailableFallback',fontScript:25,fontTechnology:2,fontAvailable:true}},...row.textKey.textStyleRange.map((r,i)=>i===0?{...r,from:1}:r)];}
  return {};
 }
 throw Error(c._obj);
})}};
const context={ps,session:'s',capture:async()=>clone(target),options:{
 hostVersion:'24.0.1',saveProof:async({id},doc)=>{
  if(proofSaveError)throw Error('save failed');
  savedProof={id,doc:doc.id,row:clone(row),layers:clone(doc.layers||[])};
 }
}};
vm.createContext(context);vm.runInContext(source.slice(source.lastIndexOf('function createTextTools('))+'\nthis.tools=createTextTools(ps,session,capture,null,options);',context);
const tools=context.tools;
async function request(find,replacement){const read=await tools.read();return {target:read.target,text:{revision:read.revision,find,replacement},expires_at_ms:Date.now()+5000};}
(async()=>{
 ps.constants={DocumentMode:{RGB:'rgb'},BitsPerChannelType:{EIGHT:8}};
 Object.assign(ps.app.activeDocument,{width:600,height:400,mode:'rgb',bitsPerChannel:8,colorProfileName:'sRGB IEC61966-2.1'});
 const metadata='<x:xmpmeta xmlns:source="https://15100px.menu/ns/text-object/1.0/" />';
 const smart={...target,kind:'smart',locked:false};
 row={layerID:11,layerKind:5,name:'embedded PDF',XMPMetadataAsUTF8:metadata,
  smartObject:{placed:{_value:'vectorData'},linked:false},smartObjectMore:{transform:[0,0,600,0,600,400,0,400]}};
 assert.equal(await tools.menuSource(smart),metadata);
 assert.equal(await tools.menuSource({...smart,layer_ids:[12]}),null);
 row.smartObject.linked=true;
 assert.equal(await tools.menuSource(smart),null);
 row.smartObject.linked=false;
 changeSmartHistory=true;
 assert.equal(await tools.menuSource(smart),null);
 changeSmartHistory=false;
 ps.app.activeDocument.width=6000;
 ps.app.activeDocument.height=4000;
 const largeObject=await tools.read(true);
 assert.equal(largeObject.object_proof,undefined);
 assert.deepEqual(clone(largeObject.object_canvas_size),[6000,4000]);
 Object.assign(ps.app.activeDocument,{width:600,height:400});
 row=clone(original);
 let preparedRequest=await request('猫','XY');
 const cells=[['项目','数量'],['Alpha','12']];
 const table=await tools.prepareObject(preparedRequest,{revision:preparedRequest.text.revision,cells});
 assert.equal(table.kind,'new-table');assert.deepEqual(clone(table.source.table),cells);
 assert.equal(table.source.canvas.width,220);assert.equal(table.source.canvas.height,64);
 await assert.rejects(tools.prepareObject(preparedRequest,{revision:preparedRequest.text.revision,cells:[['a'],[]]}),/单元格/);
 const styled={...preparedRequest,text:{revision:preparedRequest.text.revision,value:original.textKey.textKey,
   origins:Array.from({length:original.textKey.textKey.length+1},(_,i)=>i),count:0,typography:{tracking:-15,size:18,leading:24,spaceAfter:6}}};
 const styledSource=(await tools.prepareObject(styled,styled.text)).source.layer.text;
 assert.ok(styledSource.textStyleRange.every(r=>r.textStyle.tracking===-15&&r.textStyle.size._value===18&&r.textStyle.autoLeading===false));
 assert.deepEqual(Array.from(styledSource.textStyleRange,r=>r.textStyle.fontPostScriptName),['Alpha','Beta','Gamma']);
 assert.ok(styledSource.paragraphStyleRange.every(r=>r.paragraphStyle.spaceAfter._value===6));
 row.textKey.orientation={_value:'horizontal'};
 const extended=await request('猫','XY');
 const extendedPlan={...styled.text,revision:extended.text.revision,typography:{range:[0,2],align:'center',
   leading:'auto',baselineShift:-2,horizontalScale:85,verticalScale:110}};
 const extendedSource=(await tools.prepareObject(extended,extendedPlan)).source.layer.text;
 assert.equal(extendedSource.textStyleRange[0].textStyle.autoLeading,true);
 assert.equal(extendedSource.textStyleRange[0].textStyle.baselineShift._value,-2);
 assert.equal(extendedSource.textStyleRange[0].textStyle.horizontalScale,Math.fround(.85)*100);
 assert.equal(extendedSource.textStyleRange[0].textStyle.verticalScale,Math.fround(1.1)*100);
 assert.equal(extendedSource.paragraphStyleRange[0].paragraphStyle.align._value,'center');
 assert.equal(extendedSource.paragraphStyleRange.at(-1).paragraphStyle.align,'center');
 row.textKey.orientation=original.textKey.orientation;
 row.textKey.textStyleRange[0].textStyle.impliedFontSize={_unit:'pixelsUnit',_value:12};
 ps.app.activeDocument.resolution=300;
 const highDpi=await request('猫','XY');
 const selected={...styled.text,revision:highDpi.text.revision,typography:{range:[2,4],tracking:35,spaceAfter:6}};
 const selection=(await tools.prepareObject(highDpi,selected)).source.layer.text;
 assert.deepEqual(Array.from(selection.textStyleRange,r=>[r.from,r.to,r.textStyle.tracking??null]),[[0,2,null],[2,4,35],[4,5,null],[5,9,null]]);
 assert.ok(selection.paragraphStyleRange[0].paragraphStyle.spaceAfter);
 assert.equal(selection.paragraphStyleRange[1].paragraphStyle.spaceAfter,undefined);
 await assert.rejects(tools.prepareObject(highDpi,{...selected,typography:{range:[3,4],tracking:35}}),/选区/);
 const legacyParagraphs=clone(row.textKey.paragraphStyleRange);
 row.textKey.paragraphStyleRange=[{from:0,to:9,paragraphStyle:{align:'left'}}];
 const rangeRead=await request('猫','XY');
 const ranged={revision:rangeRead.text.revision,value:row.textKey.textKey,origins:[0,1,2,3,4,5,6,7,8],count:0,
  typography_runs:[{range:[0,1],fontPostScriptName:'First'},{range:[5,7],fontPostScriptName:'Second'},{range:[6,7],spaceAfter:9}]};
 const rangedText=(await tools.prepareObject(rangeRead,ranged)).source.layer.text;
 assert.deepEqual(Array.from(rangedText.textStyleRange,run=>[run.from,run.to,run.textStyle.fontPostScriptName]),
   [[0,1,'First'],[1,2,'Alpha'],[2,5,'Beta'],[5,7,'Second'],[7,9,'Gamma']]);
 assert.deepEqual(Array.from(rangedText.paragraphStyleRange,run=>[run.from,run.to,run.paragraphStyle.spaceAfter?._value??null]),
   [[0,5,null],[5,9,37.5]]);
 await assert.rejects(tools.prepareObject(rangeRead,{...ranged,typography:{size:12}}),/多范围/);
 row.textKey.paragraphStyleRange=legacyParagraphs;
 const restoredRead=await request('猫','XY');
 const highDpiSource=(await tools.prepareObject(restoredRead,{...styled.text,revision:restoredRead.text.revision})).source.layer.text;
 assert.equal(highDpiSource.textStyleRange[0].textStyle.impliedFontSize,undefined);
 assert.deepEqual(clone(highDpiSource.textStyleRange[0].textStyle.leading),{_unit:'pixelsUnit',_value:100});
 assert.equal(highDpiSource.paragraphStyleRange[0].paragraphStyle.spaceAfter._value,25);
 delete row.textKey.textStyleRange[0].textStyle.impliedFontSize;
 ps.app.activeDocument.resolution=72;
 preparedRequest=await request('猫','XY');
 await assert.rejects(tools.prepareObject(preparedRequest,{...styled.text,revision:preparedRequest.text.revision,typography:{tracking:1001}}),/字距/);
 const nativeSource=await tools.prepareObject(preparedRequest,preparedRequest.text);
 assert.equal(nativeSource.source.schema,1);assert.equal(nativeSource.source.renderer,undefined);
 assert.equal(nativeSource.source.layer.text.textKey,'AXY😀\rBCXY');
 assert.equal(nativeSource.source.layer.text.textStyleRange[1].textStyle.fontPostScriptName,'Beta');
 assert.deepEqual(clone(nativeSource.source.layer.text.textShape),original.textKey.textShape);assert.equal(writes,0);
 await assert.rejects(tools.prepareObject(preparedRequest,{...preparedRequest.text,list_xmp:'new list options'}),/矢量列表缺少排版数据/);
 row=clone(original);
 row.textKey.orientation={_value:'horizontal'};row.textKey.warp={warpStyle:{_value:'warpNone'}};
 row.textKey.textShape=[{_obj:'textShape',char:{_value:'box'},bounds:{_obj:'rectangle',left:0,top:0,right:320,bottom:240}}];
 row.textKey.textClickPoint={horizontal:{_unit:'percentUnit',_value:10},vertical:{_unit:'percentUnit',_value:20}};
 let editable=await tools.read();assert.deepEqual(clone(editable.box_size),[320,240]);
 const resized={revision:editable.revision,value:editable.text,origins:Array.from({length:editable.text.length+1},(_,i)=>i),count:0,box_size:[175.5,144]};
 const resizedSource=(await tools.prepareObject({target:editable.target,expires_at_ms:Date.now()+5000},resized)).source.layer.text;
 assert.deepEqual(clone(resizedSource.textShape[0].bounds),{_obj:'rectangle',left:0,top:0,right:175.5,bottom:144});
 assert.equal(row.textKey.textShape[0].bounds.right,320);
 const shift=320-175.5;
 const shiftedBox=(await tools.prepareObject({target:editable.target,expires_at_ms:Date.now()+5000},{...resized,box_shift_x:shift})).source.layer.text;
 assert.equal(shiftedBox.textClickPoint.horizontal._value,Math.fround(row.textKey.textClickPoint.horizontal._value+100*shift/ps.app.activeDocument.width));
 assert.equal(shiftedBox.textShape[0].bounds.right+shift,320);
 await assert.rejects(tools.prepareObject({target:editable.target,expires_at_ms:Date.now()+5000},{...resized,box_shift_x:100001}),/无法移动左边缘/);
 await assert.rejects(tools.prepareObject({target:editable.target,expires_at_ms:Date.now()+5000},{...resized,box_size:[0,144]}),/无法调整尺寸/);
 assert.equal(writes,0);
 row.XMPMetadataAsUTF8='saved list options';preparedRequest=await request('猫','XY');
 assert.equal((await tools.prepareObject(preparedRequest,{...preparedRequest.text,list_xmp:'saved list options'})).source.schema,1);
 row.layerKind=5;row.smartObjectMore={transform:[0,0,600,0,600,400,0,400]};
 let objectRead=await tools.read();
 const objectParams={revision:objectRead.revision,find:'猫',replacement:'XY',object_render:'a'.repeat(64),object_source:{schema:1,canvas:nativeSource.source.canvas,layer:{text:clone(original.textKey),xmp:''}}};
 assert.equal((await tools.prepareObject({target,expires_at_ms:Date.now()+5000},objectParams)).source.schema,1);
 objectParams.object_source.schema=2;objectParams.object_source.renderer='list-v1';
 const legacySource=(await tools.prepareObject({target,expires_at_ms:Date.now()+5000},objectParams)).source;
 assert.equal(legacySource.schema,2);assert.equal(legacySource.renderer,'list-v1');assert.equal(writes,0);
 row=clone(original);
 let req=await request('猫','XY');assert.equal(writes,0);
 const result=await tools.replace(req);assert.equal(result.count,2);assert.equal(row.textKey.textKey,'AXY😀\rBCXY');assert.equal(history,1);assert.equal(commits,1);
 assert.deepEqual(row.textKey.textStyleRange.map(r=>[r.from,r.to,r.textStyle.fontPostScriptName]),[[0,3,'Alpha'],[3,6,'Beta'],[6,11,'Gamma']]);
 assert.deepEqual(row.textKey.paragraphStyleRange.map(r=>[r.from,r.to]),[[0,6],[6,11]]);
 assert.deepEqual(row.textKey.antiAlias,original.textKey.antiAlias);
 await assert.rejects(tools.replace(req),/已过期/);
 row=clone(original);row.textKey.textStyleRange.splice(1,0,clone(row.textKey.textStyleRange[0]));req=await request('猫','x');await tools.replace(req);assert.equal(row.textKey.textKey,'Ax😀\rBCx');
 row=clone(original);row.textKey.textStyleRange.splice(1,0,{...clone(row.textKey.textStyleRange[0]),textStyle:{fontPostScriptName:'Wrong',size:12}});await assert.rejects(tools.read(),/样式范围/);
 row=clone(original);row.textKey.textStyleRange[0].textStyle.styleSheetHasParent=true;const differentParent=clone(row.textKey.textStyleRange[0]);differentParent.textStyle.styleSheetHasParent=false;row.textKey.textStyleRange.splice(1,0,differentParent);await assert.rejects(tools.read(),/样式范围/);
 row=clone(original);materializeParagraphs=true;for(const range of row.textKey.paragraphStyleRange)range.paragraphStyle={styleSheetHasParent:true,textComposerEngine:{_value:'textOptycaComposer'}};
 req=await request('猫','x');await tools.replace(req);assert.ok(row.textKey.paragraphStyleRange.every(r=>!('align' in r.paragraphStyle)));materializeParagraphs=false;
 row=clone(original);req=await request('A猫😀','');await tools.replace(req);assert.equal(row.textKey.textKey,'\rBC猫');assert.equal(row.textKey.textStyleRange[0].textStyle.fontPostScriptName,'Beta');
 row=clone(original);req=await request('\n',' ');await tools.replace(req);assert.equal(row.textKey.paragraphStyleRange.length,1);assert.equal(row.textKey.paragraphStyleRange[0].paragraphStyle.align,'left');
 row=clone(original);req=await request('BC猫','尾');await tools.replace(req);assert.equal(row.textKey.textStyleRange.at(-1).textStyle.fontPostScriptName,'Gamma');
 row=clone(original);req=await request('x','y');const oldHistory=history;await tools.replace(req);assert.equal(history,oldHistory);
 req=await request('猫','猫');await tools.replace(req);assert.equal(history,oldHistory);
 req=await request('猫','x');row.textKey.textStyleRange[0].textStyle.size=50;const oldWrites=writes;await assert.rejects(tools.replace(req),/样式已改变/);assert.equal(writes,oldWrites);
 row=clone(original);req=await request('猫','x');target.layer_ids=[22];await assert.rejects(tools.replace(req),/单选|改变/);target.layer_ids=[11];
 row=clone(original);req=await request('猫','x');failWrite=true;await assert.rejects(tools.replace(req),/写入失败/);assert.deepEqual(row,original);failWrite=false;
 row=clone(original);req=await request('猫','x');wrongStyle=true;await assert.rejects(tools.replace(req),/读回不一致/);assert.deepEqual(row,original);wrongStyle=false;
 req=await request('猫','x');failWrite=true;rollbackFail=true;await assert.rejects(tools.replace(req),error=>error.resultUnknown===true);failWrite=false;rollbackFail=false;
 row=clone(original);row.textKey.textStyleRange[0].textStyle.fontAvailable=false;await assert.rejects(tools.read(),/缺失字体/);
 row=clone(original);row.textKey.kerningRange=[{from:0,to:1}];await assert.rejects(tools.read(),/字偶样式范围/);
 row=clone(original);row.textKey.textStyleRange[1].from=3;await assert.rejects(tools.read(),/样式范围/);
 row=clone(original);req=await request('猫','x');req.expires_at_ms=0;await assert.rejects(tools.replace(req),/已取消/);
 row=clone(original);let read=await tools.read();
 normalizeFontTags=true;
 const fontChange={target:read.target,expires_at_ms:Date.now()+5000,text:{revision:read.revision,
   value:row.textKey.textKey,origins:Array.from({length:row.textKey.textKey.length+1},(_,i)=>i),count:0,
   typography:{fontPostScriptName:'Sarasa-Gothic-SC-Regular',range:[1,2]}}};
 await tools.replace(fontChange);
 assert.deepEqual(row.textKey.textStyleRange.slice(0,2).map(r=>[r.from,r.to,r.textStyle.fontPostScriptName,r.textStyle.size]),
   [[0,1,'Alpha',12],[1,2,'Sarasa-Gothic-SC-Regular',12]]);
 row=clone(original);read=await tools.read();fontChange.text.revision=read.revision;wrongStyle=true;
 await assert.rejects(tools.replace(fontChange),/读回不一致/);assert.deepEqual(row,original);
 wrongStyle=normalizeFontTags=false;read=await tools.read();
 let mapped={target:read.target,text:{revision:read.revision,value:'X猫😀\rBC猫',origins:[0,1,2,3,4,5,6,7,8],count:1},expires_at_ms:Date.now()+5000};
 await tools.replace(mapped);assert.equal(row.textKey.textKey,'X猫😀\rBC猫');assert.equal(row.textKey.textStyleRange[1].textStyle.fontPostScriptName,'Beta');
 row=clone(original);read=await tools.read();mapped.text.revision=read.revision;mapped.text.origins=[1,0,2,3,4,5,6,7,8];const pre=writes;await assert.rejects(tools.replace(mapped),/计划无效/);assert.equal(writes,pre);
 row=clone(original);row.XMPMetadataAsUTF8='foreign';read=await tools.read();assert.equal(read.list_xmp,'foreign');
 const tagged={target:read.target,text:{revision:read.revision,value:row.textKey.textKey,origins:Array.from({length:row.textKey.textKey.length+1},(_,i)=>i),count:0,list_xmp:'foreign + list options'},expires_at_ms:Date.now()+5000};
 const taggedHistory=history;await tools.replace(tagged);assert.equal(history,taggedHistory+1);assert.equal(row.XMPMetadataAsUTF8,'foreign + list options');assert.equal(row.textKey.textKey,original.textKey.textKey);
 row=clone(original);read=await tools.read();fallbackGlyph=true;
 const bullet={target:read.target,text:{revision:read.revision,value:'◦ '+original.textKey.textKey,origins:[0,0,...Array.from({length:original.textKey.textKey.length+1},(_,i)=>i)],count:1},expires_at_ms:Date.now()+5000};
 await tools.replace(bullet);assert.equal(row.textKey.textStyleRange[0].textStyle.fontPostScriptName,'AvailableFallback');
 row=clone(original);read=await tools.read();bullet.text.revision=read.revision;wrongStyle=true;await assert.rejects(tools.replace(bullet),/读回不一致/);assert.deepEqual(row,original);wrongStyle=false;fallbackGlyph=false;
 row=clone(original);row.XMPMetadataAsUTF8='foreign + list options';read=await tools.read();tagged.text.revision=read.revision;
 read=await tools.read();tagged.text.revision=read.revision;tagged.text.list_xmp='new';row.XMPMetadataAsUTF8='externally changed';const staleWrites=writes;await assert.rejects(tools.replace(tagged),/样式已改变/);assert.equal(writes,staleWrites);
 read=await tools.read();tagged.text.revision=read.revision;wrongXmp=true;const prior=clone(row);await assert.rejects(tools.replace(tagged),/列表标记读回/);assert.deepEqual(row,prior);wrongXmp=false;
 read=await tools.read();tagged.text.revision=read.revision;tagged.text.list_xmp='x'.repeat(131073);await assert.rejects(tools.replace(tagged),/标记数据无效/);
 row=clone(original);req=await request('猫','x');let nativeCommits=0;
 ps.core.getActiveTool=async()=>({classId:'typeCreateOrEditTool',isModal:nativeCommits===0});
 ps.core.endModalToolState=async commit=>{assert.equal(commit,true);nativeCommits++;};
 await tools.replace(req);assert.equal(nativeCommits,1);assert.equal(row.textKey.textKey,'Ax😀\rBCx');
 row=clone(original);req=await request('猫','x');nativeCommits=0;
 ps.core.getActiveTool=async()=>({classID:'moveTool',isModal:true});const protectedWrites=writes;
 await assert.rejects(tools.replace(req),/结束当前PS工具/);assert.equal(nativeCommits,0);assert.equal(writes,protectedWrites);
 ps.core.getActiveTool=async()=>({classID:'typeVerticalCreateOrEditTool',isModal:nativeCommits===0});
 row=clone(original);req=await request('猫','x');row.textKey.textStyleRange[0].textStyle.size=30;
 await assert.rejects(tools.replace(req),/样式已改变/);assert.equal(nativeCommits,0);
 row=clone(original);req=await request('猫','x');ps.core.endModalToolState=async()=>{nativeCommits++;row.name='latest native edit';};
 await assert.rejects(tools.replace(req),/最新内容已保留/);assert.equal(row.name,'latest native edit');assert.equal(writes,protectedWrites);
 row=clone(original);req=await request('猫','x');nativeCommits=0;let toolReads=0;ps.core.getActiveTool=async()=>({classID:++toolReads===1?'typeCreateOrEditTool':'moveTool',isModal:true});await assert.rejects(tools.replace(req),/状态已改变/);assert.equal(nativeCommits,0);ps.core.getActiveTool=async()=>({classID:'typeVerticalCreateOrEditTool',isModal:true});
 row=clone(original);req=await request('猫','x');nativeCommits=0;ps.core.endModalToolState=async()=>{throw Error('unconfirmed');};
 await assert.rejects(tools.replace(req),error=>error.resultUnknown===true);assert.equal(writes,protectedWrites);
 row=clone(original);
 Object.assign(ps.app.activeDocument,{id:1,saved:true,historyStates:[{id:sceneHistory}]});
 ps.app.documents=[ps.app.activeDocument];
 ps.core.getActiveTool=async()=>({classID:'moveTool',isModal:false});
 req=await request('猫','XY');
 const proof={...req,command:'text.proof',id:'frame'};
 const priorCommits=commits;
 const resultProof=await tools.replace(proof);
 assert.equal(resultProof.restored,true);
 assert.equal(resultProof.history,sceneHistory);
 assert.equal(savedProof.id,'frame');
 assert.equal(savedProof.doc,1);
 assert.deepEqual(row,original);
 assert.equal(commits,priorCommits);
 ps.constants.LayerKind={GROUP:'group'};
 ps.app.activeDocument.layers=[{id:7,visible:true,kind:'group',layers:[{id:11,visible:true},{id:19,visible:true}]},
   {id:23,visible:true},{id:24,visible:false}];
 const initialVisibility=clone(ps.app.activeDocument.layers);
 const isolated={...proof,proof_isolated:true};
 assert.equal((await tools.replace(isolated)).isolated,true);
 assert.deepEqual(savedProof.layers.map(l=>l.visible),[true,false,false]);
 assert.deepEqual(savedProof.layers[0].layers.map(l=>l.visible),[true,false]);
 assert.deepEqual(ps.app.activeDocument.layers,initialVisibility);
 assert.deepEqual(row,original);
 proofSaveError=true;
 await assert.rejects(tools.replace(isolated),/save failed/);
 proofSaveError=false;
 assert.deepEqual(row,original);
 assert.deepEqual(ps.app.activeDocument.layers,initialVisibility);
 sceneHistory++;
 await assert.rejects(tools.replace({...req,proof_history:resultProof.history}),/画布自预览后已改变/);
 assert.equal(commits,priorCommits);
 sceneHistory--;
 await tools.replace({...req,proof_history:resultProof.history});
 assert.deepEqual(row,savedProof.row);
 assert.equal(commits,priorCommits+1);
 row=clone(original);
 row.textKey.textShape[0].char._value='box';
 row.textKey.orientation={_value:'horizontal'};
 row.textKey.warp={warpStyle:{_value:'warpNone'}};
 const indentOriginal=clone(row);
 ps.app.activeDocument.resolution=300;
 read=await tools.read();
 assert.equal(read.paragraph_indents_supported,true);
 const indent={target:read.target,command:'text.proof',id:'indent-frame',expires_at_ms:Date.now()+5000,
  text:{revision:read.revision,value:row.textKey.textKey,origins:Array.from({length:9},(_,i)=>i),count:0,
   typography:{range:[2,4],startIndent:12,endIndent:6,firstLineIndent:-3}}};
 const vectorIndent=await tools.prepareObject(indent,indent.text);
 assert.equal(vectorIndent.source.layer.text.paragraphStyleRange[0].paragraphStyle.firstLineIndent._value,-12.5);
 const composition={...indent.text};
 delete composition.typography;
 composition.typography_runs=[{range:[5,6],textEveryLineComposer:false,hangingRoman:true,
   hyphenateWordSize:8,justifyWord:[80,100,125],justifyLetter:[-5,0,10],justifyGlyph:[95,100,105]}];
 const compositionSource=(await tools.prepareObject(indent,composition)).source.layer.text;
 assert.equal(compositionSource.paragraphStyleRange[0].paragraphStyle.textEveryLineComposer,undefined);
 const composedParagraph=compositionSource.paragraphStyleRange[1].paragraphStyle;
 assert.equal(composedParagraph.textEveryLineComposer,false);
 assert.equal(composedParagraph.justificationWordMaximum,1.25);
 assert.equal(composedParagraph.hyphenateWordSize,8);
 await tools.replace(indent);
 assert.deepEqual(row,indentOriginal);
 const projected=savedProof.row.textKey;
 assert.deepEqual(projected.textStyleRange,indentOriginal.textKey.textStyleRange);
 assert.deepEqual(projected.paragraphStyleRange[0].paragraphStyle,
  {align:'left',startIndent:{_unit:'pixelsUnit',_value:50},endIndent:{_unit:'pixelsUnit',_value:25},firstLineIndent:{_unit:'pixelsUnit',_value:-12.5}});
 assert.deepEqual(projected.paragraphStyleRange[1],indentOriginal.textKey.paragraphStyleRange[1]);
 await tools.replace({...indent,command:'text.replace',proof_history:sceneHistory});
 assert.deepEqual(row,savedProof.row);
 read=await tools.read(true);
 assert.equal(read.canvas_history,sceneHistory);
 assert.equal(read.fonts_available,true);
 const advanced={...indent,text:{...indent.text,revision:read.revision,typography:{
   range:[2,4],align:'justifyRight',hyphenate:true,autoKern:'opticalKern',
   color:[38,121,209],fontCaps:'smallCaps',ligature:true,underline:'underlineOnLeftInVertical'
 }}};
 const sourcePlan=(await tools.prepareObject(advanced,advanced.text)).source.layer.text;
 assert.equal(sourcePlan.textStyleRange.find(run=>run.from===2).textStyle.autoKern._value,'opticalKern');
 assert.equal(sourcePlan.paragraphStyleRange[0].paragraphStyle.align._value,'justifyRight');
 assert.equal(sourcePlan.paragraphStyleRange[0].paragraphStyle.hyphenate,true);
 const afterIndent=clone(row);
 await tools.replace(advanced);
 assert.deepEqual(row,afterIndent);
 assert.deepEqual(savedProof.row.textKey.textStyleRange,clone(sourcePlan.textStyleRange));
 assert.deepEqual(savedProof.row.textKey.paragraphStyleRange[1],afterIndent.textKey.paragraphStyleRange[1]);
 read=await tools.read(true);
 const kern={...indent,command:'text.replace',text:{...indent.text,revision:read.revision,
   typography:{contextualLigatures:true,figureStyle:'tabularOldStyle',fractions:true},kerning:[[1,0],[2,-85]]}};
 await tools.replace(kern);
 assert.deepEqual(row.textKey.kerningRange.map(span=>[span.from,span.to,span.kerning]),[[0,1,0],[1,2,-85]]);
 assert.equal(row.textKey.textStyleRange[0].textStyle.autoKern._value,'manual');
 read=await tools.read(true);
 assert.equal(read.kerning_supported,true);
 const originalWithKerns=clone(row);
 const shifted={...kern,command:'text.proof',text:{revision:read.revision,value:'X'+row.textKey.textKey,
   origins:[0,0,1,2,3,4,5,6,7,8],count:1,kerning:[[2,null]]}};
 await tools.replace(shifted);
 assert.deepEqual(row,originalWithKerns);
 assert.deepEqual(savedProof.row.textKey.kerningRange.map(span=>[span.from,span.to,span.kerning]),[[2,3,-85]]);
 assert.equal(savedProof.row.textKey.textStyleRange[0].textStyle.autoKern._value,'metricsKern');
 console.log('Text tools: mixed styles, UTF-16, rollback, and shared draft proof and write passed');
})().catch(error=>{console.error(error);process.exitCode=1;});
