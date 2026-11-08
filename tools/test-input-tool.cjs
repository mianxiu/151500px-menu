// Production mailbox, no real Photoshop or system input language changes.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('uxp/offline-probe/main.js','utf8');
const settle=async()=>{for(let i=0;i<30;i++)await new Promise(resolve=>setImmediate(resolve));};
async function host(){
  const files=new Map(),timers=new Map(),state={reads:0,tool:{classID:'typeCreateOrEditTool',isModal:false}};
  const folder={getEntry:async name=>{if(!files.has(name))throw Error('missing');return {read:async()=>files.get(name)};},
    createFile:async name=>({write:async text=>{if(name==='input-tool-response.json' && state.failReceipt){state.failReceipt=false;throw Error('temporary receipt write failure');}if(name==='input-tool-state.json' && state.waitStateWrite)await state.waitStateWrite;files.set(name,text);}})};
  const ps={app:{documents:[]},core:{getActiveTool:async()=>{state.reads++;const tool=state.tool;if(state.wait)await state.wait;if(state.error)throw Error('unavailable');return tool;}},
    action:{addNotificationListener(events,listener){state.events=events;state.listener=listener;}}};
  const sandbox={module:{exports:{}},console,Date,setTimeout,clearTimeout,setInterval(fn,ms){timers.set(ms,fn);},
    require(name){
      if(name==='photoshop')return ps;
      if(name==='uxp')return {host:{version:'24.0.1'},versions:{uxp:'6'},storage:{localFileSystem:{getDataFolder:async()=>folder,getPluginFolder:async()=>{throw Error('optional');}}}};
      if(name==='./probe-config.js')return 'private-test-token';
      if(name==='./labels.js')return (ps,session)=>{state.session=session;return {interfacePreferences:async()=>({}),selectionContextFresh:async()=>({})};};
      throw Error(name);
    }};
  vm.runInNewContext(source,sandbox);await settle();
  return {files,state,emit:async(event,descriptor)=>{state.listener(event,descriptor);await settle();},poll:async()=>{timers.get(16)();await settle();},toolPoll:async()=>{timers.get(250)();await settle();},
    request(changes={}){const request={schema:1,id:'a'.repeat(32),token:'private-test-token',session:state.session,expires_at_ms:Date.now()+1000,...changes};files.set('input-tool-request.json',JSON.stringify(request));return request;},
    response:()=>JSON.parse(files.get('input-tool-response.json'))};
}
(async()=>{
  const eventHost=await host();
  assert.ok(eventHost.state.events.includes('toolModalStateChanged'));
  const textEvent=state=>({level:state==='enter'?1:0,state:{_enum:'state',_value:state},
    tool:{_obj:'tool',ID:'txBx',title:'Horizontal Type Tool'},kind:{_enum:'kind',_value:'tool'}});
  await eventHost.emit('toolModalStateChanged',textEvent('enter'));
  let event=JSON.parse(eventHost.files.get('input-tool-event.json'));
  assert.equal(event.state,'enter');assert.equal(event.token,'private-test-token');assert.equal(event.sequence,1);
  assert.equal(event.tool,'txBx');assert.equal(event.kind,'tool');
  await eventHost.emit('toolModalStateChanged',textEvent('exit'));
  event=JSON.parse(eventHost.files.get('input-tool-event.json'));
  assert.equal(event.state,'exit');assert.equal(event.sequence,2);assert.equal(eventHost.state.reads,0);
  for(const [name,descriptor] of [
    ['modalStateChanged',{state:{_value:'enter'},title:'Alert'}],
    ['modalStateChanged',textEvent('exit')],
    ['toolModalStateChanged',{...textEvent('enter'),tool:{ID:'arwT'},kind:{_value:'mouse'}}],
    ['toolModalStateChanged',{...textEvent('exit'),tool:{ID:'arwT'}}],
    ['toolModalStateChanged',{...textEvent('exit'),kind:{_value:'mouse'}}],
    ['toolModalStateChanged',{state:{_value:'enter'}}],
    ['toolModalStateChanged',textEvent('invalid')],
  ])await eventHost.emit(name,descriptor);
  assert.equal(JSON.parse(eventHost.files.get('input-tool-event.json')).sequence,2);
  const idle=await host();
  idle.state.tool={classID:'typeCreateOrEditTool',isModal:false};
  await idle.toolPoll();
  const idleState=JSON.parse(idle.files.get('input-tool-state.json'));
  assert.equal(idleState.tool,'typeCreateOrEditTool');assert.equal(idleState.isModal,false);
  assert.equal(idleState.sequence,1);assert.equal(idle.state.reads,1);
  await idle.toolPoll();
  assert.equal(JSON.parse(idle.files.get('input-tool-state.json')).sequence,1,'Unchanged tool state must not be rewritten');
  idle.state.tool={classID:'moveTool',isModal:false};
  await idle.toolPoll();
  const movedState=JSON.parse(idle.files.get('input-tool-state.json'));
  assert.equal(movedState.tool,'moveTool');assert.equal(movedState.sequence,2);
  const delayed=await host();let finishRead;
  delayed.state.wait=new Promise(resolve=>finishRead=resolve);
  const pendingPoll=delayed.toolPoll();await settle();const waitingAt=Date.now();
  await new Promise(resolve=>setTimeout(resolve,5));finishRead();await pendingPoll;await settle();
  assert.ok(JSON.parse(delayed.files.get('input-tool-state.json')).created_at_ms<=waitingAt,
    'A delayed tool read must keep its observation start time');
  const reordered=await host();let releaseOld;
  reordered.state.tool={classID:'moveTool',isModal:false};
  reordered.state.wait=new Promise(resolve=>releaseOld=resolve);await reordered.toolPoll();
  reordered.state.wait=null;reordered.state.tool={classID:'typeCreateOrEditTool',isModal:false};
  await reordered.toolPoll();releaseOld();await settle();
  assert.equal(JSON.parse(reordered.files.get('input-tool-state.json')).tool,'typeCreateOrEditTool',
    'An older delayed tool read must not overwrite the newer text tool');
  const pending=await host();await pending.toolPoll();let releaseWrite;
  pending.state.waitStateWrite=new Promise(resolve=>releaseWrite=resolve);
  pending.state.tool={classID:'moveTool',isModal:false};await pending.toolPoll();
  pending.state.tool={classID:'typeCreateOrEditTool',isModal:false};await pending.toolPoll();
  pending.state.waitStateWrite=null;releaseWrite();await settle();
  assert.equal(JSON.parse(pending.files.get('input-tool-state.json')).tool,'typeCreateOrEditTool',
    'Returning to the last written tool must replace a pending different tool');
  for(const change of [{schema:2},{token:'other'},{session:'other'},{id:'invalid'},{expires_at_ms:Date.now()+90000}]){
    const h=await host();h.request(change);await h.poll();assert.equal(h.state.reads,0);assert.equal(h.files.has('input-tool-response.json'),false);
  }
  const expired=await host();const old=expired.request({expires_at_ms:Date.now()-1000});await expired.poll();
  assert.equal(expired.state.reads,0);assert.equal(expired.response().id,old.id);assert.equal(expired.response().phase,'failed');assert.match(expired.response().error,/expired-before-read/);
  const failedWrite=await host();failedWrite.state.failReceipt=true;const held=failedWrite.request();await failedWrite.poll();await failedWrite.poll();
  assert.equal(failedWrite.state.reads,1,'Retry only the receipt, never the host read');assert.equal(failedWrite.response().id,held.id);
  const h=await host();const request=h.request();await h.poll();
  assert.equal(h.state.reads,1);assert.deepEqual(h.response(),{schema:1,id:request.id,session:request.session,token:request.token,phase:'completed',classID:'typeCreateOrEditTool',isModal:false,completed_at_ms:h.response().completed_at_ms});
  const published=JSON.parse(h.files.get('input-tool-state.json'));
  assert.equal(published.tool,'typeCreateOrEditTool');assert.equal(published.isModal,false);
  assert.equal(published.token,'private-test-token');assert.equal(published.session,h.state.session);assert.equal(published.sequence,1);
  await h.poll();assert.equal(h.state.reads,1,'Do not re-run the consumed observation');
  assert.equal(JSON.parse(h.files.get('input-tool-state.json')).sequence,1,'Unchanged tool state must not be rewritten');
  h.state.tool.isModal=true;h.request({id:'b'.repeat(32)});await h.poll();assert.equal(h.response().isModal,true);
  const modalState=JSON.parse(h.files.get('input-tool-state.json'));
  assert.equal(modalState.isModal,true);assert.equal(modalState.sequence,2);
  let release;h.state.wait=new Promise(resolve=>release=resolve);h.request({id:'c'.repeat(32)});await h.poll();await h.poll();assert.equal(h.state.reads,3,'Only one tool read may be in flight');
  release();await settle();assert.equal(h.response().id,'c'.repeat(32));h.state.wait=null;
  h.state.tool={classId:'moveTool',isModal:false};h.request({id:'d'.repeat(32)});await h.poll();
  assert.equal(h.response().phase,'completed');assert.equal(h.response().classID,'moveTool');
  for(const [i,tool] of [{classID:'x',isModal:0},{classID:'',isModal:false},{classID:'moveTool',isModal:null},{classID:'moveTool',classId:'typeCreateOrEditTool',isModal:false}].entries()){
    h.state.tool=tool;h.request({id:String(i+4).repeat(32)});await h.poll();assert.equal(h.response().phase,'failed');
  }
  h.state.error=true;h.request({id:'f'.repeat(32)});await h.poll();assert.equal(h.response().phase,'failed');
  assert.match(h.response().error,/unavailable/);assert.ok(h.response().error.length<=240);
  assert.equal(h.files.has('probe-request.json'),false);assert.equal(h.files.has('probe-response.json'),false);
  console.log('PASS: authenticated read-only tool state / idle Type tool / modal editing / one in flight / no replay or document mutation');
})().catch(error=>{console.error(error);process.exitCode=1;});
