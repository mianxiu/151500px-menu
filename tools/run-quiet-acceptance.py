"""Run menu simulation and isolated real Photoshop acceptance without sending input."""
import argparse
import asyncio
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from urllib.request import ProxyHandler, build_opener
from uuid import uuid4

from tornado.httpclient import HTTPRequest
from tornado.websocket import websocket_connect

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from backend.settings import atomic_json
from backend.stability_metrics import host_scope
from backend.version import APP_VERSION

URL = 'http://127.0.0.1:15151'
HTTP = build_opener(ProxyHandler({}))
MANUAL = {
    'caps': '在 Photoshop 非输入区实按 Caps，核对菜单唤出、取消后焦点回到 Photoshop。',
    'ime': '在自建文字层用当前中文输入法组合并提交文字，退出文字编辑后实按 Caps。检查输入状态切换和快捷键都正常。',
}


def status():
    with HTTP.open(URL+'/api/status', timeout=5) as response:
        return json.load(response)


def preflight():
    value = status()
    ps = value['photoshop']
    if value['version'] != APP_VERSION or not value['menu']['ready'] or not ps.get('uxp_ready'):
        raise RuntimeError('请先启动与当前源码版本一致的后台和 Photoshop，并确认 UXP 已就绪。')
    if (ps['busy'] or value['menu']['visible'] or value['stability']['active']
            or (Path(value['data_dir'])/'uxp-pending.json').exists()):
        raise RuntimeError('请先完成当前操作并关闭菜单。')
    return value


async def request(socket, action, **args):
    identity = uuid4().hex
    await socket.write_message(json.dumps({'id':identity,'action':action,'args':args}))
    while True:
        message = await socket.read_message()
        if message is None:
            raise RuntimeError('后台连接已断开。')
        reply = json.loads(message)
        if reply.get('id') == identity:
            if not reply.get('ok'):
                raise RuntimeError(reply.get('error', '后台请求失败。'))
            return reply['data']


async def photoshop_report(folder, record, save):
    socket = await websocket_connect(HTTPRequest(URL.replace('http:', 'ws:')+'/websocket',
                                                 headers={'Origin':URL}, connect_timeout=5))
    try:
        # If submission times out, retain the unknown request and avoid resubmission.
        result = await asyncio.wait_for(request(socket, 'stability.start', profile='quiet'), 20)
        run_id = record['run_id'] = result['run_id']
        save()
        deadline = time.monotonic()+120
        while result['active']:
            if time.monotonic() >= deadline:
                await asyncio.wait_for(request(socket, 'stability.stop'), 10)
                raise RuntimeError(f'已请求停止后续步骤。请核对测试 {run_id} 的最终报告。')
            await asyncio.sleep(.4)
            result = await asyncio.wait_for(request(socket, 'stability.status'), 10)
            if result['run_id'] != run_id:
                raise RuntimeError('后台测试标识已变化。')
        detail = await asyncio.wait_for(request(socket, 'stability.export'), 10)
        if detail['run_id'] != run_id:
            raise RuntimeError('后台测试报告标识不一致。')
        atomic_json(folder, detail)
        record['report'] = str(folder)
        if (detail['phase'] != 'completed' or detail.get('original_request_pending') is not False
                or detail['cleanup'] != 'verified-closed-and-original-restored'):
            raise RuntimeError(detail.get('error') or detail['message'])
        record['steps'] = {state:sum(step['state'] == state for step in detail['steps'])
                           for state in ('passed','skipped')}
    finally:
        socket.close()


def log_manual(report, save):
    print('请在 Photoshop 中使用自建测试文档。以下步骤需要你实按，脚本只记录结果。')
    for key, instruction in MANUAL.items():
        print(instruction)
        answer = input('输入 y 表示通过，n 表示失败，直接回车表示待验收：').strip().lower()
        while answer not in ('y','n',''):
            answer = input('请输入 y、n 或直接回车：').strip().lower()
        report['interactive'][key] = {'y':'passed','n':'failed','':'pending'}[answer]
        save()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--finish-interactive', type=Path, metavar='REPORT',
                        help='Record real Caps and IME checks after a successful automatic run')
    args = parser.parse_args()
    if args.finish_interactive:
        path = args.finish_interactive.resolve()
        if (path.parent.parent != ROOT/'.runtime' or not path.parent.name.startswith('quiet-acceptance-')
                or path.name != 'report.json' or not sys.stdin.isatty()):
            parser.error('请在交互终端使用 .runtime 内的本工具报告。')
        report = json.loads(path.read_text('utf-8'))
        if report.get('automatic') != 'passed' or report.get('target') != identity(preflight()):
            parser.error('需要当前后台候选对应的成功自动报告。')
        log_manual(report, lambda:atomic_json(path,report))
        return 1 if 'failed' in report['interactive'].values() else 0

    value = preflight()
    menu = Path(value['service']['resource_root'])/'native/MenuHost.exe'
    folder = ROOT/'.runtime'/('quiet-acceptance-'+uuid4().hex)
    folder.mkdir(parents=True)
    path = folder/'report.json'
    report = {'target':identity(value), 'automatic':'running', 'stages':{},
              'interactive':dict.fromkeys(MANUAL,'pending')}

    def save():
        atomic_json(path,report)

    print(f'报告 {path}', flush=True)
    save()
    try:
        menu_stage = report['stages']['menu-simulation'] = {'state':'running','log':'menu.log'}
        save()
        with (folder/'menu.log').open('wb') as output:
            subprocess.run([str(menu),'--self-test'], cwd=menu.parent, stdout=output,
                           stderr=subprocess.STDOUT, timeout=120, check=True,
                           creationflags=subprocess.CREATE_NO_WINDOW)
        menu_stage['state'] = 'passed'
        save()

        from quiet_ps_session import ResourceLease
        pid, check_host = host_scope()
        for name in ('low-cpu','normal'):
            current = preflight()
            check_host()
            if identity(current) != report['target'] or current['service']['pid'] != value['service']['pid']:
                raise RuntimeError('运行后台或 Photoshop 版本已变化。')
            record = report['stages'][name] = {'state':'running'}
            try:
                if name == 'low-cpu':
                    with ResourceLease(pid, folder/'resources.json', seconds=160) as lease:
                        asyncio.run(photoshop_report(folder/(name+'.json'),record,save))
                    if (lease.expired or lease.record['final_affinity'] != lease.original_affinity
                            or lease.record['final_priority'] != lease.original_priority):
                        raise RuntimeError('Photoshop 测试资源恢复未通过核对。')
                else:
                    asyncio.run(photoshop_report(folder/(name+'.json'),record,save))
            except BaseException:
                record['state'] = 'incomplete'
                raise
            record['state'] = 'passed'
            print(f'{name} 完成', flush=True)
            save()
        report['automatic'] = 'passed'
        print('自动验收完成。Caps 和中文输入法待统一实按。', flush=True)
    except BaseException as error:
        for stage in report['stages'].values():
            if stage['state'] == 'running':
                stage['state'] = 'incomplete'
        report.update(automatic='incomplete',error=str(error))
        raise
    finally:
        save()
    return 0


def identity(value):
    entry = Path(value['service']['executable'])
    with entry.open('rb') as file:
        digest = hashlib.file_digest(file,'sha256').hexdigest()
    return {'backend':value['version'],'uxp':value['photoshop']['uxp_version'],'exe_sha256':digest}


if __name__ == '__main__':
    raise SystemExit(main())
