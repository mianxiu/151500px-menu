"""Serial Photoshop COM calls on one dedicated apartment thread."""
import asyncio
import json
import logging
import uuid
import time
from datetime import datetime, timezone
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from .paths import ROOT
from .layer_labels import LABEL_ACTIONS, label_script
from .photoshop_theme import read_theme, read_font_size
from .commands import ACTIONS, COMMANDS, validate_command
from .operation_target import CAPTURE_SCRIPT, validate_target, guarded_script
from .tasks import TaskStore, TaskCancelled


def load_scripts(folder=None):
    from photoshop_action.baseJsxInit import jsxFilesBooleanStr
    folder = Path(folder) if folder else ROOT / "jsx"
    move = (folder / "move_to_top_or_bottom.jsx").read_text(encoding="utf-8")
    return {"select": (folder / "select_all_layer_in_group.jsx").read_text(encoding="utf-8"),
            "color": (folder / "color_orange.jsx").read_text(encoding="utf-8"), "moveToTop": jsxFilesBooleanStr(move, True),
            "moveToBottom": jsxFilesBooleanStr(move, False)}


class PhotoshopBridge:
    def __init__(self, task_history=None):
        self.executor = ThreadPoolExecutor(max_workers=1, thread_name_prefix="photoshop-com")
        self.scripts = load_scripts()
        self.initialized = self.busy = self.closed = False
        self.last_error = ""
        self.focus_guard = None
        self.tasks = TaskStore(path=task_history)
        self.operation_timeout = 45
        self._captured_app = self._captured_target = None
        self.ui_theme = 'dark'
        self.ui_font_size = 'small'
        self.cached_status = {'connected': False, 'document': None, 'message': '检查 Photoshop 中'}
        self.status_timeout = .15
        self.status_cache_seconds = 1
        self._status_future = None
        self._status_tick = None
        self._status_updated_at = None
        self._status_error = ''
        self._status_attempt_tick = None
        self.queue_timeout = 30
        self.queue_limit = 8
        self._queue = []
        self._queue_changed = asyncio.Event()
        self._active_task_id = None

    def _application(self, prog_id='Photoshop.Application'):
        import pythoncom
        import win32com.client
        if not self.initialized:
            pythoncom.CoInitialize()
            self.initialized = True
        # Attach only: do not launch Photoshop implicitly.
        return win32com.client.GetActiveObject(prog_id)

    def _status(self):
        try:
            app = self._application()
            count = int(app.Documents.Count)
            self.ui_theme = read_theme(app, self.ui_theme)
            self.ui_font_size = read_font_size(app, self.ui_font_size)
            return {"connected": True, "ui_theme": self.ui_theme, "ui_font_size": self.ui_font_size, "document": str(app.ActiveDocument.Name) if count else None,
                    "message": "已连接 Photoshop" if count else "Photoshop 尚未打开文档"}
        except Exception:
            return {"connected": False, "ui_theme": self.ui_theme, "ui_font_size": self.ui_font_size, "document": None, "message": "请先启动 Photoshop"}

    async def status(self, force=False):
        now = time.monotonic()
        age = None if self._status_tick is None else now-self._status_tick
        pending = self._status_future is not None and not self._status_future.done()
        retry_ready = self._status_attempt_tick is None or now-self._status_attempt_tick>=self.status_cache_seconds
        if not self.busy and not self.closed and not pending and (force or retry_ready and (age is None or age>=self.status_cache_seconds)):
            self._status_attempt_tick = now
            self._status_future = asyncio.get_running_loop().run_in_executor(self.executor, self._status)
            self._status_future.add_done_callback(self._status_finished)
            pending = True
        if pending and not self.busy and not self.closed:
            try:await asyncio.wait_for(asyncio.shield(self._status_future),self.status_timeout)
            except asyncio.TimeoutError:pass
            except Exception:pass # Callback records an unavailable read; retain last known state.
        return self._status_snapshot()

    def _status_finished(self, future):
        try:
            self.cached_status = dict(future.result())
            self._status_tick = time.monotonic()
            self._status_updated_at = datetime.now(timezone.utc).isoformat()
            self._status_error = ''
        except Exception as exc:
            self._status_error = str(exc)
            logging.warning('Photoshop status read failed: %s', exc)

    def _status_snapshot(self):
        age_ms = None if self._status_tick is None else round((time.monotonic()-self._status_tick)*1000)
        pending = self._status_future is not None and not self._status_future.done()
        reason = 'service-closing' if self.closed else 'operation-running' if self.busy else 'status-read-pending' if pending else 'status-read-failed' if self._status_error else 'never-read' if age_ms is None else ''
        # A background refresh does not invalidate a recent successful read.
        # Keep pending diagnostics, but only show waiting once the cache is old.
        fresh = age_ms is not None and age_ms<=5000 and not self.busy and not self.closed and not self._status_error
        result = {**self.cached_status, 'ui_theme':self.ui_theme, 'ui_font_size':self.ui_font_size,
                  'busy':self.busy, 'last_error':self.last_error, 'queue_count':len(self._queue), 'queue_ready':False, 'execution_policy':'busy-reject',
                  'status_updated_at':self._status_updated_at,'status_age_ms':age_ms,
                  'status_fresh':fresh,
                  'status_wait_reason':reason,'status_read_pending':pending}
        if reason and not fresh:
            result['message'] = 'Photoshop 正在执行操作，状态等待更新' if self.busy else '状态等待更新，显示上次读取结果'
        return result

    def can_enqueue(self):
        if self.closed or not self.busy or not self._active_task_id or len(self._queue)>=self.queue_limit:return False
        task = self.tasks.get(self._active_task_id)
        return task['state']=='running' and bool(task.get('target')) and self._captured_app is not None

    def _capture_target(self):
        if self.focus_guard:
            self.focus_guard()
        app = self._application()
        if not app.Documents.Count:
            raise ValueError("请先在 Photoshop 中打开文档")
        target = validate_target(json.loads(app.DoJavaScript(CAPTURE_SCRIPT)))
        # This COM reference is retained and used on the same apartment thread;
        # never reconnect to a new Photoshop instance between capture and write.
        self._captured_app, self._captured_target = app, target
        return {**target, 'instance_binding':uuid.uuid4().hex}

    def _execute(self, action):
        if self.focus_guard:
            self.focus_guard()
        app, target = self._captured_app, self._captured_target
        if app is None or target is None:
            raise RuntimeError('未固定操作目标，操作已取消')
        script = label_script(action) if action in LABEL_ACTIONS else self.scripts[action]
        return app.DoJavaScript(guarded_script(script, target))

    def request_cancel(self, task_id):
        task = self.tasks.get(task_id)
        if task.get('restored'):
            raise ValueError('历史任务只读，不能取消或重新执行')
        if task['state']=='cancelled' and task.get('cancel_requested'):
            return {'task_id':task_id,'message':'任务已取消，未提交修改'}
        if task['state'] == 'queued':
            self.tasks.update(task_id,'cancelled',cancel_requested=True,message='排队任务已取消，未提交修改',error='')
            self._queue_changed.set()
            return {'task_id':task_id,'message':'排队任务已取消，未提交修改'}
        if task['state'] == 'cancel-requested':
            return {'task_id':task_id,'message':'取消已请求，等待目标读取结束；不会提交修改'}
        if task['state'] != 'capturing':
            raise ValueError('仅可取消尚在读取目标的任务；已提交的操作不能强制中断，等待 Photoshop 返回结果')
        self.tasks.update(task_id,'cancel-requested',cancel_requested=True,
                          message='已请求取消，等待目标读取结束；不会提交修改')
        return {'task_id':task_id,'message':'已请求取消；不会提交修改'}

    def _check_cancel(self, task_id):
        if self.tasks.get(task_id).get('cancel_requested'):
            raise TaskCancelled('任务已取消，未提交 Photoshop 修改')

    async def execute(self, action, before_dispatch=None, enqueue=False):
        command = validate_command(action)
        if self.closed:raise RuntimeError('服务正在退出')
        if not enqueue or not self.busy and not self._queue:
            return await self._execute_task(action,before_dispatch)
        # A waiting command inherits the fixed target and the actual apartment
        # reference, never a cached document name or a later active selection.
        active = self.tasks.get(self._active_task_id) if self._active_task_id else {}
        if active.get('state') != 'running' or not active.get('target') or self._captured_app is None:
            raise RuntimeError('当前任务目标尚未固定或结果未知，不能追加排队任务')
        if len(self._queue)>=self.queue_limit:raise RuntimeError('等待队列已满，请稍后再试')
        target, app = dict(active['target']), self._captured_app
        task_id = self.tasks.create(action,command.label,state='queued',target=target,
                                    wait_reason='previous-operation',queue_timeout_seconds=self.queue_timeout,
                                    predecessor_id=active['id'],
                                    message='等待前一项任务结束；目标改变时取消')
        self._queue.append(task_id)
        deadline = time.monotonic()+self.queue_timeout
        try:
            await self.tasks.flush()
            while True:
                self._queue_changed.clear()
                if self.tasks.get(task_id)['state']=='cancelled':raise TaskCancelled('排队任务已取消，未提交修改')
                if self.closed:raise TaskCancelled('服务正在退出，排队任务已取消')
                remaining = deadline-time.monotonic()
                if remaining<=0:raise TaskCancelled('排队期限已到，未提交 Photoshop 修改')
                if self._queue[0]==task_id and not self.busy:
                    previous = self.tasks.get(self._active_task_id)
                    if previous['state']!='succeeded' or previous.get('wait_error'):
                        raise TaskCancelled('前一项操作失败或曾超时，排队任务已取消；请检查 Photoshop')
                    self._queue.pop(0)
                    return await self._execute_task(action,before_dispatch,task_id,(app,target))
                try:await asyncio.wait_for(self._queue_changed.wait(),remaining)
                except asyncio.TimeoutError:pass
        except asyncio.CancelledError:
            if self.tasks.get(task_id)['state']=='queued':
                self.tasks.update(task_id,'cancelled',message='请求等待已取消，未提交修改',error='')
            raise
        except Exception as exc:
            if self.tasks.get(task_id)['state']=='queued':
                self.tasks.update(task_id,'cancelled' if isinstance(exc,TaskCancelled) else 'failed',message=str(exc),error='' if isinstance(exc,TaskCancelled) else str(exc))
            exc.task_id=task_id
            raise
        finally:
            if task_id in self._queue:self._queue.remove(task_id)
            self._queue_changed.set()

    def _capture_queued_target(self, binding):
        app, expected = binding
        if self.focus_guard:self.focus_guard()
        actual = validate_target(json.loads(app.DoJavaScript(CAPTURE_SCRIPT)))
        if actual != validate_target(expected):
            raise TaskCancelled('排队期间文档或图层选择已改变，未提交修改')
        self._captured_app, self._captured_target = app, actual
        return dict(expected)

    async def _execute_task(self, action, before_dispatch=None, task_id=None, binding=None, capture=None, execute=None, interactive=False, result_target=None):
        command = validate_command(action)
        if self.closed:
            raise RuntimeError("服务正在退出")
        if self.busy:
            raise RuntimeError("上一项操作尚未完成，请稍后再试")
        self.busy = True
        if task_id:self.tasks.update(task_id,'capturing',wait_reason='',message='开始重新核验固定目标')
        else:task_id = self.tasks.create(action, command.label)
        self._active_task_id = task_id
        future = None
        dispatched = False
        target = None
        def finished(completed):
            nonlocal target
            self.busy = False
            error = completed.exception() if not completed.cancelled() else None
            if dispatched:
                try:
                    if error:raise error
                    if result_target:target=result_target(completed.result())
                    message = self._result_message(action, completed.result())
                    self.tasks.update(task_id, 'succeeded', message=message, error='',target=target)
                except Exception as exc:
                    error = exc
                    if getattr(exc,'result_unknown',False):
                        self.busy=True
                        if self.tasks.get(task_id)['state']=='running':self.tasks.update(task_id,'result-unknown',error=str(exc))
                    else:self.tasks.update(task_id, 'failed', error=str(exc))
            else:
                self.tasks.update(task_id, 'failed' if error else 'cancelled',
                                  message='目标读取已结束，未提交修改', error=str(error or ''))
            self.last_error = str(error) if error else ""
            self._queue_changed.set()
        try:
            await self.tasks.flush()
            self._check_cancel(task_id)
            future = asyncio.get_running_loop().run_in_executor(self.executor, self._capture_queued_target, binding) if binding else asyncio.get_running_loop().run_in_executor(self.executor, capture or self._capture_target)
            target = await asyncio.wait_for(asyncio.shield(future), self.operation_timeout)
            self._check_cancel(task_id)
            self.tasks.update(task_id, 'capturing', target=target)
            logging.info('Operation target captured: action=%s target=%s', action, target)
            self.tasks.update(task_id, 'running')
            await self.tasks.flush()
            # Recheck the menu session after the durable-write wait, so opening
            # a new menu while disk I/O completes cancels the old operation.
            if before_dispatch:
                await before_dispatch(target)
            future = asyncio.get_running_loop().run_in_executor(self.executor, execute or self._execute, action)
            self._operation_future = future
            dispatched = True
            future.add_done_callback(finished)
            execution_timeout = 120 if action in ('text.split','text.object') else self.operation_timeout
            result = await asyncio.wait_for(asyncio.shield(future), None if action=='editStyle' or interactive else execution_timeout)
            if result_target:target=result_target(result)
            return {"message": self._result_message(action,result), "result": str(result or ""), "target":target, 'task_id':task_id}
        except asyncio.TimeoutError:
            if not dispatched:
                message='目标读取超时，未执行操作'
                self.tasks.update(task_id,'cancel-requested',error=message)
            else:
                message='操作超时，结果尚未确定；请检查 Photoshop，避免重复操作'
                if self.tasks.get(task_id)['state']=='running':
                    self.tasks.update(task_id,'result-unknown',error=message,wait_error=message)
            error=RuntimeError(message);error.task_id=task_id
            raise error from None
        except asyncio.CancelledError:
            if self.tasks.get(task_id)['state'] in ('capturing','running'):
                self.tasks.update(task_id,'result-unknown' if dispatched else 'cancel-requested',
                                  message='等待已取消；已提交的操作等待 Photoshop 返回结果' if dispatched else '等待已取消，未提交修改',
                                  wait_error='请求等待已取消，结果尚未确定' if dispatched else '')
            raise
        except (ValueError, RuntimeError) as exc:
            if not dispatched:
                if isinstance(exc,TaskCancelled):self.tasks.update(task_id,'cancelled',message=str(exc),error='')
                else:self.tasks.update(task_id,'failed',error=str(exc))
            exc.task_id=task_id
            raise
        except Exception as exc:
            if not dispatched:self.tasks.update(task_id,'failed',error=str(exc))
            error=RuntimeError("Photoshop 执行失败：" + str(exc));error.task_id=task_id
            raise error from exc
        finally:
            if not dispatched:
                if future is not None and not future.done():
                    future.add_done_callback(finished)
                else:
                    self.busy = False
                    if self.tasks.get(task_id)['state']=='cancel-requested':
                        self.tasks.update(task_id,'cancelled',message='目标读取已结束，未提交修改')
                    self._queue_changed.set()

    def _result_message(self, action, result):
        message = validate_command(action).label + '已完成'
        if action=='shape.paint' and json.loads(result).get('cancelled'):return '已取消预览并恢复原值'
        if action in ('editStyle','pasteStyle') and json.loads(result).get('cancelled'):return '已取消图层样式，未提交修改'
        if action == 'label.none':
            details = json.loads(result)
            message = f"已清除 {details['count']} 个图层的标签，并核验为无颜色"
            if details.get('inherited'):
                message += '；父组仍有颜色标签，图层面板可能继续显示父组颜色'
        return message

    def replace_scripts(self, scripts):
        if self.busy or self._queue or self.closed:
            raise RuntimeError("请等待图层操作完成后再更换脚本包")
        self.scripts = dict(scripts)

    def _release(self):
        self._captured_app = self._captured_target = None
        if self.initialized:
            import pythoncom
            pythoncom.CoUninitialize()
            self.initialized = False

    async def close(self):
        self.closed = True
        self._queue_changed.set()
        await asyncio.get_running_loop().run_in_executor(self.executor, self._release)
        self.executor.shutdown(wait=False)
        await self.tasks.close()
