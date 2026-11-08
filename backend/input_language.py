"""PS-only input language transitions; no work runs in keyboard callbacks."""
import ctypes
from ctypes import wintypes as W
import threading
import time
import math
import os
from .input_accessibility import control_state,CONTAINERS


def process_exited(identity):
    import win32api,win32process,win32con
    try:handle=win32api.OpenProcess(0x1000,False,identity[1])
    except Exception as error:
        if getattr(error,'winerror',None)!=87:return False
        try:return identity[1] not in win32process.EnumProcesses()
        except Exception:return False
    try:
        return (str(win32process.GetProcessTimes(handle)['CreationTime'])!=identity[2] or
                win32process.GetExitCodeProcess(handle)!=win32con.STILL_ACTIVE)
    except Exception:return False
    finally:handle.Close()


def read_layout_journal(journal,installed):
    import json
    if not journal.exists():return {'owned':None,'pending':None}
    if journal.stat().st_size>16384:raise ValueError('size')
    saved=json.loads(journal.read_text('utf-8'))
    if set(saved)!={'owned','pending'}:raise ValueError('fields')
    for row in saved.values():
        if row is None:continue
        if (set(row)!={'root','identity','focus','wanted','original','switch','time'} or
            any(type(row[k]) is not int or row[k]<=0 for k in ['root','focus','wanted','original']) or
            row['wanted'] not in installed or row['original'] not in installed or type(row['switch']) is not bool or
            not isinstance(row['time'],(int,float)) or not math.isfinite(row['time']) or
            not isinstance(row['identity'],list) or len(row['identity'])!=4 or row['identity'][0]!=row['root'] or
            any(type(row['identity'][i]) is not int or row['identity'][i]<=0 for i in [0,1,3]) or
            not isinstance(row['identity'][2],str) or not row['identity'][2]):raise ValueError('record')
    if saved['owned'] and not saved['owned']['switch']:raise ValueError('unconfirmed ownership')
    return saved


class LanguagePolicy:
    def __init__(self):
        self.owned=None
        self.pending=None
        self.attempted=None
        self.restore_attempt=None
        self.restore_input=False
        self.confirmed_switches=0;self.confirmed_restores=0
        self.state='unknown'

    def step(self, sample, intent, english, now):
        self.state=sample.get('state','unknown')
        if not sample.get('root'):return None
        root,layout=sample['root'],sample['layout']
        if self.pending:
            request=self.pending
            if sample.get('identity')!=request.get('identity'):return None
            if layout==request['wanted']:
                self.owned=request if request['switch'] else None
                if request['switch']:self.confirmed_switches+=1
                else:
                    self.confirmed_restores+=1
                    if self.restore_input:self.attempted=None
                self.pending=None
            elif now-request['time']>1:
                self.state='request-not-confirmed';return None
            else:return None
        if self.owned:
            if sample.get('identity')!=self.owned.get('identity'):return None
            if layout!=self.owned['wanted']:
                self.owned=None;self.state='manual-change';return None
            if self.state!='non-input':
                token=(sample.get('identity'),sample['focus'],layout)
                if token==self.restore_attempt:return None
                self.restore_attempt=token
                return self.request(sample,self.owned['original'],False,now)
        elif (self.state=='non-input' and intent and intent!=self.attempted and english and layout!=english):
            self.attempted=intent
            return self.request(sample,english,True,now)
        return None

    def request(self,sample,wanted,switch,now):
        if switch:self.restore_attempt=None
        else:self.restore_input=sample.get('state')=='input'
        original=sample['layout'] if switch else self.owned['original']
        self.pending={'root':sample['root'],'identity':sample.get('identity'),'focus':sample['focus'],'wanted':wanted,'original':original,'switch':switch,'time':now}
        return dict(self.pending)


class InputLanguage:
    def __init__(self,desktop):
        # Import COM libraries before starting the dedicated MTA UIA worker.
        if os.name=='nt':
            import pythoncom,comtypes.client
        self.desktop=desktop;self.lock=threading.Lock();self.intent=None;self.serial=0
        self.epoch=0;self.protected=False;self.accessibility=None
        self.wake=threading.Event()
        self.policy=LanguagePolicy();self.report={'state':'unknown','switch_requests':0,'restore_requests':0}
        self.thread=threading.Thread(target=self.run,name='photoshop-input-language',daemon=True)

    def click(self,hwnd,root):
        with self.lock:
            self.serial+=1;self.epoch+=1;self.protected=False;self.intent=(self.serial,hwnd,root)
        self.wake.set()

    def protect(self):
        with self.lock:self.serial+=1;self.epoch+=1;self.protected=True;self.intent=None
        self.wake.set()

    def activity(self):
        with self.lock:self.serial+=1;self.protected=False
        self.wake.set()

    def snapshot(self):
        with self.lock:return self.serial,self.epoch,self.protected,self.intent

    def focus_state(self,sample,tool):
        if sample.get('state')=='input':return 'input'
        state=control_state(sample.get('accessibility',{}),sample.get('pid'),sample.get('workspace',False),tool)
        if state=='unknown' and self.accessibility is None:
            # Preserve the previously verified canvas-only path if UIA is unavailable.
            intent=self.snapshot()[3];root=sample.get('root')
            if (intent and root==intent[2] and sample.get('class') in ('OWL.Document','PSViewC') and
                self.desktop.panel_context.describe(sample['focus'],root)=='canvas' and
                self.desktop.panel_context.describe(intent[1],root)=='canvas'):
                state=control_state({30002:sample.get('pid'),30003:50033,30008:True,30010:True,30022:False,
                                     30033:False,30040:False,30043:False},sample.get('pid'),True,tool)
        return state

    def busy(self):
        check=getattr(self.desktop,'input_language_busy',None)
        return bool(check and check())

    def retire_exited_owner(self):
        import hashlib,json
        from .settings import atomic_json
        owned=self.policy.owned
        if not owned or self.policy.pending or not process_exited(owned['identity']):return
        # Archive confirmed ownership before releasing it; unknown requests stay pinned.
        key=hashlib.sha256(json.dumps(owned,sort_keys=True).encode()).hexdigest()
        atomic_json(self.desktop.runtime/('input-language-retired-'+key+'.json'),
                    {'owned':owned,'reason':'original-process-exited','restore_sent':False})
        atomic_json(self.desktop.runtime/'input-language-owned.json',{'owned':None,'pending':None})
        self.policy.owned=None;self.policy.attempted=None;self.policy.restore_attempt=None
        self.report['retired_exited_owners']=self.report.get('retired_exited_owners',0)+1

    def foreground_identity(self,foreground):
        """Reuse the verified foreground process while its HWND/PID is unchanged.

        The held handle proves the same process is alive (a recycled PID closes
        the old object). Only a foreground change reopens/queries the process.
        """
        import win32gui,win32process,win32api
        thread,pid=win32process.GetWindowThreadProcessId(foreground)
        cached=getattr(self,'_foreground_identity',None)
        # A live window cannot outlive its process, and the held handle keeps
        # that process object (and its PID) from being recycled. Same HWND,
        # thread and PID therefore reuse the earlier executable verdict.
        if cached and cached['hwnd']==foreground and cached['thread']==thread and cached['pid']==pid:
            return cached
        if cached and cached.get('handle'):
            try:cached['handle'].Close()
            except Exception:pass
        self._foreground_identity=None
        if not pid:return None
        handle=win32api.OpenProcess(0x1000,False,pid)
        if not handle:return None
        try:
            kernel=ctypes.WinDLL('kernel32',use_last_error=True)
            kernel.QueryFullProcessImageNameW.argtypes=[W.HANDLE,W.DWORD,W.LPWSTR,ctypes.POINTER(W.DWORD)]
            kernel.QueryFullProcessImageNameW.restype=W.BOOL
            image=ctypes.create_unicode_buffer(32768);length=W.DWORD(len(image))
            if not kernel.QueryFullProcessImageNameW(int(handle),0,image,ctypes.byref(length)):
                raise ctypes.WinError(ctypes.get_last_error())
            name=image.value.rsplit('\\',1)[-1].casefold()
            created=str(win32process.GetProcessTimes(handle)['CreationTime'])
        except OSError:
            handle.Close();raise
        except Exception:
            handle.Close();return None
        cached={'hwnd':foreground,'thread':thread,'pid':pid,'handle':handle,'created':created,
                'photoshop':name=='photoshop.exe'}
        self._foreground_identity=cached
        return cached

    def sample(self,include_accessibility=True):
        import win32gui,win32process
        from .photoshop_windows import workspace_root
        gui=win32gui;foreground=gui.GetForegroundWindow()
        if not foreground or not gui.IsWindow(foreground) or not gui.IsWindowVisible(foreground) or gui.IsIconic(foreground):return {'state':'outside'}
        identity=self.foreground_identity(foreground)
        if not identity or not identity['photoshop']:return {'state':'outside'}
        thread,pid,created=identity['thread'],identity['pid'],identity['created']
        workspace=workspace_root(gui,foreground)
        root=workspace or gui.GetAncestor(foreground,3) or foreground
        if win32process.GetWindowThreadProcessId(root)[1]!=pid:return {'state':'unknown'}
        class Info(ctypes.Structure):
            _fields_=[('cbSize',W.DWORD),('flags',W.DWORD),('active',W.HWND),('focus',W.HWND),
                      ('capture',W.HWND),('menu',W.HWND),('move',W.HWND),('caret',W.HWND),('rect',W.RECT)]
        info=Info(cbSize=ctypes.sizeof(Info))
        if not self.user.GetGUIThreadInfo(thread,ctypes.byref(info)):return {'state':'unknown'}
        focus=info.focus or 0;layout=self.user.GetKeyboardLayout(thread)
        if not focus or not layout:return {'state':'unknown'}
        focus_thread,focus_pid=win32process.GetWindowThreadProcessId(focus)
        if focus_thread!=thread or focus_pid!=pid or (gui.GetAncestor(focus,3) or focus)!=root:return {'state':'unknown','foreign_focus':focus}
        name=gui.GetClassName(focus) if focus and gui.IsWindow(focus) else ''
        state='input' if info.caret or name in ('Edit','RichEdit20W','RichEdit50W','ComboBox') else 'unknown'
        row={}
        input_panel=self.desktop.visible and (getattr(self.desktop,'color_editor_active',False) or getattr(self.desktop,'menu_tools_active',False))
        if include_accessibility and state=='unknown' and self.accessibility is not None and not self.busy() and not input_panel:
            try:
                row=self.accessibility.read()
                self.report.pop('uia_error_type',None)
                native=row.get(30020)
                if type(native) is int and native<0:native&=0xffffffff
                if type(native) is not int or native and ((gui.GetAncestor(native,3) or native)!=root):row={}
            except Exception as error:self.report['uia_error_type']=type(error).__name__
        if gui.GetForegroundWindow()!=foreground:return {'state':'outside'}
        return {'root':root,'identity':[root,pid,created,thread],'focus':focus,'layout':layout,'state':state,
                'pid':pid,'workspace':bool(workspace),'accessibility':row,
                'blocked':bool(info.capture or info.menu or info.move or info.flags&30),'class':name}

    def run(self):
        import pythoncom,win32com.client,json
        from .settings import atomic_json
        pythoncom.CoInitializeEx(pythoncom.COINIT_MULTITHREADED)
        self.user=ctypes.WinDLL('user32',use_last_error=True)
        self.user.GetKeyboardLayout.argtypes=[W.DWORD];self.user.GetKeyboardLayout.restype=W.HANDLE
        self.user.GetKeyboardLayoutList.argtypes=[ctypes.c_int,ctypes.POINTER(W.HANDLE)]
        self.user.PostMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM]
        try:
            journal=self.desktop.runtime/'input-language-owned.json'
            layouts=(W.HANDLE*64)();count=self.user.GetKeyboardLayoutList(64,layouts)
            installed=list(layouts)[:count]
            if journal.exists():
                try:
                    saved=read_layout_journal(journal,installed)
                    self.policy.owned=saved['owned'];self.policy.pending=saved['pending']
                    if self.policy.pending:self.policy.pending['time']=time.monotonic()-2
                except Exception:
                    self.report['state']='journal-invalid';return
            try:self.retire_exited_owner()
            except Exception as error:
                self.report.update(state='owner-retirement-unconfirmed',error_type=type(error).__name__);return
            persisted=None
            english=next((v for v in list(layouts)[:count] if v and (v&0xffff)==0x409),None)
            try:
                from .input_accessibility import FocusAccessibility
                self.accessibility=FocusAccessibility()
            except Exception as error:self.report['uia_error_type']=type(error).__name__
            self.report.update(uia_available=self.accessibility is not None,tool_reads=0,scope='photoshop-non-input')
            examined=None;tool=None;previous_scope=None;episode=0
            def read_tool():
                self.report['tool_reads']+=1
                try:
                    reader=getattr(self.desktop,'input_language_tool',None)
                    if reader:return reader()
                    return str(win32com.client.GetActiveObject('Photoshop.Application').DoJavaScript('String(app.currentTool);'))
                except Exception as error:
                    self.report['tool_error_type']=type(error).__name__
                    return None
            while not self.desktop.stop.is_set():
                self.wake.wait(.05);self.wake.clear()
                if self.desktop.stop.is_set():break
                try:
                    serial,epoch,protected,clicked=self.snapshot()
                    sample=self.sample()
                    if sample.get('root') and self.policy.owned and sample.get('identity')!=self.policy.owned['identity']:
                        self.retire_exited_owner()
                    scope=tuple(sample.get('identity',[])) if sample.get('root') else None
                    if scope!=previous_scope:
                        episode+=1;previous_scope=scope;examined=None;tool=None
                    mouse_down=bool(self.user.GetAsyncKeyState(1)&0x8000)
                    if (mouse_down or sample.get('blocked') or self.desktop.visible or self.busy() or
                        not self.desktop.settings['enabled']):
                        examined=None;tool=None
                    row=sample.get('accessibility',{})
                    context=(scope,sample.get('focus'),serial)
                    needs_tool=(sample.get('workspace') and sample.get('state')=='unknown' and
                                control_state(row,sample.get('pid'))!='input' and
                                (row.get(30003) in CONTAINERS or protected or self.accessibility is None and clicked))
                    if needs_tool and context!=examined and not mouse_down and not sample.get('blocked'):
                        if self.desktop.settings['enabled'] and not self.desktop.visible and not self.busy():
                            examined=context;tool=None
                            tool=read_tool()
                        sample=self.sample()
                    same_context=(tuple(sample.get('identity',[])),sample.get('focus'),self.snapshot()[0])==context
                    if sample.get('root'):
                        tool_evidence=tool if same_context and examined==context else None
                        protected=protected and not (isinstance(tool_evidence,dict) and tool_evidence.get('isModal') is False)
                        sample['state']='input' if protected else self.focus_state(sample,tool_evidence)
                        if not self.desktop.settings['enabled']:sample['state']='unknown'
                    eligible=(same_context and self.desktop.settings['enabled'] and not self.busy() and not mouse_down and
                              not self.desktop.visible and sample['state']=='non-input' and not sample.get('blocked'))
                    row=sample.get('accessibility',{})
                    token=(scope,sample.get('focus'),row.get(30003),row.get(30020),
                           tool if row.get(30003) in CONTAINERS else None,epoch,episode,serial) if eligible else None
                    request=self.policy.step(sample,token,english,time.monotonic())
                    if request:
                        if request['switch'] and needs_tool:
                            if self.busy() or read_tool()!=tool:
                                self.policy.pending=None;self.policy.attempted=None;examined=None;continue
                        fresh=self.sample()
                        unchanged=self.snapshot()[0]==serial
                        valid=(fresh.get('identity')==request['identity'] and fresh.get('focus')==request['focus'] and fresh.get('layout')==sample['layout'] and
                               (not request['switch'] or unchanged and eligible and self.desktop.settings['enabled'] and not self.desktop.visible and
                                not self.busy() and not self.user.GetAsyncKeyState(1)&0x8000 and not fresh.get('blocked') and
                                self.focus_state(fresh,tool)=='non-input'))
                        if valid:
                            atomic_json(journal,{'owned':self.policy.owned,'pending':self.policy.pending})
                        if valid and self.user.PostMessageW(request['focus'],0x50,0,request['wanted']):
                            key='switch_requests' if request['switch'] else 'restore_requests';self.report[key]+=1
                        else:
                            self.policy.pending=None
                            if not valid:
                                if request['switch']:self.policy.attempted=None
                                else:self.policy.restore_attempt=None
                    self.report.update(state=self.policy.state,pending=bool(self.policy.pending),owned=bool(self.policy.owned),english_available=bool(english),confirmed_switches=self.policy.confirmed_switches,confirmed_restores=self.policy.confirmed_restores,focus_class=sample.get('class'),focus_control_type=row.get(30003),tool=tool.get('classID') if isinstance(tool,dict) else tool,tool_modal=tool.get('isModal') if isinstance(tool,dict) else None)
                    saved={'owned':self.policy.owned,'pending':self.policy.pending}
                    if saved!=persisted:
                        atomic_json(journal,saved);persisted=json.loads(json.dumps(saved))
                except Exception as error:
                    self.report.update(state='unknown',error_type=type(error).__name__)
        finally:
            # Restore only our confirmed change, still on the same foreground PS thread.
            if self.policy.owned or self.policy.pending:
                try:
                    sample=self.sample();sample['state']='unknown'
                    request=self.policy.step(sample,None,None,time.monotonic())
                    if request and not request['switch']:
                        fresh=self.sample()
                        if fresh.get('identity')==request['identity'] and fresh.get('focus')==request['focus'] and fresh.get('layout')==sample['layout']:
                            atomic_json(journal,{'owned':self.policy.owned,'pending':self.policy.pending})
                            self.user.PostMessageW(request['focus'],0x50,0,request['wanted'])
                    until=time.monotonic()+.5
                    while self.policy.pending and time.monotonic()<until:
                        self.policy.step(self.sample(),None,None,time.monotonic());time.sleep(.02)
                    atomic_json(journal,{'owned':self.policy.owned,'pending':self.policy.pending})
                except Exception:self.report['state']='shutdown-restore-unconfirmed'
            self.accessibility=None
            pythoncom.CoUninitialize()
