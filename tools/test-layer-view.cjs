const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('uxp/offline-probe/labels.js','utf8').slice(fs.readFileSync('uxp/offline-probe/labels.js','utf8').lastIndexOf('function createLayerView('));
function fixture(){
 const state={zoom:1,x:200,y:100,edge:false,extras:true,writes:0,fail:false,badMenu:false,failToggle:false,captures:0,modal:false};
 const target={session:'s',document_id:9,active_layer_id:11,layer_ids:[11]};const doc={id:9,width:900,height:650,activeLayers:[{id:11,visible:true,parent:null,boundsNoEffects:{left:400,top:200,right:500,bottom:250}}]};
 const ps={app:{activeDocument:doc,currentTool:{id:'moveTool'}},core:{redrawDocument:async()=>{},executeAsModal:async fn=>{assert.equal(state.modal,false);state.modal=true;try{return await fn({isCancelled:false,hostControl:state.history});}finally{state.modal=false;}},performMenuCommand:async({commandID})=>{state.writes++;if(state.failToggle)return false;if(commandID===1&&!state.extras)state.extras=true;else state[commandID===1?'edge':'extras']=!state[commandID===1?'edge':'extras'];return true;}},action:{batchPlay:async commands=>commands.map(c=>{
  if(c._obj==='show'||c._obj==='hide'){state.members.find(l=>l.id===c._target[0]._id).visible=c._obj==='show';return {};}
  if(c._obj==='wait'){assert.equal(c.state._value,'redrawComplete');state.redraws=(state.redraws||0)+1;return {};}
  if(c._obj==='uiInfo')return {result:{commandID:c.commandID,checked:state[c.commandID===1?'edge':'extras']}};
  if(c._obj==='get')switch(c._target[0]._property){
   case 'menuBarInfo':return {menuBarInfo:{submenu:[{name:'Layer Edges',command:1,checked:false},{name:state.badMenu?'Unknown':'Extras',command:2,checked:false}]}};
   case 'zoom':return {zoom:{_value:state.zoom}};
   case 'center':return {center:{horizontal:{_value:state.x},vertical:{_value:state.y}}};
   case 'viewTransform':return {viewTransform:state.invalidTransform?[0]:[2,0,0,2,-525,-614]};
   case 'viewInfo':
    if(state.changeTarget)target.document_id++;
    return {viewInfo:{activeView:{globalBounds:{left:{_value:-1200},top:50,right:0,bottom:770}}}};
  }
  state.writes++;
  if(c._obj==='select'){if(state.fail)return {_obj:'error',message:'host rejected fit',result:-25920};state.zoom=10;state.x=2250;state.y=1125;return {};}
  if(c._obj==='set'&&c.to._obj==='zoom'){const ratio=c.to.zoom/state.zoom;state.x*=ratio;state.y*=ratio;state.zoom=c.to.zoom;return {};}
  if(c._obj==='set'&&c.to._obj==='center'){state.x=c.to.horizontal._value;state.y=c.to.vertical._value;return {};}
  throw Error('unexpected action');
 })}};
 let clock=Date.now();const context={Date:{now:()=>clock},setTimeout:fn=>{clock+=40;fn();},ps,session:'s',capture:async()=>{state.captures++;return {...target,layer_ids:[...target.layer_ids]};}};vm.createContext(context);vm.runInContext(source+'\nthis.tool=createLayerView(ps,session,capture);',context);
 return {state,target,doc,ps,tool:context.tool,request:()=>({id:'f',session:'s',token:'t',target:structuredClone(target),expires_at_ms:Date.now()+5000})};
}
(async()=>{
 let canvas=fixture();
 const gradient=await canvas.tool.gradient(canvas.request().target);
 assert.deepEqual(JSON.parse(JSON.stringify(gradient)),{matrix:[2,0,0,2,-525,-614],viewport:[-1200,50,0,770],document:[900,650],layer:[400,200,500,250],tool:'moveTool'});
 assert.equal(canvas.state.writes,0);
 await assert.rejects(canvas.tool.gradient({...canvas.request().target,session:'old'}),/会话/);
 canvas.state.invalidTransform=true;
 await assert.rejects(canvas.tool.gradient(canvas.request().target),/变换/);
 canvas.state.invalidTransform=false; canvas.state.changeTarget=true;
 await assert.rejects(canvas.tool.gradient(canvas.request().target),/目标已改变/);
 let f=fixture();const before=structuredClone(f.doc);const result=await f.tool.frame(f.request());assert.equal(result.zoom_percent,400);assert.equal(f.state.edge,false);assert.deepEqual(f.doc,before);assert.equal(f.state.x/f.state.zoom,225);const writes=f.state.writes;await f.tool.frame(f.request());assert.equal(f.state.edge,false);assert.equal(f.state.writes-writes,4);
 f=fixture();f.state.extras=false;await f.tool.frame(f.request());assert.ok(!f.state.extras&&!f.state.edge);
 f=fixture();f.doc.activeLayers.push({id:12,visible:true,parent:null,boundsNoEffects:{left:-20,top:-10,right:40,bottom:50}});f.target.layer_ids.push(12);const multi=await f.tool.frame(f.request());assert.deepEqual(Array.from(multi.bounds),[-20,-10,500,250]);assert.equal(multi.selection_count,2);
 f=fixture();f.doc.activeLayers[0].parent={visible:false,parent:null};await assert.rejects(f.tool.frame(f.request()),/隐藏/);assert.equal(f.state.writes,0);
 f=fixture();f.doc.activeLayers[0].boundsNoEffects.right=400;await assert.rejects(f.tool.frame(f.request()),/内容边界/);assert.equal(f.state.writes,0);
 f=fixture();f.state.badMenu=true;await assert.rejects(f.tool.frame(f.request()),/原生/);assert.equal(f.state.writes,0);
 f=fixture();const stale=f.request();f.target.document_id=10;await assert.rejects(f.tool.frame(stale),/文档已改变/);assert.equal(f.state.writes,0);
 f=fixture();await assert.rejects(f.tool.frame({...f.request(),expires_at_ms:0}),/过期/);assert.equal(f.state.writes,0);
 for(const key of ['fail','failToggle']){f=fixture();f.state[key]=true;await assert.rejects(f.tool.frame(f.request()),e=>!e.resultUnknown&&(key!=='fail'||/select\[1\].*-25920.*host rejected fit/.test(e.message)));assert.equal(f.state.zoom,1);assert.equal(f.state.x,200);assert.equal(f.state.y,100);assert.equal(f.state.edge,false);}

 f=fixture();f.state.edge=true;await f.tool.frame(f.request());assert.equal(f.state.edge,false,'clear persistent 319 edges');
 for(const phase of ['commit','cancel']){
  f=fixture();let shown=false,reads=0;const io={read:async()=>({id:'f',session:'s',token:'t',seq:++reads,phase:shown?phase:'preview',lease_until_ms:Date.now()+4000}),applied:async value=>{shown=true;assert.equal(f.state.modal,false,'edges must paint outside modal while F is held');assert.equal(f.state.edge,true);assert.equal(value.native_layer_edges,true);}};
  const value=await f.tool.frame(f.request(),io);assert.equal(shown,true);assert.equal(f.state.edge,false);assert.equal(value.view_restored,phase==='cancel');assert.equal(f.state.zoom,phase==='cancel'?1:4);
 }
 f=fixture();await f.tool.frame(f.request(),{read:async()=>({id:'f',session:'s',token:'t',seq:1,phase:'cancel',lease_until_ms:Date.now()+4000})});assert.equal(f.state.writes,0,'release before dispatch cancels without flashing');
 f=fixture();let read=0;await f.tool.frame(f.request(),{read:async()=>++read===1?{id:'f',session:'s',token:'t',seq:1,phase:'preview',lease_until_ms:0}:null});assert.equal(f.state.writes,0);
 f=fixture();await f.tool.frame(f.request(),{read:async()=>({id:'f',session:'s',token:'t',seq:1,phase:'commit',lease_until_ms:Date.now()+4000}),applied:async()=>{f.state.extras=false;}});assert.equal(f.state.extras,false,'preserve a manual Extras off when F did not enable it');
 f=fixture();await assert.rejects(f.tool.frame(f.request(),{read:async()=>({id:'foreign',session:'s',token:'t',seq:1,phase:'commit',lease_until_ms:Date.now()+4000})}),/mailbox/);assert.equal(f.state.writes,0);
 for(const failure of ['','read','restore']){
  const f=fixture(),child=f.doc.activeLayers[0];
  if(!failure)delete f.ps.core.redrawDocument;
  const group={id:20,visible:false,layers:[child,{id:21,visible:true}]};
  const kept={id:30,visible:true,layers:[{id:31,visible:false}]};
  child.parent=group;child.visible=false;
  f.doc.layers=[group,kept,{id:40,visible:true},{id:41,visible:false}];
  f.doc.activeLayers.push(kept);f.target.layer_ids.push(kept.id);kept.boundsNoEffects=child.boundsNoEffects;
  const members=f.state.members=[...f.doc.layers,...group.layers,...kept.layers];
  const before=members.map(l=>l.visible);let shown=false,restores=0;
  let historyID=1;
  Object.defineProperty(f.doc,'activeHistoryState',{get:()=>({id:historyID}),set:value=>{
   restores++;assert.equal(value.id,1);if(failure==='restore')throw Error('restore failed');
   // On PS 24.0.1 rewinding history may leave all the eye icons unchanged.
   historyID=value.id;
  }});
  f.state.history={suspendHistory:async()=>17,resumeHistory:async(id,commit)=>{
   assert.equal(id,17);assert.equal(commit,true);historyID=2;
  }};
  const run=f.tool.frame({...f.request(),frame_solo:true},{
   applied:async()=>{shown=true;assert.equal(f.state.modal,false);
    assert.deepEqual(members.map(l=>l.visible),[true,true,false,false,true,false,false]);},
   read:async()=>{if(shown && failure==='read')throw Error('mailbox failed');
    return {id:'f',session:'s',token:'t',seq:1,phase:shown?'cancel':'preview',lease_until_ms:Date.now()+4000};}
  });
  if(failure)await assert.rejects(run,e=>failure==='restore'?e.resultUnknown===true:e.message==='mailbox failed');
  else assert.equal((await run).view_restored,true);
  assert.equal(shown,true);assert.equal(restores,1);assert.equal(f.state.zoom,1);
  if(failure!=='restore')assert.deepEqual(members.map(l=>l.visible),before);
 }
 f=fixture();
 const chosen=f.doc.activeLayers[0],other={id:42,visible:true},removed={id:43,visible:true};
 f.doc.layers=f.state.members=[chosen,other,removed];
 f.state.history={suspendHistory:async()=>7,resumeHistory:async(id,commit)=>assert.equal(commit,true)};
 const tap=()=>f.tool.frame({...f.request(),frame_solo:true},{read:async()=>({
  id:'f',session:'s',token:'t',seq:2,phase:'commit',lease_until_ms:Date.now()+4000
 })});
 assert.equal((await tap()).isolated,true);assert.equal(other.visible,false);assert.equal(f.state.modal,false);
 assert.equal(f.tool.isolationID(f.doc.id),'f');
 await assert.rejects(f.tool.frame({...f.request(),exit_isolation:'stale'}),/隔离已结束/);
 assert.equal(other.visible,false);
 chosen.name='edited';f.doc.layers.pop();const added={id:44,visible:false};f.doc.layers.push(added);
 f.doc.activeLayers=[added];f.target.active_layer_id=44;f.target.layer_ids=[44];
 assert.equal((await f.tool.frame({...f.request(),exit_isolation:'f'})).isolated,false);assert.equal(other.visible,true);assert.equal(added.visible,false);
 assert.equal(f.tool.isolationID(f.doc.id),undefined);
 await assert.rejects(f.tool.frame({...f.request(),exit_isolation:'f'}),/隔离已结束/);
 assert.equal(chosen.name,'edited');assert.equal(f.doc.layers.includes(removed),false);
 console.log('Layer frame and Shift+F visibility, nested groups, release and rollback passed');
})().catch(e=>{console.error(e);process.exitCode=1;});
