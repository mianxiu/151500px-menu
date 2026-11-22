"""Scoped Windows placement/resource controls for opt-in, low-impact PS tests.

No keyboard/mouse injection, foreground activation, preference changes or kill.
The Codex window is observed only to choose a monitor. No Codex UI is operated.
"""
import json
from pathlib import Path
import threading
import time

import win32api
import win32con
import win32gui
import win32process


def executable(pid):
    handle = win32api.OpenProcess(0x410, False, pid)
    try:
        return win32process.GetModuleFileNameEx(handle, 0)
    finally:
        handle.Close()


def inventory():
    monitors = [dict(win32api.GetMonitorInfo(m), handle=int(m))
                for m, _, _ in win32api.EnumDisplayMonitors()]
    windows = []

    def visit(hwnd, unused):
        if not win32gui.IsWindowVisible(hwnd):
            return
        pid = win32process.GetWindowThreadProcessId(hwnd)[1]
        try:
            path = executable(pid)
        except Exception:
            return
        name = Path(path).name.lower()
        codex = 'openai.codex_' in path.lower() and name in ('chatgpt.exe', 'codex.exe')
        if name not in ('photoshop.exe', 'chrome.exe') and not codex:
            return
        windows.append(dict(hwnd=hwnd, pid=pid, path=path,
                            role='codex' if codex else name[:-4],
                            cls=win32gui.GetClassName(hwnd),
                            rect=win32gui.GetWindowRect(hwnd),
                            placement=win32gui.GetWindowPlacement(hwnd),
                            monitor=int(win32api.MonitorFromWindow(hwnd, 2))))
    win32gui.EnumWindows(visit, None)
    return dict(monitors=monitors, windows=windows,
                foreground=win32gui.GetForegroundWindow())


def select(state):
    # Native notification/toast windows share the Codex process; only the
    # returned Chromium main window identifies the intended secondary monitor.
    codex = [w for w in state['windows'] if w['role'] == 'codex' and w.get('cls') == 'Chrome_WidgetWin_1']
    ps = [w for w in state['windows'] if w['role'] == 'photoshop' and w['cls'] == 'Photoshop']
    if len(codex) != 1 or len(ps) != 1:
        raise RuntimeError('Expected exactly one visible Codex window and Photoshop main window')
    monitor = next(m for m in state['monitors'] if m['handle'] == codex[0]['monitor'])
    if monitor['Flags'] & 1:
        raise RuntimeError('Codex is on the primary display; quiet tests require a secondary display')
    return ps[0], monitor


def save(path, value):
    path = Path(path)
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, indent=2), 'utf-8')
    deadline=time.monotonic()+.25
    while True:
        try:
            temporary.replace(path)
            return
        except PermissionError as error:
            if getattr(error,'winerror',None) not in (5,32) or time.monotonic()>=deadline:
                raise
            time.sleep(.004)


def place(state, journal, *, rightmost=False):
    if rightmost:
        hosts = [w for w in state['windows'] if w['role'] == 'photoshop' and w['cls'] == 'Photoshop']
        if len(hosts) != 1 or not state['monitors']:
            raise RuntimeError('Expected one Photoshop main window and at least one display')
        ps = hosts[0]
        monitor = max(state['monitors'], key=lambda item: (item['Monitor'][2], item['Monitor'][0]))
    else:
        ps, monitor = select(state)
    if not rightmost and win32gui.GetForegroundWindow() == ps['hwnd']:
        raise RuntimeError('Photoshop is in use; no window moved')
    # Persist original placements before moving. Retain Z-order and activation.
    owned = [w for w in state['windows'] if w['pid'] == ps['pid'] and
             w['cls'] in ('Photoshop', 'OWL.Dock')]
    save(journal, dict(original=owned, target=monitor))
    for window in owned:
        if window['monitor'] == monitor['handle']:
            continue
        source = next(m for m in state['monitors'] if m['handle'] == window['monitor'])
        left, top, right, bottom = window['rect']
        x0, y0, x1, y1 = monitor['Work']
        width, height = min(right-left, x1-x0), min(bottom-top, y1-y0)
        x = max(x0, min(left-source['Work'][0]+x0, x1-width))
        y = max(y0, min(top-source['Work'][1]+y0, y1-height))
        win32gui.SetWindowPos(window['hwnd'], 0, x, y, width, height,
                              win32con.SWP_NOACTIVATE | win32con.SWP_NOZORDER |
                              win32con.SWP_NOOWNERZORDER)
    after = inventory()
    remaining = [w for w in after['windows'] if w['pid'] == ps['pid'] and
                 w['cls'] in ('Photoshop', 'OWL.Dock') and w['monitor'] != monitor['handle']]
    if remaining:
        raise RuntimeError('Some Photoshop panels remain outside the target display')
    return dict(target=monitor['Device'], foreground_before=state['foreground'],
                foreground_after=after['foreground'], windows=owned)


class ResourceLease:
    """Handle-bound rollback, including a time-limited watchdog; never kills PS.

    Two logical processors simulate restricted CPU concurrency, not an exact i5.
    No memory/GPU/disk caps or system power settings are changed.
    """
    def __init__(self, pid, journal, seconds=120, allow_self=False):
        self.pid, self.journal, self.seconds = pid, Path(journal), seconds
        self.allow_self = allow_self
        self.lock = threading.Lock()
        self.done = threading.Event()
        self.applied = False
        self.expired = False

    def __enter__(self):
        self.handle = win32api.OpenProcess(0x610, False, self.pid)
        try:
            path = win32process.GetModuleFileNameEx(self.handle, 0)
            if not self.allow_self and Path(path).name.lower() != 'photoshop.exe':
                raise RuntimeError('Resource lease only accepts Photoshop.exe')
            self.original_affinity, _ = win32process.GetProcessAffinityMask(self.handle)
            self.original_priority = win32process.GetPriorityClass(self.handle)
            bits = [1 << n for n in range(64) if self.original_affinity & (1 << n)]
            if len(bits) < 2:
                raise RuntimeError('At least two allowed logical processors are required')
            self.affinity = sum(bits[-2:])
            self.record = dict(pid=self.pid, executable=path,
                               creation_time=str(win32process.GetProcessTimes(self.handle)['CreationTime']),
                               original_affinity=self.original_affinity,
                               original_priority=self.original_priority,
                               test_affinity=self.affinity, test_priority=0x4000,
                               logical_processors=2, state='applying')
            save(self.journal, self.record)
            self.applied = True
            win32process.SetProcessAffinityMask(self.handle, self.affinity)
            win32process.SetPriorityClass(self.handle, 0x4000)
            assert win32process.GetProcessAffinityMask(self.handle)[0] == self.affinity
            assert win32process.GetPriorityClass(self.handle) == 0x4000
            self.record['state'] = 'active'
            save(self.journal, self.record)
            self.watchdog = threading.Thread(target=self._watch, daemon=True)
            self.watchdog.start()
            return self
        except BaseException:
            try:
                if self.applied:
                    self.restore()
            finally:
                self.handle.Close()
            raise

    def _watch(self):
        if not self.done.wait(self.seconds):
            self.expired = True
            self.restore()

    def restore(self):
        with self.lock:
            if not self.applied:
                return
            # Do not overwrite a manual resource change made during the run.
            affinity = win32process.GetProcessAffinityMask(self.handle)[0]
            priority = win32process.GetPriorityClass(self.handle)
            if affinity == self.affinity:
                win32process.SetProcessAffinityMask(self.handle, self.original_affinity)
            if priority == 0x4000:
                win32process.SetPriorityClass(self.handle, self.original_priority)
            self.record.update(state='restored', expired=self.expired,
                               final_affinity=win32process.GetProcessAffinityMask(self.handle)[0],
                               final_priority=win32process.GetPriorityClass(self.handle))
            save(self.journal, self.record)
            self.applied = False

    def __exit__(self, *unused):
        self.done.set()
        try:
            self.restore()
            self.watchdog.join()
        finally:
            self.handle.Close()
