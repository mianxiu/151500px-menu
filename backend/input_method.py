"""Temporarily close the PS IME context; never select another keyboard layout."""
import ctypes
from ctypes import wintypes as W
import hashlib
import json
import math
import time
from .input_language import InputLanguage,process_exited,read_layout_journal
from .input_accessibility import FocusAccessibility,control_state,CONTAINERS
from .settings import atomic_json
from .input_context import NativeImeContext


def ime_target(value):
    return value.get('identity'),value.get('ime'),value.get('layout'),value.get('context_nonce')


TYPE_TOOLS=frozenset({'typeCreateOrEditTool','typeVerticalCreateOrEditTool',
                      'typeMaskTool','typeVerticalMaskTool'})


def type_tool(classID):
    return isinstance(classID,str) and classID in TYPE_TOOLS


class ImePolicy:
    def __init__(self):
        self.owned=None;self.pending=None;self.attempted=None
        self.state='unknown';self.confirmed_disables=0;self.confirmed_restores=0

    def step(self,sample,intent,now):
        self.state=sample.get('state','unknown')
        if not sample.get('ime') or type(sample.get('ime_open')) is not int:return
        if self.state=='input':self.attempted=None
        if self.pending:
            request=self.pending
            if ime_target(sample)!=ime_target(request):return
            if sample['ime_open']!=request['wanted']:
                if now-request['time']>1:self.state='request-not-confirmed'
                return
            self.pending=None
            self.owned=request if request['disable'] else None
            if request['disable']:self.confirmed_disables+=1
            else:
                self.confirmed_restores+=1
                if request['input_restore']:self.attempted=None
        if self.owned:
            if ime_target(sample)!=ime_target(self.owned):return
            if sample['ime_open']!=0:
                self.owned=None
                self.attempted=None
                if sample.get('native_restored'):
                    self.confirmed_restores+=1
                if self.state=='non-input' and intent is not None:
                    self.attempted=intent;return self.request(sample,True,now)
                return
            if self.state!='non-input':return self.request(sample,False,now)
        elif (intent is not None and intent!=self.attempted and self.state=='non-input' and
              sample['ime_open']==1):
            self.attempted=intent
            return self.request(sample,True,now)

    def request(self,sample,disable,now):
        self.pending={k:sample[k] for k in ('root','identity','focus','layout','ime')}
        if sample.get('context_nonce') is not None:self.pending['context_nonce']=sample['context_nonce']
        self.pending.update(wanted=0 if disable else 1,original=1,disable=disable,
                            input_restore=not disable and sample['state']=='input',time=now)
        return dict(self.pending)


class ImeContext:
    def __init__(self,user):
        self.user=user
        self.imm=ctypes.WinDLL('imm32')
        self.imm.ImmGetDefaultIMEWnd.argtypes=[W.HWND];self.imm.ImmGetDefaultIMEWnd.restype=W.HWND

    def read(self,sample):
        import win32gui,win32process
        value={**sample,'ime':None,'ime_open':None}
        if not sample.get('root'):return value
        hwnd=self.imm.ImmGetDefaultIMEWnd(sample['focus'])
        if (not hwnd or not win32gui.IsWindow(hwnd) or win32gui.GetClassName(hwnd)!='IME' or
            list(win32process.GetWindowThreadProcessId(hwnd))!=[sample['identity'][3],sample['identity'][1]]):return value
        opened=win32gui.SendMessageTimeout(hwnd,0x283,5,0,2,200)[1]
        if type(opened) is int and opened in (0,1):value.update(ime=hwnd,ime_open=opened)
        return value

    def send(self,request):
        import win32gui
        # SETOPENSTATUS is synchronous-only. No character or layout message is sent.
        win32gui.SendMessageTimeout(request['ime'],0x283,6,request['wanted'],2,200)

    def observe_owned(self,owned):
        import win32gui,win32process,win32api
        identity=owned['identity'];root,pid,created,thread=identity
        if not win32gui.IsWindow(root) or list(win32process.GetWindowThreadProcessId(root))!=[thread,pid]:return {'state':'outside'}
        handle=win32api.OpenProcess(0x1000,False,pid)
        try:
            if str(win32process.GetProcessTimes(handle)['CreationTime'])!=created:return {'state':'outside'}
        finally:handle.Close()
        class Info(ctypes.Structure):
            _fields_=[('cbSize',W.DWORD),('flags',W.DWORD),('active',W.HWND),('focus',W.HWND),
                      ('capture',W.HWND),('menu',W.HWND),('move',W.HWND),('caret',W.HWND),('rect',W.RECT)]
        info=Info(cbSize=ctypes.sizeof(Info))
        if not self.user.GetGUIThreadInfo(thread,ctypes.byref(info)):return {'state':'outside'}
        focus=info.focus or (owned['focus'] if win32gui.IsWindow(owned['focus']) else root)
        # Attached menu input queues can expose another process's focused HWND.
        if list(win32process.GetWindowThreadProcessId(focus))!=[thread,pid]:
            focus=owned['focus'] if win32gui.IsWindow(owned['focus']) else root
            if list(win32process.GetWindowThreadProcessId(focus))!=[thread,pid]:return {'state':'outside'}
        return self.read({'root':root,'identity':identity,'focus':focus,'layout':self.user.GetKeyboardLayout(thread),'state':'outside'})


class InputMethod(InputLanguage):
    def __init__(self,desktop):
        super().__init__(desktop)
        self.policy=ImePolicy();self.legacy=None;self.legacy_saved=None;self.legacy_exited=None;self.ime=None
        self.context_schema=1
        self.edit_refresh_until=0
        self.verification=None;self.ready={}
        self.focus_policy=False;self.policy_window=0;self.policy_identity=None;self.policy_failures=0
        self.canvas_tool=None;self.canvas_focus=0;self.canvas_state=False
        self.canvas_entry_at=None;self.canvas_entry_wall_ms=0;self.canvas_modal_seen=False;self.canvas_modal_state=False
        self.canvas_entry_tool_seq=0;self.canvas_last_seq=None;self.canvas_last_correct_enter=None
        self.modal_events_seen=0;self.canvas_timeouts=0
        self.report={'state':'unknown','scope':'photoshop-temporary-ime','layout_requests':0,
                     'disable_requests':0,'restore_requests':0,'focus_policy':False,'policy_windows':0,
                     'policy_reason':0,'policy_residual':False,'policy_age_ms':None}

    def saved(self):return {'schema':self.context_schema,'owned':self.policy.owned,'pending':self.policy.pending}

    def ready_context(self):
        with self.lock:return dict(self.ready)

    def verify(self,request,reply):
        self.edit_end()
        with self.lock:self.verification=(dict(request),reply,self.serial)
        self.wake.set()

    def complete_verification(self,sample,serial=None):
        if serial is None:serial=self.snapshot()[0]
        guard=getattr(self.desktop,'input_focus',None)
        focus=guard.shortcut_context() if guard else {}
        protected=bool((self.policy.owned or sample.get('focus_policy')) and sample.get('ime_open')==0 and not sample.get('policy_residual') and
                       (sample.get('tsf_disabled') is True or
                        (sample.get('focus_policy') and int(sample.get('policy_windows') or 0)>0)))
        ready=bool(protected and not self.policy.pending and focus.get('state')=='non-input' and
                   focus.get('root')==sample.get('root') and focus.get('focus')==sample.get('focus'))
        with self.lock:
            self.ready={key:focus[key] for key in ('root','focus','epoch')} if ready and serial==self.serial else {}
            pending=self.verification
            if not pending:return
            request,reply,required_serial=pending
            if serial<required_serial or serial!=self.serial:return
            valid=(request.get('input_epoch')==focus.get('epoch') and request.get('root')==sample.get('root') and request.get('hwnd')==sample.get('focus'))
            result=2 if valid and ready else 1 if valid and sample.get('state')=='input' and sample.get('ime_open')==1 and not self.policy.pending and not self.policy.owned else None
            if not valid or self.tick()>request['expires_tick_ms']:result=0
            if result is None:return
            self.verification=None
        self.report['last_ime_verification']={'id':request['verify_id'],'result':result,'reason':'scope-changed' if not valid else 'expired' if result==0 else 'protected' if result==2 else 'input-restored','elapsed_ms':max(0,self.tick()-request['expires_tick_ms']+200)}
        reply(request,result)

    def click(self,hwnd,root):
        super().click(hwnd,root)
        with self.lock:self.edit_refresh_until=time.monotonic()+2
        self.wake.set()

    def edit_end(self):
        self.activity()
        with self.lock:self.edit_refresh_until=time.monotonic()+2
        self.report['edit_end_events']=self.report.get('edit_end_events',0)+1
        self.wake.set()

    def load(self,path):
        if not path.exists():return
        if path.stat().st_size>16384:raise ValueError('IME journal size')
        value=json.loads(path.read_text('utf-8'))
        if not isinstance(value,dict) or set(value)!={'schema','owned','pending'} or type(value['schema']) is not int or value['schema'] not in (1,2):raise ValueError('IME journal schema')
        self.context_schema=value['schema']
        fields={'root','identity','focus','layout','ime','wanted','original','disable','input_restore','time'}
        if self.context_schema==2:fields.add('context_nonce')
        for row in (value['owned'],value['pending']):
            if row is None:continue
            if (not isinstance(row,dict) or set(row)!=fields or
                self.context_schema==2 and (type(row['context_nonce']) is not int or not 1<=row['context_nonce']<=0x7fffffff) or
                any(type(row[k]) is not int or row[k]<=0 for k in ('root','focus','layout','ime')) or
                not isinstance(row['identity'],list) or len(row['identity'])!=4 or row['identity'][0]!=row['root'] or
                any(type(row['identity'][i]) is not int or row['identity'][i]<=0 for i in (0,1,3)) or
                not isinstance(row['identity'][2],str) or not row['identity'][2] or
                type(row['wanted']) is not int or row['wanted'] not in (0,1) or type(row['original']) is not int or row['original']!=1 or
                type(row['disable']) is not bool or row['wanted']!=int(not row['disable']) or
                type(row['input_restore']) is not bool or row['disable'] and row['input_restore'] or
                type(row['time']) not in (int,float) or not math.isfinite(row['time'])):raise ValueError('IME journal record')
        if value['owned'] and not value['owned']['disable']:raise ValueError('IME unconfirmed ownership')
        self.policy.owned=value['owned'];self.policy.pending=value['pending']
        if self.policy.pending:self.policy.pending['time']=time.monotonic()-2

    def retire(self,path,reason):
        owned=self.policy.owned
        if not owned or self.policy.pending:return
        key=hashlib.sha256(json.dumps(owned,sort_keys=True).encode()).hexdigest()
        atomic_json(path.parent/('input-method-retired-'+key+'.json'),{'owned':owned,'reason':reason,'restore_sent':False})
        atomic_json(path,{'schema':self.context_schema,'owned':None,'pending':None})
        self.policy.owned=None;self.policy.attempted=None

    def legacy_step(self,sample):
        if not self.legacy:return True
        legacy=self.legacy;path=self.desktop.runtime/'input-language-owned.json'
        if legacy.policy.pending and sample.get('root') and sample.get('identity'):
            identities=[row['identity'] for row in (legacy.policy.owned,legacy.policy.pending) if row]
            independent=all(sample['identity'][1:3]!=identity[1:3] for identity in identities)
            if independent and self.legacy_exited is None and all(process_exited(identity) for identity in identities):
                original=path.read_bytes();key=hashlib.sha256(original).hexdigest()
                saved=json.loads(original)
                if saved.get('owned')!=legacy.policy.owned or {k:v for k,v in saved.get('pending',{}).items() if k!='time'}!={k:v for k,v in legacy.policy.pending.items() if k!='time'}:
                    raise RuntimeError('旧布局未知记录身份已变化；保留原保护')
                archive=path.parent/('input-language-unknown-exited-'+key+'.json')
                evidence={'result_status':'unknown','original_journal_utf8':original.decode('utf-8'),
                          'identities':identities,'original_processes_exited':True,
                          'original_slot_released':False,'request_republished':False,'marked_restored':False}
                atomic_json(archive,evidence)
                if json.loads(archive.read_text('utf-8'))!=evidence or path.read_bytes()!=original:
                    raise RuntimeError('旧布局未知记录归档未核验；保留原保护')
                self.legacy_exited=identities
            # The original slot stays pinned; only a different, freshly verified PS session may proceed.
            if self.legacy_exited and all(sample['identity'][1:3]!=identity[1:3] for identity in self.legacy_exited):
                self.report.update(legacy_pending=True,legacy_unknown_preserved=True,legacy_scope='original-process-exited')
                return True
        legacy.retire_exited_owner()
        if not legacy.policy.owned and not legacy.policy.pending:
            self.legacy=None;self.report['legacy_pending']=False;return True
        sample={**sample,'state':'unknown'}
        request=legacy.policy.step(sample,None,None,time.monotonic())
        if request:
            if request['switch']:raise ValueError('Legacy new layout switch prohibited')
            fresh=self.sample()
            if any(fresh.get(k)!=sample.get(k) for k in ('identity','focus','layout')):
                legacy.policy.pending=None;legacy.policy.restore_attempt=None
            else:
                atomic_json(path,{'owned':legacy.policy.owned,'pending':legacy.policy.pending})
                self.user.PostMessageW(request['focus'],0x50,0,request['wanted'])
                self.report['legacy_restore_requests']=self.report.get('legacy_restore_requests',0)+1
        saved={'owned':legacy.policy.owned,'pending':legacy.policy.pending}
        if saved!=self.legacy_saved:atomic_json(path,saved);self.legacy_saved=json.loads(json.dumps(saved))
        self.report.update(state='legacy-layout-recovery',legacy_pending=bool(legacy.policy.pending))
        return False

    def read_tool(self,sample):
        self.report['tool_reads']+=1
        try:
            value=self.desktop.input_language_tool(identity=sample['identity'])
            self.report.pop('tool_error_type',None)
            return value
        except Exception as error:self.report['tool_error_type']=type(error).__name__

    def policy_fields(self,sample,status=None):
        if status is not None:
            sample.update(self.ime.policy_state(sample.get('ime') or self.policy_window,status))
        tick=sample.get('policy_tick') or 0
        sample['policy_age_ms']=max(0,self.tick()-tick) if tick else None
        return sample

    def push_type_tool(self,tool):
        """Publish the current type-tool flag; one send per actual change."""
        if not self.focus_policy or not self.policy_window or not isinstance(tool,dict):return
        classID=tool.get('classID')
        if not isinstance(classID,str):return
        active=type_tool(classID)
        if active==self.canvas_tool:return
        try:
            self.ime.set_canvas_tool(self.policy_window,active)
            self.canvas_tool=active
            self.report.pop('canvas_push_error',None)
        except Exception as error:self.report['canvas_push_error']=type(error).__name__

    def canvas_event(self,detail,sample):
        """UXP modal enter/exit: one timestamped correction, ordered by sequence."""
        if not detail or not self.focus_policy or not self.policy_window:return
        sequence=detail['sequence']
        if self.canvas_last_seq is not None and detail['session']==self.canvas_last_seq[0] and sequence<=self.canvas_last_seq[1]:return
        self.canvas_last_seq=(detail['session'],sequence)
        self.modal_events_seen+=1
        enter=detail['state']=='enter'
        self.canvas_modal_state=enter
        if enter:self.canvas_modal_seen=True
        try:self.ime.canvas_correct(self.policy_window,enter,detail['created_at_ms'])
        except Exception as error:self.report['canvas_correction_error']=type(error).__name__
        if not enter:self.canvas_entry_at=None

    def canvas_tick(self,sample,tool_state):
        """Require a newer non-modal snapshot before reclaiming a pending entry."""
        if not self.focus_policy or not self.policy_window:return sample
        editing=bool(sample.get('canvas_text_editing'))
        if editing:
            if self.canvas_state is not True:
                self.canvas_state=True;self.canvas_entry_at=time.monotonic()
                self.canvas_entry_wall_ms=time.time()*1000
                self.canvas_modal_seen=self.canvas_modal_state
                self.canvas_entry_tool_seq=tool_state.get('sequence',0) if tool_state else 0
            if tool_state and tool_state.get('sequence',0)>self.canvas_entry_tool_seq and tool_state.get('isModal'):
                self.canvas_modal_seen=True
            if (sample.get('canvas_entry_pending') and not self.canvas_modal_seen and
                    tool_state and tool_state.get('sequence',0)>self.canvas_entry_tool_seq and
                    tool_state.get('created_at_ms',0)>self.canvas_entry_wall_ms and
                    tool_state.get('isModal') is False and self.canvas_entry_at is not None and
                    time.monotonic()-self.canvas_entry_at>=.8):
                try:
                    self.ime.canvas_correct(self.policy_window,False,time.time()*1000)
                    self.canvas_timeouts+=1
                except Exception as error:self.report['canvas_correction_error']=type(error).__name__
                self.canvas_entry_at=None
        else:
            self.canvas_state=False;self.canvas_entry_at=None;self.canvas_modal_seen=False
        # A modal text edit with the canvas still disabled is a missed entry.
        if (tool_state and tool_state.get('isModal') and self.canvas_modal_state and not editing and
                type_tool(tool_state.get('classID')) and
                self.canvas_last_correct_enter!=tool_state.get('sequence')):
            try:
                self.ime.canvas_correct(self.policy_window,True,tool_state['created_at_ms'])
                self.canvas_last_correct_enter=tool_state.get('sequence')
            except Exception as error:self.report['canvas_correction_error']=type(error).__name__
        self.report.update(canvas_text_editing=editing,canvas_entry_pending=bool(sample.get('canvas_entry_pending')),
                           type_tool_active=bool(sample.get('type_tool_active')),type_tool_known=bool(sample.get('type_tool_known')),
                           modal_events=self.modal_events_seen,canvas_timeouts=self.canvas_timeouts)
        return sample

    def canvas_diagnostics(self):
        if not self.focus_policy or not self.policy_window:return {'available':False,'state':'policy-off'}
        try:
            status=self.ime.command(self.policy_window,1,self.canvas_focus,timeout=250)
            row=self.ime.canvas_diagnostics(self.policy_window,status,self.canvas_focus)
            row['timeouts']=self.canvas_timeouts;row['modal_events']=self.modal_events_seen
            return row
        except Exception as error:return {'available':False,'error_type':type(error).__name__}

    def stop_focus_policy(self):
        """Restore every saved association and confirm the native policy is off."""
        if not self.policy_window:
            self.focus_policy=False;return True
        import win32gui
        window=self.policy_window
        try:
            if not win32gui.IsWindow(window):raise RuntimeError('PS输入保护窗口已结束')
            status=self.ime.set_policy(window,False)
        except Exception as error:
            self.report['policy_error']=type(error).__name__
            return False
        fields=self.ime.policy_state(window,status)
        if type(status) is not int or not status&1 or fields['policy_windows'] or fields['policy_residual']:
            self.report['policy_error']='restore-unconfirmed'
            return False
        self.focus_policy=False;self.policy_window=0;self.policy_identity=None
        self.canvas_tool=None;self.canvas_state=False;self.canvas_modal_state=False
        self.canvas_entry_at=None
        self.report.pop('policy_error',None)
        self.report.update(focus_policy=False,policy_windows=0,policy_residual=False,
                           **{key:fields[key] for key in ('policy_reason','policy_tick')})
        return True

    def sync_focus_policy(self,sample,old_context=False):
        """Enable the synchronous focus policy once PS and the component are attached.

        No lease disable request is issued in this mode; old lease records stay
        journaled and are never re-sent or cleared by this path.
        """
        if not callable(getattr(self.ime,'set_policy',None)) or not callable(getattr(self.ime,'policy_state',None)):
            return sample
        import win32gui
        window=sample.get('ime');identity=tuple(sample.get('identity',[])) if sample.get('identity') else None
        if self.policy_window and not win32gui.IsWindow(self.policy_window):
            self.focus_policy=False;self.policy_window=0;self.policy_identity=None
        if self.focus_policy and self.policy_window:
            if not self.desktop.settings['enabled']:return self.stop_focus_policy()
            if (identity and self.policy_identity and identity!=self.policy_identity) or (window and window!=self.policy_window):
                # The previous PS session/control cannot own the new session.
                if win32gui.IsWindow(self.policy_window):
                    if not self.stop_focus_policy():return sample
                self.focus_policy=False;self.policy_window=0;self.policy_identity=None
        eligible=(self.context_schema==2 and not old_context and self.desktop.settings['enabled'] and
                  window and identity and not self.policy.owned and not self.policy.pending)
        if not self.focus_policy and eligible:
            status=self.ime.set_policy(window,True)
            if type(status) is int and status&1 and status&0x100 and not status&8 and not status&0x200:
                self.focus_policy=True;self.policy_window=window;self.policy_identity=identity
                self.policy_failures=0
                self.canvas_tool=None;self.canvas_state=False;self.canvas_modal_state=False
                self.canvas_last_seq=None;self.modal_events_seen=0;self.canvas_entry_at=None
                self.policy_fields(sample,status)
            else:
                self.policy_failures+=1
                self.report['policy_error']=int(status) if type(status) is int else 'none'
        elif self.focus_policy and window:
            self.policy_fields(sample)
        if self.policy.owned or self.policy.pending:
            self.report['policy_blocked']='legacy-lease-preserved'
        else:self.report.pop('policy_blocked',None)
        return sample

    def run(self):
        import pythoncom
        pythoncom.CoInitializeEx(pythoncom.COINIT_MULTITHREADED)
        self.user=ctypes.WinDLL('user32',use_last_error=True)
        self.user.GetKeyboardLayout.argtypes=[W.DWORD];self.user.GetKeyboardLayout.restype=W.HANDLE
        self.tick=ctypes.WinDLL('kernel32',use_last_error=True).GetTickCount64
        self.tick.restype=ctypes.c_ulonglong
        self.user.GetKeyboardLayoutList.argtypes=[ctypes.c_int,ctypes.POINTER(W.HANDLE)]
        self.user.PostMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM]
        path=self.desktop.runtime/'input-method-owned.json';persisted=None
        try:
            self.ime=ImeContext(self.user)
            try:
                self.load(path)
                layouts=(W.HANDLE*64)();count=self.user.GetKeyboardLayoutList(64,layouts)
                saved=read_layout_journal(self.desktop.runtime/'input-language-owned.json',list(layouts)[:count])
                if saved['owned'] or saved['pending']:
                    self.legacy=InputLanguage(self.desktop);self.legacy.policy.owned=saved['owned'];self.legacy.policy.pending=saved['pending']
                    self.legacy_saved=json.loads(json.dumps(saved))
            except Exception as error:self.report.update(state='journal-invalid',error_type=type(error).__name__);return
            try:self.accessibility=FocusAccessibility()
            except Exception as error:
                self.report['uia_error_type']=type(error).__name__
                import logging
                logging.exception('IME input observer initialization failed')
            self.report.update(uia_available=self.accessibility is not None,tool_reads=0)
            old_context=self.context_schema==1 and bool(self.policy.owned or self.policy.pending)
            if not old_context:self.ime=NativeImeContext(self.user);self.context_schema=2
            self.report['mechanism']='open-close-recovery' if old_context else 'ps-thread-context-association'
            examined=None;tool=None;context_before=None;episode=0;blocked_before=False;retry_after=0
            revision_before=None;next_tool_check=0;recovering=False;recovery_after=0;stable=None;stable_at=0
            next_uia_check=time.monotonic()+1;tool_event=None
            while not self.desktop.stop.is_set():
                self.wake.wait(.05);self.wake.clear()
                if self.desktop.stop.is_set():break
                try:
                    serial,epoch,protected,_=self.snapshot();sample=self.sample()
                    if not self.desktop.settings['enabled'] and self.focus_policy:self.stop_focus_policy()
                    # Attach and arm the synchronous focus policy before any UXP
                    # or tool read: PS startup must not delay shortcut protection.
                    if (self.context_schema==2 and not old_context and self.desktop.settings['enabled'] and
                            sample.get('root') and not self.focus_policy and not self.policy.owned and not self.policy.pending):
                        try:
                            early=self.ime.read(sample)
                            self.sync_focus_policy(early,old_context)
                            if self.focus_policy:
                                for key in ('ime','ime_open','focus_policy','policy_windows','policy_reason','policy_residual',
                                            'policy_tick','policy_age_ms','context_nonce','context_revision','native_restored',
                                            'tsf_disabled','native_events','native_menu_held'):
                                    if key in early:sample[key]=early[key]
                        except Exception as error:self.report['policy_error']=type(error).__name__
                    state_reader=getattr(self.desktop,'input_tool_state',None)
                    tool_state=state_reader() if sample.get('root') and state_reader else None
                    self.push_type_tool(tool_state)
                    detail_reader=getattr(self.desktop,'input_tool_event_detail',None)
                    detail=detail_reader() if sample.get('root') and detail_reader else None
                    event_reader=getattr(self.desktop,'input_tool_event',None)
                    event=(detail['session'],detail['sequence'],detail['state']) if detail else (
                        event_reader() if sample.get('root') and event_reader else None)
                    if event and detail is None:
                        detail={'session':event[0],'sequence':event[1],'state':event[2],
                                'created_at_ms':time.time()*1000}
                    if event and event!=tool_event:
                        tool_event=event
                        self.canvas_event(detail,sample)
                        self.protect() if event[2]=='enter' else self.edit_end()
                        self.report['tool_events']=self.report.get('tool_events',0)+1
                        continue
                    if not self.legacy_step(sample):continue
                    if self.policy.owned and not self.policy.pending and process_exited(self.policy.owned['identity']):self.retire(path,'original-process-exited')
                    scope=tuple(sample.get('identity',[]))
                    mouse=bool(self.user.GetAsyncKeyState(1)&0x8000)
                    input_panel=self.desktop.visible and (getattr(self.desktop,'color_editor_active',False) or getattr(self.desktop,'menu_tools_active',False))
                    evidence_blocked=mouse or sample.get('blocked') or input_panel or self.busy() or not self.desktop.settings['enabled'] or old_context
                    blocked=evidence_blocked or self.desktop.visible
                    if sample.get('root') and not evidence_blocked and 'uia_error_type' in self.report and time.monotonic()>=next_uia_check:
                        try:
                            self.accessibility=FocusAccessibility();self.report['uia_available']=True
                            self.report['uia_reconnects']=self.report.get('uia_reconnects',0)+1
                        except Exception as error:self.report['uia_error_type']=type(error).__name__
                        next_uia_check=time.monotonic()+1
                    if bool(blocked)!=blocked_before:episode+=1;blocked_before=bool(blocked)
                    if evidence_blocked:examined=None;tool=None
                    row=sample.get('accessibility',{});context=(scope,sample.get('focus'),serial,sample.get('layout'))
                    if context!=context_before:episode+=1;context_before=context;examined=None;tool=None
                    needs_tool=(sample.get('workspace') and sample.get('state')=='unknown' and control_state(row,sample.get('pid'))!='input' and
                                (row.get(30003) in CONTAINERS or protected or self.accessibility is None))
                    if not needs_tool:examined=None;tool=None
                    if (needs_tool and tool is not None and not self.policy.owned and not self.policy.pending and
                            (time.monotonic()>=next_tool_check or isinstance(tool,dict) and tool.get('isModal') is True and time.monotonic()<self.edit_refresh_until)):examined=None
                    reconcile=getattr(self.desktop,'input_language_reconcile',None)
                    if needs_tool and context==examined and tool is None and not evidence_blocked and reconcile and reconcile():examined=None
                    if needs_tool and context!=examined and not evidence_blocked:
                        examined=context;tool=self.read_tool(sample);next_tool_check=time.monotonic()+1;sample=self.sample()
                    same=(tuple(sample.get('identity',[])),sample.get('focus'),self.snapshot()[0],sample.get('layout'))==context
                    evidence=tool if same and examined==context else None
                    if not tool_state and isinstance(tool,dict):self.push_type_tool(tool)
                    if sample.get('root'):
                        protected=protected and not (isinstance(evidence,dict) and evidence.get('isModal') is False)
                        sample['state']='input' if protected else self.focus_state(sample,evidence)
                        if blocked:sample['state']='unknown'
                        sample=self.ime.read(sample)
                        self.canvas_focus=sample.get('focus') or 0
                        self.sync_focus_policy(sample,old_context)
                        self.canvas_tick(sample,tool_state)
                    elif self.policy.owned or self.policy.pending:
                        sample=self.ime.observe_owned(self.policy.pending or self.policy.owned)
                    revision=(scope,sample.get('ime'),sample.get('context_nonce'),sample.get('context_revision'))
                    if revision_before is not None and revision[:3]==revision_before[:3] and revision[3]!=revision_before[3]:
                        examined=None;tool=None
                        if scope and sample.get('state')!='outside':sample['state']='unknown'
                    revision_before=revision
                    guard=getattr(self.desktop,'input_focus',None)
                    focus=guard.shortcut_context() if guard else {}
                    with self.lock:verification=self.verification
                    verified_key=bool(verification and serial==self.snapshot()[0] and serial>=verification[2] and
                                      focus.get('state')=='non-input' and verification[0].get('input_epoch')==focus.get('epoch') and
                                      verification[0].get('root')==sample.get('root')==focus.get('root') and
                                      verification[0].get('hwnd')==sample.get('focus')==focus.get('focus') and
                                      self.tick()<=verification[0]['expires_tick_ms'])
                    if (focus.get('state')=='input' and focus.get('root')==sample.get('root') and focus.get('focus')==sample.get('focus')):
                        sample['state']='input'
                    menu_held=bool(self.policy.owned and not self.policy.pending and sample.get('native_menu_held') and
                                   self.desktop.settings['enabled'] and not self.busy() and not old_context and
                                   sample.get('ime_open')==0 and sample.get('state')!='input' and focus.get('state')!='input')
                    retained=bool(self.policy.owned and not self.policy.pending and sample.get('ime_open')==0 and
                                  not mouse and not sample.get('blocked') and not self.busy() and self.desktop.settings['enabled'] and not old_context and
                                  sample.get('state')=='unknown' and focus.get('state')=='non-input' and
                                  focus.get('root')==sample.get('root') and focus.get('focus')==sample.get('focus'))
                    if menu_held or retained:sample['state']='non-input'
                    if (self.policy.owned and not self.policy.pending and sample.get('identity')==self.policy.owned['identity'] and
                            sample.get('layout')!=self.policy.owned['layout'] and sample.get('ime_open')==1 and
                            (self.context_schema==1 or sample.get('native_restored'))):
                        self.retire(path,'confirmed-keyboard-layout-change')
                    if self.policy.owned and not self.policy.pending and sample.get('identity')==self.policy.owned['identity'] and sample.get('ime') and sample['ime']!=self.policy.owned['ime']:
                        self.retire(path,'original-ime-window-ended')
                    eligible=same and not blocked and sample.get('state')=='non-input' and sample.get('ime') is not None and (verified_key or time.monotonic()>=retry_after)
                    now=time.monotonic()
                    if blocked or sample.get('state') in ('outside','input'):recovering=False;stable=None
                    elif sample.get('state')=='unknown':
                        if not recovering:recovering=True;recovery_after=now+1
                        stable=None
                    if recovering and eligible and not self.policy.pending and not self.policy.owned:
                        candidate=(context,row.get(30003),row.get(30020),revision,tool)
                        if candidate!=stable:stable=candidate;stable_at=now
                        edit_ready=verified_key or now<self.edit_refresh_until and isinstance(tool,dict) and tool.get('isModal') is False
                        eligible=edit_ready or now>=recovery_after and now-stable_at>=.2
                        if eligible:
                            episode+=1;recovering=False;stable=None
                            self.report['recovery_revalidations']=self.report.get('recovery_revalidations',0)+1
                    elif recovering and not eligible:stable=None
                    token=(scope,sample.get('focus'),row.get(30003),epoch,episode,serial) if eligible else None
                    request=None if self.focus_policy else self.policy.step(sample,token,time.monotonic())
                    if (not self.focus_policy and self.policy.owned and not self.policy.pending and (not blocked or menu_held or retained)
                            and callable(getattr(self.ime,'renew',None))):
                        self.ime.renew(self.policy.owned,sample)
                    if request:
                        if request['disable'] and needs_tool and self.read_tool(sample)!=tool:
                            self.policy.pending=None;self.policy.attempted=None;examined=None;continue
                        fresh=self.ime.read(self.sample()) if request['disable'] else self.ime.observe_owned(request)
                        if verified_key and self.tick()>verification[0]['expires_tick_ms']:verified_key=False
                        valid=(ime_target(fresh)==ime_target(request) and fresh.get('focus')==request['focus'] and fresh.get('ime_open')==sample.get('ime_open'))
                        if request['disable']:
                            held=any(self.user.GetAsyncKeyState(k)&0x8000 for k in range(1,256))
                            if verification:self.report['first_key_gate']={'verified':verified_key,'keys_held':held,'eligible':eligible}
                            valid=bool(valid and eligible and self.snapshot()[0]==serial and not self.desktop.visible and not self.busy() and
                                       fresh.get('context_revision')==sample.get('context_revision') and
                                       not fresh.get('blocked') and self.focus_state(fresh,tool)=='non-input' and
                                       (verified_key or not held))
                        if valid:
                            atomic_json(path,self.saved());persisted=json.loads(json.dumps(self.saved()))
                            acknowledged=self.ime.send(request)
                            if self.context_schema==2 and acknowledged:
                                if acknowledged.get('rejected'):
                                    key=hashlib.sha256(json.dumps(request,sort_keys=True).encode()).hexdigest()
                                    atomic_json(path.parent/('input-method-rejected-'+key+'.json'),{'request':request,'native_reply':'not-armed','marked_success':False})
                                    self.policy.pending=None;self.policy.state='request-rejected'
                                    # A proved rejection changed nothing; recheck fresh evidence after readiness settles.
                                    self.policy.attempted=None;examined=None;tool=None;retry_after=time.monotonic()+1
                                else:
                                    self.policy.step(acknowledged,token,time.monotonic())
                                    sample.update(ime_open=acknowledged['ime_open'],tsf_disabled=acknowledged['tsf_disabled'])
                            key='disable_requests' if request['disable'] else 'restore_requests';self.report[key]+=1
                        else:
                            self.policy.pending=None
                            if request['disable']:self.policy.attempted=None
                    self.report.update(state=self.policy.state,pending=bool(self.policy.pending),owned=bool(self.policy.owned),
                        confirmed_disables=self.policy.confirmed_disables,confirmed_restores=self.policy.confirmed_restores,
                        ime_open=sample.get('ime_open'),keyboard_layout=sample.get('layout'),focus_class=sample.get('class'),
                        tsf_disabled=sample.get('tsf_disabled'),
                        native_events=sample.get('native_events'),context_revision=sample.get('context_revision'),
                        own_menu_held=menu_held,focus_policy=self.focus_policy if self.focus_policy else bool(sample.get('focus_policy')),
                        policy_windows=sample.get('policy_windows',0),policy_reason=sample.get('policy_reason',0),
                        policy_residual=bool(sample.get('policy_residual')),policy_age_ms=sample.get('policy_age_ms'),
                        policy_failures=self.policy_failures,
                        tool=tool.get('classID') if isinstance(tool,dict) else None,tool_modal=tool.get('isModal') if isinstance(tool,dict) else None)
                    saved=self.saved()
                    if saved!=persisted:atomic_json(path,saved);persisted=json.loads(json.dumps(saved))
                    self.complete_verification(sample,serial)
                    if old_context and not self.policy.owned and not self.policy.pending:
                        self.ime=NativeImeContext(self.user);self.context_schema=2;old_context=False;examined=None
                        self.report['mechanism']='ps-thread-context-association'
                    self.report.pop('error_type',None);self.report.pop('native_error_code',None)
                except Exception as error:
                    self.report.update(state='unknown',error_type=type(error).__name__,native_error_code=getattr(error,'winerror',None))
                    with self.lock:self.ready={}
                    self.complete_verification({})
        finally:
            if self.ime and self.focus_policy:
                until=time.monotonic()+1.5
                while self.focus_policy and time.monotonic()<until:
                    if self.stop_focus_policy():break
                    time.sleep(.05)
                self.report['focus_policy_shutdown']=not self.focus_policy
                if self.focus_policy:self.report['policy_error']='shutdown-restore-unconfirmed'
            if self.ime and (self.policy.owned or self.policy.pending):
                try:
                    sample=self.ime.observe_owned(self.policy.pending or self.policy.owned)
                    request=self.policy.step(sample,None,time.monotonic())
                    if request and not request['disable']:
                        fresh=self.ime.observe_owned(request)
                        if ime_target(fresh)==ime_target(request) and fresh.get('ime_open')==sample.get('ime_open'):
                            atomic_json(path,self.saved());acknowledged=self.ime.send(request)
                            if self.context_schema==2 and acknowledged:self.policy.step(acknowledged,None,time.monotonic())
                    until=time.monotonic()+.6
                    while self.policy.pending and time.monotonic()<until:
                        self.policy.step(self.ime.observe_owned(self.policy.pending),None,time.monotonic());time.sleep(.02)
                    atomic_json(path,self.saved())
                except Exception:self.report['state']='shutdown-restore-unconfirmed'
            if self.ime and callable(getattr(self.ime,'close',None)):self.ime.close()
            self.accessibility=None;pythoncom.CoUninitialize()
