"""A resizable gradient editor bound to one Photoshop selection and history."""
import asyncio
import hashlib
import json
import logging
import math
import ntpath
from pathlib import Path

from .layer_search import require_available
from .layer_context import fresh
from .mask_preview import MaskPreview, dispatch_preview, run_preview
from .operation_target import guarded_script, validate_target
from .paths import overlay_command
from .photoshop_com_default import PhotoshopComUnavailable, running_application
from .settings import atomic_json
from .shape_paint import metadata, parameters
from .uxp_management import photoshop_process_ids

VIEW_SCRIPT = r'''(function(){
    var s=stringIDToTypeID,d=app.activeDocument;
    function get(ref, key){
        var r=new ActionReference();r.putProperty(s('property'),s(key));
        r.putEnumerated(s(ref),s('ordinal'),s('targetEnum'));return executeActionGet(r);
    }
    var m=get('document','viewTransform').getList(s('viewTransform')), a=[];
    for(var i=0;i<m.count;i++)a.push(m.getDouble(i));
    var v=get('document','viewInfo').getObjectValue(s('viewInfo')).getObjectValue(s('activeView'));
    function rect(b,unit){
        var a=[],keys=['left','top','right','bottom'];
        for(var i=0;i<4;i++){
            var key=s(keys[i]);
            if(unit && b.getUnitDoubleType(key)!==s('pixelsUnit'))throw Error('Layer bounds are not pixels');
            a.push(unit?b.getUnitDoubleValue(key):b.getDouble(key));
        }
        return '['+a.join(',')+']';
    }
    var bounds='null';
    try{bounds=rect(get('layer','boundsNoEffects').getObjectValue(s('boundsNoEffects')),true);}catch(e){}
    var tool=String(app.currentTool);
    if(!/^[a-zA-Z0-9_.-]{1,96}$/.test(tool))tool='';
    return '{"matrix":['+a.join(',')+'],"viewport":'+rect(v.getObjectValue(s('globalBounds')),false)
        +',"document":['+d.width.as('px')+','+d.height.as('px')+'],"layer":'+bounds+',"tool":"'+tool+'"}';
})();'''


def checked_view(value):
    if not isinstance(value, dict):
        raise ValueError('Photoshop 画布位置不可用')
    def numbers(key, size):
        items = value.get(key)
        if (not isinstance(items, list) or len(items) != size or
                any(type(n) not in (int, float) or abs(n)>1e8 or not math.isfinite(n) for n in items)):
            raise ValueError('Photoshop 画布位置不可用')
        return items
    matrix = numbers('matrix', 6)
    if abs(matrix[0]*matrix[3]-matrix[1]*matrix[2]) < 1e-8:
        raise ValueError('Photoshop 画布变换不可用')
    doc, viewport = numbers('document', 2), numbers('viewport', 4)
    if min(doc) <= 0 or viewport[2] <= viewport[0] or viewport[3] <= viewport[1]:
        raise ValueError('Photoshop 画布范围不可用')
    layer = numbers('layer', 4) if value.get('layer') is not None else None
    result = {'matrix':matrix,'document':doc,'viewport':viewport,'layer':layer}
    tool = value.get('tool')
    if isinstance(tool, str) and tool and len(tool)<=96:
        result['tool'] = tool
    return result

def draft_path(runtime, target, snapshot):
    path = snapshot.get('document_path')
    if isinstance(path, str) and len(path) <= 4096 and ntpath.isabs(path) and ntpath.splitdrive(path)[0]:
        identity = [ntpath.normcase(ntpath.normpath(path)), ntpath.normcase(snapshot.get('document_name','')), target['active_layer_id']]
    else:
        identity = [target['session'], target['document_id'], target['active_layer_id']]
    key = json.dumps([identity, snapshot['baseline']], ensure_ascii=False).encode('utf-8')
    return Path(runtime) / 'gradient-drafts' / (hashlib.sha256(key).hexdigest() + '.json')


def checked_draft(base, draft):
    value = parameters(draft)
    if value['revision'] != base['revision'] or value.get('mask') != base.get('mask'):
        raise ValueError('渐变目标已改变，请重新打开')
    for key in ('fill', 'stroke'):
        original = base[key]
        paint = value[key]
        if original['kind'] != 'gradient' and paint != original:
            raise ValueError('非渐变通道保持原样')
        if (paint['kind'], paint['enabled'], paint.get('width')) != (original['kind'], original['enabled'], original.get('width')):
            raise ValueError('渐变编辑保持图层类型和描边宽度')
        if original['kind'] == 'gradient':
            if (set(paint.get('options', {})) != set(original.get('options', {})) or
                    any(name not in paint or any('midpoint' not in stop for stop in paint[name])
                        for name in ('stops', 'opacity_stops'))):
                raise ValueError('渐变草稿缺少轨道或属性')
            channel = original['stops'][0].get('native', {}).get('model')
            if any(stop.get('native', {}).get('model') != channel for stop in paint['stops']):
                raise ValueError('渐变编辑保持原生颜色通道')
    return value


def load_draft(path, base):
    if not path.exists():
        return base
    if path.stat().st_size > 65536:
        raise ValueError('原渐变草稿过大，已保留')
    try:
        value = json.loads(path.read_text('utf-8'))
        for channel in ('fill', 'stroke'):
            if base[channel]['kind'] == 'gradient' and isinstance(value.get(channel), dict):
                options = value[channel].get('options')
                if isinstance(options, dict):
                    for key in ('offset_x', 'offset_y'):
                        if key in base[channel].get('options', {}):
                            options.setdefault(key, base[channel]['options'][key])
        return checked_draft(base, {**value, 'revision': base['revision']})
    except (TypeError, ValueError) as error:
        raise ValueError('原渐变草稿无法读取，已保留') from error


class GradientEditor:
    def __init__(self, app):
        self.app = app
        self.process = None
        self.hwnd = None
        self.reader = None
        self.owner = None
        self.target = None
        self.preview = None
        self.view = None
        self.view_read = None
        self.resume_preview_after_view = False
        self.overlay_tool = None
        self.baseline = None
        self.revision = None
        self.refresh_revision = False
        self.task = None
        self.lock = asyncio.Lock()
        self.phase = 'closed'
        self.message = ''
        self.tint = ''
        self.legacy_preview = False

    def preferred_overlay(self):
        path = Path(self.app.runtime) / 'gradient-mode.json'
        try:
            preference = json.loads(path.read_text('utf-8'))
            return isinstance(preference, dict) and preference.get('overlay') is True
        except FileNotFoundError:
            return False
        except (OSError, ValueError):
            logging.warning('Could not read gradient mode preference', exc_info=True)
            return False

    def owns_overlay(self, root):
        if root != self.owner or self.phase not in ('editing','preview') or not self.in_scope():
            return False
        import ctypes
        owner = ctypes.windll.user32.GetPropW
        owner.restype = ctypes.c_void_p
        return owner(ctypes.c_void_p(self.hwnd), 'PhotoshopMenu.GradientNavigation.Owner.v1') == root

    async def send(self, event):
        if not self.process or self.process.returncode is not None:
            return
        try:
            self.process.stdin.write((json.dumps(event, ensure_ascii=False)+'\n').encode('utf-8'))
            await asyncio.wait_for(self.process.stdin.drain(), 2)
        except (OSError, asyncio.TimeoutError):
            self.process.stdin.close()
            if self.preview:self.preview.cancel()

    async def send_state(self, ack=False):
        await self.send({'type':'gradient.state','phase':self.phase,'message':self.message,'ack':ack})

    async def read_view(self, restore_tool=None):
        bridge = self.app.bridge
        body = VIEW_SCRIPT
        if restore_tool:
            body = ("(function(){var tool=app.currentTool, previous="+json.dumps(restore_tool)+";"
                    " if(tool!==previous && (tool==='zoomTool'||tool==='handTool')) app.currentTool=previous;})();\n"+body)
        script = guarded_script(body, self.target)
        def capture():
            self.require_focus()
            _, heartbeat = bridge.transport.observe()
            if heartbeat.get('session') != self.target['session']:
                raise RuntimeError('Photoshop 会话已改变')
            import win32process
            owner_pid = win32process.GetWindowThreadProcessId(self.owner)[1]
            try:
                application = running_application(bridge._application, owner_pid)
            except PhotoshopComUnavailable as error:
                if not owner_pid or photoshop_process_ids() != (owner_pid,):
                    raise RuntimeError('无法确认唯一的 Photoshop 进程') from error
                response = bridge.transport.call('inspect', gradient_view_target=self.target)
                if response.get('session') != self.target['session'] or photoshop_process_ids() != (owner_pid,):
                    raise RuntimeError('Photoshop 会话已改变') from error
                view = checked_view(response.get('gradient_view'))
                if restore_tool and view.get('tool') in ('handTool','zoomTool') and view['tool'] != restore_tool:
                    raise RuntimeError('Photoshop 临时导航工具尚未恢复') from error
                return view
            value = checked_view(json.loads(application.DoJavaScript(script)))
            if photoshop_process_ids() != (owner_pid,):
                raise RuntimeError('Photoshop 会话已改变')
            return value
        if self.view_read and not self.view_read.done():
            raise RuntimeError('上一项 Photoshop 画布读取尚未退出')
        self.view_read = asyncio.get_running_loop().run_in_executor(bridge.executor, capture)
        self.view_read.add_done_callback(lambda future: None if future.cancelled() else future.exception())
        return await asyncio.wait_for(asyncio.shield(self.view_read), 2)

    async def start_window(self, desktop, epoch):
        import win32process
        with (Path(self.app.runtime)/'gradient-window.log').open('ab') as log:
            command = overlay_command(desktop.url, desktop.token, self.app.runtime)
            self.process = process = await asyncio.create_subprocess_exec(
                *command, '--gradient-window', '--gradient-anchor', str(desktop.hwnd),
                '--gradient-owner', str(self.owner),
                '--render-mode', desktop.settings.get('render_mode','auto'),
                stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=log, creationflags=0x08000000)
        try:
            event = json.loads(await asyncio.wait_for(process.stdout.readline(), 10))
            hwnd = event.get('hwnd')
            if (event.get('type') != 'ready' or event.get('pid') != process.pid or type(hwnd) is not int
                or not desktop.gui.IsWindow(hwnd) or win32process.GetWindowThreadProcessId(hwnd)[1] != process.pid
                or desktop.gui.GetClassName(hwnd) != 'PhotoshopGradientImGui'):
                raise RuntimeError('原生渐变窗口身份校验失败')
            if not desktop.visible or desktop.menu_epoch != epoch or desktop.target != self.owner:
                raise RuntimeError('菜单或 Photoshop 工作区已改变')
            await asyncio.to_thread(desktop.require_action_focus)
            self.hwnd = hwnd
            await asyncio.to_thread(desktop.gui.SendMessage, desktop.hwnd, 0x801f, process.pid, epoch)
            await self.send({'type':'gradient.open','phase':self.phase,'message':self.message,'draft':self.draft,
                             'title':self.title,'live':self.live,'theme':self.theme,'font':self.font,'channel':self.channel,
                             'legacy_preview':self.legacy_preview,
                             'vector':self.vector,'layer_tint':self.tint if len(self.target['layer_ids'])==1 else '',
                             'prefer_overlay':await asyncio.to_thread(self.preferred_overlay)})
            for _ in range(40):
                if desktop.gui.IsWindowVisible(hwnd):break
                if process.returncode is not None:raise RuntimeError('原生渐变窗口已退出')
                await asyncio.sleep(.05)
            else:raise RuntimeError('原生渐变窗口未显示')
            desktop.gradient_window = (hwnd, self.owner)
            await asyncio.to_thread(desktop.sync_native)
            self.reader = asyncio.create_task(self.read_window(process))
        except Exception:
            process.stdin.close()
            if desktop.gradient_window == (self.hwnd, self.owner):
                desktop.gradient_window = (0, 0)
                await asyncio.to_thread(desktop.sync_native)
            raise

    async def watch_tint_target(self, process):
        confirmed = visible = False
        while process is self.process and process.returncode is None:
            context = self.app.desktop.layer_context
            current = fresh(context)
            if current:
                confirmed = all(context.get(key)==self.target[key] for key in
                                ('session','document_id','layer_ids'))
            # The modal preview owns the already checked target while host observations are suspended.
            trusted = confirmed and (current or self.phase == 'preview' and self.preview is not None)
            if trusted or visible:
                await self.send({'type':'gradient.layer','verified':trusted})
            visible = trusted
            await asyncio.sleep(.3)

    async def read_window(self, process):
        tint_task = asyncio.create_task(self.watch_tint_target(process)) if self.tint and len(self.target['layer_ids'])==1 else None
        try:
            # The saved overlay can request a view while open() still owns the Photoshop operation lock.
            async with self.lock:
                pass
            while line := await process.stdout.readline():
                event = json.loads(line)
                action = event.get('type')
                if action == 'gradient.view':
                    await self.request_view(event)
                    continue
                if action == 'gradient.preference':
                    overlay = event.get('overlay')
                    if type(overlay) is not bool:
                        raise ValueError('无效渐变编辑模式')
                    try:
                        await asyncio.to_thread(atomic_json, Path(self.app.runtime) / 'gradient-mode.json', {'overlay':overlay})
                    except OSError:
                        logging.exception('Could not save gradient mode preference')
                        self.message = '无法保存渐变编辑模式，下次打开将使用上次保存的模式'
                        await self.send_state()
                    continue
                if action not in ('gradient.update','gradient.apply','gradient.cancel'):
                    raise ValueError('无效渐变窗口事件')
                try:
                    await self.request(action, event)
                except (RuntimeError, ValueError, OSError) as error:
                    self.message = str(error)
                    if action != 'gradient.update' and self.phase not in ('closing','applying','closed'):
                        self.phase = 'error'
                    logging.warning('Gradient window action %s refused: %s', action, error)
                await self.send_state(ack=True)
        except (OSError, ValueError):
            logging.exception('Native gradient window stream ended')
        finally:
            if tint_task:
                tint_task.cancel()
                await asyncio.gather(tint_task,return_exceptions=True)
            process.stdin.close()
            if process is self.process:
                self.app.desktop.gradient_window = (0, 0)
                await asyncio.to_thread(self.app.desktop.sync_native)
                self.hwnd = None
                if self.preview and self.phase != 'applying':
                    self.phase = 'closing'
                    self.preview.cancel()
                elif not self.task or self.task.done():
                    self.phase = 'closed'

    async def request_view(self, event):
        serial, refresh = event.get('serial'), event.get('refresh')
        tracking = event.get('tracking', False)
        entering = event.get('entering', False)
        if (type(serial) is not int or not 0 < serial < 2**31 or
                type(refresh) is not bool or type(tracking) is not bool or type(entering) is not bool or
                tracking and (not refresh or entering)):
            raise ValueError('无效画布位置请求')
        message, view, retry = '', None, False
        try:
            self.require_focus()
            if self.phase not in ('editing','preview'):
                raise RuntimeError('渐变事务尚未就绪')
            if refresh and self.preview:
                # The old host owns COM until the preview returns. End just the
                # transient preview before an independent, fresh view read.
                self.preview.cancel()
                await asyncio.wait_for(asyncio.shield(self.task), 8)
                if self.phase != 'editing':
                    raise RuntimeError('原渐变恢复未确认')
                self.resume_preview_after_view = True
            if tracking:
                # Reading through COM while Space is held can interrupt Photoshop's temporary Hand tool.
                await self.send({'type':'gradient.view','serial':serial,'tracking':True})
                return
            if not self.preview:
                require_available(self.app)
                async with self.app.operation_lock:
                    try:
                        restore = self.overlay_tool if refresh and not entering else None
                        self.view = await self.read_view(restore) if restore else await self.read_view()
                    except Exception as error:
                        # RPC_E_CALL_REJECTED and RPC_E_SERVERCALL_RETRYLATER
                        retry = getattr(error, 'hresult', None) in (-2147418111, -2147417846)
                        raise
            self.require_focus()
            view = self.view
            if view:
                tool = view.get('tool')
                if (not entering and tool and self.overlay_tool and tool != self.overlay_tool and
                        tool not in ('handTool','zoomTool')):
                    self.resume_preview_after_view = False
                    await self.send({'type':'gradient.view','serial':serial,'tool_changed':True})
                    return
                if entering or self.overlay_tool is None:
                    self.overlay_tool = tool
        except Exception as error:
            logging.info('Gradient canvas view unavailable: %s', error)
            message = '无法核对当前画布位置，请重试或切回独立窗口'
        response = {'type':'gradient.view','serial':serial,'view':view,'message':message}
        if retry:
            response['retry'] = True
        if tracking:
            response['tracking'] = True
        await self.send(response)
        if self.resume_preview_after_view and view is not None and self.draft != self.base:
            self.resume_preview_after_view = False
            try:
                await self.request('gradient.update', {'draft':self.draft})
            except (RuntimeError, ValueError, OSError) as error:
                self.message = str(error)
            await self.send_state()

    def in_scope(self):
        desktop = self.app.desktop
        if (not self.hwnd or self.process.returncode is not None or not desktop.gui.IsWindowVisible(self.hwnd) or
            desktop.gui.IsIconic(self.hwnd) or not desktop._is_photoshop(self.owner)):
            return False
        foreground = desktop.gui.GetForegroundWindow()
        return self.owns_foreground(foreground) or desktop._workspace_root(foreground) == self.owner

    def owns_foreground(self, foreground):
        if foreground == self.hwnd:
            return True
        if not foreground:
            return False
        import win32process
        gui = self.app.desktop.gui
        return (gui.GetClassName(foreground) == 'PhotoshopGradientHandles' and
                gui.GetWindow(foreground, 4) == self.hwnd and
                win32process.GetWindowThreadProcessId(foreground)[1] == self.process.pid)

    def require_focus(self):
        if not self.in_scope():
            raise RuntimeError('渐变窗口或原 Photoshop 工作区已离开前台')
        desktop = self.app.desktop
        if not self.owns_foreground(desktop.gui.GetForegroundWindow()):
            desktop.require_action_focus()

    def require_target(self, target):
        self.require_focus()
        if any(target.get(key) != value for key, value in self.target.items()):
            raise RuntimeError('渐变目标已改变，本次未修改')

    async def open(self, args):
        app = self.app
        if self.lock.locked() or self.process and self.process.returncode is None or self.reader and not self.reader.done():
            raise RuntimeError('请先完成已打开的渐变窗口')
        async with self.lock:
            desktop = app.desktop
            epoch = args.get('menu_epoch')
            if not desktop or type(epoch) is not int or not desktop.visible or epoch != desktop.menu_epoch or not desktop.color_editor_active:
                raise RuntimeError('请从当前图层的 Caps 颜色面板打开渐变')
            await asyncio.to_thread(desktop.require_action_focus)
            snapshot = desktop.menu_layer_context or {}
            source = snapshot.get('shape_editor', {})
            request = args.get('params', {})
            if not isinstance(request, dict) or set(request) != {'channel','draft','preview'}:
                raise ValueError('渐变窗口入口参数不完整')
            channel = request['channel']
            if channel not in ('fill', 'stroke') or type(request['preview']) is not bool:
                raise ValueError('无效的渐变通道')
            paint = parameters(request['draft']) if request['draft'] is not None else None
            if ((paint or source).get(channel, {}).get('kind') != 'gradient' or
                    paint and (paint.get('mask_batch') or paint['revision'] != source.get('revision'))):
                raise ValueError('Caps 渐变状态已改变，请重新打开')
            panel_hint = dict(desktop.panel_context.current)
            owner = desktop.target
            if request['preview']:
                current = getattr(app, '_mask_preview', None)
                task = getattr(app, '_mask_preview_task', None)
                if not paint or not current or current.epoch != epoch or not task or task.done():
                    raise RuntimeError('当前颜色预览已结束，请重新打开 Caps')
                await dispatch_preview(app, 'mask.preview.commit', {
                    'menu_epoch':epoch, 'params':paint, 'return_to_menu':True,
                })
                if await asyncio.shield(task) is not True:
                    raise RuntimeError('Caps 调整结果未确认，独立渐变未打开')
            else:
                require_available(app)
                if paint:
                    result = await app.dispatch('action.execute', {
                        'name':'shape.paint', 'menu_epoch':epoch, 'params':paint, 'return_to_menu':True,
                    })
                    proof = json.loads(result.get('result', '{}'))
                    if proof.get('verified') is not True or proof.get('cancelled'):
                        raise RuntimeError('Caps 调整结果未确认，独立渐变未打开')
            if (not desktop.visible or desktop.menu_epoch != epoch or desktop.target != owner or
                    not desktop.color_editor_active or desktop.preview_input_guard):
                raise RuntimeError('菜单或 Photoshop 工作区已改变')
            await asyncio.to_thread(desktop.require_action_focus)
            require_available(app)
            if not desktop.visible or desktop.menu_epoch != epoch or desktop.target != owner:
                raise RuntimeError('菜单或 Photoshop 工作区已改变')
            async with app.operation_lock:
                result = await asyncio.to_thread(app.bridge.transport.call, 'inspect', shape_inspect=True, gradient_details=True)
                target = result.get('gradient_target', {})
                shape = result.get('shape_editor', {})
                base = metadata(shape)
                validate_target(target)
                if (not base.get('available') or not shape.get('baseline') or
                    any(target.get(key) != snapshot.get(key) for key in ('session', 'document_id', 'layer_ids')) or
                    base[channel]['kind'] != 'gradient' or 'options' not in base[channel]):
                    raise RuntimeError('当前目标没有可编辑的渐变，或需要重新加载新版 UXP')
                if desktop.menu_epoch != epoch or desktop.target != owner:
                    raise RuntimeError('菜单或 Photoshop 工作区已改变')
                await asyncio.to_thread(desktop.require_action_focus)
                self.base = parameters({key: base[key] for key in ('revision', 'fill', 'stroke', 'mask') if key in base})
                self.baseline, self.revision = shape['baseline'], self.base['revision']
                self.refresh_revision = False
                self.path = draft_path(app.runtime, target, shape)
                self.draft = await asyncio.to_thread(load_draft, self.path, self.base)
                self.target, self.owner, self.panel_hint, self.channel = target, owner, panel_hint, channel
                self.live = base['live_preview']
                self.legacy_preview = base.get('preview_engine') == 'legacy-com'
                self.vector = base.get('vector') is True
                self.view = None
                self.resume_preview_after_view = False
                self.overlay_tool = None
                status = app.bridge._status_snapshot()
                self.theme = status.get('ui_theme') or 'dark-gray'
                self.font = status.get('ui_font_size') or 'small'
                self.tint = getattr(desktop,'settings',{}).get('gradient_layer_tint','')
                self.title = ' · '.join(str(shape.get(key, ''))[:256] for key in ('document_name', 'layer_name'))
                self.phase = 'editing'
                self.message = '已恢复上次草稿' if self.draft != self.base else ''
                try:
                    await self.start_window(desktop, epoch)
                except Exception:
                    self.phase = 'closed'
                    raise
                await asyncio.to_thread(desktop.hide_if_epoch, epoch, False)
        return {'message': '渐变编辑已打开'}

    async def save(self, value):
        if value != self.draft or not self.path.exists() and value != self.base:
            await asyncio.to_thread(atomic_json, self.path, value)
            self.draft = value

    async def refresh_restored_revision(self):
        if not self.refresh_revision:
            return
        self.require_focus()
        result = await asyncio.to_thread(self.app.bridge.transport.call, 'inspect',
                                         shape_inspect=True, gradient_details=True)
        self.require_focus()
        shape = result.get('shape_editor', {})
        target = result.get('gradient_target', {})
        if (target != self.target or not self.baseline or shape.get('baseline') != self.baseline or
                not shape.get('available')):
            raise RuntimeError('填充／描边或渐变目标已改变，请重新打开；草稿已保留')
        fresh = parameters({key: shape[key] for key in ('revision', 'fill', 'stroke', 'mask') if key in shape})
        if {**fresh, 'revision':self.base['revision']} != self.base:
            raise RuntimeError('填充／描边已改变，请重新打开；草稿已保留')
        # Rollback invalidates the host cache, not the editor's original draft identity.
        self.revision = fresh['revision']
        self.refresh_revision = False

    async def request(self, action, args):
        async with self.lock:
            allowed = ('editing', 'preview', 'error') if action == 'gradient.cancel' else ('editing', 'preview')
            if self.phase not in allowed:
                raise RuntimeError('渐变编辑已结束或正在确认')
            if action == 'gradient.cancel':
                if 'draft' in args:
                    await self.save(checked_draft(self.base, args['draft']))
                if self.preview:
                    self.phase = 'closing'
                    self.preview.cancel()
                else:
                    self.phase = 'closed'
                return
            value = checked_draft(self.base, args.get('draft'))
            changed = value != self.draft
            await self.save(value)
            commit = action == 'gradient.apply'
            if not self.in_scope() and not commit:
                self.message = '草稿已保存'
                return
            await asyncio.to_thread(self.require_focus)
            if self.preview:
                if (changed or commit) and not self.preview.update(
                        {**value, 'revision':self.revision or value['revision']}, commit=commit, immediate=True):
                    raise RuntimeError('上次渐变预览正在结束，草稿已保留')
                if commit:
                    self.phase = 'applying'
            elif value != self.base and (self.live or commit):
                require_available(self.app)
                await self.app.operation_lock.acquire()
                try:
                    await self.refresh_restored_revision()
                except BaseException:
                    self.app.operation_lock.release()
                    raise
                value = {**value, 'revision':self.revision or value['revision']}
                if self.live:
                    if self.view is None:
                        try:
                            self.view = await self.read_view()
                        except Exception:
                            self.view = None
                    preview = self.preview = MaskPreview(self.app.desktop.menu_epoch, value)
                    preview.panel_hint = self.panel_hint
                    if commit:
                        preview.update(value, commit=True)
                    self.phase = 'applying' if commit else 'preview'
                    self.app._mask_preview = self.app.desktop.mask_preview = preview
                    self.task = self.app._mask_preview_task = asyncio.create_task(self.run_live(preview))
                else:
                    self.phase = 'applying'
                    self.task = asyncio.create_task(self.apply_local(value))
            elif commit:
                await asyncio.to_thread(self.path.unlink, missing_ok=True)
                self.phase = 'closed'
            if not commit:self.message = '草稿已保存'

    async def run_live(self, preview):
        await run_preview(self.app, preview, self.target, self)
        await self.send_state()

    async def apply_local(self, value):
        async def before_dispatch(target):
            await asyncio.to_thread(self.require_target, target)
        try:
            await self.app.broadcast({'type': 'operation', 'busy': True, 'action': 'shape.paint'})
            result = await self.app.bridge.execute_shape(value, frozen_target=self.target, focus_guard=self.require_focus, before_dispatch=before_dispatch)
            await self.finished(result)
        except Exception as error:
            await self.failed(error)
        finally:
            self.app.operation_lock.release()
            await self.send_state()
            await self.app.broadcast({'type': 'operation', 'busy': self.app.bridge.busy})

    async def finished(self, result):
        proof = json.loads(result.get('result', '{}'))
        if proof.get('verified') is not True:
            raise RuntimeError('渐变读回未确认，草稿已保留')
        if proof.get('cancelled'):
            if proof.get('rollback_verified') is not True:
                raise RuntimeError('原渐变恢复待核对，草稿已保留')
            if self.phase == 'preview' and self.hwnd:
                self.refresh_revision = True
                self.phase = 'editing'
                self.message = '已恢复 Photoshop 原渐变，草稿已保留'
                return
        else:
            await asyncio.to_thread(self.path.unlink, missing_ok=True)
        self.phase = 'closed'

    async def failed(self, error):
        self.phase = 'error'
        self.message = str(error)

    async def close(self):
        if self.preview:
            self.phase = 'closing'
            self.preview.cancel()
        if self.process and self.process.returncode is None:self.process.stdin.close()
        if self.task and not self.task.done():
            try:
                await asyncio.wait_for(asyncio.shield(self.task), 10)
            except asyncio.TimeoutError:
                pass  # The existing durable request continues to own the unresolved write.
