"""One request-bound preview mailbox; replace values, never queue mutations."""
import json
import threading
import time
import asyncio
import logging
from pathlib import Path

from .shape_paint import parameters


class MaskPreview:
    def __init__(self, epoch, paint, *, trace=False):
        self.epoch = epoch
        self.trace = trace is True
        self.paint = parameters(paint)
        self.has_mask = 'mask' in self.paint
        self.revision = self.paint['revision']
        self.phase = 'preview'
        self.return_to_menu = False
        self.seq = 1
        self.binding = None
        self.lock = threading.Lock()
        self.publish_lock = threading.Lock()
        self.published = None
        self.published_at = 0
        self.closed = False
        self.feedback_shown = False
        self.progress_serial = -1
        self.progress_value = None
        self.execution_check = None
        self.execution_exited = False
        self.terminal_notifier = None
        self.terminal_requested_at = None
        self.terminal_published_at = None

    def set_terminal_notifier(self, notifier):
        with self.lock:
            self.terminal_notifier = notifier

    @staticmethod
    def notify_terminal(notifier):
        if notifier:
            try:notifier()
            except RuntimeError:pass  # A closed event loop cannot alter the original transaction.

    def execution_finished(self):
        if not self.execution_exited and self.execution_check:
            try:self.execution_exited=self.execution_check() is True
            except (OSError,ValueError,TypeError):return False
        return self.execution_exited

    def update(self, paint, commit=False, return_to_menu=False):
        if type(return_to_menu) is not bool or return_to_menu and not commit:
            raise ValueError("返回菜单只能确认当前预览")
        value = parameters(paint)
        if value['revision'] != self.revision or ('mask' in value) != self.has_mask or value.get('mask_batch')!=self.paint.get('mask_batch'):
            raise ValueError('预览草稿身份已改变')
        with self.lock:
            if self.closed or self.execution_exited or self.phase != 'preview':
                return False
            self.paint = value
            self.seq += 1
            if commit:
                self.phase = 'commit'
                self.return_to_menu = return_to_menu
                self.terminal_requested_at = time.perf_counter()
            notifier = self.terminal_notifier if commit else None
        self.notify_terminal(notifier)
        return True

    def cancel(self):
        notifier = None
        with self.lock:
            if not self.closed and self.phase == 'preview':
                self.phase = 'cancel'
                self.seq += 1
                self.terminal_requested_at = time.perf_counter()
                notifier = self.terminal_notifier
        self.notify_terminal(notifier)

    def bind(self, folder, request):
        with self.lock:
            self.binding = (Path(folder), {key: request[key] for key in ('id', 'session', 'token')})
        self.publish()

    def publish(self):
        # Called by the dedicated publisher, never by the hook/window thread.
        # Serialize publishers, copying state under a separate short lock so
        # a filesystem delay never blocks UI updates or cancellation.
        from .uxp_bridge import publish_request
        with self.publish_lock:
            with self.lock:
                if not self.binding or self.closed:return
                folder, identity = self.binding
                key=(identity['id'],self.seq)
                if key==self.published and time.monotonic()-self.published_at<.5:return
                value = {**identity, 'seq': self.seq, 'phase': self.phase,
                         'lease_until_ms': int(time.time()*1000)+4000, 'paint': self.paint}
            data = json.dumps(value, separators=(',', ':'))
            if len(data.encode('utf-8')) > 16384:
                raise ValueError('预览草稿超出协议长度')
            temporary = folder/(identity['id']+'.preview.tmp')
            temporary.write_text(data, 'utf-8')
            publish_request(temporary, folder/('mask-preview-'+identity['id']+'.json'))
            if value['phase'] in ('commit','cancel'):
                with self.lock:
                    if self.terminal_published_at is None:self.terminal_published_at=time.perf_counter()
            self.published=key;self.published_at=time.monotonic()

    def progress(self, read=True):
        """Read presentation-only evidence. Never changes execution or terminal state."""
        with self.lock:
            binding, phase, closed, seq = self.binding, self.phase, self.closed, self.seq
        if closed:return None
        if binding and read:
            folder, identity = binding
            try:
                path=folder/('preview-progress-'+identity['id']+'.json')
                if path.stat().st_size>4096:raise ValueError('Progress size')
                value=json.loads(path.read_text('utf-8-sig'))
                if (isinstance(value,dict) and all(value.get(k)==identity[k] for k in identity)
                    and type(value.get('serial')) is int and value['serial']>self.progress_serial
                    and value.get('stage') in ('prepare','read','write','verify','redraw','ready','restore','restore-verify','restore-redraw','commit')
                    and type(value.get('done')) is int and type(value.get('total')) is int
                    and 0<=value['done']<=value['total']<=256
                    and type(value.get('frame')) is int and 0<=value['frame']<=2147483647):
                    self.progress_serial=value['serial']
                    self.progress_value={k:value[k] for k in ('stage','done','total','frame','serial')}
            except (OSError,ValueError,TypeError):pass
        value=dict(self.progress_value or {'stage':'prepare','done':0,'total':0,'frame':0,'serial':0})
        value.update(type='preview.progress',epoch=self.epoch,phase=phase,
                     operation='批量羽化／密度' if self.paint.get('mask_batch') else '填充／描边／蒙版')
        # Cancellation has been requested, not necessarily accepted by PS yet.
        if phase=='cancel' and not value['stage'].startswith('restore'):
            value.update(stage='cancel-wait',done=0,total=0)
        elif phase=='preview' and value['stage']=='ready' and value['frame']<seq:value.update(stage='latest',done=0,total=0)
        elif phase=='commit' and value['stage']=='ready':value.update(stage='commit',done=0,total=0)
        return value

    def close(self):
        with self.lock:
            self.closed = True


async def dispatch_preview(app, action, args):
    from .uxp_bridge import UxpBridge
    if not isinstance(app.bridge,UxpBridge) or not app.desktop:
        raise RuntimeError('实时预览需要桌面菜单和 UXP 0.3.32')
    epoch=args.get('menu_epoch')
    if type(epoch) is not int or epoch!=app.desktop.menu_epoch:
        raise ValueError('预览菜单已过期')
    if args.get('after_action') is not None:raise ValueError('调色后的移动衔接已移除，请先确认调整')
    return_to_menu=args.get('return_to_menu',False)
    if type(return_to_menu) is not bool or return_to_menu and action!='mask.preview.commit':
        raise ValueError('无效的返回菜单请求')
    current=getattr(app,'_mask_preview',None)
    if current:
        if current.epoch!=epoch:raise RuntimeError('上一项预览尚未结束')
        if action=='mask.preview.commit':
            if not current.update(args.get('params'),commit=True,return_to_menu=return_to_menu):
                raise RuntimeError('预览正在收尾，不会重复确认')
            if not return_to_menu:await asyncio.to_thread(app.desktop.hide)
        else:current.update(args.get('params'))
        return {'message':'已更新当前预览'}
    if action!='mask.preview.update':raise RuntimeError('预览已结束，未重复提交')
    app.require_uxp_enabled()
    if (app.operation_lock.locked() or app.bridge.busy or getattr(app,'_mask_preview',None)
            or app.photoshop_launcher.active or not app.desktop.visible
            or epoch!=app.desktop.menu_epoch or not getattr(app.desktop,'color_editor_active',False)):
        raise RuntimeError('当前不能开始预览，请等待完成后重新打开菜单')
    snapshot=app.desktop.menu_layer_context
    if snapshot.get('shape_editor',{}).get('live_preview') is not True:
        raise RuntimeError('当前PS使用本地草稿，请确认后一次提交')
    preview=MaskPreview(epoch,args.get('params'))
    panel=getattr(app.desktop,'panel_context',None)
    preview.panel_hint=dict(panel.current) if panel else None
    target={key:snapshot.get(key) for key in ('session','document_id','layer_ids')}
    shape=snapshot.get('shape_editor',{})
    if preview.paint.get('mask_batch'):
        from .shape_paint import editor_matches
        if not shape.get('mask_batch') or not editor_matches(shape,snapshot):raise ValueError('批量蒙版选择已改变')
        target['active_layer_id']=shape['active_layer_id']
    else:
        if len(target.get('layer_ids') or [])!=1:raise ValueError('预览需要冻结的单选形状')
        target['active_layer_id']=target['layer_ids'][0]
    # Reserve synchronously before starting the worker. Subsequent slider
    # events replace this same session while focus/target checks are pending.
    await app.operation_lock.acquire()
    app._mask_preview=app.desktop.mask_preview=preview
    task=asyncio.create_task(run_preview(app,preview,target))
    app._mask_preview_task=task
    return {'message':'正在预览羽化／密度'}


async def run_preview(app, preview, target, editor=None):
    stopped=asyncio.Event()
    terminal_wake=asyncio.Event()
    loop=asyncio.get_running_loop()
    set_notifier=getattr(preview,'set_terminal_notifier',None)
    if set_notifier:set_notifier(lambda:loop.call_soon_threadsafe(terminal_wake.set))
    menu_confirmed=False
    last_progress=None
    async def pump():
        nonlocal last_progress
        while not stopped.is_set():
            # Clear before copying/publishing the latest value. A terminal
            # arriving during I/O stays signalled for another immediate pass.
            terminal_wake.clear()
            try:
                if preview.execution_exited:return
                if preview.phase=='preview':
                    if editor:alive=await asyncio.to_thread(editor.in_scope)
                    else:alive=app.desktop.visible and app.desktop.menu_epoch==preview.epoch and await asyncio.to_thread(app.desktop._scope)
                    if not alive:preview.cancel()
                await asyncio.to_thread(preview.publish)
                feedback=getattr(app.desktop,'preview_pending_feedback',None)
                if feedback:feedback()
                try:
                    progress=await asyncio.to_thread(preview.progress)
                    if progress:progress['epoch']=app.desktop.menu_epoch
                    if progress and progress!=last_progress:
                        app.desktop.event(progress);last_progress=progress
                except Exception:
                    logging.exception('Preview feedback unavailable; transaction unchanged')
            except Exception:
                # Stop renewing the lease if publication fails. Photoshop owns
                # rollback; a missing final receipt continues to block writes.
                preview.cancel()
                logging.exception('Mask preview publication failed')
                return
            try:await asyncio.wait_for(terminal_wake.wait(),.08)
            except asyncio.TimeoutError:pass
    publisher=asyncio.create_task(pump())
    try:
        await app.broadcast({'type':'operation','busy':True,'action':'shape.paint'})
        await asyncio.to_thread(editor.require_focus if editor else app.desktop.require_action_focus)
        prepare_guard=getattr(app.desktop,'prepare_preview_input_guard',None)
        if prepare_guard:await asyncio.to_thread(prepare_guard,preview)
        async def before_dispatch(captured):
            if editor:
                if preview.phase=='cancel':raise RuntimeError('渐变编辑已取消，本次未开始预览')
                await asyncio.to_thread(editor.require_target,captured)
                return
            if (app.desktop.menu_epoch!=preview.epoch or preview.phase=='cancel'
                    or not app.desktop.visible and preview.phase!='commit'):
                raise RuntimeError('菜单已关闭，未开始预览')
            await asyncio.to_thread(app.desktop.require_menu_layer_target,'shape.paint',captured)
        result=await app.bridge.execute_shape(preview.paint,before_dispatch=before_dispatch,
                                             frozen_target=target,preview=preview,
                                             **({'focus_guard':editor.require_focus} if editor else {}))
        app.desktop.layer_context=dict(app.bridge.layer_context)
        if editor:
            await editor.finished(result)
            return
        native=json.loads(result.get('result','{}')) if preview.return_to_menu else {}
        menu_confirmed=bool(preview.return_to_menu and preview.phase=='commit' and native.get('verified') is True
                            and not native.get('cancelled') and not app.bridge.busy)
        if preview.return_to_menu and not menu_confirmed:raise RuntimeError('调整结果未确认，未恢复普通菜单')
        if not menu_confirmed and app.desktop.menu_epoch==preview.epoch and app.desktop.visible:
            await asyncio.to_thread(app.desktop.hide)
        await app.broadcast({'type':'notice','scope':'operation','action':'shape.paint','ok':True,'message':result['message']})
    except Exception as error:
        preview.cancel()
        if editor:
            await editor.failed(error)
        elif app.desktop.menu_epoch==preview.epoch and app.desktop.visible:
            await asyncio.to_thread(app.desktop.hide,restore=False)
        await app.broadcast({'type':'notice','scope':'operation','action':'shape.paint','ok':False,'message':str(error)})
        logging.exception('Mask preview ended with error')
    finally:
        stopped.set();terminal_wake.set();await publisher
        if set_notifier:set_notifier(None)
        host_exited=not app.bridge.busy or await asyncio.to_thread(preview.execution_finished)
        finish_guard=getattr(app.desktop,'finish_preview_input_guard',None)
        if finish_guard:
            pending=getattr(getattr(app.bridge,'transport',None),'pending',None)
            identity=dict(pending.value or {}) if pending else {}
            def verified_terminal():
                if preview.execution_finished():return True
                if not identity or pending.value or pending.error:return False
                try:task=app.bridge.tasks.get(identity['task_id'])
                except (ValueError,KeyError):return False
                return (task.get('state') in ('succeeded','failed') and
                    task.get('uxp_request_id')==identity['id'] and task.get('uxp_session')==identity['session'])
            try:
                await asyncio.to_thread(finish_guard,preview,host_exited,verified_terminal)
            except Exception:logging.exception('Preview input guard release pending; terminal unchanged')
        finish_focus=getattr(app.desktop,'finish_preview_focus',None)
        if finish_focus and host_exited and not editor:
            try:await asyncio.to_thread(finish_focus,preview)
            except Exception:logging.exception('Preview focus feedback failed; terminal unchanged')
        if menu_confirmed and host_exited and app.desktop.menu_epoch==preview.epoch and app.desktop.visible:
            app.desktop.event({'type':'preview.menu.confirmed','epoch':preview.epoch})
        preview.close()
        app.operation_lock.release()
        app._mask_preview=app.desktop.mask_preview=None
        if editor:editor.preview=None
        app.desktop.event({'type':'mask.preview','active':False})
        await app.broadcast({'type':'operation','busy':app.bridge.busy})
        requested=getattr(preview,'terminal_requested_at',None)
        published=getattr(preview,'terminal_published_at',None)
        if requested is not None:
            logging.info('Preview terminal timing: %s',{
                'epoch':preview.epoch,'phase':preview.phase,
                'mailbox_ms':None if published is None else round((published-requested)*1000,3),
                'controller_ms':round((time.perf_counter()-requested)*1000,3),
                'execution_exited':host_exited,'busy_remaining':app.bridge.busy})
    return menu_confirmed and host_exited
