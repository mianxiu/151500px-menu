// Experimental executor: only fixed-target labels, no arbitrary scripts.
module.exports = function createLabels(ps, session, options={}) {
  const nativeSelection=options.nativeSelection===true;
  const extendedColors=extendedLabelColors(options.hostVersion);
  const layerSearch=createLayerSearch(ps,session,captureSelection);
  const textTools=createTextTools(ps,session,captureSelection,creationTree);
  const layerView=createLayerView(ps,session,captureSelection);
  // A bounded work-planning hint only. Every result still reads and validates
  // current identity/count/locks; this never supplies target capabilities.
  const nativePlan={total:null,single:true};
  const caps=createCapsActions(ps,session,capture,selectionContext,capsTree,captureSelection,nativeSelection);
  const colors = new Set(['none','red','orange','yellowColor','grain','blue','violet','gray','seafoam','indigo','magenta','fuchsia']);
  const shapePaint=createShapePaint(ps,session,capture,selectionContext,options.legacyPreview===true);
  const paintClipboard=createPaintClipboard(ps,session,captureSelection,selectionContext,creationTree);
  const stability=createStabilityFixture(ps,session,capture,documentTarget);
  const quickCommands=['layer.new','layer.solid','layer.group','adjustment.curves',
    'adjustment.hueSaturation','adjustment.colorBalance','adjustment.blackWhite'];
  // Scalar lookup hints only, bounded for long sessions. A hint never supplies
  // metadata or membership: its exact ID must match in a NEW sibling collection
  // on every read. Reorder/delete/undo simply fall back to the original scan.
  const positionHints=new Map(),positionHintLimit=2048;
  let nativeHintsRetryAt=0;
  // Modal creation and Caps transactions call this reader. Read scalars, never
  // pixels, effects or names. Section ends are structural records, not layers.
  async function creationTree(doc) {
    const target=[{_ref:'document',_id:doc.id}];
    const [n]=await ps.action.batchPlay([{_obj:'get',_target:[{_ref:'property',_property:'numberOfLayers'},...target],_options:{dialogOptions:'dontDisplay'}}],{});
    const count=n && n.numberOfLayers;
    if(!Number.isInteger(count)||count<1||count>4096)throw Error('Native tree range unavailable');
    const background=!!doc.layers[doc.layers.length-1].isBackgroundLayer,first=background?0:1,total=count+(background?1:0);
    const [result]=await ps.action.batchPlay([{_obj:'multiGet',_target:{_ref:target},
      extendedReference:[['layerID','parentLayerID','layerSection','itemIndex'],{_obj:'layer',index:first,count:total}],
      options:{failOnMissingProperty:false,failOnMissingElement:false},_options:{dialogOptions:'dontDisplay'}}],{});
    if(!result||!Array.isArray(result.list)||result.list.length!==total)throw Error('Incomplete native tree');
    const records=result.list.slice().sort((a,b)=>(b&&b.itemIndex)-(a&&a.itemIndex));
    const ids=new Set(),groups=new Map(),stack=[],order=[];
    for(let i=0;i<records.length;i++) {
      const row=records[i],section=row && row.layerSection && row.layerSection._value;
      // AM references start at zero when a Background exists; the returned
      // itemIndex is still one-based (including group end records).
      if(!row||row.itemIndex!==total-i)throw Error('Invalid native tree indices');
      if(section==='layerSectionEnd') {
        if(!stack.length)throw Error('Unbalanced native tree');
        stack.pop();continue;
      }
      if(!['layerSectionStart','layerSectionContent'].includes(section)||!Number.isInteger(row.layerID)||row.layerID<=0||ids.has(row.layerID)||
          !Number.isInteger(row.parentLayerID)||(row.parentLayerID>0?row.parentLayerID:null)!==(stack.length?stack[stack.length-1]:null))
        throw Error('Invalid native tree membership');
      ids.add(row.layerID);order.push(row.layerID);groups.set(row.layerID,section==='layerSectionStart');
      if(section==='layerSectionStart')stack.push(row.layerID);
    }
    if(stack.length||!order.length)throw Error('Unbalanced native tree');
    return {order,groups,rows:records.filter(row=>row.layerSection._value!=='layerSectionEnd')};
  }
  async function capsTree(doc) {
    const tree=await creationTree(doc);
    // Full fresh tree supplies lookup indices only. The feedback reader still
    // verifies each hint against a fresh DOM collection and exact layer ID.
    const positions=new Map();
    for(const row of tree.rows) {
      const parent=row.parentLayerID>0?row.parentLayerID:null;
      const index=positions.get(parent)||0;positions.set(parent,index+1);
      rememberPosition(doc.id+':'+(parent===null?'root':parent)+':'+row.layerID,index);
    }
    return tree;
  }
  function rememberPosition(key,index) {
    positionHints.delete(key);positionHints.set(key,index);
    if(positionHints.size>positionHintLimit)positionHints.delete(positionHints.keys().next().value);
  }
  // DOM-only, read-only metadata. No batchPlay, executeAsModal, pixels or
  // thumbnails on the menu path. The executor uses this same eligibility rule.
  function selectionContext(quickOnly=false,onPositionMiss=null) {
    const reader=readSelectionContext(quickOnly,onPositionMiss);
    let step;do {step=reader.next();}while(!step.done);
    return step.value;
  }
  // The same reader supplies synchronous transaction validation and cooperative
  // background/menu snapshots. Yield only between completed property reads;
  // retain all original checks, including the final fresh selection identity.
  function* readSelectionContext(quickOnly=false,onPositionMiss=null,host=ps) {
    if (!host.app.documents.length) return {session,document_id:null,layer_ids:[],kind:'none',mask:0};
    const doc=host.app.activeDocument,sourceSelection=doc.activeLayers,selected=[],k=host.constants.LayerKind;
    for(let i=0;i<sourceSelection.length;i++){selected.push(sourceSelection[i]);if(i%4===3)yield;}
    // Host-backed DOM getters are costly. Reuse reads only within this one
    // snapshot, never across requests or the final selection identity check.
    // Cross-read position hints are separately revalidated against fresh IDs.
    const properties=new Map(),siblingLists=new Map();
    function read(layer,key) {
      let values=properties.get(layer);
      if(!values) {values=Object.create(null);properties.set(layer,values);}
      if(!Object.prototype.hasOwnProperty.call(values,key))values[key]=layer[key];
      return values[key];
    }
    function* siblings(parent) {
      if(!siblingLists.has(parent)) {
        const source=parent?read(parent,'layers'):doc.layers;
        // This fresh Photoshop Layers collection captures the ordered sibling
        // IDs. Indexing constructs a host-backed Layer, so materialize only
        // the indices required by membership, adjacency and edge checks.
        // The source and objects live for this snapshot only, never across
        // requests; every selected member and the final identity are verified.
        const count=source.length,objects=new Map();
        // Tiny lists and large selections keep the original host-read path:
        // materialization has little avoidable cost in those cases.
        const eager=count<=8 || selected.length>count/2?[]:null;
        if(eager)for(let i=0;i<count;i++){eager.push(source[i]);if(i%4===3)yield;}
        const at=index=>{
          if(eager)return eager[index];
          if(!objects.has(index))objects.set(index,source[index]);
          return objects.get(index);
        };
        const prefix=doc.id+':'+(parent?read(parent,'id'):'root')+':';
        siblingLists.set(parent,{length:count,at,original:!!eager,raw:eager,
          parentId:parent?read(parent,'id'):null,
          idAt(index) {
            const id=read(at(index),'id');rememberPosition(prefix+id,index);return id;
          },
          hintedIndex(id) {
            const index=positionHints.get(prefix+id);
            return Number.isInteger(index) && index>=0 && index<count && this.idAt(index)===id?index:-1;
          },
          findIndex(predicate){for(let i=0;i<count;i++)if(predicate(at(i)))return i;return -1;}});
      }
      return siblingLists.get(parent);
    }
    function* singlePosition(order,id) {
      if(order.original){for(let i=0;i<order.length;i++){
        if(read(order.at(i),'id')===id)return i;if(i%4===3)yield;
      }return -1;}
      if(!order.length)return -1;
      const hinted=order.hintedIndex(id);if(hinted>=0)return hinted;
      if(order.idAt(0)===id)return 0;
      const last=order.length-1,edge=order.at(last);
      if(order.idAt(last)===id)return last;
      if(last>0 && read(edge,'isBackgroundLayer') && order.idAt(last-1)===id)return last-1;
      positionMiss(order);
      for(let i=0;i<order.length;i++){
        if(order.idAt(i)===id)return i;if(i%4===3)yield;
      }
      return -1;
    }
    function positionMiss(order) {
      if(onPositionMiss && order.length>=32)onPositionMiss({session,document_id:doc.id,
        layer_ids:ids,parent_id:order.parentId,sibling_count:order.length});
    }
    function layerKind(layer) {
      const background=read(layer,'isBackgroundLayer');
      return selectionKind(k,background?undefined:read(layer,'kind'),background);
    }
    const kinds=[];
    for(let i=0;i<selected.length;i++) {kinds.push(layerKind(selected[i]));if(i%4===3)yield;}
    const ids=[];for(let i=0;i<selected.length;i++){ids.push(read(selected[i],'id'));if(i%4===3)yield;}
    ids.sort((a,b)=>a-b);const background=kinds.includes('background');
    const parents=[];
    for(let i=0;i<selected.length;i++) {parents.push(read(selected[i],'parent'));if(i%4===3)yield;}
    const parentIds=parents.map(parent=>parent?read(parent,'id'):null);
    const sameParent=parentIds.every(id=>id===parentIds[0]);
    let locked=false,parentLocked=false;
    let lockReads=0;
    for(const layer of selected) {
      for(let node=layer,depth=0;node && depth<=64;node=read(node,'parent'),depth++) {
        const allLocked=read(node,'allLocked');
        if(depth<64 && (allLocked || read(node,'positionLocked')))locked=true;
        if(depth>0 && allLocked)parentLocked=true;
        if(++lockReads%4===0)yield;
      }
    }
    let mask=0,singleIndex;
    if(!quickOnly && selected.length && !kinds.includes('unknown')) {
      if(!background)mask|=2;
      if(selected.length===1) {
        const active=selected[0],order=yield* siblings(parents[0]);
        if(read(active,'kind')===k.GROUP) {if(read(active,'layers').length)mask|=1;}
        else {
          const index=yield* singlePosition(order,ids[0]);
          singleIndex=index;
          if(index>=0 && ((index>0 && read(order.at(index-1),'kind')!==k.GROUP) ||
              (index+1<order.length && read(order.at(index+1),'kind')!==k.GROUP)))mask|=1;
        }
      }
      if(!background && !locked && sameParent) {
        const order=yield* siblings(parents[0]),selectedIds=new Set(ids);
        // Photoshop sibling IDs are unique. A single ordinary layer was
        // already located for adjacency above; do not read every remaining
        // sibling ID again just to establish membership. Groups/multi-select
        // still establish membership, stopping once every selected ID is found.
        if(!order.original && singleIndex===undefined && ids.length===1)singleIndex=yield* singlePosition(order,ids[0]);
        let chosenCount=singleIndex===undefined?0:(singleIndex<0?0:1);
        if(order.original){chosenCount=0;for(let i=0;i<order.length;i++){
          if(selectedIds.has(read(order.at(i),'id')))chosenCount++;if(i%4===3)yield;
        }}
        else if(singleIndex===undefined && order.length) {
          const found=new Set(),first=order.idAt(0),last=order.idAt(order.length-1);
          if(selectedIds.has(first))found.add(first);
          if(selectedIds.has(last))found.add(last);
          for(let i=0;i<ids.length;i++){
            const id=ids[i];if(!found.has(id) && order.hintedIndex(id)>=0)found.add(id);if(i%4===3)yield;
          }
          if(found.size<ids.length)positionMiss(order);
          const reverse=selectedIds.has(last) && !selectedIds.has(first);
          for(let i=reverse?order.length-1:0;found.size<ids.length && i>=0 && i<order.length;i+=reverse?-1:1) {
            const id=order.idAt(i);if(selectedIds.has(id))found.add(id);
            if(i%4===3)yield;
          }
          chosenCount=found.size;
        }
        if(chosenCount===ids.length) {
          // To establish an edge, stop at the first unselected non-background
          // layer. There is no need to query every sibling's expensive
          // isBackgroundLayer getter; unusual background positions still work.
          function* atEdge(bottom) {
            let count=0;
            for(let i=bottom?order.length-1:0;i>=0 && i<order.length;i+=bottom?-1:1) {
              const layer=order.at(i);
              if(selectedIds.has(read(layer,'id'))) {if(++count===ids.length)return true;}
              else if(!read(layer,'isBackgroundLayer'))return false;
              if(i%4===3)yield;
            }
            return false;
          }
          if(!(yield* atEdge(false)))mask|=4;
          if(!(yield* atEdge(true)))mask|=8;
        }
      }
    }
    if(host.app.activeDocument.id!==doc.id || Array.from(doc.activeLayers,l=>l.id).sort((a,b)=>a-b).join(',')!==ids.join(','))
      throw Error('Layer context changed during read');
    const quickMask=selectionQuickMask(host.constants,doc.mode,doc.bitsPerChannel,ids,kinds,background,locked,parentLocked,sameParent);
    return {session,document_id:doc.id,layer_ids:ids,selection_count:ids.length,
      kind:kinds.every(kind=>kind===kinds[0])?(kinds[0] || 'none'):'mixed',
      types:[...new Set(kinds)],background,locked,same_parent:sameParent,mask,quick_mask:quickMask};
  }
  async function selectionContextFresh(checkpoint=null) {
    if(nativeSelection)return nativeSelectionContext(ps,session,readSelectionContext,checkpoint,true,nativePlan);
    async function read(onMiss=null) {
      if(!checkpoint)return selectionContext(false,onMiss);
      await checkpoint();
      const reader=readSelectionContext(false,onMiss);
      let step;do {step=reader.next();await checkpoint();}while(!step.done);
      return step.value;
    }
    // Warm hints, small lists, edges and majority selections keep the original
    // synchronous DOM path. Only a genuine large-list miss requests native IDs.
    if(Date.now()<nativeHintsRetryAt)return read();
    let missing;const miss={};
    try {return await read(value=>{missing=value;throw miss;});}
    catch(error) {if(error!==miss)throw error;}
    let hints=[];
    try {
      const target=[{_ref:'document',_id:missing.document_id}];
      const [size]=await ps.action.batchPlay([{_obj:'get',_target:[
        {_ref:'property',_property:'numberOfLayers'},...target],_options:{dialogOptions:'dontDisplay'}}],{});
      // Bound native work before asking for the list, including group markers.
      // Unsupported, malformed or huge documents safely use the original scan.
      const count=size && size.numberOfLayers;
      if(Number.isInteger(count) && count>0 && count<=4096) {
        const [result]=await ps.action.batchPlay([{_obj:'multiGet',_target:{_ref:target},
          extendedReference:[['layerID','parentLayerID','layerSection','itemIndex'],{_obj:'layer',index:1,count}],
          options:{failOnMissingProperty:false,failOnMissingElement:false},_options:{dialogOptions:'dontDisplay'}}],{});
        if(!result || !Array.isArray(result.list) || result.list.length>4096)throw Error('Native position hints unavailable');
        const rows=[],seenIds=new Set(),seenIndices=new Set();
        for(const row of result.list) {
          const section=row && row.layerSection && row.layerSection._value;
          if(section==='layerSectionEnd')continue;
          if(!row || !['layerSectionContent','layerSectionStart'].includes(section) ||
              !Number.isInteger(row.layerID) || row.layerID<=0 || !Number.isInteger(row.parentLayerID) ||
              !Number.isInteger(row.itemIndex) || row.itemIndex<=0 || seenIds.has(row.layerID) || seenIndices.has(row.itemIndex))
            throw Error('Incomplete native position hints');
          seenIds.add(row.layerID);seenIndices.add(row.itemIndex);
          if((row.parentLayerID>0?row.parentLayerID:null)===missing.parent_id)rows.push(row);
        }
        rows.sort((a,b)=>b.itemIndex-a.itemIndex);
        if(rows.length<=missing.sibling_count)hints=rows.map((row,index)=>[row.layerID,index]);
      }
    } catch(_) {nativeHintsRetryAt=Date.now()+5000;}
    // Async read completion may follow a user selection/document change.
    // Never refresh old capabilities or apply hints to a different identity.
    if(!selectionMatches(missing))throw Error('Layer context changed during read');
    const prefix=missing.document_id+':'+(missing.parent_id===null?'root':missing.parent_id)+':';
    for(const [id,index] of hints)rememberPosition(prefix+id,index);
    // Large lists can exceed the LRU capacity; retain selected hints last.
    const selectedIds=new Set(missing.layer_ids);
    for(const [id,index] of hints)if(selectedIds.has(id))rememberPosition(prefix+id,index);
    // Fetch NEW DOM objects/properties after the await. Native values are only
    // candidate indices: hintedIndex still verifies each exact fresh DOM ID.
    return read();
  }
  async function capture() {
    if(nativeSelection)return nativeCapture(ps,session);
    if (!ps.app.documents.length) throw Error('No document');
    const doc = ps.app.activeDocument;
    const ids = doc.activeLayers.map(layer => layer.id).sort((a,b)=>a-b);
    if(ids.length===1) {
      // A single selected layer is necessarily the active layer. Keep the
      // fresh document/selection readback, avoid a host batchPlay round trip.
      if(ps.app.activeDocument.id!==doc.id || doc.activeLayers.map(layer=>layer.id).join(',')!==ids.join(','))
        throw Error('Target changed or no selected layers');
      return {session,document_id:doc.id,active_layer_id:ids[0],layer_ids:ids};
    }
    const [active] = await ps.action.batchPlay([{_obj:'get',_target:[
      {_ref:'property',_property:'layerID'},{_ref:'layer',_enum:'ordinal',_value:'targetEnum'}]}], {});
    if (ps.app.activeDocument.id !== doc.id || !ids.length || !ids.includes(active.layerID))
      throw Error('Target changed or no selected layers');
    return {session, document_id:doc.id, active_layer_id:active.layerID, layer_ids:ids};
  }
  async function captureSelection() {
    if(nativeSelection)return nativeCapture(ps,session);
    if(!ps.app.documents.length)throw Error('No document');
    const doc=ps.app.activeDocument;
    if(doc.activeLayers.length<8)return capture();
    try {
      const [chosen,active]=await ps.action.batchPlay([
        {_obj:'get',_target:[{_ref:'property',_property:'targetLayersIDs'},{_ref:'document',_id:doc.id}],_options:{dialogOptions:'dontDisplay'}},
        {_obj:'get',_target:[{_ref:'property',_property:'layerID'},{_ref:'layer',_enum:'ordinal',_value:'targetEnum'}],_options:{dialogOptions:'dontDisplay'}}
      ],{});
      const refs=chosen?.targetLayersIDs;
      if(!Array.isArray(refs) || !refs.length || refs.some(v=>v?._ref!=='layer' || !Number.isInteger(v._id) || v._id<=0))throw Error('Native selection unavailable');
      const ids=refs.map(v=>v._id).sort((a,b)=>a-b);
      if(new Set(ids).size!==ids.length || ids.length!==doc.activeLayers.length || !ids.includes(active?.layerID) || ps.app.activeDocument.id!==doc.id)throw Error('Native selection identity invalid');
      return {session,document_id:doc.id,active_layer_id:active.layerID,layer_ids:ids};
    } catch(_) {return capture();} // Read-only compatibility fallback, never a write retry.
  }
  function selectionMatches(value) {
    if(!value || value.session!==session || !Array.isArray(value.layer_ids) || !ps.app.documents.length)return false;
    const doc=ps.app.activeDocument;
    const ids=Array.from(doc.activeLayers,l=>l.id).sort((a,b)=>a-b);
    return ps.app.activeDocument.id===doc.id && doc.id===value.document_id && ids.join(',')===value.layer_ids.join(',');
  }
  async function inheritedLabels(actual,host=ps) {
    const doc=host.app.activeDocument,selected=Array.from(doc.activeLayers),known=new Map();
    if(doc.id!==actual.document_id || selected.map(l=>l.id).sort((a,b)=>a-b).join(',')!==actual.layer_ids.join(','))
      throw Error('Label inheritance target changed');
    let inherited=0;
    for(const layer of selected) {
      let parent=layer.parent,colored=false;const path=[],seen=new Set();
      while(parent) {
        const id=parent.id;
        if(!Number.isInteger(id)||id<=0||seen.has(id))throw Error('Invalid label parent chain');
        seen.add(id);
        if(known.has(id)){colored=known.get(id);break;}
        const result=await host.action.batchPlay([{_obj:'get',_target:[{_ref:'property',_property:'color'},
          {_ref:'layer',_id:id},{_ref:'document',_id:doc.id}],_options:{dialogOptions:'dontDisplay'}}],{});
        const value=result?.[0]?.color?._value;
        if(!Array.isArray(result) || result.length!==1 || result[0]?._obj==='error' || typeof value!=='string' || !value)
          throw Error('Parent label readback unavailable');
        path.push(id);
        if(value!=='none'){colored=true;break;}
        parent=parent.parent;
      }
      for(const id of path)known.set(id,colored);
      if(colored)inherited++;
    }
    if(!selectionMatches(actual))throw Error('Label inheritance target changed');
    return inherited;
  }
  async function setLabel(request,previewIO=null) {
    if(request.command==='text.replace')return textTools.replace(request);
    if(request.command==='text.split')return textTools.split(request);
    if(request.command==='layer.focus')return layerSearch.focus(request);
    if(request.command==='layer.frame')return layerView.frame(request,previewIO);
    if(request.command==='layer.thumbnail')throw Error('非静默缩略图已停用，未提交操作');
    if(request.command==='stability.internal')return stability.execute(request,setLabel);
    if(['shape.copyPaint','shape.pastePaint'].includes(request.command))return paintClipboard.execute(request);
    if(caps.commands.includes(request.command))return caps.execute(request);
    if(request.command==='shape.paint' || request.command==='shape.preview')return shapePaint.apply(request,previewIO);
    const command = request.command || 'label';
    const quickIndex=quickCommands.indexOf(command);
    if (!['label','select','moveToTop','moveToBottom'].includes(command) && quickIndex<0) throw Error('Unknown command');
    if (command === 'label' && !colors.has(request.color)) throw Error('Unknown label color');
    if(command==='label' && ['seafoam','indigo','magenta','fuchsia'].includes(request.color) && !extendedColors)throw Error('此标签颜色需要Photoshop25.2或更新版本');
    const expected = request.target;
    if (!expected || expected.session !== session) throw Error('Plugin session changed');
    const started=Date.now(),timings={};
    return ps.core.executeAsModal(async context => {
      timings.modal_wait_ms=Date.now()-started;
      if (context.isCancelled || Date.now() > request.expires_at_ms) throw Error('Cancelled or expired');
      let stageStarted=Date.now();
      const actual = await capture();
      if (actual.document_id !== expected.document_id || actual.active_layer_id !== expected.active_layer_id ||
          !Array.isArray(expected.layer_ids) || actual.layer_ids.join(',') !== expected.layer_ids.join(','))
        throw Error('Document or layer target changed');
      if (context.isCancelled || Date.now() > request.expires_at_ms) throw Error('Cancelled or expired');
      timings.target_ms=Date.now()-stageStarted;stageStarted=Date.now();
      // A locked Background has no color property in PS 24. Setting it as a
      // layer converts it and allocates a new ID. Reject the whole selection
      // before history/write rather than silently unlocking or partially coloring.
      if (command === 'label' && ps.app.activeDocument.activeLayers.some(layer => layer.isBackgroundLayer))
        throw Error('背景图层不支持颜色标签，请先解锁背景图层；本次未修改任何图层');
      // Creation needs its own mask, not neighbouring-layer/sort capabilities.
      // All document/selection/type/lock/mode/depth checks remain fresh.
      const eligibility=selectionContext(quickIndex>=0),bit={select:1,label:2,moveToTop:4,moveToBottom:8}[command];
      if(quickIndex>=0 ? !(eligibility.quick_mask & (1<<quickIndex)) : !(eligibility.mask & bit))throw Error(quickIndex>=0?
        '当前文档模式／位深或选择不支持此操作；打组需同一父组、非背景且未锁定':command==='label'?'当前图层类型未识别，未修改图层':
        command==='select'?'当前选择没有可选的组内／相邻图层':
        '当前选择不可排序：背景、位置锁定、跨组选择或已经位于目标位置');
      timings.eligibility_ms=Date.now()-stageStarted;stageStarted=Date.now();
      const history = await context.hostControl.suspendHistory({documentID:actual.document_id,name:quickIndex>=0?'Menu '+command:'Menu layer label'});
      timings.history_open_ms=Date.now()-stageStarted;stageStarted=Date.now();
      try {
        if(quickIndex>=0) {
          const doc=ps.app.activeDocument,k=ps.constants.LayerKind;
          const kinds=[k.NORMAL,k.SOLIDFILL,k.GROUP,k.CURVES,k.HUESATURATION,k.COLORBALANCE,k.BLACKANDWHITE];
          // Reuse original type reads inside this modal; reread the actual
          // tree/order and every new node, without another full anchor walk.
          const knownGroups=new Map(),knownLayers=new Map(),largeTree={};let visited=0;
          function allIds(layers,bounded=false) {return Array.from(layers).flatMap(l=>{
            if(bounded && visited>=32)throw largeTree;
            visited++;
            const id=l.id,group=knownGroups.has(id)?knownGroups.get(id):l.kind===k.GROUP;
            knownGroups.set(id,group);knownLayers.set(id,l);
            return [id,...(group?allIds(l.layers,bounded):[])];
          });}
          let before,treeReader='dom';
          try {before=allIds(doc.layers,true);}catch(error) {
            if(error!==largeTree)throw error;
            try {
              const native=await creationTree(doc);
              // The partial DOM prefix is independently known in this modal.
              // Never accept an alternate tree that contradicts those reads.
              if([...knownGroups].some(([id,group])=>native.groups.get(id)!==group)||
                  [...knownLayers.keys()].join(',')!==native.order.slice(0,knownLayers.size).join(','))throw Error('Native tree prefix differs');
              before=native.order;knownGroups.clear();for(const [id,group] of native.groups)knownGroups.set(id,group);
              treeReader='batch';
            }catch(_) {before=allIds(doc.layers);treeReader='dom-fallback';}
          }
          // A read failure may fall back; a changed/cancelled write target may
          // not. No mutation has happened yet, and no request is replayed.
          const fresh=await capture();
          if(context.isCancelled||Date.now()>request.expires_at_ms||fresh.document_id!==actual.document_id||
              fresh.active_layer_id!==actual.active_layer_id||fresh.layer_ids.join(',')!==actual.layer_ids.join(','))throw Error('Creation target changed or expired');
          const anchor=treeReader==='dom'?knownLayers.get(actual.active_layer_id):Array.from(doc.activeLayers).find(l=>l.id===actual.active_layer_id);
          if(!anchor)throw Error('新建图层目标已改变');
          // A selected group receives its new layer inside, like native PS.
          if(!before.includes(anchor.id)||knownGroups.get(anchor.id)!==(anchor.kind===k.GROUP))throw Error('Creation anchor membership changed');
          const insideGroup=command!=='layer.group' && knownGroups.get(anchor.id);
          const parentLayer=insideGroup?anchor:anchor.parent;
          const parentId=parentLayer?parentLayer.id:null;
          const siblingLayers=()=>parentLayer?parentLayer.layers:doc.layers;
          const siblings=Array.from(siblingLayers(),l=>l.id);
          timings.tree_before_ms=Date.now()-stageStarted;stageStarted=Date.now();
          let placementMoves=0,placeholderUsed=false;
          function position(layer) {
            const parent=layer.parent,order=Array.from(siblingLayers(),l=>l.id);
            const index=order.indexOf(layer.id),anchorIndex=order.indexOf(anchor.id);
            const positioned=(parent?parent.id:null)===parentId && index>=0 &&
              (insideGroup?index===0:anchorIndex>=0 && index+1===anchorIndex);
            if(!positioned || order.filter(id=>id!==layer.id).join(',')!==siblings.join(','))
              throw Error('新建图层位置核验失败：PS 原生创建位置不符，已回滚；未移动图层');
          }
          let created;
          if(command==='layer.new') {
            // Leave name unspecified: Photoshop owns its localized Layer N
            // naming/counter, exactly as its native New Layer command does.
            const results=await ps.action.batchPlay([{_obj:'make',_target:[{_ref:'layer'}],_options:{dialogOptions:'dontDisplay'}}],{});
            if(results.some(r=>r._obj==='error'))throw Error('Photoshop 拒绝新建空白图层');
            created=doc.activeLayers[0];
            if(!created || before.includes(created.id))throw Error('新建空白图层读取失败');
          }
          else if(command==='layer.group')created=await doc.createLayerGroup({fromLayers:Array.from(doc.activeLayers)});
          else {
            // Photoshop may consume the active empty pixel layer when making
            // a fill/adjustment. Give it a disposable layer inside this same
            // history transaction so the user's selected layer is preserved.
            // Only an empty/uncertain pixel layer needs a disposable anchor.
            // Bounds without effects prove pixels exist; non-pixel/background
            // layers are not consumed by Photoshop's native make command.
            let mayConsume=anchor.kind===k.NORMAL && !anchor.isBackgroundLayer;
            if(mayConsume) {
              try {
                const b=anchor.boundsNoEffects;
                if(b && ['left','top','right','bottom'].every(key=>Number.isFinite(b[key])) && b.right>b.left && b.bottom>b.top)
                  mayConsume=false;
              } catch(_) {} // Unknown bounds keep the original protection.
            }
            let placeholder=null;
            if(mayConsume) {
              // Native make inserts above the selected layer. DOM createLayer
              // starts at the document top and its later move is visibly jarring.
              const made=await ps.action.batchPlay([{_obj:'make',_target:[{_ref:'layer'}],_options:{dialogOptions:'dontDisplay'}}],{immediateRedraw:false});
              if(made.some(r=>r._obj==='error'))throw Error('PS 拒绝创建保护层');
              placeholder=doc.activeLayers[0];placeholderUsed=true;
              if(!placeholder || before.includes(placeholder.id))throw Error('保护层创建读取失败');
              position(placeholder);
            }
            let type;
            if(command==='layer.solid') {
              const rgb=ps.app.foregroundColor.rgb;
              if(!['red','green','blue'].every(key=>Number.isFinite(rgb[key]) && rgb[key]>=0 && rgb[key]<=255))throw Error('前景色读取失败');
              type={_obj:'solidColorLayer',color:{_obj:'RGBColor',red:rgb.red,grain:rgb.green,blue:rgb.blue}};
            } else {
              const defaults={
                'adjustment.curves':{_obj:'curves',presetKind:{_enum:'presetKindType',_value:'presetKindDefault'}},
                'adjustment.hueSaturation':{_obj:'hueSaturation',presetKind:{_enum:'presetKindType',_value:'presetKindDefault'},colorize:false},
                'adjustment.colorBalance':{_obj:'colorBalance',shadowLevels:[0,0,0],midtoneLevels:[0,0,0],highlightLevels:[0,0,0],preserveLuminosity:true},
                'adjustment.blackWhite':{_obj:'blackAndWhite',red:40,yellow:60,grain:40,cyan:60,blue:20,magenta:80,useTint:false}
              };
              type=defaults[command];
            }
            const obj=command==='layer.solid'?'contentLayer':'adjustmentLayer';
            const results=await ps.action.batchPlay([{_obj:'make',_target:[{_ref:obj}],using:{_obj:obj,type},_options:{dialogOptions:'dontDisplay'}}],{});
            if(results.some(r=>r._obj==='error'))throw Error('Photoshop 拒绝创建图层');
            created=doc.activeLayers[0];
            if(created && placeholder && placeholder.id!==created.id && allIds(doc.layers).includes(placeholder.id))await placeholder.delete();
          }
          timings.creation_ms=Date.now()-stageStarted;stageStarted=Date.now();
          if(created && command!=='layer.group')position(created);
          const afterIds=allIds(doc.layers),after=new Set(afterIds),original=new Set(before);
          const added=[...after].filter(id=>!original.has(id));
          const createdId=created && created.id,createdKind=created && created.kind;
          if(!created || createdKind!==kinds[quickIndex] || added.length!==1 || added[0]!==createdId ||
              before.some(id=>!after.has(id)))throw Error('新图层类型／数量核验失败：'+JSON.stringify({actual:created && created.kind,expected:kinds[quickIndex],id:created && created.id,added,before,after:[...after]}));
          if(command!=='layer.group' && afterIds.filter(id=>id!==created.id).join(',')!==before.join(','))throw Error('原图层顺序核验失败');
          if(command==='layer.group' && Array.from(created.layers,l=>l.id).sort((a,b)=>a-b).join(',')!==actual.layer_ids.join(','))
            throw Error('打组成员核验失败');
          const name=created.name,parent=created.parent;
          const result={verified:true,command,layer_id:createdId,name,kind:createdKind,
            parent_id:parent?parent.id:null,anchor_id:anchor.id,count:command==='layer.group'?actual.layer_ids.length:1,
            placement_moves:placementMoves,placeholder_used:placeholderUsed,tree_reader:treeReader,tree_count:before.length};
          timings.verify_ms=Date.now()-stageStarted;stageStarted=Date.now();
          // Read the new state while the same PS modal still owns the target.
          // Metadata failure cannot undo a successfully verified creation:
          // the caller falls back to its normal full post-operation snapshot.
          // The native strategy must read fresh capabilities after modal exit;
          // it cannot reuse this prepared DOM context. Avoid building a full
          // snapshot that main.js always discards. Keep the DOM strategy's
          // existing prepared-context identity check and all write validation.
          if(!nativeSelection)try {result.selection_context=selectionContext();}catch(_) {}
          result.selection_feedback=nativeSelection?'fresh-native-after-modal':'prepared-dom';
          timings.selection_prepare_ms=Date.now()-stageStarted;stageStarted=Date.now();
          await context.hostControl.resumeHistory(history,true);
          timings.history_commit_ms=Date.now()-stageStarted;
          return {...result,timings_ms:timings};
        }
        if (command !== 'label') {
          const doc = ps.app.activeDocument;
          function find(layers,id) { for(const layer of layers) { if(layer.id===id)return layer; if(layer.kind===ps.constants.LayerKind.GROUP) {const hit=find(layer.layers,id);if(hit)return hit;} } }
          const active=find(doc.layers,actual.active_layer_id);
          const siblings=Array.from(active.parent ? active.parent.layers : doc.layers);
          let affected=[];
          if(command==='select') {
            let selected;
            if(active.kind===ps.constants.LayerKind.GROUP) selected=Array.from(active.layers);
            else { const index=siblings.findIndex(x=>x.id===active.id);let start=index,end=index;
              while(start>0 && siblings[start-1].kind!==ps.constants.LayerKind.GROUP)start--;
              while(end+1<siblings.length && siblings[end+1].kind!==ps.constants.LayerKind.GROUP)end++;
              selected=siblings.slice(start,end+1);
            }
            if(!selected.length)throw Error('No layers in group');
            for(let i=0;i<selected.length;i++) {
              const d={_obj:'select',_target:[{_ref:'layer',_id:selected[i].id}],makeVisible:false};
              if(i)d.selectionModifier={_enum:'selectionModifierType',_value:'addToSelection'};
              const result=await ps.action.batchPlay([d],{});if(result.some(x=>x._obj==='error'))throw Error('Selection failed');
            }
            affected=selected.map(x=>x.id).sort((a,b)=>a-b);
            if((await capture()).layer_ids.join(',')!==affected.join(','))throw Error('Selection readback mismatch');
          } else {
            const selected=siblings.filter(x=>actual.layer_ids.includes(x.id));
            if(selected.length!==actual.layer_ids.length || selected.some(x=>x.isBackgroundLayer))throw Error('Select movable layers within one parent');
            const others=siblings.filter(x=>!actual.layer_ids.includes(x.id));
            if(others.length) {
              const top=command==='moveToTop',anchor=top?others[0]:others[others.length-1];
              const order=top?selected:[...selected].reverse();
              const placement=top||anchor.isBackgroundLayer?ps.constants.ElementPlacement.PLACEBEFORE:ps.constants.ElementPlacement.PLACEAFTER;
              // Bottom above a locked background must preserve the original order.
              for(const layer of (!top&&anchor.isBackgroundLayer?selected:order))await layer.move(anchor,placement);
            }
            affected=selected.map(x=>x.id);
            const reordered=Array.from(active.parent?active.parent.layers:doc.layers).filter(x=>!x.isBackgroundLayer).map(x=>x.id);
            const edge=command==='moveToTop'?reordered.slice(0,affected.length):reordered.slice(-affected.length);
            if(edge.join(',')!==affected.join(','))throw Error('Order readback mismatch');
          }
          await context.hostControl.resumeHistory(history,true);
          return {verified:true,count:affected.length,command};
        }
        const results = await ps.action.batchPlay([{_obj:'set',_target:[{_ref:'layer',_enum:'ordinal',_value:'targetEnum'}],
          to:{_obj:'layer',color:{_enum:'color',_value:request.color}},_options:{dialogOptions:'dontDisplay'}}], {});
        if (results.some(result => result._obj === 'error')) throw Error('Photoshop rejected label');
        const verified = await ps.action.batchPlay(actual.layer_ids.map(id=>({_obj:'get',
          _target:[{_ref:'property',_property:'color'},{_ref:'layer',_id:id},{_ref:'document',_id:actual.document_id}]})), {});
        if (verified.length !== actual.layer_ids.length || verified.some(v=>!v.color || v.color._value !== request.color))
          throw Error('Label readback mismatch');
        let inherited=0;
        if(request.color==='none' && ps.constants)inherited=await inheritedLabels(actual);
        await context.hostControl.resumeHistory(history, true);
        return {verified:true, count:actual.layer_ids.length, inherited, color:request.color, target:actual};
      } catch (error) {
        try {await context.hostControl.resumeHistory(history, false);}
        catch(_) {error.resultUnknown=true;}
        throw error;
      }
    }, {commandName:'Menu layer label', timeOut:0});
  }
  function documentTarget() {
    return {session, document_ids:Array.from(ps.app.documents,d=>d.id).sort((a,b)=>a-b),
      active_document_id:ps.app.documents.length?ps.app.activeDocument.id:null};
  }
  async function createDocument(request) {
    const preset=request.preset,expected=request.target;
    const units=['pixelsUnit','millimetersUnit','centimetersUnit','inchesUnit','pointsUnit','picasUnit'];
    if (!expected || expected.session!==session || !Array.isArray(expected.document_ids)) throw Error('Plugin session changed');
    if (!preset || !units.includes(preset.units) || !['width','height'].every(k=>
        Number.isFinite(preset[k]) && preset[k]>0 && preset[k]<=300000) || JSON.stringify(preset).length>3000)
      throw Error('Invalid recent document preset');
    return ps.core.executeAsModal(async context=>{
      const actual=documentTarget();
      if(context.isCancelled || Date.now()>request.expires_at_ms)throw Error('Cancelled or expired');
      if(actual.document_ids.join(',')!==expected.document_ids.join(',') || actual.active_document_id!==expected.active_document_id)
        throw Error('Document target changed');
      let created;
      try {
        // Pass PS's own preset JSON intact. This preserves physical units,
        // symbolic print defaults, profiles, artboards and guide settings.
        created=await ps.app.createDocument({presetJSON:JSON.stringify(preset)});
        if(!created || !Number.isInteger(created.id))throw Error('New document result unavailable');
        return {document_id:created.id,name:created.title,width:created.width,height:created.height,
          resolution:created.resolution,verified:true};
      } catch(error) {
        // Never close a pre-existing/user document or replay an uncertain make.
        const added=Array.from(ps.app.documents).filter(d=>!actual.document_ids.includes(d.id));
        if(added.length===1) {
          try {await added[0].closeWithoutSaving();}
          catch(_) {error.resultUnknown=true;}
        } else if(added.length)error.resultUnknown=true;
        throw error;
      }
    },{commandName:'Menu recent document',timeOut:0});
  }
  let interfaceState={ui_theme:'dark',ui_font_size:'small'};
  const interfaceRead={font:false,theme:false};let interfaceReadAt=0;
  async function interfacePreferences(checkpoint,once=false) {
    // Only UXP DOM/get reads. Never execute JSX or enter a modal to observe UI.
    if(checkpoint)await checkpoint();
    if(!once || !interfaceRead.font)try {
      const value=ps.app.preferences.interface.textFontSize, sizes=ps.constants.FontSize || {};
      for(const key of ['SMALL','MEDIUM','LARGE'])if(sizes[key]!==undefined && value===sizes[key]){
        interfaceState.ui_font_size=key.toLowerCase();interfaceRead.font=true;interfaceReadAt=Date.now();
      }
    } catch(_) {}
    if(checkpoint)await checkpoint();
    if(!once || !interfaceRead.theme)try {
      const [value]=await ps.action.batchPlay([{_obj:'get',_target:[
        {_ref:'property',_property:'interfacePrefs'},{_ref:'application',_enum:'ordinal',_value:'targetEnum'}],
        _options:{dialogOptions:'dontDisplay'}}],{});
      const raw=value?.interfacePrefs?.kuiBrightnessLevel;
      const name=typeof raw==='string'?raw:raw?._value;
      const theme={kPanelBrightnessOriginal:'light',kPanelBrightnessLightGray:'light-gray',
        kPanelBrightnessMediumGray:'dark-gray',kPanelBrightnessDarkGray:'dark'}[name];
      if(theme){interfaceState.ui_theme=theme;interfaceRead.theme=true;interfaceReadAt=Date.now();}
    } catch(_) {}
    if(checkpoint)await checkpoint();
    if(once)return {
      ...(interfaceRead.theme?{ui_theme:interfaceState.ui_theme}:{}),
      ...(interfaceRead.font?{ui_font_size:interfaceState.ui_font_size}:{}),
      ui_preferences_complete:interfaceRead.font&&interfaceRead.theme,ui_preferences_time_ms:interfaceReadAt};
    return {...interfaceState};
  }
  // Fixed read-only comparison on an owned fixture only. The before path is
  // UXP51's discarded synchronous DOM preparation followed by its fresh native
  // read; after omits only that preparation. Neither path publishes capabilities
  // or mutates Photoshop. This measures feedback reads, not a whole operation.
  async function feedbackReadComparison(run,order=0,checkpoint=null) {
    if(![0,1].includes(order))throw Error('Invalid feedback comparison order');
    const samples={},keys=['session','document_id','layer_ids','selection_count','kind','types','background','locked','same_parent','mask','quick_mask'];
    const signature=value=>JSON.stringify(keys.map(key=>value[key]));
    const now=()=>typeof performance!=='undefined' && typeof performance.now==='function'?performance.now():Date.now();
    for(const strategy of order?['after','before']:['before','after']) {
      stability.guard(run);
      const stats={calls:0,descriptors:0,layer_records:0,response_bytes:0,checkpoints:0};
      const measured={constants:ps.constants,action:{async batchPlay(commands,options){
        stats.calls++;stats.descriptors+=commands.length;
        const result=await ps.action.batchPlay(commands,options);
        for(const row of result)stats.layer_records+=Array.isArray(row?.list)?row.list.length:Number.isInteger(row?.layerID)?1:0;
        stats.response_bytes+=JSON.stringify(result).length;return result;
      }}};
      const yieldRead=checkpoint?async()=>{stats.checkpoints++;await checkpoint();}:null;
      const started=now();let prepared,discarded=0;
      // UXP51 prepared this DOM snapshot INSIDE its existing mutation modal.
      // Out-of-modal getters can be much slower, so they are not a valid
      // baseline. Acquire a read-only diagnostic scope, measure only its DOM
      // work, and report acquisition/exit separately from comparable work.
      if(strategy==='before')await ps.core.executeAsModal(context=>{
        if(context.isCancelled)throw Error('Feedback comparison cancelled');
        stability.guard(run);const at=now();prepared=selectionContext();discarded=now()-at;
      },{commandName:'Read isolated feedback baseline',timeOut:0});
      const freshStarted=now();
      const value=await nativeSelectionContext(measured,session,readSelectionContext,yieldRead,true,nativePlan);
      const completed=now();stability.guard(run);
      if(prepared && signature(prepared)!==signature(value))throw Error('Feedback comparison capabilities changed');
      samples[strategy]={value,ms:discarded+completed-freshStarted,wall_ms:completed-started,
        diagnostic_modal_overhead_ms:strategy==='before'?Math.max(0,freshStarted-started-discarded):0,
        discarded_dom_ms:discarded,fresh_native_ms:completed-freshStarted,
        prepared_snapshots:strategy==='before'?1:0,...stats};
    }
    return {samples,equivalent:signature(samples.before.value)===signature(samples.after.value),scope:'native-feedback-reads-only'};
  }
  async function styleReadComparison(run,order=0) {
    if(![0,1].includes(order))throw Error('Invalid style comparison order');
    return ps.core.executeAsModal(async context=>{
      stability.guard(run);const actual=await capture(),doc=ps.app.activeDocument,samples={};
      const now=()=>typeof performance!=='undefined' && typeof performance.now==='function'?performance.now():Date.now();
      for(const strategy of order?['after','before']:['before','after']) {
        if(context.isCancelled)throw Error('Style comparison cancelled');
        stability.guard(run);const stats={calls:0,descriptors:0,response_bytes:0},returns=[];
        const host={action:{async batchPlay(commands,options){
          stats.calls++;stats.descriptors+=commands.length;const result=await ps.action.batchPlay(commands,options);
          returns.push(result);return result;
        }}};
        const started=now();
        const values=strategy==='after'?await readLayerEffects(host,doc,actual.layer_ids):
          await host.action.batchPlay(actual.layer_ids.map(id=>({_obj:'get',_target:[{_ref:'layer',_id:id},{_ref:'document',_id:doc.id}],_options:{dialogOptions:'dontDisplay'}})),{});
        const ms=now()-started;
        for(const value of returns)stats.response_bytes+=encodeURIComponent(JSON.stringify(value)).replace(/%[0-9A-F]{2}/g,'_').length;
        if(values.length!==actual.layer_ids.length || values.some((v,i)=>v?._obj==='error' || v?.layerID!==actual.layer_ids[i]))throw Error('Style comparison target mismatch');
        samples[strategy]={ms,...stats,values:values.map(v=>({layerID:v.layerID,layerEffects:v.layerEffects??null}))};stability.guard(run);
      }
      return {samples,equivalent:JSON.stringify(samples.before.values)===JSON.stringify(samples.after.values),scope:'native-layer-style-reads-only'};
    },{commandName:'Read isolated layer styles',timeOut:0});
  }
  async function labelInheritanceComparison(run,order=0) {
    if(![0,1].includes(order))throw Error('Invalid label comparison order');
    return ps.core.executeAsModal(async context=>{
      stability.guard(run);const actual=await capture(),samples={};
      const now=()=>typeof performance!=='undefined' && typeof performance.now==='function'?performance.now():Date.now();
      for(const strategy of order?['after','before']:['before','after']) {
        if(context.isCancelled)throw Error('Label comparison cancelled');
        stability.guard(run);const stats={calls:0,descriptors:0,response_bytes:0};
        const host={app:ps.app,constants:ps.constants,action:{async batchPlay(commands,options){
          stats.calls++;stats.descriptors+=commands.length;
          const result=await ps.action.batchPlay(commands,options);stats.response_bytes+=JSON.stringify(result).length;return result;
        }}};
        const started=now();let inherited=0;
        if(strategy==='after')inherited=await inheritedLabels(actual,host);
        else {
          // Frozen UXP59 baseline; diagnostic reads only, never used for writes.
          async function scan(layers,parentColored) {for(const layer of layers) {
            const group=layer.kind===host.constants.LayerKind.GROUP;
            if(actual.layer_ids.includes(layer.id)&&parentColored)inherited++;
            if(group) {const [v]=await host.action.batchPlay([{_obj:'get',_target:[{_ref:'property',_property:'color'},{_ref:'layer',_id:layer.id}]}],{});
              await scan(layer.layers,parentColored || v.color._value!=='none');}
          }}
          await scan(host.app.activeDocument.layers,false);
        }
        samples[strategy]={inherited,ms:now()-started,...stats};stability.guard(run);
      }
      return {samples,equivalent:samples.before.inherited===samples.after.inherited,scope:'label-inheritance-reads-only'};
    },{commandName:'Read isolated label inheritance',timeOut:0});
  }
  async function nativeReadComparison(run,order=0,checkpoint=null) {
    if(![0,1,2].includes(order))throw Error('Invalid bounded native comparison order');
    const samples={};
    const strategies=['dom','full','targeted'];
    for(const strategy of [...strategies.slice(order),...strategies.slice(0,order)]){
      stability.guard(run);const stats={calls:0,descriptors:0,layer_records:0,response_bytes:0,checkpoints:0,checkpoint_ms:0};
      const measured={constants:ps.constants,action:{async batchPlay(commands,options){
        stats.calls++;stats.descriptors+=commands.length;
        const result=await ps.action.batchPlay(commands,options);
        for(const row of result)stats.layer_records+=Array.isArray(row?.list)?row.list.length:Number.isInteger(row?.layerID)?1:0;
        stats.response_bytes+=JSON.stringify(result).length;return result;
      }}};
      const now=()=>typeof performance!=='undefined' && typeof performance.now==='function'?performance.now():Date.now();
      const yieldRead=checkpoint?async()=>{const start=now();stats.checkpoints++;await checkpoint();stats.checkpoint_ms+=now()-start;}:null;
      const started=now();
      let value;
      if(strategy==='dom'){
        if(!yieldRead)value=selectionContext();
        else{const iterator=readSelectionContext();let step;do{step=iterator.next();await yieldRead();}while(!step.done);value=step.value;}
      }else value=await nativeSelectionContext(measured,session,readSelectionContext,yieldRead,strategy==='targeted',nativePlan);
      samples[strategy]={value,ms:now()-started,...stats};stability.guard(run);
    }
    const keys=['session','document_id','layer_ids','selection_count','kind','types','background','locked','same_parent','mask','quick_mask'];
    const signature=value=>JSON.stringify(keys.map(key=>value[key]));
    return {samples,equivalent:Object.values(samples).every(sample=>signature(sample.value)===signature(samples.dom.value)),
      constant_contract:{layer_kinds:Object.fromEntries(Object.entries(ps.constants.LayerKind))}};
  }
  return {extendedLabelColors:extendedColors,inspectLayerView:layerView.inspect,readText:textTools.read,searchLayers:layerSearch.search,previewLayer:layerSearch.preview,styleReadComparison,labelInheritanceComparison,feedbackReadComparison,nativeReadComparison,selectionContextNative:(checkpoint,full=false)=>nativeSelectionContext(ps,session,readSelectionContext,checkpoint,!full,nativePlan),stability,capture,captureSelection,setLabel,documentTarget,createDocument,selectionContext,selectionContextFresh,selectionMatches,interfacePreferences,observeShape:async(...args)=>{
    if(!nativeSelection)return shapePaint.observe(...args);
    // Only explicit optional editor reads enter a short read-only modal scope.
    // Idle selection polling uses batchPlay and never occupies modal execution.
    return ps.core.executeAsModal(context=>{if(context.isCancelled)throw Error("Read cancelled");return shapePaint.observe(...args);},{commandName:"Read menu color",timeOut:0});
  },
    paintClipboardAvailable:paintClipboard.available,
    prepareLegacyShape:shapePaint.prepareLegacy,finishLegacyShape:shapePaint.finishLegacy,
    prepareLegacyText:textTools.prepareObject,finishLegacyText:textTools.finishObject};
};

// A test owns one document in this plugin session. No name-based recovery,
// arbitrary scripts, original-document edits, or automatic cleanup after failure.
function createStabilityFixture(ps,session,capture,documentTarget) {
  let owner=null;
  const equal=(a,b)=>JSON.stringify(a)===JSON.stringify(b);
  function stamp(doc){return {id:doc.id,history:doc.activeHistoryState.id,
    selected:Array.from(doc.activeLayers,x=>x.id).sort((a,b)=>a-b)};}
  function originals(){return Array.from(ps.app.documents).filter(d=>!owner || d.id!==owner.document_id).map(stamp).sort((a,b)=>a.id-b.id);}
  function guard(run) {
    if(!owner || owner.run!==run || owner.session!==session)throw Error('Test ownership unavailable; no cleanup or replay');
    if(!ps.app.documents.length || ps.app.activeDocument.id!==owner.document_id)throw Error('Test document changed; stopped');
    if(!equal(originals(),owner.originals))throw Error('Original documents changed; stopped');
    if(!equal(stamp(ps.app.activeDocument),owner.expected))throw Error('Test selection/history changed; stopped');
    return ps.app.activeDocument;
  }
  async function read(run) {
    const doc=guard(run),target=await capture();guard(run);
    return {target,history_id:doc.activeHistoryState.id,layer_count:doc.layers.length,shape_id:owner.shape_id};
  }
  function accept(run) {
    if(!owner || owner.run!==run || !ps.app.documents.length || ps.app.activeDocument.id!==owner.document_id || !equal(originals(),owner.originals))
      throw Error('Test target changed after operation');
    owner.expected=stamp(ps.app.activeDocument);
  }
  async function batch(commands) {
    const result=await ps.action.batchPlay(commands,{});
    if(result.length!==commands.length || result.some(x=>!x || x._obj==='error'))throw Error('Test fixture operation rejected');
    return result;
  }
  async function execute(request,perform) {
    const run=request.stability_run,step=request.test_step;
    if(typeof run!=='string' || !/^[a-f0-9]{32}$/.test(run))throw Error('Invalid test identity');
    const fixtureCount=request.test_layers===undefined?5:request.test_layers;
    if(step==='fixture' && ![4,5,100,300].includes(fixtureCount))throw Error('Invalid bounded fixture size');
    if(step==='fixture' && request.test_background!==undefined && typeof request.test_background!=='boolean')throw Error('Invalid fixture background');
    if(step==='fixture' && request.test_effects!==undefined && typeof request.test_effects!=='boolean')throw Error('Invalid fixture effects');
    if(step==='fixture')return ps.core.executeAsModal(async context=>{
      if(owner && !Array.from(ps.app.documents).some(d=>d.id===owner.document_id) && equal(originals(),owner.originals))owner=null;
      if(owner)throw Error('An owned test document is still retained');
      if(context.isCancelled || Date.now()>request.expires_at_ms || !equal(documentTarget(),request.target))throw Error('Test document set changed');
      const saved=originals(),active=request.target.active_document_id;
      let doc;
      try {doc=await ps.app.createDocument({name:'151500px-menu Test '+run.slice(0,8),width:128,height:128,resolution:72,mode:'RGBColorMode',fill:request.test_background===true?'white':'transparent'});}
      catch(error){if(!equal(documentTarget(),request.target))error.resultUnknown=true;throw error;}
      if(!doc || !Number.isInteger(doc.id) || request.target.document_ids.includes(doc.id)){
        const error=Error('Test creation outcome unknown');error.resultUnknown=true;throw error;
      }
      owner={run,session,document_id:doc.id,originals:saved,original_active:active,expected:stamp(doc)};
      await batch([{_obj:'make',_target:[{_ref:'contentLayer'}],using:{_obj:'contentLayer',
        type:{_obj:'solidColorLayer',color:{_obj:'RGBColor',red:40,grain:90,blue:150}},
        shape:{_obj:'rectangle',top:{_unit:'pixelsUnit',_value:8},left:{_unit:'pixelsUnit',_value:8},
          bottom:{_unit:'pixelsUnit',_value:80},right:{_unit:'pixelsUnit',_value:80}}},_options:{dialogOptions:'dontDisplay'}}]);
      owner.shape_id=doc.activeLayers[0].id;
      owner.expected=stamp(doc);
      // A first content layer can replace the transparent starter layer.
      // Keep the historical default's three makes; explicit scale fixtures
      // instead fill to their requested REAL count on this host.
      const extra=request.test_layers===undefined?3:fixtureCount-doc.layers.length;
      if(extra<0 || extra>299)throw Error('Unexpected isolated fixture layer count');
      for(let i=0;i<extra;i++){
        if(context.isCancelled || ps.app.activeDocument.id!==owner.document_id)throw Error('Test creation cancelled or target changed');
        await batch([{_obj:'make',_target:[{_ref:'layer'}],_options:{dialogOptions:'dontDisplay'}}]);
        owner.expected=stamp(doc);
      }
      await batch([{_obj:'select',_target:[{_ref:'layer',_id:owner.shape_id}],makeVisible:false,_options:{dialogOptions:'dontDisplay'}}]);
      if(request.test_effects===true)await batch([{_obj:'set',_target:[{_ref:'property',_property:'layerEffects'},
        {_ref:'layer',_id:owner.shape_id},{_ref:'document',_id:doc.id}],to:{_obj:'layerEffects',scale:{_unit:'percentUnit',_value:100},
          frameFX:{_obj:'frameFX',enabled:true,style:{_enum:'frameStyle',_value:'outsetFrame'},paintType:{_enum:'frameFill',_value:'solidColor'},
            mode:{_enum:'blendMode',_value:'normal'},opacity:{_unit:'percentUnit',_value:100},size:{_unit:'pixelsUnit',_value:3},
            color:{_obj:'RGBColor',red:220,grain:40,blue:80}}},_options:{dialogOptions:'dontDisplay'}}]);
      accept(run);return {verified:true,...await read(run)};
    },{commandName:'Menu isolated test fixture',timeOut:0});
    guard(run);
    if(['label','layer.new','layer.group'].includes(step)) {
      const target=(await read(run)).target;
      const result=await perform({...request,command:step,target,...(step==='label'?{color:request.test_color}: {})});
      accept(run);return {...result,...await read(run)};
    }
    if(!['select-shape','select-many','select-root','lock-target','undo','finish'].includes(step))throw Error('Unknown test step');
    return ps.core.executeAsModal(async context=>{
      const doc=guard(run);
      if(context.isCancelled || Date.now()>request.expires_at_ms)throw Error('Test cancelled or expired');
      if(step==='finish') {
        const original=owner.original_active;
        await doc.closeWithoutSaving();
        owner=null;
        if(original!==null) {
          const target=Array.from(ps.app.documents).find(d=>d.id===original);
          if(!target)throw Error('Original document no longer available');
          ps.app.activeDocument=target;
        }
        return {verified:true,closed_owned_document:true,original_active_restored:true};
      }
      if(step==='select-shape')await batch([{_obj:'select',_target:[{_ref:'layer',_id:owner.shape_id}],makeVisible:false,_options:{dialogOptions:'dontDisplay'}}]);
      if(step==='select-root'){
        const index=request.test_index,count=doc.layers.length;
        if(count>300 || !Number.isInteger(index) || index<0 || index>=count)throw Error('Invalid bounded root selection');
        await batch([{_obj:'select',_target:[{_ref:'layer',_id:doc.layers[index].id}],makeVisible:false,_options:{dialogOptions:'dontDisplay'}}]);
      }
      if(step==='lock-target'){
        if(!['none','all','position'].includes(request.test_lock) || doc.activeLayers.length!==1)throw Error('Invalid test lock');
        const layer=doc.activeLayers[0];layer.allLocked=false;layer.positionLocked=false;
        if(request.test_lock==='all')layer.allLocked=true;
        if(request.test_lock==='position')layer.positionLocked=true;
      }
      if(step==='select-many') {
        const ids=Array.from(doc.layers).slice(0,32).map(l=>l.id);
        for(let i=0;i<ids.length;i++)await batch([{_obj:'select',_target:[{_ref:'layer',_id:ids[i]}],makeVisible:false,
          ...(i?{selectionModifier:{_enum:'selectionModifierType',_value:'addToSelection'}}:{}),_options:{dialogOptions:'dontDisplay'}}]);
      }
      if(step==='undo') {
        const states=Array.from(doc.historyStates),index=states.findIndex(s=>s.id===doc.activeHistoryState.id);
        if(index<1 || states[index-1].id!==request.restore_history)throw Error('Single undo checkpoint mismatch');
        doc.activeHistoryState=states[index-1];
        if(doc.activeHistoryState.id!==request.restore_history)throw Error('Single undo readback mismatch');
      }
      accept(run);return {verified:true,...await read(run)};
    },{commandName:'Menu isolated test '+step,timeOut:0});
  }
  return {read,guard,accept,execute};
}


// Session-local native paint clipboard. No pixels, geometry, effects or mask
// parameters are pasted; native gradient/transparency/dash attributes survive.
function createPaintClipboard(ps,session,capture,selectionContext,readTree) {
  let clipboard=null;
  const clone=v=>JSON.parse(JSON.stringify(v));
  const stable=v=>JSON.stringify(v,(k,x)=>x && !Array.isArray(x) && typeof x==='object'?
    Object.fromEntries(Object.keys(x).sort().map(k=>[k,x[k]])):x);
  const same=(a,b)=>stable(a)===stable(b);
  const invariantKeys=['layerKind','hasVectorMask','hasUserMask','userMaskFeather','userMaskDensity','vectorMaskFeather','vectorMaskDensity',
    'layerEffects','fillOpacity','opacity','mode','group','visible','name','layerLocking'];
  const target=(id,doc)=>[{_ref:'layer',_id:id},{_ref:'document',_id:doc}];
  async function batch(commands) {
    const values=await ps.action.batchPlay(commands,{});
    if(values.length!==commands.length || values.some(v=>!v || v._obj==='error'))throw Error(values.find(v=>v?._obj==='error')?.message || '形状外观读写失败');
    return values;
  }
  function locked(layer){for(let p=layer;p;p=p.parent)if(p.allLocked || p.locked || p.pixelsLocked || p.positionLocked || p.transparentPixelsLocked || p.isBackgroundLayer)return true;return false;}
  function guard(context,request,accepted=false){if(context.isCancelled || !accepted && Date.now()>request.expires_at_ms)throw Error('Cancelled or expired');}
  async function identity(expected){const now=await capture();if(!same(now,expected))throw Error('形状外观目标已改变');return now;}
  async function snapshot(expected,context,request,accepted=false) {
    guard(context,request,accepted);await identity(expected);
    const doc=ps.app.activeDocument;
    if(doc.mode!==ps.constants.DocumentMode.RGB || ![ps.constants.BitsPerChannelType.EIGHT,ps.constants.BitsPerChannelType.SIXTEEN].includes(doc.bitsPerChannel))throw Error('形状外观仅支持RGB8／16位文档');
    const ids=expected.layer_ids;if(!ids.length || ids.length>128)throw Error('请选择1至128个未锁定形状图层');
    const rows=[];
    for(let start=0;start<ids.length;start+=8){guard(context,request,accepted);
      rows.push(...await batch(ids.slice(start,start+8).map(id=>({_obj:'get',_target:target(id,doc.id),_options:{dialogOptions:'dontDisplay'}}))));}
    if(rows.some((v,i)=>v.layerID!==ids[i] || !v.hasVectorMask || !['solidColorLayer','gradientLayer','patternLayer'].includes(v.adjustment?.[0]?._obj)))
      throw Error('整批仅支持带矢量蒙版的形状填充图层；未修改');
    const tree=await readTree(doc),parents=new Map(tree.rows.map(v=>[v.layerID,v.parentLayerID>0?v.parentLayerID:null])),descriptors=new Map(rows.map(v=>[v.layerID,v])),checked=new Set();
    async function check(id){if(checked.has(id))return;let row=descriptors.get(id);if(!row){[row]=await batch([{_obj:'get',_target:target(id,doc.id),_options:{dialogOptions:'dontDisplay'}}]);descriptors.set(id,row);}
      const locks=row.layerLocking;if(!locks || row.layerID!==id || locks.protectAll || !tree.groups.get(id) && (row.background || locks.protectComposite || locks.protectPosition || locks.protectTransparency))throw Error('形状或祖先已锁定');
      if(!parents.has(id))throw Error('形状成员已改变');checked.add(id);const parent=parents.get(id);if(parent!==null)await check(parent);}
    for(const id of ids)await check(id);
    const paths=[];for(let start=0;start<ids.length;start+=8){guard(context,request,accepted);
      paths.push(...await batch(ids.slice(start,start+8).map(id=>({_obj:'get',_target:[{_ref:'property',_property:'pathContents'},
        {_ref:'path',_enum:'path',_value:'vectorMask'},...target(id,doc.id)],_options:{dialogOptions:'dontDisplay'}}))));}
    const orders=new Map();
    for(const row of tree.rows){const parent=parents.get(row.layerID);if(!orders.has(parent))orders.set(parent,[]);orders.get(parent).push(row.layerID);}
    const members=rows.map((v,i)=>{
      const id=ids[i],parent=parents.get(id);
      if(!paths[i]?.pathContents)throw Error('形状路径读取不完整');
      return {id,parent,order:orders.get(parent),path:paths[i].pathContents,
        invariants:Object.fromEntries(invariantKeys.map(k=>[k,v[k]??null])),
        fill:v.adjustment[0],stroke:v.AGMStrokeStyleInfo??null};
    });
    await identity(expected);
    const result={identity:expected,members};if(stable(result).length>4000000)throw Error('形状外观过于复杂，请减少批量图层');return result;
  }
  async function choose(ids,active,doc) {
    const ordered=ids.filter(id=>id!==active).concat(active);
    await batch(ordered.map((id,i)=>({_obj:'select',_target:target(id,doc),makeVisible:false,
      ...(i?{selectionModifier:{_enum:'selectionModifierType',_value:'addToSelection'}}:{}),_options:{dialogOptions:'dontDisplay'}})));
  }
  async function execute(request) {
    const copying=request.command==='shape.copyPaint';
    if(!copying && request.command!=='shape.pastePaint')throw Error('Unknown paint clipboard command');
    return ps.core.executeAsModal(async context=>{
      guard(context,request);const expected=request.target;
      if(expected?.session!==session)throw Error('形状剪贴板会话已改变');
      const before=await snapshot(expected,context,request),doc=expected.document_id;
      if(copying){
        if(before.members.length!==1)throw Error('复制形状外观需要单选');
        const {fill,stroke}=before.members[0];
        // Missing native stroke is the standard disabled stroke. A concrete
        // descriptor is needed when replacing a target's existing stroke.
        const defaultStroke={_obj:'strokeStyle',strokeStyleVersion:2,strokeEnabled:false,fillEnabled:true,
          strokeStyleLineWidth:{_unit:'pixelsUnit',_value:1},strokeStyleContent:{_obj:'solidColorLayer',color:{_obj:'RGBColor',red:0,grain:0,blue:0}}};
        const next=clone({fill,stroke:stroke??defaultStroke});if(stable(next).length>524288)throw Error('形状外观过于复杂');
        clipboard=next;return {verified:true,count:1,shape_clipboard_available:true,selection_context:selectionContext()};
      }
      if(!clipboard)throw Error('请先复制形状填充与描边；重启PS后需要重新复制');
      const wanted=clone(clipboard);let open=false,uncertain=false,history;
      try {
        guard(context,request);await identity(expected);
        history=await context.hostControl.suspendHistory({documentID:doc,name:'粘贴形状填充与描边'});open=true;
        let accepted=false;
        for(const member of before.members){
          guard(context,request,accepted);accepted=true;
          await batch([{_obj:'select',_target:target(member.id,doc),makeVisible:false,_options:{dialogOptions:'dontDisplay'}}]);
          const current=await capture();
          if(current.document_id!==doc || current.active_layer_id!==member.id || current.layer_ids.join(',')!==String(member.id))throw Error('形状粘贴目标已改变');
          await batch([{_obj:'set',_target:[{_ref:'contentLayer',_enum:'ordinal',_value:'targetEnum'}],
            to:{_obj:'shapeStyle',fillContents:clone(wanted.fill),strokeStyle:clone(wanted.stroke)},_options:{dialogOptions:'dontDisplay'}}]);
        }
        await choose(expected.layer_ids,expected.active_layer_id,doc);
        const after=await snapshot(expected,context,request,true);
        for(let i=0;i<before.members.length;i++){
          const {fill:a,stroke:b,...rest}=after.members[i],{fill,stroke,...original}=before.members[i];
          if(!same(rest,original) || !same(a,wanted.fill) || !same(b,wanted.stroke))throw Error('形状外观／路径／蒙版／顺序读回不符');
        }
        guard(context,request,true);open=false;uncertain=true;await context.hostControl.resumeHistory(history,true);uncertain=false;
        return {verified:true,count:before.members.length,placement_moves:0,shape_clipboard_available:true,selection_context:selectionContext()};
      } catch(error){
        if(open)try {
          open=false;await context.hostControl.resumeHistory(history,false);
          await choose(expected.layer_ids,expected.active_layer_id,doc);
          // Cancellation/expiry stops writes, but never suppresses rollback
          // verification. Only the original owned transaction is inspected.
          const restored=await snapshot(expected,{isCancelled:false},{...request,expires_at_ms:Date.now()+30000});
          if(!same(restored,before))uncertain=true;
        }catch(_){uncertain=true;}
        if(uncertain)error.resultUnknown=true;throw error;
      }
    },{commandName:copying?'复制形状填充与描边':'粘贴形状填充与描边',timeOut:0});
  }
  return {execute,available:()=>clipboard!==null};
}

// Cached vector-paint metadata and request-bound, one-history preview writes.
function createShapePaint(ps,session,capture,selectionContext,legacyPreview=false) {
  const batchMasks=createBatchMasks(ps,session,capture,selectionContext,legacyPreview);
  let cache=null,serial=0,catalog=[],catalogAt=0,catalogInfo={};
  const clone=value=>JSON.parse(JSON.stringify(value));
  // Native menu serialization sorts keys. Preserve exact protected values
  // and array order while ignoring object insertion order across IPC.
  const valueKey=value=>JSON.stringify(value,(_,v)=>v && !Array.isArray(v) && typeof v==='object'?
    Object.fromEntries(Object.keys(v).sort().map(k=>[k,v[k]])):v);
  const rgb=color=>{
    if(color?._obj!=='RGBColor')return null;
    // Converted fills can retain normalized, out-of-gamut RGB; clamp only the menu value, preserving the raw descriptor.
    const floating=['redFloat','greenFloat','blueFloat'].some(key=>Object.prototype.hasOwnProperty.call(color,key));
    const values=floating?[color.redFloat,color.greenFloat,color.blueFloat]:[color.red,color.grain??color.green,color.blue];
    return values.every(v=>typeof v==='number' && Number.isFinite(v) && (floating || v>=0 && v<=255))?
      values.map(v=>Math.round(Math.max(0,Math.min(255,floating?v*255:v)))):null;
  };
  const nativeModels={CMYKColorClass:['cmyk',['cyan','magenta','yellowColor','black'],[[0,100],[0,100],[0,100],[0,100]]],
    labColor:['lab',['luminance','a','b'],[[0,100],[-128,127],[-128,127]]],grayscale:['gray',['gray'],[[0,100]]]};
  function channels(color) {
    const value=rgb(color);if(value)return {rgb:value};
    const model=nativeModels[color?._obj];if(!model)return null;
    const values=model[1].map(k=>color[k]);
    return values.every((v,i)=>typeof v==='number' && Number.isFinite(v) && v>=model[2][i][0] && v<=model[2][i][1])?
      {native:{model:model[0],values}}:null;
  }
  function colorDescriptor(value) {
    if(!value.native)return {_obj:'RGBColor',red:value.rgb[0],grain:value.rgb[1],blue:value.rgb[2]};
    const [name,model]=Object.entries(nativeModels).find(([,v])=>v[0]===value.native.model);
    return {_obj:name,...Object.fromEntries(model[1].map((key,i)=>[key,value.native.values[i]]))};
  }
  function documentColor(doc) {
    let profile=null;try{profile=doc.colorProfileName ?? null;}catch(_){}
    return {mode:doc.mode,depth:doc.bitsPerChannel,profile};
  }
  function paint(raw) {
    if(raw?._obj==='solidColorLayer') {const c=channels(raw.color);return c?{kind:'solid',...c}:null;}
    if(raw?._obj==='gradientLayer') {
      const colors=raw.gradient?.colors;
      if(!Array.isArray(colors) || colors.length<2 || colors.length>32)return null;
      const stops=colors.map(v=>({...(channels(v.color) || {}),location:Math.round(v.location)}));
      const method=raw.gradientsInterpolationMethod?._value ?? 'classic';
      return stops.every(v=>v.rgb || v.native) && ['classic','linear','perceptual'].includes(method)?{kind:'gradient',stops,method}:null;
    }
    if(raw?._obj==='patternLayer' && typeof raw.pattern?.name==='string')return {kind:'pattern',name:raw.pattern.name};
    return null;
  }
  const gradientStyles=['linear','radial','angle','reflected','diamond'];
  const extendedGradient=value=>value?.kind==='gradient' && (value.options!==undefined || value.opacity_stops!==undefined || value.stops?.some(s=>s.midpoint!==undefined || s.source!==undefined || s.origin!==undefined));
  const extendedPaint=params=>extendedGradient(params.fill) || extendedGradient(params.stroke);
  function gradientDetail(value,raw) {
    if(value.kind!=='gradient')return value;
    const g=raw.gradient;
    return {...value,stops:value.stops.map((stop,i)=>({...stop,origin:i,midpoint:g.colors[i].midpoint ?? 50,source:g.colors[i].type?._value ?? 'userStop'})),
      opacity_stops:(g.transparency || [0,4096].map(location=>({location,opacity:{_value:100},midpoint:50}))).map(stop=>({location:stop.location,opacity:stop.opacity._value,midpoint:stop.midpoint ?? 50})),
      options:{style:raw.type?._value ?? 'linear',angle:raw.angle?._value ?? 0,scale:raw.scale?._value ?? 100,
        reverse:raw.reverse===true,dither:raw.dither===true,align:raw.align!==false,smoothness:g.interfaceIconFrameDimmed ?? 4096}};
  }
  async function patterns() {
    if(Date.now()-catalogAt<30000)return catalog;
    try {
      const [v]=await ps.action.batchPlay([{_obj:'get',_target:[{_ref:'property',_property:'presetManager'},
        {_ref:'application',_enum:'ordinal',_value:'targetEnum'}],_options:{dialogOptions:'dontDisplay'}}],{});
      catalogInfo={keys:Object.keys(v),groups:(v.presetManager || []).map(x=>({kind:x._obj,keys:Object.keys(x),count:Array.isArray(x.name)?x.name.length:0}))};
      const group=v.presetManager?.find(x=>/pattern|PttR/i.test(x._obj || ''));
      catalog=Array.isArray(group?.name)?group.name.filter(v=>typeof v==='string' && v.length<=128).slice(0,128):[];
      catalogAt=Date.now();
    } catch(error) {catalogInfo={error:String(error)};}
    return catalog;
  }
  async function readCore(doc,id,checkpoint=null) {
    const keys=['layerKind','hasVectorMask','adjustment','AGMStrokeStyleInfo'];
    const descriptors=keys.map(key=>({_obj:'get',_target:[{_ref:'property',_property:key},
      {_ref:'layer',_id:id},{_ref:'document',_id:doc.id}],_options:{dialogOptions:'dontDisplay'}}));
    const result=[];
    if(checkpoint){for(const descriptor of descriptors){await checkpoint();result.push(...await ps.action.batchPlay([descriptor],{}));}await checkpoint();}
    else result.push(...await ps.action.batchPlay(descriptors,{}));
    const vector=result[1]?.hasVectorMask===true;
    const fill=result[2]?.adjustment?.[0],stroke=result[3]?.AGMStrokeStyleInfo;
    if(!paint(fill))return null;
    const defaultStroke={_obj:'strokeStyle',strokeStyleVersion:2,strokeEnabled:false,fillEnabled:true,
      strokeStyleLineWidth:{_unit:'pixelsUnit',_value:1},strokeStyleContent:{_obj:'solidColorLayer',color:{_obj:'RGBColor',red:0,grain:0,blue:0}}};
    const actualStroke=vector?(stroke || defaultStroke):defaultStroke;
    if(actualStroke.strokeStyleLineWidth && !['pixelsUnit','pointsUnit'].includes(actualStroke.strokeStyleLineWidth._unit))return null;
    if(!paint(actualStroke.strokeStyleContent))return null;
    const raw={fill,stroke:actualStroke,vector,strokeMissing:vector && !stroke};
    if(vector) {
      // Read only while enriching an eligible shape, never on the basic
      // snapshot path or ordinary fill layers. Unknown values stay unavailable.
      const layer=doc.activeLayers[0];
      if(!layer || layer.id!==id)return null;
      try {
        const feather=layer.vectorMaskFeather,density=layer.vectorMaskDensity;
        if(typeof feather==='number' && Number.isFinite(feather) && feather>=0 && feather<=1000 &&
           typeof density==='number' && Number.isFinite(density) && density>=0 && density<=100)
          raw.mask={feather,density};
      } catch(_) {}
    }
    return raw;
  }
  async function observe(context,force=false,frozenCatalog=null,checkpoint=null,gradientDetails=false) {
    if(context.layer_ids?.length>1)return batchMasks.observe(context,checkpoint);
    if(context.kind!=='shape-fill' || context.layer_ids?.length!==1 || context.locked || !ps.app.documents.length)
      return {available:false};
    const doc=ps.app.activeDocument,id=context.layer_ids[0],key=doc.id+':'+id;
    if(context.document_id!==undefined && context.document_id!==doc.id)return {available:false};
    if(!force && !gradientDetails && cache?.key===key && Date.now()-cache.at<500)return cache.data;
    if(!['RGB','CMYK','LAB','GRAYSCALE'].some(key=>ps.constants.DocumentMode[key]!==undefined && doc.mode===ps.constants.DocumentMode[key]) || ![ps.constants.BitsPerChannelType.EIGHT,ps.constants.BitsPerChannelType.SIXTEEN].includes(doc.bitsPerChannel))
      return {available:false};
    if(checkpoint)await checkpoint();
    const raw=await readCore(doc,id,checkpoint);
    if(!raw) {cache={key,at:Date.now(),data:{available:false}};return cache.data;}
    if(ps.app.activeDocument.id!==doc.id || doc.activeLayers.length!==1 || doc.activeLayers[0].id!==id)
      return {available:false};
    const signature=JSON.stringify({raw,color:documentColor(doc)}),revision=cache?.key===key && cache.signature===signature?cache.data.revision:session+':'+(++serial);
    const fill={...paint(raw.fill),enabled:raw.stroke.fillEnabled!==false};
    const stroke={...paint(raw.stroke.strokeStyleContent),enabled:!!raw.stroke.strokeEnabled,
      width:Number(raw.stroke.strokeStyleLineWidth?._value ?? 1)*(raw.stroke.strokeStyleLineWidth?._unit==='pointsUnit'?doc.resolution/72:1)};
    // A live transaction needs only the frozen editor catalog. Refreshing
    // application-wide presets mid-drag adds unrelated host work, and cannot
    // make a newly added pattern valid for the original frozen editor anyway.
    if(checkpoint)await checkpoint();
    const nativePaint=p=>!!p.native || p.stops?.some(s=>s.native);
    const live=doc.mode===ps.constants.DocumentMode.RGB && !nativePaint(fill) && !nativePaint(stroke);
    const data={available:true,vector:raw.vector,gradient_editor:true,native_model:({[ps.constants.DocumentMode.CMYK]:'cmyk',[ps.constants.DocumentMode.LAB]:'lab',[ps.constants.DocumentMode.GRAYSCALE]:'gray'})[doc.mode] || '',live_preview:live && (typeof ps.core.redrawDocument==='function' || legacyPreview),
      ...(live && legacyPreview && typeof ps.core.redrawDocument!=='function'?{preview_engine:'legacy-com'}:{}),revision,fill,stroke,
      patterns:frozenCatalog?frozenCatalog.patterns:await patterns(),
      pattern_catalog:frozenCatalog?frozenCatalog.pattern_catalog:catalogInfo};
    if(checkpoint)await checkpoint();
    if(raw.mask)data.mask=clone(raw.mask);
    cache={key,at:Date.now(),signature,raw,data};
    if(!gradientDetails)return data;
    const expanded={...data,fill:gradientDetail(fill,raw.fill),stroke:gradientDetail(stroke,raw.stroke.strokeStyleContent)};
    if(gradientDetails!=='inspect')return expanded;
    let documentPath='';try{if(typeof doc.path==='string')documentPath=doc.path;}catch(_){}
    return {...expanded,baseline:signature,document_path:documentPath,document_name:doc.name,layer_name:doc.activeLayers[0].name};
  }
  function validatePaint(value) {
    if(!value || !['solid','gradient','pattern'].includes(value.kind) || typeof value.enabled!=='boolean')throw Error('Invalid paint type');
    const color=v=>{
      if(Object.prototype.hasOwnProperty.call(v,'native')){const n=v.native,model=n && Object.values(nativeModels).find(m=>m[0]===n.model);return !v.rgb && model && Object.keys(n).sort().join(',')==='model,values' && Array.isArray(n.values) && n.values.length===model[1].length && n.values.every((x,i)=>typeof x==='number' && Number.isFinite(x) && x>=model[2][i][0] && x<=model[2][i][1]);}
      return Array.isArray(v.rgb) && v.rgb.length===3 && v.rgb.every(x=>typeof x==='number' && Number.isFinite(x) && x>=0 && x<=255);
    };
    if(value.kind==='solid' && !color(value))throw Error('Invalid paint color');
    if(value.kind==='gradient' && (!Array.isArray(value.stops) || value.stops.length<2 || value.stops.length>32 ||
      value.stops.some((s,i)=>!color(s) || !Number.isInteger(s.location) || s.location<0 || s.location>4096 || i && s.location<value.stops[i-1].location)))throw Error('Invalid gradient stops');
    if(value.kind==='pattern' && (typeof value.name!=='string' || !value.name || value.name.length>128))throw Error('Invalid pattern name');
    if(value.kind==='gradient' && value.method!==undefined && !['classic','linear','perceptual'].includes(value.method))throw Error('Invalid gradient interpolation');
    if(value.kind==='gradient') {
      const midpoint=x=>Number.isInteger(x) && x>=0 && x<=100;
      if(value.stops.some(s=>s.midpoint!==undefined && !midpoint(s.midpoint) || s.source!==undefined && !['userStop','foregroundColor','backgroundColor'].includes(s.source) ||
        s.origin!==undefined && (!Number.isInteger(s.origin) || s.origin<0 || s.origin>=32)))throw Error('Invalid gradient stop details');
      const opacity=value.opacity_stops;
      if(opacity!==undefined && (!Array.isArray(opacity) || opacity.length<2 || opacity.length>32 || opacity.some((s,i)=>!s || !Number.isInteger(s.location) ||
        s.location<0 || s.location>4096 || i && s.location<opacity[i-1].location || typeof s.opacity!=='number' || !Number.isFinite(s.opacity) || s.opacity<0 || s.opacity>100 ||
        s.midpoint!==undefined && !midpoint(s.midpoint))))throw Error('Invalid gradient opacity stops');
      const o=value.options;
      if(o!==undefined && (!o || typeof o!=='object' || Array.isArray(o) || Object.entries(o).some(([k,v])=>!(
        k==='style' && gradientStyles.includes(v) || ['reverse','dither','align'].includes(k) && typeof v==='boolean' ||
        typeof v==='number' && Number.isFinite(v) && (k==='angle' && Math.abs(v)<=360 || k==='scale' && v>=1 && v<=1000 ||
          k==='smoothness' && Number.isInteger(v) && v>=0 && v<=4096)))))throw Error('Invalid gradient options');
    }
  }
  function build(value,previous) {
    if(value.kind==='solid')return {_obj:'solidColorLayer',color:colorDescriptor(value)};
    if(value.kind==='pattern') {
      if(!catalog.includes(value.name) && previous?.pattern?.name!==value.name)throw Error('图案预设已改变，请重新打开菜单');
      const next=previous?._obj==='patternLayer'?clone(previous):{_obj:'patternLayer'};
      next.pattern=previous?.pattern?.name===value.name?clone(previous.pattern):{_obj:'pattern',name:value.name};return next;
    }
    const next=previous?._obj==='gradientLayer'?clone(previous):{_obj:'gradientLayer',type:{_enum:'gradientType',_value:'linear'},
      angle:{_unit:'angleUnit',_value:0},scale:{_unit:'percentUnit',_value:100}};
    const old=next.gradient || {};
    const options=value.options || {};
    if(options.style!==undefined)next.type={_enum:'gradientType',_value:options.style};
    for(const [key,unit] of [['angle','angleUnit'],['scale','percentUnit']])if(options[key]!==undefined)next[key]={_unit:unit,_value:options[key]};
    for(const key of ['reverse','dither','align'])if(options[key]!==undefined)next[key]=options[key];
    if(value.method!==undefined)next.gradientsInterpolationMethod={_enum:'gradientInterpolationMethodType',_value:value.method};
    next.gradient={...old,_obj:'gradientClassEvent',name:old.name || 'Menu gradient',gradientForm:{_enum:'gradientForm',_value:'customStops'},
      interfaceIconFrameDimmed:options.smoothness ?? old.interfaceIconFrameDimmed ?? 4096,colors:value.stops.map((s,i)=>{
        const prior=old.colors?.[s.origin ?? i],color=s.native?{native:s.native}:{rgb:s.rgb};
        return {...prior,_obj:'colorStop',color:prior && valueKey(channels(prior.color))===valueKey(color)?clone(prior.color):colorDescriptor(s),
          type:{_enum:'colorStopType',_value:s.source ?? 'userStop'},location:s.location,midpoint:s.midpoint ?? prior?.midpoint ?? 50};
      }),
      transparency:value.opacity_stops?.map(s=>({_obj:'transferSpec',opacity:{_unit:'percentUnit',_value:s.opacity},location:s.location,midpoint:s.midpoint ?? 50})) ||
        old.transparency || [0,4096].map(location=>({_obj:'transferSpec',opacity:{_unit:'percentUnit',_value:100},location,midpoint:50}))};
    return next;
  }
  function samePaint(actual,wanted) {
    if(!actual || actual.kind!==wanted.kind || actual.enabled!==wanted.enabled)return false;
    const close=(a,b)=>a.length===b.length && a.every((v,i)=>Math.abs(v-b[i])<=1);
    const sameColor=(a,b)=>a.native || b.native?a.native?.model===b.native?.model && !!a.native && !!b.native && a.native.values.length===b.native.values.length && a.native.values.every((v,i)=>Math.abs(v-b.native.values[i])<=.01):close(a.rgb,b.rgb);
    if(actual.kind==='solid')return sameColor(actual,wanted);
    if(actual.kind==='pattern')return actual.name===wanted.name;
    if(wanted.options && (!actual.options || Object.entries(wanted.options).some(([k,v])=>{
      const read=actual.options[k];
      if(typeof v!=='number')return read!==v;
      if(typeof read!=='number' || !Number.isFinite(read))return true;
      const distance=Math.abs(read-v);
      return (k==='angle'?Math.min(distance%360,360-distance%360):distance)>.01;
    })))return false;
    if(wanted.opacity_stops && (!actual.opacity_stops || actual.opacity_stops.length!==wanted.opacity_stops.length || actual.opacity_stops.some((s,i)=>{
      const other=wanted.opacity_stops[i],quantized=Math.round(other.opacity*255/100)*100/255;
      return s.location!==other.location || Math.min(Math.abs(s.opacity-other.opacity),Math.abs(s.opacity-quantized))>.01 || s.midpoint!==(other.midpoint ?? 50);
    })))return false;
    return (wanted.method===undefined || actual.method===wanted.method) && actual.stops.length===wanted.stops.length && actual.stops.every((s,i)=>{
      const other=wanted.stops[i];return s.location===other.location && sameColor(s,other) && (other.midpoint===undefined || s.midpoint===other.midpoint) && (other.source===undefined || s.source===other.source);
    });
  }
  function validateParams(params) {
    if(!params || typeof params.revision!=='string' || params.revision.length>128)throw Error('Missing shape revision');
    validatePaint(params.fill);validatePaint(params.stroke);
    if(typeof params.stroke.width!=='number' || !Number.isFinite(params.stroke.width) || params.stroke.width<0 || params.stroke.width>1000)throw Error('Invalid stroke width');
    if(Object.prototype.hasOwnProperty.call(params,'mask') && (!params.mask || typeof params.mask!=='object' ||
      Object.keys(params.mask).sort().join(',')!=='density,feather' ||
      typeof params.mask.feather!=='number' || !Number.isFinite(params.mask.feather) || params.mask.feather<0 || params.mask.feather>1000 ||
      typeof params.mask.density!=='number' || !Number.isFinite(params.mask.density) || params.mask.density<0 || params.mask.density>100))throw Error('Invalid vector mask');
  }
  async function redrawPreview(identity,io) {
    // Use only the host document redraw API (PS24.1+). Nested JSX refresh
    // under PS24.0 can remain blocked, even after Cancel Plugin Command.
    const started=Date.now();
    if(io?.stage)await io.stage('redraw-target');
    const [value]=await ps.action.batchPlay([{_obj:'get',_target:[
      {_ref:'property',_property:'layerID'},{_ref:'layer',_id:identity.layerID},
      {_ref:'document',_id:identity.documentID}],_options:{dialogOptions:'silent'}}],{});
    if(value?._obj==='error' || value?.layerID!==identity.layerID)throw Error('Photoshop 预览刷新目标校验失败');
    if(ps.app.activeDocument.id!==identity.documentID || ps.app.activeDocument.activeLayers.length!==1 ||
        ps.app.activeDocument.activeLayers[0].id!==identity.layerID)throw Error('Preview target changed before redraw');
    if(typeof ps.core.redrawDocument!=='function')throw Error('当前PS使用本地草稿，实时预览需要PS24.1及官方重绘API');
    if(io?.stage)await io.stage('redraw-native');
    await ps.core.redrawDocument({documentID:identity.documentID});
    const method='core.redrawDocument';
    return {redraw_method:method,redraw_ms:Date.now()-started};
  }
  async function previewMask(context,request,before,doc,anchor,identity,io,applyPaint) {
    if(!io || typeof io.read!=='function')throw Error('Missing preview mailbox');
    let seq=0,lease=Date.now()+4000,applied=before.mask?clone(before.mask):undefined,updates=0,maxWriteMs=0;
    const close=(a,b)=>!a&&!b || !!a&&!!b&&Math.abs(a.feather-b.feather)<=.01 && Math.abs(a.density-b.density)<=100/255+.001;
    const read=()=>({feather:anchor.vectorMaskFeather,density:anchor.vectorMaskDensity});
    const target=()=>ps.app.activeDocument.id===identity.documentID && doc.activeLayers.length===1 &&
      doc.activeLayers[0].id===identity.layerID && !anchor.allLocked;
    while(true) {
      if(context.isCancelled)throw Error('Photoshop cancelled preview');
      if(io.stage)await io.stage('read-control',seq);
      const packet=await io.read(),now=Date.now();
      if(packet) {
        if(packet.id!==request.id || packet.session!==request.session || packet.token!==request.token ||
          !Number.isSafeInteger(packet.seq) || packet.seq<seq ||
          !Number.isFinite(packet.lease_until_ms) || packet.lease_until_ms>now+5000 ||
          !['preview','commit','cancel'].includes(packet.phase))throw Error('Invalid preview identity or lease');
        lease=packet.lease_until_ms;
        if(lease<now)return {cancelled:true,updates,max_write_ms:maxWriteMs,reason:'lease-expired'};
        if(!target())throw Error('Preview target changed');
        if(packet.phase==='cancel')return {cancelled:true,updates,max_write_ms:maxWriteMs,reason:'cancel'};
        validateParams(packet.paint);
        if(packet.paint.revision!==before.revision || !!packet.paint.mask!==!!request.paint.mask)throw Error('Preview revision changed');
        if(applied && !close(read(),applied))throw Error('Vector mask changed outside preview');
        if(packet.seq>seq || packet.phase==='commit') {
          if(io.stage)await io.stage('validate-frame',packet.seq);
          const wanted=packet.paint.mask || before.mask,started=Date.now();
          const maskChanged=!close(applied,wanted);
          const {changed:colorChanged,...colors}=await applyPaint(packet.paint);
          if(packet.phase==='commit')return {paint:packet.paint,updates,max_write_ms:Math.max(maxWriteMs,Date.now()-started)};
          if(wanted && maskChanged) {
            // Same properties/units as Adobe's DOM setters, with an async
            // modifying batch so PS24.0 can flush the final combined frame.
            const to={_obj:'layer'};
            if(wanted.feather!==applied.feather)to.vectorMaskFeather={_unit:'pixelsUnit',_value:wanted.feather};
            if(wanted.density!==applied.density)to.vectorMaskDensity={_unit:'percentUnit',_value:wanted.density};
            // Photoshop's layer setter processes these mask properties one at
            // a time. Keep both descriptors in one batch and redraw once.
            const descriptors=Object.entries(to).filter(([key])=>key!=='_obj').map(([key,value])=>({
              _obj:'set',_target:[{_ref:'layer',_id:identity.layerID},{_ref:'document',_id:identity.documentID}],
              to:{_obj:'layer',[key]:value},_options:{dialogOptions:'silent'}}));
            if(io.stage)await io.stage('write-mask');
            const responses=await ps.action.batchPlay(descriptors,{});
            for(const response of responses)if(response?._obj==='error')throw Error(response.message || 'Photoshop 拒绝蒙版预览');
            if(io.stage)await io.stage('read-mask');
            applied=read();
          }
          if(!target() || !close(applied,wanted))throw Error('Preview mask readback mismatch');
          const redrawCompleted=!!(colorChanged || maskChanged);
          const redraw=redrawCompleted?await redrawPreview(identity,io):{};
          if(!target())throw Error('Preview target changed during redraw');
          updates++;maxWriteMs=Math.max(maxWriteMs,Date.now()-started);seq=packet.seq;
          if(io.stage)await io.stage('publish-frame');
          if(io.applied)await io.applied({seq,...colors,...(applied?{mask:applied}:{}),updates,max_write_ms:maxWriteMs,redraw_completed:redrawCompleted,...redraw});
        }
      }
      // A slow Photoshop redraw may outlast the previously read lease. Read
      // the current mailbox before expiring a still-renewed live session.
      if(!packet && Date.now()>lease)return {cancelled:true,updates,max_write_ms:maxWriteMs,reason:'lease-expired'};
      await new Promise(resolve=>setTimeout(resolve,40));
    }
  }
  async function apply(request,previewIO=null) {
    if(request.paint?.mask_batch)return batchMasks.apply(request,previewIO);
    let params=request.paint;validateParams(params);
    const preview=request.command==='shape.preview';
    if(preview && typeof ps.core.redrawDocument!=='function')
      throw Error('当前PS使用本地草稿，实时预览需要PS24.1及官方重绘API');
    const stage=async name=>{if(previewIO?.stage)await previewIO.stage(name);};
    await stage('enter-modal');
    // Photoshop may recreate thrown errors across the native modal boundary,
    // dropping custom fields. Keep uncertainty outside that boundary.
    let uncertain=false;
    try {return await ps.core.executeAsModal(async context=>{
      await stage('capture-target');
      if(context.isCancelled || Date.now()>request.expires_at_ms)throw Error('Cancelled or expired');
      const actual=await capture(),expected=request.target;
      if(expected?.session!==session || actual.document_id!==expected.document_id || actual.active_layer_id!==expected.active_layer_id || actual.layer_ids.join(',')!==expected.layer_ids?.join(','))throw Error('Document or layer target changed');
      await stage('read-initial-paint');
      const details=extendedPaint(params),eligibility=selectionContext(true),before=await observe(eligibility,true,null,null,details);
      if(!before.available || before.revision!==params.revision)throw Error('填充／描边已改变，请重新打开菜单；本次未修改');
      if(preview && !before.live_preview)throw Error('原生颜色通道使用本地草稿，确认后写入');
      if(params.mask && (!before.vector || !before.mask))throw Error('当前图层不支持矢量蒙版羽化／密度；本次未修改');
      if(!before.vector && params.fill.kind!==before.fill.kind)throw Error('普通填充层保持PS原生类型；本次未修改');
      if(!before.vector && (!params.fill.enabled || valueKey(params.stroke)!==valueKey(before.stroke)))
        throw Error('普通填充层不支持形状描边或禁用填充；本次未修改');
      const original=cache.raw,doc=ps.app.activeDocument,anchor=doc.activeLayers[0];
      // A first shapeStyle write can materialize PS's previously absent, disabled stroke in the document model.
      const strokeMatches=value=>samePaint(value,params.stroke) || original.strokeMissing &&
        valueKey(params.stroke)===valueKey(before.stroke) && value?.enabled===false && Math.abs(value.width-before.stroke.width)<=.01;
      const originalColor=valueKey(documentColor(doc));
      const identity={documentID:doc.id,layerID:anchor.id};
      const target=()=>ps.app.activeDocument.id===identity.documentID && doc.activeLayers.length===1 &&
        doc.activeLayers[0].id===identity.layerID && !anchor.allLocked;
      const parent=anchor.parent,parentId=parent?.id ?? null,order=Array.from(parent?parent.layers:doc.layers,l=>l.id);
      let appliedPaint=before,liveColors=before,appliedRaw=original,paintTouched=false;
      const rawKey=raw=>JSON.stringify([raw.fill,raw.stroke,raw.vector]);
      let appliedRawKey=rawKey(original);
      const preparePaint=()=>{
        const key=valueKey;
        const fillChanged=key(params.fill)!==key(appliedPaint.fill),strokeChanged=key(params.stroke)!==key(appliedPaint.stroke);
        const paintChanged=fillChanged || strokeChanged;
        // Vector-backed content must use Photoshop's shapeStyle setter.
        // Direct solidColorLayer writes are unavailable in some real shapes,
        // including vector masks inherited by a newly created solid fill.
        const to=before.vector?{_obj:'shapeStyle'}:build(params.fill,original.fill);
        if(before.vector) {
          if(fillChanged)to.fillContents=build(params.fill,original.fill);
          if(strokeChanged || params.fill.enabled!==appliedPaint.fill.enabled) {
            to.strokeStyle={...clone(appliedRaw.stroke),strokeStyleVersion:2,fillEnabled:params.fill.enabled};
            if(strokeChanged)Object.assign(to.strokeStyle,{strokeEnabled:params.stroke.enabled,
              strokeStyleContent:build(params.stroke,original.stroke.strokeStyleContent)});
            if(params.stroke.width!==appliedPaint.stroke.width)
              to.strokeStyle.strokeStyleLineWidth={_unit:'pixelsUnit',_value:params.stroke.width};
          }
        }
        return {paintChanged,to};
      };
      let prepared=preparePaint();
      const applyPreviewPaint=async value=>{
        if(!target())throw Error('Preview target changed');
        params=value;
        if(!before.vector && (params.fill.kind!==before.fill.kind || !params.fill.enabled ||
          params.stroke.kind!==before.stroke.kind || params.stroke.enabled!==before.stroke.enabled ||
          params.stroke.width!==before.stroke.width || JSON.stringify(params.stroke.rgb)!==JSON.stringify(before.stroke.rgb)))
          throw Error('普通填充层保持PS原生类型与描边');
        const next=preparePaint();
        // Mask-only updates retain the cheap DOM path. Colors use four targeted
        // properties, never a document tree or per-frame pattern enumeration.
        if(next.paintChanged || paintTouched) {
          await stage('read-color-before');
          const raw=await readCore(doc,identity.layerID);
          if(!raw || rawKey(raw)!==appliedRawKey)throw Error('Fill or stroke changed outside preview');
        }
        if(!target())throw Error('Preview target changed during read');
        if(next.paintChanged) {
          await stage('write-color');
          if(!target())throw Error('Preview target changed before write');
          // PS24 contentLayer setters require the current content reference.
          // ID references can reject or silently do nothing. The frozen target
          // is checked immediately above, with no intervening await, and the
          // actual color/identity is checked again after the write.
          const [response]=await ps.action.batchPlay([{_obj:'set',_target:[{_ref:'contentLayer',_enum:'ordinal',_value:'targetEnum'}],
            to:next.to,_options:{dialogOptions:'silent'}}],{});
          if(response?._obj==='error')throw Error(response.message || 'Photoshop 拒绝颜色预览');
          await stage('read-color-after');
          // Paint validation needs fresh identity/type/ancestor locks, not
          // sibling adjacency and sort capabilities on every color frame.
          const actual=await observe(selectionContext(true),true,before,null,details);
          if(!target() || !actual.available || actual.vector!==before.vector || !samePaint(actual.fill,params.fill) ||
            !strokeMatches(actual.stroke) || Math.abs(actual.stroke.width-params.stroke.width)>.01)
            throw Error('Fill or stroke preview readback mismatch');
          appliedPaint=clone(params);liveColors=actual;appliedRaw=cache.raw;appliedRawKey=rawKey(appliedRaw);paintTouched=true;
        }
        return {fill:liveColors.fill,stroke:liveColors.stroke,changed:next.paintChanged};
      };
      if(context.isCancelled || Date.now()>request.expires_at_ms)throw Error('Cancelled or expired');
      await stage('suspend-history');
      const history=await context.hostControl.suspendHistory({documentID:doc.id,name:'Menu shape paint'});
      let historyOpen=true,committed=false;
      const originalSignature=cache.signature;
      try {
        let previewStats=null;
        if(preview) {
          const signature=cache.signature;
          previewStats=await previewMask(context,request,before,doc,anchor,identity,previewIO,applyPreviewPaint);
          if(previewStats.cancelled) {
            await stage('rollback-history');
            historyOpen=false;uncertain=true;
            await context.hostControl.resumeHistory(history,false);cache=null;
            await redrawPreview(identity,previewIO);
            await stage('verify-rollback');
            await observe(selectionContext(true),true,before);
            if(!cache || cache.signature!==signature || doc.activeLayers.length!==1 || doc.activeLayers[0].id!==anchor.id ||
              (anchor.parent?.id ?? null)!==parentId || Array.from(parent?parent.layers:doc.layers,l=>l.id).join(',')!==order.join(','))
              {const error=Error('预览回滚结果待核对');error.resultUnknown=true;throw error;}
            uncertain=false;
            return {cancelled:true,verified:true,rollback_verified:true,preview:previewStats,selection_context:selectionContext()};
          }
          params=previewStats.paint;
          prepared=preparePaint();
        }
        const {paintChanged,to}=prepared;
        if(context.isCancelled || !preview && Date.now()>request.expires_at_ms)throw Error('Cancelled or expired');
        if(!target())throw Error('Paint target changed before final write');

        if(paintChanged) {
          const [response]=await ps.action.batchPlay([{_obj:'set',_target:[{_ref:'contentLayer',_enum:'ordinal',_value:'targetEnum'}],
            to,_options:{dialogOptions:'silent'}}],{});
          if(response?._obj==='error')throw Error(response.message || 'Photoshop 拒绝修改形状颜色');
        }
        if(params.mask) {
          if(params.mask.feather!==anchor.vectorMaskFeather)anchor.vectorMaskFeather=params.mask.feather;
          if(params.mask.density!==anchor.vectorMaskDensity)anchor.vectorMaskDensity=params.mask.density;
        }
        await stage('verify-final');
        const after=await observe(selectionContext(true),true,preview?before:null,null,details);
        const wantedMask=params.mask || before.mask;
        if(wantedMask && (!after.mask || Math.abs(after.mask.feather-wantedMask.feather)>.01 ||
          Math.abs(after.mask.density-wantedMask.density)>100/255+.001))throw Error('矢量蒙版羽化／密度读回不符，已回滚');
        if(valueKey(documentColor(doc))!==originalColor || !after.available || after.vector!==before.vector || !samePaint(after.fill,params.fill) || !strokeMatches(after.stroke) || Math.abs(after.stroke.width-params.stroke.width)>.01 ||
          doc.activeLayers.length!==1 || doc.activeLayers[0].id!==anchor.id || (anchor.parent?.id ?? null)!==parentId ||
          Array.from(parent?parent.layers:doc.layers,l=>l.id).join(',')!==order.join(','))throw Error('形状颜色／图层身份读回不符，已回滚');
        await stage('commit-history');
        historyOpen=false;uncertain=true;
        await context.hostControl.resumeHistory(history,true);committed=true;uncertain=false;
        return {count:1,verified:true,placement_moves:0,fill_kind:after.fill.kind,stroke_kind:after.stroke.kind,
          ...(after.mask?{mask:after.mask}:{}),...(previewStats?{preview:previewStats}:{}),selection_context:selectionContext()};
      } catch(error) {
        await stage('rollback-error');
        uncertain=uncertain || !!error.resultUnknown || committed;
        if(historyOpen) {
          try {
            historyOpen=false;
            await context.hostControl.resumeHistory(history,false);cache=null;
            await stage('verify-error-rollback');
            await observe(selectionContext(true),true,before);
            if(!target() || !cache || cache.signature!==originalSignature ||
                (anchor.parent?.id ?? null)!==parentId ||
                Array.from(parent?parent.layers:doc.layers,l=>l.id).join(',')!==order.join(','))uncertain=true;
          } catch(_) {uncertain=true;}
        }
        throw error;
      }
    },{commandName:preview?'Menu paint preview':'Menu shape paint',timeOut:0,...(preview?{interactive:true}:{})});
    } catch(error) {
      if(uncertain) {
        const result=Error(String(error));result.resultUnknown=true;throw result;
      }
      throw error;
    }
  }
  async function prepareLegacy(request) {
    if(!legacyPreview || typeof ps.core.redrawDocument==='function')throw Error('Legacy preview channel unavailable');
    if(request.paint?.mask_batch)return batchMasks.prepareLegacy(request);
    validateParams(request.paint);
    const actual=await capture(),expected=request.target;
    if(Date.now()>request.expires_at_ms || expected?.session!==session || actual.document_id!==expected.document_id ||
      actual.active_layer_id!==expected.active_layer_id || actual.layer_ids.join(',')!==expected.layer_ids?.join(','))throw Error('Legacy preview target changed');
    const before=await observe(selectionContext(true),true,null,null,extendedPaint(request.paint));
    if(!before.available || before.revision!==request.paint.revision)throw Error('填充／描边已改变，请重新打开菜单；本次未修改');
    if(!before.live_preview)throw Error('原生颜色通道使用本地草稿，确认后写入');
    if(request.paint.mask && (!before.vector || !before.mask))throw Error('Vector mask unavailable');
    if(!before.vector && (request.paint.fill.kind!==before.fill.kind || !request.paint.fill.enabled ||
      valueKey(request.paint.stroke)!==valueKey(before.stroke)))throw Error('Ordinary fill type must remain unchanged');
    const doc=ps.app.activeDocument,anchor=doc.activeLayers[0];
    if(doc.id!==actual.document_id || doc.activeLayers.length!==1 || anchor?.id!==actual.active_layer_id)throw Error('Legacy prepare selection changed');
    return {before:clone(before),raw:clone(cache.raw),parent_id:anchor.parent?.id??null,
      order:Array.from(anchor.parent?anchor.parent.layers:doc.layers,l=>l.id)};
  }
  async function finishLegacy(request,prepared,response) {
    if(request.paint?.mask_batch)return batchMasks.finishLegacy(request,prepared,response);
    if(response.phase!=='completed') {const error=Error(response.error || 'Legacy preview failed');error.resultUnknown=response.result_unknown===true;throw error;}
    let uncertain=true;
    try {
      const result=response.result;
      if(!result || result.verified!==true || typeof result.cancelled!=='undefined' && typeof result.cancelled!=='boolean')throw Error('Invalid native preview result');
      const wanted=result.cancelled?prepared.before:result.paint;
      validateParams(wanted);
      if(wanted.revision!==request.paint.revision || result.preview?.engine!=='standalone-extendscript' || !Number.isInteger(result.preview.last_seq) || result.preview.last_seq<0 ||
        !result.cancelled && (result.placement_moves!==0 || ![0,1].includes(result.count)) || result.cancelled && result.rollback_verified!==true)throw Error('Native preview proof incomplete');
      // The native transaction read back its target, paint, vector path, pixel
      // mask and sibling order BEFORE publishing this authenticated terminal.
      // Do not reinterpret a later user selection as an unknown old write, or
      // add a duplicate paint query. Fresh selection feedback is separate.
      cache=null;
      uncertain=false;
      const {paint,...summary}=result;
      try {summary.selection_context=selectionContext();}catch(_){}
      return summary;
    } catch(error) {if(uncertain)error.resultUnknown=true;throw error;}
  }
  return {observe,apply,prepareLegacy,finishLegacy};
}

async function readLayerEffects(ps,doc,ids) {
  let values;
  try {values=await ps.action.batchPlay(ids.map(id=>({_obj:'multiGet',
    _target:{_ref:[{_ref:'layer',_id:id},{_ref:'document',_id:doc.id}]},
    extendedReference:[['layerID','layerEffects']],
    options:{failOnMissingProperty:false,failOnMissingElement:true},_options:{dialogOptions:'dontDisplay'}})),{});}
  catch(_) {} // Read-only compatibility fallback; no modifying command is retried.
  if(!values || Array.isArray(values) && values.some(v=>v?._obj==='error'))values=await ps.action.batchPlay(ids.map(id=>({_obj:'get',
    _target:[{_ref:'layer',_id:id},{_ref:'document',_id:doc.id}],_options:{dialogOptions:'dontDisplay'}})),{});
  if(!Array.isArray(values) || values.length!==ids.length || values.some((v,i)=>!v || v._obj==='error' || v.layerID!==ids[i] ||
      v.layerEffects!==undefined && (!v.layerEffects || typeof v.layerEffects!=='object' || Array.isArray(v.layerEffects))))
    throw Error('Layer style readback unavailable or target changed');
  return values;
}

// Fixed reference-Caps actions. No menu letters, synthetic keys or IME changes.
function createCapsActions(ps,session,capture,selectionContext,readNativeTree,captureSelection,nativeSelection=false) {
  // The native strategy prepares fresh feedback after modal exit; its DOM copy is discarded.
  const readFeedback=()=>nativeSelection?null:selectionContext();
  const commands=['moveDocumentTop','moveDocumentBottom','selectAbove','selectBelow','extendAbove','extendBelow','moveUp','moveDown','copyStyle','pasteStyle','editStyle','convertSmartObject','convertSmartObjectsIndividually'];
  async function convertIndividually(request) {
    return ps.core.executeAsModal(async context=>{
      const check=()=>{if(context.isCancelled || Date.now()>request.expires_at_ms)throw Error('Cancelled or expired');};
      check();const actual=await capture(),expected=request.target;
      const sameTarget=value=>value.document_id===actual.document_id && value.active_layer_id===actual.active_layer_id && value.layer_ids.join(',')===actual.layer_ids.join(',');
      if(expected?.session!==session || !sameTarget(expected))throw Error('Smart object target changed');
      const doc=ps.app.activeDocument,k=ps.constants.LayerKind;
      function tree(layers,parent=null){return Array.from(layers).flatMap(l=>[{layer:l,id:l.id,parent},...(l.kind===k.GROUP?tree(l.layers,l.id):[])]);}
      const before=tree(doc.layers),selected=before.filter(l=>actual.layer_ids.includes(l.id)),byId=new Map(before.map(l=>[l.id,l]));
      if(!selected.length || selected.length!==actual.layer_ids.length)throw Error('Smart object target missing');
      for(const item of selected){
        for(let at=item;at;at=byId.get(at.parent)){
          if(at.layer.isBackgroundLayer || at.layer.allLocked || at.layer.positionLocked)throw Error('请先解锁目标图层及父组');
          if(at!==item && actual.layer_ids.includes(at.id))throw Error('不能同时选择父组和其子图层逐层转换');
        }
      }
      const pending=selected.filter(l=>l.layer.kind!==k.SMARTOBJECT),mapping=new Map(selected.map(l=>[l.id,l.id]));
      if(!pending.length)return {verified:true,unchanged:true,count:selected.length,converted_count:0,command:request.command,selection_context:readFeedback()};
      const bounds=l=>{const b=l.boundsNoEffects;return ['left','top','right','bottom'].map(key=>b[key]);};
      const positions=new Map(pending.map(l=>[l.id,bounds(l.layer)]));
      if([...positions.values()].some(b=>b.some(v=>!Number.isFinite(v))))throw Error('逐层转换位置尚未确认');
      const removedById=new Map(pending.map(item=>{
        const ids=new Set([item.id]);for(const l of before)if(ids.has(l.parent))ids.add(l.id);return [item.id,ids];
      }));
      const pairs=rows=>rows.map(l=>[l.id,l.parent]);
      const original=pairs(before);let expectedTree=original;
      const history=await context.hostControl.suspendHistory({documentID:doc.id,name:'Menu convert each smart object'});
      async function batch(descriptors){const values=await ps.action.batchPlay(descriptors,{});if(values.some(v=>v._obj==='error'))throw Error('Photoshop 拒绝逐层转换智能对象');}
      const selectId=id=>({_obj:'select',_target:[{_ref:'layer',_id:id},{_ref:'document',_id:doc.id}],makeVisible:false,_options:{dialogOptions:'dontDisplay'}});
      try {
        for(const item of pending){
          check();await batch([selectId(item.id)]);
          const current=await capture();
          if(current.document_id!==doc.id || current.active_layer_id!==item.id || current.layer_ids.join(',')!==String(item.id))throw Error('逐层转换选择核验失败');
          await batch([{_obj:'newPlacedLayer',_options:{dialogOptions:'dontDisplay'}}]);
          const after=await capture(),now=tree(doc.layers),made=now.find(l=>l.id===after.active_layer_id);
          if(after.document_id!==doc.id || after.layer_ids.length!==1 || !made || made.layer.kind!==k.SMARTOBJECT || made.parent!==item.parent || before.some(l=>l.id===made.id))
            throw Error('逐层转换类型／父组／身份核验失败');
          const removed=removedById.get(item.id),position=bounds(made.layer),previous=positions.get(item.id);
          // Native Smart Objects rasterize fractional edges to pixel bounds.
          if(position.some((v,i)=>!Number.isFinite(v) || Math.abs(v-previous[i])>1))throw Error('逐层转换改变了画面位置');
          expectedTree=expectedTree.filter(([id])=>id===item.id || !removed.has(id)).map(([id,parent])=>[id===item.id?made.id:id,parent]);
          if(JSON.stringify(pairs(now))!==JSON.stringify(expectedTree))throw Error('逐层转换改变了原父组或层序');
          mapping.set(item.id,made.id);
        }
        check();const ids=actual.layer_ids.filter(id=>id!==actual.active_layer_id).concat(actual.active_layer_id).map(id=>mapping.get(id));
        await batch(ids.map((id,i)=>({...selectId(id),...(i?{selectionModifier:{_enum:'selectionModifierType',_value:'addToSelection'}}:{})})));
        const after=await capture(),wanted=ids.slice().sort((a,b)=>a-b);
        if(after.document_id!==doc.id || after.active_layer_id!==mapping.get(actual.active_layer_id) || after.layer_ids.join(',')!==wanted.join(',') || JSON.stringify(pairs(tree(doc.layers)))!==JSON.stringify(expectedTree))
          throw Error('逐层转换最终选择／层序核验失败');
        const selection=readFeedback();await context.hostControl.resumeHistory(history,true);
        return {verified:true,count:selected.length,converted_count:pending.length,unchanged_count:selected.length-pending.length,command:request.command,smart_object_ids:ids,selection_context:selection};
      }catch(error){
        try{
          await context.hostControl.resumeHistory(history,false);
          if(!sameTarget(await capture()) || JSON.stringify(pairs(tree(doc.layers)))!==JSON.stringify(original))throw Error('Rollback readback mismatch');
        }catch(_){error.resultUnknown=true;}
        throw error;
      }
    },{commandName:'Menu convert each smart object',timeOut:0});
  }
  async function convert(request) {
    return ps.core.executeAsModal(async context=>{
      if(context.isCancelled || Date.now()>request.expires_at_ms)throw Error('Cancelled or expired');
      const expected=request.target,actual=await capture();
      if(expected?.session!==session || actual.document_id!==expected.document_id || actual.active_layer_id!==expected.active_layer_id || actual.layer_ids.join(',')!==expected.layer_ids?.join(','))
        throw Error('Smart object target changed');
      const doc=ps.app.activeDocument,k=ps.constants.LayerKind;
      function tree(layers,parent=null) {return Array.from(layers).flatMap(l=>[{layer:l,id:l.id,parent},...(l.kind===k.GROUP?tree(l.layers,l.id):[])]);}
      const before=tree(doc.layers),selected=before.filter(l=>actual.layer_ids.includes(l.id));
      if(selected.length!==actual.layer_ids.length)throw Error('Smart object target missing');
      const parent=selected[0].parent;
      if(selected.some(l=>l.parent!==parent))throw Error('请在同一父组内选择要转换的图层');
      if(selected.length===1 && selected[0].layer.kind===k.SMARTOBJECT)
        return {verified:true,unchanged:true,count:1,command:'convertSmartObject',selection_context:readFeedback()};
      const removed=new Set();
      function packed(l){removed.add(l.id);if(l.kind===k.GROUP)for(const child of l.layers)packed(child);}
      for(const item of selected)packed(item.layer);
      const unchanged=before.filter(l=>!removed.has(l.id)).map(l=>[l.id,l.parent]);
      if(context.isCancelled || Date.now()>request.expires_at_ms)throw Error('Cancelled or expired');
      const history=await context.hostControl.suspendHistory({documentID:doc.id,name:'Menu convert smart object'});
      try {
        const results=await ps.action.batchPlay([{_obj:'newPlacedLayer',_options:{dialogOptions:'dontDisplay'}}],{});
        if(results.some(v=>v._obj==='error'))throw Error('Photoshop 拒绝转换智能对象');
        const after=await capture(),now=tree(doc.layers),made=now.find(l=>l.id===after.active_layer_id);
        if(after.document_id!==actual.document_id || after.layer_ids.length!==1 || !made || made.layer.kind!==k.SMARTOBJECT || made.parent!==parent ||
           JSON.stringify(now.filter(l=>l.id!==made.id).map(l=>[l.id,l.parent]))!==JSON.stringify(unchanged))
          throw Error('智能对象类型／选择／原图层顺序读回不符，已回滚');
        const selection=readFeedback();
        await context.hostControl.resumeHistory(history,true);
        return {verified:true,count:selected.length,command:'convertSmartObject',smart_object_id:made.id,selection_context:selection};
      } catch(error) {
        try {await context.hostControl.resumeHistory(history,false);}catch(_){error.resultUnknown=true;}
        throw error;
      }
    },{commandName:'Menu convert smart object',timeOut:0});
  }
  async function select(request) {
    const timings={},started=Date.now();
    return ps.core.executeAsModal(async context=>{
      timings.modal_wait_ms=Date.now()-started;
      const check=()=>{if(context.isCancelled || Date.now()>request.expires_at_ms)throw Error('Cancelled or expired');};
      check();let at=Date.now();
      const actual=await captureSelection(),expected=request.target;
      if(actual.document_id!==expected.document_id || actual.active_layer_id!==expected.active_layer_id || actual.layer_ids.join(',')!==expected.layer_ids?.join(','))throw Error('Caps target changed');
      timings.target_ms=Date.now()-at;
      const doc=ps.app.activeDocument;
      function walk(layers){return Array.from(layers).flatMap(l=>[l,...(l.kind===ps.constants.LayerKind.GROUP?walk(l.layers):[])]);}
      async function order(){
        if(doc.layers.length>=8)try{return (await readNativeTree(doc)).order;}catch(_){}
        return walk(doc.layers).map(l=>l.id);
      }
      at=Date.now();const before=await order(),members=new Set(before);
      if(!actual.layer_ids.length || !actual.layer_ids.includes(actual.active_layer_id) || actual.layer_ids.some(id=>!members.has(id)))throw Error('Caps target missing');
      timings.before_tree_ms=Date.now()-at;check();
      const command=request.command,extend=command.startsWith('extend');
      const step={_obj:'select',_target:[{_ref:'layer',_enum:'ordinal',_value:command.endsWith('Above')?'forwardEnum':'backwardEnum'}],makeVisible:false,_options:{dialogOptions:'dontDisplay'}};
      if(extend)step.selectionModifier={_enum:'selectionModifierType',_value:'addToSelection'};
      const descriptors=[];
      if(!extend && actual.layer_ids.length>1)descriptors.push({_obj:'select',_target:[{_ref:'layer',_id:actual.active_layer_id}],makeVisible:false,_options:{dialogOptions:'dontDisplay'}});
      descriptors.push(step);at=Date.now();
      let rejected;
      try {rejected=(await ps.action.batchPlay(descriptors,{})).find(v=>v._obj==='error');}
      catch(error) {rejected=error;}
      if(rejected) {
        try {
          const sameTarget=value=>value.document_id===actual.document_id && value.active_layer_id===actual.active_layer_id && value.layer_ids.join(',')===actual.layer_ids.join(',');
          const current=await captureSelection(),currentOrder=await order();
          if(current.document_id!==actual.document_id || currentOrder.join(',')!==before.join(','))throw Error('Selection recovery target changed');
          if(!sameTarget(current)) {
            // Only undo our verified multi-selection collapse, never a later target change.
            if(descriptors.length!==2 || current.active_layer_id!==actual.active_layer_id || current.layer_ids.join(',')!==String(actual.active_layer_id))throw Error('Selection recovery identity changed');
            const ids=actual.layer_ids.filter(id=>id!==actual.active_layer_id).concat(actual.active_layer_id);
            const restore=ids.map((id,i)=>({_obj:'select',_target:[{_ref:'layer',_id:id},{_ref:'document',_id:actual.document_id}],makeVisible:false,
              ...(i?{selectionModifier:{_enum:'selectionModifierType',_value:'addToSelection'}}:{}),_options:{dialogOptions:'dontDisplay'}}));
            const results=await ps.action.batchPlay(restore,{});
            if(results.some(v=>v._obj==='error') || !sameTarget(await captureSelection()) || (await order()).join(',')!==before.join(','))throw Error('Selection recovery readback mismatch');
          }
        } catch(error) {
          const unknown=Error('选层失败，原选择恢复结果待核对；不重发');unknown.resultUnknown=true;throw unknown;
        }
        if(rejected.result===-128 || rejected.cancelled===true)return {verified:true,cancelled:true,rollback_verified:true,count:0,command,selection_context:readFeedback(),selection_timings_ms:timings};
        throw Error(rejected.message||'Photoshop rejected selection');
      }
      timings.select_ms=Date.now()-at;at=Date.now();
      const after=await captureSelection(),now=await order();
      if(after.document_id!==actual.document_id || now.length!==before.length || now.some(id=>!members.has(id)))throw Error('Caps document/layer identity changed');
      if(extend?actual.layer_ids.some(id=>!after.layer_ids.includes(id)):after.layer_ids.length!==1)throw Error('Layer selection readback mismatch');
      if(now.join(',')!==before.join(','))throw Error('Caps layer order changed');
      timings.after_tree_ms=Date.now()-at;at=Date.now();
      const feedback=readFeedback();
      // No deferred old snapshot: complete only with current capabilities.
      if(feedback && (feedback.document_id!==after.document_id || feedback.layer_ids.join(',')!==after.layer_ids.join(',')))throw Error('Caps feedback identity changed');
      timings.feedback_ms=Date.now()-at;
      return {verified:true,count:actual.layer_ids.length,command,selection_context:feedback,selection_timings_ms:timings};
    },{commandName:'Menu '+request.command,timeOut:0});
  }
  async function execute(request) {
    const command=request.command,expected=request.target;
    if(!commands.includes(command) || expected?.session!==session)throw Error('Unsupported Caps command/session');
    if(command==='convertSmartObject')return convert(request);
    if(command==='convertSmartObjectsIndividually')return convertIndividually(request);
    if(['selectAbove','selectBelow','extendAbove','extendBelow'].includes(command))return select(request);
    return ps.core.executeAsModal(async context=>{
      if(context.isCancelled || Date.now()>request.expires_at_ms)throw Error('Cancelled or expired');
      const actual=await capture();
      if(actual.document_id!==expected.document_id || actual.active_layer_id!==expected.active_layer_id || actual.layer_ids.join(',')!==expected.layer_ids?.join(','))throw Error('Caps target changed');
      const doc=ps.app.activeDocument;
      function walk(layers){return Array.from(layers).flatMap(l=>[l,...(l.kind===ps.constants.LayerKind.GROUP?walk(l.layers):[])]);}
      async function readOrder() {
        // Keep full before/after tree validation. Large flat documents use
        // the already validated scalar reader, not per-layer DOM objects.
        if(doc.layers.length>=64)try {return (await readNativeTree(doc)).order;}catch(_){}
        return walk(doc.layers).map(l=>l.id);
      }
      const order=await readOrder(),membership=new Set(order);
      const selected=Array.from(doc.activeLayers),active=selected.find(l=>l.id===actual.active_layer_id);
      if(context.isCancelled || Date.now()>request.expires_at_ms)throw Error('Cancelled or expired');
      if(!active || selected.length!==actual.layer_ids.length || selected.some(l=>!actual.layer_ids.includes(l.id) || !membership.has(l.id)))throw Error('Caps target missing');
      if(['copyStyle','editStyle'].includes(command) && selected.length!==1)throw Error('此操作需要单选图层');
      const selecting=['selectAbove','selectBelow','extendAbove','extendBelow'].includes(command);
      const mutate=!selecting && command!=='copyStyle';
      if(mutate && selected.some(l=>l.isBackgroundLayer || l.allLocked || l.positionLocked))throw Error('请先解锁目标图层');
      async function batch(descriptors){const values=await ps.action.batchPlay(descriptors,{});if(values.some(v=>v._obj==='error')){const v=values.find(v=>v._obj==='error');const e=Error(v.message || 'Photoshop rejected command');e.cancelled=v.result===-128;throw e;}return values;}
      async function effects(){return (await readLayerEffects(ps,doc,[active.id]))[0].layerEffects;}
      let history=null;
      if(mutate && command!=='editStyle')history=await context.hostControl.suspendHistory({documentID:doc.id,name:'Menu '+command});
      try {
        if(selecting) {
          const descriptor={_obj:'select',_target:[{_ref:'layer',_enum:'ordinal',_value:command.endsWith('Above')?'forwardEnum':'backwardEnum'}],makeVisible:false,_options:{dialogOptions:'dontDisplay'}};
          if(command.startsWith('extend'))descriptor.selectionModifier={_enum:'selectionModifierType',_value:'addToSelection'};
          // Ordinal selection over a full multi-selection may keep every layer.
          // Collapse to the verified active ID before stepping, in this same
          // modal request and batch. Shift extension never collapses the range.
          const descriptors=[];
          if(!command.startsWith('extend') && selected.length>1)
            descriptors.push({_obj:'select',_target:[{_ref:'layer',_id:active.id}],makeVisible:false,_options:{dialogOptions:'dontDisplay'}});
          descriptors.push(descriptor);
          await batch(descriptors);
        } else if(command==='moveUp' || command==='moveDown') {
          const parent=active.parent,parentId=parent?.id??null;
          if(selected.some(l=>(l.parent?.id??null)!==parentId))throw Error('请在同一父组内移动');
          const siblings=Array.from(parent?parent.layers:doc.layers),indices=siblings.map((l,i)=>actual.layer_ids.includes(l.id)?i:-1).filter(i=>i>=0);
          if(indices.length!==selected.length || indices.at(-1)-indices[0]+1!==indices.length)throw Error('请先选择相邻图层');
          const up=command==='moveUp',anchor=siblings[up?indices[0]-1:indices.at(-1)+1];
          if(anchor && !anchor.isBackgroundLayer){const chosen=siblings.filter(l=>actual.layer_ids.includes(l.id));for(const layer of up?chosen:chosen.reverse())await layer.move(anchor,up?ps.constants.ElementPlacement.PLACEBEFORE:ps.constants.ElementPlacement.PLACEAFTER);}
          const next=Array.from(parent?parent.layers:doc.layers,l=>l.id),expectedOrder=siblings.map(l=>l.id);
          if(anchor && !anchor.isBackgroundLayer){const chosen=expectedOrder.filter(id=>actual.layer_ids.includes(id)),others=expectedOrder.filter(id=>!actual.layer_ids.includes(id));let at=others.indexOf(anchor.id)+(up?0:1);others.splice(at,0,...chosen);if(next.join(',')!==others.join(','))throw Error('Step order readback mismatch');}
        } else if(command==='moveDocumentTop' || command==='moveDocumentBottom') {
          const parentId=l=>l.parent?.id===doc.id?null:l.parent?.id??null;
          const parent=parentId(active);
          if(selected.some(l=>parentId(l)!==parent))throw Error('请在同一父组内选择图层');
          for(let p=active.parent;p && p.id!==doc.id;p=p.parent)if(p.allLocked || p.positionLocked)throw Error('请先解锁父组');
          const tree=layers=>Array.from(layers).flatMap(l=>[[l.id,parentId(l)],...(l.kind===ps.constants.LayerKind.GROUP?tree(l.layers):[])]);
          const before=tree(doc.layers),ids=new Set(actual.layer_ids),moving=new Set(walk(selected).map(l=>l.id));
          const siblings=Array.from(active.parent?.layers??doc.layers),chosen=siblings.filter(l=>ids.has(l.id));
          if(chosen.length!==selected.length)throw Error('文档排序目标不完整');
          const top=command==='moveDocumentTop',roots=Array.from(doc.layers),others=roots.filter(l=>!ids.has(l.id));
          const anchor=top?others[0]:others.at(-1),background=anchor?.isBackgroundLayer;
          if(anchor)for(const l of top || background?chosen:[...chosen].reverse()) {
            if(context.isCancelled)throw Error('Cancelled');
            await l.move(anchor,top || background?ps.constants.ElementPlacement.PLACEBEFORE:ps.constants.ElementPlacement.PLACEAFTER);
          }
          const now=tree(doc.layers),expected=chosen.map(l=>l.id),rootIds=Array.from(doc.layers).filter(l=>!l.isBackgroundLayer).map(l=>l.id);
          const edge=top?rootIds.slice(0,chosen.length):rootIds.slice(-chosen.length);
          if(edge.join(',')!==expected.join(',') || chosen.some(l=>parentId(l)!==null) ||
             JSON.stringify(now.filter(([id])=>!moving.has(id)))!==JSON.stringify(before.filter(([id])=>!moving.has(id))) ||
             JSON.stringify(now.filter(([id])=>moving.has(id)))!==JSON.stringify(before.filter(([id])=>moving.has(id)).map(([id,parent])=>[id,ids.has(id)?null:parent])))
            throw Error('文档排序父组／层序读回不符，已回滚');
        } else if(command==='copyStyle') {
          if(!await effects())throw Error('当前图层没有可复制的样式');
          await batch([{_obj:'copyEffects',_options:{dialogOptions:'dontDisplay'}}]);
        } else if(command==='pasteStyle') {
          await batch([{_obj:'pasteEffects',_options:{dialogOptions:'dontDisplay'}}]);
          const values=await readLayerEffects(ps,doc,selected.map(l=>l.id));
          if(values.some(v=>!v.layerEffects))throw Error('粘贴样式未通过读回核验');
        } else {
          // Photoshop edits the current effects object in its own dialog.
          // Never replace existing effects with an empty object before showing.
          const value=await effects() || {_obj:'layerEffects',scale:{_unit:'percentUnit',_value:100}};
          await batch([{_obj:'set',_target:[{_ref:'property',_property:'layerEffects'},{_ref:'layer',_id:active.id}],to:value,_options:{dialogOptions:'display'}}]);
        }
        const after=await capture(),now=await readOrder();
        if(after.document_id!==actual.document_id || now.length!==order.length || now.some(id=>!membership.has(id)))throw Error('Caps document/layer identity changed');
        if(!selecting && after.layer_ids.join(',')!==actual.layer_ids.join(','))throw Error('Caps selection changed');
        if(selecting && (command.startsWith('extend')?actual.layer_ids.some(id=>!after.layer_ids.includes(id)):after.layer_ids.length!==1))throw Error('Layer selection readback mismatch');
        if(!['moveUp','moveDown','moveDocumentTop','moveDocumentBottom'].includes(command) && now.join(',')!==order.join(','))throw Error('Caps layer order changed');
        if(history)await context.hostControl.resumeHistory(history,true);
        return {verified:true,count:selected.length,command,selection_context:readFeedback()};
      } catch(error) {
        if(history){try{await context.hostControl.resumeHistory(history,false);}catch(_){error.resultUnknown=true;}}
        if(error.cancelled)return {verified:true,cancelled:true,count:0,command,selection_context:readFeedback()};
        throw error;
      }
    },{commandName:'Menu '+command,interactive:command==='editStyle',timeOut:0});
  }
  return {commands,execute};
}

// Selected vector masks only. No selection changes, paint setters or per-frame history.
function createBatchMasks(ps,session,capture,selectionContext,legacyPreview) {
  const clone=v=>JSON.parse(JSON.stringify(v)),same=(a,b)=>JSON.stringify(a)===JSON.stringify(b);
  let cache=null,serial=0;
  const contentKeys=['layerKind','adjustment','AGMStrokeStyleInfo','hasVectorMask','hasUserMask','userMaskFeather','userMaskDensity'];
  const close=(a,b)=>Math.abs(a.feather-b.feather)<.01 && Math.abs(a.density-b.density)<=100/255+.001;
  function validate(p) {
    if(!p || p.mask_batch!==true || Object.keys(p).sort().join(',')!=='mask,mask_batch,revision' || typeof p.revision!=='string' || !p.revision || p.revision.length>128 ||
        !p.mask || !Object.keys(p.mask).length || Object.keys(p.mask).some(k=>!['feather','density'].includes(k) || typeof p.mask[k]!=='number' || !Number.isFinite(p.mask[k]) || p.mask[k]<0 || p.mask[k]>(k==='feather'?1000:100)))throw Error('Invalid batch mask parameters');
  }
  function locked(layer) {for(let p=layer;p;p=p.parent)if(p.locked || p.allLocked || p.pixelsLocked || p.positionLocked || p.transparentPixelsLocked || p.isBackgroundLayer)return true;return false;}
  function targetSame(a,b) {return a.session===b?.session && a.document_id===b.document_id && a.active_layer_id===b.active_layer_id && same(a.layer_ids,b.layer_ids);}
  async function snapshot(expected=null,checkpoint=null) {
    const identity=await capture();if(expected && !targetSame(identity,expected))throw Error('批量蒙版选择已改变');
    const doc=ps.app.activeDocument,layers=Array.from(doc.activeLayers).sort((a,b)=>a.id-b.id);
    async function checkLocks(){for(let i=0;i<layers.length;i++){if(checkpoint && i%4===0)await checkpoint();if(locked(layers[i]))return false;}return true;}
    if(layers.length<2 || layers.length>128 || !await checkLocks())throw Error('请选择2至128个未锁定的矢量蒙版图层');
    async function read(descriptors){
      if(!checkpoint)return ps.action.batchPlay(descriptors,{});
      const values=[];for(let i=0;i<descriptors.length;i+=4){await checkpoint();values.push(...await ps.action.batchPlay(descriptors.slice(i,i+4),{}));}await checkpoint();return values;
    }
    const descriptors=await read(layers.map(l=>({_obj:'get',_target:[{_ref:'layer',_id:l.id},{_ref:'document',_id:doc.id}],_options:{dialogOptions:'dontDisplay'}})));
    if(descriptors.length!==layers.length || descriptors.some((v,i)=>v?._obj==='error' || v?.layerID!==layers[i].id || v.hasVectorMask!==true))throw Error('所有选中图层都必须有矢量蒙版');
    const paths=await read(layers.map(l=>({_obj:'get',_target:[{_ref:'property',_property:'pathContents'},
      {_ref:'path',_enum:'path',_value:'vectorMask'},{_ref:'layer',_id:l.id},{_ref:'document',_id:doc.id}],_options:{dialogOptions:'dontDisplay'}})));
    const parents=new Map();
    const members=[];for(let i=0;i<layers.length;i++){
      if(checkpoint && i%4===0)await checkpoint();
      const l=layers[i];
      const mask={feather:l.vectorMaskFeather,density:l.vectorMaskDensity},parent=l.parent?.id??null;
      if(!Number.isFinite(mask.feather)||mask.feather<0||mask.feather>1000||!Number.isFinite(mask.density)||mask.density<0||mask.density>100 || !paths[i]?.pathContents)throw Error('矢量蒙版读回不可用');
      if(!parents.has(parent)){
        const siblings=l.parent?l.parent.layers:doc.layers;
        if(!checkpoint)parents.set(parent,Array.from(siblings,x=>x.id));
        else {const order=[],count=siblings.length;for(let j=0;j<count;j++){if(j%8===0)await checkpoint();order.push(siblings[j].id);}parents.set(parent,order);}
      }
      members.push({id:l.id,mask,content:Object.fromEntries(contentKeys.map(k=>[k,descriptors[i][k]??null])),path:paths[i].pathContents,parent_id:parent,order:parents.get(parent)});
    }
    if(!targetSame(await capture(),identity) || !await checkLocks())throw Error('批量蒙版读取期间选择已改变');
    return {identity,members};
  }
  async function observe(context,checkpoint=null) {
    try {
      if(context.locked || context.background || !Array.isArray(context.layer_ids) || context.layer_ids.length<2 || context.layer_ids.length>128)return {available:false};
      if(checkpoint)await checkpoint();
      const snap=await snapshot(null,checkpoint);
      if(snap.identity.document_id!==context.document_id || !same(snap.identity.layer_ids,context.layer_ids))return {available:false};
      const signature=JSON.stringify(snap);if(signature.length>524288)throw Error('所选矢量路径过于复杂，请减少批量图层');
      const revision=cache?.signature===signature?cache.data.revision:session+':batch:'+ ++serial;
      const mask=clone(snap.members[0].mask),mixed=Object.fromEntries(['feather','density'].map(k=>[k,snap.members.some(v=>Math.abs(v.mask[k]-mask[k])>(k==='feather'?.01:100/255+.001))]));
      const data={available:true,mask_batch:true,vector:true,revision,mask,mixed,layer_ids:snap.identity.layer_ids,active_layer_id:snap.identity.active_layer_id,
        live_preview:typeof ps.core.redrawDocument==='function'||legacyPreview,...(legacyPreview && typeof ps.core.redrawDocument!=='function'?{preview_engine:'legacy-com'}:{})};
      cache={signature,data,snap};return data;
    } catch(error) {if(error.menuReadInterrupted)throw error;return {available:false,reason:String(error).slice(0,200)};}
  }
  async function prepare(request) {
    validate(request.paint);
    if(Date.now()>request.expires_at_ms)throw Error('Batch request expired');
    const before=await observe(selectionContext(true));
    if(!before.available || before.revision!==request.paint.revision || !targetSame(cache.snap.identity,request.target))throw Error('批量蒙版或选择已改变，请重新打开菜单');
    return {before:clone(before),members:clone(cache.snap.members),identity:clone(cache.snap.identity)};
  }
  async function prepareLegacy(request) {
    // Older backends do not promise the exact native baseline check. Keep
    // their original complete preparation, including after a plugin-only update.
    if(request.native_batch_validation!==2)return prepare(request);
    validate(request.paint);
    const observed=cache;
    if(Date.now()>request.expires_at_ms || !observed || observed.data.revision!==request.paint.revision ||
        !targetSame(observed.snap.identity,request.target))throw Error('批量蒙版预览已失效，请重新打开菜单');
    // The observed snapshot supplies the expected values, never authority to
    // write. The standalone executor rereads every member/content/path/order
    // and checks exact original masks before opening history or mutating PS.
    // Do not duplicate that complete host read here or start a second modal.
    if(!targetSame(await capture(),request.target) || Date.now()>request.expires_at_ms)throw Error('批量蒙版选择已改变');
    // Preserve the observed double without decimal parsing in ExtendScript.
    // Its number-to-string/parser precision differs from the UXP runtime.
    const doubleHex=value=>{const view=new DataView(new ArrayBuffer(8));view.setFloat64(0,value,false);
      return Array.from({length:8},(_,i)=>view.getUint8(i).toString(16).padStart(2,'0')).join('');};
    const members=clone(observed.snap.members).map(m=>({...m,mask_exact:{feather:doubleHex(m.mask.feather),density:Math.round(m.mask.density*255/100)}}));
    return {before:clone(observed.data),members,identity:clone(observed.snap.identity),native_batch_validation:2};
  }
  function check(actual,baseline,masks) {
    if(!targetSame(actual.identity,baseline.identity) || actual.members.length!==baseline.members.length)throw Error('Batch identity changed');
    for(let i=0;i<actual.members.length;i++){
      const {mask:a,...rest}=actual.members[i],{mask:b,...base}=baseline.members[i];
      if(!same(rest,base) || !close(a,masks[i]))throw Error('Batch mask/content/path/order readback mismatch');
    }
  }
  async function finishLegacy(request,prepared,response) {
    if(response.phase!=='completed'){const e=Error(response.error||'Batch native preview failed');e.resultUnknown=response.result_unknown===true;throw e;}
    try {
      const v=response.result;validate(v.paint);
      if(v.verified!==true || v.mask_batch!==true || !same(v.layer_ids,request.target.layer_ids) || !Number.isInteger(v.count) || v.count<0 || v.count>prepared.members.length || v.placement_moves!==0 ||
          v.paint.revision!==request.paint.revision || v.preview?.engine!=='standalone-extendscript' || !Number.isInteger(v.preview.last_seq) ||
          v.cancelled && v.rollback_verified!==true || !Array.isArray(v.masks) || v.masks.length!==prepared.members.length)throw Error('Incomplete batch proof');
      for(let i=0;i<v.masks.length;i++)if(v.masks[i].id!==prepared.members[i].id || !close(v.masks[i].mask,{...prepared.members[i].mask,...(v.cancelled?{}:v.paint.mask)}))throw Error('Batch final mask mismatch');
      cache=null;const {paint,...result}=v;try{result.selection_context=selectionContext();}catch(_){}return result;
    } catch(error){error.resultUnknown=true;throw error;}
  }
  async function apply(request,io) {
    validate(request.paint);const preview=request.command==='shape.preview';
    if(preview && (typeof ps.core.redrawDocument!=='function' || !io))throw Error('Batch live preview unavailable');
    let progressFrame=0;const progress=async(stage,done=0,total=0)=>{if(io?.progress)await io.progress(stage,done,total,progressFrame);};
    let uncertain=false;
    try {return await ps.core.executeAsModal(async context=>{
      await progress('read');const prepared=await prepare(request),baseline={identity:prepared.identity,members:prepared.members};
      let applied=baseline.members.map(v=>clone(v.mask)),params=request.paint,seq=0,updates=0,changed=false,cancelled=false;
      let historyOpen=false,committed=false,history;const touched=new Set();
      const verify=async masks=>{await progress('verify');check(await snapshot(baseline.identity),baseline,masks);await progress('verify',masks.length,masks.length);};
      async function restoreSelection() {
        const actual=await capture();if(actual.document_id!==baseline.identity.document_id)throw Error('Batch document changed during rollback');
        if(targetSame(actual,baseline.identity))return;
        const ids=baseline.identity.layer_ids.filter(id=>id!==baseline.identity.active_layer_id).concat(baseline.identity.active_layer_id);
        const results=await ps.action.batchPlay(ids.map((id,i)=>({_obj:'select',_target:[{_ref:'layer',_id:id},{_ref:'document',_id:baseline.identity.document_id}],
          makeVisible:false,...(i?{selectionModifier:{_enum:'selectionModifierType',_value:'addToSelection'}}:{}),_options:{dialogOptions:'dontDisplay'}})),{});
        if(results.some(v=>v?._obj==='error'))throw Error('Batch selection restoration failed');
      }
      async function frame(p) {
        validate(p);if(p.revision!==prepared.before.revision)throw Error('Batch revision changed');
        if(context.isCancelled)throw Error('Cancelled');
        await verify(applied);const wanted=baseline.members.map(v=>({...v.mask,...p.mask})),commands=[];
        for(let i=0;i<wanted.length;i++)for(const k of ['feather','density'])if(Math.abs(wanted[i][k]-applied[i][k])>(k==='feather'?.00001:100/255+.001)){
          commands.push({_obj:'set',_target:[{_ref:'layer',_id:baseline.members[i].id},{_ref:'document',_id:baseline.identity.document_id}],
            to:{_obj:'layer',[k==='feather'?'vectorMaskFeather':'vectorMaskDensity']:{_unit:k==='feather'?'pixelsUnit':'percentUnit',_value:wanted[i][k]}},_options:{dialogOptions:'silent'}});touched.add(baseline.members[i].id);
        }
        if(commands.length){await progress('write');changed=true;const results=await ps.action.batchPlay(commands,{});if(results.length!==commands.length || results.some(v=>v?._obj==='error'))throw Error('Batch mask setter failed');await progress('write',wanted.length,wanted.length);}
        await verify(wanted);applied=wanted;params=p;updates++;
        if(preview && commands.length){await progress('redraw');await ps.core.redrawDocument({documentID:baseline.identity.document_id});await verify(applied);}await progress('ready',wanted.length,wanted.length);
      }
      try {
        history=await context.hostControl.suspendHistory({documentID:baseline.identity.document_id,name:'批量羽化／密度'});historyOpen=true;
        if(!preview)await frame(params);
        else {
          let lease=Date.now()+4000;
          while(true){
            if(context.isCancelled)throw Error('Cancelled');
            const packet=await io.read(),now=Date.now();
            if(packet){
              if(packet.id!==request.id||packet.session!==request.session||packet.token!==request.token||!Number.isSafeInteger(packet.seq)||packet.seq<seq||!Number.isFinite(packet.lease_until_ms)||packet.lease_until_ms>now+5000||!['preview','commit','cancel'].includes(packet.phase))throw Error('Invalid batch mailbox');
              lease=packet.lease_until_ms;if(lease<now || packet.phase==='cancel'){cancelled=true;break;}
              validate(packet.paint);if(packet.paint.revision!==prepared.before.revision || packet.seq===seq && !same(packet.paint,params))throw Error('Batch sequence payload changed');
              if(packet.seq>seq){progressFrame=packet.seq;await frame(packet.paint);seq=packet.seq;if(io.applied)await io.applied({seq,updates,count:baseline.members.length,mask_batch:true,redraw_completed:true});}
              if(packet.phase==='commit')break;
            } else if(now>lease){cancelled=true;break;}
            await new Promise(resolve=>setTimeout(resolve,40));
          }
        }
        await verify(applied);await progress(cancelled?'restore':'commit');historyOpen=false;uncertain=true;await context.hostControl.resumeHistory(history,!cancelled);committed=!cancelled;uncertain=false;
        if(cancelled){await restoreSelection();await verify(baseline.members.map(v=>v.mask));if(changed){await progress('restore-redraw');await ps.core.redrawDocument({documentID:baseline.identity.document_id});}await verify(baseline.members.map(v=>v.mask));}
        cache=null;return {verified:true,mask_batch:true,count:cancelled?0:touched.size,placement_moves:0,layer_ids:baseline.identity.layer_ids,
          ...(cancelled?{cancelled:true,rollback_verified:true}:{}),preview:{updates,last_seq:seq},selection_context:selectionContext()};
      } catch(error){
        uncertain=uncertain||committed||!!error.resultUnknown;
        if(historyOpen){historyOpen=false;try{await progress('restore');await context.hostControl.resumeHistory(history,false);await restoreSelection();await verify(baseline.members.map(v=>v.mask));if(preview&&changed){await progress('restore-redraw');await ps.core.redrawDocument({documentID:baseline.identity.document_id});}await verify(baseline.members.map(v=>v.mask));}catch(_){uncertain=true;}}
        cache=null;throw error;
      }
    },{commandName:'批量羽化／密度',timeOut:0,...(preview?{interactive:true}:{})});}
    catch(error){if(uncertain){const e=Error(String(error));e.resultUnknown=true;throw e;}throw error;}
  }
  return {observe,apply,prepare,prepareLegacy,finishLegacy};
}

function extendedLabelColors(version) {
  const match=/^(\d+)\.(\d+)(?:\.|$)/.exec(String(version));
  return !!match && (Number(match[1])>25 || Number(match[1])===25 && Number(match[2])>=2);
}
function selectionKind(k,kind,background) {
  if(background)return 'background';
  for(const [key,name] of [['NORMAL','pixel'],['GROUP','group'],['TEXT','text'],['SMARTOBJECT','smart'],
      ['SOLIDFILL','shape-fill'],['GRADIENTFILL','shape-fill'],['PATTERNFILL','shape-fill']])
    if(k[key]!==undefined && kind===k[key])return name;
  const adjustment=['BLACKANDWHITE','BRIGHTNESSCONTRAST','CHANNELMIXER','COLORBALANCE','COLORLOOKUP',
    'CURVES','EXPOSURE','GRADIENTMAP','HUESATURATION','INVERSION','LEVELS','PHOTOFILTER',
    'POSTERIZE','SELECTIVECOLOR','THRESHOLD','VIBRANCE','GRAIN','CLARITY'];
  if(adjustment.some(key=>k[key]!==undefined && kind===k[key]))return 'adjustment';
  return Object.values(k).includes(kind)?'other':'unknown';
}
function selectionQuickMask(constants,mode,depth,ids,kinds,background,locked,parentLocked,sameParent) {
  const modes=constants.DocumentMode || {},depths=constants.BitsPerChannelType || {};
  const layered=['RGB','CMYK','LAB','GRAYSCALE'].some(key=>modes[key]!==undefined && mode===modes[key]);
  let mask=layered && ids.length && !parentLocked && !kinds.includes('unknown')?3:0;
  if(mask && !background && !locked && sameParent)mask|=4;
  if(mask && [depths.EIGHT,depths.SIXTEEN].some(v=>v!==undefined && depth===v)) {
    mask|=8;if(mode===modes.RGB)mask|=112;
  }
  return mask;
}
function nativeKinds(constants) {
  const k=constants.LayerKind;
  return {1:k.NORMAL,2:k.CURVES,3:k.TEXT,4:k.SOLIDFILL,5:k.SMARTOBJECT,6:k.VIDEO,8:k.LAYER3D,9:k.GRADIENTFILL,10:k.PATTERNFILL,11:k.SOLIDFILL};
}
function nativeModeDepth(constants,before) {
  const m=constants.DocumentMode,b=constants.BitsPerChannelType;
  const modes={RGBColor:m.RGB,CMYKColor:m.CMYK,labColor:m.LAB,grayscale:m.GRAYSCALE,
    bitmap:m.BITMAP,indexedColor:m.INDEXEDCOLOR,multichannel:m.MULTICHANNEL,duotone:m.DUOTONE};
  const mode=modes[before[4].mode?._value],depth=({8:b.EIGHT,16:b.SIXTEEN,32:b.THIRTYTWO})[before[5].depth];
  if(mode===undefined||depth===undefined)throw Error('Unsupported native document mode or depth');
  return {mode,depth};
}
// Photoshop 24.2 Windows has severe per-getter DOM overhead outside modal work.
// No host DOM objects or cross-request capability cache enter this read path.
async function nativeDocumentCount(ps,checkpoint=null) {
  if(checkpoint)await checkpoint();
  const rows=await ps.action.batchPlay([{_obj:'get',_target:[{_ref:'property',_property:'numberOfDocuments'},
    {_ref:'application',_enum:'ordinal',_value:'targetEnum'}],_options:{dialogOptions:'silent'}}],{});
  if(checkpoint)await checkpoint();
  const count=rows?.[0]?.numberOfDocuments;
  if(!Number.isInteger(count)||count<0)throw Error('Native document state unavailable');
  return count;
}
async function nativeSelectionContext(ps,session,reader,checkpoint=null,targeted=true,plan={total:null,single:true}) {
  if(await nativeDocumentCount(ps,checkpoint)===0)return {session,document_id:null,layer_ids:[],kind:'none',mask:0};
  const tick=async()=>{if(checkpoint)await checkpoint();};
  const ref=[{_ref:'document',_enum:'ordinal',_value:'targetEnum'}];
  const property=name=>({_obj:'get',_target:[{_ref:'property',_property:name},...ref],_options:{dialogOptions:'silent'}});
  async function batch(commands,required=commands.length){await tick();const result=await ps.action.batchPlay(commands,{});await tick();
    if(!Array.isArray(result)||result.length!==commands.length||result.slice(0,required).some(v=>!v||v._obj==='error'))throw Error('Native selection unavailable');return result;}
  function identity(rows){
    const id=rows[0].documentID,refs=rows[1].targetLayersIDs;
    if(!Number.isInteger(id)||id<=0||!Array.isArray(refs)||!refs.length||refs.some(x=>x?._ref!=='layer'||!Number.isInteger(x._id)||x._id<=0))throw Error('Invalid native selection identity');
    const ids=refs.map(x=>x._id).sort((a,b)=>a-b);if(new Set(ids).size!==ids.length)throw Error('Duplicate native selection');return {id,ids};
  }
  const fields=['layerID','parentLayerID','itemIndex','layerSection','layerKind','background','layerLocking'];
  const scalar=target=>({_obj:'multiGet',_target:{_ref:target},extendedReference:[fields],
    options:{failOnMissingProperty:false,failOnMissingElement:false},_options:{dialogOptions:'silent'}});
  const headers=()=>['documentID','targetLayersIDs','numberOfLayers','hasBackgroundLayer','mode','depth'].map(property);
  const prefetch=targeted && plan.single && (plan.total===null || plan.total>8);
  const initial=await batch([...headers(),...(prefetch?[scalar([{_ref:'layer',_enum:'ordinal',_value:'targetEnum'},...ref])]:[])],6);
  const before=initial.slice(0,6);
  const chosen=identity(before),count=before[2].numberOfLayers,bg=before[3].hasBackgroundLayer;
  if(!Number.isSafeInteger(count)||count<0||count>1000000||typeof bg!=='boolean'||count+(bg?1:0)<1)throw Error('Native selection range unavailable');
  const total=count+(bg?1:0),documentRef=[{_ref:'document',_id:chosen.id}];
  const kindMap=nativeKinds(ps.constants),{mode,depth}=nativeModeDepth(ps.constants,before);
  if(targeted){plan.total=total;plan.single=chosen.ids.length===1;}
  let fallback='';
  // Tiny trees already return fewer records with fewer descriptors. Preserve
  // that bounded fast path rather than making small documents slower.
  if(targeted && total>8 && chosen.ids.length===1){
    const row=prefetch?initial[6]:(await batch([scalar([{_ref:'layer',_id:chosen.ids[0]},...documentRef])],0))[0],section=row?.layerSection?._value;
    if(Number.isInteger(row?.layerID) && row.layerID>0 && row.layerID!==chosen.ids[0])throw Error('Layer context changed during native read');
    const locks=row?.layerLocking,position=row?.itemIndex;
    const root=row && row.layerID===chosen.ids[0] && section==='layerSectionContent' &&
      Number.isInteger(row.parentLayerID) && row.parentLayerID<=0 && row.background===false &&
      Number.isInteger(position) && position>=(bg?2:1) && position<=total &&
      locks && typeof locks.protectAll==='boolean' && typeof locks.protectPosition==='boolean' && kindMap[row.layerKind]!==undefined;
    if(root){
      // Only immediate AM neighbours are needed for an ordinary root layer.
      // A section end above denotes a group sibling; a section start below
      // denotes a group sibling. No partial tree is presented as a full tree.
      const indices=[position<total?position+1:null,position>1?position-1:null];
      const commands=indices.filter(x=>x!==null).map(index=>scalar([{_ref:'layer',_index:index-(bg?1:0)},...documentRef]));
      const neighbours=commands.length?await batch(commands,0):[];
      let offset=0;const adjacent=indices.map(index=>index===null?null:neighbours[offset++]);
      function relation(value,index,above){
        if(index===null)return {nonGroup:false};
        if(!value || value._obj==='error' || value.itemIndex!==index)return null;
        const part=value.layerSection?._value;
        if(above && part==='layerSectionEnd')return {nonGroup:false};
        if(!Number.isInteger(value.layerID)||value.layerID<=0||value.layerID===row.layerID||
           !Number.isInteger(value.parentLayerID)||value.parentLayerID>0)return null;
        if(part==='layerSectionStart' && !above)return {nonGroup:false};
        if(part!=='layerSectionContent' || typeof value.background!=='boolean' ||
           value.background!==(bg && index===1))return null;
        return {nonGroup:true};
      }
      const above=relation(adjacent[0],indices[0],true),below=relation(adjacent[1],indices[1],false);
      if(above && below){
        const selected=scalar([{_ref:'layer',_id:chosen.ids[0]},...documentRef]);
        // Close the bracket with active document/selection identity last.
        const verified=await batch([selected,...commands,...headers().slice(2),...headers().slice(0,2)]);
        const tail=verified.slice(1+commands.length),after=[...tail.slice(4),...tail.slice(0,4)],last=identity(after);
        const equal=(a,b)=>JSON.stringify(a)===JSON.stringify(b);
        if(last.id!==chosen.id || last.ids.join(',')!==chosen.ids.join(',') ||
           !equal(after.slice(2,6),before.slice(2,6)) || !equal(verified[0],row) ||
           !equal(verified.slice(1,1+commands.length),neighbours))throw Error('Layer context changed during native root read');
        const kind=selectionKind(ps.constants.LayerKind,kindMap[row.layerKind],false),locked=locks.protectAll||locks.protectPosition;
        if(kind==='unknown')throw Error('Unsupported native layer kind');
        let mask=2;if(above.nonGroup||below.nonGroup)mask|=1;
        if(!locked){if(position<total)mask|=4;if(position>(bg?2:1))mask|=8;}
        return {session,document_id:chosen.id,layer_ids:chosen.ids,selection_count:1,kind,types:[kind],
          background:false,locked,same_parent:true,mask,
          quick_mask:selectionQuickMask(ps.constants,mode,depth,chosen.ids,[kind],false,locked,false,true),
          read_path:'native-root-scalars',read_scope:'root-single'};
      }
      fallback='uncertain-adjacency';
    }else fallback='structural-or-property';
    // A read-only fallback is permitted only for the same current selection.
    // Cancellation, a superseding menu and identity changes never downgrade.
    const fence=identity(await batch(['documentID','targetLayersIDs'].map(property)));
    if(fence.id!==chosen.id || fence.ids.join(',')!==chosen.ids.join(','))throw Error('Layer context changed before native fallback');
  }
  if(count>4096)throw Error('Native selection range unavailable');
  const [result]=await batch([{_obj:'multiGet',_target:{_ref:documentRef},extendedReference:[
    ['layerID','parentLayerID','itemIndex','layerSection','layerKind','background','layerLocking'],
    {_obj:'layer',index:bg?0:1,count:total}],options:{failOnMissingProperty:false,failOnMissingElement:false},_options:{dialogOptions:'silent'}}]);
  if(!Array.isArray(result.list)||result.list.length!==total)throw Error('Incomplete native selection tree');
  const rows=result.list.slice().sort((a,b)=>b.itemIndex-a.itemIndex),layers=[],map=new Map(),stack=[];
  const k=ps.constants.LayerKind;
  for(let i=0;i<rows.length;i++){
    if(i%16===0)await tick();const row=rows[i],section=row?.layerSection?._value;
    if(!row||row.itemIndex!==total-i)throw Error('Invalid native tree order');
    if(section==='layerSectionEnd'){if(!stack.length)throw Error('Unbalanced native groups');stack.pop();continue;}
    const parent=stack.length?stack[stack.length-1]:null,locks=row.layerLocking;
    if(!['layerSectionContent','layerSectionStart'].includes(section)||!Number.isInteger(row.layerID)||row.layerID<=0||map.has(row.layerID)||
       !Number.isInteger(row.parentLayerID)||(row.parentLayerID>0?row.parentLayerID:null)!==(parent?.id??null)||
       !locks||typeof locks.protectAll!=='boolean'||typeof locks.protectPosition!=='boolean')throw Error('Invalid native layer membership or locks');
    // Background is an explicit, validated role; never infer it from an
    // otherwise unknown numeric kind (nor reject it for kind=12).
    const kind=section==='layerSectionStart'?k.GROUP:row.background===true?k.NORMAL:kindMap[row.layerKind];
    if(kind===undefined)throw Error('Unsupported native layer kind');
    const layer={id:row.layerID,kind,parent,layers:[],isBackgroundLayer:row.background===true,allLocked:locks.protectAll,positionLocked:locks.protectPosition};
    if(layer.isBackgroundLayer && (!bg||parent||i!==rows.length-1))throw Error('Invalid native background');
    (parent?parent.layers:layers).push(layer);map.set(layer.id,layer);if(section==='layerSectionStart')stack.push(layer);
  }
  if(stack.length||!layers.length||(bg!==layers[layers.length-1].isBackgroundLayer)||chosen.ids.some(id=>!map.has(id)))throw Error('Incomplete native selection membership');
  const doc={id:chosen.id,layers,activeLayers:chosen.ids.map(id=>map.get(id)),mode,bitsPerChannel:depth};
  const virtual={app:{documents:[doc],activeDocument:doc},constants:ps.constants};
  const iterator=reader(false,null,virtual);let step;do{step=iterator.next();await tick();}while(!step.done);
  const after=await batch(['documentID','targetLayersIDs','mode','depth'].map(property)),last=identity(after);
  if(last.id!==chosen.id||last.ids.join(',')!==chosen.ids.join(',')||JSON.stringify(after[2].mode)!==JSON.stringify(before[4].mode)||after[3].depth!==before[5].depth)throw Error('Layer context changed during native read');
  return {...step.value,read_path:'native-24.2',...(targeted && total<=8?{read_scope:'bounded-tree'}:{}),...(fallback?{read_fallback:fallback}:{})};
}

async function nativeCapture(ps,session) {
  if(await nativeDocumentCount(ps)===0)throw Error('No active document; no operation submitted');
  const property=(name,layer=false)=>({_obj:'get',_target:[{_ref:'property',_property:name},{_ref:layer?'layer':'document',_enum:'ordinal',_value:'targetEnum'}],_options:{dialogOptions:'silent'}});
  const result=await ps.action.batchPlay([property('documentID'),property('targetLayersIDs'),property('layerID',true)],{});
  if(!Array.isArray(result)||result.length!==3||result.some(v=>!v||v._obj==='error'))throw Error('Native target unavailable');
  const id=result[0].documentID,refs=result[1].targetLayersIDs,active=result[2].layerID;
  if(!Number.isInteger(id)||id<=0||!Array.isArray(refs)||!refs.length||refs.some(x=>x?._ref!=='layer'||!Number.isInteger(x._id)||x._id<=0))throw Error('Invalid native target');
  const ids=refs.map(x=>x._id).sort((a,b)=>a-b);
  if(new Set(ids).size!==ids.length||!ids.includes(active))throw Error('Invalid native active layer');
  const after=await ps.action.batchPlay([property('documentID'),property('targetLayersIDs')],{});
  if(after?.[0]?.documentID!==id||!Array.isArray(after[1]?.targetLayersIDs)||after[1].targetLayersIDs.map(x=>x._id).sort((a,b)=>a-b).join(',')!==ids.join(','))throw Error('Target changed during native capture');
  return {session,document_id:id,active_layer_id:active,layer_ids:ids};
}
function createLayerSearch(ps,session,capture) {
  const types=['all','pixel','text','group','shape','smart-object','adjustment','fill','other'];
  const property=(name,ref)=>({_obj:'get',_target:[{_ref:'property',_property:name},...ref],_options:{dialogOptions:'silent'}});
  const fields=['layerID','name','layerKind','layerSection','parentLayerID','itemIndex','hasVectorMask'];
  function type(row) {
    if(row.layerSection?._value==='layerSectionStart')return 'group';
    if(row.layerKind===4||row.layerKind===11)return row.hasVectorMask===true?'shape':'fill';
    return ({1:'pixel',2:'adjustment',3:'text',5:'smart-object',9:'fill',10:'fill'})[row.layerKind] || 'other';
  }
  async function batch(commands,checkpoint) {
    if(checkpoint)await checkpoint();
    const result=await ps.action.batchPlay(commands,{});
    if(checkpoint)await checkpoint();
    if(!Array.isArray(result)||result.length!==commands.length||result.some(r=>!r||r._obj==='error'))throw Error('图层读取失败，请重新搜索');
    return result;
  }
  const docRef=id=>[{_ref:'document',_id:id}];
  async function header(ref,checkpoint) {
    const rows=await batch(['documentID','title','numberOfLayers','hasBackgroundLayer'].map(key=>property(key,ref)),checkpoint);
    const id=rows[0].documentID,title=rows[1].title,count=rows[2].numberOfLayers,bg=rows[3].hasBackgroundLayer;
    if(!Number.isSafeInteger(id)||id<=0||typeof title!=='string'||title.length>2000||!Number.isSafeInteger(count)||count<0||count>100000||typeof bg!=='boolean')throw Error('文档读取不完整，请重新搜索');
    return {id,title,count,bg};
  }
  function validRow(row) {
    return Number.isSafeInteger(row?.layerID)&&row.layerID>0&&typeof row.name==='string'&&row.name.length<=2000&&Number.isSafeInteger(row.parentLayerID)&&Number.isInteger(row.layerKind)&&['layerSectionContent','layerSectionStart'].includes(row.layerSection?._value);
  }
  async function search(query,checkpoint) {
    if(!query||typeof query.name!=='string'||query.name.length>200||typeof query.document!=='string'||query.document.length>200||!types.includes(query.type))throw Error('无效的搜索条件');
    const count=await nativeDocumentCount(ps,checkpoint);
    if(count>64)throw Error('打开的文档超过64个，请先关闭不需要的文档');
    const matches=[],documents=[],seen=new Set(),needle=query.name.toLowerCase(),file=query.document.toLowerCase();
    let scanned=0,truncated=false;
    for(let index=1;index<=count;index++) {
      const doc=await header([{_ref:'document',_index:index}],checkpoint);
      if(seen.has(doc.id))throw Error('打开的文档已改变，请重新搜索');
      seen.add(doc.id);documents.push(doc.id);
      if(!doc.title.toLowerCase().includes(file))continue;
      const total=doc.count+(doc.bg?1:0),ids=new Set();
      for(let offset=0;offset<total;offset+=128) {
        if(scanned>=20000){truncated=true;break;}
        const size=Math.min(128,total-offset,20000-scanned);
        const [value]=await batch([{_obj:'multiGet',_target:{_ref:docRef(doc.id)},extendedReference:[fields,{_obj:'layer',index:(doc.bg?0:1)+offset,count:size}],options:{failOnMissingProperty:false,failOnMissingElement:false},_options:{dialogOptions:'silent'}}],checkpoint);
        if(!Array.isArray(value.list)||value.list.length!==size)throw Error('图层列表已改变，请重新搜索');
        scanned+=size;
        for(const row of value.list) {
          if(row.layerSection?._value==='layerSectionEnd')continue;
          if(!validRow(row)||ids.has(row.layerID))throw Error('图层列表不完整，请重新搜索');
          ids.add(row.layerID);
          const kind=type(row);
          if((query.type==='all'||kind===query.type)&&row.name.toLowerCase().includes(needle)) {
            matches.push({document_id:doc.id,document_name:doc.title,layer_id:row.layerID,name:row.name,type:kind,layer_kind:row.layerKind,parent_id:row.parentLayerID});
            if(matches.length===500){truncated=true;break;}
          }
        }
        if(truncated)break;
      }
      const after=await header(docRef(doc.id),checkpoint);
      if(JSON.stringify(after)!==JSON.stringify(doc))throw Error('文档已改变，请重新搜索');
      if(truncated)break;
    }
    if(await nativeDocumentCount(ps,checkpoint)!==count)throw Error('打开的文档已改变，请重新搜索');
    return {session,items:matches,scanned,truncated,document_count:count};
  }
  async function focus(request) {
    const expected=request.target,row=request.destination;
    if(expected?.session!==session||!row||!Number.isSafeInteger(row.document_id)||row.document_id<=0||!Number.isSafeInteger(row.layer_id)||row.layer_id<=0||typeof row.name!=='string'||row.name.length>2000||typeof row.document_name!=='string'||row.document_name.length>2000||!types.includes(row.type)||!Number.isSafeInteger(row.parent_id)||!Number.isInteger(row.layer_kind))throw Error('搜索结果无效，请重新搜索');
    let uncertain=false;
    try {return await ps.core.executeAsModal(async context=>{
      const original=await capture();
      const sameTarget=(a,b)=>a.session===b.session&&a.document_id===b.document_id&&a.active_layer_id===b.active_layer_id&&a.layer_ids.join(',')===b.layer_ids.join(',');
      if(context.isCancelled||Date.now()>request.expires_at_ms||!sameTarget(original,expected))throw Error('当前选择已改变，定位已取消');
      const doc=await header(docRef(row.document_id));
      const [value]=await batch([{_obj:'multiGet',_target:{_ref:[{_ref:'layer',_id:row.layer_id},...docRef(row.document_id)]},extendedReference:[fields],options:{failOnMissingProperty:false,failOnMissingElement:false},_options:{dialogOptions:'silent'}}]);
      const actual=value;
      if(doc.title!==row.document_name||!validRow(actual)||actual.layerID!==row.layer_id||actual.name!==row.name||actual.layerKind!==row.layer_kind||actual.parentLayerID!==row.parent_id||type(actual)!==row.type)throw Error('搜索结果已改变，请重新搜索');
      if(context.isCancelled||Date.now()>request.expires_at_ms)throw Error('定位已取消');
      const chooseDoc=id=>batch([{_obj:'select',_target:docRef(id),_options:{dialogOptions:'silent'}}]);
      const chooseLayer=(id,add=false)=>batch([{_obj:'select',_target:[{_ref:'layer',_id:id}],makeVisible:false,...(add?{selectionModifier:{_enum:'selectionModifierType',_value:'addToSelection'}}:{}),_options:{dialogOptions:'silent'}}]);
      try {
        if(original.document_id!==row.document_id)await chooseDoc(row.document_id);
        await chooseLayer(row.layer_id);
        const selected=await capture();
        if(selected.document_id!==row.document_id||selected.layer_ids.join(',')!==String(row.layer_id)||selected.active_layer_id!==row.layer_id)throw Error('定位结果未确认');
        return {verified:true,count:1,destination:row};
      } catch(error) {
        try {
          await chooseDoc(original.document_id);
          const order=original.layer_ids.filter(id=>id!==original.active_layer_id).concat(original.active_layer_id);
          for(let i=0;i<order.length;i++)await chooseLayer(order[i],i>0);
          if(!sameTarget(await capture(),original))throw Error('Selection restore mismatch');
        } catch(_) {uncertain=true;}
        throw error;
      }
    },{commandName:'定位图层',timeOut:0});}
    catch(error){if(uncertain){const unknown=Error('定位恢复结果待核对');unknown.resultUnknown=true;throw unknown;}throw error;}
  }
  async function preview(row,checkpoint,verifyOnly=false) {
    if(!row||!Number.isSafeInteger(row.document_id)||row.document_id<=0||!Number.isSafeInteger(row.layer_id)||row.layer_id<=0||typeof row.name!=='string'||row.name.length>2000||typeof row.document_name!=='string'||row.document_name.length>2000||!types.includes(row.type)||!Number.isSafeInteger(row.parent_id)||!Number.isInteger(row.layer_kind))throw Error('预览目标无效，请重新搜索');
    if(!verifyOnly&&typeof ps.imaging?.getPixels!=='function')return {available:false,reason:'当前PS不支持图层像素预览'};
    async function verify() {
      const doc=await header(docRef(row.document_id),checkpoint);
      const [actual]=await batch([{_obj:'multiGet',_target:{_ref:[{_ref:'layer',_id:row.layer_id},...docRef(row.document_id)]},extendedReference:[fields],options:{failOnMissingProperty:false,failOnMissingElement:false},_options:{dialogOptions:'silent'}}],checkpoint);
      if(doc.title!==row.document_name||!validRow(actual)||actual.layerID!==row.layer_id||actual.name!==row.name||actual.layerKind!==row.layer_kind||actual.parentLayerID!==row.parent_id||type(actual)!==row.type)throw Error('预览目标已改变，请重新搜索');
    }
    if(verifyOnly){await verify();return;}
    let image;
    try {
      await verify();
      const [mode]=await batch([property('mode',docRef(row.document_id))],checkpoint);
      if(mode.mode?._value!=='RGBColor')return {available:false,reason:'当前仅支持RGB图层预览'};
      if(row.type==='group' || row.type==='adjustment')return {available:false,fallback:true,reason:'该图层类型不支持静默像素预览'};
      const [area]=await batch([{_obj:'get',_target:[{_ref:'property',_property:'boundsNoEffects'},{_ref:'layer',_id:row.layer_id},...docRef(row.document_id)],_options:{dialogOptions:'silent'}}],checkpoint);
      const bounds=area.boundsNoEffects,number=v=>typeof v==='number'?v:v?._value;
      const sourceWidth=number(bounds?.right)-number(bounds?.left),sourceHeight=number(bounds?.bottom)-number(bounds?.top);
      if(!Number.isFinite(sourceWidth)||!Number.isFinite(sourceHeight)||sourceWidth<=0||sourceHeight<=0)return {available:false,fallback:true,reason:'目标缺少直接像素边界'};
      const targetSize=sourceWidth>=sourceHeight?{width:Math.min(128,Math.ceil(sourceWidth))}:{height:Math.min(128,Math.ceil(sourceHeight))};
      try {
        image=await ps.imaging.getPixels({documentID:row.document_id,layerID:row.layer_id,targetSize,componentSize:8,colorSpace:'RGB',colorProfile:'sRGB IEC61966-2.1',applyAlpha:false});
      } catch(error){return {available:false,fallback:true,reason:'直接像素读取不可用：'+String(error.message||error).slice(0,120)};}
      if(checkpoint)await checkpoint();
      const data=image?.imageData,width=data?.width,height=data?.height,components=data?.components;
      if(!data||width===0||height===0)return {available:false,reason:'图层没有可显示的像素'};
      if(!Number.isInteger(width)||!Number.isInteger(height)||width<1||height<1||width>512||height>512||width*height>65536||![3,4].includes(components)||data.componentSize!==8)return {available:false,fallback:true,reason:'直接预览像素格式不支持'};
      let raw;
      try {raw=await data.getData({chunky:true});}
      catch(error){return {available:false,fallback:true,reason:'直接像素解码不可用：'+String(error.message||error).slice(0,120)};}
      if(raw.length!==width*height*components)throw Error('预览像素不完整');
      const factor=Math.min(1,128/Math.max(width,height)),w=Math.max(1,Math.round(width*factor)),h=Math.max(1,Math.round(height*factor)),pixels=[];
      for(let y=0;y<h;y++)for(let x=0;x<w;x++){
        const i=(Math.min(height-1,Math.floor(y/factor))*width+Math.min(width-1,Math.floor(x/factor)))*components;
        pixels.push(raw[i],raw[i+1],raw[i+2],components===4?raw[i+3]:255);
      }
      await verify();
      return {available:true,width:w,height:h,pixels};
    } finally {image?.imageData?.dispose();}
  }
  return {search,focus,preview};
}
function createTextTools(ps,session,capture,readTree) {
  let snapshot=null,serial=0;
  const clone=value=>JSON.parse(JSON.stringify(value));
  const stable=value=>JSON.stringify(value,(key,item)=>item&&!Array.isArray(item)&&typeof item==='object'?Object.fromEntries(Object.keys(item).sort().map(key=>[key,item[key]])):item);
  const same=(a,b)=>stable(a)===stable(b);
  const sameTarget=(a,b)=>a?.session===b?.session&&a.document_id===b.document_id&&a.active_layer_id===b.active_layer_id&&a.layer_ids.join(',')===b.layer_ids.join(',');
  async function batch(commands) {
    const rows=await ps.action.batchPlay(commands,{});
    if(!Array.isArray(rows)||rows.length!==commands.length)throw Error('文本读取或写入结果不完整');
    const failure=rows.find(row=>!row||row._obj==='error');
    if(rows.some(row=>!row||row._obj==='error'))throw Error((commands[0]._obj==='set'?'文本写入失败：':'文本读取失败：')+(failure?.message||'Photoshop rejected command')+' ('+failure?.result+')');
    return rows;
  }
  const ref=target=>[{_ref:'layer',_id:target.active_layer_id},{_ref:'document',_id:target.document_id}];
  async function descriptor(target,objects=false) {
    if(target?.session!==session||target.layer_ids.length!==1)throw Error('请单选一个文字图层');
    const [row,metadata]=await batch([{_obj:'multiGet',_target:{_ref:ref(target)},extendedReference:[['layerID','name','layerKind','layerLocking','textKey']],options:{failOnMissingProperty:false,failOnMissingElement:false},_options:{dialogOptions:'silent'}},
      {_obj:'get',_target:[{_ref:'property',_property:'XMPMetadataAsUTF8'},...ref(target)],_options:{dialogOptions:'silent'}}]);
    if(metadata.XMPMetadataAsUTF8!==undefined&&typeof metadata.XMPMetadataAsUTF8!=='string')throw Error('图层元数据读取结果无效');
    row.XMPMetadataAsUTF8=metadata.XMPMetadataAsUTF8||'';
    if(row.layerID!==target.active_layer_id||![3,...(objects?[5]:[])].includes(row.layerKind))throw Error('请单选一个文字图层或本工具的文字对象');
    if(row.layerLocking&&Object.entries(row.layerLocking).some(([key,value])=>key!=='_obj'&&value===true))throw Error('请先解锁文字图层');
    if(row.layerKind===5){if(row.XMPMetadataAsUTF8.length>2097152)throw Error('文字对象源超出上限');const [more]=await batch([{_obj:'get',_target:[{_ref:'property',_property:'smartObjectMore'},...ref(target)],_options:{dialogOptions:'silent'}}]);row.smartObjectMore=more.smartObjectMore;if(!sameTarget(await capture(),target))throw Error('文字对象目标已改变');return clone(row);}
    if(!row.textKey||typeof row.textKey.textKey!=='string')throw Error('请单选一个文字图层');
    if(row.textKey.textKey.length>20000||JSON.stringify(row.textKey).length>524288)throw Error('文本或样式超出本次处理上限');
    if(row.textKey.kerningRange?.length)throw Error('含手动字距的文本暂不支持替换');
    if(!sameTarget(await capture(),target))throw Error('文字目标已改变，请重新读取');
    const result=clone(row);
    for(const key of ['textStyleRange','paragraphStyleRange'])result.textKey[key]=styleRuns(result.textKey,key);
    return result;
  }
  function styleRuns(raw,key) {
    const ranges=raw[key];let end=0;
    if(!Array.isArray(ranges)||!ranges.length||ranges.length>1024)throw Error('文本样式范围不完整');
    const field=key==='textStyleRange'?'textStyle':'paragraphStyle',base=styleSignature(ranges[0]?.[field]?.baseParentStyle),normalized=[];
    for(const range of ranges) {
      if(!range[field]||typeof range[field]!=='object')throw Error('文本样式范围不完整');
      if(key==='textStyleRange'&&(range.textStyle?.fontAvailable===false||range.textStyle?.baseParentStyle?.fontAvailable===false))throw Error('文本包含缺失字体，请先在PS中处理');
      const previous=normalized[normalized.length-1];
      // PS may repeat the same complete style range after reopening a PSD.
      if(previous&&range.from===previous.from&&range.to===previous.to&&range[field].styleSheetHasParent===previous[field].styleSheetHasParent&&same({...base,...styleSignature(previous[field])},{...base,...styleSignature(range[field])}))continue;
      if(!Number.isInteger(range.from)||!Number.isInteger(range.to)||range.from!==end||range.to<=end||range.to>raw.textKey.length+1)throw Error('文本样式范围不完整');
      end=range.to;normalized.push(range);
    }
    if(end<raw.textKey.length)throw Error('文本样式范围不完整');
    return normalized;
  }
  function plan(raw,params) {
    if(params&&Object.prototype.hasOwnProperty.call(params,'value')) {
      const value=params.value,origins=params.origins;
      if(typeof value!=='string'||!value.length||value.length>20000||/[\n\0]/.test(value)||!Number.isInteger(params.count)||params.count<0||params.count>40001||
          !Array.isArray(origins)||origins.length!==value.length+1||origins.some((origin,index)=>!Number.isInteger(origin)||origin<0||origin>raw.textKey.length||index>0&&origin<origins[index-1]))throw Error('文字预览计划无效');
      for(const char of value)if(char.length===1&&char.charCodeAt(0)>=0xd800&&char.charCodeAt(0)<=0xdfff)throw Error('文字包含不完整的Unicode字符');
      return {value:mappedText(raw,value,origins),count:params.count,origins};
    }
    if(!params||typeof params.find!=='string'||!params.find||params.find.length>200||typeof params.replacement!=='string'||params.replacement.length>500)throw Error('查找内容不能为空，查找／替换内容超出上限');
    const find=params.find.replace(/\r\n|\n/g,'\r'),replacement=params.replacement.replace(/\r\n|\n/g,'\r');
    for(const char of find+replacement)if(char.length===1&&char.charCodeAt(0)>=0xd800&&char.charCodeAt(0)<=0xdfff)throw Error('文字包含不完整的Unicode字符');
    const text=raw.textKey,origins=[],parts=[];let cursor=0,count=0,hit,lastHit=-1;
    function append(from,to){parts.push(text.slice(from,to));for(let i=from;i<to;i++)origins.push(i);}
    while((hit=text.indexOf(find,cursor))!==-1) {
      append(cursor,hit);parts.push(replacement);for(let i=0;i<replacement.length;i++)origins.push(hit);
      cursor=hit+find.length;lastHit=hit;count++;
      if(origins.length>20000)throw Error('替换后的文本超过20000字符');
    }
    append(cursor,text.length);origins.push(cursor<text.length?text.length:(lastHit>=0?lastHit:text.length));
    const value=parts.join('');
    if(!value.length||value.length>20000)throw Error('替换后的文本为空或超过20000字符');
    return {value:mappedText(raw,value,origins),count,origins};
  }
  function mappedText(raw,value,origins) {
    const result={_obj:'textLayer',textKey:value};
    for(const key of ['antiAlias','orientation','textGridding'])if(raw[key]!==undefined)result[key]=clone(raw[key]);
    for(const key of ['textStyleRange','paragraphStyleRange']) {
      const ranges=styleRuns(raw,key),mapped=[];let index=0,previous=-1;
      // PS omits the shared parent on later runs; independent text needs those defaults explicitly.
      const field=key==='textStyleRange'?'textStyle':'paragraphStyle',base=ranges[0][field].baseParentStyle||{};
      const copy=source=>({...clone(source),[field]:{...clone(base),...clone(source[field])}});
      if(key==='paragraphStyleRange') {
        let from=0;
        while(from<=value.length) {
          while(index<ranges.length-1&&origins[from]>=ranges[index].to)index++;
          const newline=value.indexOf('\r',from),to=newline<0?value.length+1:newline+1;
          mapped.push({...copy(ranges[index]),from,to});from=to;
        }
        result[key]=mapped;continue;
      }
      for(let offset=0;offset<origins.length;offset++) {
        while(index<ranges.length-1&&origins[offset]>=ranges[index].to)index++;
        const source=ranges[index],prior=mapped[mapped.length-1];
        if(prior&&previous===index)prior.to=offset+1;
        else mapped.push({...copy(source),from:offset,to:offset+1});
        previous=index;
      }
      result[key]=mapped;
    }
    return result;
  }
  function styleSignature(style) {
    if(!style||typeof style!=='object')return style;
    const value={...(style.baseParentStyle?styleSignature(style.baseParentStyle):{})};
    for(const [key,item] of Object.entries(style))if(!['_obj','baseParentStyle','styleSheetHasParent','fontAvailable','fontName','fontStyleName'].includes(key)&&!key.startsWith('implied'))value[key]=item;
    return value;
  }
  function pixels(value,resolution) {
    if(!Number.isFinite(value?._value))return NaN;
    return value._unit==='pixelsUnit'?value._value:value._unit==='pointsUnit'&&Number.isFinite(resolution)&&resolution>0?value._value*resolution/72:NaN;
  }
  function stylesMatch(actual,wanted,resolution,original,origins,listFormatting=false) {
    for(const [key,field] of [['textStyleRange','textStyle'],['paragraphStyleRange','paragraphStyle']]) {
      const a=styleRuns(actual,key),b=styleRuns(wanted,key);let ai=0,bi=0;
      const signatures=ranges=>ranges.map(row=>{
        const style={...styleSignature(ranges[0][field].baseParentStyle),...styleSignature(row[field])},baseline=style.baselineShift;
        if(baseline?._unit==='pointsUnit'&&resolution===undefined)resolution=ps.app.activeDocument.resolution;
        const offset=baseline===undefined||baseline===0?0:pixels(baseline,resolution);
        if(field==='textStyle'&&Number.isFinite(offset))delete style.baselineShift;
        if(field==='paragraphStyle')for(const key of ['startIndent','firstLineIndent'])if(style[key]!==undefined){if(resolution===undefined)resolution=ps.app.activeDocument.resolution;style[key]=style[key]===0?0:pixels(style[key],resolution);}
        const fontless={...style};for(const key of ['fontPostScriptName','fontScript','fontTechnology'])delete fontless[key];
        return {style:stable(style),fontless:stable(fontless),font:style.fontPostScriptName,
          available:row[field].fontAvailable===true||row[field].baseParentStyle?.fontAvailable===true,offset};
      });
      const signaturesA=signatures(a),signaturesB=signatures(b);
      for(let offset=0;offset<=wanted.textKey.length;offset++) {
        while(ai<a.length-1&&offset>=a[ai].to)ai++;
        while(bi<b.length-1&&offset>=b[bi].to)bi++;
        const left=signaturesA[ai],right=signaturesB[bi];
        // Compare the same physical shift across descriptor unit conversions.
        const inserted=original&&origins&&offset<wanted.textKey.length&&wanted.textKey.codePointAt(offset)!==original.textKey.codePointAt(origins[offset]);
        // PS chooses a fallback for a new glyph absent from the requested font.
        const forcedSymbol=listFormatting?.marker_font==='SegoeUISymbol'&&right.font==='SegoeUISymbol';
        const fallback=field==='textStyle'&&!forcedSymbol&&inserted&&left.available&&typeof left.font==='string'&&left.font!==right.font&&left.fontless===right.fontless;
        const paragraphDefaults=field==='paragraphStyle'&&listFormatting&&Object.entries(JSON.parse(right.style)).every(([key,value])=>{
          const actual=JSON.parse(left.style)[key];return ['startIndent','firstLineIndent'].includes(key)&&Number.isFinite(actual)&&Number.isFinite(value)?Math.abs(actual-value)<.001:same(actual,value);
        });
        if(left.style!==right.style&&!fallback&&!paragraphDefaults || Number.isFinite(left.offset)&&Number.isFinite(right.offset)&&Math.abs(left.offset-right.offset)>.0001)return false;
      }
    }
    return true;
  }
  function textWriter(raw) {
    const ranges=styleRuns(raw,'paragraphStyleRange');
    const uniform=ranges.every(range=>same(range.paragraphStyle,ranges[0].paragraphStyle));
    return value=>{
      const to={...value};
      // Rewriting identical inherited paragraphs materializes PS defaults and changes their representation.
      if(uniform&&value.paragraphStyleRange?.every(range=>same(styleSignature(range.paragraphStyle),styleSignature(ranges[0].paragraphStyle))))delete to.paragraphStyleRange;
      return batch([{_obj:'set',_target:[{_ref:'textLayer',_enum:'ordinal',_value:'targetEnum'}],to,_options:{dialogOptions:'silent'}}]);
    };
  }
  function layout(raw) {
    return {antiAlias:raw.antiAlias,textGridding:raw.textGridding,orientation:raw.orientation,textClickPoint:raw.textClickPoint,transform:raw.transform,warp:raw.warp,
      textShape:raw.textShape?.map(shape=>({shapeType:shape.shapeType,orientation:shape.orientation,transform:shape.transform,
        ...(shape.char?._value==='box'?{bounds:shape.bounds}:{}),char:shape.char}))};
  }
  function paragraphLayout(raw) {
    const resolution=ps.app.activeDocument.resolution;
    const points=value=>value===undefined||value===0?0:pixels(value,resolution)*72/resolution;
    return styleRuns(raw,'paragraphStyleRange').map(range=>{const style=styleSignature(range.paragraphStyle);return {from:range.from,to:range.to,start:points(style.startIndent),first:points(style.firstLineIndent)};});
  }
  async function formatList(raw,wanted,params,target,context) {
    const rows=params?.paragraphs,resolution=ps.app.activeDocument.resolution;
    if(typeof params?.enabled!=='boolean'||!Array.isArray(rows)||!rows.length||rows.length>1024||rows.length!==wanted.paragraphStyleRange.length||
        rows.some((row,i)=>!Number.isInteger(row.from)||!Number.isInteger(row.to)||row.from!==wanted.paragraphStyleRange[i].from||row.to!==wanted.paragraphStyleRange[i].to||!Number.isInteger(row.prefix)||row.prefix<row.from||row.prefix>=row.to||['start','first'].some(k=>!Number.isFinite(row[k])||Math.abs(row[k])>1296)))throw Error('列表排版范围无效');
    if(raw.orientation?._value!=='horizontal'||raw.textShape?.length!==1||!['paint','box'].includes(raw.textShape[0].char?._value)||raw.warp?.warpStyle?._value!=='warpNone')throw Error('列表排版目前需要水平普通文字');
    if(wanted.paragraphStyleRange.some(range=>{const align=styleSignature(range.paragraphStyle).align;return align!==undefined&&align._value!=='left';}))throw Error('列表排版目前需要左对齐段落，请先在PS中调整');
    if(params.marker_font!==undefined) {
      if(!params.enabled||params.marker_font!=='SegoeUISymbol')throw Error('列表符号字体参数无效');
      let font;try{font=ps.app.fonts.getByName(params.marker_font);}catch(_){throw Error('PS未识别Segoe UI Symbol，请确认系统符号字体可用');}
      if(font?.postScriptName!==params.marker_font)throw Error('列表符号字体身份不一致');
      for(const row of rows) {
        const prefix=wanted.textKey.slice(row.from,row.prefix),symbol=prefix.trim();
        if(Array.from(symbol).length!==1||symbol.codePointAt(0)<=127)continue;
        const from=row.from+prefix.search(/\S/),to=from+symbol.length;
        wanted.textStyleRange=wanted.textStyleRange.flatMap(range=>{
          if(range.to<=from||range.from>=to)return [range];
          const style={...styleSignature(range.textStyle),_obj:'textStyle',fontPostScriptName:font.postScriptName,fontName:font.family,fontStyleName:font.style,fontScript:0,fontTechnology:1};
          return [...(range.from<from?[{...clone(range),to:from}]:[]),{...clone(range),from:Math.max(from,range.from),to:Math.min(to,range.to),textStyle:style},...(range.to>to?[{...clone(range),from:to}]:[])];
        });
      }
      if(wanted.textStyleRange.length>1024)throw Error('列表符号与正文样式超过范围上限');
    }
    let probe=null;
    try {
      const cache=new Map(),advances=[],columns=new Map();
      for(let index=0;index<rows.length;index++) {
        if(context.isCancelled)throw Error('列表排版已取消');
        const row=rows[index],paragraph=wanted.paragraphStyleRange[index];let advance=0;
        if(params.enabled&&row.prefix>row.from) {
          const prefix=wanted.textKey.slice(row.from,row.prefix);
          if(!prefix.endsWith('\t'))throw Error('列表正文起点缺少制表符');
          const at=Math.min(row.prefix,wanted.textKey.length-1),single=mappedText(wanted,'M',[at,at]);
          const text=prefix+'M',origins=Array.from({length:text.length+1},(_,i)=>Math.min(row.from+i,at)),full=mappedText(wanted,text,origins);
          const key=stable([prefix,full.textStyleRange,single.textStyleRange]);
          if(cache.has(key))advance=cache.get(key);
          else {
            if(probe===null) {
              const seed={...clone(raw),...single,textShape:[{...clone(raw.textShape[0]),char:{_enum:'char',_value:'paint'}}]};
              delete seed.textShape[0].bounds;
              const [made]=await batch([{_obj:'make',_target:[{_ref:'textLayer'}],using:seed,_options:{dialogOptions:'silent'}}]);
              const selected=await capture();
              if(selected.document_id!==target.document_id||selected.layer_ids.length!==1||selected.active_layer_id===target.active_layer_id||made.layerID!==selected.active_layer_id)throw Error('列表测量图层身份无效');
              probe=selected.active_layer_id;
              await batch([{_obj:'set',_target:[{_ref:'layer',_id:probe}],to:{_obj:'layer',opacity:{_unit:'percentUnit',_value:0}},_options:{dialogOptions:'silent'}}]);
            }
            const write=value=>({_obj:'set',_target:[{_ref:'textLayer',_enum:'ordinal',_value:'targetEnum'}],to:{...value,paragraphStyleRange:[{from:0,to:value.textKey.length+1,paragraphStyle:{_obj:'paragraphStyle',align:{_enum:'alignmentType',_value:'left'},startIndent:{_unit:'pointsUnit',_value:0},firstLineIndent:{_unit:'pointsUnit',_value:0}}}]},_options:{dialogOptions:'silent'}});
            const read={_obj:'get',_target:[{_ref:'property',_property:'textKey'},{_ref:'layer',_id:probe}],_options:{dialogOptions:'silent'}};
            const measured=await batch([write(full),read,write(single),read]);
            advance=(pixels(measured[1].textKey?.boundingBox?.right,resolution)-pixels(measured[3].textKey?.boundingBox?.right,resolution))*72/resolution;
            if(!Number.isFinite(advance)||advance<=0||advance>1296)throw Error('无法核对列表符号宽度');
            cache.set(key,advance);
          }
        }
        const level=(wanted.textKey.slice(row.from,row.prefix).match(/^ */)?.[0].length||0)/4;
        advances.push({advance,level});columns.set(level,Math.max(columns.get(level)||0,advance));
      }
      for(let index=0;index<rows.length;index++) {
        const row=rows[index],paragraph=wanted.paragraphStyleRange[index],{advance,level}=advances[index];
        // Shorter numbers move right so every body in this level shares a column.
        const start=advance?row.start+row.first+columns.get(level):row.start,first=advance?-advance:row.first;
        if(Math.abs(start)>1296||Math.abs(first)>1296)throw Error('列表缩进超出PS范围');
        // Readback's implied values override edits if sent back to Photoshop.
        const style=styleSignature(paragraph.paragraphStyle);
        paragraph.paragraphStyle={...style,_obj:'paragraphStyle',...(advance?{align:{_enum:'alignmentType',_value:'left'}}:{}),startIndent:{_unit:'pointsUnit',_value:start},firstLineIndent:{_unit:'pointsUnit',_value:first}};
      }
    } finally {
      if(probe!==null)await batch([{_obj:'delete',_target:[{_ref:'layer',_id:probe}],_options:{dialogOptions:'silent'}},{_obj:'select',_target:[{_ref:'layer',_id:target.active_layer_id}],makeVisible:false,_options:{dialogOptions:'silent'}}]);
    }
  }
  async function read() {
    snapshot=null;
    const target=await capture(),row=await descriptor(target,true);
    const revision=session+':text:'+String(++serial);
    snapshot={target,row,revision,time:Date.now()};
    if(row.layerKind===5)return {target,revision,name:row.name,object_xmp:row.XMPMetadataAsUTF8};
    return {target,revision,text:row.textKey.textKey,name:row.name,list_xmp:row.XMPMetadataAsUTF8,list_paragraphs:paragraphLayout(row.textKey)};
  }
  async function finishEditing(request,saved) {
    const tool=await ps.core.getActiveTool(),kind=tool?.classID??tool?.classId;
    if(tool?.isModal===false)return;
    if(tool?.isModal!==true||!['typeCreateOrEditTool','typeVerticalCreateOrEditTool'].includes(kind))throw Error('请先结束当前PS工具操作，再写回文字');
    if(Date.now()>request.expires_at_ms)throw Error('文本操作已取消');
    if(!same(await descriptor(request.target),saved.row))throw Error('文字内容或样式已改变，请重新读取');
    const current=await ps.core.getActiveTool();
    if(Date.now()>request.expires_at_ms)throw Error('文本操作已取消');
    if(current?.isModal!==true||(current.classID??current.classId)!==kind)throw Error('PS工具编辑状态已改变，本次未写回');
    // Commit the user's native edit before requesting our own modal scope.
    try {
      await ps.core.endModalToolState(true);
      if((await ps.core.getActiveTool())?.isModal!==false||!sameTarget(await capture(),request.target))throw Error('文字编辑退出或目标待核对');
    } catch(error) {error.resultUnknown=true;throw error;}
    if(!same(await descriptor(request.target),saved.row))throw Error('PS文字编辑已确认，最新内容已保留；请重新读取后写回');
  }
  async function prepareObject(request,params){
    const saved=snapshot;
    if(!saved||params?.revision!==saved.revision||!sameTarget(saved.target,request.target)||Date.now()-saved.time>1800000)throw Error('文字对象读取已过期');
    await finishEditing(request,saved);
    const row=await descriptor(request.target,true);if(!same(row,saved.row))throw Error('文字或对象已改变，请重新读取');
    const existing=row.layerKind===5,raw=existing?params.object_source?.layer?.text:row.textKey;
    if(!raw||existing&&(!params.object_source||!/^[0-9a-f]{64}$/.test(params.object_render)))throw Error('文字对象源或显示校验缺失');
    const wanted=plan(raw,params).value,doc=ps.app.activeDocument;
    const canvas=existing?params.object_source.canvas:{width:doc.width,height:doc.height,resolution:doc.resolution,mode:doc.mode===ps.constants.DocumentMode.RGB?'RGB':'unsupported',depth:doc.bitsPerChannel===ps.constants.BitsPerChannelType.EIGHT?8:0,profile:doc.colorProfileName};
    if(canvas.mode!=='RGB'||canvas.depth!==8)throw Error('矢量文字对象目前仅支持RGB8源');
    const legacy=existing&&params.object_source.schema===2;
    const originalXmp=existing?params.object_source.layer.xmp:row.XMPMetadataAsUTF8;
    if(!legacy&&Object.prototype.hasOwnProperty.call(params,'list_xmp')&&params.list_xmp!==originalXmp)throw Error('请先写回普通文字列表，再生成矢量文字对象；本次未修改');
    return {kind:existing?'smart-object':'text',row,before_xmp:row.XMPMetadataAsUTF8,...(!existing?{before_text:row.textKey}:{transform:row.smartObjectMore?.transform,before_render:params.object_render}),
      source:{...(legacy?{schema:2,renderer:'list-v1'}:{schema:1}),canvas,layer:{name:row.name,text:{...raw,...wanted},xmp:Object.prototype.hasOwnProperty.call(params,'list_xmp')?params.list_xmp:originalXmp}}};
  }
  async function finishObject(request,prepared,native,expected){
    if(native.phase!=='completed'){
      let unknown=native.result_unknown===true;
      if(!unknown)try{unknown=!same(await descriptor(request.target,true),prepared.row);}catch(_){unknown=true;}
      const error=Error(native.error||'文字对象执行失败');error.resultUnknown=unknown;throw error;
    }
    const target=await capture();
    if(target.document_id!==request.target.document_id||target.layer_ids.length!==1||target.active_layer_id!==native.result?.layer_id){const e=Error('文字对象目标结果待核对');e.resultUnknown=true;throw e;}
    const row=await descriptor(target,true);
    if(row.layerKind!==5||row.XMPMetadataAsUTF8!==expected.xmp){const e=Error('文字对象源读回不一致');e.resultUnknown=true;throw e;}
    snapshot=null;return native.result;
  }
  async function replace(request) {
    const saved=snapshot,params=request.text;
    if(!saved||params?.revision!==saved.revision||!sameTarget(saved.target,request.target)||Date.now()-saved.time>(Object.prototype.hasOwnProperty.call(params || {},'value')?1800000:60000))throw Error('文本预览已过期，请重新读取');
    const wanted=plan(saved.row.textKey,params);
    const tagging=Object.prototype.hasOwnProperty.call(params,'list_xmp');
    if(tagging&&(typeof params.list_xmp!=='string'||params.list_xmp.length>131072||params.list_xmp.includes('\0')))throw Error('列表标记数据无效');
    if(wanted.value.textKey===saved.row.textKey.textKey&&!params.list_layout&&(!tagging||params.list_xmp===(saved.row.XMPMetadataAsUTF8||'')))return {verified:true,count:0};
    await finishEditing(request,saved);
    let uncertain=false;
    try {return await ps.core.executeAsModal(async context=>{
      if(context.isCancelled||Date.now()>request.expires_at_ms)throw Error('文本操作已取消');
      const before=await descriptor(request.target);
      if(!same(before,saved.row))throw Error('文字内容或样式已改变，请重新读取');
      const history=await context.hostControl.suspendHistory({documentID:request.target.document_id,name:'处理文字'});
      try {
        if(context.isCancelled||Date.now()>request.expires_at_ms)throw Error('文本操作已取消');
        // PS24.0 rejects a document-qualified textLayer identifier. The modal
        // fresh capture above binds the active text reference to this exact ID.
        if(params.list_layout){if(!tagging)throw Error('列表排版缺少持久标记');await formatList(before.textKey,wanted.value,params.list_layout,request.target,context);}
        await textWriter(before.textKey)(wanted.value);
        if(tagging&&params.list_xmp!==(before.XMPMetadataAsUTF8||''))await batch([{_obj:'set',_target:[{_ref:'property',_property:'XMPMetadataAsUTF8'},...ref(request.target)],to:{_obj:'layer',XMPMetadataAsUTF8:params.list_xmp},_options:{dialogOptions:'silent'}}]);
        const after=await descriptor(request.target);
        if(after.textKey.textKey!==wanted.value.textKey||!stylesMatch(after.textKey,wanted.value,undefined,before.textKey,wanted.origins,params.list_layout)||!same(layout(after.textKey),layout(before.textKey)))throw Error('文本或样式读回不一致');
        if((after.XMPMetadataAsUTF8||'')!==(tagging?params.list_xmp:(before.XMPMetadataAsUTF8||'')))throw Error('列表标记读回不一致');
        await context.hostControl.resumeHistory(history,true);snapshot=null;
        return {verified:true,count:wanted.count};
      } catch(error) {
        try {
          await context.hostControl.resumeHistory(history,false);
          if(!same(await descriptor(request.target),before))throw Error('Text restore mismatch');
        } catch(_) {uncertain=true;}
        throw error;
      }
    },{commandName:'处理文字',timeOut:0});}
    catch(error){if(uncertain){const unknown=Error('文字恢复结果待核对');unknown.resultUnknown=true;throw unknown;}throw error;}
  }
  async function split(request) {
    const saved=snapshot,params=request.text,ends=params?.breaks;let raw=saved?.row.textKey;
    if(!saved||params?.revision!==saved.revision||!sameTarget(saved.target,request.target)||Date.now()-saved.time>(Object.prototype.hasOwnProperty.call(params || {},'value')?1800000:60000))throw Error('文本预览已过期，请重新读取');
    const prepared=params&&Object.prototype.hasOwnProperty.call(params,'value')?plan(raw,params).value:null;
    if(prepared)raw={...raw,...prepared};
    if(prepared&&!['original','lines'].includes(params.layout))throw Error('文字拆分布局无效');
    const flow=prepared&&params.layout==='lines';
    if(flow&&raw.orientation&&raw.orientation._value!=='horizontal')throw Error('逐行排列暂不支持竖排文字，请选择保留位置');
    if(!Array.isArray(ends)||ends.length<2||ends.length>64||ends[ends.length-1]!==raw.textKey.length||ends.some((end,index)=>!Number.isInteger(end)||end<1||end>raw.textKey.length||(index>0&&end<=ends[index-1])||(end<raw.textKey.length&&raw.textKey[end-1]!=='\r')))throw Error('分组必须按预览行顺序完整覆盖文字，最多64组');
    let from=0;const groups=[];
    for(const end of ends) {
      const to=raw.textKey[end-1]==='\r'?end-1:end,text=raw.textKey.slice(from,to);
      if(/\S/.test(text))groups.push({from,to,text});from=end;
    }
    if(groups.length<2)throw Error('拆分需要至少两组非空文字');
    const shape=raw.textShape?.[0],matrix=raw.transform||{xx:1,xy:0,yx:0,yy:1};
    // shortcut: path/warp/effect text is rejected until its glyph geometry can be verified.
    if(raw.textShape?.length!==1||!['paint','box'].includes(shape?.char?._value)||raw.warp?.warpStyle?._value!=='warpNone'||
        (shape.columnCount??1)!==1||(shape.rowCount??1)!==1||shape.char._value==='box'&&raw.orientation?._value!=='horizontal'||
        shape.transform&&(['xx','yy'].some(key=>shape.transform[key]!==1)||['xy','yx'].some(key=>shape.transform[key]!==0))||
        ['xx','xy','yx','yy'].some(key=>!Number.isFinite(matrix[key]))||Math.abs(matrix.xx*matrix.yy-matrix.xy*matrix.yx)<.000001)throw Error('此文字的路径或变形暂不能安全拆分');
    await finishEditing(request,saved);
    let uncertain=false;
    try {return await ps.core.executeAsModal(async context=>{
      if(context.isCancelled||Date.now()>request.expires_at_ms)throw Error('文本操作已取消');
      const before=await descriptor(request.target);
      if(!same(before,saved.row))throw Error('文字内容或样式已改变，请重新读取');
      const [flags]=await batch([{_obj:'multiGet',_target:{_ref:ref(request.target)},extendedReference:[['visible','group','layerEffects','opacity']],options:{failOnMissingProperty:false,failOnMissingElement:false},_options:{dialogOptions:'silent'}}]);
      if(typeof flags.visible!=='boolean'||typeof flags.group!=='boolean'||flags.group===true||flags.layerEffects&&Object.keys(flags.layerEffects).some(key=>!['_obj','scale'].includes(key)))throw Error('带剪贴或图层效果的文字暂不能安全拆分');
      const doc=ps.app.activeDocument,width=doc.width,height=doc.height,resolution=doc.resolution,selected=doc.activeLayers;
      const opacity=selected?.length===1&&selected[0].id===request.target.active_layer_id?selected[0].opacity:NaN;
      if(!Number.isFinite(opacity)||opacity<0||opacity>100||flags.opacity===undefined)throw Error('无法核对原文字透明度，未执行拆分');
      if(doc.id!==request.target.document_id||!Number.isFinite(width)||!Number.isFinite(height)||width<=0||height<=0)throw Error('无法读取文字画布尺寸');
      const tree=await readTree(doc),original=request.target.active_layer_id,index=tree.order.indexOf(original),parent=tree.rows.find(row=>row.layerID===original)?.parentLayerID;
      if(index<0||!Number.isInteger(parent))throw Error('文字位置已改变，请重新读取');
      const history=await context.hostControl.suspendHistory({documentID:doc.id,name:'拆分文字图层'}),created=[],positions=[];
      const property=target=>({_obj:'multiGet',_target:{_ref:ref(target)},extendedReference:[['layerID','textKey','parentLayerID']],options:{failOnMissingProperty:false,failOnMissingElement:false},_options:{dialogOptions:'silent'}});
      const opacityTo=(id,value)=>({_obj:'set',_target:[{_ref:'layer',_id:id}],to:{_obj:'layer',opacity:{_unit:'percentUnit',_value:value}},_options:{dialogOptions:'silent'}});
      const visibility=target=>({_obj:'multiGet',_target:{_ref:ref(target)},extendedReference:[['layerID','visible','opacity']],options:{failOnMissingProperty:false,failOnMissingElement:false},_options:{dialogOptions:'silent'}});
      let phase='完整文字';
      function ink(value) {
        const box=value?.boundingBox,keys=['left','top','right','bottom'];
        const values=keys.map(key=>pixels(box?.[key],resolution));
        if(values.some(value=>!Number.isFinite(value))||values[2]<=values[0]||values[3]<=values[1])throw Error('无法读取独立文字字形（'+phase+'；单位='+keys.map(key=>box?.[key]?._unit||'缺失').join('/')+'；DPI='+doc.resolution+'）');
        return values;
      }
      let write=textWriter(raw),preparedOriginal=before;
      try {
        if(prepared&&prepared.textKey!==before.textKey.textKey) {
          await textWriter(before.textKey)(prepared);
          preparedOriginal=await descriptor(request.target);
          if(preparedOriginal.textKey.textKey!==prepared.textKey||!stylesMatch(preparedOriginal.textKey,prepared,resolution)||!same(layout(preparedOriginal.textKey),layout(before.textKey)))throw Error('拆分前的文字或样式读回不一致');
          raw=preparedOriginal.textKey;write=textWriter(raw);
        }
        for(const group of groups) {
          if(context.isCancelled)throw Error('文本操作已取消');
          await batch([{_obj:'duplicate',_target:ref(request.target),_options:{dialogOptions:'silent'}}]);
          const target=await capture();
          if(target.document_id!==doc.id||target.layer_ids.length!==1||tree.order.includes(target.active_layer_id)||created.includes(target.active_layer_id))throw Error('新文字图层身份未确认');
          created.push(target.active_layer_id);
          // Keep text layout active without painting the measurement copy.
          await batch([opacityTo(target.active_layer_id,0),{_obj:'show',_target:[{_ref:'layer',_id:target.active_layer_id}],_options:{dialogOptions:'silent'}}]);
          const [fullRow]=await batch([property(target)]);
          if(fullRow.layerID!==target.active_layer_id||fullRow.parentLayerID!==parent||fullRow.textKey?.textKey!==raw.textKey||!stylesMatch(fullRow.textKey,raw,resolution))throw Error('新文字副本读取不一致');
          phase='完整文字';const sourceBox=ink(fullRow.textKey);
          let originalTop=null;
          if((shape.char._value==='box'||flow)&&group.from) {
            const originalBox=sourceBox,distance=2*(originalBox[3]-originalBox[1])+100;
            if(!Number.isFinite(resolution)||resolution<=0)throw Error('无法核对文字分辨率');
            const prefix=raw.textKey.slice(0,group.to),origins=Array.from({length:prefix.length+1},(_,i)=>Math.min(i,group.to-1)),probe=mappedText(raw,prefix,origins);
            // Preserve the real prefix's wrapping; move only its ink below the target to read that target's top.
            probe.textStyleRange=probe.textStyleRange.flatMap(range=>{
              if(range.from>=group.from)return [range];
              const shifted=clone(range),style=shifted.textStyle,baseline=style.baselineShift;
              const offset=baseline===undefined||baseline===0?0:pixels(baseline,resolution);
              if(!Number.isFinite(offset))throw Error('无法核对文字基线（单位='+String(baseline?._unit||'缺失')+'；DPI='+resolution+'）');
              shifted.to=Math.min(range.to,group.from);style.baselineShift={_unit:'pixelsUnit',_value:offset-distance};delete style.impliedBaselineShift;
              return range.to>group.from?[shifted,{...range,from:group.from}]:[shifted];
            });
            const outgoing=clone(probe);
            for(const range of outgoing.textStyleRange)if(range.from<group.from)range.textStyle.baselineShift={_unit:'pointsUnit',_value:range.textStyle.baselineShift._value*72/resolution};
            await write(outgoing);
            const [measured]=await batch([property(target)]);
            if(measured.textKey.textKey!==prefix||!stylesMatch(measured.textKey,probe,resolution))throw Error('文字基线测量读回不一致');
            phase='分组定位';originalTop=ink(measured.textKey)[1];
            if(originalTop<originalBox[1]-.05||originalTop>originalBox[3]+.05)throw Error('溢出文本框的文字暂不能安全拆分');
          }
          let expected=null;
          if(!flow) {
            const masked=raw.textKey.slice(0,group.from).replace(/[^\r]/g,' ')+group.text;
            const maskedOrigins=Array.from({length:masked.length+1},(_,index)=>Math.min(index,group.to-1));
            await write(mappedText(raw,masked,maskedOrigins));
            const [maskRow]=await batch([property(target)]);phase='位置参照';expected=ink(maskRow.textKey);
          }
          const origins=Array.from({length:group.text.length+1},(_,index)=>Math.min(group.from+index,group.to-1)),value=mappedText(raw,group.text,origins);
          await write(value);
          const [isolated]=await batch([property(target)]);phase='独立分组';const box=ink(isolated.textKey);
          if(isolated.parentLayerID!==parent)throw Error('拆分后的父组改变，已取消');
          if(!stylesMatch(isolated.textKey,value,resolution))throw Error('拆分后的文字样式改变，已取消');
          if(!flow&&(Math.abs((expected[2]-expected[0])-(box[2]-box[0]))>.05||Math.abs((expected[3]-expected[1])-(box[3]-box[1]))>.05))throw Error('保留位置的排版校验未通过，请改用逐行排列（'+expected.map((v,i)=>Math.round((v-box[i])*1000)/1000).join(',')+'）');
          if(!flow&&shape.char._value==='box'&&(originalTop??expected[1])+box[3]-box[1]>sourceBox[3]+.05)throw Error('溢出文本框的文字暂不能安全拆分');
          const localX=flow?0:expected[0]-box[0],localY=(originalTop??(flow?box[1]:expected[1]))-box[1],dx=matrix.xx*localX+matrix.yx*localY,dy=matrix.xy*localX+matrix.yy*localY;
          const click=clone(raw.textClickPoint);
          if(click?.horizontal?._unit!=='percentUnit'||click?.vertical?._unit!=='percentUnit'||!Number.isFinite(click.horizontal._value)||!Number.isFinite(click.vertical._value))throw Error('无法核对文字锚点');
          click.horizontal._value+=dx/width*100;click.vertical._value+=dy/height*100;
          await write({...value,textClickPoint:click});
          const [placed]=await batch([property(target)]),actualClick=placed.textKey.textClickPoint;
          if(placed.layerID!==target.active_layer_id||placed.parentLayerID!==parent||placed.textKey.textKey!==group.text||!stylesMatch(placed.textKey,value,resolution)||!Number.isFinite(actualClick?.horizontal?._value)||!Number.isFinite(actualClick?.vertical?._value)||
              !same(placed.textKey.antiAlias,raw.antiAlias)||!same(placed.textKey.textGridding,raw.textGridding)||!same(placed.textKey.orientation,raw.orientation)||!same(placed.textKey.transform,raw.transform)||Math.abs((actualClick?.horizontal?._value-click.horizontal._value)*width/100)>.05||Math.abs((actualClick?.vertical?._value-click.vertical._value)*height/100)>.05)throw Error('拆分位置或样式读回不一致');
          positions.push({layer_id:target.active_layer_id,dx,dy});
        }
        await batch([{_obj:'select',_target:[{_ref:'layer',_id:original}],makeVisible:false,_options:{dialogOptions:'silent'}}]);
        if(!same(await descriptor(request.target),preparedOriginal))throw Error('原文字已改变，已取消拆分');
        await batch([{_obj:'hide',_target:[{_ref:'layer',_id:original}],_options:{dialogOptions:'silent'}},
          ...(!flags.visible?created.map(id=>({_obj:'hide',_target:[{_ref:'layer',_id:id}],_options:{dialogOptions:'silent'}})):[]),
          ...created.map(id=>opacityTo(id,opacity)),
          {_obj:'delete',_target:ref(request.target),_options:{dialogOptions:'silent'}},
          {_obj:'select',_target:[{_ref:'layer',_id:created[0]}],makeVisible:false,_options:{dialogOptions:'silent'}}]);
        const after=await readTree(doc),wanted=tree.order.slice();wanted.splice(index,1,...created);
        if(after.order.join(',')!==wanted.join(',')||created.some(id=>after.rows.find(row=>row.layerID===id)?.parentLayerID!==parent))throw Error('拆分图层顺序读回不一致');
        const visible=await batch(created.map(id=>visibility({...request.target,active_layer_id:id})));
        if(visible.some((row,i)=>row.layerID!==created[i]||row.visible!==flags.visible||!same(row.opacity,flags.opacity)))throw Error('拆分文字透明度或可见性读回不一致');
        await context.hostControl.resumeHistory(history,true);snapshot=null;
        return {verified:true,count:created.length,layer_ids:created,positions};
      } catch(error) {
        try {
          await context.hostControl.resumeHistory(history,false);
          await batch([{_obj:'select',_target:[{_ref:'layer',_id:original}],makeVisible:false,_options:{dialogOptions:'silent'}}]);
          const [restored]=await batch([visibility(request.target)]);
          if(restored.visible!==flags.visible||!same(restored.opacity,flags.opacity)||!same(await descriptor(request.target),before)||(await readTree(doc)).order.join(',')!==tree.order.join(','))throw Error('Text split restore mismatch');
        } catch(_) {uncertain=true;}
        throw error;
      }
    },{commandName:'拆分文字图层',timeOut:0});}
    catch(error){if(uncertain){const unknown=Error('文字拆分恢复结果待核对');unknown.resultUnknown=true;throw unknown;}throw error;}
  }
  return {read,replace,split,prepareObject,finishObject};
}

function createLayerView(ps,session,capture) {
  const property=name=>({_obj:'get',_target:[{_ref:'property',_property:name},{_ref:'document',_enum:'ordinal',_value:'targetEnum'}],_options:{dialogOptions:'silent'}});
  const number=v=>typeof v==='number'?v:v?._value;
  async function batch(commands) {
    const result=await ps.action.batchPlay(commands,{});
    if(!Array.isArray(result)||result.length!==commands.length||result.some(r=>!r||r._obj==='error'))throw Error('PS聚焦视图命令被拒绝');
    return result;
  }
  async function menus() {
    const [result]=await batch([{_obj:'get',_target:[{_ref:'property',_property:'menuBarInfo'},{_ref:'application',_enum:'ordinal',_value:'targetEnum'}],_options:{dialogOptions:'silent'}}]);
    const found=[];
    function visit(value) {
      if(!value||typeof value!=='object')return;
      if(Number.isInteger(value.command)&&typeof value.checked==='boolean') {
        const name=String(value.name||value.title||'').replace(/&/g,'');
        if(/^(Layer Edges|图层边缘|Extras|显示额外内容|额外内容)$/i.test(name))found.push({...value,name});
      }
      for(const item of Object.values(value))if(item&&typeof item==='object')visit(item);
    }
    visit(result);
    const edge=found.filter(r=>/^(Layer Edges|图层边缘)$/i.test(r.name)),extras=found.filter(r=>/^(Extras|显示额外内容|额外内容)$/i.test(r.name));
    if(edge.length!==1||extras.length!==1)throw Error('无法核验PS原生图层边缘／额外内容状态');
    const ids=[edge[0].command,extras[0].command];
    // menuBarInfo caches checked flags; uiInfo reads the current native state.
    const states=await batch(ids.map(commandID=>({_obj:'uiInfo',_target:{_ref:'application',_enum:'ordinal',_value:'targetEnum'},command:'getCommandEnabled',commandID})));
    const live=states.map((r,i)=>{if(r.result?.commandID!==ids[i]||typeof r.result?.checked!=='boolean')throw Error('PS原生边缘实时状态不可用');return {command:ids[i],checked:r.result.checked};});
    return {edge:live[0],extras:live[1]};
  }
  async function view() {
    const [z,c]=await batch(['zoom','center'].map(property)),zoom=number(z.zoom),x=number(c.center?.horizontal),y=number(c.center?.vertical);
    if(!Number.isFinite(zoom)||zoom<=0||!Number.isFinite(x)||!Number.isFinite(y))throw Error('无法核验PS视图位置');
    return {zoom,x,y};
  }
  const zoomCommand=zoom=>({_obj:'set',_target:property('zoom')._target,to:{_obj:'zoom',zoom},_options:{dialogOptions:'silent'}});
  async function inspect() {return {menus:await menus(),view:await view()};}
  async function frame(request,io) {
    let finishPreview=null,rollback=null;
    const prepared=await ps.core.executeAsModal(async context=>{
      const expected=request.target;
      async function documentGuard() {
        const target=await capture();
        if(expected?.session!==session||target.session!==session||target.document_id!==expected.document_id)throw Error('聚焦文档已改变，未操作其他文档');
        return target;
      }
      async function guard() {
        if(context.isCancelled||Date.now()>request.expires_at_ms)throw Error('聚焦已取消或过期');
        const target=await documentGuard();
        if(target.active_layer_id!==expected.active_layer_id||target.layer_ids.join(',')!==expected.layer_ids?.join(','))throw Error('聚焦目标已改变，本次未执行');
      }
      let seq=0,lease=Date.now()+4000,phase=io?'preview':'commit';
      async function input() {
        if(!io)return;
        const packet=await io.read(),now=Date.now();
        if(packet){
          if(packet.id!==request.id||packet.session!==request.session||packet.token!==request.token||!Number.isSafeInteger(packet.seq)||packet.seq<seq||!Number.isFinite(packet.lease_until_ms)||packet.lease_until_ms>now+5000||!['preview','commit','cancel'].includes(packet.phase))throw Error('Invalid F gesture mailbox');
          seq=packet.seq;lease=packet.lease_until_ms;phase=packet.phase;
        }
        if(context.isCancelled||now>lease)phase='cancel';
      }
      await guard();await input();
      if(phase==='cancel')return {verified:true,cancelled:true,view_restored:true};
      const doc=ps.app.activeDocument,layers=Array.from(doc.activeLayers);
      if(!layers.length||layers.length>128||layers.map(l=>l.id).sort((a,b)=>a-b).join(',')!==expected.layer_ids.join(','))throw Error('聚焦最多支持128个选中图层');
      const boxes=[];
      for(const layer of layers) {
        if(typeof layer.visible!=='boolean')throw Error('图层可见性无法确认');
        for(let parent=layer;parent&&parent!==doc;parent=parent.parent)if(parent.visible===false)throw Error('不能显示隐藏的目标图层边界');
        const b=layer.boundsNoEffects,rect=['left','top','right','bottom'].map(k=>number(b?.[k]));
        if(!rect.every(Number.isFinite))throw Error('图层边界无法确认');
        if(rect[2]>rect[0]&&rect[3]>rect[1])boxes.push(rect);
      }
      if(!boxes.length)throw Error('所选图层没有可聚焦的内容边界');
      const bounds=[Math.min(...boxes.map(b=>b[0])),Math.min(...boxes.map(b=>b[1])),Math.max(...boxes.map(b=>b[2])),Math.max(...boxes.map(b=>b[3]))];
      const saved=await inspect();let changed=false;
      async function display(edge,extras) {
        let state=await menus();
        async function toggle(key) {
          await documentGuard();
          if(!await ps.core.performMenuCommand({commandID:state[key].command}))throw Error('PS原生边界显示命令不可用');
          state=await menus();
        }
        // With Extras off, the edge command only turns Extras on, leaving its own flag unchanged.
        if(state.edge.checked!==edge){
          if(!state.extras.checked)await toggle('extras');
          await toggle('edge');
        }
        if(state.extras.checked!==extras)await toggle('extras');
        if(state.edge.checked!==edge||state.extras.checked!==extras)throw Error('原生边界显示恢复未确认');
      }
      async function restoreView() {
        await documentGuard();
        await batch([zoomCommand(saved.view.zoom),{_obj:'set',_target:property('center')._target,to:{_obj:'center',horizontal:{_unit:'distanceUnit',_value:saved.view.x},vertical:{_unit:'distanceUnit',_value:saved.view.y}},_options:{dialogOptions:'silent'}}]);
        const restored=await view();
        if(Math.abs(restored.zoom-saved.view.zoom)>1e-6||Math.abs(restored.x-saved.view.x)>1||Math.abs(restored.y-saved.view.y)>1)throw Error('View restore mismatch');
      }
      try {
        await guard();changed=true;
        await batch([{_obj:'select',_target:[{_ref:'menuItemClass',_enum:'menuItemType',_value:'fitLayersOnScreen'}],_options:{dialogOptions:'silent'}}]);
        const fitted=await view(),zoom=Math.max(.0001,Math.min(4,fitted.zoom));
        if(zoom!==fitted.zoom){await guard();await batch([zoomCommand(zoom)]);}
        await display(true,true);await guard();const after=await inspect();
        if(Math.abs(after.view.zoom-zoom)>1e-6||!after.menus.edge.checked||!after.menus.extras.checked||Math.abs(after.view.x/zoom-fitted.x/fitted.zoom)>1||Math.abs(after.view.y/zoom-fitted.y/fitted.zoom)>1)throw Error('聚焦视图或原生边界读回不一致');
        rollback=()=>ps.core.executeAsModal(async()=>{await restoreView();await display(saved.menus.edge.checked,(await menus()).extras.checked && saved.menus.extras.checked);},{commandName:'恢复聚焦视图',timeOut:0});
        finishPreview=async()=>{
        const shown=Date.now();
        if(io)await io.applied({seq,view:after.view,native_layer_edges:true});
        do {
          await input();
          const current=await documentGuard();
          if(current.active_layer_id!==expected.active_layer_id||current.layer_ids.join(',')!==expected.layer_ids.join(','))phase='cancel';
          if(phase==='cancel'||phase==='commit'&&Date.now()-shown>=500)break;
          await new Promise(resolve=>setTimeout(resolve,40));
        } while(true);
        await ps.core.executeAsModal(async()=>{
        if(phase==='cancel')await restoreView();
        // F is a temporary highlight, including when upgrading from persistent 319 edges.
        await display(false,(await menus()).extras.checked && saved.menus.extras.checked);
        },{commandName:'结束临时聚焦',timeOut:0});
        return {verified:true,bounds,zoom_percent:after.view.zoom*100,selection_count:layers.length,native_layer_edges:false,view_restored:phase==='cancel',cancelled:phase==='cancel'};
        };return null;
      } catch(error) {
        if(changed)try {await restoreView();await display(saved.menus.edge.checked,(await menus()).extras.checked && saved.menus.extras.checked);}
        catch(_){error.resultUnknown=true;}
        throw error;
      }
    },{commandName:'聚焦所选图层',timeOut:0});
    if(!finishPreview)return prepared;
    // Native layer edges paint after the modal exits; holding F keeps this same request open.
    try{return await finishPreview();}
    catch(error){try{await rollback();}catch(_){error.resultUnknown=true;}throw error;}
  }
  return {inspect,frame};
}
