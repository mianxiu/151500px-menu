"""Native icon handles for the notification area."""
import ctypes
from ctypes import wintypes as W
from pathlib import Path
import threading
from .paths import ROOT

ICON_PATH = ROOT / "frontend/public/icon.ico"


class WindowIcons:
    def __init__(self):
        self.handles = {}
        self.lock = threading.Lock()
        self.u = ctypes.WinDLL("user32", use_last_error=True)
        for name, args, result in (
            ("LoadImageW", [W.HINSTANCE, W.LPCWSTR, W.UINT, ctypes.c_int, ctypes.c_int, W.UINT], W.HANDLE),
            ("GetDpiForWindow", [W.HWND], W.UINT),
            ("IsWindow", [W.HWND], W.BOOL),
            ("DestroyIcon", [W.HICON], W.BOOL),
            ("SendMessageTimeoutW", [W.HWND, W.UINT, W.WPARAM, W.LPARAM, W.UINT, W.UINT, ctypes.POINTER(ctypes.c_size_t)], W.LPARAM),
        ):
            api = getattr(self.u, name)
            api.argtypes, api.restype = args, result

    def load(self, size):
        with self.lock:
            if size not in self.handles:
                handle = self.u.LoadImageW(None, str(ICON_PATH), 1, size, size, 0x10)
                if not handle:
                    raise ctypes.WinError(ctypes.get_last_error())
                self.handles[size] = handle
            return self.handles[size]

    def message(self, hwnd, message, kind, value):
        result = ctypes.c_size_t()
        if not self.u.SendMessageTimeoutW(hwnd, message, kind, value, 0x2, 100, ctypes.byref(result)):
            return None
        return result.value

    def apply(self, hwnd):
        if not self.u.IsWindow(hwnd):
            return False
        dpi = self.u.GetDpiForWindow(hwnd) or 96
        for kind, base_size in ((0, 16), (1, 32)):
            icon = self.load(max(16, round(base_size * dpi / 96)))
            current = self.message(hwnd, 0x7f, kind, dpi)
            if current is None:
                return False
            if current != icon and self.message(hwnd, 0x80, kind, icon) is None:
                return False
        return True

    def detach(self, hwnd):
        if self.u.IsWindow(hwnd):
            for kind in (0, 1):
                current = self.message(hwnd, 0x7f, kind, 0)
                if current in self.handles.values():
                    self.message(hwnd, 0x80, kind, 0)

    def close(self):
        for handle in self.handles.values():
            self.u.DestroyIcon(handle)
        self.handles.clear()
