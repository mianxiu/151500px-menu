const assert=require('node:assert/strict');
const create=require('../uxp/offline-probe/labels.js');
const copy=v=>JSON.parse(JSON.stringify(v));
async function main(){
 const layer={id:30,kind:'solid',parent:null,vectorMaskFeather:0,vectorMaskDensity:100},doc={id:12,activeLayers:[layer],layers:[layer],mode:'rgb',bitsPerChannel:8,resolution:144};
 let fill={_obj:'solidColorLayer',color:{_obj:'RGBColor',red:12,grain:34,blue:56}};
 let stroke={_obj:'strokeStyle',strokeStyleVersion:2,strokeEnabled:false,fillEnabled:true,
  strokeStyleLineWidth:{_unit:'pointsUnit',_value:1},strokeStyleLineCapType:{_enum:'strokeStyleLineCapType',_value:'strokeStyleRoundCap'},
  strokeStyleContent:{_obj:'solidColorLayer',color:{_obj:'RGBColor',red:0,grain:0,blue:0}}};
 let gets=0,sets=0,history=0,rollbacks=0,vector=true,mismatch=false,rollbackFailure=false,saved;
 let missingStroke=false,materializedStroke=null;
 let maskFeather=0,maskDensity=100,maskReads=0,maskSetFailure=false;
 Object.defineProperties(layer,{
  vectorMaskFeather:{get(){maskReads++;return maskFeather;},set(value){maskFeather=value;}},
  vectorMaskDensity:{get(){maskReads++;return maskDensity;},set(value){if(maskSetFailure)throw Error('mask setter refused');maskDensity=value;}}
 });
 const ps={constants:{LayerKind:{SOLIDFILL:'solid',NORMAL:'pixel'},DocumentMode:{RGB:'rgb'},BitsPerChannelType:{EIGHT:8,SIXTEEN:16}},
 app:{documents:[doc],activeDocument:doc},action:{batchPlay:async commands=>commands.map(c=>{
  if(c._obj==='set'){
   if(vector && c.to._obj!=='shapeStyle')throw Error('Set unavailable on vector-backed fill');
   sets++;if(c.to._obj==='shapeStyle'){if(c.to.fillContents)fill=copy(c.to.fillContents);if(c.to.strokeStyle)stroke=copy(c.to.strokeStyle);}else fill=copy(c.to);
   if(materializedStroke){stroke=copy(materializedStroke);missingStroke=false;}if(mismatch==='armed')mismatch=true;return {};
  }
  gets++;const key=c._target[0]._property;
  if(key==='layerKind')return {layerKind:4};
  if(key==='hasVectorMask')return {hasVectorMask:vector};
  if(key==='adjustment')return {adjustment:[mismatch===true?{...fill,color:{_obj:'RGBColor',red:99,grain:99,blue:99}}:copy(fill)]};
  if(key==='AGMStrokeStyleInfo')return missingStroke?{}:{AGMStrokeStyleInfo:copy(stroke)};
  if(key==='presetManager')return {presetManager:[{_obj:'PttR',name:['Loaded Pattern']}]};
  throw Error('Unexpected get '+key);
 })},core:{executeAsModal:async fn=>fn({hostControl:{suspendHistory:async()=>{history++;saved={fill:copy(fill),stroke:copy(stroke),maskFeather,maskDensity,missingStroke};return 1;},
 resumeHistory:async(id,commit)=>{if(!commit){rollbacks++;if(rollbackFailure)throw Error('rollback failure');fill=saved.fill;stroke=saved.stroke;maskFeather=saved.maskFeather;maskDensity=saved.maskDensity;missingStroke=saved.missingStroke;}}}})}};
 const api=create(ps,'s'),target=await api.capture();
 const inspect=()=>api.observeShape(api.selectionContext(),true);
 let data=await inspect();assert.equal(data.stroke.width,2);assert.deepEqual(data.patterns,['Loaded Pattern']);assert.deepEqual(data.mask,{feather:0,density:100});
 const reads=gets;await api.observeShape(api.selectionContext());assert.equal(gets,reads);assert.equal(sets,0);assert.equal(history,0);
 const request=d=>({command:'shape.paint',target,expires_at_ms:Date.now()+10000,paint:{revision:d.revision,fill:copy(d.fill),stroke:copy(d.stroke)}});
 let r=request(data);r.paint.fill.rgb=[200,30,10];r.paint.stroke.enabled=true;
 assert.equal((await api.setLabel(r)).verified,true);assert.equal(sets,1);assert.equal(history,1);
 assert.equal(stroke.strokeStyleLineCapType._value,'strokeStyleRoundCap');
 await assert.rejects(api.setLabel(r),/已改变/);assert.equal(sets,1);assert.equal(history,1);
 data=await inspect();r=request(data);r.paint.mask={feather:8.5,density:62.5};
 // Parameter key order differs after backend normalization; mask-only edits
 // must not rewrite shapeStyle, colors, gradients or vector path data.
 r.paint.fill={kind:data.fill.kind,enabled:data.fill.enabled,rgb:copy(data.fill.rgb)};
 const maskOnlySets=sets;await api.setLabel(r);assert.equal(sets,maskOnlySets);
 assert.equal(layer.vectorMaskFeather,8.5);assert.equal(layer.vectorMaskDensity,62.5);
 data=await inspect();r=request(data);r.paint.mask={feather:10,density:20};layer.vectorMaskDensity=75;
 const maskHistory=history;await assert.rejects(api.setLabel(r),/已改变/);assert.equal(history,maskHistory);assert.equal(layer.vectorMaskDensity,75);
 data=await inspect();for(const invalid of [null,{feather:-1,density:100},{feather:2,density:101},{feather:NaN,density:100}]){
  r=request(data);r.paint.mask=invalid;await assert.rejects(api.setLabel(r),/Invalid vector mask/);
 }
 r=request(data);r.paint.mask={feather:25,density:20};const oldMask=copy(data.mask);maskSetFailure=true;
 await assert.rejects(api.setLabel(r),/mask setter refused/);maskSetFailure=false;
 assert.deepEqual((await inspect()).mask,oldMask);assert.equal(rollbacks,1);
 const finalMaskHistory=history;
 data=await inspect();r=request(data);r.target={...target,document_id:99};await assert.rejects(api.setLabel(r));assert.equal(history,finalMaskHistory);
 data=await inspect();r=request(data);r.paint.fill={kind:'gradient',enabled:true,stops:[{rgb:[0,0,0],location:0},{rgb:[255,255,255],location:4096}]};
 await api.setLabel(r);assert.equal(fill._obj,'gradientLayer');assert.equal(fill.gradient.transparency.length,2);
 fill.angle._value=40;fill.gradient.colors[0].midpoint=37;fill.gradient.transparency[0].opacity._value=80;
 data=await inspect();r=request(data);r.paint.fill.stops[0].rgb=[0,20,40];await api.setLabel(r);
 assert.equal(fill.angle._value,40);assert.equal(fill.gradient.colors[0].midpoint,37);assert.equal(fill.gradient.transparency[0].opacity._value,80);
 data=await inspect();r=request(data);r.paint.fill.method='perceptual';await api.setLabel(r);
 assert.deepEqual(fill.gradientsInterpolationMethod,{_enum:'gradientInterpolationMethodType',_value:'perceptual'});
 data=await inspect();assert.equal(data.fill.method,'perceptual');
 const perceptualHistory=history;r=request(data);r.paint.fill.method='oklab';await assert.rejects(api.setLabel(r),/Invalid gradient interpolation/);assert.equal(history,perceptualHistory);
 r=request(data);r.paint.fill.method='linear';fill.gradientsInterpolationMethod._value='classic';
 await assert.rejects(api.setLabel(r),/已改变/);assert.equal(history,perceptualHistory);
 data=await inspect();r=request(data);r.paint.fill.method='linear';await api.setLabel(r);assert.equal(fill.gradientsInterpolationMethod._value,'linear');
 assert.equal(fill.angle._value,40);assert.equal(fill.gradient.colors[0].midpoint,37);assert.equal(fill.gradient.transparency[0].opacity._value,80);
 data=await inspect();r=request(data);r.paint.fill={kind:'pattern',enabled:true,name:'Loaded Pattern'};await api.setLabel(r);assert.equal(fill.pattern.name,'Loaded Pattern');
 data=await inspect();r=request(data);r.paint.fill.name='Removed Pattern';const before=history;await assert.rejects(api.setLabel(r),/图案预设/);assert.equal(history,before);
 layer.allLocked=true;assert.equal((await inspect()).available,false);layer.allLocked=false;
 // A CMYK-to-RGB conversion can retain normalized out-of-gamut RGB, even in an 8-bit document.
 const beforeConverted=copy(fill);
 fill={_obj:'solidColorLayer',color:{_obj:'RGBColor',redFloat:-.2,greenFloat:.619,blueFloat:1.1}};
 data=await inspect();assert.equal(data.available,true);assert.deepEqual(data.fill.rgb,[0,158,255]);
 const floatFill=copy(fill),unchangedSets=sets;
 await api.setLabel(request(data));assert.deepEqual(fill,floatFill);assert.equal(sets,unchangedSets);
 data=await inspect();r=request(data);r.paint.fill.rgb=[10,30,80];await api.setLabel(r);
 assert.deepEqual((await inspect()).fill.rgb,[10,30,80]);
 for(const bad of [{redFloat:NaN,greenFloat:0,blueFloat:0},{redFloat:0,greenFloat:0},{redFloat:0,greenFloat:0,blueFloat:'1'}]) {
  fill={_obj:'solidColorLayer',color:{_obj:'RGBColor',...bad}};assert.equal((await inspect()).available,false);
 }
 fill=beforeConverted;
 const vectorFill=copy(fill),vectorStroke=copy(stroke);
 vector=false;fill={_obj:'solidColorLayer',color:{_obj:'RGBColor',red:12,grain:34,blue:56}};
 const maskReadsBeforeFill=maskReads;data=await inspect();assert.equal(data.available,true);assert.equal(data.vector,false);assert.equal(maskReads,maskReadsBeforeFill);
 assert.equal(data.mask,undefined);r=request(data);r.paint.mask={feather:0,density:100};await assert.rejects(api.setLabel(r),/不支持矢量蒙版/);
 r=request(data);r.paint.stroke={width:data.stroke.width,rgb:copy(data.stroke.rgb),enabled:data.stroke.enabled,kind:data.stroke.kind};
 r.paint.fill={kind:'solid',enabled:true,rgb:[30,60,90]};await api.setLabel(r);
 assert.deepEqual(fill.color,{_obj:'RGBColor',red:30,grain:60,blue:90});assert.deepEqual(stroke,vectorStroke);
 data=await inspect();r=request(data);r.paint.stroke.enabled=true;const fillSets=sets;
 await assert.rejects(api.setLabel(r),/普通填充层/);assert.equal(sets,fillSets);
 r=request(data);r.paint.fill.enabled=false;await assert.rejects(api.setLabel(r),/普通填充层/);assert.equal(sets,fillSets);
 r=request(data);r.paint.fill={kind:'pattern',enabled:true,name:'Loaded Pattern'};await assert.rejects(api.setLabel(r),/原生类型/);assert.equal(sets,fillSets);
 // Native menu JSON sorts object keys. Legacy ordinary-fill preparation must
 // compare exact values without treating key order as a changed stroke.
 const legacy=create(ps,'s',{legacyPreview:true});
 const legacyData=await legacy.observeShape(legacy.selectionContext(),true);
 const sorted=value=>JSON.parse(JSON.stringify(value,(_,v)=>v && !Array.isArray(v) && typeof v==='object'?Object.fromEntries(Object.entries(v).sort(([a],[b])=>a.localeCompare(b))):v));
 const legacyRequest=()=>({...request(legacyData),command:'shape.legacy.preview',paint:sorted({...request(legacyData).paint,fill:{...legacyData.fill,rgb:[90,80,70]}})});
 const beforeLegacySets=sets,beforeLegacyHistory=history;
 const prepared=await legacy.prepareLegacyShape(legacyRequest());
 assert.deepEqual(prepared.before.stroke,legacyData.stroke);
 assert.equal(sets,beforeLegacySets);assert.equal(history,beforeLegacyHistory);
 for(const change of [s=>s.enabled=true,s=>s.width+=.1,s=>s.rgb[0]+=1,s=>s.extra=true]) {
  const changed=legacyRequest();change(changed.paint.stroke);
  await assert.rejects(legacy.prepareLegacyShape(changed),/Ordinary fill/);
 }
 const disabled=legacyRequest();disabled.paint.fill.enabled=false;
 await assert.rejects(legacy.prepareLegacyShape(disabled),/Ordinary fill/);
 const converted=legacyRequest();converted.paint.fill={kind:'pattern',enabled:true,name:'Loaded Pattern'};
 await assert.rejects(legacy.prepareLegacyShape(converted),/Ordinary fill/);
 assert.equal(sets,beforeLegacySets);assert.equal(history,beforeLegacyHistory);
 vector=true;fill=vectorFill;stroke=vectorStroke;
 for(const [mode,model,type,keys,values] of [
  ['cmyk','cmyk','CMYKColorClass',['cyan','magenta','yellowColor','black'],[12.345,20,30,4]],
  ['lab','lab','labColor',['luminance','a','b'],[55.5,-12.75,27]],
  ['gray','gray','grayscale',['gray'],[62.5]]]){
  ps.constants.DocumentMode[model==='gray'?'GRAYSCALE':model.toUpperCase()]=mode;
  for(const depth of [8,16]){
   doc.mode=mode;doc.bitsPerChannel=depth;doc.colorProfileName='Custom '+model;
   const raw={_obj:type,...Object.fromEntries(keys.map((k,i)=>[k,values[i]]))};fill={_obj:'solidColorLayer',color:copy(raw)};
   const beforeRead=history;data=await inspect();assert.equal(history,beforeRead);assert.equal(data.native_model,model);assert.equal(data.live_preview,false);assert.deepEqual(data.fill.native,{model,values});
   r=request(data);r.paint.fill.native.values[0]+=1;const beforeWrite=sets;await api.setLabel(r);assert.equal(sets,beforeWrite+1);
   assert.deepEqual(fill.color,{...raw,[keys[0]]:values[0]+1});assert.equal(doc.colorProfileName,'Custom '+model);assert.equal(doc.mode,mode);
   data=await inspect();r=request(data);r.paint.fill.native.values[0]=Infinity;await assert.rejects(api.setLabel(r),/color/);
   data=await inspect();r=request(data);doc.colorProfileName='Changed profile';await assert.rejects(api.setLabel(r),/已改变/);
  }
 }
 doc.mode='cmyk';doc.bitsPerChannel=8;missingStroke=true;
 fill={_obj:'solidColorLayer',color:{_obj:'CMYKColorClass',cyan:12,magenta:20,yellowColor:30,black:4}};
 materializedStroke={...copy(vectorStroke),strokeEnabled:false,strokeStyleLineWidth:{_unit:'pixelsUnit',_value:1},strokeStyleContent:{_obj:'solidColorLayer',color:{_obj:'CMYKColorClass',cyan:74.97,magenta:67.92,yellowColor:67.05,black:90.15}}};
 data=await inspect();r=request(data);r.paint.fill.native.values[0]+=1;await api.setLabel(r);
 data=await inspect();assert.equal(data.stroke.enabled,false);assert.equal(data.stroke.native.model,'cmyk');
 // An existing disabled stroke is still protected, even when only the fill changes.
 r=request(data);r.paint.fill.native.values[0]+=1;materializedStroke.strokeStyleContent.color.cyan=10;
 await assert.rejects(api.setLabel(r),/回滚/);assert.equal((await inspect()).stroke.native.values[0],74.97);
 materializedStroke=null;
 doc.mode='rgb';doc.bitsPerChannel=8;delete doc.colorProfileName;
 fill={_obj:'gradientLayer',gradient:{_obj:'gradientClassEvent',colors:[0,4096].map((location,i)=>({_obj:'colorStop',location,midpoint:37,color:{_obj:'labColor',luminance:50+i*20,a:-12,b:23}}))}};
 data=await inspect();r=request(data);r.paint.fill.stops[1].native.values[1]=-5.25;await api.setLabel(r);
 assert.equal(fill.gradient.colors[1].color.a,-5.25);assert.equal(fill.gradient.colors[1].midpoint,37);
 fill=copy(vectorFill);stroke=copy(vectorStroke);
 doc.activeLayers=[layer,{id:31,kind:'solid'}];assert.equal((await inspect()).available,false);doc.activeLayers=[layer];
 data=await inspect();r=request(data);r.paint.fill={kind:'solid',enabled:true,rgb:[20,40,60]};mismatch='armed';
 const priorRollbacks=rollbacks;await assert.rejects(api.setLabel(r),/回滚/);assert.equal(rollbacks,priorRollbacks+1);assert.equal(fill._obj,'patternLayer');
 mismatch=false;data=await inspect();r=request(data);r.paint.fill={kind:'solid',enabled:true,rgb:[20,40,60]};mismatch='armed';rollbackFailure=true;
 await assert.rejects(api.setLabel(r),e=>e.resultUnknown===true);assert.equal(rollbacks,priorRollbacks+2);
 console.log('PASS shape: cached targeted gets; vector/RGB/lock/multi/revision identity; one history; solid/gradient/pattern; preserved stroke/alpha; rollback and unknown never replay');
}
main().catch(e=>{console.error(e);process.exitCode=1;});
