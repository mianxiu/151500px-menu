const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const clone=v=>JSON.parse(JSON.stringify(v));
const original={_obj:'textLayer',textKey:'9.\tFirst\r10.\tSecond',orientation:{_value:'horizontal'},warp:{warpStyle:{_value:'warpNone'}},textShape:[{char:{_value:'box'}}],
 textStyleRange:[{from:0,to:20,textStyle:{fontPostScriptName:'Font',size:{_unit:'pointsUnit',_value:24},fontAvailable:true}}],
 paragraphStyleRange:[{from:0,to:9,paragraphStyle:{_obj:'paragraphStyle',impliedStartIndent:{_unit:'pixelsUnit',_value:0},spaceAfter:2}},{from:9,to:20,paragraphStyle:{_obj:'paragraphStyle',spaceAfter:4}}]};
let active=11,probe=null,makes=0,deletes=0,badBounds=false;
const target={session:'s',document_id:1,active_layer_id:11,layer_ids:[11]};
const ps={app:{activeDocument:{resolution:300},fonts:{getByName:name=>({postScriptName:name,family:'Segoe UI Symbol',style:'Regular'})}},action:{batchPlay:async commands=>commands.map(c=>{
 if(c._obj==='make'){assert.equal(c.using.textKey,'M');assert.equal(c.using.textShape[0].char._value,'paint');assert.ok(c.using.textStyleRange.length);makes++;active=22;probe={};return {layerID:22};}
 if(c._obj==='set'&&c.to._obj==='layer'){assert.equal(c._target[0]._id,22);assert.equal(c.to.opacity._value,0);return {};}
 if(c._obj==='set'){assert.equal(active,22);probe=clone(c.to);return {};}
 if(c._obj==='get'){assert.equal(active,22);return {textKey:{boundingBox:{right:{_unit:'pixelsUnit',_value:badBounds?NaN:probe.textKey.length===1?50:probe.textKey.startsWith('10.')?250:200}}}};}
 if(c._obj==='delete'){assert.equal(c._target[0]._id,22);deletes++;probe=null;return {};}
 if(c._obj==='select'){assert.equal(c.makeVisible,false);active=c._target[0]._id;return {};}
 throw Error(c._obj);
})}};
let source=fs.readFileSync('uxp/offline-probe/labels.js','utf8');
source=source.slice(source.lastIndexOf('function createTextTools('));
const exportedTools=/return \{read,replace,split,[^\r\n]*\};/;
assert.match(source,exportedTools);
source=source.replace(exportedTools,'return {formatList,stylesMatch};');
const context={ps,session:'s',capture:async()=>({...target,active_layer_id:active,layer_ids:[active]})};vm.createContext(context);vm.runInContext(source+'\nthis.tools=createTextTools(ps,session,capture);',context);
const tools=context.tools,params={enabled:true,paragraphs:[{from:0,to:9,start:12,first:3,prefix:3},{from:9,to:20,start:24,first:-2,prefix:13}]};
(async()=>{
 let wanted=clone(original);await tools.formatList(original,wanted,params,target,{isCancelled:false});
 assert.equal(active,11);assert.equal(makes,1);assert.equal(deletes,1);
 assert.equal(wanted.paragraphStyleRange[0].paragraphStyle.startIndent._value,63);
 assert.equal(wanted.paragraphStyleRange[0].paragraphStyle.firstLineIndent._value,-36);
 assert.equal(wanted.paragraphStyleRange[1].paragraphStyle.startIndent._value,70);
 assert.equal(wanted.paragraphStyleRange[1].paragraphStyle.spaceAfter,4);
 assert.ok(!('impliedStartIndent' in wanted.paragraphStyleRange[0].paragraphStyle));
 badBounds=true;await assert.rejects(tools.formatList(original,clone(original),params,target,{isCancelled:false}),/符号宽度/);assert.equal(active,11);assert.equal(deletes,2);badBounds=false;
 const restored=clone(original);await tools.formatList(original,restored,{...params,enabled:false},target,{isCancelled:false});assert.equal(makes,2);assert.equal(restored.paragraphStyleRange[0].paragraphStyle.startIndent._value,12);
 await assert.rejects(tools.formatList(original,clone(original),{...params,paragraphs:[{...params.paragraphs[0],prefix:100}]},target,{}),/范围/);
 const centered=clone(original);centered.paragraphStyleRange[0].paragraphStyle.align={_value:'center'};await assert.rejects(tools.formatList(original,centered,params,target,{}),/左对齐/);assert.equal(makes,2);
 const bullets=clone(original);bullets.textKey='◦\tBody\r    ▪\tNext';bullets.textStyleRange[0].to=bullets.textKey.length+1;bullets.paragraphStyleRange=[{from:0,to:7,paragraphStyle:{}},{from:7,to:bullets.textKey.length+1,paragraphStyle:{}}];
 const formatted=clone(bullets),symbols={enabled:true,marker_font:'SegoeUISymbol',paragraphs:[{from:0,to:7,start:0,first:0,prefix:2},{from:7,to:bullets.textKey.length+1,start:0,first:0,prefix:13}]};
 await tools.formatList(bullets,formatted,symbols,target,{});
 for(let i=0;i<formatted.textKey.length;i++){const style=formatted.textStyleRange.find(r=>r.from<=i&&r.to>i).textStyle;assert.equal(style.fontPostScriptName,[0,11].includes(i)?'SegoeUISymbol':'Font');if([0,11].includes(i))assert.equal(style.fontTechnology,1);assert.equal(style.size._value,24);}
 assert.ok(tools.stylesMatch(formatted,formatted,300,bullets,[],symbols));
 const wrong=clone(formatted);wrong.textStyleRange.find(r=>r.from===0).textStyle.fontPostScriptName='InstalledButMissingGlyph';assert.equal(tools.stylesMatch(wrong,formatted,300,bullets,[],symbols),false);
 wrong.textStyleRange[0].textStyle.fontAvailable=true;const prior=clone(bullets);prior.textKey='A'+prior.textKey.slice(1);assert.equal(tools.stylesMatch(wrong,formatted,300,prior,Array.from({length:formatted.textKey.length+1},(_,i)=>i),symbols),false);
 console.log('List layout: native advance/DPI, paragraph styles, restoration, invalid plans and owned probe cleanup passed');
})().catch(error=>{console.error(error);process.exitCode=1;});
