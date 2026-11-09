"""Verify the real hotkey startup protocol, with all product hotkeys disabled."""
import ctypes
from ctypes import wintypes as W
import json
import os
from pathlib import Path
from queue import Queue
import subprocess
import sys
from threading import Thread

user=ctypes.windll.user32
user.GetPropW.argtypes=[W.HWND,W.LPCWSTR];user.GetPropW.restype=W.HANDLE
user.GetClassNameW.argtypes=[W.HWND,W.LPWSTR,ctypes.c_int]
executable=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[1]/'native/HotkeyHost.exe'
child=subprocess.Popen([str(executable),'--parent',str(os.getpid())],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
try:
    lines=Queue();Thread(target=lambda:lines.put(child.stdout.readline()),daemon=True).start()
    ready=json.loads(lines.get(timeout=5));endpoint=ready.get('frame_hwnd')
    assert ready['type']=='ready' and endpoint,ready
    name=ctypes.create_unicode_buffer(64);user.GetClassNameW(endpoint,name,64)
    assert name.value=='PhotoshopMenu.FrameKey.v1',name.value
    assert user.GetPropW(endpoint,'PhotoshopMenu.FrameKey.Owner.v1')==os.getpid()
    print('Hotkey ready JSON publishes the verified F endpoint')
finally:
    child.stdin.close();child.wait(timeout=5)
