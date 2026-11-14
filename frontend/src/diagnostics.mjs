// Local presentation clock; never polls PS or changes diagnostic state.
export const diagnosticScopes=[['all','综合检测（推荐）'],['menu','菜单与快捷键'],['uxp','UXP安装与加载'],['runtime','运行与图层读取']];
export const diagnosticComponents={menu:'菜单与快捷键',uxp:'UXP安装与加载',runtime:'运行与图层读取',input:'Hook与输入保护'};
const findings={uxp_loaded:'UXP已连接',uxp_files_missing:'未找到当前用户的UXP插件文件',uxp_current_host_logs_unavailable:'未取得当前PS启动后的加载日志，暂不能判断扫描路径',uxp_user_external_scan_not_observed:'当前启动日志未记录用户External目录扫描；这是排查线索，尚非故障根因',uxp_load_unconfirmed:'插件文件存在，但PS加载尚未确认'};
Object.assign(findings,{uxp_registration_missing:'用户加载索引缺少本插件，请保存并关闭PS后修复加载登记',uxp_registration_disabled:'本插件加载登记已禁用，可在插件管理中主动修复并启用',uxp_registration_invalid:'加载索引异常或路径冲突，已保留原文件，请在插件管理中检查',uxp_registration_outdated:'本插件加载登记待同步，请保存并关闭PS后修复'});
export function diagnosticFinding(code){return findings[code] || '检测结果待核对';}
export function diagnosticClock(value={},now=0){
  const remaining=Math.max(0,(value.remaining_ms || 0)-Math.max(0,now-(value.received_at ?? now)));
  const seconds=Math.ceil(remaining/1000);
  let label=value.message || '尚未采集诊断';
  if(value.phase==='collecting' && !seconds && value.stage==='prepare')label='正在开始采集，请保持PS前台';
  if(value.phase==='collecting' && !seconds && value.stage==='sampling')label='正在等待采集结束，请保持PS前台';
  return {label,seconds,showSeconds:value.phase==='collecting' && seconds>0};
}

export const keyDecisions={'action-emitted':'原生动作已发出（不代表执行成功）',disabled:'快捷键已暂停','outside-photoshop':'前台未识别为Photoshop','activation-rejected':'PS激活条件未满足','workspace-rejected':'工作区范围未通过','modified-key':'存在组合键','input-protected':'输入保护未放行','focus-changed':'焦点版本不匹配','no-action':'按键已收到，未匹配动作'};
export const toolReadReasons={'transport-unavailable':'工具读取通道不可用','invalid-host-identity':'PS进程身份无效','previous-host-heartbeat':'执行插件尚未属于当前PS','executor-too-old':'执行插件版本不支持工具读取','pending-invalid':'原工具观察记录损坏','pending-changed':'原工具观察身份已变化','publication-failed':'工具读取未能发布','pending-session-changed':'原工具观察属于旧会话','response-too-large':'工具返回记录超限','response-invalid':'工具返回终态无效','response-time-invalid':'工具返回时间无效','response-fields-invalid':'工具返回字段无效','request-unconfirmed':'原工具读取尚未确认','expired-before-read':'工具读取在宿主执行前已过期','unsupported-tool-state':'PS返回的工具字段不兼容','host-tool-failed':'PS工具接口读取失败','tool-state-stale':'工具返回状态已失鲜','retirement-blocked':'旧工具观察归档或退出核验未通过'};
export function inputHealthRows(value={}){
 const focus=value.focus || {},ime=value.ime || {},health=value.health || {},config=value.configuration || {};
 const observer=value.tool_observation || {};
 const stateNames={unavailable:'桌面组件不可用',stopped:'热键进程已停止',starting:'等待组件就绪','awaiting-health':'等待首次健康上报',stale:'健康上报已过期','maintenance-failed':'Hook维护失败',reporting:'健康上报持续更新'};
 const focusNames={'non-input':'已确认非输入区',input:'输入区，快捷键让行',unknown:'未确认，保护中',outside:'Photoshop不在前台'};
 const imeNames={...focusNames,'legacy-layout-recovery':'旧布局请求待恢复','journal-invalid':'恢复记录无法读取','request-not-confirmed':'请求尚未确认','request-rejected':'请求已拒绝'};
 const lastKey=key=>[...(value.events || [])].reverse().find(e=>e.key===key && (e.message===256 || e.message===260));
 const decision=event=>event?(keyDecisions[event.decision] || '拦截结果待核对'):'尚无本轮按键记录';
 const yes=value=>value===true?'是':value===false?'否':'未确认';
 return [
  ['Hook状态',stateNames[value.state] || '等待状态'],
  ['热键进程 PID',value.pid ?? '—'],
  ['最近健康上报',value.health_age_ms==null?'尚未收到':`${value.health_age_ms} ms`],
  ['配置已生效',yes(config.acknowledged)],
  ['Hook重新挂载',health.renew_attempted===false?'本轮未尝试':yes(health.renew_ok)],
  ['事件丢失累计',health.dropped_total ?? '—'],
  ...(health.keyboard_peak_us==null ? [['回调峰值（微秒）',health.callback_peak_us ?? '—']] : [
    ['键盘回调峰值（微秒）',health.keyboard_peak_us],
    ['鼠标回调峰值（微秒）',health.pointer_peak_us ?? '—'],
    ['后续 Hook 峰值（微秒）',health.next_hook_peak_us ?? '—']
  ]),
  ['最近菜单键',decision(lastKey(20) || [...(value.events || [])].reverse().find(e=>e.type==='trigger.trace' && (e.message===256 || e.message===260)))],
  ['最近 F',decision(lastKey(70))],
  ['焦点保护',focusNames[focus.state] || '等待状态'],
  ['焦点识别 UIA',yes(focus.uia_available)],
  ['当前工具',focus.tool || '未取得'],
  ['工具状态读取',observer.state==='ready'?'已取得工具状态':toolReadReasons[observer.reason] || '等待状态'],
  ['IME状态',imeNames[ime.state] || ime.state || '未取得'],
  ['当前IME请求待核对',yes(ime.pending || false)],
  ['旧布局未知记录',ime.legacy_pending?(ime.legacy_scope==='original-process-exited'?'历史未知已保留（原PS已退出）':'待原会话恢复'):'无'],
  ['已确认禁用／恢复',`${ime.confirmed_disables ?? 0} / ${ime.confirmed_restores ?? 0}`],
  ['按键诊断采集中',yes(value.trace_active)]
 ];
}
