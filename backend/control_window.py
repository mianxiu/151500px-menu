"""Independent management window; no Photoshop focus requirement."""
import os
from pathlib import Path
import subprocess
import time
import webbrowser
import sys
import ctypes
import logging
from .window_icons import WindowIcons
from .paths import runtime_dir

TITLE = "151500px-menu — 后台管理"


class ControlWindow:
    def __init__(self, url, runtime=None):
        self.url = url
        self.runtime = Path(runtime) if runtime else runtime_dir()
        self.icons = None
        self.hwnd = None
        self.process = None
        self.owned_pids = set()
        self.edge = next((p for p in (
            Path(os.environ.get("PROGRAMFILES(X86)", "C:/Program Files (x86)")) / "Microsoft/Edge/Application/msedge.exe",
            Path(os.environ.get("PROGRAMFILES", "C:/Program Files")) / "Microsoft/Edge/Application/msedge.exe",
        ) if p.is_file()), None)

    def _profile_pids(self):
        """Only accept Edge processes launched with this exact private profile."""
        import pythoncom
        import win32com.client
        profile = os.path.normcase(str((self.runtime / 'control-profile').resolve()))
        shell = ctypes.WinDLL('shell32', use_last_error=True)
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        shell.CommandLineToArgvW.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
        shell.CommandLineToArgvW.restype = ctypes.POINTER(ctypes.c_wchar_p)
        kernel.LocalFree.argtypes = [ctypes.c_void_p]
        def collect():
            result = set()
            wmi = win32com.client.GetObject('winmgmts:')
            for process in wmi.ExecQuery("SELECT ProcessId, CommandLine FROM Win32_Process WHERE Name='msedge.exe'"):
                if not process.CommandLine:
                    continue
                count = ctypes.c_int()
                args = shell.CommandLineToArgvW(str(process.CommandLine), ctypes.byref(count))
                if not args:
                    continue
                try:
                    for index in range(count.value):
                        arg = args[index]
                        if arg.startswith('--user-data-dir=') and os.path.normcase(arg.split('=',1)[1]) == profile:
                            result.add(int(process.ProcessId))
                finally:
                    kernel.LocalFree(args)
            return result
        pythoncom.CoInitialize()
        try:
            # Release all WMI wrappers inside collect before uninitializing COM.
            return collect()
        finally:
            pythoncom.CoUninitialize()

    def _owns(self, hwnd):
        import win32gui as g
        import win32process
        return bool(hwnd and g.IsWindow(hwnd) and win32process.GetWindowThreadProcessId(hwnd)[1] in self.owned_pids)

    def _find_window(self):
        import win32gui as g
        found = []
        def candidate(hwnd, unused):
            if g.GetWindowText(hwnd) == TITLE and self._owns(hwnd):
                found.append(hwnd)
        g.EnumWindows(candidate, None)
        return found[0] if found else None

    def open(self):
        if os.name != "nt" or not self.edge:
            webbrowser.open(self.url)
            return {"message": "后台管理已在浏览器打开"}
        import win32gui as g
        if self.icons is None:
            self.icons = WindowIcons()
        if not self._owns(self.hwnd):
            try:
                self.owned_pids = self._profile_pids()
            except Exception:
                logging.exception('Could not enumerate private Edge profile')
            self.hwnd = self._find_window()
        if not self.hwnd:
            # External Edge must use its own DLLs rather than the frozen bundle's.
            kernel = ctypes.WinDLL("kernel32", use_last_error=True) if getattr(sys,"frozen",False) else None
            if kernel:
                kernel.SetDllDirectoryW.argtypes = [ctypes.c_wchar_p]
                kernel.SetDllDirectoryW(None)
            try:
                self.process = subprocess.Popen([str(self.edge), "--app=" + self.url,
                                  "--user-data-dir=" + str((self.runtime / "control-profile").resolve()),
                                  "--no-first-run", "--disable-extensions",
                                  "--disable-background-mode", "--window-size=1120,820"],
                                  stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                  stderr=subprocess.DEVNULL, creationflags=0x08000000)
                self.owned_pids.add(self.process.pid)
            finally:
                if kernel:
                    kernel.SetDllDirectoryW(sys._MEIPASS)
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                self.hwnd = self._find_window()
                if self.hwnd:
                    break
                time.sleep(.1)
            if not self.hwnd:
                # Edge can delegate to an already running process of this profile.
                self.owned_pids.update(self._profile_pids())
                self.hwnd = self._find_window()
            if not self.hwnd:
                code = self.process.poll()
                raise RuntimeError(f'后台管理窗口未成功启动（Edge 退出码 {code}），可访问 {self.url}')
        if self.hwnd:
            self.icons.apply(self.hwnd)
            g.ShowWindow(self.hwnd, 9)
            try:
                g.SetForegroundWindow(self.hwnd)
            except Exception:
                pass
        return {"message": "后台管理已打开"}

    def refresh_icon(self):
        if self.hwnd and self.icons and self._owns(self.hwnd):
            self.icons.apply(self.hwnd)

    def close(self):
        if self.hwnd and self.icons and self._owns(self.hwnd):
            import win32gui as g
            self.icons.detach(self.hwnd)
            if g.IsWindow(self.hwnd):
                g.PostMessage(self.hwnd, 0x0010, 0, 0)
        if self.icons:
            self.icons.close()
