"""Local HTTP/WebSocket service and desktop application lifecycle."""
import argparse
import asyncio
import json
import logging
import os
from pathlib import Path
import signal
import sys
import logging.handlers
from urllib.parse import urlparse

from tornado import web, websocket
from tornado.httpclient import AsyncHTTPClient, HTTPRequest
from .photoshop_bridge import ACTIONS, COMMANDS, PhotoshopBridge, load_scripts
from .uxp_bridge import UxpBridge
from .layer_labels import LabelPalette
from .commands import command_contracts, validate_command
from .desktop import Desktop, Tray
from .control_window import ControlWindow
from .settings import Settings
from .plugins import Plugins, MAX_PACKAGE
from .paths import ROOT, runtime_dir
from .tasks import TaskStore, TaskCancelled
from .broadcasts import StatusDelivery
from .uxp_management import UxpManager
from .photoshop_launch import PhotoshopLauncher
from .language import read_language, save_language
from .recent_documents import RecentDocuments
from .application_updates import ApplicationUpdates
from .uxp_auto_update import UxpAutoUpdate
from .diagnostics import Diagnostics,input_report
from .version_health import VersionHealth
from .stability import StabilityTest
from . import input_service

APP_ID = "photoshop-menu"
from .version import APP_VERSION


def service_identity():
    return {"version": APP_VERSION, "pid": os.getpid(),
            "executable": str(Path(sys.executable).resolve()), "resource_root": str(ROOT)}


def allowed_origin(origin, port):
    try:
        parsed = urlparse(origin)
        return parsed.scheme == "http" and parsed.hostname in ("127.0.0.1", "localhost") and parsed.port in (port, 3001)
    except ValueError:
        return False


class LocalHandler(web.RequestHandler):
    def prepare(self):
        if self.request.host.split(":", 1)[0] not in ("127.0.0.1", "localhost"):
            raise web.HTTPError(403)
        origin = self.request.headers.get("Origin")
        if origin and not allowed_origin(origin, self.application.port):
            raise web.HTTPError(403)


class HealthHandler(LocalHandler):
    def get(self):
        self.write({"ok": True, "app": APP_ID, "ready": self.application.startup_ready(), **service_identity()})


class StatusHandler(LocalHandler):
    async def get(self):
        self.write(await self.application.dispatch("status", {}))


class TasksHandler(LocalHandler):
    def get(self):
        self.write(self.application.tasks.snapshot())


class ServiceHandler(LocalHandler):
    async def post(self):
        try:
            data = json.loads(self.request.body)
            if not isinstance(data, dict) or data.get("action") != "open":
                raise ValueError("未知的服务请求")
            self.write({"ok": True, "data": await self.application.dispatch("control.open", {})})
        except (ValueError, RuntimeError) as exc:
            self.set_status(400)
            self.write({"ok": False, "error": str(exc)})


class WSHandler(websocket.WebSocketHandler):
    def check_origin(self, origin):
        return allowed_origin(origin, self.application.port)

    def open(self):
        if self.request.host.split(":", 1)[0] not in ("127.0.0.1", "localhost"):
            self.close(1008, "Local connections only")
            return
        self._requests=set()
        self.application.clients.add(self)
        self.write_message({"type": "connected", "actions": ACTIONS})

    def on_message(self, message):
        # Returning a pending coroutine makes Tornado serialize this socket's
        # messages. That would queue clicks behind a long PS transaction and
        # prevent even hide/status requests from reaching their own guards.
        if len(self._requests)>=64:
            self.close(1013,'Too many concurrent requests')
            return
        task=asyncio.create_task(self._dispatch_message(message))
        self._requests.add(task)
        task.add_done_callback(self._request_finished)

    def _request_finished(self, task):
        self._requests.discard(task)
        if not task.cancelled() and task.exception():
            logging.error('WebSocket response delivery failed',exc_info=task.exception())

    async def _dispatch_message(self, message):
        request_id = None
        try:
            data = json.loads(message)
            if not isinstance(data, dict) or not isinstance(data.get("id"), str) or not isinstance(data.get("action"), str):
                raise ValueError("无效请求")
            request_id = data["id"]
            args = data.get("args", {})
            if not isinstance(args, dict):
                raise ValueError("参数必须为对象")
            result = await self.application.dispatch(data["action"], args)
            if self.ws_connection:
                await self.write_message({"type": "response", "id": request_id, "ok": True, "data": result})
        except Exception as exc:
            if not isinstance(exc, (ValueError, RuntimeError)):
                logging.exception("Request failed")
            if self.ws_connection:
                await self.write_message({"type": "response", "id": request_id, "ok": False, "error": str(exc) or "操作失败", 'task_id':getattr(exc,'task_id',None)})

    def on_close(self):
        self.application.clients.discard(self)
        if hasattr(self.application,'status_delivery'):self.application.status_delivery.forget(self)


class StaticHandler(web.StaticFileHandler):
    def set_extra_headers(self, path):
        self.set_header("Cache-Control", "no-cache")


class PluginPreviewHandler(LocalHandler):
    async def post(self):
        try:
            files = self.request.files.get("file", [])
            if len(files) != 1:
                raise ValueError("请选择一个 ZIP 脚本包")
            result = await asyncio.to_thread(self.application.plugins.prepare, files[0]["body"])
            self.write({"ok": True, "data": result})
        except Exception as exc:
            self.set_status(400)
            self.write({"ok": False, "error": str(exc)})


class PluginExportHandler(LocalHandler):
    async def get(self):
        self.set_header("Content-Type", "application/zip")
        self.set_header("Content-Disposition", 'attachment; filename="photoshop-menu-actions.zip"')
        self.write(await asyncio.to_thread(self.application.plugins.export))


class UxpPreviewHandler(LocalHandler):
    async def post(self):
        try:
            files=self.request.files.get('file',[])
            if len(files)!=1:raise ValueError('请选择一个 UXP ZIP 更新包')
            result=await asyncio.to_thread(self.application.uxp.prepare,files[0]['body'])
            self.write({'ok':True,'data':result})
        except Exception as exc:
            self.set_status(400);self.write({'ok':False,'error':str(exc)})


class UxpExportHandler(LocalHandler):
    async def get(self):
        self.set_header('Content-Type','application/zip')
        self.set_header('Content-Disposition','attachment; filename="photoshop-menu-uxp.zip"')
        self.write(await asyncio.to_thread(self.application.uxp.export))


class Application(web.Application):
    def __init__(self, port, bridge=None, runtime=None):
        self.port = port
        self.runtime = Path(runtime) if runtime else runtime_dir()
        self.bridge = bridge or UxpBridge(task_history=self.runtime / 'task-history.json')
        self.tasks = getattr(self.bridge, 'tasks', None) or TaskStore()
        self.tasks.on_change = self.task_changed
        self.preferences = Settings(self.runtime / "settings.json")
        self.plugins = Plugins(self.runtime / "plugins", ROOT / "jsx")
        self.uxp=UxpManager(self.runtime,ROOT/'uxp/offline-probe')
        self.photoshop_launcher=PhotoshopLauncher(self.runtime,self.photoshop_start_ready)
        if isinstance(self.bridge,UxpBridge):self.bridge.execution_guard=self.require_uxp_enabled
        if bridge is None:
            self.bridge.replace_scripts(load_scripts(self.plugins.folder()))
        self.clients = set()
        self.stop_event = asyncio.Event()
        self.desktop = self.tray = None
        self.control = ControlWindow(f"http://127.0.0.1:{port}", self.runtime)
        self.panel_lock = asyncio.Lock()
        self.operation_lock = asyncio.Lock()
        self._dialog_tasks=set()
        self.recent_documents = RecentDocuments()
        self.label_palette = LabelPalette()
        self.application_updates = ApplicationUpdates(self.runtime)
        self.uxp_auto=UxpAutoUpdate(self.runtime)
        self.diagnostics=Diagnostics(self.runtime,ROOT/"tools/inspect-panels.ps1",port)
        self.stability=StabilityTest(self)
        self.version_health=VersionHealth(self.runtime)
        self.updating = False
        self.closing = False
        self.headless = self.no_desktop = False
        super().__init__([
            (r"/websocket", WSHandler), (r"/api/health", HealthHandler),
            (r"/api/status", StatusHandler), (r"/api/service", ServiceHandler),
            (r"/api/tasks", TasksHandler),
            (r"/api/plugins/preview", PluginPreviewHandler), (r"/api/plugins/export", PluginExportHandler),
            (r"/api/uxp/preview", UxpPreviewHandler), (r"/api/uxp/export", UxpExportHandler),
            (r"/(.*)", StaticHandler, {"path": str(ROOT / "frontend/dist"), "default_filename": "index.html"}),
        ], websocket_max_message_size=65536, websocket_ping_interval=20, websocket_ping_timeout=20)

    def startup_ready(self):
        if self.closing:
            return False
        if self.no_desktop or os.name != 'nt':
            return True
        desktop, tray = self.desktop, self.tray
        return bool(desktop and desktop.hwnd and desktop.process and desktop.process.poll() is None
                    and desktop.native and desktop.native.poll() is None and desktop.ready.is_set()
                    and tray and tray.hwnd and tray.ready.is_set() and tray.thread.is_alive() and not tray.error)

    async def broadcast(self, event):
        if getattr(getattr(self,'stability',None),'active',False):
            if event.get('type')=='operation':event={**event,'busy':True}
            if event.get('type')=='status':event={**event,'data':{**event['data'],'photoshop':{**event['data']['photoshop'],'busy':True}}}
        if self.desktop:
            self.desktop.event(event)
        if not hasattr(self,'status_delivery'):self.status_delivery=StatusDelivery(self.clients)
        self.status_delivery.send(event)

    def task_changed(self, task):
        # COM completion callbacks run on the asyncio thread. The read-only
        # task feed does not query Photoshop or wait on menu/window locks.
        try:loop=asyncio.get_running_loop()
        except RuntimeError:return
        async def publish():
            await self.broadcast({'type':'tasks', 'data':self.tasks.snapshot()})
            event = {'type':'operation', 'busy':self.bridge.busy,
                                  'task_id':task['id'], 'state':task['state'],
                                  'queue_ready':False,
                                  'queue_count':len(getattr(self.bridge,'_queue',[]))}
            if isinstance(self.bridge,PhotoshopBridge) and task['id']==self.bridge._active_task_id and task['state'] in ('capturing','running'):
                event['action']=task['action']
            await self.broadcast(event)
        loop.create_task(publish())

    def execution_contracts(self):
        contracts = command_contracts()
        if isinstance(self.bridge, UxpBridge):
            for item in contracts:
                if item['id']=='document.recent':
                    item['version']=3
                    continue
                item.update(channel='uxp-local-files', version=3,
                            target_document='captured-id; same-UXP-session; recheck-inside-modal',
                            cancellation='before-dispatch-only; submitted operation awaits UXP result')
        return contracts

    def require_uxp_enabled(self):
        if self.updating:raise RuntimeError('应用正在更新，本次操作未接受')
        if not self.uxp.enabled:raise RuntimeError('UXP 执行已禁用，请在后台插件管理中启用')

    def require_update_idle(self):
        pending=getattr(getattr(self.bridge,'transport',None),'pending',None)
        if (self.updating or self.operation_lock.locked() or self.bridge.busy or
            getattr(self.bridge,'_queue',[]) or self.photoshop_launcher.active or
            pending and (pending.value or pending.error)):
            raise RuntimeError('请先完成操作或核对未知结果，再安装应用更新；未排队、未重发')
        if self.desktop and self.desktop.visible:raise RuntimeError('请先收起操作菜单，再安装应用更新')

    async def sync_uxp_auto(self, force=False):
        import time
        from .plugins import version
        if not isinstance(self.bridge,UxpBridge):return
        if not force and time.monotonic()-self.uxp_auto.checked<15:return
        self.uxp_auto.checked=time.monotonic()
        if self.operation_lock.locked() or self.updating or self.closing:return
        if not self.uxp_auto.enabled:
            self.uxp_auto.phase='disabled';return
        # Read-only discovery does not occupy the interactive operation lock.
        current=await asyncio.to_thread(self.uxp.status)
        registration=current.get('registration',{})
        if current['installed'] and current['enabled'] and not current['error'] and registration.get('state') in ('invalid','disabled'):
            self.uxp_auto.phase='blocked';self.uxp_auto.message='加载登记需要检查，请在插件管理中修复';return
        if current['installed'] and current['enabled'] and not current['error'] and (version(current['version'])<version(current['bundled_version']) or registration.get('auto_repair')):
            if await asyncio.to_thread(self.uxp.running):
                self.uxp_auto.phase='waiting';self.uxp_auto.message='UXP 待更新；保存并关闭 PS 后自动安装';return
        else:
            if current['error']:
                self.uxp_auto.phase='blocked';self.uxp_auto.message='安装异常，请在插件管理中检查'
            elif not current['installed'] or not current['enabled']:
                self.uxp_auto.phase='inactive';self.uxp_auto.message='未安装或已禁用；保留当前选择'
            else:
                self.uxp_auto.phase='current';self.uxp_auto.message='已安装版本已跟随内置插件'
            return
        pending=getattr(getattr(self.bridge,'transport',None),'pending',None)
        blocked=bool(self.bridge.busy or getattr(self.bridge,'_queue',[]) or self.photoshop_launcher.active or
                     pending and (pending.value or pending.error) or self.desktop and self.desktop.visible)
        if self.operation_lock.locked() or self.updating or self.closing:return
        async with self.operation_lock:
            await asyncio.to_thread(self.uxp_auto.step,self.uxp,blocked)

    async def photoshop_start_ready(self, started_at_ms):
        if not isinstance(self.bridge,UxpBridge):return True
        try:
            folder=await asyncio.to_thread(lambda:self.bridge.transport.folder)
            heartbeat=json.loads((folder/'probe-last.json').read_text('utf-8-sig'))
            return heartbeat.get('phase')=='ready' and int(heartbeat['session'].split('-')[0])>=started_at_ms
        except (OSError,ValueError,RuntimeError,KeyError):return False

    async def dispatch(self, action, args):
        if getattr(self,'closing',False) is True and action not in ('status','tasks.list','tasks.get','logs.read','application.update.status'):
            raise RuntimeError('服务正在退出，本次操作未接受')
        if getattr(self,'updating',False) is True and action not in ('status','tasks.list','tasks.get','logs.read','application.update.status'):
            raise RuntimeError('应用正在更新，本次操作未接受')
        if action in ('menu.tools.open','menu.tools.request','menu.tools.preview'):
            from .menu_tools import MenuTools
            if not getattr(self,'menu_tools',None):self.menu_tools=MenuTools(self)
            return await getattr(self.menu_tools,action.rsplit('.',1)[1])(args)
        if action in ('layers.search','layers.focus'):
            from .layer_search import LayerSearch
            if not getattr(self,'layer_search',None):self.layer_search=LayerSearch(self)
            return await getattr(self.layer_search,'search' if action=='layers.search' else 'focus')(args)
        if action in ('text.read','text.replace','text.split'):
            from .text_tools import TextTools
            if not getattr(self,'text_tools',None):self.text_tools=TextTools(self)
            return await getattr(self.text_tools,action.split('.')[1])(args)
        if action=='stability.start':return await self.stability.start(args.get('profile','standard'))
        if action=='stability.stop':return self.stability.stop()
        if action=='stability.cleanup':return await self.stability.cleanup()
        if action=='stability.status':return self.stability.snapshot()
        if action=='stability.export':return self.stability.export()
        if getattr(getattr(self,'stability',None),'active',False) and action in ('diagnostics.start','panel.inspect','panel.calibrate','settings.save'):
            raise RuntimeError('稳定性测试进行中，请先停止后续测试')
        if action=='diagnostics.start':
            desktop=self.desktop
            scope=args.get('scope','all')
            def diagnostic_progress(snapshot):
                if desktop and scope in ('all','menu'):desktop.event({'type':'diagnostic.progress',**snapshot,'trigger':desktop.settings['shortcuts']['toggle'],'language':read_language(self.runtime)})
                asyncio.create_task(self.broadcast({'type':'diagnostics','data':snapshot}))
            self.diagnostics.listener=diagnostic_progress
            self.diagnostics.runtime_provider=lambda:{'application':service_identity(),
                'photoshop':self.bridge._status_snapshot(),'tasks':self.tasks.snapshot(),
                'application_update':self.application_updates.snapshot(),
                'graphics':getattr(self.desktop,'graphics',{}) if self.desktop else {},
                'menu_context_delivery':getattr(self.desktop,'menu_context_delivery',{}) if self.desktop else {}}
            self.diagnostics.input_provider=lambda:input_report(self.desktop)
            result=self.diagnostics.start(scope)
            if desktop and scope in ('all','menu'):
                from collections import deque
                desktop.trigger_events=deque(maxlen=64);desktop.diagnostic_trace=True;desktop._native_config()
                desktop.hotkey_events.clear()
                self.diagnostics.provider=lambda:{'events':list(desktop.trigger_events),'activation':getattr(desktop,'activation_snapshot',None),'configured_trigger':desktop.settings['shortcuts']['toggle'],'enabled':desktop.settings['enabled']}
                def finish_trace(_):
                    desktop.diagnostic_trace=False;desktop._native_config()
                self.diagnostics.task.add_done_callback(finish_trace)
            return result
        if action=='version.health.export':return await self.version_health.export()
        if action=='version.health.acknowledge':
            result=await self.version_health.acknowledge()
            await self.broadcast({'type':'version-health','data':result})
            return result
        if action=='diagnostics.status':return self.diagnostics.snapshot()
        if action=='diagnostics.export':return self.diagnostics.export()
        if action=='uxp.auto':
            result=self.uxp_auto.set_enabled(args.get('enabled'))
            await self.broadcast({'type':'status','data':await self.dispatch('status',{})})
            return result
        if action=='application.update.status':return self.application_updates.snapshot()
        if action=='application.update.auto':return self.application_updates.set_auto(args.get('enabled'))
        if action in ('application.update.check','application.update.download'):
            return self.application_updates.start(action.rsplit('.',1)[1])
        if action=='application.update.install':
            self.require_update_idle()
            async with self.operation_lock:
                self.updating=True
                try:
                    result=await self.application_updates.prepare_install(self.port,self.headless,self.no_desktop)
                except Exception:
                    self.updating=False
                    raise
                asyncio.get_running_loop().call_later(.5,self.stop_event.set)
                return result
        if action=='language.set':
            save_language(self.runtime,args.get('language'))
            await self.broadcast({'type':'status','data':await self.dispatch('status',{})})
            return {'message':'界面语言已保存；PS 插件名称需关闭 PS 后应用','language':read_language(self.runtime)}
        if isinstance(self.bridge,UxpBridge) and action in ('plugin.install','plugin.rollback','plugin.uninstall'):
            raise ValueError('当前使用 UXP 执行端，旧 JSX 脚本包不能替换菜单操作')
        if action == "commands.list":
            return {"commands": Application.execution_contracts(self)}
        if action == 'tasks.list':
            return self.tasks.snapshot()
        if action == 'tasks.get':
            return self.tasks.get(args.get('id'))
        if action == 'tasks.cancel':
            return self.bridge.request_cancel(args.get('id'))
        if action == 'tasks.reconcile':
            if not isinstance(self.bridge,UxpBridge):raise ValueError('结果核对仅适用于 UXP 任务')
            result=await self.bridge.reconcile_result()
            await self.broadcast({'type':'status','data':await self.dispatch('status',{})})
            return {**result,'message':result.get('message','没有待核对的 UXP 任务')}
        if action=='photoshop.start':
            await self.sync_uxp_auto(force=True)
            if self.operation_lock.locked() or self.bridge.busy or getattr(self.bridge,'_queue',[]):
                raise RuntimeError('请等待当前操作结束；启动请求未排队')
            async with self.operation_lock:
                uxp=await asyncio.to_thread(self.uxp.status)
                return await self.photoshop_launcher.start(uxp['installed'] and uxp['enabled'] and not uxp['error'])
        if action=='uxp.status':return await asyncio.to_thread(self.uxp.status)
        if action in ('uxp.install','uxp.enable','uxp.disable','uxp.uninstall','uxp.rollback','uxp.apply-language','uxp.repair-registration'):
            if self.photoshop_launcher.active or self.operation_lock.locked() or self.bridge.busy or getattr(self.bridge,'_queue',[]):
                raise RuntimeError('请等待图层操作结束后再管理 UXP 插件；本次请求未排队')
            async with self.operation_lock:
                if action=='uxp.install':result=await asyncio.to_thread(self.uxp.install,args.get('token'),args.get('photoshop_exe') or None)
                elif action in ('uxp.enable','uxp.disable'):result=await asyncio.to_thread(self.uxp.set_enabled,action=='uxp.enable')
                elif action=='uxp.apply-language':result=await asyncio.to_thread(self.uxp.apply_language)
                elif action=='uxp.repair-registration':result=await asyncio.to_thread(self.uxp.repair_registration)
                else:result=await asyncio.to_thread(getattr(self.uxp,action.split('.')[1]))
            if action in ('uxp.rollback','uxp.uninstall'):self.uxp_auto.set_enabled(False)
            await self.broadcast({'type':'status','data':await self.dispatch('status',{})})
            return result
        if action == "status":
            status = await self.bridge.status(force=True) if args.get('refresh') and isinstance(self.bridge,PhotoshopBridge) else await self.bridge.status()
            uxp=await asyncio.to_thread(self.uxp.status)
            recent=await asyncio.to_thread(self.recent_documents.refresh,status.get('photoshop_version'))
            label_colors=await asyncio.to_thread(self.label_palette.refresh,status)
            if isinstance(self.bridge,UxpBridge) and not uxp['enabled']:
                status={**status,'uxp_ready':False,'message':'UXP 执行已禁用'}
            value = {"photoshop": status, "actions": ACTIONS, 'recent_documents':recent,
                    "commands": Application.execution_contracts(self),
                    'tasks':self.tasks.snapshot(), 'photoshop_start':self.photoshop_launcher.snapshot(),
                    'language':read_language(self.runtime),
                    "label_colors": label_colors, 'label_colors_source':self.label_palette.source,
                    "version": APP_VERSION, "settings": self.preferences.get(), "plugin": self.plugins.status(), 'uxp':uxp,
                    "data_dir": str(self.runtime),
                    "service": service_identity(),
                    "application_update": self.application_updates.snapshot(),
                    "uxp_auto_update": self.uxp_auto.snapshot(), "diagnostics": self.diagnostics.snapshot(),
                    "menu": {"ready": bool(self.desktop and self.desktop.hwnd),
                             "hwnd": self.desktop.hwnd if self.desktop else None,
                             "pid": self.desktop.process.pid if self.desktop and self.desktop.process else None,
                             "renderer": "Dear ImGui / C++ / DirectX 11",
                             "graphics": getattr(self.desktop,'graphics',{}) if self.desktop else {},
                             "visible": bool(self.desktop and self.desktop.visible),
                             "panel_context": self.desktop.panel_context.current if self.desktop and self.desktop.panel_context else {'kind': 'generic', 'label': '通用操作', 'source': 'fallback'},
                             "native_hotkeys": bool(self.desktop and self.desktop.native and self.desktop.native.poll() is None and self.desktop.ready.is_set()),
                             "hotkey_pid": self.desktop.native.pid if self.desktop and self.desktop.native else None},
                    "desktop": self.desktop.message if self.desktop else "无桌面模式",
                    "input_language": dict(self.desktop.input_language.report) if self.desktop and self.desktop.input_language else {'state':'unavailable'},
                    "input_focus": dict(self.desktop.input_focus.report) if self.desktop and self.desktop.input_focus else {'state':'unavailable'},
                    "input_diagnostics": input_report(self.desktop),
                    "input_service": input_service.status(),
                    "tray_error": self.tray.error if self.tray else ""}
            self.version_health.observe(value)
            value['version_health']=self.version_health.snapshot()
            value['stability']=self.stability.snapshot() if hasattr(self,'stability') else {}
            if getattr(getattr(self,'stability',None),'active',False):value['photoshop']={**value['photoshop'],'busy':True}
            return value
        if action == 'document.new':
            if args.get('params') not in (None, {}):raise ValueError('新建窗口入口不接受文档参数')
            if not self.desktop:raise RuntimeError('无桌面模式不提供新建窗口')
            if self.photoshop_launcher.active or self.operation_lock.locked() or self.bridge.busy:
                raise RuntimeError('操作进行中，请等待完成后重新选择；本次操作未接受')
            async with self.operation_lock:
                async with self.panel_lock:
                    if 'menu_epoch' in args and args['menu_epoch'] != self.desktop.menu_epoch:
                        return {'message':'已忽略旧菜单操作'}
                    result = await asyncio.to_thread(self.desktop.new_document)
            logging.info('Homepage New dialog request: %s', result)
            return result
        if action == 'document.recent':
            if not self.desktop or not isinstance(self.bridge,UxpBridge):raise RuntimeError('最近预设新建需要 UXP 执行端')
            params=args.get('params',{})
            if not isinstance(params,dict) or set(params)!={'preset_id'}:raise ValueError('请选择一个最近预设')
            if self.photoshop_launcher.active or self.operation_lock.locked() or self.bridge.busy:
                raise RuntimeError('操作进行中，请等待完成后重新选择；本次操作未接受')
            async with self.operation_lock:
                async with self.panel_lock:
                    if 'menu_epoch' in args and args['menu_epoch']!=self.desktop.menu_epoch:return {'message':'已忽略旧菜单操作'}
                    await asyncio.to_thread(self.desktop.require_home)
                    preset=self.recent_documents.resolve(params['preset_id'])
                try:
                    await self.broadcast({'type':'operation','busy':True,'action':'document.recent'})
                    async def before_new(target):
                        async with self.panel_lock:
                            if 'menu_epoch' in args and args['menu_epoch']!=self.desktop.menu_epoch:raise TaskCancelled('菜单已重新打开，未创建文档')
                            await asyncio.to_thread(self.desktop.require_home)
                            if params['preset_id'].startswith('clipboard_'):
                                await asyncio.to_thread(self.recent_documents.resolve,params['preset_id'])
                            await asyncio.to_thread(self.desktop.hide)
                    result=await self.bridge.execute_recent(preset,before_new)
                    await self.broadcast({'type':'notice','scope':'operation','ok':True,'message':result['message']})
                    return result
                except Exception as error:
                    await self.broadcast({'type':'notice','scope':'operation','ok':False,'message':str(error)})
                    raise
                finally:
                    await self.broadcast({'type':'operation','busy':self.bridge.busy})
        if action in ('mask.preview.update','mask.preview.commit'):
            from .mask_preview import dispatch_preview
            return await dispatch_preview(self,action,args)
        if action in ('layer.frame.commit','layer.frame.restore'):
            preview=getattr(self,'_frame_preview',None)
            if preview and preview.identity==args.get('frame_id'):
                preview.finish(action=='layer.frame.restore')
            return {'message':'已接收F松键状态'}
        if action == "action.execute":
            name = args.get("name")
            if name=='stability.internal':raise ValueError('测试命令仅能通过隔离测试入口执行')
            if name=='layer.focus':raise ValueError('请从图层搜索结果点击定位')
            if name in ('text.replace','text.split'):raise ValueError('请从文本工具预览后确认')
            if name=='document.recent':raise ValueError('请从首页最近预设列表选择尺寸')
            validate_command(name, args.get("params"))
            from .commands import CAPS_ACTIONS
            if name in CAPS_ACTIONS and not isinstance(self.bridge,UxpBridge):
                raise ValueError("参考Caps快捷键需要更新并重载UXP 0.3.16")
            if name in ('layer.frame','shape.paint','shape.copyPaint','shape.pastePaint') and not isinstance(self.bridge,UxpBridge):raise ValueError('形状颜色需要UXP执行插件')
            if name=='editStyle' and not args.get('_wait_for_dialog'):
                if self._dialog_tasks or self.operation_lock.locked() or self.bridge.busy:
                    raise RuntimeError('请先结束当前操作或关闭图层样式窗口')
                worker=asyncio.create_task(self.dispatch('action.execute',{**args,'_wait_for_dialog':True}))
                self._dialog_tasks.add(worker)
                def finished(future):
                    self._dialog_tasks.discard(future)
                    try:future.result()
                    except asyncio.CancelledError:pass
                    except Exception as error:
                        logging.exception('Native style dialog operation failed')
                        asyncio.create_task(self.broadcast({'type':'notice','scope':'operation','ok':False,'message':str(error)}))
                worker.add_done_callback(finished)
                return {'message':'正在打开原生图层样式；确认或取消后任务结束'}
            if self._dialog_tasks and not args.get('_wait_for_dialog'):
                raise RuntimeError('请先关闭图层样式窗口')
            if self.photoshop_launcher.active:raise RuntimeError('Photoshop 正在启动或结果待确认，请等待就绪后重新选择')
            if isinstance(self.bridge,UxpBridge):self.require_uxp_enabled()
            logging.info("Script action requested: %s", name)
            return_to_menu=args.get('return_to_menu',False)
            if type(return_to_menu) is not bool or return_to_menu and (name!='shape.paint' or 'menu_epoch' not in args):
                raise ValueError('无效的颜色返回菜单请求')
            if not self.desktop:
                raise RuntimeError("无桌面模式不提供图层操作")
            frame_context=self.desktop.frame_input_context(args.get('frame_id'),args.get('input_guard')) if name=='layer.frame' else None
            if self.operation_lock.locked() or self.bridge.busy:
                raise RuntimeError("操作进行中，请等待完成后重新选择；本次操作未接受")
            frame_preview=None;frame_publisher=None;frame_stopped=asyncio.Event()
            if name=='layer.frame' and 'frame_id' in args:
                from .layer_frame import FramePreview
                if type(args['frame_id']) is not int or args['frame_id']<=0:raise ValueError('无效的F手势')
                frame_preview=self._frame_preview=FramePreview(args['frame_id'])
                frame_publisher=asyncio.create_task(frame_preview.pump(self.desktop,frame_stopped))
            try:
                async with self.operation_lock:
                    # Reserve before any awaited focus/target work. A repeat
                    # arriving during that work is rejected, never postponed.
                    if not self.bridge.busy:
                        await self.broadcast({"type":"operation","busy":True,"action":name})
                    await asyncio.to_thread(self.desktop.require_action_focus)
                    frame_input=await asyncio.to_thread(self.desktop.require_layer_frame,args.get('input_guard'),accepted=frame_context) if name=='layer.frame' else None
                    if name in ('moveToTop','moveToBottom','moveUp','moveDown','moveDocumentTop','moveDocumentBottom'):
                        await asyncio.to_thread(self.desktop.require_move_context,name)
                    async with self.panel_lock:
                        if 'menu_epoch' in args and args['menu_epoch'] != self.desktop.menu_epoch:
                            return {"message":"已忽略旧菜单操作"}
                        frozen_target=None
                        snapshot=getattr(self.desktop,'menu_layer_context',None)
                        if isinstance(self.bridge,UxpBridge) and 'menu_epoch' in args and snapshot and (len(snapshot.get('layer_ids',[]))==1 or name in ('shape.paint','shape.pastePaint') and snapshot.get('shape_editor',{}).get('mask_batch') is True):
                            frozen_target={key:snapshot.get(key) for key in ('session','document_id','layer_ids')}
                            frozen_target['active_layer_id']=(snapshot.get('shape_editor',{}).get('active_layer_id') if name in ('shape.paint','shape.pastePaint') and snapshot.get('shape_editor',{}).get('mask_batch') else snapshot['layer_ids'][0])
                    async def before_dispatch(target):
                        async with self.panel_lock:
                            if name=='layer.frame':await asyncio.to_thread(self.desktop.require_layer_frame,frame_input,reserved=True)
                            if name in ('moveToTop','moveToBottom','moveUp','moveDown','moveDocumentTop','moveDocumentBottom'):
                                await asyncio.to_thread(self.desktop.require_move_context,name)
                            if 'menu_epoch' in args and args['menu_epoch'] != self.desktop.menu_epoch:
                                raise TaskCancelled('菜单已重新打开，已取消旧操作')
                            if 'menu_epoch' in args and hasattr(self.desktop,'require_menu_layer_target'):
                                await asyncio.to_thread(self.desktop.require_menu_layer_target,name,target)
                            if not return_to_menu:await asyncio.to_thread(self.desktop.hide)
                    # Frozen menu targets stay bound; direct UXP Caps actions capture
                    # in their single request and revalidate inside the write transaction.
                    if frame_preview:
                        result=await self.bridge.execute_frame(frame_preview,before_dispatch=before_dispatch)
                    elif name=='shape.paint':
                        result=await self.bridge.execute_shape(args.get('params'),before_dispatch=before_dispatch,frozen_target=frozen_target)
                        if return_to_menu:
                            terminal=json.loads(result.get('result','{}'))
                            if terminal.get('verified') is not True or terminal.get('cancelled'):
                                raise RuntimeError('颜色结果未确认，未恢复普通菜单')
                    elif frozen_target is not None:
                        result=await self.bridge.execute(name,before_dispatch=before_dispatch,frozen_target=frozen_target)
                    elif isinstance(self.bridge,UxpBridge) and 'menu_epoch' not in args:
                        result=await self.bridge.execute(name,before_dispatch=before_dispatch,capture_current=True)
                    else:result = await self.bridge.execute(name, before_dispatch=before_dispatch)
                    if isinstance(self.bridge,UxpBridge):
                        self.desktop.layer_context=dict(self.bridge.layer_context)
                logging.info("Script action completed: %s", name)
                await self.broadcast({"type": "notice", "scope": "operation", "action": name, "ok": True, "message": result["message"]})
                return result
            except TaskCancelled as exc:
                await self.broadcast({"type":"notice", "scope":"operation", "action":name,
                                      "ok":True, "message":str(exc)})
                return {'message':str(exc), 'task_id':getattr(exc,'task_id',None),'cancelled':True}
            except Exception as exc:
                await self.broadcast({"type": "notice", "scope": "operation", "action": name, "ok": False, "message": str(exc)})
                raise
            finally:
                if frame_preview:
                    frame_stopped.set()
                    await asyncio.gather(frame_publisher,return_exceptions=True)
                    if getattr(self,'_frame_preview',None) is frame_preview:self._frame_preview=None
                if isinstance(self.bridge,PhotoshopBridge):
                    await self.broadcast({'type':'operation','busy':self.bridge.busy,'queue_ready':False,'queue_count':len(self.bridge._queue)})
                else:await self.broadcast({"type": "operation", "busy": self.bridge.busy})
        if action in ("panel.open", "panel.context", "panel.hide", "panel.toggle", "panel.dismiss", "panel.hold_begin", "panel.hold_release", "panel.hold_cancel", "panel.hold_pin"):
            if not self.desktop:
                return {"message": "当前运行在无桌面模式"}
            async with self.panel_lock:
                if 'menu_epoch' in args and args['menu_epoch'] != self.desktop.menu_epoch:
                    return {"message":"已忽略旧菜单事件"}
                if action in ("panel.open","panel.context") or action=="panel.toggle" and not self.desktop.visible:
                    await self.broadcast({"type":"popup.prepare","mode":"context" if action=="panel.context" else "quick"})
                if action in ("panel.hold_begin","panel.hold_release") and 'cursor' in args:
                    cursor=args['cursor']
                    if not isinstance(cursor,list) or len(cursor)!=2 or any(type(v) is not int or not -2147483648<=v<=2147483647 for v in cursor):
                        raise ValueError('无效的松键鼠标坐标')
                    return await asyncio.to_thread(getattr(self.desktop,action.split('.')[1]),cursor=cursor)
                return await asyncio.to_thread(getattr(self.desktop, action.split(".")[1]))
        if action == "overlay.ready":
            if not self.desktop:
                raise RuntimeError("原生菜单未启动")
            self.desktop.attach(args.get("hwnd"), args.get("pid"), args.get("token"))
            return await self.dispatch("status", {})
        if action == "control.open":
            async with self.panel_lock:
                return await asyncio.to_thread(self.control.open)
        if action == "settings.save":
            previous_url = self.preferences.data["update_url"]
            data = self.preferences.save(args.get("settings"))
            if data["update_url"] != previous_url:
                self.plugins.update = None
            if self.desktop:
                self.desktop.apply_settings(data)
            await self.broadcast({"type": "status", "data": await self.dispatch("status", {})})
            return {"message": "设置已保存并生效", "settings": data}
        if action == "logs.read":
            path = self.runtime / "service.log"
            if not path.exists():
                return {"text": "暂无日志"}
            with path.open("rb") as log:
                log.seek(0, 2)
                log.seek(max(0, log.tell()-65536))
                return {"text": "\n".join(log.read().decode("utf-8", errors="replace").splitlines()[-200:])}
        if action == "plugin.check":
            return await self.plugins.check(self.preferences.data["update_url"])
        if action == "plugin.download":
            return await self.plugins.download()
        if action in ("plugin.install", "plugin.rollback", "plugin.uninstall"):
            if self.operation_lock.locked() or self.bridge.busy or getattr(self.bridge,"_queue",[]):
                raise RuntimeError("请等待图层操作完成后再更换脚本包")
            async with self.operation_lock:
                if action == "plugin.install":
                    scripts = load_scripts(self.plugins.prepared_folder(args.get("token")))
                    result = await asyncio.to_thread(self.plugins.install, args.get("token"))
                else:
                    result = await asyncio.to_thread(self.plugins.restore, action == "plugin.uninstall")
                    scripts = load_scripts(self.plugins.folder())
                self.bridge.replace_scripts(scripts)
            await self.broadcast({"type": "status", "data": await self.dispatch("status", {})})
            return {"message": "脚本包已切换并生效", "plugin": result}
        if action == "service.quit":
            if self.operation_lock.locked() or self.bridge.busy:raise RuntimeError("请先关闭图层样式窗口或等待操作结束，再退出后台")
            # Let the response reach the browser before closing sockets.
            asyncio.get_running_loop().call_later(.2, self.stop_event.set)
            return {"message": "服务正在退出"}
        if action == "input.service.status":
            return input_service.status()
        if action in ("input.service.install", "input.service.remove"):
            # One elevation prompt per change; never run while a Photoshop
            # operation owns the lock.
            if self.operation_lock.locked() or self.bridge.busy:
                raise RuntimeError("请等待当前操作结束后再更改输入法服务")
            operation = action.rsplit(".", 1)[1]
            if operation == "install":
                input_service.set_owner(os.getpid())
            result = await asyncio.to_thread(input_service.request, operation, self.runtime)
            await self.broadcast({"type": "status", "data": await self.dispatch("status", {})})
            return result
        raise ValueError("未知动作")


async def reuse(port, headless):
    client = AsyncHTTPClient()
    response = await client.fetch(f"http://127.0.0.1:{port}/api/health", request_timeout=2)
    health = json.loads(response.body)
    if health.get("app") != APP_ID:
        raise RuntimeError(f"端口 {port} 已被其他服务占用")
    running_version = health.get("version")
    if not running_version:
        # Older releases did not include a version in the health response.
        response = await client.fetch(f"http://127.0.0.1:{port}/api/status", request_timeout=3)
        running_version = json.loads(response.body).get("version")
    if running_version != APP_VERSION:
        raise RuntimeError(f"启动的是 v{APP_VERSION}，端口 {port} 正在运行 v{running_version or '未知'}。"
                           "请先通过托盘或后台管理退出旧服务，等待退出完成后再打开此版本；本次未切换版本。")
    if not headless:
        await client.fetch(HTTPRequest(f"http://127.0.0.1:{port}/api/service", method="POST",
                          headers={"Content-Type": "application/json"}, body=json.dumps({"action": "open"}), request_timeout=12))
    print(f"已复用运行中的 151500px-menu v{running_version}：http://127.0.0.1:{port}")


async def run(port=15151, headless=False, no_desktop=False, runtime=None):
    if not (ROOT / "frontend/dist/index.html").is_file():
        raise RuntimeError("缺少前端构建，请在 frontend 中运行 npm install 和 npm run build")
    app = Application(port, runtime=runtime)
    app.headless,app.no_desktop=headless,no_desktop
    try:
        server = app.listen(port, address="127.0.0.1", max_buffer_size=MAX_PACKAGE + 65536)
    except OSError:
        await app.bridge.close()
        await reuse(port, headless)
        return
    loop = asyncio.get_running_loop()
    async def notification(action):
        try:
            event_args = {}
            if isinstance(action, dict):
                event_args = {key:action[key] for key in ('menu_epoch','cursor','params','after_action','return_to_menu','input_guard','frame_id') if key in action}
                action = action['action']
                if 'menu_epoch' in event_args and (not app.desktop or event_args['menu_epoch'] != app.desktop.menu_epoch):
                    return
            mapping = {"toggle": "panel.toggle", "context": "panel.context", "open": "panel.open", "hide": "panel.hide", "dismiss": "panel.dismiss", "hold_begin": "panel.hold_begin", "hold_release": "panel.hold_release", "hold_cancel": "panel.hold_cancel", "control": "control.open", "quit": "service.quit"}
            mapping['hold_pin'] = 'panel.hold_pin'
            mapping['tools.search']='menu.tools.open'
            mapping.update({name:name for name in ('menu.tools.open','menu.tools.request','menu.tools.preview')})
            mapping['document.new'] = 'document.new'
            mapping.update({name:name for name in ('mask.preview.update','mask.preview.commit')})
            mapping.update({name:name for name in ('layer.frame.commit','layer.frame.restore')})
            if action.startswith('document.recent.'):
                event_args['params']={'preset_id':action.removeprefix('document.recent.')}
                action='document.recent'
                mapping[action]=action
            await app.dispatch(mapping.get(action, "action.execute"), event_args if action in mapping else {**event_args,"name": action})
        except Exception as exc:
            logging.exception("Desktop action failed")
            if action in ('mask.preview.update','mask.preview.commit') and not getattr(app,'_mask_preview',None) and app.desktop:
                app.desktop.event({'type':'mask.preview','active':False})
            await app.broadcast({"type": "notice", "scope": "interaction", "ok": False, "message": str(exc)})
    def notify(action):
        loop.call_soon_threadsafe(lambda: asyncio.create_task(notification(action)))
    async def poll_status():
        while not app.stop_event.is_set():
            try:await app.sync_uxp_auto()
            except Exception:logging.exception('UXP follow update check failed')
            try:
                await app.broadcast({"type": "status", "data": await app.dispatch("status", {})})
            except Exception:
                logging.exception('Status observation failed')
                app.version_health.observe({'version':APP_VERSION, '_status_unavailable':True})
                await app.broadcast({'type':'version-health','data':app.version_health.snapshot()})
            await asyncio.to_thread(app.control.refresh_icon)
            try:
                await asyncio.wait_for(app.stop_event.wait(), 3)
            except asyncio.TimeoutError:
                pass
    status_task = None
    try:
        if os.name == "nt" and not no_desktop:
            app.desktop, app.tray = Desktop(f"http://127.0.0.1:{port}", notify, app.preferences.get(), app.runtime), Tray(notify)
            app.bridge.focus_guard = app.desktop.require_action_focus
            if isinstance(app.bridge,UxpBridge):app.bridge.document_guard=app.desktop.require_home
            if isinstance(app.bridge,UxpBridge):
                app.desktop.layer_context_provider=app.bridge.read_layer_context
                app.desktop.input_language_busy=lambda:app.bridge.busy or app.operation_lock.locked() or app.photoshop_launcher.active
                from .input_tool import ToolObservation
                tool_observer=ToolObservation(app.bridge.transport)
                app.desktop.input_language_tool=tool_observer.read
                app.desktop.input_language_reconcile=tool_observer.reconcile
                app.desktop.input_tool_event=tool_observer.event
                app.desktop.input_tool_event_detail=tool_observer.event_detail
                app.desktop.input_tool_state=tool_observer.state
                app.desktop.input_tool_observation=tool_observer.snapshot
                app.desktop.menu_context_requester=app.bridge.request_menu_context
                app.desktop.menu_context_canceller=app.bridge.cancel_menu_context
            # Publish this service process for the TSF transport; harmless when
            # the machine service is not registered.
            try:input_service.set_owner(os.getpid())
            except Exception:logging.exception("Input service owner publication failed")
            await asyncio.to_thread(app.desktop.start)
            await asyncio.to_thread(app.tray.start)
            if app.tray.error:
                logging.error("Tray: %s", app.tray.error)
            uxp_install=await asyncio.to_thread(app.uxp.status)
            if not headless and (app.preferences.data["open_control_on_start"] or
                                 isinstance(app.bridge,UxpBridge) and (not uxp_install['installed'] or uxp_install['error'])):
                await app.dispatch("control.open", {})
        await app.sync_uxp_auto(force=True)
        status_task = asyncio.create_task(poll_status())
        app.application_updates.schedule=asyncio.create_task(app.application_updates.periodic())
        await app.photoshop_launcher.resume()
        print(f"151500px-menu：http://127.0.0.1:{port}", flush=True)
        for sig in (signal.SIGINT, signal.SIGTERM):
            signal.signal(sig, lambda *_: loop.call_soon_threadsafe(app.stop_event.set))
        await app.stop_event.wait()
    finally:
        app.closing=True
        await app.diagnostics.close()
        await app.stability.close()
        await app.version_health.close()
        await app.application_updates.close()
        if status_task:
            status_task.cancel()
            await asyncio.gather(status_task, return_exceptions=True)
        server.stop()
        for client in tuple(app.clients):
            client.close()
        if app.desktop:
            await asyncio.to_thread(app.desktop.close)
        if app.tray:
            await asyncio.to_thread(app.tray.close)
        await asyncio.to_thread(app.control.close)
        await app.photoshop_launcher.close()
        await app.bridge.close()


def input_service_helper(argv):
    """Elevated helper: only writes the machine registration and a receipt."""
    parser = argparse.ArgumentParser(description="151500px-menu 输入法服务安装助手")
    parser.add_argument("--input-service", choices=("install", "remove"), required=True)
    parser.add_argument("--receipt", required=True)
    args = parser.parse_args(argv)
    value = input_service.apply_privileged(args.input_service, args.receipt)
    return 0 if value.get("ok") else 1


def cli():
    if "--input-service" in sys.argv:
        raise SystemExit(input_service_helper(sys.argv[1:]))
    parser = argparse.ArgumentParser(description="151500px-menu 本地应用")
    parser.add_argument("--port", type=int, default=15151)
    parser.add_argument("--headless", action="store_true", help="启动后台且不自动打开管理窗口")
    parser.add_argument("--no-desktop", action="store_true", help="禁用窗口、快捷键和托盘")
    parser.add_argument("--data-dir", type=Path, help="自定义设置、插件和日志存储目录")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("端口必须为 1–65535")
    runtime = args.data_dir.resolve() if args.data_dir else runtime_dir()
    runtime.mkdir(parents=True, exist_ok=True)
    handler = logging.handlers.RotatingFileHandler(runtime / "service.log", maxBytes=1024*1024, backupCount=3, encoding="utf-8")
    logging.basicConfig(handlers=[handler], level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    try:
        asyncio.run(run(args.port, args.headless, args.no_desktop, runtime))
    except KeyboardInterrupt:
        pass
    except Exception as exc:
        logging.exception("Startup failed")
        if os.name == "nt" and getattr(sys, "frozen", False) and not args.headless:
            import ctypes
            ctypes.windll.user32.MessageBoxW(None, str(exc), "151500px-menu — 启动失败", 0x10)
        raise SystemExit(str(exc)) from exc
