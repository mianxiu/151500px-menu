const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('uxp/offline-probe/labels.js','utf8'),clone=value=>JSON.parse(JSON.stringify(value));
let layers,active=11,serial=100,rollbackFail=false,failSet=false,history=0,commits=0,before,compactStyles=false,wrapWidth=0,materializeParagraphs=false,wrongParagraph=false,blankPrefixBounds=false,hiddenBoundsUnavailable=false,pointBounds=false,opacityRestoreFail=false,opacityWriteFail=false,baselineReadUnit='pixelsUnit',wrongBaseline=false;
const ink=(text,ranges=[])=>{let points=[],y=0,x=0,offset=0;for(const char of text){if(char==='\r'){y+=10;x=0;}else{const advance=wrapWidth&&char!==' '?2:1;if(wrapWidth&&x+advance>wrapWidth){x=0;y+=10;}const baseline=ranges.find(r=>r.from<=offset&&r.to>offset)?.textStyle.baselineShift;const shift=(baseline?._value||0)*(baseline?._unit==='pointsUnit'?ps.app.activeDocument.resolution/72:1);if(char!==' ')points.push([x,y-shift]);x+=advance;}offset+=char.length;}return {_obj:'boundingBox',...Object.fromEntries(['left','top','right','bottom'].map((key,index)=>[key,{_unit:'pixelsUnit',_value:points.length?(index===0?Math.min(...points.map(p=>p[0])):index===1?Math.min(...points.map(p=>p[1])):index===2?Math.max(...points.map(p=>p[0]))+1:Math.max(...points.map(p=>p[1]))+5):0}]))};};
const original={layerID:11,parentLayerID:7,name:'source',layerKind:3,layerLocking:{protectAll:false},visible:true,opacity:255,group:false,textKey:{_obj:'textLayer',textKey:'AA\rBB\rCC',
 textStyleRange:[{from:0,to:9,textStyle:{fontPostScriptName:'Font',size:14}}],paragraphStyleRange:[{from:0,to:9,paragraphStyle:{align:'left'}}],kerningRange:[],
 antiAlias:{_value:'antiAliasCrisp'},textShape:[{char:{_value:'paint'},transform:{xx:1,yy:1,xy:0,yx:0}}],warp:{warpStyle:{_value:'warpNone'}},textClickPoint:{horizontal:{_unit:'percentUnit',_value:10},vertical:{_unit:'percentUnit',_value:20}},boundingBox:ink('AA\rBB\rCC')}};
const reset=()=>{layers=[{layerID:10,parentLayerID:7},clone(original),{layerID:12,parentLayerID:7}];active=11;failSet=false;rollbackFail=false;};reset();
const capture=async()=>({session:'s',document_id:1,active_layer_id:active,layer_ids:[active]});
const tree=async()=>({order:layers.map(l=>l.layerID),rows:layers.map(l=>({layerID:l.layerID,parentLayerID:l.parentLayerID}))});
const ps={app:{activeDocument:{id:1,width:400,height:400,resolution:72}},core:{getActiveTool:async()=>({classID:"moveTool",isModal:false}),executeAsModal:async fn=>fn({isCancelled:false,hostControl:{suspendHistory:async()=>{history++;before={layers:clone(layers),active};return 'h';},resumeHistory:async(_,commit)=>{if(commit)commits++;else{if(rollbackFail)throw Error('rollback');layers=before.layers;active=before.active;if(opacityRestoreFail)layers.find(l=>l.layerID===active).opacity=0;}}}})},action:{batchPlay:async commands=>commands.map(c=>{
 if(c._obj==='get'&&c._target[0]._property==='XMPMetadataAsUTF8')return {XMPMetadataAsUTF8:layers.find(l=>l.layerID===(c._target[1]._id||active)).XMPMetadataAsUTF8||''};
 const ref=c._target?._ref||c._target,id=ref?.[0]?._id||active,row=layers.find(l=>l.layerID===id);
 if(c._obj==='multiGet'){if(!row)return {_obj:'error'};const result=Object.fromEntries(c.extendedReference[0].filter(key=>row[key]!==undefined).map(key=>[key,clone(row[key])]));if(result.textKey&&hiddenBoundsUnavailable&&!row.visible)delete result.textKey.boundingBox;if(result.textKey?.boundingBox&&pointBounds)for(const item of Object.values(result.textKey.boundingBox))if(item&&typeof item==='object'&&item._unit==='pixelsUnit'){item._unit='pointsUnit';item._value*=72/ps.app.activeDocument.resolution;}if(compactStyles&&result.textKey)for(const range of result.textKey.textStyleRange.slice(1)){delete range.textStyle.fill;delete range.textStyle.lineWidth;}return result;}
 if(c._obj==='duplicate'){const next=clone(row);next.layerID=++serial;layers.splice(layers.indexOf(row),0,next);active=next.layerID;return {};}
 if(c._obj==='set'&&c.to?._obj==='layer'){if(opacityWriteFail)return {_obj:'error'};assert.equal(c.to.opacity._unit,'percentUnit');row.opacity=Math.round(c.to.opacity._value*255/100);return {};}
 if(c._obj==='set'){if(failSet)return {_obj:'error'};assert.equal(c._target[0]._value,'targetEnum');row.textKey={...row.textKey,antiAlias:{_value:'antiAliasSharp'},...clone(c.to)};
  if(!c.to.paragraphStyleRange)row.textKey.paragraphStyleRange=[{...clone(row.textKey.paragraphStyleRange[0]),from:0,to:row.textKey.textKey.length+1}];
  if(materializeParagraphs && c.to.paragraphStyleRange)for(const range of row.textKey.paragraphStyleRange)range.paragraphStyle={align:'left',hyphenate:false,...range.paragraphStyle};
  if(wrongParagraph)row.textKey.paragraphStyleRange[0].paragraphStyle.align='right';
  for(const range of row.textKey.textStyleRange){const b=range.textStyle.baselineShift;if(b&&['pixelsUnit','pointsUnit'].includes(b._unit)&&b._unit!==baselineReadUnit)range.textStyle.baselineShift={_unit:baselineReadUnit,_value:b._value*(baselineReadUnit==='pixelsUnit'?ps.app.activeDocument.resolution/72:72/ps.app.activeDocument.resolution)};if(wrongBaseline&&range.textStyle.baselineShift)range.textStyle.baselineShift._value+=.01;}row.textKey.boundingBox=ink(row.textKey.textKey,row.textKey.textStyleRange);if(blankPrefixBounds&&/^ +\r/.test(row.textKey.textKey))row.textKey.boundingBox.top._value-=3;return {};}
 if(c._obj==='hide'||c._obj==='show'){row.visible=c._obj==='show';return {};}
 if(c._obj==='select'){assert.equal(c.makeVisible,false);active=id;return {};}
 if(c._obj==='delete'){layers.splice(layers.indexOf(row),1);return {};}
 throw Error(c._obj);
})}};
Object.defineProperty(ps.app.activeDocument,'activeLayers',{get:()=>[{id:active,opacity:layers.find(l=>l.layerID===active).opacity*100/255}]});
const context={ps,session:'s',capture,tree};vm.createContext(context);vm.runInContext(source.slice(source.lastIndexOf('function createTextTools('))+'\nthis.tools=createTextTools(ps,session,capture,tree);',context);
const tools=context.tools;
async function request(breaks=[3,6,8]){const read=await tools.read();return {target:read.target,text:{revision:read.revision,breaks},expires_at_ms:Date.now()+5000};}
(async()=>{
 let result=await tools.split(await request());assert.equal(result.count,3);assert.equal(history,1);assert.equal(commits,1);
 assert.deepEqual(layers.slice(1,-1).map(l=>l.textKey.textKey),['AA','BB','CC']);assert.deepEqual(Array.from(result.positions,r=>r.dy),[0,10,20]);assert.ok(layers.slice(1,-1).every(l=>l.parentLayerID===7&&l.visible));
 assert.ok(layers.slice(1,-1).every(l=>l.textKey.antiAlias._value==='antiAliasCrisp'));
 reset();layers[1].textKey.textStyleRange.push(clone(layers[1].textKey.textStyleRange[0]));
 result=await tools.split(await request());assert.equal(result.count,3);
 reset();materializeParagraphs=true;layers[1].textKey.paragraphStyleRange=[{from:0,to:3,paragraphStyle:{styleSheetHasParent:true,textComposerEngine:{_value:'textOptycaComposer'}}},{from:3,to:9,paragraphStyle:{styleSheetHasParent:true,textComposerEngine:{_value:'textOptycaComposer'}}}];
 result=await tools.split(await request());assert.equal(result.count,3);assert.ok(layers.slice(1,-1).every(l=>l.textKey.paragraphStyleRange.every(r=>!('align' in r.paragraphStyle))));materializeParagraphs=false;
 reset();wrongParagraph=true;const changedParagraph=await request();await assert.rejects(tools.split(changedParagraph),/文字样式改变/);assert.deepEqual(layers[1],original);wrongParagraph=false;
 reset();layers[1].textKey.paragraphStyleRange=[{from:0,to:3,paragraphStyle:{align:'left'}},{from:3,to:9,paragraphStyle:{align:'center'}}];
 result=await tools.split(await request());assert.deepEqual(layers.slice(1,-1).map(l=>l.textKey.paragraphStyleRange[0].paragraphStyle.align),['left','center','center']);
 reset();result=await tools.split(await request([6,8]));assert.deepEqual(layers.slice(1,-1).map(l=>l.textKey.textKey),['AA\rBB','CC']);
 reset();let req=await request([2,8]);const oldHistory=history;await assert.rejects(tools.split(req),/分组必须/);assert.equal(history,oldHistory);
 reset();req=await request();failSet=true;await assert.rejects(tools.split(req),/写入失败/);assert.deepEqual(layers.map(l=>l.layerID),[10,11,12]);assert.deepEqual(layers[1],original);
 reset();req=await request();failSet=true;rollbackFail=true;await assert.rejects(tools.split(req),error=>error.resultUnknown===true);
 reset();layers[1].textKey.warp.warpStyle._value='warpArc';req=await request();await assert.rejects(tools.split(req),/路径或变形/);
 reset();layers[1].layerEffects={dropShadow:{enabled:true}};req=await request();await assert.rejects(tools.split(req),/图层效果/);
 reset();layers[1].visible=false;result=await tools.split(await request());assert.ok(layers.slice(1,-1).every(l=>!l.visible));
 reset();layers[1].textKey.textStyleRange=[{from:0,to:3,textStyle:{fontPostScriptName:'Font',size:14,baseParentStyle:{fill:true,lineWidth:1}}},{from:3,to:9,textStyle:{fontPostScriptName:'OtherFont',size:18}}];
 result=await tools.split(await request());
 assert.ok(layers.slice(1,-1).every(l=>l.textKey.textStyleRange[0].textStyle.fill===true&&l.textKey.textStyleRange[0].textStyle.lineWidth===1));
 assert.equal(layers[2].textKey.textStyleRange[0].textStyle.fontPostScriptName,'OtherFont');
 reset();compactStyles=true;layers[1].textKey.textStyleRange=[{from:0,to:3,textStyle:{fontPostScriptName:'Font',size:14,baseParentStyle:{fill:true,lineWidth:1}}},{from:3,to:9,textStyle:{fontPostScriptName:'OtherFont',size:18}}];
 result=await tools.split(await request([6,8]));assert.equal(result.count,2);assert.equal(layers[1].textKey.textStyleRange[1].textStyle.fontPostScriptName,'OtherFont');compactStyles=false;
 reset();wrapWidth=6;const raw=layers[1].textKey;raw.textKey='AAAA\rBB\rCC';raw.textShape[0].char._value='box';raw.orientation={_value:'horizontal'};raw.textStyleRange[0].to=11;raw.paragraphStyleRange[0].to=11;raw.boundingBox=ink(raw.textKey);
 result=await tools.split(await request([5,8,10]));assert.deepEqual(Array.from(result.positions,r=>r.dy),[0,20,30]);assert.ok(layers.slice(1,-1).every(l=>l.textKey.textStyleRange.every(r=>!r.textStyle.baselineShift)));wrapWidth=0;
 reset();wrapWidth=6;Object.assign(layers[1].textKey,{...clone(raw),boundingBox:{...clone(raw.boundingBox),bottom:{_unit:'pixelsUnit',_value:25}}});const overflow=clone(layers[1]);
 await assert.rejects(tools.split(await request([5,10])),/溢出文本框/);assert.deepEqual(layers[1],overflow);assert.deepEqual(layers.map(l=>l.layerID),[10,11,12]);wrapWidth=0;
 reset();layers[1].textKey.textKey='AA,BB';layers[1].textKey.textStyleRange[0].to=6;layers[1].textKey.paragraphStyleRange[0].to=6;layers[1].textKey.boundingBox=ink('AA,BB');
 let prepared=await request([4,6]);prepared.text={...prepared.text,value:'AA,\rBB',origins:[0,1,2,3,3,4,4],count:2,layout:'lines'};const preparedHistory=history;result=await tools.split(prepared);assert.equal(history,preparedHistory+1);assert.deepEqual(layers.slice(1,-1).map(l=>l.textKey.textKey),['AA,','BB']);
 reset();blankPrefixBounds=true;const preserved=clone(layers);
 await assert.rejects(tools.split(await request()),/字形排版|保留位置.*逐行排列/);assert.deepEqual(layers,preserved);
 reset();let flow=await request();flow.text={...flow.text,value:'AA\rBB\rCC',origins:Array.from({length:9},(_,i)=>i),count:3,layout:'lines'};
 result=await tools.split(flow);assert.equal(result.count,3);assert.deepEqual(Array.from(result.positions,r=>r.dx),[0,0,0]);assert.deepEqual(Array.from(result.positions,r=>r.dy),[0,10,20]);blankPrefixBounds=false;
 reset();layers[1].textKey.orientation={_value:'vertical'};flow=await request();flow.text={...flow.text,value:'AA\rBB\rCC',origins:Array.from({length:9},(_,i)=>i),count:3,layout:'lines'};
 const verticalHistory=history;await assert.rejects(tools.split(flow),/逐行排列暂不支持竖排/);assert.equal(history,verticalHistory);
 reset();hiddenBoundsUnavailable=true;layers[1].visible=false;layers[1].opacity=93;
 flow=await request();flow.text={...flow.text,value:'AA\rBB\rCC',origins:Array.from({length:9},(_,i)=>i),count:3,layout:'lines'};
 result=await tools.split(flow);assert.equal(result.count,3);assert.ok(layers.slice(1,-1).every(l=>l.opacity===93&&!l.visible));hiddenBoundsUnavailable=false;
 reset();pointBounds=true;ps.app.activeDocument.resolution=300;result=await tools.split(await request());assert.deepEqual(Array.from(result.positions,r=>r.dy),[0,10,20]);pointBounds=false;ps.app.activeDocument.resolution=72;
 reset();opacityWriteFail=true;const opacitySource=clone(layers);await assert.rejects(tools.split(await request()),/写入失败/);assert.deepEqual(layers,opacitySource);opacityWriteFail=false;
 reset();failSet=true;opacityRestoreFail=true;await assert.rejects(tools.split(await request()),error=>error.resultUnknown===true);opacityRestoreFail=false;
 reset();prepared=await request();prepared.text={...prepared.text,value:'AX\rBY\rCZ',origins:[0,1,2,3,4,5,6,7,7],count:3,layout:'lines'};failSet=true;const sourceBefore=clone(layers);await assert.rejects(tools.split(prepared),/写入失败/);assert.deepEqual(layers,sourceBefore);
 for(const unit of ['pointsUnit','pixelsUnit'])for(const dpi of [72,300])for(const layout of ['original','lines']) {
  reset();baselineReadUnit=unit;ps.app.activeDocument.resolution=dpi;
  const raw=layers[1].textKey;raw.textShape[0].char._value='box';raw.orientation={_value:'horizontal'};
  raw.textStyleRange=[{from:0,to:3,textStyle:{fontPostScriptName:'Font',size:14,baselineShift:{_unit:'pointsUnit',_value:1.25}}},{from:3,to:6,textStyle:{fontPostScriptName:'Font',size:14,baselineShift:{_unit:'pixelsUnit',_value:-2.5}}},{from:6,to:9,textStyle:{fontPostScriptName:'Font',size:14,baselineShift:{_unit:'pointsUnit',_value:0}}}];
  raw.boundingBox=ink(raw.textKey,raw.textStyleRange);
  const req=await request();if(layout==='lines')Object.assign(req.text,{value:raw.textKey,origins:Array.from({length:9},(_,i)=>i),count:3,layout});
  result=await tools.split(req);assert.deepEqual(Array.from(result.positions,r=>r.dy),[0,10,20]);
  assert.deepEqual(layers.slice(1,-1).map(l=>{const b=l.textKey.textStyleRange[0].textStyle.baselineShift;return Math.round(b._value*(b._unit==='pointsUnit'?dpi/72:1)*1e6)/1e6;}),[Math.round(1.25*dpi/72*1e6)/1e6,-2.5,0]);
 }
 baselineReadUnit='pixelsUnit';ps.app.activeDocument.resolution=72;
 reset();layers[1].textKey.textShape[0].char._value='box';layers[1].textKey.orientation={_value:'horizontal'};layers[1].textKey.textStyleRange[0].textStyle.baselineShift={_unit:'percentUnit',_value:1};const invalidBaseline=clone(layers);const baselineCommits=commits;
 await assert.rejects(tools.split(await request()),/无法核对文字基线/);assert.deepEqual(layers,invalidBaseline);assert.equal(commits,baselineCommits);
 reset();layers[1].textKey.textStyleRange[0].textStyle.baselineShift={_unit:'pixelsUnit',_value:2};const baselineSource=clone(layers);wrongBaseline=true;
 await assert.rejects(tools.split(await request()),/文字样式改变/);assert.deepEqual(layers,baselineSource);wrongBaseline=false;
 console.log('Text split: order/parent/style/position, grouping, hidden state, bounds, rollback and unknown passed');
})().catch(error=>{console.error(error);process.exitCode=1;});
