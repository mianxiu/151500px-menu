"""Scoped Photoshop hotkeys and a separate C++ Dear ImGui menu process."""
import ctypes
import copy
from ctypes import wintypes as W
import logging
import math
import os
from pathlib import Path
import subprocess
import threading
import time
import secrets
import json
from collections import deque
from functools import wraps
from .window_icons import WindowIcons
from .settings import DEFAULTS, chord, ALIASES, MODIFIERS
from .paths import ROOT, runtime_dir, overlay_command
from .panel_context import PanelContext, panel_flags

TITLE = "151500px-menu - Actions"
MAX_CONTEXT_PACKET_BYTES = 256 * 1024


def window_transition(method):
    @wraps(method)
    def call(self, *args, **kwargs):
        with self.window_lock:
            return method(self, *args, **kwargs)
    return call


def per_monitor_dpi(method):
    @wraps(method)
    def call(self,*args,**kwargs):
        user=ctypes.WinDLL('user32',use_last_error=True)
        user.SetThreadDpiAwarenessContext.argtypes,user.SetThreadDpiAwarenessContext.restype=[W.HANDLE],W.HANDLE
        previous=user.SetThreadDpiAwarenessContext(W.HANDLE(-4))
        try:return method(self,*args,**kwargs)
        finally:
            if previous:user.SetThreadDpiAwarenessContext(previous)
    return call


def shape_editor_allowed(panel, shape, hold, context=None):
    context=context or {}
    from .shape_paint import editor_matches
    return bool(hold and shape.get('available') is True and panel.get('kind')=='layers' and not panel.get('input_focused')
                and editor_matches(shape,context)
                and not context.get('locked') and not context.get('background'))


def process_parent(pid):
    """Windows venv python.exe can launch a second Python process."""
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    class Entry(ctypes.Structure):
        _fields_ = [("size", W.DWORD), ("usage", W.DWORD), ("pid", W.DWORD),
                    ("heap", ctypes.c_size_t), ("module", W.DWORD), ("threads", W.DWORD),
                    ("parent", W.DWORD), ("priority", W.LONG), ("flags", W.DWORD), ("name", W.WCHAR*260)]
    kernel.CreateToolhelp32Snapshot.argtypes, kernel.CreateToolhelp32Snapshot.restype = [W.DWORD,W.DWORD], W.HANDLE
    kernel.Process32FirstW.argtypes, kernel.Process32FirstW.restype = [W.HANDLE,ctypes.POINTER(Entry)], W.BOOL
    kernel.Process32NextW.argtypes, kernel.Process32NextW.restype = [W.HANDLE,ctypes.POINTER(Entry)], W.BOOL
    kernel.CloseHandle.argtypes, kernel.CloseHandle.restype = [W.HANDLE], W.BOOL
    snapshot = kernel.CreateToolhelp32Snapshot(2,0)
    if snapshot == ctypes.c_void_p(-1).value:
        return None
    try:
        entry = Entry()
        entry.size = ctypes.sizeof(entry)
        more = kernel.Process32FirstW(snapshot,ctypes.byref(entry))
        while more:
            if entry.pid == pid:
                return entry.parent
            more = kernel.Process32NextW(snapshot,ctypes.byref(entry))
        return None
    finally:
        kernel.CloseHandle(snapshot)


class KeyState:
    def __init__(self, shortcuts=None):
        self.held = set()
        self.consumed = set()
        self.pending_toggle = None
        self.toggle_used = False
        self.configure(shortcuts or DEFAULTS["shortcuts"])

    def configure(self, shortcuts):
        if getattr(self, "shortcuts", None) == shortcuts:
            return
        self.shortcuts = dict(shortcuts)
        self.bindings = {name: chord(value) for name, value in shortcuts.items()}

    def event(self, key, down, scoped):
        key = ALIASES.get(key, key)
        repeated = key in self.held
        self.held.add(key) if down else self.held.discard(key)
        if not down and key in self.consumed:
            self.consumed.discard(key)
            if key == self.pending_toggle:
                action = "hold_release" if scoped and not self.toggle_used else "hold_cancel"
                self.pending_toggle = None
                return True, action
            return True, None
        if not scoped or not down:
            return bool(down and key in self.consumed), None
        matches = [(name, keys) for name, keys in self.bindings.items()
                   if keys and keys[-1] == key and set(keys).issubset(self.held)
                   and (self.held & MODIFIERS) == (set(keys) & MODIFIERS)]
        if matches:
            name, _ = max(matches, key=lambda item: len(item[1]))
            self.consumed.add(key)
            if name == "toggle":
                if not repeated:
                    self.pending_toggle, self.toggle_used = key, False
                    return True, "hold_begin"
                return True, None
            if self.pending_toggle is not None:
                self.toggle_used = True
            return True, None if repeated else name
        # Withhold Tab only if it is a configured combination prefix.
        if key == 9 and not self.held & MODIFIERS and any(9 in keys[:-1] for keys in self.bindings.values()):
            self.consumed.add(key)
            return True, None
        if self.pending_toggle is not None and key != self.pending_toggle:
            self.toggle_used = True
        return key in self.consumed, None


def panel_bounds(ps_rect, work_rect, cursor, width, height):
    left, top = max(ps_rect[0], work_rect[0]), max(ps_rect[1], work_rect[1])
    right, bottom = min(ps_rect[2], work_rect[2]), min(ps_rect[3], work_rect[3])
    if right <= left or bottom <= top:
        raise ValueError("Photoshop 窗口没有可用的显示区域")
    width, height = min(width, right-left), min(height, bottom-top)
    return max(left, min(cursor[0], right-width)), max(top, min(cursor[1], bottom-height)), width, height


def pie_layout(ps_rect, work_rect, cursor, scale, home=False, color_editor=False, left_work=None, right_inset=None):
    width, height = round((920 if color_editor else 820 if home else 620)*scale), round((620 if color_editor else 440 if home else 500)*scale)
    # The color editor is left of the radial controls. Reserve asymmetric
    # space instead of forcing the mouse to the middle of the entire window.
    anchor = round(650*scale) if color_editor else round(270*scale) if home else width//2
    bounds = panel_bounds(ps_rect, work_rect, (cursor[0]-anchor, cursor[1]-height//2), width, height)
    if (not home and left_work and ps_rect[0] <= work_rect[0] == left_work[2]
            and cursor[0]-anchor < work_rect[0] and left_work[1] <= bounds[1]
            and bounds[1]+bounds[3] <= left_work[3]):
        ps_rect = (left_work[0], *ps_rect[1:])
        work_rect = (left_work[0], *work_rect[1:])
        bounds = panel_bounds(ps_rect, work_rect, (cursor[0]-anchor, cursor[1]-height//2), width, height)
    editor_right = False
    if color_editor:
        side = min(round(230*scale), bounds[2]//2)
        anchor = max(side, min(anchor, bounds[2]-side))
        if cursor[0] < bounds[0]+anchor:
            bounds = panel_bounds(ps_rect, work_rect, (cursor[0]-side, cursor[1]-height//2), width, height)
            anchor, editor_right = side, True
    else:
        anchor = min(anchor,bounds[2]//2)
    if not home:
        inset = min(round(200*scale), anchor)
        right_limit = anchor if editor_right or right_inset is None else max(anchor, bounds[2]-math.ceil(right_inset*scale))
        anchor = max(inset, min(right_limit, cursor[0]-bounds[0]))
    return bounds, anchor, editor_right


def adjacent_left_work(work, bounds, dpi):
    import win32api
    for monitor, _, rect in win32api.EnumDisplayMonitors():
        if rect[2] != work[0]:continue
        adjacent = win32api.GetMonitorInfo(monitor)['Work']
        if adjacent[2] != work[0] or adjacent[1] > bounds[1] or adjacent[3] < bounds[1]+bounds[3]:continue
        read = ctypes.WinDLL('shcore').GetDpiForMonitor
        read.argtypes = [W.HANDLE, ctypes.c_int, ctypes.POINTER(W.UINT), ctypes.POINTER(W.UINT)]
        read.restype = ctypes.c_long
        x, y = W.UINT(), W.UINT()
        if read(int(monitor), 0, ctypes.byref(x), ctypes.byref(y)) == 0 and x.value == y.value == dpi:
            return adjacent
    return None


def show_popup(gui, hwnd, owner, bounds, activate=True):
    """Showing an iconified GLFW viewport does not restore its render surface."""
    import win32con as c
    gui.SetWindowLong(hwnd, c.GWL_HWNDPARENT, owner)
    if gui.IsIconic(hwnd):
        gui.ShowWindow(hwnd, c.SW_RESTORE if activate else c.SW_SHOWNOACTIVATE)
    gui.SetWindowPos(hwnd, c.HWND_TOP, *bounds, c.SWP_SHOWWINDOW | (0 if activate else c.SWP_NOACTIVATE))


class Desktop:
    def __init__(self, url, notify, settings=None, runtime=None):
        self.url, self.notify = url, notify
        self.settings = settings or dict(DEFAULTS)
        self.runtime = Path(runtime) if runtime else runtime_dir()
        self.target = self.hwnd = None
        self.visible = False
        self.hold_active = False
        self.menu_tools_active = False
        self.restore_pending = False
        self.menu_epoch = 0
        self.menu_context_requester=None
        self.pie_right_inset=None
        self.menu_context_pending=None
        self.menu_context_active=None
        self.menu_context_delivery={'phase':'idle'}
        self.menu_context_canceller=None
        self.document_caption = None
        self.caption_window = 0
        self.stop = threading.Event()
        self.icons = None
        self.process = None
        self.token = secrets.token_hex(24)
        self.message = "桌面功能未启动"
        self.native = None
        self.hook_health=None
        self.frame_host=0;self.frame_grants={};self.frame_grant_lock=threading.Lock()
        self.hotkey_events=deque(maxlen=64)
        self.hook_generation=0
        self.native_lock = threading.Lock()
        self.native_payload = None
        self.native_sequence=0
        self.native_applied_sequence=0
        self.native_applied=threading.Condition()
        self.preview_input_guard=None
        self.menu_lock = threading.Lock()
        self.menu_thread = None
        self.menu_ready = threading.Event()
        self.window_lock = threading.RLock()
        self.panel_context = None
        self.input_language = None
        self.input_focus = None
        self.layer_context_provider=None
        self.layer_monitor=None
        self.layer_context={}
        self.menu_layer_context=None
        self.preview_hint_serial=0
        self.preview_hint_value={'type':'preview.pending','active':False}
        self.preview_hint_at=0

    def start(self):
        import win32gui
        self.gui = win32gui
        self.panel_context = PanelContext(win32gui)
        from .input_method import InputMethod
        from .input_focus import InputFocus
        self.input_focus=InputFocus(self)
        self.input_focus.thread.start()
        self.input_language=InputMethod(self)
        self.input_language.thread.start()
        self.icons = WindowIcons()
        self.ready = threading.Event()
        host = ROOT / "native/HotkeyHost.exe"
        if not host.is_file():
            raise RuntimeError("缺少 Windows 热键组件，请运行 tools/build-hotkey-host.ps1")
        self._spawn_native()
        self.thread = threading.Thread(target=self._native_events, name="photoshop-hotkeys", daemon=True)
        self.thread.start()
        self.ready.wait(3)
        if not self.ready.is_set() or self.native.poll() is not None:
            raise RuntimeError("Windows 热键组件未能启动，请查看服务日志")
        self.monitor = threading.Thread(target=self._monitor, name="photoshop-window", daemon=True)
        self.monitor.start()
        if self.layer_context_provider:
            self.layer_monitor=threading.Thread(target=self._monitor_layer_context,name='photoshop-layer-cache',daemon=True)
            self.layer_monitor.start()
        self._launch_overlay()

    def _spawn_native(self):
        with self.native_lock:
            if self.stop.is_set():
                return
            self.native = subprocess.Popen([str(ROOT / "native/HotkeyHost.exe"), "--parent", str(os.getpid())], stdin=subprocess.PIPE,
                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding="utf-8",
                bufsize=1, creationflags=0x08000000)
            self.native_payload = None
            self.hook_health=None
            self.hotkey_events.clear()
            self.hook_generation+=1
        self._native_config()

    def _native_config(self):
        if not self.native or self.native.poll() is not None:
            return 0
        guard=self.preview_input_guard
        guarded=guard and not guard.get('release')
        reader=getattr(getattr(self,'input_focus',None),'shortcut_context',None)
        search=reader() if callable(reader) else {}
        ime_reader=getattr(getattr(self,'input_language',None),'ready_context',None)
        ime_ready=ime_reader() if callable(ime_reader) else {}
        value={"enabled": self.settings["enabled"],
            "text_input": bool(self.panel_context and (self.panel_context.current['kind'] == 'text' or self.panel_context.current.get('input_focused')) or
                               callable(reader) and (search.get('state')=='input' or not self.visible and search.get('state')!='non-input')),
            "color_editor": bool(self.visible and (getattr(self,'color_editor_active',False) or getattr(self,'menu_tools_active',False))),
            "search_non_input":search.get('state')=='non-input',
            "editor_input":search.get('state')=='input',
            "search_root":search.get('root',0),"search_focus":search.get('focus',0),"search_epoch":search.get('epoch',0),
            "ime_ready":bool(ime_ready),"ime_root":ime_ready.get('root',0),"ime_focus":ime_ready.get('focus',0),"ime_epoch":ime_ready.get('epoch',0),
            "diagnostic_trace": bool(getattr(self,"diagnostic_trace",False)),
            "remote_compatibility": self.settings.get("remote_compatibility",False),
            "right_click_open": False,
            "hwnd": self.hwnd or 0, "visible": self.visible,
            "preview_guard_owner":guard['owner'] if guarded else 0,
            "preview_guard_panel":guard['panel'] if guarded else 0,
            "bindings": {name: list(chord(value)) for name,value in self.settings["shortcuts"].items()}}
        payload=json.dumps(value)
        with self.native_lock:
            if self.stop.is_set() or not self.native or self.native.poll() is not None:
                return 0
            # Re-read guard ownership after serialization: a late monitor must
            # never re-arm a guard whose known terminal already requested release.
            guard=self.preview_input_guard
            guarded=guard and not guard.get('release')
            value.update(preview_guard_owner=guard['owner'] if guarded else 0,
                         preview_guard_panel=guard['panel'] if guarded else 0)
            payload=json.dumps(value)
            if payload != self.native_payload:
                try:
                    self.native_sequence+=1
                    self.native.stdin.write(json.dumps({**value,'configuration_sequence':self.native_sequence}) + "\n")
                    self.native.stdin.flush()
                    self.native_payload = payload
                except (OSError, ValueError):
                    logging.exception("Native hotkey settings failed")
                    return 0
            return self.native_sequence

    def sync_native(self):
        # Called by the input observers as soon as a verdict changes. The hook
        # never waits for it; this only shortens the conservative window.
        self._native_config()

    def prepare_preview_input_guard(self,preview):
        owner=self.target
        hint=getattr(preview,'panel_hint',None) or (self.panel_context.current if self.panel_context else {})
        panel=self.panel_context.layer_list_window(hint.get('hwnd',0),owner) if self.panel_context else 0
        with self.window_lock:
            if self.mask_preview is not preview or owner!=self.target or not panel or not self._is_photoshop(owner):
                raise RuntimeError('图层点击保护未能识别列表；本次未开始预览')
            if self.preview_input_guard and self.preview_input_guard['preview'] is not preview:
                raise RuntimeError('上一项预览仍在收尾；本次未开始预览')
            self.preview_input_guard={'preview':preview,'owner':owner,'panel':panel,'epoch':preview.epoch,
                                      'terminal_check':getattr(preview,'execution_finished',None)}
        seq=self._native_config()
        with self.native_applied:
            ready=bool(seq) and self.native_applied.wait_for(lambda:self.native_applied_sequence>=seq,.5)
        if not ready:raise RuntimeError('图层点击保护未就绪；本次未开始预览')
        logging.info('Preview input guard armed: epoch=%s panel=%s sequence=%s',preview.epoch,panel,seq)

    def finish_preview_input_guard(self,preview,known=True,terminal_check=None):
        with self.window_lock:
            guard=self.preview_input_guard
            if not guard or guard['preview'] is not preview:return
            if not known:
                guard.update(unknown=True,terminal_check=terminal_check)
                self.preview_pending_feedback();return
            guard['release']=True
            guard.pop('unknown',None)
        seq=self._native_config()
        with self.native_applied:
            ready=bool(seq) and self.native_applied.wait_for(lambda:self.native_applied_sequence>=seq,.5)
        if ready:
            with self.window_lock:
                if self.preview_input_guard is guard:self.preview_input_guard=None
            logging.info('Preview input guard released: epoch=%s sequence=%s',preview.epoch,seq)
        self.preview_pending_feedback()

    def maintain_preview_input_guard(self):
        guard=self.preview_input_guard
        if not guard:return
        if not guard.get('release') and guard.get('terminal_check') and guard['terminal_check']():
            self.finish_preview_input_guard(guard['preview']);return
        if guard.get('release'):
            seq=self._native_config()
            if seq and self.native_applied_sequence>=seq:
                with self.window_lock:
                    if self.preview_input_guard is guard:self.preview_input_guard=None
        self.preview_pending_feedback()

    def _input_focus_verified(self,request,result):
        if request.get('need_ime') and result in (1,2) and self.input_language:
            self.input_language.verify(request,self._input_verify_reply)
        else:self._input_verify_reply(request,result)

    def _input_verify_reply(self,request,result):
        if self.input_focus:self.input_focus.report['last_verification']={'id':request['verify_id'],'result':result,'need_ime':request['need_ime'],'input_epoch':request['input_epoch'],'received_at_ms':round(time.time()*1000)}
        logging.info('Native input verification: request=%s result=%s need_ime=%s epoch=%s',request['verify_id'],result,request['need_ime'],request['input_epoch'])
        logging.info('Native input verification detail: guard=%s ime=%s gate=%s',getattr(self.input_focus,'report',{}).get('last_guard_verification'),getattr(self.input_language,'report',{}).get('last_ime_verification'),getattr(self.input_language,'report',{}).get('first_key_gate'))
        self._native_config()
        with self.native_lock:
            if self.stop.is_set() or not self.native or self.native.poll() is not None:return
            try:
                self.native.stdin.write(json.dumps({'verify_reply':request['verify_id'],'input_result':result})+'\n')
                self.native.stdin.flush()
            except (OSError,ValueError):logging.exception('Native input verification reply failed')

    def _native_events(self):
        while not self.stop.is_set():
            self._read_native_events()
            self.ready.clear()
            if self.stop.is_set():
                break
            self.message = "热键进程已停止，正在自动恢复"
            logging.error("Native hotkey process stopped; restarting")
            old = self.native
            if old.poll() is None:
                old.terminate()
                old.wait(3)
            old.stdin.close()
            old.stdout.close()
            delay = 1
            while not self.stop.wait(delay):
                try:
                    self._spawn_native()
                    break
                except OSError:
                    logging.exception("Native hotkey restart failed")
                    delay = min(delay * 2, 30)

    def _read_native_events(self):
        try:
            for line in self.native.stdout:
                event = json.loads(line)
                if event.get('type') in ('caps.trace','trigger.trace','search.trace'):
                    from .diagnostics import key_evidence
                    self.hotkey_events.append(key_evidence(event))
                if event.get("type") == "ready":
                    self.message = "C++ / Win32 原生热键 · 仅 Photoshop 前台生效"
                    self.frame_host=event.get('frame_hwnd',0)
                    with self.frame_grant_lock:self.frame_grants.clear()
                    self.ready.set()
                    if self.input_focus:self.input_focus.invalidate(event.get('input_epoch',0))
                elif event.get('type') == 'input.verify':
                    if self.input_focus:self.input_focus.verify(event,self._input_focus_verified)
                elif event.get('type') == 'panel.click':
                    if self.panel_context:
                        self.panel_context.record_click(event['hwnd'], event['root'])
                    if self.input_language:self.input_language.click(event['hwnd'],event['root'])
                    if self.input_focus:self.input_focus.click(event['hwnd'],event['root'],event.get('input_epoch'))
                elif event.get('type') == 'input.language.protect':
                    if self.input_language:self.input_language.protect()
                    if self.input_focus:self.input_focus.protect(event.get('input_epoch'))
                elif event.get('type') == 'input.language.activity':
                    if self.input_language:self.input_language.activity()
                    if self.input_focus:
                        if event.get('hwnd'):self.input_focus.focus_changed(event['hwnd'],event.get('input_epoch'))
                        else:self.input_focus.activity(event.get('input_epoch'))
                elif event.get('type') == 'input.language.edit_end':
                    if self.input_language:self.input_language.edit_end()
                    if self.input_focus:self.input_focus.edit_end(event.get('input_epoch'))
                elif event.get("type") == "action":
                    logging.info('Native hook action: %s cursor=%s', event['action'], event.get('cursor'))
                    if event.get('action')=='layer.frame' and event.get('native_non_input') is True:
                        with self.frame_grant_lock:
                            self.frame_grants[event.get('frame_id')]=dict(event.get('input_guard') or {})
                            while len(self.frame_grants)>8:self.frame_grants.pop(next(iter(self.frame_grants)))
                    args={key:event[key] for key in ('action','cursor','input_guard','frame_id') if key in event}
                    self.notify(args if len(args)>1 else event['action'])
                elif event.get('type') == 'trigger.trace':
                    from collections import deque
                    if not hasattr(self,'trigger_events'):self.trigger_events=deque(maxlen=64)
                    if getattr(self,'diagnostic_trace',False):self.trigger_events.append(event)
                    logging.info('Configured menu trigger decision: %s',event)
                elif event.get('type') == 'search.trace':
                    if self.input_focus:self.input_focus.report['last_search_trace']=event
                    logging.info('Native F input guard decision: %s',event)
                elif event.get('type') == 'caps.trace':
                    logging.info('CapsLock hook decision: %s', event)
                elif event.get('type') == 'hook.health':
                    self.hook_health=(time.monotonic(),dict(event))
                    if self.input_focus:self.input_focus.reconcile_epoch(event.get('input_epoch'))
                    logging.info('Native hook health: %s', event)
                elif event.get('type') == 'caps.state':
                    logging.info('CapsLock system state: %s', event)
                elif event.get('type') == 'activation.trace':
                    logging.info('Photoshop activation click: %s', event)
                elif event.get('type') == 'activation.foreground':
                    self.activation_snapshot=event
                    logging.info('Photoshop activation foreground: %s', event)
                elif event.get("type") == "configured":
                    with self.native_applied:
                        self.native_applied_sequence=max(self.native_applied_sequence,event.get('configuration_sequence',0))
                        self.native_applied.notify_all()
                    logging.debug("Native hotkey settings acknowledged")
                else:
                    logging.error("Native hotkey error: %s", event)
        except Exception:
            logging.exception("Native hotkey event stream failed")

    def _launch_overlay(self):
        if self.process and self.process.poll() is None:
            return
        self.hwnd = None
        self.menu_ready.clear()
        self.runtime.mkdir(parents=True, exist_ok=True)
        with (self.runtime / "overlay.log").open("a", encoding="utf-8") as log:
            self.process = subprocess.Popen(overlay_command(self.url, self.token, self.runtime),
                                            cwd=str(ROOT), stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log,
                                            text=True, encoding='utf-8', bufsize=1,
                                            creationflags=0x08000000)
        self.menu_thread = threading.Thread(target=self._menu_events, args=(self.process,), name='imgui-menu', daemon=True)
        self.menu_thread.start()
        if not self.menu_ready.wait(5):
            raise RuntimeError('Dear ImGui 菜单未就绪，请查看 overlay.log')

    def _menu_events(self, process):
        try:
            for line in process.stdout:
                event = json.loads(line)
                if event.get('type') == 'ready':
                    self.graphics=event.get('graphics',{})
                    self.attach(event['hwnd'], event['pid'], self.token)
                    inset=event.get('pie_right_inset')
                    self.pie_right_inset=inset if type(inset) is int and 0<inset<310 else None
                    self.menu_ready.set()
                elif event.get('type') == 'graphics':
                    self.graphics=event.get('graphics',{})
                    logging.info('Menu graphics: %s',self.graphics)
                elif event.get('type') == 'action':
                    logging.info('Dear ImGui menu action: %s', event['action'])
                    self.notify({'action':event['action'], 'menu_epoch':event.get('session',self.menu_epoch),
                        **({'params':event['params']} if 'params' in event else {}),
                        **({'after_action':event['after_action']} if 'after_action' in event else {}),
                        **({'return_to_menu':event['return_to_menu']} if 'return_to_menu' in event else {})})
                elif event.get('type') == 'selection':
                    logging.info('Menu release decision: %s', event)
        except Exception:
            logging.exception('Dear ImGui menu event stream failed')
        finally:
            if self.process is process:
                self.hwnd, self.visible = None, False
                self.menu_ready.clear()

    def event(self, event):
        if event.get('type') not in ('status', 'operation', 'notice', 'popup.prepare', 'document', 'diagnostic.progress','mask.preview','preview.progress','preview.menu.confirmed','menu.tools.opened','menu.tools.result','menu.tools.preview.result'):
            return
        preview=getattr(self,'mask_preview',None)
        if preview and event.get('type')=='preview.progress' and event.get('epoch')==preview.epoch:
            # A newly opened busy menu observes the same original transaction.
            event={**event,'epoch':self.menu_epoch}
        if preview and preview.epoch==self.menu_epoch and preview.phase=='preview' and not getattr(preview,'execution_exited',False):
            # Only the owning editor remains interactive. Other consumers still
            # receive the real busy/pending state and cannot submit operations.
            if event.get('type')=='status':
                event={**event,'data':{**event['data'],'photoshop':{**event['data']['photoshop'],
                    'busy':False,'uxp_recovery':{},'mask_preview':True}}}
            elif event.get('type')=='operation':event={**event,'busy':False}
        if preview and getattr(preview,'return_to_menu',False) and preview.phase=='commit' and not preview.closed:
            # Return only after the original receipt and guard release, never
            # on an intermediate bridge/status heartbeat.
            if event.get('type')=='operation':event={**event,'busy':True}
            elif event.get('type')=='status':event={**event,'data':{**event['data'],'photoshop':{**event['data']['photoshop'],'busy':True}}}
        if event.get('type')=='preview.progress':
            self.preview_pending_feedback()
        elif event.get('type')=='mask.preview' and not event.get('active'):
            self.preview_pending_feedback()
        self._send_menu_event(event)

    def preview_pending_feedback(self):
        """A separate click-through indicator, never a reopened menu or PS query."""
        with self.window_lock:
            preview=getattr(self,'mask_preview',None)
            guard=self.preview_input_guard
            active=bool((guard or preview and not preview.closed and not getattr(preview,'execution_exited',False) and preview.phase in ('commit','cancel'))
                        and not self.visible and (guard or self.settings['enabled'])
                        and self.target and getattr(self,'preview_hint_anchor',None))
            value={'type':'preview.pending','active':active}
            if active:
                progress=preview.progress(read=False) if preview else None
                if guard and (guard.get('unknown') or guard.get('release')):
                    progress={'stage':'unknown' if guard.get('unknown') else 'release','phase':'cancel','operation':'预览收尾','done':0,'total':0,'frame':0}
                if progress:
                    value.update({key:progress[key] for key in ('stage','phase','operation','done','total','frame')})
                    value.update(epoch=guard['epoch'] if guard else preview.epoch,owner=guard['owner'] if guard else self.target,anchor=self.preview_hint_anchor)
                else:
                    active=False;value['active']=False
            now=time.monotonic()
            if value==self.preview_hint_value and (not active or now-self.preview_hint_at<.5):return
            keys=('active','epoch','phase','stage')
            if tuple(value.get(k) for k in keys)!=tuple(self.preview_hint_value.get(k) for k in keys):
                logging.info('Preview finishing feedback: %s', {k:value[k] for k in
                    ('active','epoch','phase','stage','owner','anchor') if k in value})
            self.preview_hint_value=dict(value);self.preview_hint_at=now
            self.preview_hint_serial+=1;value['serial']=self.preview_hint_serial
        self._send_menu_event(value)

    def _send_menu_event(self, event):
        with self.menu_lock:
            if self.process and self.process.poll() is None:
                try:
                    self.process.stdin.write(json.dumps(event, ensure_ascii=False) + '\n')
                    self.process.stdin.flush()
                except (OSError, ValueError):
                    logging.exception('Dear ImGui menu update failed')

    def attach(self, hwnd, pid, token):
        import win32process
        if not self.process or self.process.poll() is not None or token != self.token or type(hwnd) is not int or type(pid) is not int:
            raise ValueError("无效的操作菜单连接")
        if pid != self.process.pid and process_parent(pid) != self.process.pid:
            raise ValueError("操作菜单进程不属于当前后台")
        if not self.gui.IsWindow(hwnd) or win32process.GetWindowThreadProcessId(hwnd)[1] != pid:
            raise ValueError("操作菜单窗口不匹配")
        self.hwnd = hwnd
        self.visible = False
        self.gui.ShowWindow(hwnd, 0)
        self.icons.apply(hwnd)
        self._native_config()

    def _workspace_root(self, hwnd):
        from .photoshop_windows import workspace_root
        return workspace_root(self.gui, hwnd)

    def _is_photoshop(self, hwnd):
        return bool(self._workspace_root(hwnd))

    @window_transition
    def _scope(self):
        inspection=getattr(self,"_stability_inspection",None)
        if inspection:
            try:inspection["check"]();return self._is_photoshop(inspection["root"])
            except RuntimeError:return False
        if not self.settings["enabled"]:
            return False
        hwnd = self.gui.GetForegroundWindow()
        if self._is_photoshop(hwnd):
            # Floating document windows can be hidden/replaced when PS changes
            # active documents. Keep popup ownership on the stable workspace.
            self.target = self._workspace_root(hwnd)
            self.caption_window = hwnd
            return True
        # Windows can retain our hidden popup as foreground while restoring
        # focus. It still belongs to this PS session, unlike a foreign app.
        owned = self._is_menu_window(hwnd)
        if not owned:
            self.restore_pending = False
        return bool((self.visible or self.restore_pending) and owned and self._is_photoshop(self.target))

    def _is_menu_window(self, hwnd):
        if not hwnd or not self.hwnd or not self.gui.IsWindow(hwnd):
            return False
        if hwnd == self.hwnd:
            return True
        import win32process
        try:
            owner = win32process.GetWindowThreadProcessId(self.hwnd)[1]
            return bool(owner and win32process.GetWindowThreadProcessId(hwnd)[1] == owner)
        except Exception:
            return False

    def focus_search_result(self, keep_menu=False):
        # A deliberate result click may activate PS; ordinary shortcuts keep
        # their existing foreground-only guard.
        roots=[]
        self.gui.EnumWindows(lambda hwnd,_:roots.append(hwnd) if self._is_photoshop(hwnd) and self._workspace_root(hwnd)==hwnd else None,None)
        if len(roots)!=1:raise RuntimeError('请仅保留一个可用的Photoshop工作区')
        root=roots[0]
        if not keep_menu:self.hide(restore=False)
        elif self._is_menu_window(self.gui.GetForegroundWindow()):
            import win32api
            with self.window_lock:
                window,epoch=self.hwnd,self.menu_epoch
                if not self.visible or not self.menu_tools_active or not 0<epoch<0x80000000:
                    raise RuntimeError('菜单会话已关闭，本次未执行')
                deadline=time.monotonic()+.5
                # Queue once; the native owner validates epoch and expiry before activation.
                packet=(epoch<<32)|((win32api.GetTickCount()+500)&0xffffffff)
                try:self.gui.PostMessage(window,0x8019,root,packet)
                except Exception as error:raise RuntimeError('菜单焦点交接未能提交，本次未执行') from error
            while True:
                foreground=self.gui.GetForegroundWindow()
                if self.hwnd!=window or self.menu_epoch!=epoch or not self.visible or not self.menu_tools_active:
                    raise RuntimeError('菜单会话已改变，本次未执行')
                if self._workspace_root(foreground)==root:break
                if not self._is_menu_window(foreground):raise RuntimeError('前台已改变，本次未执行')
                if time.monotonic()>=deadline:raise RuntimeError('菜单尚未将焦点交回PS，本次未执行；请先激活PS后重试')
                time.sleep(.01)
        if keep_menu and self._workspace_root(self.gui.GetForegroundWindow())!=root:
            raise RuntimeError('菜单未能将焦点交回PS或前台已改变，本次未执行')
        if self._workspace_root(self.gui.GetForegroundWindow())!=root:
            try:self.gui.SetForegroundWindow(root)
            except Exception:logging.info('Foreground activation not acknowledged; checking actual Photoshop focus',exc_info=True)
        if self._workspace_root(self.gui.GetForegroundWindow())!=root:
            # Cross-thread foreground activation is asynchronous; wait outside the menu lock.
            try:self.gui.SendMessageTimeout(root,0,0,0,2,200)
            except Exception as error:raise RuntimeError('Photoshop未及时响应，本次未执行') from error
        if self._workspace_root(self.gui.GetForegroundWindow())!=root:raise RuntimeError('Windows未允许激活Photoshop，本次未执行；请先激活PS后重试')
        self.require_action_focus()

    @window_transition
    def require_action_focus(self, allow_home=False):
        if not self._scope():
            raise RuntimeError("图层操作仅在 Photoshop 或由其唤出的操作菜单中可用")
        if self.panel_context:
            panel = self.panel_context.current if self.visible else self.panel_context.snapshot(self.target)
            if panel['kind'] == 'text' or panel.get('input_focused'):
                raise RuntimeError('正在编辑输入框，请先退出输入再执行图层操作')
            if panel['kind'] == 'home' and not allow_home:
                raise RuntimeError('当前为 Photoshop 首页，请进入文档后执行图层操作')

    def frame_input_context(self,identity,guard):
        with self.frame_grant_lock:grant=self.frame_grants.pop(identity,None)
        if grant and grant==guard:return {'state':'non-input',**grant}
        return self.input_focus.shortcut_context() if self.input_focus else None

    @window_transition
    def require_layer_frame(self, input_guard=None, reserved=False, accepted=None):
        self.require_action_focus()
        if reserved:
            sample=self.input_focus.native_sample() if self.input_focus else {}
            identity={key:sample.get(key) for key in ('root','focus')};identity['epoch']=getattr(self.input_focus,'native_epoch',None)
            if (not sample.get('workspace') or sample.get('blocked') or sample.get('state')=='input' or identity!=input_guard):
                raise RuntimeError('F触发后的原生焦点或输入世代已改变，本次未聚焦')
            return identity
        # Busy can clear the shared cache after dispatch reserved this press.
        observed=accepted if accepted is not None else self.input_focus.shortcut_context() if self.input_focus else {}
        if observed.get('state')!='non-input':raise RuntimeError('输入状态未确认，未聚焦图层')
        if input_guard is not None:
            if (not isinstance(input_guard,dict) or set(input_guard)!={'root','focus','epoch'} or
                    any(type(input_guard[k]) is not int or input_guard[k]<(0 if k=='epoch' else 1) for k in input_guard)):
                raise ValueError('无效的F聚焦输入身份')
            if any(observed.get(k)!=v for k,v in input_guard.items()):raise RuntimeError('F触发后的输入上下文已改变，本次未聚焦')
        identity={key:observed[key] for key in ('root','focus','epoch')}
        return self.require_layer_frame(identity,reserved=True) if accepted is not None else identity

    @window_transition
    def require_menu_tools(self, epoch):
        if not self.visible or epoch != self.menu_epoch or not self.menu_tools_active:
            raise RuntimeError('操作面板已关闭或重新打开，本次未执行')
        self.require_action_focus()

    @per_monitor_dpi
    def open_menu_tools(self, mode, epoch=None,input_guard=None):
        current=None
        try:
            with self.window_lock:
                if epoch is not None and (not self.visible or self.menu_epoch != epoch):
                    raise RuntimeError('菜单已关闭或重新打开')
                self.require_action_focus()
                if input_guard is not None:
                    if (not isinstance(input_guard,dict) or set(input_guard)!={'root','focus','epoch'} or
                        any(type(input_guard[k]) is not int or input_guard[k]<(0 if k=='epoch' else 1) for k in input_guard)):
                        raise ValueError('无效的F输入保护身份')
                    observed=self.input_focus.shortcut_context() if self.input_focus else {}
                    if any(observed.get(k)!=value for k,value in input_guard.items()):
                        raise RuntimeError('F触发后的输入上下文已改变，本次未打开')
                if mode=='search' and self.input_focus and self.input_focus.shortcut_context()['state']!='non-input':
                    raise RuntimeError('输入状态尚未确认可使用F，请先退出文字编辑')
                if getattr(self,'mask_preview',None):raise RuntimeError('请先完成颜色或羽化调整')
                if not self.visible:self.open(hold=True,keyboard_anchor=True)
                current=self.menu_epoch
                if mode=='split':
                    value=self.menu_layer_context or {}
                    if value.get('kind') not in ('text','smart') or len(value.get('layer_ids',[]))!=1 or value.get('locked'):
                        raise RuntimeError('请单选未锁定的文字图层或文字对象后打开文本工具')
                self.hold_active=False;self.menu_tools_active=True
                if self.input_focus:self.input_focus.menu_open(mode)
                self.menu_epoch+=1
                current=self.menu_epoch
                self.gui.SendMessage(self.hwnd,0x800d,1,self.menu_epoch)
                self.gui.SendMessage(self.hwnd,0x8010,1,0)
                import win32api
                anchor=win32api.GetCursorPos()
                rect=self.gui.GetWindowRect(self.target)
                work=win32api.GetMonitorInfo(win32api.MonitorFromWindow(self.target,2))['Work']
                dpi=self.icons.u.GetDpiForWindow(self.hwnd) or 96
                bounds=panel_bounds(rect,work,(round(anchor[0]+12*dpi/96),round(anchor[1]+12*dpi/96)),round((432 if mode=='search' else 632)*dpi/96),round((274 if mode=='search' else 780)*dpi/96))
                self.gui.SetWindowPos(self.hwnd,0,*bounds,0x4|0x10)
                scale=dpi/96
                self.preview_hint_anchor=[round(bounds[0]+bounds[2]*.5),round(bounds[1]+bounds[3]*.5)]
                current=self.menu_epoch
            sequence=self._native_config()
            with self.native_applied:
                ready=bool(sequence) and self.native_applied.wait_for(lambda:self.native_applied_sequence>=sequence,.5)
            if not ready:
                raise RuntimeError('面板输入保护未就绪，本次未打开')
            self.require_menu_tools(current)
            return current
        except Exception:
            if current is not None:self.hide_if_epoch(current)
            raise

    @per_monitor_dpi
    @window_transition
    def resize_menu_tools(self,epoch,rows):
        self.require_menu_tools(epoch)
        import win32api
        current=self.gui.GetWindowRect(self.hwnd);rect=self.gui.GetWindowRect(self.target)
        work=win32api.GetMonitorInfo(win32api.MonitorFromWindow(self.target,2))['Work']
        scale=(self.icons.u.GetDpiForWindow(self.hwnd) or 96)/96
        bounds=panel_bounds(rect,work,current[:2],current[2]-current[0],round((274+30*max(0,min(8,rows)-1))*scale))
        self.gui.SetWindowPos(self.hwnd,0,*bounds,0x4|0x10)

    @window_transition
    def require_move_context(self, name):
        # Canvas and generic menus have no reordering operations.
        # Keep standalone PS chords unchanged; block stale menu/native-hook
        # dispatches too, rather than just hiding their buttons.
        if (name in ('moveToTop','moveToBottom','moveUp','moveDown','moveDocumentTop','moveDocumentBottom') and self.visible
                and self.panel_context and self.panel_context.current['kind'] in ('generic','canvas')):
            raise RuntimeError('通用操作不提供图层移动，请在图层面板重新打开菜单')

    @window_transition
    def outside_click(self, point):
        if not self.visible or not self.hwnd:
            return False
        rect = self.gui.GetWindowRect(self.hwnd)
        if rect[0] <= point[0] < rect[2] and rect[1] <= point[1] < rect[3]:
            return False
        scoped = self._scope()
        self.visible = False
        self.notify("dismiss")
        return scoped

    def _monitor_layer_context(self):
        # Independent of UIA/focus inspection and the window lock. A slow panel
        # lookup must not leave the next menu with old selection eligibility.
        from .layer_context import unavailable
        while not self.stop.is_set():
            try:
                self.layer_context=self.layer_context_provider()
                self._cancel_closed_menu_context()
                self._publish_menu_context()
                self._finish_menu_context()
            except Exception:
                self.layer_context=unavailable()
                logging.exception('Layer cache monitor failed')
            if self.stop.wait(.04):break

    def _monitor(self):
        next_icon_refresh = 0
        while not self.stop.wait(.1):
            try:
                panel_target=None
                with self.window_lock:
                    scoped = self._scope()
                    if scoped:
                        self._refresh_document_name()
                    if not self.visible and self.panel_context and self._is_photoshop(self.gui.GetForegroundWindow()):
                        panel_target=self.target
                    if self.hwnd and not self.gui.IsWindow(self.hwnd):
                        self.hwnd, self.visible = None, False
                    if self.hwnd and time.monotonic() >= next_icon_refresh:
                        self.icons.apply(self.hwnd)
                        next_icon_refresh = time.monotonic() + 2
                    if self.visible and (not scoped or not self._is_photoshop(self.target)):
                        logging.info('Menu auto-hide: scoped=%s foreground=%s target=%s target_valid=%s',
                                     scoped, self.gui.GetForegroundWindow(), self.target, self._is_photoshop(self.target))
                        self.hide(restore=False)
                    self._native_config()
                if panel_target is not None:
                    self.panel_context.snapshot(panel_target,accept=lambda:
                        not self.visible and self.target==panel_target)
                    # Publish fresh input protection on this pass, rather
                    # than waiting an additional monitor interval.
                    self._native_config()
                self.maintain_preview_input_guard()
                self._close_completed_menu()
            except Exception:
                logging.exception("Window monitor failed")

    @window_transition
    def apply_settings(self, settings):
        self.settings = settings
        if not settings["enabled"]:
            self.hide(restore=False)
        self._native_config()

    @per_monitor_dpi
    def open(self, context=False, hold=False, origin=None, keyboard_anchor=False):
        if keyboard_anchor:return self._open(context,hold,origin,keyboard_anchor=True)
        return self._open(context, hold, origin) if origin is not None else self._open(context, hold)

    def begin_stability_menu(self, pid, check, observe_busy=False):
        """Owned read-only inspection; production popup/context path, no input injection."""
        import win32process
        check()
        if self.visible or getattr(self,'_stability_inspection',None):raise RuntimeError('菜单已打开，测试未覆盖它')
        roots=[]
        self.gui.EnumWindows(lambda hwnd,_:roots.append(hwnd) if self.gui.IsWindowVisible(hwnd) and win32process.GetWindowThreadProcessId(hwnd)[1]==pid and self._workspace_root(hwnd)==hwnd else None,None)
        if len(roots)!=1:raise RuntimeError('无法唯一识别 Photoshop 工作区')
        root=roots[0];panel=self.panel_context
        current=panel.snapshot(root)
        if current.get('input_focused') or current.get('kind')=='text':raise RuntimeError('Photoshop 输入保护生效，测试停止')
        candidates=[]
        def visit(hwnd,_):
            if self.gui.IsWindowVisible(hwnd) and self.gui.GetClassName(hwnd)=='PSViewC':
                selected=panel.layer_list_window(hwnd,root)
                if selected and selected not in candidates:candidates.append(selected)
        self.gui.EnumChildWindows(root,visit,None)
        if len(candidates)!=1:raise RuntimeError('请显示唯一的图层面板后开始测试')
        bounds=self.gui.GetWindowRect(candidates[0]);cursor=((bounds[0]+bounds[2])//2,(bounds[1]+bounds[3])//2)
        inspection={'root':root,'cursor':cursor,'panel':{'kind':'layers','source':'verified-stability-list','hwnd':candidates[0],'input_focused':False},'check':check,'previous_panel':dict(panel.current)}
        with self.window_lock:
            inspection['frames_start']=self.gui.SendMessage(self.hwnd,0x800b,0,0)
            self._stability_inspection=inspection;panel.current=inspection['panel']
            # The dedicated busy-wake lab observes real operation events. The
            # normal stability runner keeps its existing read-only suppression.
            self.gui.SendMessage(self.hwnd,0x801a,2 if observe_busy else 1,0)
            try:
                self._open(hold=True,origin=cursor,_inspection=inspection)
                self.gui.SendMessage(self.hwnd,0x8010,1,0)
                return self.menu_epoch
            except Exception:
                self.end_stability_menu();raise

    @window_transition
    def stability_menu_state(self, epoch):
        if not getattr(self,'_stability_inspection',None) or epoch!=self.menu_epoch:raise RuntimeError('测试菜单轮次已变化')
        return {'epoch':epoch,'frames_start':self._stability_inspection['frames_start'],'visible':bool(self.visible and self.gui.IsWindowVisible(self.hwnd)),
            'delivery':dict(getattr(self,'menu_context_delivery',{})),
            'context':dict(self.menu_layer_context or {}),
            'native':self.gui.SendMessage(self.hwnd,0x801b,epoch,0),
            'frames':self.gui.SendMessage(self.hwnd,0x800b,0,0)}

    @window_transition
    def end_stability_menu(self):
        inspection=getattr(self,'_stability_inspection',None)
        if not inspection:return
        self.hide(restore=False)
        self.gui.SendMessage(self.hwnd,0x801a,0,0)
        self.panel_context.current=inspection['previous_panel'];self._stability_inspection=None

    def _refresh_document_name(self):
        from .document_caption import document_name
        from .photoshop_windows import workspace_root
        try:
            root = workspace_root(self.gui, self.target)
            if not root:
                return
            source = self.caption_window if (self.caption_window and
                workspace_root(self.gui, self.caption_window) == root and
                self.gui.GetClassName(self.caption_window) == 'OWL.DocumentWindow') else root
            name = document_name(self.gui.GetWindowText(source))
            if name and name != self.document_caption:
                self.document_caption = name
                self.event({'type':'document', 'name':name})
        except Exception:
            logging.debug('Document caption unavailable', exc_info=True)

    @window_transition
    def _open(self, context=False, hold=False, origin=None, _inspection=None, keyboard_anchor=False):
        open_started = time.perf_counter()
        import win32api
        import win32con as c
        hwnd = self.gui.GetForegroundWindow()
        foreground = hwnd
        if _inspection:hwnd=_inspection["root"]
        if self.restore_pending and self._is_menu_window(hwnd) and self._is_photoshop(self.target):
            hwnd = self.target
        if not self.settings["enabled"] or not self._is_photoshop(hwnd):
            raise RuntimeError("请先让 Photoshop 主窗口获得焦点，再唤出操作菜单")
        from .photoshop_windows import workspace_root
        cursor = _inspection["cursor"] if _inspection else tuple(origin) if hold and origin is not None else win32api.GetCursorPos()
        root = workspace_root(self.gui, hwnd)
        rect = self.gui.GetWindowRect(root)
        hovered = self.gui.WindowFromPoint(cursor)
        over_workspace = workspace_root(self.gui, hovered) == root
        if over_workspace:
            floating = self.gui.GetWindowRect(self.gui.GetAncestor(hovered, 2))
            rect = (min(rect[0], floating[0]), min(rect[1], floating[1]),
                    max(rect[2], floating[2]), max(rect[3], floating[3]))
        if not (rect[0] <= cursor[0] < rect[2] and rect[1] <= cursor[1] < rect[3]):
            if not keyboard_anchor:raise RuntimeError("鼠标需要位于 Photoshop 窗口内")
        self.target = root
        self.caption_window = hwnd
        self._refresh_document_name()
        if not self.hwnd or not self.gui.IsWindow(self.hwnd):
            self._launch_overlay()
            raise RuntimeError("操作菜单正在初始化，请稍后再试")
        u = ctypes.WinDLL("user32", use_last_error=True)
        u.GetDpiForWindow.argtypes, u.GetDpiForWindow.restype = [W.HWND], W.UINT
        dpi_window = hovered if over_workspace else hwnd
        scale = (u.GetDpiForWindow(dpi_window) or 96) / 96
        monitor = (win32api.MonitorFromWindow(root,c.MONITOR_DEFAULTTONEAREST) if keyboard_anchor or not over_workspace
                   else win32api.MonitorFromPoint(cursor,c.MONITOR_DEFAULTTONEAREST))
        work = win32api.GetMonitorInfo(monitor)["Work"]
        panel = _inspection['panel'] if _inspection else self.panel_context.snapshot(hwnd) if self.panel_context else {'kind': 'generic'}
        self.menu_layer_context=None
        if self.layer_context_provider and panel.get('kind')!='home':
            from .layer_context import fresh,unavailable
            self.menu_layer_context=dict(self.layer_context) if fresh(self.layer_context) else unavailable()
        self.menu_context_pending=None
        if hold and panel.get('kind')=='layers' and self.menu_context_requester:
            # Reserve an identity-protected refresh, but do not touch UXP files
            # before showing. Windows read-sharing retries run on the existing
            # layer monitor, outside the window/input lock.
            self.menu_context_pending={'epoch':self.menu_epoch+1,'unpublished':True,'deadline':time.monotonic()+10}
            self.menu_context_delivery={'phase':'queued','epoch':self.menu_epoch+1}
            from .layer_context import unavailable
            self.menu_layer_context=unavailable()
        from .shape_paint import metadata
        shape=metadata((self.menu_layer_context or {}).get('shape_editor'))
        self.color_editor_active=shape_editor_allowed(panel,shape,hold,self.menu_layer_context)
        if not self.color_editor_active:shape={'available':False}
        width = min(self.settings["menu_width"],260) if context else self.settings["menu_width"]
        height = min(self.settings["menu_height"],240) if context else self.settings["menu_height"]
        bounds = panel_bounds(rect, work, cursor, round(width*scale), round(height*scale))
        anchor, editor_right = bounds[2]//2, False
        if hold:
            home = panel.get('kind') == 'home'
            color_editor = self.color_editor_active or bool(self.menu_context_pending)
            bounds, anchor, editor_right = pie_layout(rect, work, cursor, scale, home=home, color_editor=color_editor,
                                                     right_inset=self.pie_right_inset)
            if not home and rect[0] <= bounds[0] == work[0] and cursor[0]-work[0] < (650 if color_editor else 310)*scale:
                left = adjacent_left_work(work, bounds, round(scale*96))
                if left:
                    bounds, anchor, editor_right = pie_layout(rect, work, cursor, scale, color_editor=color_editor, left_work=left,
                                                             right_inset=self.pie_right_inset)
        # Recheck after setup: focus can change while a hotkey request is queued.
        if self.gui.GetForegroundWindow() != foreground or not self._is_photoshop(hwnd):
            raise RuntimeError("Photoshop 焦点已改变，已取消唤出")
        self.menu_tools_active = False
        self.visible = True
        self.preview_hint_anchor=[bounds[0]+anchor,
            round(bounds[1]+bounds[3]*.5)]
        self.preview_pending_feedback()
        self.restore_pending = False
        context_ready = time.perf_counter()
        logging.info('Menu panel context: %s', panel)
        # Apply context, input protection, layout and session together before
        # showing. Avoid three synchronous renderer-thread round trips.
        self.menu_epoch += 1
        flags = panel_flags(panel, context, hold)
        if self.menu_layer_context is not None:
            from .layer_context import flags as layer_flags
            flags |= layer_flags(self.menu_layer_context)
        if hold:flags |= (min(65534,anchor)+1)<<40 | int(editor_right)<<56
        self.gui.SendMessage(self.hwnd, 0x8011, flags, self.menu_epoch)
        if hold and origin is not None:self.gui.SendMessage(self.hwnd,0x8017,origin[0],origin[1])
        class CopyData(ctypes.Structure):
            _fields_=[('tag',ctypes.c_size_t),('size',W.DWORD),('data',ctypes.c_void_p)]
        payload=json.dumps(shape,ensure_ascii=False).encode('utf-8')
        if len(payload)>65534:raise RuntimeError('形状颜色信息超出菜单限制')
        buffer=ctypes.create_string_buffer(payload)
        packet=CopyData(0x53485045,len(payload)+1,ctypes.cast(buffer,ctypes.c_void_p))
        self.gui.SendMessage(self.hwnd,0x4a,0,ctypes.addressof(packet))
        show_popup(self.gui, self.hwnd, root, bounds, activate=not bool(_inspection))
        try:
            if not _inspection:self.gui.SetForegroundWindow(self.hwnd)
        except Exception:
            pass
        self._native_config()
        logging.info('Menu shown: hwnd=%s foreground=%s target=%s', self.hwnd, self.gui.GetForegroundWindow(), hwnd)
        if hold:logging.info('Caps placement: cursor=%s centre=%s work=%s bounds=%s dpi=%s editor_right=%s',
                             cursor,self.preview_hint_anchor,work,bounds,round(scale*96),editor_right)
        logging.info('Menu open timing: setup/context=%.1fms show=%.1fms total=%.1fms',
                     (context_ready-open_started)*1000, (time.perf_counter()-context_ready)*1000,
                     (time.perf_counter()-open_started)*1000)
        return {"message": "操作菜单已打开", "panel_context": panel,'layer_context':self.menu_layer_context}

    @window_transition
    def _close_completed_menu(self):
        # Only native success + idle + the 450ms feedback interval qualify.
        # No Photoshop reads or window activation are needed to retire feedback.
        if not self.visible or not self.hwnd or getattr(self,'preview_input_guard',None):return
        preview=getattr(self,'mask_preview',None)
        if preview and not preview.closed:return
        if self.gui.SendMessage(self.hwnd,0x8018,0,0)!=1:return
        return_mode=self.gui.SendMessage(self.hwnd,0x801c,0,0)
        if return_mode in (1,2):
            if not self._scope():self.hide(restore=False);return
            panel=self.panel_context.current if self.panel_context else {}
            if panel.get('kind')!='layers' or panel.get('input_focused'):return
            attempt='busy_wake_attempt' if return_mode==1 else 'hover_return_attempt'
            if getattr(self,attempt,None)==self.menu_epoch:return
            if not self.menu_context_requester:return
            setattr(self,attempt,self.menu_epoch)
            self.menu_context_pending={'epoch':self.menu_epoch,'unpublished':True,
                'busy_wake':return_mode==1,'hover_return':return_mode==2,'deadline':time.monotonic()+10}
            self.menu_context_delivery={'phase':'return-reading','epoch':self.menu_epoch}
            return
        self.hide(restore=self._scope())

    def _cancel_closed_menu_context(self):
        with self.window_lock:
            request=getattr(self,'menu_context_active',None)
            if not request or self.visible and request['epoch']==self.menu_epoch:return
        cancel=getattr(self,'menu_context_canceller',None)
        if cancel:cancel(request)
        with self.window_lock:
            if self.menu_context_active is request:self.menu_context_active=None

    def _publish_menu_context(self):
        with self.window_lock:
            pending=self.menu_context_pending
            if pending and (pending.get('resume') or not self.visible or self.menu_epoch!=pending['epoch']
                    or time.monotonic()>pending['deadline']):
                return self._apply_menu_context(pending,None,None)
            if not pending or not pending.get('unpublished'):return
            if pending.get('resume'):self.menu_context_pending=None;return
            if not self.visible or self.menu_epoch!=pending['epoch'] or self.stop.is_set() or time.monotonic()>pending['deadline']:
                self.menu_context_pending=None;return
            pending['unpublished']=False  # Exactly one publication per menu.
        try:request=self.menu_context_requester(pending['epoch'])
        except Exception:
            logging.exception('Fresh menu context request failed');request=None
        with self.window_lock:
            # Retain the published ID even if close/reopen raced publication.
            # The monitor cancels it outside this lock on its next iteration.
            if request:self.menu_context_active=request
            if self.menu_context_pending is not pending:return
            if request and self.visible and self.menu_epoch==pending['epoch'] and not self.stop.is_set():
                request['deadline']=min(request['deadline'],pending['deadline'])
                if pending.get('hover_return'):request['hover_return']=True
                if pending.get('busy_wake'):request['busy_wake']=True
                self.menu_context_pending=request
            else:self.menu_context_pending=None

    def _finish_menu_context(self):
        # Disk I/O and schema/catalog validation must not hold the same lock
        # used by Caps/open/hide. Revalidate the exact pending object on commit.
        with self.window_lock:
            pending=self.menu_context_pending
        if not pending:
            self._menu_context_file_cache=None
            return
        if 'id' not in pending:
            return self._apply_menu_context(pending,None,None)
        try:
            source=pending['folder']/'menu-context-result.json'
            def version(stat):
                return (stat.st_dev,stat.st_ino,stat.st_mtime_ns,stat.st_ctime_ns,stat.st_size)
            key=version(source.stat())
            cached=getattr(self,'_menu_context_file_cache',None)
            if cached and cached[0] is pending and cached[1]==key:
                snapshot,prepared=cached[2],copy.deepcopy(cached[3])
            else:
                snapshot=json.loads(source.read_text('utf-8-sig'))
                prepared=None
                if snapshot.get('menu_phase','completed') in ('basic','completed'):
                    from .layer_context import validate
                    value=validate(snapshot,pending['session'])
                    # validate already normalizes optional editor metadata.
                    prepared=(value,value.get('shape_editor',{'available':False}))
                # Cache only one stable file version for this exact request.
                # The application step still checks original age, identity,
                # epoch, deadline, input/focus and native acceptance each pass.
                self._menu_context_file_cache=(pending,key,snapshot,copy.deepcopy(prepared)) if version(source.stat())==key else None
        except (OSError,ValueError,KeyError,TypeError):return
        self._apply_menu_context(pending,snapshot,prepared)

    @window_transition
    def _apply_menu_context(self,pending,snapshot,prepared):
        if self.menu_context_pending is not pending:return
        if pending.get('resume'):self.menu_context_pending=None;return
        if not self.visible or self.menu_epoch!=pending['epoch'] or time.monotonic()>pending['deadline']:
            self.menu_context_delivery={'phase':'expired' if self.visible and self.menu_epoch==pending['epoch'] else 'closed','epoch':pending['epoch']}
            self.menu_context_pending=None;return
        if 'id' not in pending:return
        try:
            if snapshot.get('menu_request_id')!=pending['id']:return
            phase=snapshot.get('menu_phase','completed')
            if phase=='reading':
                if snapshot.get('schema')==1 and snapshot.get('session')==pending['session'] and not pending.get('applied_phase'):
                    # This is read acceptance, NOT a usable layer snapshot.
                    # A fixed deadline from publication cannot be renewed by
                    # repeated markers, heartbeats or an unfinished mutation.
                    pending['deadline']=max(pending['deadline'],pending.get('completion_deadline',pending['deadline']))
                    self.menu_context_delivery={'phase':'reading','epoch':pending['epoch']}
                return
            if phase not in ('basic','completed') or pending.get('applied_phase')==phase:return
            if pending.get('hover_return') and (phase!='completed' or getattr(self,'preview_input_guard',None)
                    or getattr(self,'mask_preview',None) and not self.mask_preview.closed):return
            if pending.get('busy_wake') and (getattr(self,'preview_input_guard',None)
                    or getattr(self,'mask_preview',None) and not self.mask_preview.closed):return
            from .layer_context import fresh,flags
            if prepared is None:return
            value,shape=prepared
            if not fresh(value):return
            if value.get('kind') in ('unknown','none'):return
            if pending.get('applied_phase')=='basic' and any(
                    value.get(key)!=(self.menu_layer_context or {}).get(key) for key in ('session','document_id','layer_ids')):
                self.menu_context_pending=None;return
            panel=self.panel_context.current
            if panel.get('kind')!='layers' or panel.get('input_focused') or not self._scope():return
            active=shape_editor_allowed(panel,shape,True,value)
            envelope={'epoch':pending['epoch'],'flags':flags(value),'shape':shape if active else {'available':False}}
            if pending.get('hover_return'):envelope.update(hover_return=True,next_epoch=self.menu_epoch+1)
            if pending.get('busy_wake'):envelope.update(busy_wake=True,next_epoch=self.menu_epoch+1)
            payload=json.dumps(envelope).encode('utf-8')
            editor_omitted=len(payload)+1>MAX_CONTEXT_PACKET_BYTES
            if editor_omitted:
                # Optional editor data must not strand fresh basic layer
                # actions. No PS request is retried or snapshot age extended.
                active=False
                payload=json.dumps({**envelope,'shape':{'available':False}}).encode('utf-8')
            class CopyData(ctypes.Structure):
                _fields_=[('tag',ctypes.c_size_t),('size',W.DWORD),('data',ctypes.c_void_p)]
            buffer=ctypes.create_string_buffer(payload);packet=CopyData(0x4d435458,len(payload)+1,ctypes.cast(buffer,ctypes.c_void_p))
            accepted=bool(self.gui.SendMessage(self.hwnd,0x4a,0,ctypes.addressof(packet)))
            self.menu_context_delivery={'phase':'applied' if accepted else 'native-rejected','epoch':pending['epoch'],
                'menu_phase':phase,'kind':value['kind'],'selection_count':len(value['layer_ids']),
                'packet_bytes':len(payload)+1,'selection_age_ms':max(0,round(time.time()*1000-value['plugin_time_ms'])),
                'accepted':accepted,'editor_omitted':editor_omitted,'request_id':pending['id']}
            if accepted:
                if pending.get('hover_return'):self.menu_epoch+=1
                if pending.get('busy_wake'):
                    self.menu_epoch+=1;self.hold_active=False
                    pending['epoch']=self.menu_epoch;pending.pop('busy_wake',None)
                    self.menu_context_delivery['epoch']=self.menu_epoch
                self.menu_layer_context=dict(value);self.color_editor_active=active
                if phase=='completed':self.menu_context_pending=None
                else:
                    pending['applied_phase']=phase
                    # A matching fresh basic result proves acceptance. Give its
                    # optional read a bounded completion window; never rewrite
                    # the request expiry or extend any modifying operation.
                    pending['deadline']=max(pending['deadline'],pending.get('completion_deadline',pending['deadline']))
                self._native_config()
        except (OSError,ValueError,KeyError):return

    def require_menu_layer_target(self,name,target):
        if name in ('shape.paint','shape.copyPaint','shape.pastePaint') and not getattr(self,'color_editor_active',False):
            from .tasks import TaskCancelled
            raise TaskCancelled('形状颜色仅在图层面板菜单可用；本次未修改')
        if self.layer_context_provider:
            from .layer_context import require_menu_target
            require_menu_target(self.menu_layer_context,name,target)

    def context(self):
        return self.open(context=True)

    @window_transition
    def require_home(self):
        self.require_action_focus(allow_home=True)
        from .photoshop_windows import workspace_root
        root = workspace_root(self.gui, self.target or self.gui.GetForegroundWindow())
        if not root or not self.panel_context or not self.panel_context.home_visible(root):
            raise RuntimeError('Photoshop 已离开首页，请重新打开菜单选择操作')
        return root

    @window_transition
    def new_document(self):
        root = self.require_home()
        self.hide()
        from .native_commands import open_new_dialog
        return open_new_dialog(self.gui, root)

    @window_transition
    def hold_begin(self, cursor=None):
        if self.visible:
            self.hide()
        result = self.open(hold=True,origin=cursor) if cursor is not None else self.open(hold=True)
        self.hold_active = True
        return result

    @window_transition
    def hold_release(self, cursor=None):
        if not self.hold_active or not self.visible:
            return {"message": "操作菜单已收起"}
        self.hold_active = False
        if not self._scope():
            return self.hide(restore=False)
        # Resolve the physical cursor against rendered row rectangles in the
        # menu process; keyboard selection and stale hover never execute.
        if cursor is None:
            # Compatibility callers still resolve in the renderer; production
            # hook releases provide a frozen physical-key-up position.
            self.gui.SendMessage(self.hwnd, 0x800e, 0, 0)
        else:
            self.gui.SendMessage(self.hwnd, 0x8015, cursor[0], cursor[1])
        return {"message": "已结束菜单选择"}

    @window_transition
    def hold_pin(self):
        if not self.visible or not self.hold_active:
            return {"message": "操作菜单已收起"}
        if not self._scope():
            return self.hide(restore=False)
        self.hold_active = False
        self.gui.SendMessage(self.hwnd, 0x8010, 1, 0)
        return {"message": "操作菜单已固定"}

    @window_transition
    def hold_cancel(self):
        if self.hold_active:
            return self.hide(restore=False)
        return {"message": "操作菜单已收起"}

    @window_transition
    def hide(self, restore=True):
        preview=getattr(self,'mask_preview',None)
        if preview:
            preview.cancel()
            # Explicit close hides immediately. Cancellation/restoration keeps
            # its original transaction and remains observable in the backend.
            preview.feedback_shown=True

        # Hiding the window can activate a different application's window.
        # Capture ownership before ShowWindow, including ImGui submenus.
        foreground = self.gui.GetForegroundWindow()
        owned_foreground = self._is_menu_window(foreground)
        input_panel_closed = self.menu_tools_active
        # A focus-neutral close must not strand our own hidden foreground.
        # Preserve session ownership until an actual foreign foreground is
        # observed; restore controls focus movement, not session validity.
        self.restore_pending = bool(owned_foreground and self.target)
        self.visible = False
        self.hold_active = False
        self.menu_tools_active = False
        if self.hwnd and self.gui.IsWindow(self.hwnd):
            self.gui.ShowWindow(self.hwnd, 0)
        self._native_config()
        if restore and owned_foreground and self._is_photoshop(self.target):
            try:
                if hasattr(self.gui,'PostMessage'):
                    self.gui.PostMessage(self.hwnd,0x8019,self.target,0)
                else:self.gui.SetForegroundWindow(self.target)
                if input_panel_closed and self.input_language:self.input_language.edit_end()
            except Exception:
                logging.info('Photoshop focus restore refused: before=%s target=%s', foreground, self.target)
        self.preview_pending_feedback()
        logging.info('Menu hidden: restore=%s before=%s after=%s', restore, foreground, self.gui.GetForegroundWindow())
        return {"message": "操作菜单已收起"}

    @window_transition
    def hide_if_epoch(self,epoch,restore=True):
        if self.visible and self.menu_epoch==epoch:return self.hide(restore=restore)

    def dismiss(self):
        with self.window_lock:
            inspection=getattr(self,'_stability_inspection',None)
            if inspection and not self._is_photoshop(self.gui.GetForegroundWindow()):
                try:
                    inspection['check']()
                    return {'message':'后台只读菜单测试继续'}
                except RuntimeError:pass
        return self.hide(restore=False)

    @window_transition
    def finish_preview_focus(self, preview):
        """Complete only our hidden-menu handoff after a known host terminal."""
        if (getattr(self,'mask_preview',None) is not preview or self.visible
                or not self.restore_pending or not self.target):return
        foreground=self.gui.GetForegroundWindow()
        if self._is_photoshop(foreground):
            self.restore_pending=False;return
        if not self._is_menu_window(foreground):
            self.restore_pending=False;return
        if not self._is_photoshop(self.target):return
        # No foreign application is activated over; a refused OS request does
        # not justify injected keys, thread attachment or retry loops.
        try:
            if self.hwnd and hasattr(self.gui,'SendMessageTimeout'):
                self.gui.SendMessageTimeout(self.hwnd,0x8019,self.target,0,2,100)
            else:self.gui.SetForegroundWindow(self.target)
        except Exception:logging.info('Preview focus handoff was refused')
        after=self.gui.GetForegroundWindow()
        if self._is_photoshop(after):self.restore_pending=False
        logging.info('Preview terminal focus handoff: before=%s after=%s target=%s',foreground,after,self.target)

    @window_transition
    def toggle(self):
        if self.visible:
            # Closing is a menu action, not a script action: text-input guards
            # must not prevent CapsLock from closing a paused menu.
            if not self._scope():
                raise RuntimeError('请先让 Photoshop 或其菜单获得焦点')
            if getattr(self,'menu_tools_active',False):return self.hide()
            if getattr(self,'color_editor_active',False):
                self.gui.SendMessage(self.hwnd,0x8016,0,0)
                return {'message':'已结束形状颜色调整'}
            return self.hide()
        return self.open()

    def close(self):
        self.stop.set()
        if self.input_language:self.input_language.thread.join(2)
        if self.input_focus:self.input_focus.thread.join(2)
        if self.native:
            try:
                self.native.stdin.close()
                self.native.wait(3)
            except (OSError, subprocess.TimeoutExpired):
                self.native.terminate()
                self.native.wait(3)
            self.native.stdout.close()
        for name in ("thread", "monitor", "layer_monitor"):
            if getattr(self, name, None):
                getattr(self, name).join(2)
        if self.process and self.process.poll() is None:
            if self.hwnd and self.gui.IsWindow(self.hwnd):
                self.gui.PostMessage(self.hwnd, 0x0010, 0, 0)
            try:
                self.process.wait(3)
            except subprocess.TimeoutExpired:
                self.process.terminate()
                self.process.wait(3)
        if self.icons:
            self.icons.close()
        if self.menu_thread:
            self.menu_thread.join(2)
        if self.process:
            for stream in (self.process.stdin, self.process.stdout):
                if stream:
                    stream.close()


class Tray:
    def __init__(self, notify):
        self.notify, self.hwnd = notify, None
        self.ready = threading.Event()
        self.error = ""

    def start(self):
        self.thread = threading.Thread(target=self._run, name="photoshop-tray", daemon=True)
        self.thread.start()
        self.ready.wait(3)

    def _run(self):
        import win32api
        import win32con as c
        import win32gui as g
        icon_added = False
        icons = WindowIcons()
        instance, name = win32api.GetModuleHandle(None), "PhotoshopMenuTray"
        taskbar_created = g.RegisterWindowMessage("TaskbarCreated")
        icons.u.SetTimer.argtypes, icons.u.SetTimer.restype = [W.HWND, ctypes.c_size_t, W.UINT, W.LPVOID], ctypes.c_size_t
        icons.u.KillTimer.argtypes, icons.u.KillTimer.restype = [W.HWND, ctypes.c_size_t], W.BOOL
        retries = 0
        def add_icon():
            nonlocal icon_added, retries
            icons.u.KillTimer(self.hwnd, 1)
            try:
                tray_dpi = icons.u.GetDpiForWindow(g.FindWindow('Shell_TrayWnd', None)) or 96
                tray_icon = icons.load(max(16, round(16 * tray_dpi / 96)))
                data = (self.hwnd, 1, g.NIF_ICON | g.NIF_MESSAGE | g.NIF_TIP, c.WM_USER + 1, tray_icon, TITLE)
                if icon_added:
                    try:
                        g.Shell_NotifyIcon(g.NIM_MODIFY, data)
                    except Exception:
                        g.Shell_NotifyIcon(g.NIM_ADD, data)
                else:
                    g.Shell_NotifyIcon(g.NIM_ADD, data)
                icon_added, self.error = True, ""
            except Exception as exc:
                self.error = str(exc)
                logging.exception("Tray icon registration failed")
                if retries:
                    retries -= 1
                    if not icons.u.SetTimer(self.hwnd, 1, 1000, None):
                        self.error += "; tray retry timer failed"
        def procedure(hwnd, message, wparam, lparam):
            nonlocal retries
            if message == taskbar_created:
                retries = 5
                add_icon()
                return 0
            if message == c.WM_TIMER and wparam == 1:
                add_icon()
                return 0
            if message == c.WM_USER + 1:
                if lparam == c.WM_LBUTTONDBLCLK:
                    self.notify("control")
                elif lparam == c.WM_RBUTTONUP:
                    menu = g.CreatePopupMenu()
                    try:
                        g.AppendMenu(menu, c.MF_STRING, 1, "打开后台管理")
                        g.AppendMenu(menu, c.MF_STRING, 2, "退出服务")
                        g.SetForegroundWindow(hwnd)
                        selected = g.TrackPopupMenu(menu, c.TPM_RETURNCMD | c.TPM_RIGHTBUTTON, *win32api.GetCursorPos(), 0, hwnd, None)
                        if selected:
                            self.notify("control" if selected == 1 else "quit")
                        g.PostMessage(hwnd, c.WM_NULL, 0, 0)
                    finally:
                        g.DestroyMenu(menu)
                return 0
            if message == c.WM_CLOSE:
                g.DestroyWindow(hwnd)
                return 0
            if message == c.WM_DESTROY:
                g.PostQuitMessage(0)
                return 0
            return g.DefWindowProc(hwnd, message, wparam, lparam)
        try:
            wc = g.WNDCLASS()
            wc.hInstance, wc.lpszClassName, wc.lpfnWndProc = instance, name, procedure
            wc.hIcon = icons.load(32)
            g.RegisterClass(wc)
            self.hwnd = g.CreateWindow(name, name, 0, 0, 0, 0, 0, 0, 0, instance, None)
            retries = 5
            add_icon()
            self.ready.set()
            g.PumpMessages()
        except Exception as exc:
            self.error = str(exc)
            logging.exception("Tray failed")
        finally:
            if self.hwnd:
                icons.u.KillTimer(self.hwnd, 1)
            if icon_added:
                try:
                    g.Shell_NotifyIcon(g.NIM_DELETE, (self.hwnd, 1))
                except Exception:
                    pass
            self.hwnd = None
            self.ready.set()
            try:
                g.UnregisterClass(name, instance)
            except Exception:
                pass
            icons.close()

    def close(self):
        if self.hwnd:
            import win32gui
            win32gui.PostMessage(self.hwnd, 0x0010, 0, 0)
        if getattr(self, "thread", None):
            self.thread.join(3)
