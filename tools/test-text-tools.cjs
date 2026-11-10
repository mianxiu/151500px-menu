const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('uxp/offline-probe/labels.js','utf8');
const clone=value=>JSON.parse(JSON.stringify(value));
const original={layerID:11,name:'mixed',layerKind:3,layerLocking:{protectAll:false},textKey:{_obj:'textLayer',textKey:'A猫😀\rBC猫',
 textStyleRange:[{_obj:'textStyleRange',from:0,to:2,textStyle:{fontPostScriptName:'Alpha',size:12,color:{red:1}}},{_obj:'textStyleRange',from:2,to:5,textStyle:{fontPostScriptName:'Beta',size:16}},{_obj:'textStyleRange',from:5,to:9,textStyle:{fontPostScriptName:'Gamma',size:20}}],
 paragraphStyleRange:[{_obj:'paragraphStyleRange',from:0,to:5,paragraphStyle:{align:'left'}},{_obj:'paragraphStyleRange',from:5,to:9,paragraphStyle:{align:'center'}}],
 antiAlias:{_value:'antiAliasCrisp'},textClickPoint:{x:1,y:2},orientation:'horizontal',textShape:[{char:{_value:'paint'},transform:{xx:1,yy:1}}],warp:{style:'none'},kerningRange:[]}};
let row=clone(original),target={session:'s',document_id:1,active_layer_id:11,layer_ids:[11]},before,history=0,commits=0,writes=0,failWrite=false,wrongStyle=false,rollbackFail=false,materializeParagraphs=false,wrongXmp=false,fallbackGlyph=false;
const ps={app:{activeDocument:{resolution:72}},core:{getActiveTool:async()=>({classID:"moveTool",isModal:false}),executeAsModal:async fn=>fn({isCancelled:false,hostControl:{suspendHistory:async()=>{history++;before=clone(row);return 'h';},resumeHistory:async(_,commit)=>{if(commit)commits++;else{if(rollbackFail)throw Error('rollback');row=before;}}}})},action:{batchPlay:async commands=>commands.map(c=>{
 assert.equal(c._options.dialogOptions,'silent');
 if(c._obj==='multiGet')return clone(row);
 if(c._obj==='get'&&c._target[0]._property==='XMPMetadataAsUTF8')return {XMPMetadataAsUTF8:row.XMPMetadataAsUTF8||''};
 if(c._obj==='get'&&c._target[0]._property==='smartObjectMore')return {smartObjectMore:row.smartObjectMore};
 if(c._obj==='set'&&c.to?._obj==='layer'){assert.equal(c._target[0]._property,'XMPMetadataAsUTF8');if(!wrongXmp)row.XMPMetadataAsUTF8=c.to.XMPMetadataAsUTF8;return {};}
 if(c._obj==='set'){
  assert.equal(c._target.length,1);assert.equal(c._target[0]._ref,'textLayer');assert.equal(c._target[0]._value,'targetEnum');
  writes++;if(failWrite)return {_obj:'error'};
  row.textKey={...row.textKey,antiAlias:{_value:'antiAliasSharp'},...clone(c.to)};
  if(!c.to.paragraphStyleRange)row.textKey.paragraphStyleRange=[{...clone(row.textKey.paragraphStyleRange[0]),from:0,to:row.textKey.textKey.length+1}];
  if(materializeParagraphs && c.to.paragraphStyleRange)for(const range of row.textKey.paragraphStyleRange)range.paragraphStyle={align:'left',hyphenate:false,...range.paragraphStyle};
  if(wrongStyle)row.textKey.textStyleRange[0].textStyle.fontPostScriptName='Wrong';
  if(fallbackGlyph){const run=row.textKey.textStyleRange[0];row.textKey.textStyleRange=[{...clone(run),to:1,textStyle:{...run.textStyle,fontPostScriptName:'AvailableFallback',fontScript:25,fontTechnology:2,fontAvailable:true}},...row.textKey.textStyleRange.map((r,i)=>i===0?{...r,from:1}:r)];}
  return {};
 }
 throw Error(c._obj);
})}};
const context={ps,session:'s',capture:async()=>clone(target)};
vm.createContext(context);vm.runInContext(source.slice(source.lastIndexOf('function createTextTools('))+'\nthis.tools=createTextTools(ps,session,capture);',context);
const tools=context.tools;
async function request(find,replacement){const read=await tools.read();return {target:read.target,text:{revision:read.revision,find,replacement},expires_at_ms:Date.now()+5000};}
(async()=>{
 ps.constants={DocumentMode:{RGB:'rgb'},BitsPerChannelType:{EIGHT:8}};
 Object.assign(ps.app.activeDocument,{width:600,height:400,mode:'rgb',bitsPerChannel:8,colorProfileName:'sRGB IEC61966-2.1'});
 let preparedRequest=await request('猫','XY');
 const nativeSource=await tools.prepareObject(preparedRequest,preparedRequest.text);
 assert.equal(nativeSource.source.schema,1);assert.equal(nativeSource.source.renderer,undefined);
 assert.equal(nativeSource.source.layer.text.textKey,'AXY😀\rBCXY');
 assert.equal(nativeSource.source.layer.text.textStyleRange[1].textStyle.fontPostScriptName,'Beta');
 assert.deepEqual(clone(nativeSource.source.layer.text.textShape),original.textKey.textShape);assert.equal(writes,0);
 await assert.rejects(tools.prepareObject(preparedRequest,{...preparedRequest.text,list_xmp:'new list options'}),/先写回普通文字列表/);
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
 row=clone(original);row.textKey.kerningRange=[{from:0,to:1}];await assert.rejects(tools.read(),/手动字距/);
 row=clone(original);row.textKey.textStyleRange[1].from=3;await assert.rejects(tools.read(),/样式范围/);
 row=clone(original);req=await request('猫','x');req.expires_at_ms=0;await assert.rejects(tools.replace(req),/已取消/);
 row=clone(original);let read=await tools.read();
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
 console.log('Text tools: mixed styles, UTF-16, paragraph ranges, no-op, stale target, missing fonts, rollback and unknown passed');
})().catch(error=>{console.error(error);process.exitCode=1;});
