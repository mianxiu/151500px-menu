// Production worker ordering with controlled host waits. No real PS claim.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('uxp/offline-probe/main.js','utf8');
const settle=async()=>{for(let n=0;n<30;n++)await new Promise(resolve=>setImmediate(resolve));};
async function host(initialCommand=false,registration='ready',cssWidth=null){
  const files=new Map(),c={now:100000,captures:0,preferences:0,documents:0,selection:0};
  const timers=new Map();let release;
  const folder={async getEntry(name){if(!files.has(name))throw Error('missing');return {read:async()=>files.get(name)};},
    async createFile(name){return {write:async text=>files.set(name,text)};}};
  const snapshot=()=>({session:c.session,document_id:1,layer_ids:[2],kind:'pixel',mask:15,quick_mask:127});
  const labels={isShapeTool:command=>command==='tool.rectangle',selectionContextFresh:async checkpoint=>{if(checkpoint)await checkpoint();c.selection++;if(c.onSelection)c.onSelection();return snapshot();},
    interfacePreferences:async()=>{c.preferences++;if(c.block)await new Promise(resolve=>{release=resolve;});return {ui_theme:'light-gray',ui_font_size:'large',ui_preferences_complete:true,ui_preferences_time_ms:c.now};},
    capture:async()=>{c.captures++;return snapshot();},setLabel:async()=>({verified:true,selection_context:null}),documentTarget:()=>({document_ids:[1]}),paintClipboardAvailable:()=>false};
  const app={get documents(){c.documents++;if(c.onDocuments)c.onDocuments();return [{title:'original'}];},activeDocument:{title:'original'}};
  function command(id){files.set('probe-request.json',JSON.stringify({token:'token',id,operation:'capture',expires_at_ms:c.now+9000}));}
  if(initialCommand)command('initial');
  c.cssWidth=cssWidth;c.cssNodes=[];
  const sandbox={console,Date:class extends Date{static now(){return c.now;}},module:{exports:{}},
    ...(cssWidth===null?{}:{document:{createElement:()=>({style:{}}),head:{appendChild:v=>c.cssNodes.push(v)},body:{appendChild:v=>c.cssNodes.push(v)}},
      getComputedStyle:()=>({width:String(c.cssWidth)+'px'})}),
    setInterval(fn,ms){timers.set(ms,fn);},setTimeout,clearTimeout,
    require(name){
      if(name==='photoshop')return {app,action:{addNotificationListener(events,listener){
        assert.deepEqual(Array.from(events),['select','make','delete','set','undo','redo','open','close','toolModalStateChanged','modalStateChanged']);
        if(registration==='throw')throw Error('registration failed');
        if(registration==='reject')return Promise.reject(Error('registration failed'));
        c.notify=listener;return Promise.resolve();
      }},core:{}};
      if(name==='uxp')return {host:{version:'24.2.0'},versions:{uxp:'6.4'},storage:{localFileSystem:{getDataFolder:async()=>folder,getPluginFolder:async()=>{throw Error('optional');}}}};
      if(name==='./labels.js')return (ps,session)=>{c.session=session;return labels;};
      if(name==='./probe-config.js')return 'token';throw Error(name);
    }};
  vm.runInNewContext(source,sandbox);await settle();
  return {files,c,command,poll:()=>timers.get(16)(),pulse:()=>timers.get(1500)(),release:()=>release(),
    heartbeat:()=>JSON.parse(files.get('probe-last.json')),layer:()=>files.get('layer-context.json'),
    menu:()=>files.set('menu-context-request.json',JSON.stringify({token:'token',session:c.session,id:'a'.repeat(32),expires_at_ms:c.now+9000}))};
}
(async()=>{
  let h=await host(true);
  const manifestVersion=JSON.parse(fs.readFileSync('uxp/offline-probe/manifest.json','utf8')).version;
  assert.equal(h.heartbeat().probe_version,manifestVersion);
  assert.equal(h.c.captures,1);assert.equal(h.c.preferences,0);assert.equal(h.c.documents,0);
  assert.equal(h.heartbeat().metadata_time_ms,0);assert.equal(h.heartbeat().metadata_phase,'pending');
  h.c.now+=100;await h.poll();assert.equal(h.c.preferences,1);assert.equal(h.c.documents,1);
  assert.equal(h.heartbeat().metadata_time_ms,h.c.now);assert.equal(h.heartbeat().document_name,'original');
  const preferencesAt=h.heartbeat().ui_preferences_time_ms;
  for(let n=0;n<12;n++){h.c.now+=6000;await h.poll();}
  assert.equal(h.c.preferences,1,'Successful startup preferences must not be polled');
  assert.equal(h.heartbeat().ui_preferences_time_ms,preferencesAt);
  assert.equal(h.c.documents,13,'Document metadata still changes independently');
  // Theme notifications carry the new value: no preference getter or modal.
  for(const [raw,theme] of [['kPanelBrightnessOriginal','light'],['kPanelBrightnessLightGray','light-gray'],
      ['kPanelBrightnessMediumGray','dark-gray'],['kPanelBrightnessDarkGray','dark']]) {
    h.c.notify('set',{to:{kuiBrightnessLevel:{_enum:'uiBrightnessLevelEnumType',_value:raw}}});
    await h.pulse();await settle();assert.equal(h.heartbeat().ui_theme,theme);assert.equal(h.c.preferences,1);
  }
  h.c.notify('set',{to:{kuiBrightnessLevel:{_value:'unknown'}}});await h.pulse();await settle();assert.equal(h.heartbeat().ui_theme,'dark');
  h.c.now+=6000;await h.poll();assert.equal(h.heartbeat().ui_theme,'dark');assert.equal(h.c.preferences,1);
  h=await host(true);h.c.notify('set',{to:{kuiBrightnessLevel:{_value:'kPanelBrightnessOriginal'}}});
  h.c.now+=100;await h.poll();assert.equal(h.heartbeat().ui_theme,'light','Startup read overwrote a newer theme event');
  assert.equal(h.c.preferences,1);
  h=await host(false,'ready',1);
  assert.equal(h.heartbeat().ui_theme,'light');assert.equal(h.c.cssNodes.length,2);
  for(const [width,theme] of [[2,'light-gray'],[3,'dark-gray'],[4,'dark']]) {
    h.c.cssWidth=width;h.c.now+=1600;await h.pulse();await settle();
    assert.equal(h.heartbeat().ui_theme,theme);assert.equal(h.c.preferences,1);
  }
  assert.equal(h.c.cssNodes.length,2,'Theme observer recreated hidden DOM nodes');
  h.c.cssWidth='invalid';h.c.now+=6000;await h.pulse();await settle();assert.equal(h.heartbeat().ui_theme,'dark');
  // A due metadata refresh must not precede either command or basic menu read.
  for(const kind of ['command','menu']){
    h=await host();h.c.now+=6000;const before={...h.c};
    if(kind==='command')h.command('capture-due');else h.menu();
    await h.poll();assert.equal(h.c.preferences,before.preferences);assert.equal(h.c.documents,before.documents);
    if(kind==='command')assert.equal(h.c.captures,1);
    else assert.equal(JSON.parse(h.files.get('menu-context-result.json')).menu_phase,'completed');
  }
  // JS liveness during an awaited metadata getter never freshens selection,
  // metadata, or execution evidence; commands wait for that original call.
  h=await host(true);const metadataAt=h.heartbeat().metadata_time_ms;
  h.c.now+=6000;h.c.block=true;const pending=h.poll();await settle();
  const oldLayer=h.layer(),documents=h.c.documents,preferences=h.c.preferences;
  h.command('after-wait');h.c.now+=6000;await h.pulse();await settle();
  assert.equal(h.heartbeat().plugin_time_ms,h.c.now);assert.equal(h.heartbeat().metadata_time_ms,metadataAt);
  assert.equal(h.heartbeat().reason,'js-alive');assert.equal(h.layer(),oldLayer);
  assert.equal([...h.files.keys()].some(name=>name.startsWith('execution-')),false);
  await h.poll();assert.equal(h.c.captures,1);assert.equal(h.c.preferences,preferences);
  h.release();await pending;assert.equal(h.c.documents,documents,'Yield before the next document getter');
  assert.equal(h.heartbeat().metadata_time_ms,metadataAt,'Interrupted metadata must not publish');
  h.c.block=false;await h.poll();assert.equal(h.c.captures,2);await h.poll();assert.equal(h.c.captures,2);
  // Checkpoints after synchronous getters also skip remaining metadata work.
  h=await host();h.c.now+=6000;h.c.onDocuments=()=>h.command('during-document-get');
  const previous=h.heartbeat().metadata_time_ms;await h.poll();assert.equal(h.heartbeat().metadata_time_ms,previous);
  await h.poll();assert.equal(h.c.captures,1);
  // Callbacks only invalidate; the existing poll owns all host reads.
  h=await host();const reads=h.c.selection;
  h.c.now+=1;h.c.notify('select',{});h.c.notify('set',{});
  assert.equal(h.c.selection,reads);await h.poll();assert.equal(h.c.selection,reads+1);
  await h.poll();assert.equal(h.c.selection,reads+1,'Notifications coalesce into one read');
  // An event arriving inside the read must survive its completion timestamp.
  h.c.onSelection=()=>{h.c.onSelection=null;h.c.notify('delete',{});};
  h.c.notify('make',{});await h.poll();const during=h.c.selection;
  await h.poll();assert.equal(h.c.selection,during+1,'In-flight invalidation was lost');
  h.c.onSelection=()=>{h.c.onSelection=null;h.c.notify('select',{});};
  h.menu();await h.poll();const menuReads=h.c.selection;
  await h.poll();assert.equal(h.c.selection,menuReads+1,'Menu read erased a notification');
  h.c.now+=6000;await h.pulse();await settle();
  assert.equal(h.heartbeat().layer_observation.notifications.state,'ready');
  assert.equal(h.heartbeat().layer_observation.notifications.received,5);
  for(const failure of ['throw','reject']){
    h=await host(false,failure);h.c.now+=6000;await h.pulse();await settle();
    assert.equal(h.heartbeat().layer_observation.notifications.state,'failed');
    assert.match(h.heartbeat().layer_observation.notifications.error,/registration failed/);
    const count=h.c.selection;h.c.now+=1100;await h.poll();
    assert.ok(h.c.selection>count,'Polling must survive failed registration');
  }
  h=await host();const selectionBefore=h.c.selection,id='b'.repeat(32);
  h.files.set('probe-request.json',JSON.stringify({token:'token',id,operation:'set_label',command:'selectAbove',target:{session:h.c.session,document_id:1,layer_ids:[2]},expires_at_ms:h.c.now+9000}));
  await h.poll();const terminal=JSON.parse(h.files.get('result-'+id+'.json'));
  assert.equal(terminal.probe_version,manifestVersion);
  assert.equal(terminal.phase,'completed');assert.equal(terminal.layer_context.document_id,1);
  const expiredId='c'.repeat(32);
  h.files.set('probe-request.json',JSON.stringify({token:'token',id:expiredId,operation:'set_label',command:'selectAbove',expires_at_ms:h.c.now-1}));
  await h.poll();const expired=JSON.parse(h.files.get('result-'+expiredId+'.json'));
  assert.equal(expired.probe_version,manifestVersion);assert.equal(expired.not_executed,true);
  assert.equal(h.c.selection,selectionBefore+1,'Fresh native feedback must precede the persisted terminal');
  const toolId='d'.repeat(32);
  const beforeTool=h.layer(),beforeToolReads=h.c.selection;
  h.files.set('probe-request.json',JSON.stringify({token:'token',id:toolId,operation:'set_label',command:'tool.rectangle',target:{session:h.c.session,document_id:1,layer_ids:[2]},expires_at_ms:h.c.now+9000}));
  await h.poll();const toolTerminal=JSON.parse(h.files.get('result-'+toolId+'.json'));
  assert.equal(toolTerminal.phase,'completed');
  assert.equal(toolTerminal.layer_context,undefined);
  assert.equal(h.c.selection,beforeToolReads);
  assert.equal(h.layer(),beforeTool);
  console.log('PASS metadata scheduling: cold/due command and menu priority; awaited-call ownership; pure JS pulse; original ages; getter checkpoints; no replay');
})().catch(error=>{console.error(error);process.exitCode=1;});
