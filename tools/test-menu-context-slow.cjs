// Production poll loop with a simulated slow DOM/color read. No real PS claim.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const code=process.argv.includes('--baseline')?require('node:child_process').execFileSync('git',['show','HEAD:uxp/offline-probe/main.js'],{encoding:'utf8'}):fs.readFileSync('uxp/offline-probe/main.js','utf8');
const settle=async()=>{for(let i=0;i<30;i++)await new Promise(resolve=>setImmediate(resolve));};
async function host(){
 const files=new Map(),control={now:Date.now(),cost:0,reads:0,shapes:0,kind:'shape-fill',ids:[7],changed:false};
 let poll,release,releaseRead;
 const context=()=>{control.reads++;control.now+=control.cost;return {session:control.session,schema:1,document_id:1,layer_ids:control.ids,kind:control.kind,mask:15,quick_mask:127};};
 const folder={async getEntry(name){if(name==='menu-context-cancel.json')control.cancelChecks=(control.cancelChecks||0)+1;if(!files.has(name))throw Error('missing');return {async read(){return files.get(name);}};},async createFile(name){return {async write(raw){files.set(name,raw);}};}};
 const labels={isShapeTool:()=>false,capture:async()=>{control.captures=(control.captures||0)+1;return {session:control.session,document_id:1,layer_ids:[7],active_layer_id:7};},
   setLabel:async request=>{control.writes=(control.writes||0)+1;
     if(request.capture_current){const saved=JSON.parse(files.get('result-'+request.id+'.json'));assert.equal(saved.phase,'accepted');assert.deepEqual(saved.captured_target,request.target);assert.equal(request.target.active_layer_id,7);}
     return {verified:true,selection_context:context()};},
   selectionContext:context,selectionContextFresh:async checkpoint=>{if(control.readBlock)await new Promise(r=>{releaseRead=r;});if(checkpoint)await checkpoint(!!control.readBlock);for(let i=0;i<(control.extraChecks||0);i++){control.beforeCheck?.(i);await checkpoint();}return context();},selectionMatches:()=>!control.changed,interfacePreferences:async()=>({}),
   observeShape:async(context,force,catalog,checkpoint)=>{control.shapes++;if(control.block)await new Promise(r=>{release=r;});if(checkpoint)await checkpoint(true);control.afterShape=(control.afterShape||0)+1;if(control.shapeFail)throw Error('color unavailable');
     return {available:true,revision:'s:1'};}};
 labels.captureSelection=labels.capture;
 const sandbox={console:{log(){}},module:{exports:{}},Date:class extends Date{static now(){return control.now;}},setInterval(fn,ms){if(ms===16)poll=fn;},require(name){
   if(name==='./labels.js')return (ps,session)=>{control.session=session;return labels;};if(name==='./probe-config.js')return 'token';
   if(name==='photoshop')return {app:{documents:[]},action:{addNotificationListener(){}}};
   if(name==='uxp')return {storage:{localFileSystem:{getDataFolder:async()=>folder,getPluginFolder:async()=>{throw Error('no manifest');}}},host:{},versions:{}};
   throw Error(name);
 }};
 vm.runInNewContext(code,sandbox);await settle();
 const session=JSON.parse(files.get('probe-last.json')).session;
 const request=(id='a'.repeat(32),values={})=>files.set('menu-context-request.json',JSON.stringify({id,session,token:'token',expires_at_ms:control.now+9000,...values}));
 return {files,control,poll:()=>poll(),request,release:()=>release(),releaseRead:()=>releaseRead(),result:()=>JSON.parse(files.get('menu-context-result.json'))};
}
async function main(){
 // A pending Caps chord must win over an optional, potentially slow color
 // read. The capture/write handoff must not start that read between packets.
 let priority=await host();priority.request();priority.control.block=true;
 const submit=(id,operation)=>priority.files.set('probe-request.json',JSON.stringify({id,operation,token:'token',session:priority.control.session,expires_at_ms:priority.control.now+9000}));
 submit('capture-priority','capture');await priority.poll();await settle();
 assert.equal(priority.control.captures,1);assert.equal(priority.control.shapes,0);
 await priority.poll();await settle();assert.equal(priority.control.shapes,0,'Capture handoff must not start optional paint reads');
 submit('d'.repeat(32),'set_label');await priority.poll();await settle();
 assert.equal(priority.control.writes,1);assert.equal(priority.control.shapes,0);
 assert.equal(JSON.parse(priority.files.get('result-'+ 'd'.repeat(32)+'.json')).phase,'completed');
 priority.control.block=false;priority.control.now+=100;await priority.poll();await settle();
 assert.equal(priority.control.writes,1,'Duplicate write remains fenced');
 assert.equal(priority.control.shapes,1,'Idle menu must still finish without delaying freshness');
 assert.equal(priority.result().menu_phase,'completed');
 // Combined direct chord binds a real target before the existing executor;
 // never overwrite a menu-frozen target or allow another command family.
 const direct=await host();
 async function sendDirect(id,extra={}){direct.files.set('probe-request.json',JSON.stringify({id,operation:'set_label',command:'selectAbove',capture_current:true,capture_session:direct.control.session,token:'token',session:direct.control.session,expires_at_ms:direct.control.now+9000,...extra}));await direct.poll();await settle();return JSON.parse(direct.files.get('result-'+id+'.json'));}
 const combined=await sendDirect('e'.repeat(32));assert.equal(combined.phase,'completed');assert.equal(combined.captured_target.active_layer_id,7);assert.equal(direct.control.writes,1);assert.equal(direct.control.captures,1);
 await sendDirect('e'.repeat(32));assert.equal(direct.control.writes,1);
 for(const [i,extra] of [{command:'layer.new'},{target:{document_id:9}},{capture_session:'old'},{capture_current:'yes'}].entries()){
   assert.equal((await sendDirect(String(i+1).repeat(32),extra)).phase,'failed');assert.equal(direct.control.writes,1);assert.equal(direct.control.captures,1);
 }
 let h=await host();assert.equal(h.control.shapes,0,'Idle polling must not read vector metadata or preset catalogs');
 h.control.cost=2400;h.control.block=true;h.request();h.poll();await settle();
 assert.equal(h.result().menu_phase,'basic','Slow color read must not block ordinary layer capabilities');
 assert.equal(h.result().mask,15);assert.equal(h.result().timings_ms.selection_ms,2400);
 assert.equal(h.control.shapes,1);const before=h.control.reads;
 h.control.now+=3500;h.release();await settle();
 assert.equal(h.result().menu_phase,'completed');assert.equal(h.result().shape_editor.available,true);
 assert.ok(h.control.reads>before,'Slow color completion rereads eligibility instead of retimestamping old locks');
 assert.equal(h.result().plugin_time_ms,h.control.now);
 const shapes=h.control.shapes;await h.poll();await settle();assert.equal(h.control.shapes,shapes,'Completed menu ID must not be reread');
 h=await host();h.control.block=true;h.request();h.poll();await settle();h.control.changed=true;h.release();await settle();
 assert.equal(h.result().menu_phase,'basic','Changed identity must never publish paint metadata');
 h=await host();h.control.shapeFail=true;h.request();await h.poll();await settle();
 assert.equal(h.result().menu_phase,'completed');assert.equal(h.result().mask,15);assert.equal(h.result().shape_editor.available,false);
 h=await host();h.control.kind='pixel';h.request();await h.poll();await settle();
 assert.equal(h.result().menu_phase,'completed');assert.equal(h.control.shapes,0,'Ordinary layers must not read vector metadata');
 if(!process.argv.includes('--baseline')){
  for(const incoming of ['none','closed','command','superseded'])for(const elapsed of [0,12]){
   h=await host();h.control.kind='pixel';h.control.extraChecks=40;
   const id='a'.repeat(32);
   const close=()=>h.files.set('menu-context-cancel.json',JSON.stringify({id,session:h.control.session,token:'token'}));
   h.request(id);h.control.cancelChecks=0;
   h.control.beforeCheck=i=>{
    if(i===1){
     if(incoming==='closed')close();
     if(incoming==='superseded')h.request('b'.repeat(32));
     if(incoming==='command')h.files.set('probe-request.json',JSON.stringify({id:'capture-in-window',operation:'capture',token:'token',session:h.control.session,expires_at_ms:h.control.now+9000}));
     h.control.now+=elapsed;
    }
    if(i>1 && elapsed && incoming!=='none')assert.fail('Reached another host checkpoint after the polling window');
   };
   await h.poll();await settle();
   assert.equal(h.result().menu_phase,incoming==='none'?'completed':'reading');
   assert.ok(h.control.cancelChecks<=3,'Fast checkpoints repeatedly hit the same files');
   if(incoming==='command'){await h.poll();assert.equal(h.control.captures,1);}
  }
 }
 h=await host();h.request('b'.repeat(32),{token:'wrong'});await h.poll();await settle();assert.equal(h.files.has('menu-context-result.json'),false);
 h.request('c'.repeat(32),{expires_at_ms:h.control.now-1});await h.poll();await settle();assert.equal(h.files.has('menu-context-result.json'),false);
 h=await host();h.control.kind='pixel';h.control.readBlock=true;
 h.request('a'.repeat(32));h.poll();await settle();
 h.request('b'.repeat(32));h.releaseRead();await settle();
 assert.equal(h.result().menu_phase,'reading','Superseded read must not publish stale capabilities');
 assert.equal(h.result().mask,undefined);assert.equal(h.result().document_id,undefined);
 h.control.readBlock=false;await h.poll();await settle();
 assert.equal(h.result().menu_request_id,'b'.repeat(32),'A newer menu gets its own fresh read, never the previous result');
 // A queued command waits for the current host call, but skips remaining optional work.
 h=await host();h.control.block=true;h.request();h.poll();await settle();
 h.files.set('probe-request.json',JSON.stringify({id:'f'.repeat(32),operation:'set_label',command:'selectBelow',token:'token',session:h.control.session,expires_at_ms:h.control.now+9000}));
 await h.poll();assert.equal(h.control.writes,undefined,'Do not race a host read');
 h.release();await settle();assert.equal(h.control.afterShape,undefined,'Skip remaining optional calls');
 await h.poll();await settle();assert.equal(h.control.writes,1);assert.equal(h.result().menu_phase,'basic');
 // Acceptance expiry must not discard an already accepted large read.
 h=await host();h.control.block=true;h.request();h.poll();await settle();
 h.control.now+=11000;h.release();await settle();assert.equal(h.result().menu_phase,'completed');assert.equal(h.result().plugin_time_ms,h.control.now);
 const liveBeat=JSON.parse(h.files.get('probe-last.json'));assert.equal(liveBeat.reason,'read-progress');assert.equal(liveBeat.plugin_time_ms,h.control.now);assert.ok(liveBeat.metadata_time_ms<liveBeat.plugin_time_ms);
 await h.poll();await settle();const refreshedBeat=JSON.parse(h.files.get('probe-last.json'));assert.equal(refreshedBeat.reason,'heartbeat');assert.equal(refreshedBeat.read_only_work,undefined);
 h=await host();h.control.block=true;h.request();h.poll();await settle();
 h.control.now+=31000;h.release();await settle();assert.equal(h.result().menu_phase,'basic');
 // Background selection reads also yield before consuming the next DOM block.
 h=await host();h.control.now+=200;h.control.readBlock=true;h.poll();await settle();
 h.files.set('probe-request.json',JSON.stringify({id:'8'.repeat(32),operation:'set_label',command:'selectAbove',token:'token',session:h.control.session,expires_at_ms:h.control.now+9000}));
 const earlierReads=h.control.reads;h.releaseRead();await settle();assert.equal(h.control.reads,earlierReads);
 h.control.readBlock=false;await h.poll();await settle();assert.equal(h.control.writes,1);
 // Closed menu cancels only its read; wrong session/token cannot suppress it.
 for(const valid of [true,false]){
  h=await host();h.control.block=true;h.request();h.poll();await settle();
  h.files.set('menu-context-cancel.json',JSON.stringify({id:'a'.repeat(32),session:h.control.session,token:valid?'token':'wrong'}));
  h.release();await settle();assert.equal(h.result().menu_phase,valid?'basic':'completed');
 }
 h=await host();h.request();h.files.set('menu-context-cancel.json',JSON.stringify({id:'a'.repeat(32),session:h.control.session,token:'token'}));
 const reads=h.control.reads;await h.poll();await settle();assert.equal(h.control.reads,reads);assert.equal(h.control.shapes,0);
 // A prior-session packet is preserved without starving the new menu after restart.
 h=await host();const oldPacket=JSON.stringify({id:'7'.repeat(32),operation:'set_label',command:'selectBelow',token:'token',session:'previous-host',expires_at_ms:h.control.now-1});
 h.files.set('probe-request.json',oldPacket);h.request();await h.poll();await settle();
 assert.equal(h.result().menu_phase,'completed');assert.equal(h.control.writes,undefined);
 assert.equal(h.files.get('probe-request.json'),oldPacket);assert.equal(h.files.has('result-'+ '7'.repeat(32)+'.json'),false);
 // Expired unseen mutation receives a matching failure, never a write;
 // accepted/other-session records remain untouched.
 h=await host();const expiredId='9'.repeat(32);
 const expired={id:expiredId,operation:'set_label',command:'selectBelow',token:'token',session:h.control.session,expires_at_ms:h.control.now-1};
 h.files.set('probe-request.json',JSON.stringify(expired));await h.poll();await settle();
 const rejection=JSON.parse(h.files.get('result-'+expiredId+'.json'));assert.equal(rejection.phase,'failed');assert.equal(rejection.not_executed,true);assert.equal(h.control.writes,undefined);
 h.files.set('probe-request.json',JSON.stringify({...expired,expires_at_ms:h.control.now+9000}));await h.poll();await settle();assert.equal(h.control.writes,undefined);
 for(const accepted of [true,false]){
  h=await host();h.files.set('probe-request.json',JSON.stringify({...expired,session:accepted?h.control.session:'old'}));
  if(accepted)h.files.set('result-'+expiredId+'.json','{"phase":"accepted"}');
  await h.poll();await settle();assert.equal(h.files.get('result-'+expiredId+'.json'),accepted?'{"phase":"accepted"}':undefined);assert.equal(h.control.writes,undefined);
 }
 console.log('PASS: slow basic/paint reads; basic before paint; fresh eligibility; identity/expiry/token fences; paint failure; no idle color reads');
}
main().catch(e=>{console.error(e);process.exitCode=1;});
