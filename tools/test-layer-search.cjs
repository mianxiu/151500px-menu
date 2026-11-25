const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(require.resolve('../uxp/offline-probe/labels.js'),'utf8');
let activeDoc=1,selected=[11,12],active=12,writes=0,failSelect=false,failRestore=false,badList=false;let pixelBounds={left:0,top:0,right:2,bottom:1};
const row=(id,name,kind=1,extra={})=>({layerID:id,name,layerKind:kind,parentLayerID:0,layerSection:{_value:'layerSectionContent'},...extra});
let docs=[{id:1,title:'原稿.psd',rows:[row(11,'重名'),row(12,'重名',3)]},{id:2,title:'目标.psd',rows:[row(21,'重名',4,{hasVectorMask:true}),row(22,'填充',4)]}];
const capture=async()=>({session:'s',document_id:activeDoc,active_layer_id:active,layer_ids:[...selected].sort((a,b)=>a-b)});
const ps={core:{executeAsModal:async fn=>fn({isCancelled:false})},action:{batchPlay:async commands=>commands.map(c=>{
  const ref=c._target?._ref||c._target,docRef=ref.find(r=>r._ref==='document'),doc=docRef?docs.find((d,i)=>docRef._id?d.id===docRef._id:i+1===docRef._index):docs.find(d=>d.id===activeDoc);
  if(c._obj==='get'){
    assert.equal(c._options.dialogOptions,'silent');
    if(!doc)return {_obj:'error'};
    return {[ref[0]._property]:({documentID:doc.id,title:doc.title,numberOfLayers:doc.rows.length,hasBackgroundLayer:false,mode:{_value:'RGBColor'},boundsNoEffects:pixelBounds,bounds:pixelBounds})[ref[0]._property]};
  }
  if(c._obj==='multiGet'){
    assert.equal(c._options.dialogOptions,'silent');
    if(!doc)return {_obj:'error'};
    if(c.extendedReference[1]){const {index,count}=c.extendedReference[1];return {list:badList?[]:doc.rows.slice(index-1,index-1+count)};}
    return doc.rows.find(r=>r.layerID===ref[0]._id)||{_obj:'error'};
  }
  if(c._obj==='select'){
    writes++;assert.equal(c._options.dialogOptions,'silent');
    if(docRef){if(!doc)return {_obj:'error'};activeDoc=doc.id;return {};}
    assert.equal(c.makeVisible,false);
    if(failSelect&&ref[0]._id===21||failRestore&&ref[0]._id===11)return {_obj:'error'};
    active=ref[0]._id;selected=c.selectionModifier?[...selected,active]:[active];return {};
  }
  throw Error('Unexpected command '+c._obj);
})}};
const context={ps,capture,session:'s',nativeDocumentCount:async()=>docs.length};
vm.createContext(context);vm.runInContext(source.slice(source.lastIndexOf('function createLayerSearch('))+'\nthis.search=createLayerSearch(ps,session,capture);',context);
const search=context.search,query={name:'重名',document:'',type:'all'};
(async()=>{
  let checks=0;
  const result=await search.search(query,async()=>checks++);
  assert.equal(result.items.length,3);assert.equal(checks,12);assert.equal(writes,0);
  assert.deepEqual(Array.from(result.items,r=>r.type),['pixel','text','shape']);
  let disposed=0;const original=JSON.stringify(await capture());
  const tile={width:2,height:1,components:4,componentSize:8,getData:async()=>new Uint8Array([255,0,0,255,0,255,0,128]),dispose:()=>disposed++};
  assert.equal((await search.preview(result.items[0])).available,false);
  ps.imaging={getPixels:async options=>{assert.equal(options.layerID,11);assert.equal(options.targetSize.width,2);assert.equal(options.colorProfile,'sRGB IEC61966-2.1');return {imageData:tile};}};
  const image=await search.preview(result.items[0]);assert.deepEqual(Array.from(image.pixels),[255,0,0,255,0,255,0,128]);assert.equal(disposed,1);
  assert.equal(JSON.stringify(await capture()),original);assert.equal(writes,0);
  pixelBounds={left:0,top:0,right:1,bottom:20000};
  ps.imaging.getPixels=async options=>{assert.deepEqual(Object.keys(options.targetSize),['height']);assert.equal(options.targetSize.height,128);return {imageData:{width:1,height:128,components:4,componentSize:8,getData:async()=>new Uint8Array(512),dispose:()=>{}}};};
  assert.equal((await search.preview(result.items[0])).height,128);
  pixelBounds={left:0,top:0,right:2,bottom:1};
  ps.imaging.getPixels=async()=>{throw Error('unsupported pixels');};
  assert.equal((await search.preview(result.items[0])).fallback,true);
  ps.imaging.getPixels=async()=>({imageData:tile});

  tile.getData=async()=>{throw Error('pixels failed');};assert.equal((await search.preview(result.items[0])).fallback,true);assert.equal(disposed,2);
  tile.getData=async()=>new Uint8Array(8);ps.imaging.getPixels=async()=>{docs[0].rows[0].name='changed';return {imageData:tile};};
  await assert.rejects(search.preview(result.items[0]),/已改变/);assert.equal(disposed,3);docs[0].rows[0].name='重名';
  assert.equal((await search.search({...query,document:'目标',type:'shape'})).items[0].layer_id,21);
  assert.equal((await search.search({...query,name:'',type:'fill'})).items[0].layer_id,22);
  const vector={placed:{_value:'vectorData'},linked:false};
  const xmp='https://15100px.menu/ns/text-object/1.0/';
  docs[1].rows.push(row(23,'文字源',5,{smartObject:vector,XMPMetadataAsUTF8:xmp}),
    row(24,'引用',5,{smartObject:{...vector,linked:true},XMPMetadataAsUTF8:xmp}),
    row(25,'普通对象',5,{smartObject:vector}));
  const smart=await search.search({...query,name:'',type:'smart-object'});
  assert.deepEqual(Array.from(smart.items,row=>row.text_source),[true,undefined,undefined]);
  await assert.rejects(search.search({...query,type:'bad'}),/无效/);
  badList=true;await assert.rejects(search.search(query),/列表已改变/);badList=false;
  await assert.rejects(search.search(query,()=>{throw Error('yield');}),/yield/);assert.equal(writes,0);
  const destination=result.items[2],request={target:await capture(),destination,expires_at_ms:Date.now()+5000};
  await assert.rejects(search.focus({...request,destination:{...destination,name:'changed'}}),/已改变/);assert.equal(writes,0);
  failSelect=true;await assert.rejects(search.focus(request),/读取失败/);assert.deepEqual(await capture(),request.target);
  failRestore=true;await assert.rejects(search.focus(request),error=>error.resultUnknown===true);failRestore=false;failSelect=false;
  activeDoc=1;selected=[11,12];active=12;
  await search.focus(request);assert.equal(activeDoc,2);assert.deepEqual(selected,[21]);
  await assert.rejects(search.focus(request),/当前选择已改变/);
  docs=[{id:1,title:'Large.psd',rows:Array.from({length:600},(_,i)=>row(i+1,'x'))}];
  const limited=await search.search({...query,name:''});assert.equal(limited.items.length,500);assert.equal(limited.truncated,true);

  docs=[{id:1,title:'原稿.psd',rows:[row(11,'重名'),row(12,'重名',3)]}];activeDoc=1;selected=[11,12];active=12;
  const beforeUnsupported=JSON.stringify(await capture()),writesBefore=writes;
  ps.app={documents:[],createDocument:()=>assert.fail('Preview must never create a document')};
  ps.imaging=undefined;
  const unavailable=await search.preview(result.items[0]);assert.equal(unavailable.available,false);
  assert.equal(typeof search.thumbnail,'undefined');assert.equal(writes,writesBefore);assert.equal(JSON.stringify(await capture()),beforeUnsupported);
  assert(!source.slice(source.lastIndexOf('function createLayerSearch(')).includes('ps.app.createDocument'));
  assert(!source.slice(source.lastIndexOf('function createLayerSearch(')).includes('exportSelectionAsFileTypePressed'));
  console.log('Unavailable silent preview: no document, export, selection or fallback; direct imaging retained');
  console.log('Layer search: name/file/type, native chunks, limits, yield, stale, cross-document, rollback and unknown passed');
  const worker=fs.readFileSync('uxp/offline-probe/main.js','utf8'),files=new Map(),id='a'.repeat(32);
  let calls=0;
  const mailbox={token:'t',session:'s',searchRefreshId:'',Date,
    labels:{searchLayers:async(query,tick)=>{calls++;await tick();return {items:[]};}},
    record:async(_,name,value)=>files.set(name,JSON.stringify(value))};
  vm.createContext(mailbox);
  vm.runInContext(worker.slice(worker.indexOf('async function refreshLayerSearch('),worker.indexOf('async function finishResponse(')),mailbox);
  const folder={getEntry:async name=>{if(!files.has(name))throw Error('missing');return {read:async()=>files.get(name)};}};
  files.set('layer-search-request.json',JSON.stringify({id,token:'t',session:'s',query,expires_at_ms:Date.now()+1000}));
  await mailbox.refreshLayerSearch(folder,Date.now(),async()=>{const error=Error('priority');error.menuReadInterrupted=true;throw error;});
  assert.match(JSON.parse(files.get('layer-search-result.json')).error,/优先/);
  assert.equal(await mailbox.refreshLayerSearch(folder,Date.now(),async()=>{}),false);assert.equal(calls,1);
  console.log('Search mailbox: priority interruption and no replay passed');
})().catch(error=>{console.error(error);process.exitCode=1;});
