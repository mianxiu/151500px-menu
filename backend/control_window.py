"""Independent management window backed by the Windows WebView2 Runtime."""
import ctypes
import os
from pathlib import Path
import subprocess
import sys
import time
import webbrowser

from .paths import ROOT, runtime_dir


TITLE = "151500px-menu — 后台管理"


class ControlWindow:
    def __init__(self, url, runtime=None):
        self.url = url
        self.runtime = Path(runtime) if runtime else runtime_dir()
        self.hwnd = None
        self.process = None

    def _owns(self, hwnd):
        import win32gui
        import win32process
        return bool(self.process and self.process.poll() is None and hwnd
                    and win32gui.IsWindow(hwnd)
                    and win32process.GetWindowThreadProcessId(hwnd)[1] == self.process.pid)

    def _find_window(self):
        import win32gui
        found = []

        def candidate(hwnd, unused):
            if win32gui.GetWindowText(hwnd) == TITLE and self._owns(hwnd):
                found.append(hwnd)

        win32gui.EnumWindows(candidate, None)
        return found[0] if found else None

    def open(self):
        if os.name != "nt":
            webbrowser.open(self.url)
            return {"message": "后台管理已在浏览器打开"}
        import win32gui
        if not self._owns(self.hwnd):
            self.hwnd = None
            if self.process is None or self.process.poll() is not None:
                host = ROOT / "native/control/ControlHost.exe"
                if not host.is_file():
                    raise RuntimeError(f"后台窗口组件缺失，可手动访问 {self.url}")
                kernel = ctypes.WinDLL("kernel32", use_last_error=True) if getattr(sys, "frozen", False) else None
                if kernel:
                    kernel.SetDllDirectoryW.argtypes = [ctypes.c_wchar_p]
                    kernel.SetDllDirectoryW(None)
                try:
                    self.process = subprocess.Popen(
                        [str(host), self.url, str((self.runtime / "webview2-profile").resolve()), str(os.getpid())],
                        stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                        creationflags=subprocess.CREATE_NO_WINDOW,
                    )
                finally:
                    if kernel:
                        kernel.SetDllDirectoryW(sys._MEIPASS)
            deadline = time.monotonic() + 12
            while time.monotonic() < deadline and self.process.poll() is None:
                self.hwnd = self._find_window()
                if self.hwnd:
                    break
                time.sleep(.1)
            if not self.hwnd:
                if self.process.poll() == 3:
                    raise RuntimeError(f"后台需要 Microsoft Edge WebView2 Runtime，可手动访问 {self.url}")
                raise RuntimeError(f"后台窗口未就绪，可手动访问 {self.url}")

        win32gui.ShowWindow(self.hwnd, 9)
        try:
            win32gui.SetForegroundWindow(self.hwnd)
        except Exception:
            pass
        return {"message": "后台管理已打开"}

    def close(self):
        if self.hwnd and self._owns(self.hwnd):
            import win32gui
            win32gui.PostMessage(self.hwnd, 0x0010, 0, 0)
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                pass
