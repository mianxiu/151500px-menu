"""Bounded, observation-only WH_KEYBOARD/GETMESSAGE/CALLWNDPROC capture.

No key injection, input-language writes, document commands or production F routing.
An armed probe expires after 30 seconds, or when this controller exits.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import time

ROOT = Path(__file__).resolve().parents[1]


def analyze(path):
    lines = Path(path).read_text('utf-8').splitlines()
    metadata = dict(line.split('\t') for line in lines[:3])
    frequency, count, lost = (int(metadata[key]) for key in ('frequency', 'count', 'lost'))
    if frequency <= 0 or count < 0 or lost != max(0, count - 4096):
        raise ValueError('Invalid probe counters')
    names = ('index', 'tick', 'stage', 'message', 'flags', 'epoch', 'entry_newer', 'focus', 'layout')
    rows = []
    for index, line in enumerate(lines[3:]):
        values = [int(value) for value in line.split('\t')]
        if len(values) != len(names) or values[0] != index:
            raise ValueError('Invalid probe event sequence')
        row = dict(zip(names, values))
        if row['stage'] not in (1, 2, 3, 4) or (rows and row['tick'] < rows[-1]['tick']):
            raise ValueError('Invalid probe stage or clock')
        rows.append(row)
    if len(rows) != min(count, 4096):
        raise ValueError('Incomplete probe export')
    keyboard = [r for r in rows if r['stage'] == 1 and r['message'] == 0]
    return dict(frequency=frequency, count=count, lost=lost, events=rows,
                keyboard_actions=len(keyboard), keyboard_peeks=sum(r['stage'] == 1 and r['message'] == 3 for r in rows),
                down=sum(not r['flags'] & 0x80000000 and not r['flags'] & 0x40000000 for r in keyboard),
                repeat=sum(not r['flags'] & 0x80000000 and bool(r['flags'] & 0x40000000) for r in keyboard),
                up=sum(bool(r['flags'] & 0x80000000) for r in keyboard),
                queue_removed=sum(r['stage'] == 3 for r in rows),
                migration_gate='not-validated',
                note='Observation only. A missing F can be consumed upstream; event order alone does not establish exclusive consumption or IME safety.')


def capture(dll, seconds, directory):
    import ctypes
    from ctypes import wintypes as W
    from urllib.request import urlopen
    user = ctypes.WinDLL('user32', use_last_error=True)
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    user.SetWindowsHookExW.argtypes = (ctypes.c_int, ctypes.c_void_p, W.HINSTANCE, W.DWORD)
    user.SetWindowsHookExW.restype = W.HANDLE
    user.UnhookWindowsHookEx.argtypes = (W.HANDLE,)
    user.FindWindowExW.argtypes = (W.HWND, W.HWND, W.LPCWSTR, W.LPCWSTR)
    user.FindWindowExW.restype = W.HWND
    user.GetWindowThreadProcessId.argtypes = (W.HWND, ctypes.POINTER(W.DWORD))
    user.GetClassNameW.argtypes = (W.HWND, W.LPWSTR, ctypes.c_int)
    user.IsWindowVisible.argtypes = (W.HWND,)
    user.SendMessageTimeoutW.argtypes = (W.HWND, W.UINT, W.WPARAM, W.LPARAM, W.UINT, W.UINT, ctypes.POINTER(ctypes.c_size_t))
    user.SendMessageTimeoutW.restype = W.LPARAM
    user.RegisterWindowMessageW.argtypes = (W.LPCWSTR,)
    kernel.OpenProcess.argtypes = (W.DWORD, W.BOOL, W.DWORD)
    kernel.OpenProcess.restype = W.HANDLE
    kernel.CloseHandle.argtypes = (W.HANDLE,)
    kernel.QueryFullProcessImageNameW.argtypes = (W.HANDLE, W.DWORD, W.LPWSTR, ctypes.POINTER(W.DWORD))
    kernel.GetProcessTimes.argtypes = (W.HANDLE,) + (ctypes.POINTER(W.FILETIME),) * 4
    def integrity(target):
        advapi = ctypes.WinDLL('advapi32', use_last_error=True)
        advapi.OpenProcessToken.argtypes = (W.HANDLE, W.DWORD, ctypes.POINTER(W.HANDLE))
        advapi.GetTokenInformation.argtypes = (W.HANDLE, ctypes.c_int, ctypes.c_void_p, W.DWORD, ctypes.POINTER(W.DWORD))
        advapi.GetSidSubAuthorityCount.argtypes = (ctypes.c_void_p,)
        advapi.GetSidSubAuthorityCount.restype = ctypes.POINTER(ctypes.c_ubyte)
        advapi.GetSidSubAuthority.argtypes = (ctypes.c_void_p, W.DWORD)
        advapi.GetSidSubAuthority.restype = ctypes.POINTER(W.DWORD)
        process = kernel.OpenProcess(0x1000, False, target)
        if not process:return None
        token = W.HANDLE()
        try:
            if not advapi.OpenProcessToken(process, 8, ctypes.byref(token)):return None
            length = W.DWORD()
            advapi.GetTokenInformation(token, 25, None, 0, ctypes.byref(length))
            if not length.value:return None
            buffer = ctypes.create_string_buffer(length.value)
            if not advapi.GetTokenInformation(token, 25, buffer, length, ctypes.byref(length)):return None
            sid = ctypes.cast(buffer, ctypes.POINTER(ctypes.c_void_p))[0]
            count = advapi.GetSidSubAuthorityCount(sid).contents.value
            return advapi.GetSidSubAuthority(sid, count-1).contents.value if count else None
        finally:
            if token:kernel.CloseHandle(token)
            kernel.CloseHandle(process)
    callback = ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
    user.EnumWindows.argtypes = (callback, W.LPARAM)
    roots = []
    def visit(window, unused):
        name = ctypes.create_unicode_buffer(64)
        user.GetClassNameW(window, name, 64)
        if name.value == 'Photoshop' and user.IsWindowVisible(window):roots.append(window)
        return True
    user.EnumWindows(callback(visit), 0)
    if len(roots) != 1:raise RuntimeError(f'Expected one PS root, found {len(roots)}')
    root = roots[0];target_pid = W.DWORD()
    thread = user.GetWindowThreadProcessId(root, ctypes.byref(target_pid));pid = target_pid.value
    process = kernel.OpenProcess(0x1000, False, pid)
    if not process:raise ctypes.WinError(ctypes.get_last_error())
    try:
        path = ctypes.create_unicode_buffer(32768);length = W.DWORD(32768)
        created, exited, system, elapsed = (W.FILETIME() for _ in range(4))
        if not kernel.QueryFullProcessImageNameW(process, 0, path, ctypes.byref(length)) or Path(path.value).name.lower() != 'photoshop.exe':
            raise RuntimeError('Root process is not verified Photoshop.exe')
        if not kernel.GetProcessTimes(process, *(ctypes.byref(v) for v in (created, exited, system, elapsed))):
            raise ctypes.WinError(ctypes.get_last_error())
        creation = (created.dwHighDateTime << 32) | created.dwLowDateTime
    finally:
        kernel.CloseHandle(process)
    with urlopen('http://127.0.0.1:15151/api/status', timeout=5) as response:
        before = json.load(response)
    if before['photoshop']['busy'] or before['menu']['visible'] or before['photoshop'].get('uxp_recovery', {}).get('active'):
        raise RuntimeError('Close menu and settle pending work before observing')
    dll = Path(dll).resolve(strict=True)
    directory = Path(directory);directory.mkdir(parents=True, exist_ok=False)
    result = dict(service_version=before['version'], service_pid=before['service']['pid'], ps_pid=pid,
                  ps_thread=thread, ps_root=root, ps_executable=path.value, ps_created=creation,
                  dll=str(dll), dll_sha256=hashlib.sha256(dll.read_bytes()).hexdigest(),
                  self_integrity=integrity(os.getpid()), ps_integrity=integrity(pid),
                  seconds=seconds, stopped=False, bootstrap_unhooked=False,
                  production_routing_changed=False, document_commands=False)
    journal = directory / 'capture.json'
    def save():
        journal.write_text(json.dumps(result, ensure_ascii=False, indent=2), 'utf-8')
    save()
    if result['self_integrity'] is None or result['self_integrity'] != result['ps_integrity']:
        raise RuntimeError('Probe requires the same integrity as Photoshop; no automatic elevation or policy change')
    cls = 'PhotoshopMenu.ThreadFProbe.v1'
    message = user.RegisterWindowMessageW('PhotoshopMenu.ThreadFProbe.Control.v1')
    if user.FindWindowExW(W.HWND(-3), None, cls, None):
        raise RuntimeError('A pinned probe already exists: keep its evidence; normal PS restart required before another capture')
    library = ctypes.WinDLL(str(dll))
    hook = user.SetWindowsHookExW(4, ctypes.cast(library.ThreadFLoader, ctypes.c_void_p), library._handle, thread)
    if not hook:
        raise ctypes.WinError(ctypes.get_last_error())
    control = None
    attempted = False
    def call(operation, value=0):
        output = ctypes.c_size_t()
        if not user.SendMessageTimeoutW(control, message, operation, value, 2, 1000, ctypes.byref(output)):
            raise RuntimeError(f'Probe operation {operation} unknown; never republish it')
        return output.value
    try:
        output = ctypes.c_size_t()
        # A synchronous harmless root message boots the thread-scoped loader.
        if not user.SendMessageTimeoutW(root, 0, 0, 0, 2, 1000, ctypes.byref(output)):
            raise RuntimeError('Bootstrap exit unknown; do not resend')
        control = user.FindWindowExW(W.HWND(-3), None, cls, None)
        owner_pid = W.DWORD()
        if not control or user.GetWindowThreadProcessId(control, ctypes.byref(owner_pid)) != thread or owner_pid.value != pid:
            raise RuntimeError('Probe control process/thread mismatch')
        result['control'] = control;save()
        attempted = True
        if call(1, os.getpid()) != 1:
            raise RuntimeError('Probe did not confirm all three hooks')
        print(json.dumps(dict(ready=True, **result)), flush=True)
        time.sleep(seconds)
    except BaseException as error:
        result['error'] = str(error)
        raise
    finally:
        try:
            if attempted:
                # Stop is distinct from the start request; unknown start is never retried.
                result['stopped'] = call(2) == 1 and call(4) == 0
                if not result['stopped']:
                    raise RuntimeError('Unhook not confirmed; retain protection and do not unload DLL')
                exported = call(3)
                if exported >= 1 << 63:
                    raise RuntimeError('Probe export failed')
                raw = dll.with_name('thread-f-events.tsv')
                content = raw.read_bytes();(directory / raw.name).write_bytes(content)
                result['raw_sha256'] = hashlib.sha256(content).hexdigest()
                result['analysis'] = analyze(directory / raw.name)
        except BaseException as error:
            result['cleanup_error'] = str(error)
            raise
        finally:
            result['bootstrap_unhooked'] = bool(user.UnhookWindowsHookEx(hook))
            save()
    if not result['bootstrap_unhooked']:
        raise RuntimeError('Bootstrap hook removal unknown; original identities retained')
    print(json.dumps({k: v for k, v in result.items() if k != 'analysis'}, ensure_ascii=False), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll', type=Path, default=ROOT / '.runtime/thread-f-probe/build/ThreadFProbe.dll')
    parser.add_argument('--seconds', type=int, choices=range(1, 26), default=20)
    parser.add_argument('--output', type=Path, default=ROOT / '.runtime/thread-f-probe' / str(time.time_ns()))
    parser.add_argument('--analyze', type=Path)
    args = parser.parse_args()
    if args.analyze:
        print(json.dumps(analyze(args.analyze), indent=2))
    else:
        capture(args.dll, args.seconds, args.output)


if __name__ == '__main__':
    main()
