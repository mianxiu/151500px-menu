const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('uxp/offline-probe/labels.js','utf8').slice(fs.readFileSync('uxp/offline-probe/labels.js','utf8').lastIndexOf('function createLayerView('));
function fixture(){
 const state={zoom:1,x:200,y:100,edge:false,extras:true,writes:0,fail:false,badMenu:false,failToggle:false,captures:0,modal:false};
 const target={session:'s',document_id:9,active_layer_id:11,layer_ids:[11]};const doc={activeLayers:[{id:11,visible:true,parent:null,boundsNoEffects:{left:400,top:200,right:500,bottom:250}}]};
 const ps={app:{activeDocument:doc},core:{executeAsModal:async fn=>{assert.equal(state.modal,false);state.modal=true;try{return await fn({isCancelled:false});}finally{state.modal=false;}},performMenuCommand:async({commandID})=>{state.writes++;if(state.failToggle)return false;if(commandID===1&&!state.extras)state.extras=true;else state[commandID===1?'edge':'extras']=!state[commandID===1?'edge':'extras'];return true;}},action:{batchPlay:async commands=>commands.map(c=>{
  if(c._obj==='uiInfo')return {result:{commandID:c.commandID,checked:state[c.commandID===1?'edge':'extras']}};
  if(c._obj==='get')switch(c._target[0]._property){
   case 'menuBarInfo':return {menuBarInfo:{submenu:[{name:'Layer Edges',command:1,checked:false},{name:state.badMenu?'Unknown':'Extras',command:2,checked:false}]}};
   case 'zoom':return {zoom:{_value:state.zoom}};
   case 'center':return {center:{horizontal:{_value:state.x},vertical:{_value:state.y}}};
  }
  state.writes++;
  if(c._obj==='select'){if(state.fail)return {_obj:'error'};state.zoom=10;state.x=2250;state.y=1125;return {};}
  if(c._obj==='set'&&c.to._obj==='zoom'){const ratio=c.to.zoom/state.zoom;state.x*=ratio;state.y*=ratio;state.zoom=c.to.zoom;return {};}
  if(c._obj==='set'&&c.to._obj==='center'){state.x=c.to.horizontal._value;state.y=c.to.vertical._value;return {};}
  throw Error('unexpected action');
 })}};
 let clock=Date.now();const context={Date:{now:()=>clock},setTimeout:fn=>{clock+=40;fn();},ps,session:'s',capture:async()=>{state.captures++;return {...target,layer_ids:[...target.layer_ids]};}};vm.createContext(context);vm.runInContext(source+'\nthis.tool=createLayerView(ps,session,capture);',context);
 return {state,target,doc,tool:context.tool,request:()=>({id:'f',session:'s',token:'t',target:structuredClone(target),expires_at_ms:Date.now()+5000})};
}
(async()=>{
 let f=fixture();const before=structuredClone(f.doc);const result=await f.tool.frame(f.request());assert.equal(result.zoom_percent,400);assert.equal(f.state.edge,false);assert.deepEqual(f.doc,before);assert.equal(f.state.x/f.state.zoom,225);const writes=f.state.writes;await f.tool.frame(f.request());assert.equal(f.state.edge,false);assert.equal(f.state.writes-writes,4);
 f=fixture();f.state.extras=false;await f.tool.frame(f.request());assert.ok(!f.state.extras&&!f.state.edge);
 f=fixture();f.doc.activeLayers.push({id:12,visible:true,parent:null,boundsNoEffects:{left:-20,top:-10,right:40,bottom:50}});f.target.layer_ids.push(12);const multi=await f.tool.frame(f.request());assert.deepEqual(Array.from(multi.bounds),[-20,-10,500,250]);assert.equal(multi.selection_count,2);
 f=fixture();f.doc.activeLayers[0].parent={visible:false,parent:null};await assert.rejects(f.tool.frame(f.request()),/隐藏/);assert.equal(f.state.writes,0);
 f=fixture();f.doc.activeLayers[0].boundsNoEffects.right=400;await assert.rejects(f.tool.frame(f.request()),/内容边界/);assert.equal(f.state.writes,0);
 f=fixture();f.state.badMenu=true;await assert.rejects(f.tool.frame(f.request()),/原生/);assert.equal(f.state.writes,0);
 f=fixture();const stale=f.request();f.target.document_id=10;await assert.rejects(f.tool.frame(stale),/文档已改变/);assert.equal(f.state.writes,0);
 f=fixture();await assert.rejects(f.tool.frame({...f.request(),expires_at_ms:0}),/过期/);assert.equal(f.state.writes,0);
 for(const key of ['fail','failToggle']){f=fixture();f.state[key]=true;await assert.rejects(f.tool.frame(f.request()),e=>!e.resultUnknown);assert.equal(f.state.zoom,1);assert.equal(f.state.x,200);assert.equal(f.state.y,100);assert.equal(f.state.edge,false);}

 f=fixture();f.state.edge=true;await f.tool.frame(f.request());assert.equal(f.state.edge,false,'clear persistent 319 edges');
 for(const phase of ['commit','cancel']){
  f=fixture();let shown=false,reads=0;const io={read:async()=>({id:'f',session:'s',token:'t',seq:++reads,phase:shown?phase:'preview',lease_until_ms:Date.now()+4000}),applied:async value=>{shown=true;assert.equal(f.state.modal,false,'edges must paint outside modal while F is held');assert.equal(f.state.edge,true);assert.equal(value.native_layer_edges,true);}};
  const value=await f.tool.frame(f.request(),io);assert.equal(shown,true);assert.equal(f.state.edge,false);assert.equal(value.view_restored,phase==='cancel');assert.equal(f.state.zoom,phase==='cancel'?1:4);
 }
 f=fixture();await f.tool.frame(f.request(),{read:async()=>({id:'f',session:'s',token:'t',seq:1,phase:'cancel',lease_until_ms:Date.now()+4000})});assert.equal(f.state.writes,0,'release before dispatch cancels without flashing');
 f=fixture();let read=0;await f.tool.frame(f.request(),{read:async()=>++read===1?{id:'f',session:'s',token:'t',seq:1,phase:'preview',lease_until_ms:0}:null});assert.equal(f.state.writes,0);
 f=fixture();await f.tool.frame(f.request(),{read:async()=>({id:'f',session:'s',token:'t',seq:1,phase:'commit',lease_until_ms:Date.now()+4000}),applied:async()=>{f.state.extras=false;}});assert.equal(f.state.extras,false,'preserve a manual Extras off when F did not enable it');
 f=fixture();await assert.rejects(f.tool.frame(f.request(),{read:async()=>({id:'foreign',session:'s',token:'t',seq:1,phase:'commit',lease_until_ms:Date.now()+4000})}),/mailbox/);assert.equal(f.state.writes,0);
 console.log('Layer frame: native bounds, union, zoom cap, idempotent extras, target/hidden/empty/expiry guards and view rollback passed');
})().catch(e=>{console.error(e);process.exitCode=1;});
