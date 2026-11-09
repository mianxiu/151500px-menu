"""Durable identity of one submitted UXP mutation; recovery only reads receipts."""
import json
import os
from pathlib import Path
import re
import time
import threading
from .operation_target import validate_target


def captured_target(response, session):
    value=response.get('captured_target')
    if not isinstance(value,dict) or value.get('session')!=session:
        raise ValueError('Missing captured target/session')
    return {**validate_target(value),'session':session,'instance_binding':session}


class PendingUxp:
    def __init__(self, path, storage):
        self.path = Path(path)
        self.storage = Path(storage).resolve()
        self.value = None
        self.error = ''
        self._execution_lock=threading.Lock()
        if self.path.exists():
            try:
                value = json.loads(self.path.read_text('utf-8'))
                self.validate(value)
                self.value = value
            except Exception:
                self.error = 'UXP 待核对记录损坏，已阻止修改；请保留记录并检查 Photoshop'

    def validate(self, value):
        if value.get('schema') != 1 or not re.fullmatch('[0-9a-f]{32}', value.get('id', '')):
            raise ValueError('Invalid pending request')
        if not re.fullmatch('[0-9a-f]{32}', value.get('task_id', '')) or not isinstance(value.get('session'), str) or not value['session']:
            raise ValueError('Invalid task/session')
        folder = Path(value['folder']).resolve()
        folder.relative_to(self.storage)
        if folder.name != 'PluginData' or folder.parent.name != 'com.15100px.menu.offline-probe':
            raise ValueError('Invalid plugin data folder')
        if value.get('operation') not in ('set_label', 'new_document'):
            raise ValueError('Not a modifying request')
        if 'preview_command' in value and value['preview_command'] not in ('shape.preview','shape.legacy.preview','layer.frame'):
            raise ValueError('Invalid preview execution identity')
        if 'thumbnail' in value and (value['thumbnail'] is not True or value.get('operation')!='set_label'):
            raise ValueError('Invalid thumbnail receipt identity')
        if 'capture_current' in value and value['capture_current'] is not True:
            raise ValueError('Invalid target binding mode')

    def begin(self, request, folder, task_id):
        if self.value or self.error:
            raise RuntimeError('上一项 UXP 操作结果待核对，未提交新任务')
        value = {key: request[key] for key in ('id', 'session', 'operation')}
        if request.get('capture_current') is True:value['capture_current']=True
        if request.get('command')=='layer.thumbnail':value['thumbnail']=True
        if request.get('command') in ('shape.preview','shape.legacy.preview','layer.frame'):value['preview_command']=request['command']
        value.update(schema=1, folder=str(Path(folder).resolve()), task_id=task_id,
                     submitted_at_ms=int(time.time()*1000))
        self.validate(value)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_suffix('.tmp')
        try:
            with temporary.open('w', encoding='utf-8') as stream:
                json.dump(value, stream)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, self.path)
        finally:
            if temporary.exists(): temporary.unlink()
        self.value = value

    def clear(self):
        # Only after a proven terminal result and durable task history, or a
        # publication failure known to precede dispatch. Never erase on timeout.
        if self.path.exists(): self.path.unlink()
        self.value = None

    def read_result(self, allow_unknown=False):
        value = self.value
        if not value: return None
        try:
            path = Path(value['folder']) / ('result-'+value['id']+'.json')
            if path.stat().st_size > (512*1024 if value.get('thumbnail') is True else 65536): return None
            result = json.loads(path.read_text('utf-8-sig'))
            if not isinstance(result, dict): return None
            if any(result.get(key) != value[key] for key in ('id', 'session', 'operation')):
                return None
            if result.get('schema') != 1 or result.get('phase') not in ('completed', 'failed'):
                return None
            # Older Photoshop can drop custom Error fields across executeAsModal.
            # This exact plugin-generated failure explicitly means rollback was
            # not verified. Preserve the receipt bytes and fail closed locally.
            uncertain = result.get('result_unknown') or (result.get('phase') == 'failed' and
                result.get('error') in ('预览回滚结果待核对', 'Error: 预览回滚结果待核对'))
            if value.get('capture_current') and result.get('phase')=='completed':
                try:captured_target(result,value['session'])
                except ValueError:
                    uncertain=True
                    result={**result,'error':'UXP 原操作已返回，但固定目标收据不完整；结果待核对'}
            if uncertain:
                if not allow_unknown: return None
                result = {**result, 'result_unknown':True}
            if result['phase'] == 'completed' and not isinstance(result.get('result'), dict):
                return None
            if result['phase'] == 'failed' and not isinstance(result.get('error'), str):
                return None
            return result
        except (OSError, ValueError, TypeError):
            return None

    def execution_has_exited(self, native_returned=False):
        with self._execution_lock:
            return self._execution_has_exited(native_returned)

    def _execution_has_exited(self, native_returned=False):
        """Independent host-exit evidence; never clears pending or changes outcome.

        A JSX receipt is written before DoJavaScript returns, so it alone is
        insufficient. Modern UXP explicitly attests that its awaited modal
        call exited. Only the original final receipt is read, never the host.
        """
        value=self.value
        if not value or self.error:return False
        command=value.get('preview_command')
        if command not in ('shape.preview','shape.legacy.preview','layer.frame'):return False
        identity={key:value[key] for key in ('id','session','operation')}
        engine='standalone-extendscript' if command=='shape.legacy.preview' else 'uxp-modal'
        path=self.path.parent/('uxp-execution-exit-'+value['id']+'.json')
        try:
            if path.stat().st_size<=4096:
                proof=json.loads(path.read_text('utf-8'))
                if (isinstance(proof,dict) and type(proof.get('schema')) is int and proof.get('schema')==1 and proof.get('exited') is True
                        and proof.get('engine')==engine and all(proof.get(k)==v for k,v in identity.items())):return True
        except (OSError,ValueError,TypeError):pass
        if command=='shape.legacy.preview':
            if native_returned is not True:return False
        else:
            response=self.read_result(allow_unknown=True)
            if response is None:return False
            from .plugins import version
            try:modern=version(response.get('probe_version'))>=(0,3,43)
            except (ValueError,TypeError):return False
            marker=response.get('execution_exit')
            if (not modern or not isinstance(marker,dict) or type(marker.get('schema')) is not int or marker.get('exited') is not True
                    or marker!={'schema':1,'engine':'uxp-modal','exited':True}):return False
        from .settings import atomic_json
        try:atomic_json(path,{**identity,'schema':1,'engine':engine,'exited':True})
        except OSError:return False
        return True

    def snapshot(self):
        if self.error: return {'active':True, 'state':'invalid', 'message':self.error}
        if not self.value: return {'active':False}
        value = self.value
        message = 'UXP 结果待核对，已阻止新操作；只读取原请求结果，不自动重发'
        session_current = False
        try:
            heartbeat = json.loads((Path(value['folder'])/'probe-last.json').read_text('utf-8-sig'))
            if not isinstance(heartbeat,dict):raise ValueError('Invalid heartbeat')
            if heartbeat.get('session') != value['session']:
                message = 'UXP 会话已改变，原任务结果尚未确认；保留记录，不自动重发'
            elif time.time()*1000-heartbeat.get('plugin_time_ms', 0)>15000:
                message = 'UXP 通讯中断，等待原任务最终记录；不自动重发'
            else:
                session_current = True
        except (OSError, ValueError, TypeError):
            message = 'UXP 通讯不可用，等待原任务最终记录；不自动重发'
        exited=self.execution_has_exited()
        if exited:message='脚本已退出，PS可正常使用；恢复结果待核对，已暂停插件修改，不自动重发'
        return {'active':True, 'state':'checking', 'message':message,'execution_exited':exited,
                'task_id':value['task_id'], 'request_id':value['id'], 'session_current':session_current}
