import {render} from 'solid-js/web';
import {createSignal, createMemo, createEffect, onMount, onCleanup, For, Show} from 'solid-js';
import {Client} from './client.mjs';
import {translate} from './i18n.mjs';
import {diagnosticClock,diagnosticScopes,diagnosticComponents,diagnosticFinding,inputHealthRows,keyDecisions} from './diagnostics.mjs';
import {healthView,healthFinding} from './version-health.mjs';
import {applyMenuTheme} from './theme.mjs';
import {stabilityIssue} from './stability.mjs';
import {prefixOptions,triggerKeys,keyLabel,shortcutParts,triggerAllowed,changePrefix,changeTrigger,shortcutErrors} from './shortcuts.mjs';
import './admin.css';

const pages = [['overview','运行状态'],['settings','菜单设置'],['plugins','插件管理'],['updates','应用更新'],['tasks','最近任务'],['logs','运行日志']];
const keyLabels = {toggle:'按住唤出 / 松开执行', hide:'收起菜单', select:'选择组内图层', color:'橙色图层标签', moveToTop:'移到顶部', moveToBottom:'移到底部',selectAbove:'单选上一个图层',selectBelow:'单选下一个图层',extendAbove:'向上扩展图层多选',extendBelow:'向下扩展图层多选',copyStyle:'复制图层样式',pasteStyle:'粘贴图层样式',editStyle:'打开图层样式',convertSmartObject:'转换为智能对象'};
function App() {
  function t(text){return translate(text,state().language || 'zh-CN');}
  const [connection,setConnection] = createSignal('connecting');
  const [state,setState] = createSignal({photoshop:{},plugin:{active:{}},settings:{shortcuts:{}}});
  const [page,setPage] = createSignal('overview');
  const [form,setForm] = createSignal(null);
  const [dirty,setDirty] = createSignal(false);
  const [working,setWorking] = createSignal(false);
  const [notice,setNotice] = createSignal({message:'',ok:true});
  createEffect(()=>{
    const current=notice();
    if(!current.message || !current.ok)return;
    const timer=setTimeout(()=>setNotice(value=>value===current?{message:'',ok:true}:value),5000);
    onCleanup(()=>clearTimeout(timer));
  });
  const [preview,setPreview] = createSignal(null);
  const [logs,setLogs] = createSignal('');
  const [update,setUpdate] = createSignal(null);
  const [uxpPreview,setUxpPreview]=createSignal(null);
  const [photoshopExe,setPhotoshopExe]=createSignal('');
  const [updateRestart,setUpdateRestart]=createSignal(false);
  const appUpdate=()=>state().application_update || {};
  const updateBusy=()=>['checking','downloading','installing'].includes(appUpdate().phase);
  createEffect(()=>{document.documentElement.lang=state().language || 'zh-CN';document.title='151500px-menu — '+t('后台管理');});
  createEffect(()=>applyMenuTheme(document.documentElement,state().photoshop.ui_theme));
  const receivedDiagnostic=value=>({...value,received_at:performance.now()});
  function accept(data) {
    setState({...data,diagnostics:receivedDiagnostic(data.diagnostics)});
    if (!dirty()) setForm(structuredClone(data.settings));
  }
  const client = new Client(`ws://${location.host}/websocket`, status => {
    setConnection(status);
    if(status==='connected') client.request('status').then(accept).catch(error=>setNotice({ok:false,message:error.message}));
  }, event => {
    if(event.type==='status') accept(event.data);
    if(event.type==='notice') setNotice(event);
    if(event.type==='diagnostics')setState(previous=>({...previous,diagnostics:receivedDiagnostic(event.data)}));
    if(event.type==='version-health')setState(previous=>({...previous,version_health:event.data}));
    if(event.type==='stability')setState(previous=>({...previous,stability:event.data}));
    if(event.type==='tasks') setState(previous=>({...previous,tasks:event.data}));
  });
  async function call(action,args={}) {
    setWorking(true);
    try {const result=await client.request(action,args); if(action==='status')accept(result); if(action==='diagnostics.start')setState(previous=>({...previous,diagnostics:receivedDiagnostic(result)})); if(result.photoshop_start)setState(previous=>({...previous,photoshop_start:result.photoshop_start})); if(!['application.update.check','application.update.download'].includes(action))setNotice({ok:true,message:result.message || t("操作完成")}); return result;}
    catch(error){setNotice({ok:false,message:error.message}); throw error;}
    finally{setWorking(false);}
  }
  const disabled = ()=>working() || connection()!=='connected';
  const perform = (action,args={})=>call(action,args).catch(()=>{});
  async function installApplication(){
    try{await call('application.update.install');setUpdateRestart(true);}
    catch{}
  }
  createEffect(()=>{
    if(updateRestart() && connection()==='connected' && appUpdate().installation?.phase==='completed'){
      // Load the newly installed frontend after the service confirms its path
      // and version; never replay the installation request on reconnect.
      location.reload();
    }
  });
  function ApplicationUpdate(){return <section>
    <div class="section-heading"><h2>{t("GitHub 应用更新")}</h2><span class="version-tag">v{state().version || '—'}</span></div>
    <p class="update-message" role="status">{t(appUpdate().message || '尚未检查 GitHub 正式版本')}</p>
    <Show when={appUpdate().release?.version}><p>{t("正式版本")} v{appUpdate().release.version}</p></Show>
    <Show when={appUpdate().error}><p class="error">{t(appUpdate().error)}</p></Show>
    <Show when={appUpdate().phase==='downloading'}><progress aria-label={t("下载进度")} max="100" value={appUpdate().progress || 0}/><p>{appUpdate().progress || 0}%</p></Show>
    <div class="buttons"><button class="primary" disabled={disabled() || updateBusy()} onClick={()=>perform('application.update.check')}>{t("检查应用更新")}</button><Show when={appUpdate().available || appUpdate().staged}><button disabled={disabled() || updateBusy() || !appUpdate().available} onClick={()=>perform('application.update.download')}>{t("下载并校验")}</button><button disabled={disabled() || updateBusy() || dirty() || !appUpdate().supported || !appUpdate().staged || state().photoshop.busy || state().photoshop_start?.active || state().menu?.visible} onClick={installApplication}>{t("安装并重启后台")}</button></Show></div>
    <label class="option-row"><input type="checkbox" checked={appUpdate().auto_check ?? true} disabled={disabled() || appUpdate().phase==='installing'} onChange={e=>perform('application.update.auto',{enabled:e.target.checked})}/><span>{t("每天自动检查正式版本（不自动下载或安装）")}</span></label>
    <Show when={appUpdate().last_checked}><p class="caption">{t("上次检查：")}{new Date(appUpdate().last_checked*1000).toLocaleString(state().language || 'zh-CN')}</p></Show>
    <details class="help"><summary>{t("更新说明")}</summary><p>{t("更新只正常重启后台，保留配置与旧版本，不关闭 Photoshop。任务忙碌或结果待核对时拒绝安装，不自动重发操作。")}</p><p>{t("内置 UXP 随应用更新；已安装插件默认在保存并关闭PS后自动跟随较新内置版。")}</p><a href={appUpdate().source || 'https://github.com/mianxiu/151500px-menu/releases'} target="_blank" rel="noreferrer">{t("打开 GitHub Releases")}</a><Show when={!appUpdate().supported}><p>{t("源码运行模式可以检查与下载；自动安装请使用 Windows 发行版。")}</p></Show></details>
    <Show when={appUpdate().release?.notes}><details class="help"><summary>{t("发行说明")}</summary><pre class="release-notes">{appUpdate().release.notes}</pre></details></Show>
  </section>;}
  function edit(key,value){setDirty(true);setForm({...form(),[key]:value});}
  function shortcut(key,value){setDirty(true);setForm({...form(),shortcuts:{...form().shortcuts,[key]:value}});}
  const shortcutIssues=createMemo(()=>shortcutErrors(form()?.shortcuts));
  const invalidShortcuts=()=>Object.keys(shortcutIssues()).length>0;
  async function save(){
    if(invalidShortcuts()){setNotice({ok:false,message:t('请先修正快捷键设置')});return;}
    try{const result=await call('settings.save',{settings:form()});setForm(result.settings);setDirty(false);}catch{}
  }
  async function readLogs(){try{setLogs((await call('logs.read')).text);}catch{}}
  function ZipUpload(props){
    let input;
    return <><button type="button" disabled={disabled()} onClick={()=>input.click()}>{t(props.label)}</button><input ref={input} hidden type="file" accept=".zip" aria-label={t(props.label)} disabled={disabled()} onChange={props.onChange}/></>;
  }
  async function upload(event){
    const file=event.currentTarget.files?.[0]; if(!file) return;
    setWorking(true);setPreview(null);
    try{const body=new FormData();body.append('file',file);const response=await fetch('/api/plugins/preview',{method:'POST',body});const result=await response.json();if(!result.ok)throw new Error(result.error);setPreview(result.data);setNotice({ok:true,message:result.data.message});}
    catch(error){setNotice({ok:false,message:error.message});}
    finally{setWorking(false);event.target.value='';}
  }
  async function install(){try{await call('plugin.install',{token:preview().token});setPreview(null);setUpdate(null);}catch{}}
  async function check(){try{setUpdate(await call('plugin.check'));}catch{}}
  async function download(){try{setPreview(await call('plugin.download'));}catch{}}
  async function uploadUxp(event){
    const file=event.currentTarget.files?.[0];if(!file)return;
    setWorking(true);setUxpPreview(null);
    try{const body=new FormData();body.append('file',file);const response=await fetch('/api/uxp/preview',{method:'POST',body});const result=await response.json();if(!result.ok)throw new Error(result.error);setUxpPreview(result.data);setNotice({ok:true,message:result.data.message});}
    catch(error){setNotice({ok:false,message:error.message});}
    finally{setWorking(false);event.target.value='';}
  }
  function UxpRecovery(){return <Show when={state().photoshop.uxp_recovery?.active || state().photoshop.uxp_recovery?.message}><div role="status"><p>{t(state().photoshop.uxp_recovery.message)}</p><Show when={state().photoshop.uxp_recovery.active}><button disabled={disabled()} onClick={()=>perform('tasks.reconcile')}>{t("核对 UXP 任务结果")}</button><p>{t("只读取原请求的最终记录，不重新执行、不切换 COM。缺少可靠结果时继续阻止修改，重启不会清除待核对记录。")}</p></Show></div></Show>;}
  function DiagnosticPanel(){
    const [now,setNow]=createSignal(performance.now());
    const [scope,setScope]=createSignal('all');
    createEffect(()=>{if(state().diagnostics?.phase!=='collecting')return;
      const timer=setInterval(()=>setNow(performance.now()),200);onCleanup(()=>clearInterval(timer));});
    const clock=()=>diagnosticClock(state().diagnostics,now());
    return <section><h2>{t("统一诊断")}</h2><label class="option-row"><span>{t("检测范围")}</span><select aria-label={t("检测范围")} value={scope()} disabled={disabled() || state().stability?.active || state().diagnostics?.phase==='collecting'} onChange={e=>setScope(e.target.value)}><For each={diagnosticScopes}>{([id,label])=><option value={id}>{t(label)}</option>}</For></select></label><Show when={scope()==='all' || scope()==='menu'}><p>{t("开始后5秒内切回PS，点击画布或图层列表，在采集倒计时内分别按菜单唤出键和 F；输入故障可再进入文字或数值框，退出后重试。")}</p><p><kbd>{state().settings.shortcuts?.toggle}</kbd><kbd>F</kbd></p></Show>
      <div class="diagnostic-progress"><span role="status">{t(clock().label)}</span><Show when={clock().showSeconds}><strong aria-live="off">{clock().seconds} {t("秒")}</strong></Show></div>
      <Show when={state().diagnostics?.phase==='collecting' && state().diagnostics?.stage==='sampling'}><p class="caption">{state().diagnostics.samples_done || 0} / {state().diagnostics.samples_total || 10} {t("个采样")}</p></Show>
      <For each={Object.entries(state().diagnostics?.components || {})}>{([id,phase])=><p class="caption">{t(diagnosticComponents[id])} · {t(({collecting:'检测中',completed:'已完成',failed:'未完成'})[phase] || '等待检测')}</p>}</For>
      <For each={state().diagnostics?.findings || []}>{code=><p role="status">{t(diagnosticFinding(code))}</p>}</For>
      <div class="buttons"><button disabled={disabled() || state().stability?.active || state().diagnostics?.phase==='collecting'} onClick={()=>perform('diagnostics.start',{scope:scope()})}>{t("开始只读诊断")}</button><button disabled={disabled() || !state().diagnostics?.ready} onClick={exportDiagnostics}>{t("导出统一报告")}</button></div><p class="caption">{t("综合检测合并菜单／热键、UXP安装加载、图层读取耗时、任务与更新状态，生成一份JSON；单项失败仍可导出其余结果。")}</p><p class="caption">{t("仅读取窗口结构、插件加载日志和已有状态，不读取输入文字、文档内容或凭据，不向PS提交操作。")}</p></section>;
  }
  function InputHealthPanel(){return <section><h2>{t('Hook与输入保护')}</h2>
    <dl class="diagnostic-facts"><For each={inputHealthRows(state().input_diagnostics)}>{([label,value])=><div><dt>{t(label)}</dt><dd>{t(String(value))}</dd></div>}</For></dl>
    <p class="caption">{t('健康上报只证明组件仍在通信，不能证明每个按键都已收到。F明细仅在统一诊断采集期间记录，不记录输入正文。')}</p>
    <details><summary>{t('最近按键与保护详情')}</summary><For each={[...(state().input_diagnostics?.events || [])].reverse().filter(e=>e.message===256 || e.message===260).slice(0,12)}>{event=><p class="caption">{new Date(event.received_at_ms).toLocaleTimeString()} · {event.key===70?'F':event.key===20?'Caps':`VK ${event.key}`} · {t(keyDecisions[event.decision] || '拦截结果待核对')}</p>}</For><pre>{JSON.stringify({tool_observation:state().input_diagnostics?.tool_observation,focus:state().input_diagnostics?.focus,ime:state().input_diagnostics?.ime,configuration:state().input_diagnostics?.configuration},null,2)}</pre></details>
  </section>;}
  function StabilityPanel(){
    const [profile,setProfile]=createSignal('standard');
    const test=()=>state().stability || {};
    const issue=()=>stabilityIssue(test());
    async function start(){try{const result=await call('stability.start',{profile:profile()});setState(previous=>({...previous,stability:result}));}catch{}}
    async function stop(){try{const result=await call('stability.stop');setState(previous=>({...previous,stability:result}));}catch{}}
    async function cleanup(){try{const result=await call('stability.cleanup');setState(previous=>({...previous,stability:result}));}catch{}}
    async function download(){try{const report=await call('stability.export');const url=URL.createObjectURL(new Blob([JSON.stringify(report,null,2)],{type:'application/json;charset=utf-8'}));const link=document.createElement('a');link.href=url;link.download='ps-menu-stability-'+report.run_id+'.json';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}catch{}}
    return <section id="stability-test"><h2>{t('自动稳定性测试')}</h2>
      <p>{t('在隔离临时文档中反复唤起／收起原生菜单，核验图层读取和按钮恢复，并测试标签、增层、颜色／羽化及撤销。菜单会短暂显示；请勿操作PS，可停留在后台管理窗口。')}</p>
      <label class="advanced-field"><span>{t('测试强度')}</span><select aria-label={t('测试强度')} disabled={disabled() || test().active} value={profile()} onChange={e=>setProfile(e.target.value)}><option value="standard">{t('标准：3轮，24次新增图层')}</option><option value="extended">{t('持续：10轮，80次新增图层')}</option></select></label>
      <p role="status">{t(test().message || '尚未开始测试')}</p>
      <Show when={test().active || test().ready}><p class="caption">{test().round || 0} / {test().rounds || 0} {t('轮')} · {test().completed_steps || 0} {t('个步骤')} · {test().samples || 0} {t('个系统采样')}</p></Show>
      <Show when={issue()}>{problem=><><p class={['user-stopped','foreground-changed'].includes(problem().code)?'caption':'error'}>{t(problem().message)}</p><p class="caption">{t(problem().hint)}</p><details><summary>{t('错误详情')}</summary><p class="caption">{t('测试阶段')} · {t(problem().stage)}</p><p>{problem().detail}</p></details></>}</Show>
      <Show when={!test().active && test().cleanup?.includes('retained')}><p>{t('临时文档已保留供检查，未自动关闭或重发操作。')}</p></Show>
      <Show when={test().cleanup_available}><div class="buttons"><button disabled={disabled() || test().active} onClick={cleanup}>{t('结束上次测试文档')}</button></div><p class="caption">{t('仅核验并关闭原测试临时文档，恢复原活动文档；未知请求、会话或目标变化时不执行。')}</p></Show>
      <div class="buttons"><button class="primary" disabled={disabled() || test().active || state().photoshop.busy || !state().photoshop.uxp_ready || state().menu?.visible} onClick={start}>{t('一键开始测试')}</button><button disabled={disabled() || !test().active || test().stop_requested} onClick={stop}>{t('停止后续测试')}</button><button disabled={disabled() || !test().ready} onClick={download}>{t('导出测试报告')}</button></div>
      <p class="caption">{t('自动保存本地报告，记录每步耗时、逐核CPU、频率、内存及磁盘等待；不改变电源计划，不上传。停止会等待当前请求结束，未知结果保留保护。')}</p>
      <p class="caption">{t('未支持的实时预览单独标记；实体Caps、UU远程、鼠标手感及画布目视不计为自动通过。')}</p>
    </section>;
  }
  async function exportDiagnostics(){try{const report=await call('diagnostics.export');const url=URL.createObjectURL(new Blob([JSON.stringify(report,null,2)],{type:'application/json;charset=utf-8'}));const link=document.createElement('a');link.href=url;link.download='ps-menu-diagnostic-'+Date.now()+'.json';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}catch{}}
  const health=()=>healthView(state().version_health);
  async function exportHealth(){try{const report=await call('version.health.export');const url=URL.createObjectURL(new Blob([JSON.stringify(report,null,2)],{type:'application/json;charset=utf-8'}));const link=document.createElement('a');link.href=url;link.download='ps-menu-version-check-'+Date.now()+'.json';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}catch{}}
  function VersionHealthPanel(){return <section id="version-health"><div class="section-heading"><h2>{t('版本变化自动自检')}</h2><span classList={{'status-pill':true,warn:state().version_health?.attention,good:health().ready && !state().version_health?.attention}}>{t(health().message)}</span></div>
    <p>{t('仅检查版本、连接和接口；不代表文档操作或低配性能已验收。')}</p>
    <Show when={health().ready}><p>Photoshop {health().versions.photoshop || '—'} · UXP {health().versions.loaded_uxp || '—'} · {t('应用')} {health().versions.application || '—'}</p><For each={health().findings}>{finding=><p>{t(healthFinding(finding.code))}</p>}</For><p class="caption">{t('上次检查：')}{new Date(state().version_health.checked_at*1000).toLocaleString(state().language || 'zh-CN')}</p></Show>
    <div class="buttons"><button disabled={disabled() || !health().ready} onClick={exportHealth}>{t('导出自检报告')}</button><Show when={health().alert}><button disabled={disabled()} onClick={()=>perform('version.health.acknowledge')}>{t('标为已读')}</button></Show></div>
    <p class="caption">{t('版本变化和异常稳定出现后自动留证，最多保留20份；不写文档、不切换焦点、不上传数据。')}</p></section>;}
  function UxpFollow(){return <><label class="option-row"><input type="checkbox" checked={state().uxp_auto_update?.enabled ?? true} disabled={disabled()} onChange={e=>perform('uxp.auto',{enabled:e.target.checked})}/><span>{t("已安装UXP自动跟随内置版本更新")}</span></label><p role="status">{t(state().uxp_auto_update?.message || '等待检查内置 UXP')}</p><p class="caption">{t("保存并关闭PS后自动安装较新内置版，不自动关闭PS。禁用、卸载、回退和未知结果保护优先。")}</p></>;}
  function PhotoshopStartup(){return <>
    <p>{t(state().photoshop_start?.message) || t("等待启动状态")}<Show when={state().photoshop_start?.active}> · {state().photoshop_start?.elapsed_seconds || 0} {t("秒")}</Show></p>
    <p>{t("打开后台或安装／更新插件后，请手动启动 Photoshop。")}</p>
  </>;}
  const managedBusy=()=>disabled() || state().photoshop.busy || state().photoshop_start?.active;
  function Overview(){return <div class="panel-scroll overview-grid">
    <section class="connection-card"><div class="section-heading"><h2>{t("Photoshop 连接")}</h2><span classList={{'status-pill':true,good:state().photoshop.connected && state().photoshop.uxp_ready && !state().photoshop.busy,warn:state().photoshop.busy}}>{!state().photoshop.connected?t("未连接"):state().photoshop.busy?t("操作进行中"):state().photoshop.uxp_ready?t("已就绪"):t("等待插件加载")}</span></div>
    <strong class="document-name" title={state().photoshop.document}>{state().photoshop.document || (state().photoshop.connected?t("Photoshop 首页"):t("请启动 Photoshop"))}</strong>
    <p class="connection-caption">{state().photoshop.connected?t("激活 Photoshop 后使用菜单快捷键"):t(state().photoshop.message) || t("检查中…")} <Show when={state().photoshop.connected}><kbd>{state().settings.shortcuts?.toggle || 'CapsLock'}</kbd></Show></p>
    <div class="status-grid"><div><span>Photoshop</span><strong>{state().photoshop.photoshop_version || '—'}</strong></div><div><span>{t("执行插件")}</span><strong>{state().photoshop.uxp_version ? 'v'+state().photoshop.uxp_version : '—'}</strong></div><div><span>{t("操作菜单")}</span><strong>{!state().settings.enabled?t("已暂停"):state().menu?.ready?t("已启用"):t("准备中")}</strong></div></div>
    <UxpRecovery/>
    <Show when={!state().photoshop.status_fresh}><p class="error">{t("状态等待更新 · 显示缓存")}</p></Show>
    <div class="buttons"><Show when={!state().photoshop.connected || state().photoshop_start?.active}><button class="primary" disabled={managedBusy()} onClick={()=>perform('photoshop.start')}>{state().photoshop_start?.active?t("正在启动…"):t("启动 Photoshop")}</button></Show><button disabled={disabled()} onClick={()=>perform('status',{refresh:true})}>{t("刷新连接状态")}</button><button onClick={()=>choose('settings')}>{t("菜单设置")}</button><button onClick={()=>choose('plugins')}>{t("插件管理")}</button></div>
    </section>
    <VersionHealthPanel/><InputHealthPanel/><StabilityPanel/><DiagnosticPanel/><section class="compact-section"><details><summary>{t("启动状态")}</summary><PhotoshopStartup/></details></section>
    <section class="compact-section"><details><summary>{t("连接详情")}</summary><dl class="info-list"><div><dt>{t("当前面板：")}</dt><dd>{t(state().menu?.panel_context?.label) || t("通用操作")}</dd></div><div><dt>{t("上次读取：")}</dt><dd>{state().photoshop.status_updated_at ? new Date(state().photoshop.status_updated_at).toLocaleTimeString(state().language || 'zh-CN') : '—'}</dd></div><div><dt>{t("当前运行版本")}</dt><dd>v{state().version || '—'}</dd></div><div><dt>{t("程序：")}</dt><dd>{state().service?.executable || t("此服务未提供程序路径")}</dd></div></dl><p>{t(state().desktop)}</p><Show when={state().tray_error}><p class="error">{t("托盘：")}{state().tray_error}</p></Show><button class="danger" disabled={managedBusy()} onClick={()=>perform('service.quit')}>{t("退出后台服务")}</button></details></section>
  </div>;}
  function UxpInstalled(){return <div class="panel-scroll"><section>
    <div class="section-heading"><h2>{t("UXP 插件管理")}</h2><span classList={{'status-pill':true,good:state().uxp?.installed && !state().uxp?.error && state().uxp?.enabled,warn:!!state().uxp?.error}}>{!state().uxp?.installed?t("未安装"):state().uxp?.error?t("需要检查"):state().uxp?.enabled?t("已启用"):t("已暂停")}</span></div>
    <strong class="plugin-name">{state().uxp?.name || state().uxp?.desired_name}</strong>
    <p>{!state().uxp?.installed || state().uxp?.error?t('尚未安装可用插件'):!state().uxp?.enabled?t('新菜单／快捷键操作已停止接受'):state().photoshop.connected && state().photoshop.uxp_ready && !state().photoshop_start?.active?t('Photoshop 已加载，可执行操作'):t('已安装，等待 Photoshop 加载')}</p>
    <div class="status-grid"><div><span>{t("安装版本")}</span><strong>{state().uxp?.version || '—'}</strong></div><div><span>{t("内置版本")}</span><strong>{state().uxp?.bundled_version || '—'}</strong></div><div><span>{t("最低 Photoshop")}</span><strong>{state().uxp?.minimum_photoshop || '24.0.1'}</strong></div></div>
    <Show when={state().uxp?.error}><p class="error">{t(state().uxp.error)}</p></Show>
    <div class="inline-notice"><p>{t('PS 加载登记：')}{t(({directory:'使用 External 目录扫描',registered:'已登记到用户索引',missing:'用户索引缺少本插件',disabled:'本插件加载登记已禁用',outdated:'本插件加载登记待同步',invalid:'加载索引需要检查'})[state().uxp?.registration?.state] || '检查中…')}</p><Show when={state().uxp?.registration?.error}><p class="error">{t(state().uxp.registration.error)}</p></Show><Show when={state().uxp?.installed && state().uxp?.registration?.needs_repair}><p>{t('保存并关闭 PS 后修复，只维护本插件条目，保留其他插件；再次启动后核对连接。')}</p><button disabled={managedBusy() || !!state().uxp?.error} onClick={()=>perform('uxp.repair-registration')}>{t('修复并启用加载登记')}</button></Show></div>
    <div class="buttons"><Show when={!state().uxp?.installed || !state().uxp?.matches_builtin}><button class="primary" disabled={managedBusy()} onClick={()=>installUxp(null)}>{state().uxp?.installed?t("安装／更新为内置版本"):t("安装内置插件")}</button></Show><Show when={state().uxp?.installed}><button disabled={managedBusy() || !!state().uxp?.error} onClick={()=>perform(state().uxp?.enabled?'uxp.disable':'uxp.enable')}>{state().uxp?.enabled?t("禁用执行"):t("启用执行")}</button></Show><button onClick={()=>selectTab('updates')}>{t("离线更新包")}</button></div>
    <Show when={state().uxp?.language_pending}><div class="inline-notice"><p>{t('PS 插件名称待应用')} · {state().uxp?.desired_name}</p><p>{t('关闭 Photoshop 后点击应用，不会自动关闭或丢弃文档。')}</p><button disabled={managedBusy() || !!state().uxp?.error} onClick={()=>perform('uxp.apply-language')}>{t('应用插件名称语言')}</button></div></Show>
    <UxpFollow/><details class="help"><summary>{t("安装说明")}</summary><p>{t("主程序自带离线插件，安装、更新和回退均在此管理，无需单独安装器或 Adobe 工具。安装／更换／卸载前请先保存并正常关闭 Photoshop。已有用户索引时同步本插件登记，没有索引时保留目录扫描；重启 PS 后以实际连接为准。")}</p><p>{t("禁用立即生效并保留插件文件；已加载的插件仍驻留 PS，卸载才会移除。运行中的任务结束前不能禁用或更换。")}</p></details>
    <details class="help"><summary>{t("安装详情与维护")}</summary><p>{t('安装检测：')}{t(({missing:'未安装',invalid:'安装不完整或凭据异常',installed:'文件与凭据校验通过'})[state().uxp?.installation] || '检查中…')}</p><p>{t('检测目录：')}{state().uxp?.plugin_dir || '—'}</p><div class="buttons"><button disabled={managedBusy()} onClick={()=>installUxp(null)}>{t("安装／更新为内置版本")}</button><button disabled={managedBusy() || !state().uxp?.can_rollback} onClick={()=>perform('uxp.rollback')}>{t("回退上次安装")}</button><button class="danger" disabled={managedBusy() || !state().uxp?.installed} onClick={()=>perform('uxp.uninstall')}>{t("卸载 UXP 插件")}</button></div><label class="advanced-field">{t("Photoshop.exe 完整路径")}<input value={photoshopExe()} placeholder={t("留空自动检测")} onInput={e=>setPhotoshopExe(e.target.value)}/></label></details>
  </section></div>;}
  function ShortcutPicker(props){
    const value=createMemo(()=>form().shortcuts[props.name] || '');
    const parts=createMemo(()=>shortcutParts(value()));
    const required=()=>['toggle','hide'].includes(props.name);
    const issue=()=>shortcutIssues()[props.name];
    return <div class="shortcut-field" data-shortcut={props.name}>
      <span class="shortcut-title">{t(props.label)}</span>
      <div class="shortcut-controls">
        <select aria-label={`${t(props.label)} · ${t('组合键')}`} aria-invalid={!!issue()} disabled={disabled()} value={parts().prefix} onChange={e=>shortcut(props.name,changePrefix(value(),e.target.value))}>
          <For each={prefixOptions}>{option=><option value={option.value}>{t(option.label)}</option>}</For>
        </select>
        <select aria-label={`${t(props.label)} · ${t('按键')}`} aria-invalid={!!issue()} disabled={disabled()} value={parts().key} onChange={e=>shortcut(props.name,changeTrigger(value(),e.target.value))}>
          <option value="">{t(required() || parts().prefix?'请选择按键':'未绑定')}</option>
          <For each={triggerKeys}>{key=><option value={key} disabled={!triggerAllowed(parts().prefix,key)}>{keyLabel(key)}</option>}</For>
        </select>
      </div>
      <Show when={issue()}><p class="field-error" role="alert">{t(issue())}</p></Show>
    </div>;
  }
  function KeySettings(){return <div class="panel-scroll"><section><h2>{t("菜单快捷键")}</h2><div class="checks"><label><input type="checkbox" checked={form().remote_compatibility} onChange={e=>edit("remote_compatibility",e.target.checked)}/>{t("远程兼容（默认关闭）")}</label></div><p class="caption">{t("通过向日葵等远程软件操作时可开启。保存后先在PS工作区内点击一次，再按菜单快捷键；不增加任务栏、Alt+Tab或悬停激活，输入框和弹窗保护保留。")}</p><div class="fields shortcut-fields"><For each={Object.entries(keyLabels).filter(([key])=>['toggle','hide'].includes(key))}>{([key,label])=><ShortcutPicker name={key} label={label}/>}</For></div><details class="help"><summary>{t("使用说明")}</summary><p>{t("先选组合键，再选按键；图层快捷键可选择未绑定。唤出与收起必须保留，重复或未完成的组合不能保存。")}</p></details></section><section><h2>{t("图层操作快捷键")}</h2><div class="fields shortcut-fields"><For each={Object.entries(keyLabels).filter(([key])=>!['toggle','hide'].includes(key))}>{([key,label])=><ShortcutPicker name={key} label={label}/>}</For></div></section></div>;}
  async function installUxp(token){try{await call('uxp.install',{token,photoshop_exe:photoshopExe()});setUxpPreview(null);}catch{}}
  function exportTasks() {
    const report={exported_at:new Date().toISOString(),version:state().version,connection:connection(),service:state().service,tasks:state().tasks};
    const url=URL.createObjectURL(new Blob([JSON.stringify(report,null,2)],{type:'application/json;charset=utf-8'}));
    const link=document.createElement('a');link.href=url;link.download='photoshop-menu-tasks-'+new Date().toISOString().replace(/[:.]/g,'-')+'.json';
    link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
  }

  const [subpages,setSubpages]=createSignal({settings:'behavior',plugins:'installed'});
  const [taskId,setTaskId]=createSignal(null);
  const selectedTask=()=>state().tasks?.items?.find(task=>task.id===taskId()) || state().tasks?.items?.[0];
  const currentTab=()=>subpages()[page()];
  const tabItems=()=>({settings:[['behavior','常用'],['keys','快捷键'],['advanced','高级']],plugins:[['installed','安装与状态'],['updates','离线更新包']]})[page()] || [];
  function selectTab(id){setSubpages(previous=>({...previous,[page()]:id}));}
  function tabKeys(event){
    const ids=tabItems().map(([id])=>id),index=ids.indexOf(currentTab());
    const next=event.key==='ArrowRight'?(index+1)%ids.length:event.key==='ArrowLeft'?(index+ids.length-1)%ids.length:event.key==='Home'?0:event.key==='End'?ids.length-1:-1;
    if(next<0)return;event.preventDefault();selectTab(ids[next]);event.currentTarget.querySelectorAll('[role=tab]')[next].focus();
  }
  function SaveBar(){return <Show when={dirty()}><footer class="save-bar"><span>{invalidShortcuts()?t("请先修正快捷键设置"):dirty()?t("有未保存的修改"):t("设置已同步")}</span><div class="buttons"><button class="primary" disabled={disabled() || !dirty() || invalidShortcuts()} onClick={save}>{t("保存并立即生效")}</button><button disabled={!dirty()} onClick={()=>{setForm(structuredClone(state().settings));setDirty(false);}}>{t("放弃修改")}</button></div></footer></Show>;}
  function choose(name){setPage(name);if(name==='logs')readLogs();}
  onMount(()=>client.connect());onCleanup(()=>client.close());
  return <div class="shell">
    <aside><div class="brand"><img src="/icon.png?v=ps-menu-hand-3" alt=""/><div><strong>151500px-menu</strong><small>{t("后台管理")}</small></div></div>
      <label class="language-switch"><span>{t("语言 / Language")}</span><select aria-label="语言 / Language" value={state().language || 'zh-CN'} disabled={disabled()} onChange={e=>perform('language.set',{language:e.target.value})}><option value="zh-CN">{t("简体中文")}</option><option value="en">English</option></select></label>
      <nav aria-label={t("主导航")}><For each={pages}>{([name,label])=><button aria-current={page()===name?'page':undefined} classList={{selected:page()===name}} onClick={()=>choose(name)}>{t(label)}</button>}</For></nav>
      <div class="aside-note">{t("仅在 Photoshop 内唤出")}</div><div class="version">{state().version ? `${t('应用 v'+state().version)}${connection()==='connected'?'':t("（上次连接）")}` : t("应用版本待确认")}</div>
    </aside>
    <main><header><div><h1>{t(pages.find(([name])=>name===page())?.[1])}</h1><p>{t(({overview:'连接状态和常用操作',settings:'调整菜单行为与快捷键',plugins:'管理 Photoshop 执行插件',updates:'检查与安装正式版本',tasks:'查看操作结果与耗时',logs:'按需查看服务诊断'})[page()])}</p></div><div class="header-status"><span class="version-tag">{state().version ? 'v'+state().version : '—'}</span><span classList={{badge:true,online:connection()==='connected'}}>{connection()==='connected'?t("后台在线"):connection()==='connecting'?t("正在连接"):t("已断开 · 自动重连")}</span></div></header>
      <Show when={notice().message}><div classList={{notice:true,error:!notice().ok}} role="status"><span>{t(notice().message)}</span><button class="dismiss" aria-label={t("关闭提示")} onClick={()=>setNotice({message:'',ok:true})}>×</button></div></Show>
      <Show when={health().alert}><div class="notice" role="status"><span>{t(health().message)}</span><button disabled={disabled()} onClick={()=>{choose('overview');queueMicrotask(()=>document.getElementById('version-health')?.scrollIntoView({block:'nearest'}));}}>{t('查看自检报告')}</button><button disabled={disabled()} onClick={()=>perform('version.health.acknowledge')}>{t('标为已读')}</button></div></Show>
      <Show when={tabItems().length}><div class="subtabs" role="tablist" aria-label={t("页面选项")} onKeyDown={tabKeys}><For each={tabItems()}>{([id,label])=><button id={'tab-'+page()+'-'+id} role="tab" aria-selected={currentTab()===id} aria-controls="active-panel" tabIndex={currentTab()===id?0:-1} classList={{selected:currentTab()===id}} onClick={()=>selectTab(id)}>{t(label)}</button>}</For></div></Show>
      <div id="active-panel" class="workspace" role={tabItems().length?'tabpanel':undefined} aria-labelledby={tabItems().length?'tab-'+page()+'-'+currentTab():undefined}>
<Show when={page()==='overview'}><Overview/></Show>
<Show when={page()==='updates'}><div class="panel-scroll"><ApplicationUpdate/></div></Show>
<Show when={page()==='settings' && form() && currentTab()==='behavior'}><div class="panel-scroll"><section><h2>{t("菜单行为")}</h2><div class="checks"><label><input type="checkbox" checked={form().enabled} onChange={e=>edit('enabled',e.target.checked)}/>{t("启用 Photoshop 操作菜单与快捷键")}</label><label><input type="checkbox" checked={form().right_click_hide} onChange={e=>edit('right_click_hide',e.target.checked)}/>{t("在菜单内右键收起")}</label><label><input type="checkbox" checked={form().open_control_on_start} onChange={e=>edit('open_control_on_start',e.target.checked)}/>{t("启动时打开后台管理窗口")}</label></div></section>
  <section><h2>{t("渐变画布叠加")}</h2>
    <div class="checks"><label><input type="checkbox" checked={!!form().gradient_layer_tint} onChange={e=>edit('gradient_layer_tint',e.target.checked?'#58c778':'')}/>{t("高亮正在编辑的图层行")}</label></div>
    <div class="fields"><label>{t("图层高亮颜色")}<input type="color" value={form().gradient_layer_tint || '#58c778'} disabled={!form().gradient_layer_tint} onInput={e=>edit('gradient_layer_tint',e.target.value)}/></label></div>
  </section></div></Show>
<Show when={page()==='settings' && form() && currentTab()==='advanced'}><div class="panel-scroll"><section><h2>{t("渲染与兼容")}</h2><div class="fields"><label>{t("菜单渲染")}<select value={form().render_mode || "auto"} onChange={e=>edit("render_mode",e.target.value)}><option value="auto">{t("自动选择（推荐）")}</option><option value="gpu">{t("GPU（优先硬件）")}</option><option value="cpu">{t("CPU（软件兼容）")}</option></select></label></div><p>{t("自动优先使用GPU，硬件不可用时回退CPU。保存后在菜单收起时切换，不重启PS。")}</p><p>{t("当前渲染：")}{state().menu?.graphics?.active?.toUpperCase() || "—"} {state().menu?.graphics?.adapter || ""}</p></section><section><details class="help"><summary>{t("兼容面板尺寸")}</summary><div class="fields"><label>{t("菜单宽度")}<input type="number" min="300" max="1200" value={form().menu_width} onInput={e=>edit('menu_width',Number(e.target.value))}/></label><label>{t("菜单高度")}<input type="number" min="240" max="1000" value={form().menu_height} onInput={e=>edit('menu_height',Number(e.target.value))}/></label></div><p>{t("仅用于兼容面板；Caps 环绕菜单根据控件自动布局，不受这里的宽高影响。")}</p></details></section><Show when={state().photoshop.execution_backend!=='uxp-local-files'}><section><label>{t("更新清单 HTTPS 地址")}<input type="url" placeholder="https://…/updates.json" value={form().update_url} onInput={e=>edit('update_url',e.target.value)}/></label></section></Show><section><h2>{t("输入保护")}</h2><p>{t("在 Photoshop 前台临时禁用输入法，避免单字母快捷键被输入法吞掉；进入文字输入时恢复。")}</p><details class="help"><summary>{t("诊断详情")}</summary><p>{t("当前通道：")}{state().input_service?.channel==='tsf'?t("系统 TSF 服务"):t("进程内注入")}</p><p class="caption">{t("本机不使用系统 TSF 服务：它只有被选为输入法时才会加载，而那会替换你的中文输入法。输入法保护使用进程内通道。")}</p><Show when={state().input_service?.registered}><p class="caption">{t("检测到已注册的 TSF 服务；它不会被自动加载，可以卸载。")}</p><div class="buttons"><button disabled={disabled()} onClick={()=>perform('input.service.remove')}>{t("卸载输入法服务（管理员）")}</button></div></Show></details></section></div></Show>
<Show when={page()==='settings' && form() && currentTab()==='keys'}><KeySettings/></Show>
<Show when={page()==='plugins' && state().photoshop.execution_backend==='uxp-local-files' && currentTab()==='installed'}><UxpInstalled/></Show>
<Show when={page()==='plugins' && state().photoshop.execution_backend==='uxp-local-files' && currentTab()==='updates'}><div class="panel-scroll"><section><h2>{t("离线更新包")}</h2><UxpFollow/><details class="help"><summary>{t("使用说明")}</summary><p>{t("导入 151500px-menu UXP ZIP，校验版本、兼容协议、文件清单和 SHA256 后安装。凭据只在本机生成，导出包不含个人设置或令牌。")}</p></details><div class="buttons"><ZipUpload label="导入 UXP ZIP" onChange={uploadUxp}/><a class="button" href="/api/uxp/export">{t("导出 UXP 更新包")}</a><button disabled={disabled()} onClick={()=>perform('status')}>{t("刷新状态")}</button></div><Show when={uxpPreview()}><div class="preview"><h3>{t("待安装 v")}{uxpPreview().manifest.version}</h3><p>{t("请正常关闭 Photoshop，后台保持运行，再安装。")}</p><div class="buttons"><button class="primary" disabled={disabled() || state().photoshop.busy || state().photoshop_start?.active} onClick={()=>installUxp(uxpPreview().token)}>{t("安装此更新包并启用")}</button><button onClick={()=>setUxpPreview(null)}>{t("取消预览")}</button></div></div></Show></section></div></Show>
<Show when={page()==='plugins' && state().photoshop.execution_backend!=='uxp-local-files' && currentTab()==='installed'}><div class="panel-scroll"><section><h2>{t("Photoshop 图层脚本包")}</h2><div class="package"><img src="/icon.png" alt=""/><div><strong>{state().plugin.active.name || t("151500px-menu 图层操作")}</strong><p>{t("当前版本")} {state().plugin.active.version} · {state().plugin.installed?t("已安装升级包"):t("随应用提供的内置版本")}</p></div></div><p>{t("管理 COM 调用的 JSX 脚本包。更换脚本后即时生效，Dear ImGui 菜单保持不变。")}</p><div class="buttons"><ZipUpload label="选择 ZIP 升级包" onChange={upload}/><a class="button" href="/api/plugins/export">{t("导出当前脚本包")}</a><button disabled={disabled() || !state().plugin.can_rollback || state().photoshop.busy} onClick={()=>perform('plugin.rollback')}>{t("回滚上一版本")}</button><button disabled={disabled() || !state().plugin.installed || state().photoshop.busy} onClick={()=>perform('plugin.uninstall')}>{t("卸载升级包，恢复内置版")}</button></div></section></div></Show>
<Show when={page()==='plugins' && state().photoshop.execution_backend!=='uxp-local-files' && currentTab()==='updates'}><div class="panel-scroll"><Show when={preview()}><section class="preview"><h2>{t("待安装脚本包")}</h2><strong>{preview().manifest.name || preview().manifest.id} · v{preview().manifest.version}</strong><p>{t("文件清单、版本和 SHA256 已校验。安装期间不会执行 Photoshop 脚本。")}</p><div class="buttons"><button class="primary" disabled={disabled() || state().photoshop.busy} onClick={install}>{t("安装并启用")}</button><button onClick={()=>setPreview(null)}>{t("关闭预览")}</button></div></section></Show><section><h2>{t("在线更新")}</h2><p>{state().settings.update_url || t("尚未配置更新清单地址。可在菜单与快捷键设置中填写。")}</p><div class="buttons"><button disabled={disabled() || !state().settings.update_url} onClick={check}>{t("检查更新")}</button><Show when={update()}><span>{update().available?t(`发现 v${update().version}`):t("当前已是更新源中的最新版本")}</span><button disabled={disabled() || !update()?.available} onClick={download}>{t("下载并预览")}</button></Show></div></section></div></Show>
<Show when={page()==='tasks'}><section class="task-workspace"><div class="section-heading"><div><h2>{t("最近任务")}</h2><p>{t("保留最近 64 次操作，点击一项查看详情。")}</p></div><button disabled={!state().tasks?.items?.length} onClick={exportTasks}>{t("导出任务诊断")}</button></div>
<UxpRecovery/><details class="help"><summary>{t("任务规则与超时说明")}</summary><p>{t("保留最近 64 次操作，重启后仍可查看。超时表示结果尚未确定；后台收到最终结果后会更新同一任务。忙碌时不接受新任务、不自动排队；读取目标时可取消；已提交的操作等待 Photoshop 返回结果，不自动重试。重启时未结束的历史任务不会自动重跑。")}</p></details><Show when={state().tasks?.persistence_error}><p class="error">{state().tasks.persistence_error}</p></Show>
<Show when={state().tasks?.items?.length} fallback={<div class="empty">{t("尚无任务记录。")}</div>}><div class="task-split"><div class="task-list" aria-label={t("任务列表")}><For each={state().tasks?.items}>{task=><button classList={{'task-row':true,selected:selectedTask()?.id===task.id}} aria-pressed={selectedTask()?.id===task.id} onClick={()=>setTaskId(task.id)}><strong>{t(task.label)}</strong><span class="task-state">{t(task.state_label)} · {(task.elapsed_ms/1000).toFixed(2)} {t("秒")}</span><small>{new Date(task.created_at).toLocaleTimeString(state().language || 'zh-CN')}</small></button>}</For></div><div class="task-detail" aria-label={t("任务详情")}><Show when={selectedTask()}>{task=><><h3>{t(task().label)}</h3><span class="badge">{t(task().state_label)}</span><p>{t("开始时间：")}{new Date(task().created_at).toLocaleString(state().language || 'zh-CN')}</p><details class="help"><summary>{t("任务诊断详情")}</summary><p>{t("任务 ID：")}{task().id}</p><p>{t("· 通道：")}{task().channel}</p></details><Show when={task().timeline?.length} fallback={<p>{t("旧版本记录未采集阶段耗时。")}</p>}><details class="help"><summary>{t("耗时说明")}</summary><p>{t("阶段耗时按后台状态切换记录，包含通讯等待；不是脚本内部进度。历史任务仅显示退出前已保存的数据。")}</p></details><table><thead><tr><th>{t("阶段")}</th><th>{t("开始于")}</th><th>{t("耗时")}</th></tr></thead><tbody><For each={task().timeline}>{phase=><tr><td>{t(phase.state_label)}{phase.active?t(" · 进行中"):''}</td><td>{(phase.start_elapsed_ms/1000).toFixed(2)} {t("秒")}</td><td>{(phase.duration_ms/1000).toFixed(2)} {t("秒")}</td></tr>}</For></tbody></table></Show><Show when={task().state==='queued'}><p>{t("等待前一项操作，最多")} {task().queue_timeout_seconds} {t("秒；执行前重新核验原文档与图层。")}</p></Show><Show when={task().state==='capturing' && !task().restored}><button disabled={disabled()} onClick={()=>perform('tasks.cancel',{id:task().id})}>{t("取消尚未提交的任务")}</button></Show><Show when={task().state==='cancel-requested'}><p>{t("等待目标读取结束，不会提交修改。")}</p></Show><Show when={task().restored}><p>{t("上次服务的历史记录 · 只读，不自动重跑")}</p></Show><Show when={task().target}><p>{t("目标文档 ID：")}{task().target.document_id} {t("· 选中")} {task().target.layer_ids?.length} {t("个图层")}</p></Show><Show when={t(task().message)}><p>{t(task().message)}</p></Show><Show when={t(task().error)}><p class="error">{t(task().error)}</p></Show><Show when={task().wait_error && task().state!=='result-unknown'}><p>{t("曾停止等待：")}{t(task().wait_error)}</p></Show></>}</Show></div></div></Show></section></Show>
<Show when={page()==='logs'}><section class="log-workspace"><div class="section-heading"><h2>{t("最近 200 行服务日志")}</h2><button disabled={disabled()} onClick={readLogs}>{t("刷新日志")}</button></div><pre>{logs() || t("暂无日志")}</pre><p>{t("应用数据目录：")}{state().data_dir || '.runtime'}{t("。原生菜单的启动诊断另存于该目录的 overlay.log。")}</p></section></Show>
</div>
<Show when={form()}><SaveBar/></Show>
    </main>
  </div>;
}
render(()=><App/>,document.getElementById('root'));
