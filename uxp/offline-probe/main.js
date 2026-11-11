// Read-only, network-free installation and local IPC probe.
const {app} = require('photoshop');
const uxp = require('uxp');
const token = require('./probe-config.js');
const session = String(Date.now()) + '-' + Math.random().toString(16).slice(2);
// Independent read-only mailbox; never enters modal execution or blocks commands.
const inputTool=(()=>{
  const core=require('photoshop').core;
  let reading=false,lastId='',pendingReceipt=null;
  return async folder=>{
    if(reading)return;
    reading=true;
    try {
      if(pendingReceipt){await record(folder,'input-tool-response.json',pendingReceipt);pendingReceipt=null;}
      let request;
      try {const raw=await(await folder.getEntry('input-tool-request.json')).read();if(raw.length>2048)return;request=JSON.parse(raw);}catch(_){return;}
      const now=Date.now();
      if(!request || typeof request!=='object' || Array.isArray(request) || request.schema!==1 || request.token!==token || request.session!==session ||
         typeof request.id!=='string' || !/^[0-9a-f]{32}$/.test(request.id) || request.id===lastId ||
         !Number.isFinite(request.expires_at_ms) || request.expires_at_ms>now+2000)return;
      lastId=request.id;
      const response={schema:1,id:request.id,session,token},observedAt=Date.now(),observation=++inputToolObservationSequence;
      try {
        if(request.expires_at_ms<now)throw Error('expired-before-read');
        const tool=await core.getActiveTool();
        // PS24 returns classId; the reference documents classID.
        const classID=tool?.classID ?? tool?.classId;
        if(typeof classID!=='string' || !/^[A-Za-z_][A-Za-z0-9_]{0,79}$/.test(classID) || typeof tool.isModal!=='boolean' ||
           tool.classID!==undefined && tool.classId!==undefined && tool.classID!==tool.classId)
          throw Error('Unsupported tool state: '+JSON.stringify({classID:String(classID).slice(0,80),isModal:String(tool?.isModal).slice(0,20),fields:Object.keys(tool || {}).slice(0,12)}));
        Object.assign(response,{phase:'completed',classID,isModal:tool.isModal});
      }catch(error){response.phase='failed';response.error=String(error).slice(0,240);}
      response.completed_at_ms=Date.now();
      if(response.phase==='completed')await publishInputToolState({classID:response.classID,isModal:response.isModal},observedAt,observation);
      pendingReceipt=response;
      await record(folder,'input-tool-response.json',pendingReceipt);pendingReceipt=null;
    } finally {reading=false;}
  };
})();
// The backend needs the current tool and modal flag before a canvas click so
// InputContext can restore the canvas HIMC synchronously. Only a changed state
// is written; this is a read-only local file and never enters modal execution.
let inputToolStateSequence=0,inputToolStateWriting=false,inputToolStatePending=null,lastInputToolStateKey='';
let inputToolObservationSequence=0,lastInputToolObservation=0;
async function publishInputToolState(tool,observedAt=Date.now(),observation=++inputToolObservationSequence) {
  if(!heartbeatFolder)return;
  let value=tool;
  if(!value) {
    try {value=await require('photoshop').core.getActiveTool();} catch(_) {return;}
  }
  const classID=value?.classID ?? value?.classId;
  if(typeof classID!=='string' || !/^[A-Za-z_][A-Za-z0-9_]{0,79}$/.test(classID) || typeof value.isModal!=='boolean')return;
  // A host read may finish after a newer read has already published its tool.
  if(observation<lastInputToolObservation)return;
  lastInputToolObservation=observation;
  const key=classID+'|'+value.isModal;
  if(key===lastInputToolStateKey && !inputToolStatePending)return;
  inputToolStatePending={schema:1,session,token,sequence:++inputToolStateSequence,tool:classID,
    isModal:value.isModal,created_at_ms:observedAt};
  if(inputToolStateWriting)return;
  inputToolStateWriting=true;
  try {
    while(inputToolStatePending) {
      const written=inputToolStatePending;
      await record(heartbeatFolder,'input-tool-state.json',written);
      lastInputToolStateKey=written.tool+'|'+written.isModal;
      if(inputToolStatePending===written)inputToolStatePending=null;
    }
  } catch(error) {
    lastInputToolStateKey='';
    console.log('Input tool state unavailable',String(error));
  } finally {inputToolStateWriting=false;}
}
let inputToolEventSequence=0,inputToolEventWriting=false,inputToolEventPending=null;
async function inputToolEvent(event,descriptor) {
  const state=descriptor?.state?._value;
  // Q10: Move Tool mouse modals and Alert dialogs also emit enter/exit.
  if(!heartbeatFolder || event!=='toolModalStateChanged' || descriptor?.tool?.ID!=='txBx' ||
     descriptor?.kind?._value!=='tool' || !['enter','exit'].includes(state))return;
  inputToolEventPending={schema:1,session,token,sequence:++inputToolEventSequence,state,
    tool:'txBx',kind:'tool',created_at_ms:Date.now()};
  if(inputToolEventWriting)return;
  inputToolEventWriting=true;
  try {
    while(inputToolEventPending) {
      const value=inputToolEventPending;
      await record(heartbeatFolder,'input-tool-event.json',value);
      if(inputToolEventPending===value)inputToolEventPending=null;
    }
  } finally {inputToolEventWriting=false;}
}
// PS24.0.0 shares the standalone app.refresh channel with 24.0.1.
// A 24.1+ host still needs its actual official redraw API; no nested JSX.
const legacyPreview=/^24\.0(?:\.\d+)*$/.test(String(uxp.host?.version || ''));
const nativeSelection=/^24\.2(?:\.\d+)*$/.test(String(uxp.host?.version || ''));
const hostVersion=uxp.host.version;
const labels = require('./labels.js')(require('photoshop'), session, {legacyPreview,nativeSelection,hostVersion});
const seen = new Set();
let sequence = 0;
let running = false;
let lastRequest = null;
let lastHeartbeat = 0;
let pluginManifest = null;
let contextReadAt=0,contextWriteAt=0,contextSignature='';
let contextGeneration=0;
let contextInterval=100;
let observation={};
let pendingResponse=null;
let completedReceipts=[];
let lastWorkAt=0;
let menuRefreshId='';
let searchRefreshId='';
let heartbeatValue=null;
let heartbeatFolder=null,heartbeatWriting=false,metadataReadAt=0;
let interfacePreferencesValue=null;
let interfaceTheme=null;
let interfaceThemeProbe=null;
function localInterfaceTheme() {
  try {
    if(typeof document==='undefined' || typeof getComputedStyle!=='function') {
      observation.interface_theme={source:'css-media',available:false,reason:'no-dom'};return;
    }
    if(!interfaceThemeProbe) {
      const root=document.head||document.body;if(!root || !document.body)return;
      const style=document.createElement('style'),probe=document.createElement('div');
      probe.id='photoshop-menu-theme-probe';probe.style.display='none';
      style.textContent=['lightest','light','dark','darkest'].map((name,i)=>
        '@media (prefers-color-scheme: '+name+') { #photoshop-menu-theme-probe { width: '+(i+1)+'px; } }').join('\n');
      root.appendChild(style);document.body.appendChild(probe);interfaceThemeProbe=probe;
    }
    const width=getComputedStyle(interfaceThemeProbe).width;
    const theme=['light','light-gray','dark-gray','dark'][parseFloat(width)-1];
    observation.interface_theme={source:'css-media',width:String(width).slice(0,20),available:!!theme};
    if(theme && theme!==interfaceTheme) {
      interfaceTheme=theme;
      if(interfacePreferencesValue)interfacePreferencesValue={...interfacePreferencesValue,ui_theme:theme};
      if(heartbeatValue)heartbeatValue={...heartbeatValue,ui_theme:theme};
      lastHeartbeat=0;
    }
  } catch(error) {observation.interface_theme={source:'css-media',available:false,error:String(error).slice(0,200)};}
}
let activeExecution=null,executionPulseRunning=false;
// This pulse performs only local file I/O. A pending Photoshop await must not
// hide which original operation owns the host, or masquerade as fresh layer data.
async function executionPulse() {
  if(!activeExecution || executionPulseRunning)return;
  executionPulseRunning=true;
  try {
    const current=activeExecution;
    await record(current.folder,'execution-'+current.id+'.json',{
      schema:1,id:current.id,session,operation:current.operation,command:current.command,
      stage:current.stage,stage_started_at_ms:current.stageAt,seq:current.seq,
      phase:'running',plugin_time_ms:Date.now()});
  } catch(error) {console.log('Execution pulse unavailable',String(error));}
  finally {executionPulseRunning=false;}
}
async function executionStage(stage,seq) {
  if(!activeExecution)return;
  activeExecution.stage=stage;activeExecution.stageAt=Date.now();
  if(Number.isSafeInteger(seq))activeExecution.seq=seq;
  // Detailed durable checkpoints are opt-in for owned test documents only.
  // Normal dragging does not add per-frame filesystem writes.
  if(activeExecution.trace)await executionPulse();
}
function boundedPreviewRead(read,timeout=250) {
  let pending=null;
  const expired={};
  return async()=>{
    // Only local mailbox I/O can be timed out here. Photoshop mutations and
    // redraw calls must always remain awaited until their actual outcome.
    // Reuse an unresolved read rather than accumulating abandoned file reads.
    if(!pending) {
      pending=Promise.resolve().then(read).then(value=>({value}),()=>({value:null}));
    }
    let timer;const current=pending;
    try {
      const result=await Promise.race([current,new Promise(resolve=>{timer=setTimeout(()=>resolve(expired),timeout);})]);
      if(result===expired)return null;
      // Retain late data until it is consumed. Clearing on resolution would
      // lose every frame if disk latency consistently just exceeded timeout.
      if(pending===current)pending=null;
      return result.value;
    }
    finally {clearTimeout(timer);}
  };
}
async function refreshMenuContext(folder,now) {
  let request;
  try {request=JSON.parse(await (await folder.getEntry('menu-context-request.json')).read());}catch(_){return false;}
  if(request.token!==token || request.session!==session || !/^[0-9a-f]{32}$/.test(request.id) || request.id===menuRefreshId || !Number.isFinite(request.expires_at_ms) || request.expires_at_ms<now || request.expires_at_ms>now+10000)return false;
  const started=Date.now(),generation=contextGeneration;
  // Yield only between read-only host calls. Never abandon a Photoshop call
  // or consume/replay an operation here: the next poll owns its normal dispatch.
  async function checkpoint() {
    await readHeartbeat(folder);
    let current,cancel,command;
    try {current=JSON.parse(await(await folder.getEntry('menu-context-request.json')).read());}catch(_){}
    try {cancel=JSON.parse(await(await folder.getEntry('menu-context-cancel.json')).read());}catch(_){}
    try {command=JSON.parse(await(await folder.getEntry('probe-request.json')).read());}catch(_){}
    let reason='';
    // expires_at_ms limits acceptance, not an already accepted read. A large
    // snapshot may finish later, with fresh identity/capability validation.
    // Still bound the read itself and yield immediately to close/new commands.
    if(Date.now()-started>30000)reason='read-timeout';
    else if(current?.token!==token || current?.session!==session || current?.id!==request.id)reason='superseded';
    else if(cancel?.token===token && cancel?.session===session && cancel?.id===request.id)reason='closed';
    else if(command?.token===token && (!command.session || command.session===session) &&
        typeof command.id==='string' && !seen.has(command.id) &&
        ['inspect','capture','set_label','new_document'].includes(command.operation) &&
        Number.isFinite(command.expires_at_ms) && command.expires_at_ms>=Date.now() && command.expires_at_ms<=Date.now()+30000)reason='command';
    if(reason){const error=Error('Menu read yielded: '+reason);error.menuReadInterrupted=true;
      menuRefreshId=request.id;observation.menu={...observation.menu,interrupted:reason,elapsed_ms:Date.now()-started,completed_at_ms:Date.now()};throw error;}
  }
  let context;
  try {
    await checkpoint();
    // Acknowledges only this read, never layer eligibility or a mutation.
    // The menu can keep its bounded completion window when the FIRST DOM
    // snapshot itself takes longer than the request acceptance window.
    await record(folder,'menu-context-result.json',{schema:1,session,
      menu_request_id:request.id,menu_phase:'reading',started_at_ms:started});
    context=await labels.selectionContextFresh(checkpoint);await checkpoint();
  }
  catch(error) {
    if(error.menuReadInterrupted)return true;
    menuRefreshId=request.id;
    observation.menu={selection_ms:Date.now()-started,error:String(error).slice(0,200),completed_at_ms:Date.now()};
    return true;
  }
  const readDone=Date.now();contextReadAt=generation===contextGeneration?readDone:0;
  const needsShape=!context.locked && !context.background && (context.kind==='shape-fill' && context.layer_ids.length===1 || context.layer_ids.length>=2 && context.layer_ids.length<=128);
  const basic={...context,shape_editor:{available:false},schema:1,plugin_time_ms:readDone,
    menu_request_id:request.id,menu_phase:needsShape?'basic':'completed',timings_ms:{selection_ms:readDone-started}};
  // Publish ordinary capabilities before optional vector/pattern metadata.
  // A slow or failed color read must not leave every layer action disabled.
  await record(folder,'menu-context-result.json',basic);
  await record(folder,'layer-context.json',basic);
  menuRefreshId=request.id;
  observation.menu={selection_ms:readDone-started,completed_at_ms:readDone,phase:basic.menu_phase};
  if(needsShape) {
    let shape={available:false},error='';
    try {await checkpoint();shape=await labels.observeShape(context,true,null,checkpoint);await checkpoint();}
    catch(e){if(e.menuReadInterrupted)return true;error=String(e).slice(0,200);}
    let nativeLatest=null;
    if(nativeSelection){try{nativeLatest=await labels.selectionContextFresh(checkpoint);}catch(e){if(e.menuReadInterrupted)return true;throw e;}}
    const matches=nativeLatest?nativeLatest.document_id===context.document_id && nativeLatest.layer_ids.join(',')===context.layer_ids.join(','):labels.selectionMatches(context),done=Date.now();
    observation.menu={...observation.menu,shape_ms:done-readDone,shape_error:error,selection_changed:!matches,phase:'completed'};
    // A slow optional read needs fresh eligibility, not a fresh timestamp on
    // old locks/capabilities. A changed identity never enables the editor.
    if(matches) {
      let latest;
      try {latest=nativeLatest || (done-readDone>=1000?await labels.selectionContextFresh(checkpoint):context);}
      catch(error){if(error.menuReadInterrupted)return true;throw error;}
      try {await checkpoint();}catch(error){if(error.menuReadInterrupted)return true;throw error;}
      if(latest.document_id===context.document_id && latest.layer_ids.join(',')===context.layer_ids.join(','))
        await record(folder,'menu-context-result.json',{...basic,...latest,plugin_time_ms:done-readDone>=1000?Date.now():readDone,
          shape_editor:shape,menu_phase:'completed',timings_ms:{...basic.timings_ms,shape_ms:done-readDone}});
    }
  }
  return true;
}
async function refreshLayerSearch(folder,now,priorityCheckpoint) {
  let request;
  try {request=JSON.parse(await(await folder.getEntry('layer-search-request.json')).read());}catch(_){return false;}
  if(request.token!==token||request.session!==session||!/^[0-9a-f]{32}$/.test(request.id)||request.id===searchRefreshId||!Number.isFinite(request.expires_at_ms)||request.expires_at_ms<now||request.expires_at_ms>now+30000)return false;
  searchRefreshId=request.id;
  async function checkpoint() {
    await priorityCheckpoint();
    let current;
    try {current=JSON.parse(await(await folder.getEntry('layer-search-request.json')).read());}catch(_){}
    if(current?.id!==request.id||current?.token!==token||current?.session!==session||Date.now()>request.expires_at_ms)throw Error('搜索已取消或超时，请重新搜索');
  }
  const result={schema:1,id:request.id,session};
  try {result.result=request.preview?await labels.previewLayer(request.preview,checkpoint):await labels.searchLayers(request.query,checkpoint);await checkpoint();}
  catch(error){result.error=error.menuReadInterrupted?'菜单或操作已优先执行，请重新搜索':error.message||String(error);}
  await record(folder,'layer-search-result.json',result);
  return true;
}
async function finishResponse(folder,response) {
  // Store the final outcome before the compatibility response slot. If storage
  // is temporarily unavailable, retain the outcome in memory and only retry
  // writing that outcome, never run the Photoshop command again.
  await record(folder,'result-'+response.id+'.json',response);
  await record(folder,'probe-response.json',response);
  pendingResponse=null;
  if(activeExecution?.id===response.id)activeExecution=null;
  if(!response.result_unknown)completedReceipts.push('result-'+response.id+'.json');
  if(completedReceipts.length>64) {
    const name=completedReceipts.shift();
    // Only IDs whose terminal result was persisted by this session enter this
    // list. Never sweep an old directory or delete an unknown request's proof.
    for(const file of [name,name.replace('result-','mask-preview-'),name.replace('result-','mask-preview-status-'),name.replace('result-','preview-progress-'),name.replace('result-','execution-'),name.replace('result-','legacy-prepared-'),name.replace('result-','legacy-result-'),name.replace('result-','legacy-exited-'),name.replace('result-','text-object-page-').replace('.json','.html'),name.replace('result-','text-object-layout-'),name.replace('result-','text-object-input-'),name.replace('result-','text-object-expected-'),name.replace('result-','text-object-final-'),name.replace('result-','text-object-render-').replace('.json','.pdf'),name.replace('result-','text-object-before-').replace('.json','.pdf'),name.replace('result-','text-object-').replace('.json','.pdf')])
      try {await (await folder.getEntry(file)).delete();}catch(_){}
  }
}
async function updateLayerContext(folder,now,force=false,prepared=null,checkpoint=null) {
  if(!force && now-contextReadAt<contextInterval)return;
  const generation=contextGeneration;
  contextReadAt=now;
  let value;
  let error='';
  try {value=prepared && !nativeSelection && labels.selectionMatches(prepared)?prepared:await labels.selectionContextFresh(checkpoint);}
  catch(e) {if(e.menuReadInterrupted){observation.background={interrupted:e.reason,elapsed_ms:Date.now()-now,completed_at_ms:Date.now()};return;}
    error=String(e).slice(0,200);value={session,document_id:null,layer_ids:[],kind:'unknown',mask:0};}
  // Throttle from read completion: if DOM work itself takes over 100ms,
  // measuring from its start would immediately launch another costly read.
  const readDone=Date.now();contextReadAt=generation===contextGeneration?readDone:0;
  contextInterval=Math.max(error?1000:100,Math.min(1000,readDone-now));
  observation.background={selection_ms:readDone-now,interval_ms:contextInterval,completed_at_ms:readDone,error};
  // Vector properties and the pattern catalog are only needed for an opened
  // layer menu. Do not continuously reread them while Photoshop is idle.
  const shape={available:false};
  const signature=JSON.stringify([value,shape]);
  const snapshot={...value,shape_editor:shape,schema:1,plugin_time_ms:Date.now()};
  if(force || signature!==contextSignature || now-contextWriteAt>=1000) {
    await record(folder,'layer-context.json',snapshot);
    contextSignature=signature;contextWriteAt=Date.now();
  }
  return snapshot;
}
async function metadata() {
  if (!pluginManifest) {
    try {
      const folder = await uxp.storage.localFileSystem.getPluginFolder();
      pluginManifest = JSON.parse(await (await folder.getEntry('manifest.json')).read());
    } catch (_) { return {plugin_name: 'unavailable', command_label: 'unavailable'}; }
  }
  return {plugin_name: pluginManifest.name, command_label: pluginManifest.entrypoints[0].label.default};
}
function optional(read) {
  try { const value = read(); return value == null ? 'unavailable' : String(value); }
  catch (_) { return 'unavailable'; }
}
async function record(folder, name, value) {
  const file = await folder.createFile(name, {overwrite: true});
  await file.write(JSON.stringify(value));
}
async function readHeartbeat(folder) {
  return publishHeartbeat(folder,'read-progress',5000,{read_only_work:true});
}
async function publishHeartbeat(folder,reason,interval=5000,extra={}) {
  // A local CSS media value covers hosts which omit preference notifications.
  if(reason==='js-alive' || !interfaceThemeProbe)localInterfaceTheme();
  const now=Date.now();
  if(!heartbeatValue || heartbeatWriting || now-lastHeartbeat<interval)return;
  // Only a live JS checkpoint can renew transport liveness. No host getters,
  // and no retimestamping of layer snapshots or metadata: their original
  // timestamps remain separate. A synchronous host call blocking JS cannot
  // renew this lease; an asynchronous wait says nothing about host completion.
  heartbeatWriting=true;
  try {
    await record(folder,'probe-last.json',{...heartbeatValue,...extra,reason,
      sequence:++sequence,plugin_time_ms:now,layer_observation:observation,
      layer_preview_available:typeof require('photoshop').imaging?.getPixels==='function',
      layer_thumbnail_available:false,
      layer_thumbnail_engine:'disabled',
      shape_clipboard_available:labels.paintClipboardAvailable?.()===true});
    lastHeartbeat=now;
  } finally {heartbeatWriting=false;}
}
async function refreshMetadata(folder,checkpoint) {
  const started=Date.now();
  if(metadataReadAt && started-metadataReadAt<5000)return;
  // This work may await Photoshop, so only start it after interactive reads.
  // Unfinished host calls remain awaited; checkpoints skip the remaining work.
  metadataReadAt=started;
  try {
    await checkpoint();
    // Startup reads stay cached; theme changes arrive in the existing set event.
    if(!interfacePreferencesValue?.ui_preferences_complete)
      interfacePreferencesValue=await labels.interfacePreferences(checkpoint,true);
    if(interfaceTheme)interfacePreferencesValue={...interfacePreferencesValue,ui_theme:interfaceTheme};
    await checkpoint();
    const documents=app.documents;
    await checkpoint();
    const count=documents.length;
    await checkpoint();
    let name=null;
    if(count){const document=app.activeDocument;await checkpoint();name=document.title;}
    await checkpoint();
    const done=Date.now();
    heartbeatValue={...heartbeatValue,...interfacePreferencesValue,document_count:count,document_name:name,
      metadata_time_ms:done,metadata_started_at_ms:started,metadata_phase:'ready',metadata_error:''};
    observation.metadata={elapsed_ms:done-started,completed_at_ms:done};
    await publishHeartbeat(folder,'heartbeat',0);
  } catch(error) {
    observation.metadata={elapsed_ms:Date.now()-started,completed_at_ms:Date.now(),
      ...(error.menuReadInterrupted?{interrupted:error.reason}:{error:String(error).slice(0,200)})};
    if(error.menuReadInterrupted){metadataReadAt=0;return;}
    heartbeatValue={...heartbeatValue,metadata_phase:'failed',metadata_error:String(error).slice(0,200)};
  }
}
async function probe(reason) {
  if (running) return;
  running = true;
  try {
    const folder = await uxp.storage.localFileSystem.getDataFolder();
    heartbeatFolder=folder;
    if(pendingResponse) {await finishResponse(folder,pendingResponse);return;}
    const now = Date.now();
    if (!heartbeatValue) {
      heartbeatValue={
        ...await metadata(),
        probe_version: '0.3.105', session, transport: 'plugin-data-files',
        photoshop_version: hostVersion,
        extended_label_colors:labels.extendedLabelColors,
        uxp_version: optional(() => uxp.versions.uxp),
        document_redraw_available: typeof require('photoshop').core?.redrawDocument === 'function',
        legacy_preview_available:legacyPreview,
        plugin_id: 'com.15100px.menu.offline-probe',
        metadata_time_ms:0,metadata_phase:'pending',phase:'ready'
      };
    }
    // Transport publication performs local I/O only, never Photoshop getters.
    await publishHeartbeat(folder,reason==='poll'?'heartbeat':reason);
    // A chord may follow Caps after its menu request was published. Service
    // the accepted command before optional menu/color reads; capture's short
    // quiet period also protects the following fixed-target write.
    async function idleCheckpoint() {
        await readHeartbeat(folder);
        let command,menu;
        try {command=JSON.parse(await(await folder.getEntry('probe-request.json')).read());}catch(_){}
        try {menu=JSON.parse(await(await folder.getEntry('menu-context-request.json')).read());}catch(_){}
        const pendingCommand=command?.token===token && (!command.session || command.session===session) &&
          typeof command.id==='string' && !seen.has(command.id) &&
          ['inspect','capture','set_label','new_document'].includes(command.operation) &&
          Number.isFinite(command.expires_at_ms) && command.expires_at_ms>=Date.now() && command.expires_at_ms<=Date.now()+30000;
        const pendingMenu=menu?.token===token && menu.session===session && /^[0-9a-f]{32}$/.test(menu.id) &&
          menu.id!==menuRefreshId && Number.isFinite(menu.expires_at_ms) && menu.expires_at_ms>=Date.now() && menu.expires_at_ms<=Date.now()+10000;
        if(pendingCommand || pendingMenu){const error=Error('Background read yielded');error.menuReadInterrupted=true;error.reason=pendingCommand?'command':'menu';throw error;}
    }
    async function idleRead() {
      if(now-lastWorkAt<80)return;
      if(await refreshMenuContext(folder,now))return;
      if(await refreshLayerSearch(folder,now,idleCheckpoint))return;
      await updateLayerContext(folder,now,false,null,idleCheckpoint);
      await refreshMetadata(folder,idleCheckpoint);
    }
    let request;
    try {
      const file = await folder.getEntry('probe-request.json');
      const raw = await file.read();
      if (raw.length > 4096) return;
      request = JSON.parse(raw);
    } catch (_) { await idleRead();return; }
    if (request.token !== token || !['inspect','capture','set_label','new_document'].includes(request.operation) ||
        typeof request.id !== 'string' || request.id.length > 128 ||
        seen.has(request.id) || !Number.isFinite(request.expires_at_ms) ||
        request.expires_at_ms > Date.now() + 30000) {
      // Metadata DOM reads are comparatively costly. Pending work takes
      // priority; a short quiet period also lets capture's write arrive before
      // an unrelated cache refresh. Execution still rechecks fresh eligibility.
      await idleRead();
      return;
    }
    // A previous host's mailbox must remain untouched, but must not starve
    // fresh read-only snapshots after a normal Photoshop restart.
    if(request.session && request.session!==session){await idleRead();return;}
    const modifying=['set_label','new_document'].includes(request.operation);
    if(modifying) {
      if(!/^[0-9a-f]{32}$/.test(request.id))return;
      // A durable accepted/final receipt survives plugin reload. Its presence
      // consumes this ID even if no final result could be recorded.
      try {await folder.getEntry('result-'+request.id+'.json');seen.add(request.id);return;}
      catch(_){}
    }
    if(request.expires_at_ms<Date.now()) {
      if(!modifying){await idleRead();return;}
      // A valid, unseen request can expire behind a slow read. Give its
      // original identity a durable rejection instead of silently dropping it
      // and making the caller wait forever for an outcome. Existing accepted
      // records above, other sessions and malformed packets are never changed.
      seen.add(request.id);
      const rejected={schema:1,session,id:request.id,probe_version:'0.3.44',operation:request.operation,
        phase:'failed',error:'请求在执行前已过期；未修改 Photoshop',not_executed:true,result_unknown:false,completed_at_ms:Date.now()};
      pendingResponse=rejected;await finishResponse(folder,rejected);
      lastRequest=request.id;return;
    }
    seen.add(request.id); // Consume before any write; never retry after an uncertain response.
    const response = {schema:1,session,id: request.id, probe_version: '0.3.105', operation: request.operation,
      transport: 'plugin-data-files',
      completed_at_ms: Date.now(), phase: 'completed'};
    const timingStarted=Date.now(),timings={};
    if(modifying)await record(folder,'result-'+request.id+'.json',{...response,phase:'accepted'});
    timings.accept_ms=Date.now()-timingStarted;
    const operationStarted=Date.now();
    if(modifying) {
      activeExecution={folder,id:request.id,operation:request.operation,command:request.command,
        stage:'accepted',stageAt:Date.now(),seq:0,trace:request.preview_trace===true};
      await executionStage('dispatch');
    }
    let progressSerial=0,progressLast='',progressAt=0,progressFrame=0,progressRestoring=false;
    function progressStage(name){
      if(name.includes('rollback')){progressRestoring=true;return name.startsWith('verify')?'restore-verify':'restore';}
      if(name==='redraw-native')return progressRestoring?'restore-redraw':'redraw';
      if(name==='publish-frame')return 'ready';
      if(name==='commit-history')return 'commit';
      if(name.startsWith('write'))return 'write';
      if(name.startsWith('verify') || name==='validate-frame')return progressRestoring?'restore-verify':'verify';
      if(name==='capture-target' || name==='enter-modal')return 'prepare';
      return null;
    }
    async function previewProgress(stage,done=0,total=0,frame){
      if(Number.isSafeInteger(frame))progressFrame=frame;
      if(progressRestoring && stage==='verify')stage='restore-verify';
      if(progressRestoring && stage==='redraw')stage='restore-redraw';
      if(stage==='restore')progressRestoring=true;
      const now=Date.now(),key=stage+':'+progressFrame;
      if(key===progressLast && now-progressAt<100 && done!==total)return;
      progressLast=key;progressAt=now;
      try{await record(folder,'preview-progress-'+request.id+'.json',{id:request.id,session,token,
        serial:++progressSerial,stage,done,total,frame:progressFrame});}catch(_){}
    }
    try {
      if(request.stability_run && request.command!=='stability.internal')labels.stability.guard(request.stability_run);
      if(request.capture_current!==undefined) {
        if(request.capture_current!==true || request.capture_session!==session || request.operation!=='set_label' || request.target!==undefined ||
            !['selectAbove','selectBelow','extendAbove','extendBelow','copyStyle','pasteStyle'].includes(request.command))throw Error('Invalid direct Caps binding');
        // Keep both fresh captures: bind here, then the existing executor
        // checks the exact target again inside its modal before any write.
        const captured=await (['selectAbove','selectBelow','extendAbove','extendBelow'].includes(request.command)?labels.captureSelection():labels.capture());
        response.captured_target=captured;
        await record(folder,'result-'+request.id+'.json',{...response,phase:'accepted'});
        request={...request,target:captured};
      }
      if(request.operation==='inspect'&&request.layer_view_inspect===true)response.layer_view=await labels.inspectLayerView();
      else if(request.operation==='inspect'&&request.text_inspect===true)response.text=await labels.readText();
      else if (request.operation === 'inspect') {
        response.target = labels.documentTarget();
        response.document_count = response.target.document_ids.length;
        if(request.stability_run)response.stability=await (/^24\.2(?:\.\d+)*$/.test(String(uxp.host?.version || ''))?require('photoshop').core.executeAsModal(()=>labels.stability.read(request.stability_run),{commandName:'Read isolated target',timeOut:0}):labels.stability.read(request.stability_run));
        if(request.stability_run && request.native_selection_test)response.native_selection=await labels.selectionContextNative();
        if(request.stability_run && request.native_selection_compare===true)response.native_comparison=await labels.nativeReadComparison(request.stability_run,request.native_read_order===undefined?0:request.native_read_order,request.native_read_cooperative===true?idleCheckpoint:null);
        if(request.stability_run && request.native_feedback_compare===true)response.feedback_comparison=await labels.feedbackReadComparison(request.stability_run,request.native_read_order===undefined?0:request.native_read_order,request.native_read_cooperative===true?idleCheckpoint:null);
        if(request.stability_run && request.label_inheritance_compare===true)response.label_inheritance_comparison=await labels.labelInheritanceComparison(request.stability_run,request.native_read_order===undefined?0:request.native_read_order);
        if(request.stability_run && request.style_read_compare===true)response.style_read_comparison=await labels.styleReadComparison(request.stability_run,request.native_read_order===undefined?0:request.native_read_order);
        if(request.shape_inspect){
          const selected=await labels.selectionContextFresh();
          response.shape_editor=await labels.observeShape(selected,true,null,null,request.gradient_details===true?'inspect':false);
          if(request.gradient_details===true && response.shape_editor.available && selected.layer_ids.length===1)
            response.gradient_target={session,document_id:selected.document_id,layer_ids:selected.layer_ids,active_layer_id:selected.layer_ids[0]};
        }
      }
      if (request.operation === 'capture') response.target = await labels.capture();
      if(request.operation==='set_label' && ['shape.legacy.preview','text.object'].includes(request.command)) {
        // Reserve the plugin's existing worker without entering executeAsModal.
        // While native JSX owns the host, only mailbox I/O and cached pulses run.
        const textObject=request.command==='text.object';
        let input=null;
        if(textObject){const raw=await(await folder.getEntry('text-object-input-'+request.id+'.json')).read();if(raw.length>2097152)throw Error('文字对象参数超出上限');input=JSON.parse(raw);if(!['id','session','operation','token'].every(k=>input[k]===request[k]))throw Error('文字对象参数身份无效');}
        const prepared=textObject?await labels.prepareLegacyText(request,input.params):await labels.prepareLegacyShape(request);
        if(JSON.stringify(prepared).length>(textObject?2097152:request.paint?.mask_batch?900000:60000))throw Error('执行准备数据过大');
        await record(folder,'legacy-prepared-'+request.id+'.json',{schema:1,id:request.id,session,operation:request.operation,token,phase:'prepared',prepared});
        await executionStage('legacy-native-wait');
        while(true) {
          let native=null;
          try {const raw=await(await folder.getEntry('legacy-result-'+request.id+'.json')).read();if(raw.length<=65536)native=JSON.parse(raw);}catch(_){}
          if(native?.schema===1 && native.id===request.id && native.session===session && native.operation===request.operation && native.token===token && ['completed','failed'].includes(native.phase)) {
            let exited=null;
            try{const raw=await(await folder.getEntry('legacy-exited-'+request.id+'.json')).read();if(raw.length<=4096)exited=JSON.parse(raw);}catch(_){}
            if(exited?.schema===1 && exited.exited===true && exited.id===request.id && exited.session===session && exited.operation===request.operation && exited.token===token){
              if(textObject){
                try{
                  let expected={};
                  if(native.phase==='completed'){
                    const raw=await(await folder.getEntry('text-object-final-'+request.id+'.json')).read();if(raw.length>4194304)throw Error('文字对象核对数据超出上限');expected=JSON.parse(raw);
                    if(!['id','session','operation','token'].every(k=>expected[k]===request[k]))throw Error('文字对象核对身份无效');
                  }
                  response.result=await labels.finishLegacyText(request,prepared,native,expected);
                }catch(error){if(native.phase==='completed')error.resultUnknown=true;throw error;}
              }else response.result=await labels.finishLegacyShape(request,prepared,native);
              break;
            }
          }
          await publishHeartbeat(folder,'legacy-native',1000,{phase:'preview'});
          await new Promise(resolve=>setTimeout(resolve,40));
        }
      }
      if (request.operation === 'set_label' && !['shape.legacy.preview','text.object'].includes(request.command)) response.result = await labels.setLabel(request,
        (request.command==='shape.preview'||request.command==='layer.frame'&&request.frame_preview===true)?{
          stage:async(name,seq)=>{await executionStage(name,seq);const stage=progressStage(name);if(stage)await previewProgress(stage,0,0,seq);},
          progress:previewProgress,
          read:boundedPreviewRead(async()=>{
            await publishHeartbeat(folder,'mask-preview',1000,{phase:'preview'});
            try {
              const raw=await (await folder.getEntry('mask-preview-'+request.id+'.json')).read();
              if(raw.length>16384)throw Error('Preview mailbox exceeds limit');
              return JSON.parse(raw);
            } catch(error) {
              // A transient filesystem read cannot renew the preview lease.
              // Expiry rolls back; never restart or replay the operation.
              return null;
            }
          }),
          applied:value=>record(folder,'mask-preview-status-'+request.id+'.json',{
            id:request.id,session,phase:'preview',...value,plugin_time_ms:Date.now()})
        }:null);
      if (request.operation === 'new_document') response.result = await labels.createDocument(request);
      if(request.stability_run && request.command!=='stability.internal' && request.operation==='set_label')labels.stability.accept(request.stability_run);
    } catch (error) { response.phase = 'failed'; response.error = String(error); response.result_unknown=!!error.resultUnknown; }
    // This executes after the awaited modal function (including its finally)
    // returned or rejected. Content rollback can still be unknown independently.
    if(request.operation==='set_label' && ['shape.preview','layer.frame'].includes(request.command))
      response.execution_exit={schema:1,engine:'uxp-modal',exited:true};
    response.completed_at_ms = Date.now();
    timings.operation_ms=Date.now()-operationStarted;
    if(modifying) {
      // Keep the committed outcome even if metadata storage fails. Prepare the
      // new selection before confirming completion, so an immediately reopened
      // menu cannot freeze the previous layer. This never repeats a mutation.
      pendingResponse=response;
      if(response.phase==='completed') {
        const feedbackStarted=Date.now();
        try {response.layer_context=await updateLayerContext(folder,Date.now(),true,response.result.selection_context);}
        catch(error) {console.log('Selection feedback unavailable',String(error));}
        timings.selection_feedback_ms=Date.now()-feedbackStarted;
      }
      response.timings_ms=timings;
      await executionStage('persist-result');
      await finishResponse(folder,response);
    }
    else await record(folder, 'probe-response.json', response);
    lastRequest = request.id;
    if(['shape.preview','shape.legacy.preview'].includes(request.command))lastHeartbeat=0;
    // Capture gets a quiet period while its write follows; after modification
    // refresh the new selection on the next poll, without an extra 80ms wait.
    lastWorkAt=modifying?0:Date.now();
  } catch (error) {
    console.log('Offline file probe error', String(error));
  } finally { running = false; }
}
module.exports = {commands: {probe: () => probe('command')}};
probe('startup');
// Notifications make selection/type/lock changes visible on the next poll;
// the bounded DOM poll also covers changes made by external COM scripts.
observation.notifications={state:'pending',received:0};
Promise.resolve().then(()=>require('photoshop').action.addNotificationListener(
  ['select','make','delete','set','undo','redo','open','close','toolModalStateChanged','modalStateChanged'],(event,descriptor)=>{
    if(event==='toolModalStateChanged' || event==='modalStateChanged') {
      inputToolEvent(event,descriptor).catch(error=>console.log('Input tool event unavailable',String(error)));
      return;
    }
    if(event==='set') {
      const raw=descriptor?.to?.kuiBrightnessLevel;
      const theme={kPanelBrightnessOriginal:'light',kPanelBrightnessLightGray:'light-gray',
        kPanelBrightnessMediumGray:'dark-gray',kPanelBrightnessDarkGray:'dark'}[typeof raw==='string'?raw:raw?._value];
      if(typeof theme==='string') {
        interfaceTheme=theme;
        if(interfacePreferencesValue)interfacePreferencesValue={...interfacePreferencesValue,ui_theme:theme};
        if(heartbeatValue)heartbeatValue={...heartbeatValue,ui_theme:theme};
        lastHeartbeat=0;
      }
    }
    contextGeneration++;contextReadAt=0;
    observation.notifications={...observation.notifications,received:observation.notifications.received+1,
      last_event:event,last_event_at_ms:Date.now()};
  })).then(()=>{observation.notifications.state='ready';},error=>{
    observation.notifications.state='failed';observation.notifications.error=String(error).slice(0,200);
    console.log('Selection notifications unavailable',String(error));
  });
setInterval(() => {
  if(heartbeatFolder)inputTool(heartbeatFolder).catch(error=>console.log('Input tool observation unavailable',String(error)));
  return probe('poll');
}, 16);
// Tool selection has no action notification; a low-rate read catches it before
// the next canvas click. Only a changed classID/isModal is written.
setInterval(() => {
  if(heartbeatFolder)publishInputToolState().catch(error=>console.log('Input tool state unavailable',String(error)));
}, 250);
setInterval(executionPulse,1000);
// A pending asynchronous Photoshop call leaves JS responsive. Renew only the
// transport lease; metadata, selection and original execution evidence keep
// their own timestamps and ownership. No second host request is dispatched.
setInterval(()=>{if(heartbeatFolder)publishHeartbeat(heartbeatFolder,'js-alive').catch(error=>console.log('Transport pulse unavailable',String(error)));},1500);
