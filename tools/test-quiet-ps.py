"""Opt-in secondary-monitor PS smoke batches; production executor, owned docs.

Use --inspect first, --place to move PS panels without activation, --run for
three small CPU-restricted batches. A stop file prevents the next operation;
it never cancels an accepted operation or replays an unknown result.
"""
import argparse
import asyncio
import json
import os
from pathlib import Path
import shutil
import sys
import time
from urllib.request import urlopen

import win32gui
import win32process
import win32com.client

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from backend.uxp_bridge import UxpBridge
from benchmark_read_helpers import TREE, SEMANTIC, fixture
from quiet_ps_session import inventory, select, place, save, ResourceLease


def idle():
    state = json.load(urlopen('http://127.0.0.1:15151/api/status', timeout=5))
    ps = state['photoshop']
    if (ps['busy'] or state['menu']['visible'] or state['photoshop_start']['active']
            or ps.get('uxp_recovery', {}).get('active')
            or (Path(state['data_dir'])/'uxp-pending.json').exists()
            or any(t['state'] in ('capturing', 'running', 'result-unknown')
                   for t in state['tasks']['items'])):
        raise RuntimeError('Live service is busy, visible, starting, or has an unresolved result')
    if not ps.get('uxp_ready'):
        raise RuntimeError('Installed UXP is not ready')
    return state


async def run(folder):
    state = inventory()
    ps, monitor = select(state)
    live = idle()
    placement = place(state, folder/'placement.json')
    if placement['foreground_before'] != placement['foreground_after']:
        raise RuntimeError('Foreground changed during placement; tests not started')
    config = folder/'uxp-config.json'
    shutil.copy2(Path(live['data_dir'])/'uxp-config.json', config)
    bridge = UxpBridge(folder/'tasks.json', config)
    app = win32com.client.GetActiveObject('Photoshop.Application')
    previous = app.ActiveDocument if app.Documents.Count else None
    scratch = None
    owned_id = None
    closed_owned = False
    stop = ROOT/'.runtime/quiet-ps.stop'
    report = dict(version=live['version'], uxp=live['photoshop']['uxp_version'],
                  placement=placement, batches=[], complete=False, physical_input=False,
                  simulation='two logical processors; below-normal PS priority; no RAM/GPU emulation')

    def guard():
        if stop.exists() or lease.expired:
            raise RuntimeError('Cooperative stop requested; no next operation submitted')
        foreground = win32gui.GetForegroundWindow()
        if foreground and win32process.GetWindowThreadProcessId(foreground)[1] == ps['pid']:
            raise RuntimeError('User entered Photoshop; no next operation submitted')
        if int(app.DoJavaScript('app.activeDocument.id;')) != owned_id:
            raise RuntimeError('Owned test document is no longer active')
        idle()

    def js(code):
        guard()
        return app.DoJavaScript(code)

    def close_owned():
        nonlocal scratch, owned_id, closed_owned
        if scratch is not None:
            js('app.activeDocument.close(SaveOptions.DONOTSAVECHANGES);')
            scratch = owned_id = None
            closed_owned = True

    bridge.execution_guard = guard
    try:
        with ResourceLease(ps['pid'], folder/'resources.json', seconds=120) as lease:
            for kind, count in [('shape', 4), ('mixed', 40), ('smart', 40)]:
                idle()
                if stop.exists() or lease.expired:
                    raise RuntimeError('Stopped before next batch')
                if win32process.GetWindowThreadProcessId(win32gui.GetForegroundWindow())[1] == ps['pid']:
                    raise RuntimeError('Photoshop is in use; no fixture created')
                foreground = win32gui.GetForegroundWindow()
                app.DoJavaScript(fixture(kind, count))
                scratch = app.ActiveDocument
                owned_id = int(app.DoJavaScript('app.activeDocument.id;'))
                if win32gui.GetForegroundWindow() != foreground:
                    report['foreground_change']=dict(before=foreground,after=win32gui.GetForegroundWindow())
                    raise RuntimeError('Fixture changed foreground; stopped after setup')
                before, semantic = str(js(TREE)), str(js(SEMANTIC))
                batch = dict(kind=kind, count=len(before.splitlines()), samples=[])
                report['batches'].append(batch)
                for action in ('layer.new', 'layer.solid', 'adjustment.curves'):
                    guard()
                    target = await asyncio.to_thread(bridge._capture_target)
                    assert target['document_id'] == owned_id
                    foreground = win32gui.GetForegroundWindow()
                    started = time.perf_counter()
                    response = await bridge.execute(action, frozen_target=target)
                    result = json.loads(response['result'])
                    elapsed = round((time.perf_counter()-started)*1000, 2)
                    assert not bridge.transport.pending.value and not bridge.busy
                    assert result['verified'] and result['placement_moves'] == 0
                    assert bridge.layer_context['document_id'] == owned_id
                    assert bridge.layer_context['layer_ids'] == [result['layer_id']]
                    assert result['tree_reader'] == ('dom' if batch['count'] <= 32 else 'batch')
                    after = str(js(TREE)).splitlines()
                    old_ids = {line.split('\t')[1] for line in before.splitlines()}
                    assert len(after) == batch['count']+1
                    assert [line for line in after if line.split('\t')[1] in old_ids] == before.splitlines()
                    assert str(js(SEMANTIC)) == semantic
                    js('app.activeDocument.activeHistoryState=app.activeDocument.historyStates[app.activeDocument.historyStates.length-2];')
                    assert str(js(TREE)) == before and str(js(SEMANTIC)) == semantic
                    assert win32gui.GetForegroundWindow() == foreground
                    batch['samples'].append(dict(action=action, ms=elapsed,
                        tree_reader=result['tree_reader'], verified=True, one_undo=True,
                        original_preserved=True, foreground_unchanged=True))
                    save(folder/'report.json', report)
                    print(json.dumps(dict(batch=kind, action=action, ms=elapsed)), flush=True)
                    await asyncio.sleep(2)
                close_owned()
                save(folder/'report.json', report)
                await asyncio.sleep(4)
            report['complete'] = True
    except BaseException as error:
        report['error'] = str(error)
        raise
    finally:
        # A trusted terminal receipt is required before touching a scratch doc.
        if not bridge.transport.pending.value and not bridge.busy and scratch is not None:
            try:
                close_owned()
            except Exception as error:
                report['cleanup_pending'] = str(error)
        foreground=win32gui.GetForegroundWindow()
        user_in_ps=bool(foreground and win32process.GetWindowThreadProcessId(foreground)[1]==ps['pid'])
        if closed_owned and scratch is None and previous is not None and not user_in_ps:
            app.ActiveDocument = previous
        report['pending_result'] = bool(bridge.transport.pending.value)
        report['foreground_after'] = win32gui.GetForegroundWindow()
        save(folder/'report.json', report)
        await bridge.close()
    print(json.dumps(dict(complete=True, report=str(folder/'report.json'))), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--inspect', action='store_true')
    mode.add_argument('--place', action='store_true')
    mode.add_argument('--place-right', action='store_true')
    mode.add_argument('--run', action='store_true')
    mode.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.inspect:
        print(json.dumps(inventory(), indent=2))
        return
    folder = ROOT/'.runtime'/('quiet-ps-'+str(time.time_ns()))
    folder.mkdir(parents=True)
    if args.self_test:
        with ResourceLease(os.getpid(), folder/'normal.json', allow_self=True) as lease:
            assert lease.applied
        assert lease.record['final_affinity'] == lease.original_affinity
        assert lease.record['final_priority'] == lease.original_priority
        try:
            with ResourceLease(os.getpid(), folder/'exception.json', allow_self=True):
                raise ValueError('Expected exception')
        except ValueError:
            pass
        normal=json.loads((folder/'exception.json').read_text('utf-8'))
        assert normal['final_affinity']==normal['original_affinity']
        assert normal['final_priority']==normal['original_priority']
        with ResourceLease(os.getpid(), folder/'watchdog.json', seconds=.2, allow_self=True) as lease:
            time.sleep(.5)
            assert lease.expired and not lease.applied
            assert lease.record['final_affinity']==lease.original_affinity
            assert lease.record['final_priority']==lease.original_priority
        print('Resource normal, exception and watchdog restoration passed')
    elif args.place or args.place_right:
        if args.place:
            idle()
        print(json.dumps(place(inventory(), folder/'placement.json', rightmost=args.place_right)))
    else:
        asyncio.run(run(folder))


if __name__ == '__main__':
    main()
