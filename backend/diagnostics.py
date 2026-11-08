"""Opt-in, bounded read-only diagnostics. No Photoshop requests or input."""
import asyncio,json,os,uuid,time,math
from pathlib import Path

SCOPES={'all','menu','uxp','runtime'}

def pick(source,keys):
    return {k:source[k] for k in keys if k in source and isinstance(source[k],(str,int,float,bool,type(None)))}

def input_report(desktop):
    """Cached evidence only; a live process or heartbeat does not prove key delivery."""
    if not desktop:return {'state':'unavailable','read_only':True}
    native=desktop.native
    running=bool(native and native.poll() is None)
    health=getattr(desktop,'hook_health',None)
    age=max(0,round((time.monotonic()-health[0])*1000)) if health else None
    state=('stopped' if not running else 'starting' if not desktop.ready.is_set() else
           'awaiting-health' if health is None else 'stale' if age>3500 else
           'maintenance-failed' if health[1].get('renew_ok') is False else 'reporting')
    focus=dict(getattr(getattr(desktop,'input_focus',None),'report',{}))
    ime=dict(getattr(getattr(desktop,'input_language',None),'report',{}))
    observation=focus.get('observation') or {}
    return {'read_only':True,'state':state,'process_running':running,'pid':native.pid if native else None,
        'generation':getattr(desktop,'hook_generation',0),'health_age_ms':age,
        'health':pick(health[1] if health else {},('scan_us','renew_us','timer_gap_ms','callback_peak_us','dropped_total','renew_ok','renew_attempted','input_epoch')),
        'configuration':{'sent':desktop.native_sequence,'applied':desktop.native_applied_sequence,
                         'acknowledged':running and desktop.native_sequence>0 and desktop.native_applied_sequence>=desktop.native_sequence},
        'trace_active':bool(getattr(desktop,'diagnostic_trace',False)),
        'tool_observation':desktop.input_tool_observation() if callable(getattr(desktop,'input_tool_observation',None)) else {},
        'events':list(getattr(desktop,'hotkey_events',())),
        'focus':{**pick(focus,('state','root','focus','epoch','tool','tool_modal','tool_reads','uia_available','uia_error_type','error_type','tool_error_type')),
                 'verification':verification(focus.get('last_verification')),'guard_verification':verification(focus.get('last_guard_verification')),
                 'observation':{**pick(observation,('state','root','focus','pid','class','layout','workspace','blocked','foreign_focus','observed_at_ms')),
                                'accessibility':{str(k):v for k,v in (observation.get('accessibility') or {}).items() if str(k) in ('30002','30003','30008','30010','30020','30022','30033','30040','30043','30046','30048') and type(v) in (int,bool,type(None))}}},
        'ime':{**pick(ime,('state','mechanism','pending','owned','legacy_pending','legacy_scope','legacy_unknown_preserved','uia_available','tool','tool_modal','tool_reads','confirmed_disables','confirmed_restores','disable_requests','restore_requests','ime_open','keyboard_layout','focus_class','tsf_disabled','native_events','context_revision','error_type','error_code','native_error_code','focus_policy','policy_windows','policy_reason','policy_residual','policy_age_ms','policy_failures','policy_error','policy_blocked','focus_policy_shutdown','canvas_text_editing','canvas_entry_pending','type_tool_active','type_tool_known','modal_events','canvas_timeouts')),
               'verification':verification(ime.get('last_ime_verification')),
               'first_key_gate':pick(ime.get('first_key_gate') or {},('state','reason','elapsed_ms','input_epoch')),
               'canvas':(getattr(getattr(desktop,'input_language',None),'canvas_diagnostics',None)() if callable(getattr(getattr(desktop,'input_language',None),'canvas_diagnostics',None)) else {})}}

def verification(row):
    return pick(row or {},('id','result','need_ime','input_epoch','reason','elapsed_ms','received_at_ms'))

def key_decision(event):
    if event.get('matched_action') not in (None,'','None'):return 'action-emitted'
    if not event.get('enabled'):return 'disabled'
    if not event.get('process_scope'):return 'outside-photoshop'
    if not event.get('activation_allowed'):return 'activation-rejected'
    if not event.get('command_scope'):return 'workspace-rejected'
    if event.get('key')==70:
        if event.get('modifiers'):return 'modified-key'
        if event.get('text_input') or not event.get('guard_allowed'):return 'input-protected'
        if event.get('input_epoch')!=event.get('guard_epoch'):return 'focus-changed'
    return 'no-action'

def key_evidence(event):
    row=pick(event,('type','key','modifiers','matched_action','text_input','input_epoch','guard_epoch','guard_allowed','hwnd','pid','window_class','message','flags','key_time_ms','command_scope','process_scope','enabled','repeat','owned_before','blocked','activation_allowed','activation_active','taskbar_switching','decision_us','delivery_age_ms','dropped_total'))
    return {**row,'decision':key_decision(row),'received_at_ms':round(time.time()*1000)}

def runtime_report(value):
    """Export cached counters only, never documents, credentials or raw errors."""
    ps=value.get('photoshop',{});layer=ps.get('layer_context') or {}
    observed={}
    for name in ('background','menu'):
        row=(ps.get('layer_observation') or {}).get(name,{})
        observed[name]={**pick(row,('selection_ms','shape_ms','interval_ms','completed_at_ms','phase','selection_changed')),
                        'error_present':bool(row.get('error') or row.get('shape_error'))}
    tasks=[]
    for row in value.get('tasks',{}).get('items',[])[:64]:
        tasks.append({**pick(row,('id','state','channel','elapsed_ms','created_at')),
            'error_present':bool(row.get('error')),
            'timeline':[pick(p,('state','start_elapsed_ms','duration_ms','active')) for p in row.get('timeline',[])[:32]]})
    updates=value.get('application_update',{})
    context=pick(layer,('kind','mask','quick_mask','plugin_time_ms','locked','background','same_parent'))
    context['selection_count']=len(layer.get('layer_ids',[]))
    context['age_ms']=max(0,round(time.time()*1000-layer['plugin_time_ms'])) if layer.get('plugin_time_ms') else None
    return {'schema':1,'read_only':True,'source':'cached-state-no-photoshop-request',
        'application':pick(value.get('application',{}),('version','pid','frozen')),
        'photoshop':pick(ps,('connected','busy','uxp_ready','uxp_version','photoshop_version','status_fresh','status_age_ms','status_wait_reason')),
        'layer_context':context,'layer_observation':observed,'tasks':tasks,
        'application_update':{**pick(updates,('phase','current','available','last_checked','last_attempt')),'error_present':bool(updates.get('error'))},
        'graphics':pick(value.get('graphics',{}),('requested','active','actual','adapter','requested_mode','actual_mode','adapter_name','backend','software')),
        'menu_context_delivery':pick(value.get('menu_context_delivery',{}),('phase','epoch','menu_phase','kind','selection_count','packet_bytes','selection_age_ms','accepted','editor_omitted'))}

def uxp_findings(report):
    backend=report.get('backend',{})
    if backend.get('ready'):return ['uxp_loaded']
    if not report.get('plugin_exists'):return ['uxp_files_missing']
    registration=backend.get('registration',{}).get('state')
    if registration in ('missing','disabled','invalid','outdated'):return ['uxp_registration_'+registration]
    logs=[l for l in report.get('logs',[]) if l.get('matches_running_host') and l.get('after_host_start')]
    if not logs:return ['uxp_current_host_logs_unavailable']
    lines='\n'.join(line for log in logs for line in log.get('loader_lines',[]))
    # Absence is an observation in the captured logs, never a proven root cause.
    if 'user fallback plugins folder:' not in lines.lower():return ['uxp_user_external_scan_not_observed']
    return ['uxp_load_unconfirmed']

def trigger_summary(evidence):
    if evidence.get('enabled') is False:return 'menu_disabled'
    events=evidence.get('events',[])
    down=[e for e in events if e.get('message') in (0x100,0x104)]
    if not down:return 'no_trigger_keydown_observed'
    if any(e.get('matched_action')=='hold_begin' for e in down):return 'native_open_action_emitted'
    if not any(e.get('process_scope') for e in down):return 'outside_recognized_ps_process'
    if not any(e.get('activation_allowed') for e in down):return 'activation_not_qualified'
    if not any(e.get('command_scope') for e in down):return 'workspace_scope_rejected'
    return 'key_received_without_open_match'

class Diagnostics:
    def __init__(self,runtime,script,port,uxp_script=None):
        self.folder=Path(runtime)/'diagnostics';self.script=Path(script);self.port=port
        self.uxp_script=Path(uxp_script) if uxp_script else self.script.with_name('inspect-uxp.ps1')
        self.runtime_provider=None
        self.input_provider=None;self.input_samples=[]
        self.provider=None;self.listener=None;self.deadline=0;self.task=None;self.report=None;self.state={'phase':'idle','message':'尚未采集诊断'}

    def snapshot(self):
        return {**self.state,'ready':self.report is not None,
                'remaining_ms':max(0,math.ceil((self.deadline-time.monotonic())*1000)) if self.deadline else 0}

    def progress(self,stage,message,duration=0,**extra):
        if self.input_provider and len(self.input_samples)<64:
            sample=self.input_provider()
            self.input_samples.append({'stage':stage,'captured_at_ms':round(time.time()*1000),'input':{k:v for k,v in sample.items() if k!='events'}})
        self.deadline=time.monotonic()+duration/1000 if duration else 0
        self.state.update(stage=stage,message=message,**extra)
        if self.listener:self.listener(self.snapshot())

    async def read_progress(self,process):
        async for raw in process.stdout:
            line=raw.decode('utf-8',errors='replace').strip()
            if not line.startswith('PSMENU_PROGRESS '):continue
            try:
                event=json.loads(line[len('PSMENU_PROGRESS '):])
                stage=event['stage'];remaining=event.get('remaining_ms',0);completed=event.get('samples_done',0)
                if stage not in ('sampling','processing') or type(remaining) is not int or not 0<=remaining<=30000 or type(completed) is not int or not 0<=completed<=10:continue
            except (ValueError,KeyError,TypeError):continue
            self.progress(stage,'正在采集：请在PS中按菜单唤出键和 F' if stage=='sampling' else '采集结束，正在整理报告',remaining,samples_done=completed,samples_total=10)


    def start(self,scope='menu'):
        if not isinstance(scope,str) or scope not in SCOPES:raise ValueError('无效的诊断范围')
        if self.task and not self.task.done():raise RuntimeError('诊断正在采集，请稍候')
        self.scope=scope;self.report=None;self.input_samples=[];self.state={'phase':'collecting','scope':scope,'components':{}}
        if scope in ('all','menu'):self.progress('prepare','准备：请切回PS并点击图层列表',5000,samples_done=0,samples_total=10)
        else:self.progress('starting','正在准备只读检测')
        self.task=asyncio.create_task(self.collect());return self.snapshot()

    async def run_script(self,kind):
        process=None
        try:
            self.folder.mkdir(parents=True,exist_ok=True)
            path=self.folder/(uuid.uuid4().hex+'.json')
            exe=Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
            script=self.script if kind=='menu' else self.uxp_script
            args=[str(exe),'-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(script),'-BackendPort',str(self.port),'-OutputPath',str(path)]
            if kind=='menu':args+=['-DelaySeconds','0','-ProgressStream']
            process=await asyncio.create_subprocess_exec(*args,stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.DEVNULL,creationflags=0x08000000)
            await asyncio.wait_for(asyncio.gather(process.wait(),self.read_progress(process)),35 if kind=='menu' else 20)
            if process.returncode!=0 or not path.is_file():raise RuntimeError('native diagnostic failed')
            if path.stat().st_size>2*1024*1024:raise ValueError('report too large')
            report=json.loads(path.read_text('utf-8-sig'))
            if report.get('schema')!=(2 if kind=='menu' else 1) or report.get('read_only') is not True:raise ValueError('invalid report')
            return report
        finally:
            if process and process.returncode is None:
                process.kill();await process.wait() # Only our read-only diagnostic child.

    async def collect(self):
        try:
            scope=self.scope
            if scope in ('all','menu'):
                await asyncio.sleep(5)
                self.progress('starting','正在启动只读检测，请保持PS前台')
            report={'schema':2,'read_only':True,'diagnostic_scope':scope,'components':{},'findings':[]}
            kinds=('menu','uxp','runtime') if scope=='all' else (scope,)
            for kind in kinds:
                self.state['components'][kind]='collecting'
                if kind=='uxp':self.progress('uxp','正在检查UXP安装与加载日志')
                if kind=='runtime':self.progress('runtime','正在整理运行状态与图层读取耗时')
                started=time.monotonic()
                reason=''
                try:
                    if kind=='runtime':
                        if not self.runtime_provider:raise RuntimeError('runtime provider unavailable')
                        report['runtime']=runtime_report(self.runtime_provider())
                    else:
                        part=await self.run_script(kind)
                        if kind=='menu':report.update(part)
                        else:
                            report['uxp_startup']=part
                            report['findings']=uxp_findings(part)
                    self.state['components'][kind]='completed'
                except asyncio.CancelledError:raise
                except Exception as error:
                    self.state['components'][kind]='failed'
                    reason='timed_out' if isinstance(error,TimeoutError) else 'tool_unavailable' if isinstance(error,OSError) else 'invalid_report' if isinstance(error,ValueError) else 'collection_failed'
                report['components'][kind]={'phase':self.state['components'][kind],'elapsed_ms':round((time.monotonic()-started)*1000),'reason':reason}
            if self.input_provider:
                report['input_diagnostics']=self.input_provider()
                report['input_samples']=list(self.input_samples)
                report['components']['input']={'phase':'completed','reason':'cached-state-only'}
                self.state['components']['input']='completed'
            if not any(v['phase']=='completed' for v in report['components'].values()):raise RuntimeError('No diagnostic section completed')
            if self.provider and scope in ('all','menu'):
                report['trigger_decisions']=self.provider()
                report['trigger_summary']=trigger_summary(report['trigger_decisions'])
            partial=any(v['phase']=='failed' for v in report['components'].values())
            report['partial']=partial
            encoded=json.dumps(report,ensure_ascii=False,indent=2)
            if len(encoded.encode('utf-8'))>2*1024*1024:raise ValueError('combined report too large')
            self.folder.mkdir(parents=True,exist_ok=True)
            path=self.folder/(uuid.uuid4().hex+'-combined.json')
            path.write_text(encoded,'utf-8')
            self.report=report;self.state.update(phase='completed',partial=partial,findings=report['findings'])
            self.progress('completed','部分检测未完成，可导出已有报告' if partial else '诊断采集完成，可导出报告')
        except asyncio.CancelledError:
            self.state={'phase':'cancelled'};self.progress('cancelled','诊断采集已取消');raise
        except Exception:
            self.state={'phase':'failed'};self.progress('failed','诊断采集失败，请检查运行日志或使用独立检测脚本')

    def export(self):
        if self.report is None:raise RuntimeError('请先完成一次诊断采集')
        return self.report

    async def close(self):
        if self.task and not self.task.done():
            self.task.cancel();await asyncio.gather(self.task,return_exceptions=True)
