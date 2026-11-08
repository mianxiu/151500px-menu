"""Production UXP execution using private local files; no COM write fallback."""
import asyncio
import copy
import json
import os
from pathlib import Path
import secrets
import time
import threading
from .photoshop_bridge import PhotoshopBridge
from .photoshop_version import MIN_PHOTOSHOP_TEXT, require_version, version_tuple
from .plugins import version
from .layer_context import unavailable,validate,fresh
from .uxp_recovery import PendingUxp,captured_target

DIRECT_CAPS=frozenset(('selectAbove','selectBelow','extendAbove','extendBelow','copyStyle','pasteStyle'))

def compatible(value):
    try:return version(value)[:2]==(0,3)
    except ValueError:return False

def legacy_preview_available(heartbeat):
    """Select the standalone engine only from this host's explicit capability.

    UXP44 first advertises support for 24.0.0. An older executor must keep
    its existing draft behavior rather than borrowing another host's support.
    """
    host=version_tuple(heartbeat.get('photoshop_version'))
    try:executor=version(heartbeat.get('probe_version'))
    except (ValueError,TypeError):return False
    minimum=(0,3,44) if host<(24,0,1) else (0,3,32)
    return ((24,0,0)<=host<(24,1,0) and executor>=minimum and
            heartbeat.get('legacy_preview_available') is True and
            heartbeat.get('document_redraw_available') is False)

def read_heartbeat(folder):
    """Read current metadata through UXP's brief in-place overwrite window.

    No cached heartbeat, request publication or mutation retry. Callers still
    validate the returned session/version/lease. Never wait on missing files
    or ordinary permission failures; seven 4ms gaps bound transient retries.
    """
    for attempt in range(8):
        try:return json.loads((folder/'probe-last.json').read_text('utf-8-sig'))
        except json.JSONDecodeError:
            if attempt==7:raise
        except PermissionError as error:
            if getattr(error,'winerror',None) not in (5,32) or attempt==7:raise
        time.sleep(.004)

class UxpRequestFailed(RuntimeError):
    """A matching terminal failure proves this original request has exited."""


class UxpResultUnknown(RuntimeError):
    result_unknown = True

def publish_request(temporary, destination):
    """Retry only failed atomic publication, never an accepted operation."""
    deadline=time.monotonic()+.25
    try:
        while True:
            try:
                os.replace(temporary,destination)
                return
            except PermissionError as error:
                if getattr(error,'winerror',None) not in (5,32) or time.monotonic()>=deadline:raise
                # UXP may still hold a read handle without Windows delete-share.
                time.sleep(.004)
    except OSError:
        if temporary.exists():temporary.unlink()
        raise

class UxpTransport:
    def __init__(self, config):
        self.config=Path(config)
        self.storage=Path(os.environ.get('APPDATA',str(Path.home()/'AppData/Roaming')))/'Adobe/UXP/PluginsStorage/PHSP'
        self.pending=PendingUxp(self.config.parent/'uxp-pending.json',self.storage)
        self.task_id=None
        self.response_timeout=120
        self._observation_lock=threading.Lock()
        self._observation_binding=None
        self._observation_until=0

    @property
    def folder(self):
        # Every operation publication still performs authoritative discovery.
        return self._discover()[0]

    def _discover(self):
        candidates=[]
        for folder in self.storage.glob('*/External/com.15100px.menu.offline-probe/PluginData'):
            try:
                heartbeat=read_heartbeat(folder)
                age=time.time()*1000-heartbeat.get('plugin_time_ms',0)
                if compatible(heartbeat.get('probe_version')) and 0<=age<15000:
                    require_version(heartbeat.get('photoshop_version'))
                    candidates.append((folder,heartbeat))
            except (OSError,ValueError,TypeError):pass
        if len(candidates)>1:
            raise RuntimeError('检测到多个 Photoshop UXP 实例，请仅保留一个实例运行')
        if not candidates:
            raise RuntimeError('UXP 未就绪，请启动 Photoshop '+MIN_PHOTOSHOP_TEXT+' 或更新版本')
        return candidates[0]

    def observe(self):
        """Bound only background reads; never authorize an operation from cache."""
        with self._observation_lock:
            now=time.monotonic()
            if self._observation_binding is not None and now<self._observation_until:
                folder=self._observation_binding
                try:
                    heartbeat=read_heartbeat(folder)
                    age=time.time()*1000-heartbeat.get('plugin_time_ms',0)
                    if not compatible(heartbeat.get('probe_version')) or not 0<=age<15000:
                        raise ValueError('Observation lease expired')
                    require_version(heartbeat.get('photoshop_version'))
                    return folder,heartbeat
                except (OSError,ValueError,TypeError):
                    self._observation_binding=None
            folder,heartbeat=self._discover()
            self._observation_binding=folder
            self._observation_until=now+.5
            return folder,heartbeat

    def call(self, operation, preview=None, native_preview=None, **payload):
        import msvcrt
        if self.pending.value or self.pending.error:raise RuntimeError('上一项 UXP 结果待核对，未提交新请求')
        token=json.loads(self.config.read_text('utf-8-sig'))['token']
        folder=self.folder # Pin this host for the entire request, including its response.
        with (folder/'transport.lock').open('a+b') as lease:
            lease.seek(0);lease.write(b'0');lease.flush();lease.seek(0)
            try:msvcrt.locking(lease.fileno(),msvcrt.LK_NBLCK,1)
            except OSError:raise RuntimeError('UXP 正在执行其他请求，未排队') from None
            try:
                heartbeat=read_heartbeat(folder)
                if not compatible(heartbeat.get('probe_version')) or time.time()*1000-heartbeat.get('plugin_time_ms',0)>15000:
                    raise RuntimeError('UXP 未就绪，请正常启动 Photoshop 并加载 0.3.x 插件')
                identity=secrets.token_hex(16)
                modifying=operation in ('set_label','new_document')
                if (payload.get('text_inspect') or payload.get('command')=='text.replace') and version(heartbeat.get('probe_version'))<(0,3,58):
                    raise RuntimeError('文本工具需要正常重启PS加载UXP0.3.58')
                if payload.get('command')=='text.split' and version(heartbeat.get('probe_version'))<(0,3,59):
                    raise RuntimeError('文字拆分需要正常重启PS加载UXP0.3.59')
                if payload.get('command') in ('text.replace','text.split') and 'value' in payload.get('text',{}) and version(heartbeat.get('probe_version'))<(0,3,71):
                    raise RuntimeError('文字处理计划需要正常重启PS加载UXP0.3.71；未提交修改')
                if payload.get('command')=='text.replace' and 'list_xmp' in payload.get('text',{}) and version(heartbeat.get('probe_version'))<(0,3,81):
                    raise RuntimeError('列表保存需要正常重启PS加载UXP0.3.81；未提交修改')
                if 'list_layout' in payload.get('text',{}) and version(heartbeat.get('probe_version'))<(0,3,92):
                    raise RuntimeError('列表排版需要正常重启PS加载UXP0.3.92；未提交修改')
                if payload.get('command')=='text.split' and version(heartbeat.get('probe_version'))<(0,3,78):
                    raise RuntimeError('文字拆分修复需要正常重启PS加载UXP0.3.78；未提交修改')
                if payload.get('command')=='text.split' and version(heartbeat.get('probe_version'))<(0,3,80):
                    raise RuntimeError('文字基线修复需要正常重启PS加载UXP0.3.80；未提交修改')
                if payload.get('command') in (None,'label') and payload.get('color') in ('seafoam','indigo','magenta','fuchsia'):
                    if version(heartbeat.get('probe_version'))<(0,3,79):raise RuntimeError('新增标签需要正常重启PS加载UXP0.3.79；未提交修改')
                    if version_tuple(heartbeat.get('photoshop_version'))<(25,2,0) or heartbeat.get('extended_label_colors') is not True:raise RuntimeError('当前PS执行端未确认支持此标签颜色；未提交修改')
                if payload.get('command')=='layer.thumbnail':
                    raise RuntimeError('非静默缩略图已停用，未提交操作')
                if payload.get('command')=='layer.frame' and version(heartbeat.get('probe_version'))<(0,3,97):raise RuntimeError('F聚焦需要正常重启PS加载UXP0.3.97')
                if payload.get('command')=='layer.focus' and (version(heartbeat.get('probe_version'))<(0,3,57) or payload.get('target',{}).get('session')!=heartbeat.get('session')):
                    raise RuntimeError('搜索会话或UXP版本已改变，请重新搜索')
                if payload.get('stability_run') and version(heartbeat.get('probe_version'))<(0,3,47):
                    raise RuntimeError('自动稳定性测试需要实际加载 UXP 0.3.47')
                if modifying and version(heartbeat.get('probe_version'))<(0,3,8):
                    raise RuntimeError('请更新并重新加载 UXP 0.3.8，以支持结果核对；未提交修改')
                if 'capture_current' in payload:
                    if operation!='set_label' or payload['capture_current'] is not True or payload.get('command') not in DIRECT_CAPS or 'target' in payload:
                        raise ValueError('Invalid direct Caps request')
                    if version(heartbeat.get('probe_version'))<(0,3,34) or payload.get('capture_session')!=heartbeat.get('session'):
                        raise RuntimeError('UXP 版本或会话已改变，未提交快捷键操作')
                if payload.get('command') in ('selectAbove','selectBelow','extendAbove','extendBelow') and version(heartbeat.get('probe_version'))<(0,3,16):
                    raise RuntimeError('请更新并重新加载 UXP 0.3.16，以支持单选与Shift多选；未提交操作')
                if payload.get('command') in ('moveDocumentTop','moveDocumentBottom') and version(heartbeat.get('probe_version'))<(0,3,64):
                    raise RuntimeError('文档级排序需要更新并重启PS加载UXP0.3.64')
                if payload.get('command')=='convertSmartObject' and version(heartbeat.get('probe_version'))<(0,3,18):
                    raise RuntimeError('请更新并重新加载 UXP 0.3.18，以支持转换智能对象；未提交操作')
                if payload.get('command')=='convertSmartObjectsIndividually' and version(heartbeat.get('probe_version'))<(0,3,65):
                    raise RuntimeError('逐层转换需要更新并重启PS加载UXP0.3.65；未提交操作')
                if payload.get('command') in ('shape.copyPaint','shape.pastePaint') and version(heartbeat.get('probe_version'))<(0,3,41):
                    raise RuntimeError('复制／粘贴形状外观需要更新并加载UXP0.3.41；未提交操作')
                if any('method' in payload.get('paint',{}).get(k,{}) for k in ('fill','stroke')) and version(heartbeat.get('probe_version'))<(0,3,42):
                    raise RuntimeError('感知渐变需要更新并加载UXP0.3.42；未提交操作')
                if payload.get('paint',{}).get('mask_batch') and version(heartbeat.get('probe_version'))<(0,3,35):
                    raise RuntimeError('批量羽化／密度需要更新并加载UXP0.3.35；未提交')
                if payload.get('command')=='shape.paint' and 'mask' in payload.get('paint',{}) and version(heartbeat.get('probe_version'))<(0,3,20):
                    raise RuntimeError('请更新并重新加载 UXP 0.3.20，以支持形状羽化／密度；未提交操作')
                if payload.get('command')=='shape.preview' and (preview is None or version(heartbeat.get('probe_version'))<(0,3,31)):
                    raise RuntimeError('请更新并重新加载 UXP 0.3.31，以支持颜色及蒙版实时预览；未提交操作')
                if payload.get('command')=='shape.preview' and (version_tuple(heartbeat.get('photoshop_version'))<(24,1,0) or heartbeat.get('document_redraw_available') is not True):
                    raise RuntimeError('当前PS使用本地草稿，实时预览需要PS24.1及官方重绘API；未提交修改')
                text_object=payload.get('command')=='text.object'
                if text_object and (native_preview is None or version(heartbeat.get('probe_version'))<(0,3,90)):
                    raise RuntimeError('智能文字对象需要正常重启PS加载UXP0.3.90；未提交操作')
                legacy=payload.get('command')=='shape.legacy.preview' or text_object
                if legacy and not text_object and (preview is None or native_preview is None or not legacy_preview_available(heartbeat)):
                    raise RuntimeError('旧版独立预览能力未就绪；未提交修改')
                # A solid preview may switch to gradient within this transaction.
                # Reject old executors before binding or publishing any request.
                if payload.get('command') in ('shape.paint','shape.preview','shape.legacy.preview') and not payload.get('paint',{}).get('mask_batch') and version(heartbeat.get('probe_version'))<(0,3,42):
                    raise RuntimeError('颜色编辑需要更新并加载UXP0.3.42，以支持预览中切换感知渐变；未提交操作')
                if preview is not None and version(heartbeat.get('probe_version'))<(0,3,43):
                    raise RuntimeError('实时预览需要更新并加载UXP0.3.43，以核验脚本退出并恢复PS原生操作；未提交操作')
                if preview is not None and getattr(preview,'trace',False):payload['preview_trace']=True
                request={**payload,'id':identity,'token':token,'session':heartbeat.get('session'),'operation':operation,'expires_at_ms':int(time.time()*1000)+5000}
                if text_object:
                    from .settings import atomic_json
                    params=request.pop('text');request['text']={'revision':params['revision']}
                    data={**{k:request[k] for k in ('id','session','operation','token')},'params':params}
                    if len(json.dumps(data,ensure_ascii=False).encode('utf-8'))>2097152:raise ValueError('文字对象参数超出上限')
                    atomic_json(folder/('text-object-input-'+identity+'.json'),data)
                encoded=json.dumps(request,ensure_ascii=False)
                if len(encoded.encode('utf-8'))>4096:raise ValueError('UXP 请求超出本地协议长度，未提交操作')
                temporary=folder/(identity+'.tmp')
                if modifying:self.pending.begin(request,folder,self.task_id)
                native_exit={'returned':False}
                if preview is not None:
                    def execution_check():
                        current=self.pending.value
                        return bool(current and all(current.get(k)==request[k] for k in ('id','session','operation'))
                                    and self.pending.execution_has_exited(native_exit['returned']))
                    preview.execution_check=execution_check
                try:
                    if preview is not None:preview.bind(folder,request)
                    temporary.write_text(encoded,'utf-8');publish_request(temporary,folder/'probe-request.json')
                except Exception:
                    if modifying:self.pending.clear()
                    raise
                interactive=operation=='set_label' and payload.get('command') in ('editStyle','shape.preview','shape.legacy.preview')
                deadline=None if interactive else time.monotonic()+self.response_timeout
                checked=0;native_started=False;native_error_at=None
                while deadline is None or time.monotonic()<deadline:
                    try:
                        response=self.pending.read_result(allow_unknown=True) if modifying else json.loads((folder/'probe-response.json').read_text('utf-8-sig'))
                        if response is not None and response.get('id')==identity:
                            if preview is not None:getattr(preview,'execution_finished',preview.execution_check)()
                            if response.get('phase')!='completed':
                                if response.get('result_unknown'):raise UxpResultUnknown(response.get('error','新建结果未知，请检查 Photoshop'))
                                if response.get('phase')=='failed' and all(response.get(k)==request[k] for k in ('id','session','operation')):
                                    raise UxpRequestFailed(response.get('error','UXP failed'))
                                raise RuntimeError(response.get('error','UXP failed'))
                            return response
                    except (FileNotFoundError,PermissionError,json.JSONDecodeError):pass
                    if legacy and not native_started:
                        ready=None
                        try:
                            ready_path=folder/('legacy-prepared-'+identity+'.json')
                            if ready_path.stat().st_size<=(2097152 if text_object else 1048576 if payload.get('paint',{}).get('mask_batch') else 65536):ready=json.loads(ready_path.read_text('utf-8-sig'))
                        except (OSError,ValueError):pass
                        if isinstance(ready,dict) and ready.get('phase')=='prepared' and ready.get('schema')==1 and all(ready.get(k)==request[k] for k in ('id','session','operation','token')):
                            native_started=True  # A COM error never permits invoking this again.
                            try:
                                from .legacy_preview import returned_result
                                native_value=native_preview(folder,request,ready)
                                native_exit['returned']=returned_result(native_value,request)
                                if native_exit['returned']:
                                    # UXP must not publish a final legacy outcome
                                    # from a receipt written just before COM returns.
                                    from .settings import atomic_json
                                    try:atomic_json(folder/('legacy-exited-'+identity+'.json'),{
                                        **{k:request[k] for k in ('id','session','operation','token')},'schema':1,'exited':True,
                                        **({'engine':'backend-preflight','not_started':True} if json.loads(native_value).get('execution_exit')=={'engine':'backend-preflight','not_started':True,'exited':True} else {'engine':'standalone-extendscript'})})
                                    finally:
                                        if preview is not None:getattr(preview,'execution_finished',preview.execution_check)()
                            except Exception:native_error_at=time.monotonic()
                            continue  # Read the original UXP terminal BEFORE checking host state.
                    if native_error_at is not None and time.monotonic()-native_error_at>5:
                        raise UxpResultUnknown('旧版原生预览调用结果待核对；不重发')
                    if interactive and time.monotonic()-checked>=1:
                        checked=time.monotonic()
                        from .uxp_management import photoshop_running
                        operation_name='预览' if payload.get('command') in ('shape.preview','shape.legacy.preview') else '样式'
                        if not photoshop_running():raise UxpResultUnknown('Photoshop已退出，'+operation_name+'结果待核对；不重发')
                        try:
                            current=json.loads((folder/'probe-last.json').read_text('utf-8-sig'))
                            if current.get('session')!=request['session']:raise UxpResultUnknown('PS会话已改变，'+operation_name+'结果待核对；不重发')
                        except (OSError,ValueError):pass
                    time.sleep(.008)
                raise UxpResultUnknown('UXP 回应超时，结果待核对；已阻止新操作，不自动重发')
            finally:
                lease.seek(0);msvcrt.locking(lease.fileno(),msvcrt.LK_UNLCK,1)

class UxpBridge(PhotoshopBridge):
    def _observe_transport(self):
        if isinstance(self.transport,UxpTransport):return self.transport.observe()
        folder=self.transport.folder
        return folder,read_heartbeat(folder)

    def _status(self):
        # Avoid status DoJavaScript calls that may reset Photoshop's tool cursor
        # even for a read-only script. Observe the UXP heartbeat instead.
        # A lost plugin must not add COM work to a potentially blocked host.
        # Process presence is enough to distinguish "start PS" from "wait for
        # UXP"; unknown document/selection data stays unavailable.
        try:
            _,heartbeat=self._observe_transport()
            theme=heartbeat.get('ui_theme');font=heartbeat.get('ui_font_size')
            if theme in ('light','light-gray','dark-gray','dark'):self.ui_theme=theme
            if font in ('small','medium','large'):self.ui_font_size=font
            name=heartbeat.get('document_name')
            return {'connected':True,'ui_theme':self.ui_theme,'ui_font_size':self.ui_font_size,
                'document':name if isinstance(name,str) else None,'message':'已连接 Photoshop'}
        except (OSError,ValueError,RuntimeError):
            try:
                from .uxp_management import photoshop_running
                running=photoshop_running()
                return {'connected':running,'ui_theme':self.ui_theme,'ui_font_size':self.ui_font_size,
                    'document':None,'message':'Photoshop 已运行，等待 UXP 恢复连接' if running else '请先启动 Photoshop'}
            except Exception:
                return {'connected':False,'ui_theme':self.ui_theme,'ui_font_size':self.ui_font_size,
                    'document':None,'message':'请先启动 Photoshop'}

    def __init__(self, task_history=None, config=None):
        super().__init__(task_history)
        self.tasks.channel='uxp-local-files'
        self.execution_guard=None
        self.document_guard=None
        self.transport=UxpTransport(config or Path(task_history).parent/'uxp-config.json')
        self.layer_context=unavailable()
        self._reconcile_lock=asyncio.Lock()
        self._recovery_feedback=''
        if self.transport.pending.value or self.transport.pending.error:
            self.busy=True
            if self.transport.pending.value:self._active_task_id=self.transport.pending.value['task_id']

    async def reconcile_result(self):
        """Read original receipt only; never publish a request or query PS state."""
        async with self._reconcile_lock:
            pending=self.transport.pending
            if not isinstance(pending,PendingUxp):return {'active':False}
            if not pending.value:return pending.snapshot()
            task_id=pending.value['task_id']
            try:task=self.tasks.get(task_id)
            except ValueError:return {**pending.snapshot(),'message':'原任务历史记录缺失，保留待核对记录并阻止修改'}
            if task['state'] in ('succeeded','failed'):
                # An older modal boundary could strip the rollback-unknown flag
                # and incorrectly classify this receipt as a known failure.
                # The matching original receipt takes priority over that state.
                original=await asyncio.to_thread(pending.read_result,allow_unknown=True)
                if original and original.get('result_unknown'):
                    self.busy=True
                    return pending.snapshot()
                await self.tasks.flush()
                await asyncio.to_thread(pending.clear)
                self.busy=False
                if task.get('reconciled'):
                    self.last_error=task.get('error','')
                    self._recovery_feedback='UXP 最终结果已核对：'+task.get('message','')+'；未重复执行'
                return {'active':False}
            # A live worker still owns the result callback; do not release busy
            # or overwrite its outcome while it waits for the same receipt.
            if task['state']!='result-unknown' or (not task.get('restored') and getattr(self,'_operation_future',None) is not None and not self._operation_future.done()):
                return pending.snapshot()
            response=await asyncio.to_thread(pending.read_result)
            if response is None:return pending.snapshot()
            success=response['phase']=='completed'
            try:message=self._result_message(task['action'],json.dumps(response['result'])) if success else '原 UXP 任务已确认失败，未重复执行'
            except (ValueError,KeyError,TypeError):return pending.snapshot()
            self.tasks.confirm_uxp_result(task_id,'succeeded' if success else 'failed',message=message,
                error='' if success else response['error'],reconciled=True,
                uxp_request_id=response['id'],uxp_session=response['session'],
                **({'target':captured_target(response,response['session'])} if success and pending.value.get('capture_current') else {}))
            await self.tasks.flush()
            await asyncio.to_thread(pending.clear)
            self.busy=False
            self.last_error='' if success else response['error']
            self._recovery_feedback='UXP 最终结果已核对：'+message+'；未重复执行'
            self._queue_changed.set()
            return {'active':False,'message':self._recovery_feedback,'task_id':task_id}

    async def status(self,force=False):
        await self.reconcile_result()
        return await super().status(force)

    async def _execute_task(self,*args,**kwargs):
        await self.reconcile_result()
        if not self.busy:self._recovery_feedback=''
        try:return await super()._execute_task(*args,**kwargs)
        finally:await self.reconcile_result()

    def request_menu_context(self, epoch):
        folder=self.transport.folder
        heartbeat=read_heartbeat(folder)
        if version(heartbeat.get('probe_version'))<(0,3,14):return None
        token=json.loads(self.transport.config.read_text('utf-8-sig'))['token']
        # New plugins publish basic eligibility before optional paint metadata.
        # Allow a slow host to finish this read-only request without relaxing
        # snapshot freshness, request identity, or any mutation target checks.
        lifetime=10 if version(heartbeat.get('probe_version'))>=(0,3,17) else 3
        deadline=time.monotonic()+lifetime
        request={'id':secrets.token_hex(16),'session':heartbeat['session'],'token':token,'expires_at_ms':int(time.time()*1000)+lifetime*1000}
        temporary=folder/(request['id']+'.menu.tmp')
        try:
            temporary.write_text(json.dumps(request),'utf-8')
            # UXP briefly holds its read handle without delete sharing. Retry
            # only publication of this same, not-yet-accepted request; no new
            # ID, request replay, longer expiry or keyboard-hook wait.
            publish_request(temporary,folder/'menu-context-request.json')
        finally:
            if temporary.exists():temporary.unlink()
        return {'id':request['id'],'session':request['session'],'folder':folder,'epoch':epoch,'deadline':deadline,
                'completion_deadline':deadline+20 if version(heartbeat.get('probe_version'))>=(0,3,39) else deadline}

    def cancel_menu_context(self, request):
        # Read-only observation cancellation, never an operation cancellation.
        # Called by the layer monitor, outside the window/keyboard lock.
        token=json.loads(self.transport.config.read_text('utf-8-sig'))['token']
        value={'id':request['id'],'session':request['session'],'token':token}
        folder=request['folder'];temporary=folder/(request['id']+'.cancel-menu.tmp')
        try:
            temporary.write_text(json.dumps(value),'utf-8')
            publish_request(temporary,folder/'menu-context-cancel.json')
        finally:
            if temporary.exists():temporary.unlink()

    def read_layer_context(self):
        # Called by the desktop monitor, never by the low-level hook or open().
        try:
            folder,heartbeat=self._observe_transport()
        except (OSError,ValueError,RuntimeError,KeyError,TypeError):
            self.layer_context=unavailable()
            return self.layer_context
        try:
            source=folder/'layer-context.json'
            stat=source.stat()
            if stat.st_size>32768:raise ValueError('Layer metadata exceeds limit')
            key=(folder,heartbeat['session'],stat.st_mtime_ns,stat.st_size)
            cached=getattr(self,'_layer_file_cache',None)
            if cached and cached[0]==key:raw=cached[1]
            else:
                raw=json.loads(source.read_text('utf-8-sig'))
                after=source.stat()
                # Never cache an in-place rewrite as a stable file version.
                if (after.st_mtime_ns,after.st_size)==(stat.st_mtime_ns,stat.st_size):
                    self._layer_file_cache=(key,raw)
            # Recheck real age on every use; cache hits never renew timestamps.
            self.layer_context=validate(copy.deepcopy(raw),heartbeat['session'])
        except (OSError,ValueError,RuntimeError,KeyError,TypeError):
            # A reader may meet UXP's short overwrite window. Retain only a
            # recent snapshot from this exact plugin session, never an old host.
            if self.layer_context.get('session')!=heartbeat.get('session') or not fresh(self.layer_context):
                self.layer_context=unavailable()
        return self.layer_context

    def _status_snapshot(self):
        value=super()._status_snapshot()
        value['execution_backend']='uxp-local-files'
        value['layer_context']=self.layer_context
        recovery=self.transport.pending.snapshot() if isinstance(self.transport.pending,PendingUxp) else {'active':False}
        if recovery['active']:
            value.update(message=recovery['message'],status_wait_reason='uxp-result-pending',status_fresh=False,busy=True)
        elif self._recovery_feedback:recovery['message']=self._recovery_feedback
        value['uxp_recovery']=recovery
        try:
            _,heartbeat=self._observe_transport()
            value['uxp_ready']=compatible(heartbeat.get('probe_version')) and time.time()*1000-heartbeat.get('plugin_time_ms',0)<15000
            value['uxp_version']=heartbeat.get('probe_version')
            value['layer_preview_available']=heartbeat.get('layer_preview_available') is True
            value['layer_thumbnail_available']=False
            value['photoshop_version']=heartbeat.get('photoshop_version')
            value['extended_label_colors']=heartbeat.get('extended_label_colors') is True
            capability=heartbeat.get('document_redraw_available')
            value['document_redraw_available']=capability if type(capability) is bool else None
            value['legacy_preview_available']=heartbeat.get('legacy_preview_available') is True
            value['shape_clipboard_available']=(heartbeat.get('shape_clipboard_available') is True or
                bool(heartbeat.get('session')) and getattr(self,'_paint_clipboard_session',None)==heartbeat.get('session'))
            value['layer_observation']=heartbeat.get('layer_observation',{})
        except (OSError,ValueError,RuntimeError):value['uxp_ready']=False
        return value

    def _capture_target(self):
        if self.focus_guard:self.focus_guard()
        target=self.transport.call('capture')['target']
        self._captured_target=target
        self._captured_app=target['session']
        return {**target,'instance_binding':target['session']}

    def _execute(self, action):
        if self.focus_guard:self.focus_guard()
        if not self._captured_target:raise RuntimeError('未固定 UXP 目标')
        direct=self._captured_target.get('capture_current') is True
        payload={'capture_current':True,'capture_session':self._captured_app} if direct else {'target':self._captured_target}
        if action.startswith('label.') or action=='color':
            payload.update(command='label',color=action.split('.',1)[1] if '.' in action else 'orange')
        else:payload['command']=action
        self.transport.task_id=self._active_task_id
        self._uxp_worker_active=True
        try:
            response=self.transport.call('set_label',**payload)
            if direct:
                try:bound=captured_target(response,self._captured_app)
                except ValueError as error:raise UxpResultUnknown('快捷键目标收据待核对；不重发') from error
            # The plugin captured this selection after the mutation, before its
            # final receipt. Hand it to the desktop before completion feedback;
            # do not reuse the pre-creation cache in a freshly opened menu.
            context=validate(response.get('layer_context'),self._captured_app)
            self.layer_context=context if fresh(context) else unavailable()
            if action=='shape.copyPaint' and response.get('result',{}).get('shape_clipboard_available') is True:
                self._paint_clipboard_session=self._captured_app
            return json.dumps({**response['result'],**({'captured_target':bound} if direct else {})})
        finally:self._uxp_worker_active=False

    def _prepare_direct_caps(self):
        if self.focus_guard:self.focus_guard()
        heartbeat=read_heartbeat(self.transport.folder)
        if version(heartbeat.get('probe_version'))<(0,3,34):return self._capture_target()
        session=heartbeat.get('session')
        if not isinstance(session,str) or not session:raise RuntimeError('UXP 会话不可用，未提交操作')
        # No cached layer is treated as a target. The same UXP request will
        # capture, durably record, then revalidate the fresh selection in PS.
        self._captured_app=session
        self._captured_target={'session':session,'capture_current':True}
        return {**self._captured_target,'instance_binding':session}

    def _direct_caps_result_target(self,result):
        if self._captured_target.get('capture_current'):
            try:return captured_target(json.loads(result),self._captured_app)
            except ValueError as error:raise UxpResultUnknown('快捷键目标收据待核对；不重发') from error
        return {**self._captured_target,'instance_binding':self._captured_app}

    def _bind_menu_target(self,target):
        if self.focus_guard:self.focus_guard()
        if (not isinstance(target,dict) or not isinstance(target.get('session'),str) or not target['session'] or
            type(target.get('document_id')) is not int or target['document_id']<=0 or
            not isinstance(target.get('layer_ids'),list) or len(target['layer_ids'])!=1 or
            type(target['layer_ids'][0]) is not int or target['layer_ids'][0]<=0 or
            target.get('active_layer_id')!=target['layer_ids'][0]):
            raise ValueError('Invalid frozen single-layer menu target')
        self._captured_target={key:target[key] for key in ('session','document_id','active_layer_id','layer_ids')}
        self._captured_target['layer_ids']=list(target['layer_ids'])
        self._captured_app=target['session']
        return {**self._captured_target,'instance_binding':target['session']}

    async def execute(self, action, before_dispatch=None, enqueue=False, frozen_target=None, capture_current=False):
        if action=='document.recent':raise ValueError('请从首页最近预设列表选择尺寸')
        if self.execution_guard:self.execution_guard()
        if frozen_target is not None:
            # The menu already froze one selected layer. The plugin rechecks
            # session/document/active selection inside its modal before writes;
            # another capture IPC would only rediscover the same identity.
            frozen={**frozen_target,'layer_ids':list(frozen_target.get('layer_ids',[]))}
            return await self._execute_task(action,before_dispatch,capture=lambda:(self._bind_batch_mask_target if action=='shape.pastePaint' and len(frozen['layer_ids'])>1 else self._bind_menu_target)(frozen))
        if capture_current and action in DIRECT_CAPS:
            return await self._execute_task(action,before_dispatch,capture=self._prepare_direct_caps,result_target=self._direct_caps_result_target)
        return await super().execute(action,before_dispatch,enqueue=False)

    def _bind_batch_mask_target(self,target):
        from .operation_target import validate_target
        if self.focus_guard:self.focus_guard()
        bound=validate_target(target)
        if not isinstance(target.get('session'),str) or not target['session'] or not 2<=len(bound['layer_ids'])<=128:
            raise ValueError('Invalid frozen batch mask target')
        self._captured_app=target['session'];self._captured_target={**bound,'session':target['session']}
        return {**self._captured_target,'instance_binding':self._captured_app}

    async def execute_focus(self,session,destination,before_dispatch=None):
        if self.execution_guard:self.execution_guard()
        def capture():
            target=self._capture_target()
            if target['session']!=session:raise RuntimeError('搜索会话已改变，请重新搜索')
            return target
        def write(action):
            if self.focus_guard:self.focus_guard()
            self.transport.task_id=self._active_task_id;self._uxp_worker_active=True
            try:
                response=self.transport.call('set_label',command='layer.focus',target=self._captured_target,destination=destination)
                value=validate(response.get('layer_context'),session)
                self.layer_context=value if fresh(value) else unavailable()
                return json.dumps(response['result'])
            finally:self._uxp_worker_active=False
        return await self._execute_task('layer.focus',before_dispatch,capture=capture,execute=write)

    async def execute_text(self,target,params,action,before_dispatch=None):
        from .text_tools import replace_parameters,split_parameters
        if action not in ('text.replace','text.split'):raise ValueError('无效的文字操作')
        params=split_parameters(params) if action=='text.split' else replace_parameters(params,allow_xmp=True)
        if self.execution_guard:self.execution_guard()
        def write(action):
            if self.focus_guard:self.focus_guard()
            self.transport.task_id=self._active_task_id;self._uxp_worker_active=True
            try:
                response=self.transport.call('set_label',command=action,target=self._captured_target,text=params)
                value=validate(response.get('layer_context'),self._captured_app)
                self.layer_context=value if fresh(value) else unavailable()
                return json.dumps(response['result'])
            finally:self._uxp_worker_active=False
        return await self._execute_task(action,before_dispatch,capture=lambda:self._bind_menu_target(target),execute=write)

    async def execute_text_object(self,target,params,before_dispatch=None):
        from .text_tools import replace_parameters
        from . import text_object
        params=dict(params);existing=params.pop('object_source',None);render=params.pop('object_render',None)
        params=replace_parameters(params,allow_xmp=True)
        if existing is not None:
            import re
            text_object.source(existing)
            if not isinstance(render,str) or not re.fullmatch('[0-9a-f]{64}',render):raise ValueError('文字对象显示校验无效')
            params.update(object_source=existing,object_render=render)
        if self.execution_guard:self.execution_guard()
        def write(_):
            if self.focus_guard:self.focus_guard()
            application=self._application()
            self.transport.task_id=self._active_task_id;self._uxp_worker_active=True
            def native(folder,request,ready):
                import hashlib
                from .settings import atomic_json
                from .text_object_native import invoke
                try:
                    model=ready['prepared']['source'];encoded=text_object.source(model)
                    xml=text_object.write(ready['prepared']['before_xmp'],model)
                    ready['prepared']['source_sha256']=hashlib.sha256(encoded).hexdigest()
                    if model.get('renderer')=='list-v1':
                        from .text_object_render import render
                        started=int(time.time()*1000)
                        if started>request['expires_at_ms']:raise ValueError('智能文字对象在开始排版前已过期；未提交修改')
                        ready['prepared']['render_started_ms']=started
                        pdf=render(model,folder,request['id'])
                        ready['prepared'].update(vector_pdf_sha256=hashlib.sha256(pdf.read_bytes()).hexdigest(),render_finished_ms=int(time.time()*1000))
                    atomic_json(folder/('text-object-expected-'+request['id']+'.json'),{**{k:request[k] for k in ('id','session','operation','token')},'xmp':xml})
                except FileExistsError:
                    raise  # An existing identity never becomes a new preflight failure.
                except (ValueError,KeyError,TypeError,OSError) as error:
                    rejected={**{k:request[k] for k in ('id','session','operation','token')},'schema':1,'phase':'failed','error':str(error),'not_executed':True,'result_unknown':False,'execution_exit':{'engine':'backend-preflight','not_started':True,'exited':True}}
                    atomic_json(folder/('legacy-result-'+request['id']+'.json'),rejected)
                    return json.dumps(rejected)
                return invoke(application,folder,request,ready,model,xml)
            try:
                response=self.transport.call('set_label',command='text.object',target=self._captured_target,text=params,native_preview=native)
                value=validate(response.get('layer_context'),self._captured_app);self.layer_context=value if fresh(value) else unavailable()
                return json.dumps(response['result'])
            finally:self._uxp_worker_active=False
        return await self._execute_task('text.object',before_dispatch,capture=lambda:self._bind_menu_target(target),execute=write)

    async def execute_shape(self, params, before_dispatch=None, frozen_target=None, preview=None, _stability_run=None):
        from .shape_paint import parameters
        params=parameters(params)
        if self.execution_guard:self.execution_guard()
        def write(action):
            self.transport.task_id=self._active_task_id;self._uxp_worker_active=True
            try:
                native=None;command='shape.preview' if preview else 'shape.paint'
                if preview:
                    heartbeat=read_heartbeat(self.transport.folder)
                    if legacy_preview_available(heartbeat):
                        # Bind the COM instance before the UXP preparation. A host
                        # restart can only invalidate this proxy, never attach a
                        # later process that might reuse document/layer IDs.
                        application=self._application()
                        from .legacy_preview import invoke
                        native=lambda folder,request,ready:invoke(application,folder,request,ready)
                        command='shape.legacy.preview'
                # Versioned handshake: UXP may reuse the observed expectation
                # only when this executor performs an exact fresh native
                # baseline check before history or writes. Old plugins ignore
                # this field and retain their complete preparation.
                native_contract={'native_batch_validation':2} if native and params.get('mask_batch') else {}
                response=self.transport.call('set_label',preview=preview,native_preview=native,command=command,target=self._captured_target,paint=params,**native_contract,**({'stability_run':_stability_run} if _stability_run else {}))
                value=validate(response.get('layer_context'),self._captured_app)
                self.layer_context=value if fresh(value) else unavailable()
                return json.dumps(response['result'])
            finally:self._uxp_worker_active=False
        capture=(lambda:(self._bind_batch_mask_target if params.get('mask_batch') else self._bind_menu_target)(frozen_target)) if frozen_target is not None else self._capture_target
        if _stability_run:capture=lambda:self._capture_stability(_stability_run)
        return await self._execute_task('shape.paint',before_dispatch,capture=capture,execute=write,interactive=preview is not None)

    def _capture_stability(self, run):
        target=self.transport.call('inspect',stability_run=run)['stability']['target']
        from .operation_target import validate_target
        validate_target(target)
        self._captured_app=target['session'];self._captured_target=dict(target)
        return dict(target)

    async def execute_stability(self, run, step, before_dispatch=None, **values):
        """Internal owned-fixture operations; never exposed as arbitrary actions."""
        if self.execution_guard:self.execution_guard()
        def capture():
            if step=='fixture':
                self._captured_target=self.transport.call('inspect')['target']
                return dict(self._captured_target)
            return self._capture_stability(run)
        def execute(_):
            self.transport.task_id=self._active_task_id;self._uxp_worker_active=True
            try:
                response=self.transport.call('set_label',command='stability.internal',stability_run=run,
                    test_step=step,target=self._captured_target,**values)
                value=validate(response.get('layer_context'),response.get('session'))
                self.layer_context=value if fresh(value) else unavailable()
                return json.dumps({'result':response['result'],'timings_ms':response.get('timings_ms',{})})
            finally:self._uxp_worker_active=False
        return await self._execute_task('stability.internal',before_dispatch,capture=capture,execute=execute)

    def _capture_new_target(self):
        if self.document_guard:self.document_guard()
        target=self.transport.call('inspect').get('target')
        if not isinstance(target,dict) or 'document_ids' not in target:
            raise RuntimeError('最近预设新建需要更新并重新加载 UXP 0.3.2')
        self._captured_target=target
        return dict(target)

    def _execute_new_document(self, preset):
        if self.document_guard:self.document_guard()
        self.transport.task_id=self._active_task_id
        self._uxp_worker_active=True
        try:return json.dumps(self.transport.call('new_document',target=self._captured_target,preset=preset)['result'])
        finally:self._uxp_worker_active=False

    async def execute_recent(self, preset, before_dispatch=None):
        if self.execution_guard:self.execution_guard()
        return await self._execute_task('document.recent',before_dispatch,
            capture=self._capture_new_target,execute=lambda action:self._execute_new_document(preset))

    def replace_scripts(self,scripts):
        # Existing script-package UI stays readable; JSX never controls UXP.
        self.scripts=dict(scripts)
