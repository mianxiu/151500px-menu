"""Launch Photoshop COM from Explorer's normal desktop environment.

The same offline EXE hosts the helper. The service never activates COM itself
to start Photoshop, never closes documents, and never retries unknown starts.
"""
import argparse
import asyncio
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid

from .paths import ROOT
from .settings import atomic_json
from .uxp_management import photoshop_running
from .photoshop_version import exe_version, require_version
from .version import APP_VERSION


def registered_executable(prog_id='Photoshop.Application'):
    import re
    import winreg
    clsid = winreg.QueryValue(winreg.HKEY_CLASSES_ROOT, prog_id+r'\CLSID')
    with winreg.OpenKey(winreg.HKEY_CLASSES_ROOT, 'CLSID\\'+clsid+'\\LocalServer32') as key:
        try: value = winreg.QueryValueEx(key, 'ServerExecutable')[0]
        except OSError:
            command = winreg.QueryValueEx(key, None)[0]
            match = re.match(r'^\s*"?(.+?\.exe)"?(?:\s|$)', command, re.I)
            if not match: raise RuntimeError('无法识别 Photoshop COM 启动路径')
            value = match.group(1)
    path = Path(os.path.expandvars(value)).resolve(strict=True)
    if path.name.lower() != 'photoshop.exe': raise RuntimeError('COM 注册未指向 Photoshop.exe')
    require_version(exe_version(path))
    return path


def worker_command(request):
    if getattr(sys, 'frozen', False):
        return [str(Path(sys.executable).resolve()), '--ps-launch-worker', str(request)]
    executable = Path(sys.executable).with_name('pythonw.exe')
    if not executable.exists(): executable = Path(sys.executable)
    return [str(executable), str(ROOT/'main.py'), '--ps-launch-worker', str(request)]


def launch_from_desktop(request):
    import pythoncom
    import win32api
    import win32com.client
    import win32process
    pythoncom.CoInitialize()
    try:
        desktop = win32com.client.Dispatch('Shell.Application').Windows().FindWindowSW(0, 0, 8, 0, 1)
        pid = win32process.GetWindowThreadProcessId(desktop.HWND)[1]
        handle = win32api.OpenProcess(0x410, False, pid)
        try:
            if Path(win32process.GetModuleFileNameEx(handle, 0)).name.lower() != 'explorer.exe':
                raise RuntimeError('未找到正常桌面 Explorer，未直接启动 Photoshop')
        finally: handle.Close()
        command = worker_command(request)
        desktop.Document.Application.ShellExecute(command[0], subprocess.list2cmdline(command[1:]),
                                                 str(Path(command[0]).parent), 'open', 0)
        return pid
    finally: pythoncom.CoUninitialize()


def execute_worker(request):
    import msvcrt
    import pythoncom
    import win32com.client
    request = Path(request).resolve(strict=True)
    data = json.loads(request.read_text('utf-8'))
    identity = data.get('id', '')
    if not isinstance(identity, str) or len(identity) != 32 or any(c not in '0123456789abcdef' for c in identity):
        raise ValueError('无效启动请求')
    if data.get('version') != APP_VERSION or data.get('state') != 'requested':
        raise ValueError('启动请求版本不匹配或已消费')
    if not 0 <= time.time()*1000-data.get('started_at_ms', 0) < 90000:
        raise ValueError('启动请求已过期')
    result = request.with_suffix('.result.json')
    with request.with_suffix('.lock').open('a+b') as lease:
        lease.seek(0); lease.write(b'0'); lease.flush(); lease.seek(0)
        msvcrt.locking(lease.fileno(), msvcrt.LK_NBLCK, 1)
        try:
            # Re-read inside the lease so duplicate helpers cannot activate twice.
            data = json.loads(request.read_text('utf-8'))
            if data['state'] != 'requested': raise ValueError('启动请求已消费')
            data['state'] = 'consumed'; atomic_json(request, data)
            report = {'id':identity, 'version':APP_VERSION, 'state':'running', 'pid':os.getpid(),
                      'started_at_ms':data['started_at_ms'],
                      'processor_architecture':os.environ.get('PROCESSOR_ARCHITECTURE')}
            atomic_json(result, report)
            pythoncom.CoInitialize()
            try:
                target = registered_executable()
                if str(target).casefold() != data['executable'].casefold():
                    raise RuntimeError('Photoshop COM 注册已改变，未启动')
                if photoshop_running():
                    app = win32com.client.GetActiveObject('Photoshop.Application')
                    report['existing_instance'] = True
                else:
                    app = win32com.client.Dispatch('Photoshop.Application')
                    report['existing_instance'] = False
                report['photoshop_version'] = str(app.Version)
                require_version(report['photoshop_version'])
                report['state'] = 'succeeded'
                del app
            except Exception as error:
                report.update(state='failed', error=str(error), hresult=getattr(error, 'hresult', None))
            finally:
                pythoncom.CoUninitialize()
                report['completed_at_ms'] = int(time.time()*1000)
                atomic_json(result, report)
        finally:
            lease.seek(0); msvcrt.locking(lease.fileno(), msvcrt.LK_UNLCK, 1)


class PhotoshopLauncher:
    def __init__(self, runtime, ready, *, running=None, registered=None, launch=None, timeout=90):
        self.root = Path(runtime)/'photoshop-start'
        self.ready = ready
        self.running = running or photoshop_running
        self.registered = registered or registered_executable
        self.launch = launch or launch_from_desktop
        self.timeout = timeout
        self.task = None
        self.state = {'phase':'idle', 'message':'尚未请求启动 Photoshop'}
        self.last = self.root/'latest.json'
        if self.last.exists():
            self.state = json.loads(self.last.read_text('utf-8'))
            if self.active:
                self.state.update(phase='unknown', message='上次启动结果待核对；未重复启动')

    @property
    def active(self): return self.state['phase'] in ('starting', 'waiting-uxp', 'unknown')

    def snapshot(self):
        value = {**self.state, 'active':self.active}
        if self.active: value['elapsed_seconds'] = round((time.time()*1000-value['started_at_ms'])/1000, 1)
        return value

    def update(self, **values):
        self.state.update(values); atomic_json(self.last, self.state)

    async def start(self, wait_uxp=True):
        if self.active: raise RuntimeError('Photoshop 启动进行中或结果未确认；未重复启动')
        if await asyncio.to_thread(self.running):
            self.state = {'phase':'already-running', 'message':'Photoshop 已运行，未重复启动'}
            atomic_json(self.last, self.state)
            return {'message':self.state['message'], 'photoshop_start':self.snapshot()}
        target = await asyncio.to_thread(self.registered)
        identity = uuid.uuid4().hex
        request = self.root/(identity+'.json')
        data = {'id':identity, 'version':APP_VERSION, 'state':'requested', 'executable':str(target),
                'started_at_ms':int(time.time()*1000)}
        atomic_json(request, data)
        self.state = {'phase':'starting', 'message':'正在通过桌面 COM 助手启动 Photoshop',
                      'id':identity, 'started_at_ms':data['started_at_ms'], 'wait_uxp':wait_uxp}
        atomic_json(self.last, self.state)
        self.task = asyncio.create_task(self.observe(request, send=True))
        return {'message':self.state['message'], 'photoshop_start':self.snapshot()}

    async def resume(self):
        if self.active and self.state.get('id'):
            self.task = asyncio.create_task(self.observe(self.root/(self.state['id']+'.json'), send=False))

    async def observe(self, request, send=False):
        report = None
        try:
            if send: self.update(desktop_pid=await asyncio.to_thread(self.launch, request))
            deadline = asyncio.get_running_loop().time()+self.timeout
            while asyncio.get_running_loop().time() < deadline:
                try:
                    report = json.loads(request.with_suffix('.result.json').read_text('utf-8'))
                    if report.get('id') != self.state['id'] or report.get('version') != APP_VERSION:
                        raise RuntimeError('启动助手返回了不匹配的结果')
                except (FileNotFoundError, PermissionError, json.JSONDecodeError): report = None
                if report and report['state'] == 'failed':
                    self.update(phase='failed', message='Photoshop 启动失败：'+report['error'], worker=report)
                    return
                if report and report['state'] == 'succeeded':
                    if not self.state['wait_uxp'] or await self.ready(self.state['started_at_ms']):
                        self.update(phase='ready', message='Photoshop 已启动'+('，UXP 已就绪' if self.state['wait_uxp'] else ''), worker=report)
                        return
                    if self.state['phase'] != 'waiting-uxp':
                        self.update(phase='waiting-uxp', message='Photoshop 已启动，等待 UXP 加载', worker=report)
                await asyncio.sleep(.25)
            if report and report.get('state') == 'succeeded':
                self.update(phase='started', message='Photoshop 已启动，但 UXP 尚未就绪；请检查插件管理', worker=report)
            else:
                self.update(phase='unknown', message='Photoshop 启动结果未确认；请检查 PS，未重复启动')
        except asyncio.CancelledError: raise
        except Exception as error:
            # Desktop launch exceptions might happen after accepting the request.
            self.update(phase='unknown', message='启动结果未确认：'+str(error)+'；未重复启动')

    async def close(self):
        if self.task and not self.task.done():
            self.task.cancel()
            await asyncio.gather(self.task, return_exceptions=True)


def cli():
    parser = argparse.ArgumentParser()
    parser.add_argument('request', type=Path)
    args = parser.parse_args()
    execute_worker(args.request)
