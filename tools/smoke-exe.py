"""Exercise the delivered EXE from an isolated, Unicode working directory."""
import argparse
import asyncio
import io
import json
import os
import runpy
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import zipfile
from tornado.httpclient import AsyncHTTPClient, HTTPRequest
from tornado.websocket import websocket_connect


async def exercise(executable, workspace, port, show_control=False):
    url = f'http://127.0.0.1:{port}'
    client = AsyncHTTPClient()
    try:
        await client.fetch(url+'/api/health', connect_timeout=1, request_timeout=1)
    except Exception:
        pass
    else:
        raise RuntimeError('Smoke-test port is already in use')
    with tempfile.TemporaryDirectory(prefix='EXE 测试 ', dir=workspace) as folder:
        folder = Path(folder)
        copy = folder/'151500px-menu.exe'
        shutil.copy2(executable,copy)
        data = folder/'用户数据'
        # This isolated service must not compete with the user's live CapsLock
        # hook. Native state-machine tests exercise actual key decisions.
        settings = runpy.run_path(str(Path(__file__).resolve().parents[1]/'backend/settings.py'))['DEFAULTS']
        settings['enabled'] = False
        data.mkdir()
        (data/'settings.json').write_text(json.dumps(settings),encoding='utf-8')
        # Exercise local startup/shutdown independently of a remote release check.
        (data/'application-update.json').write_text(json.dumps({'auto_check':False}),encoding='utf-8')
        history_id = 'smoke-history-success'
        (data/'task-history.json').write_text(json.dumps({'protocol_version':1,'items':[
            {'id':history_id,'action':'label.red','label':'历史测试','channel':'com-jsx',
             'state':'succeeded','elapsed_ms':123,'target':{'document_id':1,'layer_ids':[2]},
             'created_at':'2026-10-11T00:00:00+00:00','updated_at':'2026-10-11T00:00:01+00:00'},
            {'id':'smoke-history-interrupted','action':'label.none','label':'中断测试','channel':'com-jsx',
             'state':'running','elapsed_ms':456,'created_at':'2026-10-11T00:00:00+00:00',
             'updated_at':'2026-10-11T00:00:01+00:00'}]}),encoding='utf-8')
        env = {key:value for key,value in os.environ.items()
               if key not in ('PYTHONHOME','PYTHONPATH','VIRTUAL_ENV')}
        async def session(restarted=False):
            process = subprocess.Popen([str(copy),'--headless','--port',str(port),'--data-dir',str(data)],
                cwd=str(folder), env=env, creationflags=subprocess.CREATE_NO_WINDOW, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            ws = None
            menu_pid = None
            try:
                deadline = time.monotonic()+30
                while time.monotonic()<deadline:
                    if process.poll() is not None:
                        logs={p.name:p.read_text('utf-8',errors='replace')[-4000:] for p in data.glob('*.log')}
                        evidence={'exit_code':process.returncode,'logs':logs}
                        (workspace/'smoke-startup-failure.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2),'utf-8')
                        raise RuntimeError('EXE exited before becoming ready: '+json.dumps(evidence,ensure_ascii=False))
                    try:
                        response = await client.fetch(url+'/api/status',request_timeout=2)
                        status = json.loads(response.body)
                        if status['menu']['ready']:
                            health=json.loads((await client.fetch(url+'/api/health',request_timeout=2)).body)
                            if health.get('ready') is True:
                                break
                    except Exception:
                        pass
                    await asyncio.sleep(.2)
                else:
                    raise RuntimeError('Native menu was not registered in time')
                import win32gui,win32process
                health=json.loads((await client.fetch(url+'/api/health')).body)
                assert health['version']==status['version']==status['service']['version']
                assert health['ready'] is True, 'HTTP responds before desktop components are ready'
                assert Path(status['service']['executable']).resolve()==copy.resolve()
                for _ in range(20):
                    if status.get('input_language',{}).get('uia_available') is True and 'root' in status.get('input_focus',{}):break
                    await asyncio.sleep(.1)
                    status=json.loads((await client.fetch(url+'/api/status')).body)
                assert status.get('input_language',{}).get('uia_available') is True, status.get('input_language')
                assert {'root','focus','epoch'}<=status.get('input_focus',{}).keys(),status.get('input_focus')
                history=json.loads((await client.fetch(url+'/api/tasks')).body)
                assert history['persistent'] and not history['persistence_error']
                saved={item['id']:item for item in history['items']}
                assert saved[history_id]['state']=='succeeded' and saved[history_id]['restored']
                assert saved['smoke-history-interrupted']['state']=='result-unknown'
                assert saved['smoke-history-interrupted']['previous_state']=='running'
                assert saved['smoke-history-interrupted']['elapsed_ms']==456
                import win32api,win32con
                for worker_pid in (status['menu']['pid'],status['menu']['hotkey_pid']):
                    handle=win32api.OpenProcess(win32con.PROCESS_QUERY_INFORMATION|win32con.PROCESS_VM_READ,False,worker_pid)
                    try:
                        worker_path=Path(win32process.GetModuleFileNameEx(handle,0))
                        assert worker_path.parent==Path(status['service']['resource_root'])/'native','Worker came from another build'
                    finally:win32api.CloseHandle(handle)
                hwnd = status['menu']['hwnd']
                assert hwnd and not win32gui.IsWindowVisible(hwnd), 'Menu must start hidden'
                assert not win32gui.IsIconic(hwnd), 'Hidden viewport must not be minimized: SWP_SHOWWINDOW cannot restore it'
                menu_pid = win32process.GetWindowThreadProcessId(hwnd)[1]
                assert menu_pid == status['menu']['pid'], 'Menu must belong to the tested service'
                assert status['menu']['panel_context']['kind'] in ('generic','canvas','layers','text')
                ws=await websocket_connect(HTTPRequest(url.replace('http:','ws:')+'/websocket',headers={'Origin':url}))
                async def request(action,args=None):
                    await ws.write_message(json.dumps({'id':action,'action':action,'args':args or {}}))
                    while True:
                        message=json.loads(await asyncio.wait_for(ws.read_message(),15))
                        if message.get('id')==action:
                            if not message.get('ok'): raise RuntimeError(message)
                            return message.get('data')
                assert not status['tray_error'],status['tray_error']
                tray_check = runpy.run_path(str(Path(__file__).with_name('check-tray.py')))['check_tray']
                tray_check(status['service']['pid'], recover=True)
                for task_id in (history_id,'missing-task'):
                    try:await request('tasks.cancel',{'id':task_id})
                    except RuntimeError as exc:
                        assert '历史任务只读' in str(exc) if task_id==history_id else '未找到' in str(exc)
                    else:raise AssertionError('Historical or missing task cancellation was accepted')
                assert status['menu']['renderer']=='Dear ImGui / C++ / DirectX 11'
                assert win32gui.GetClassName(hwnd)=='PhotoshopMenuImGui'
                frame_count=win32gui.SendMessage(hwnd,0x800b,0,0)
                await asyncio.sleep(.3)
                assert win32gui.SendMessage(hwnd,0x800b,0,0)==frame_count,'Hidden ImGui menu kept rendering'
                assert status['menu']['native_hotkeys'],'Independent Win32 hotkey host was not started'
                if not restarted:
                    import win32api,win32con
                    settings=status['settings']; settings['enabled']=False
                    await request('settings.save',{'settings':settings})
                    old_pid=status['menu']['hotkey_pid']
                    handle=win32api.OpenProcess(win32con.PROCESS_TERMINATE,False,old_pid)
                    try: win32api.TerminateProcess(handle,1)
                    finally: win32api.CloseHandle(handle)
                    for _ in range(40):
                        await asyncio.sleep(.1)
                        recovered=await request('status')
                        if recovered['menu']['hotkey_pid']!=old_pid and recovered['menu']['native_hotkeys']: break
                    else: raise AssertionError('Frozen native hotkey host did not recover')
                    assert not recovered['settings']['enabled']
                    print('C++ hotkey host crash recovery passed')
                page=(await client.fetch(url+'/')).body.decode()
                assert 'assets/' in page and 'root' in page
                assert (await client.fetch(url+'/icon.ico')).code==200
                exported=(await client.fetch(url+'/api/plugins/export')).body
                with zipfile.ZipFile(io.BytesIO(exported)) as archive:
                    assert 'manifest.json' in archive.namelist()
                    builtin=json.loads(archive.read('manifest.json'))
                    if not restarted:
                        builtin['version']='9.0.0'
                        stream=io.BytesIO()
                        with zipfile.ZipFile(stream,'w') as bundle:
                            for name in archive.namelist():
                                bundle.writestr(name,json.dumps(builtin) if name=='manifest.json' else archive.read(name))
                if restarted:
                    assert status['settings']['menu_width']==460
                else:
                    settings=status['settings']; settings['menu_width']=460
                    await request('settings.save',{'settings':settings})
                    boundary='EXESmokeBoundary'
                    payload=(f'--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="test.zip"\r\nContent-Type: application/zip\r\n\r\n'.encode()+stream.getvalue()+f'\r\n--{boundary}--\r\n'.encode())
                    preview=json.loads((await client.fetch(HTTPRequest(url+'/api/plugins/preview',method='POST',body=payload,
                        headers={'Origin':url,'Content-Type':f'multipart/form-data; boundary={boundary}'}))).body)['data']
                    if status['photoshop'].get('execution_backend')=='uxp-local-files':
                        for action in ('plugin.install','plugin.rollback'):
                            try:await request(action,{'token':preview['token']})
                            except RuntimeError as exc:assert 'UXP' in str(exc)
                            else:raise AssertionError('Legacy JSX mutation accepted by UXP service')
                    else:
                        await request('plugin.install',{'token':preview['token']})
                        assert (await request('status'))['plugin']['active']['version']=='9.0.0'
                        await request('plugin.rollback')
                    assert (await request('status'))['plugin']['active']['version']=='1.0.0'
                    result=await request('logs.read'); assert isinstance(result['text'],str)
                    if show_control:
                        await request('control.open')
                        if Path(os.environ.get('PROGRAMFILES(X86)','C:/Program Files (x86)')).joinpath('Microsoft/Edge/Application/msedge.exe').is_file():
                            assert win32gui.FindWindow(None,'151500px-menu — 后台管理'),'Management app window did not open'
                    duplicate=subprocess.Popen([str(copy),'--headless','--port',str(port),'--data-dir',str(data)],
                        cwd=str(folder),env=env,creationflags=subprocess.CREATE_NO_WINDOW, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                    assert await asyncio.to_thread(duplicate.wait,15)==0,'Duplicate launch failed'
                await request('service.quit')
                assert await asyncio.to_thread(process.wait,15)==0,'EXE failed to exit cleanly'
                if menu_pid:
                    from ctypes import WinDLL
                    kernel=WinDLL('kernel32')
                    # Query the recorded worker PID only; do not affect any other process.
                    import ctypes
                    kernel.OpenProcess.restype=ctypes.c_void_p
                    kernel.CloseHandle.argtypes=[ctypes.c_void_p]
                    handle=kernel.OpenProcess(0x1000,False,menu_pid)
                    if handle:
                        try:
                            code=ctypes.c_ulong()
                            kernel.GetExitCodeProcess.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_ulong)]
                            kernel.GetExitCodeProcess(handle,ctypes.byref(code))
                            assert code.value!=259,'Native menu worker was left running'
                        finally:kernel.CloseHandle(handle)
                print('Restart persistence passed' if restarted else 'EXE HTTP, resources, native menu, tray, settings, plugin upgrade/rollback, duplicate launch and shutdown passed')
            finally:
                if ws:ws.close()
                if process.poll() is None:
                    # On failure, ask the service to clean up its own tray and worker.
                    try:
                        cleanup=await websocket_connect(HTTPRequest(url.replace('http:','ws:')+'/websocket',headers={'Origin':url}))
                        await cleanup.write_message(json.dumps({'id':'cleanup','action':'service.quit'}))
                        await asyncio.sleep(1)
                        cleanup.close()
                        await asyncio.to_thread(process.wait,10)
                    except Exception:
                        process.terminate()
                        await asyncio.to_thread(process.wait,10)
        await session()
        await session(restarted=True)
    client.close()


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('executable',type=Path)
    parser.add_argument('--port',type=int,default=15153)
    parser.add_argument('--workspace',type=Path,default=Path(__file__).resolve().parents[1]/'.runtime')
    parser.add_argument('--show-control-window',action='store_true',help='Opt in to the visible Edge management-window test')
    args=parser.parse_args()
    args.workspace.mkdir(parents=True,exist_ok=True)
    asyncio.run(exercise(args.executable.resolve(),args.workspace.resolve(),args.port,args.show_control_window))
