"""Read-only text-input guard, independent of IME ownership and recovery."""
import ctypes
import time
from .input_language import InputLanguage
from .input_accessibility import FocusAccessibility,control_state,CONTAINERS


class InputFocus(InputLanguage):
    def __init__(self,desktop):
        super().__init__(desktop)
        self.native_epoch=0
        self.current={'state':'unknown','root':0,'focus':0,'epoch':0}
        self.thread.name='photoshop-input-focus'
        self.refresh_serial=0
        self.menu_origin=None
        self.verification=None
        self.report={'state':'unknown','tool_reads':0}

    def shortcut_context(self):
        with self.lock:return dict(self.current)

    def verify(self,request,reply):
        if (not isinstance(request,dict) or any(type(request.get(key)) is not int for key in ('verify_id','root','hwnd','input_epoch','expires_tick_ms')) or
            request['verify_id']<=0 or request['root']<=0 or request['hwnd']<=0 or request['input_epoch']<0 or type(request.get('need_ime')) is not bool):
            return
        with self.lock:
            self.refresh_serial+=1
            self.verification=(dict(request),reply,self.refresh_serial)
            self.current={'state':'unknown','root':0,'focus':0,'epoch':self.native_epoch}
            self.report.update(self.current)
        self.wake.set()

    def complete_verification(self,state,sample,serial,refresh=None):
        with self.lock:
            pending=self.verification
            if not pending:return
            request,reply,required_refresh=pending
            if state!='unknown' and (refresh is None or refresh<required_refresh):return
            self.verification=None
            valid=(serial==self.serial and request.get('input_epoch')==self.native_epoch and
                   request.get('root')==sample.get('root') and request.get('hwnd')==sample.get('focus'))
        now=self.tick()
        reason='scope-changed' if not valid else 'expired' if now>request['expires_tick_ms'] else 'state-unknown' if state=='unknown' else state
        self.report['last_guard_verification']={'id':request['verify_id'],'reason':reason,'elapsed_ms':max(0,now-request['expires_tick_ms']+200)}
        if not valid or state=='unknown' or now>request['expires_tick_ms']:reply(request,0);return
        reply(request,2 if state=='non-input' else 1)

    def native_sync(self):
        # The keyboard hook no longer waits for a verdict, so a changed verdict
        # must reach it immediately instead of at the 100 ms monitor tick.
        sync=getattr(self.desktop,'sync_native',None)
        if callable(sync):
            try:sync()
            except Exception:pass

    def invalidate(self,epoch=None):
        with self.lock:
            if type(epoch) is int:self.native_epoch=epoch
            self.serial+=1
            self.intent=None
            self.menu_origin=None
            self.current={'state':'unknown','root':0,'focus':0,'epoch':self.native_epoch}
            self.report.update(self.current)
        self.wake.set()
        self.native_sync()

    def protect(self,epoch=None):self.invalidate(epoch)
    def activity(self,epoch=None):self.invalidate(epoch)
    def edit_end(self,epoch=None):self.invalidate(epoch)
    def reconcile_epoch(self,epoch):
        if type(epoch) is int and epoch!=self.native_epoch:self.invalidate(epoch)

    def click(self,hwnd,root,epoch=None):
        super().click(hwnd,root)
        with self.lock:
            if type(epoch) is int:self.native_epoch=epoch
            self.menu_origin=None
            self.current={'state':'unknown','root':0,'focus':0,'epoch':self.native_epoch}
        self.native_sync()

    def refresh(self):
        with self.lock:self.refresh_serial+=1
        self.wake.set()

    def menu_open(self,mode):
        with self.lock:
            self.menu_origin=(dict(self.current),self.serial) if mode=='search' and self.current['state']=='non-input' else None

    def native_sample(self):return super().sample(include_accessibility=False)

    def menu_return(self,sample,serial):
        with self.lock:
            origin=self.menu_origin
            return bool(origin and origin[1]==serial==self.serial and sample.get('state')!='input' and not sample.get('blocked') and
                        origin[0]['root']==sample.get('root') and origin[0]['focus']==sample.get('focus') and
                        origin[0]['epoch']==self.native_epoch)

    def focus_changed(self,hwnd,epoch=None):
        self.reconcile_epoch(epoch)
        if not self.desktop._is_photoshop(hwnd) and not self.desktop._is_menu_window(hwnd):self.invalidate()
        else:
            try:
                serial=self.snapshot()[0];sample=self.native_sample()
                with self.lock:origin=self.menu_origin
                if self.menu_return(sample,serial):
                    with self.lock:
                        if self.serial==serial and self.menu_origin==origin:
                            self.serial+=1;serial=self.serial
                            self.menu_origin=(self.menu_origin[0],serial)
                        else:serial=None
                    if serial is not None:self.publish('non-input',sample,serial)
            except Exception:pass  # A failed native sample leaves the normal worker to recheck.
            self.refresh()

    def publish(self,state,sample,serial):
        with self.lock:
            if serial!=self.serial:return
            value={'state':state,'root':sample.get('root',0),'focus':sample.get('focus',0),'epoch':self.native_epoch}
            changed=self.current!=value
            self.current=value
            self.report.update(value)
        if changed:self.native_sync()

    def run(self):
        import pythoncom,win32gui
        from ctypes import wintypes as W
        pythoncom.CoInitializeEx(pythoncom.COINIT_MULTITHREADED)
        self.user=ctypes.WinDLL('user32',use_last_error=True)
        self.user.GetKeyboardLayout.argtypes=[W.DWORD];self.user.GetKeyboardLayout.restype=W.HANDLE
        self.tick=ctypes.WinDLL('kernel32',use_last_error=True).GetTickCount64
        self.tick.restype=ctypes.c_ulonglong
        context=None;tool=None;examined=None;refreshed=-1;tool_event=None
        try:
            try:self.accessibility=FocusAccessibility()
            except Exception as error:
                self.report['uia_error_type']=type(error).__name__
                import logging
                logging.exception('Shortcut input observer initialization failed')
            client_refresh=-1;next_uia_check=time.monotonic()+1
            while not self.desktop.stop.is_set():
                self.wake.wait(.05);self.wake.clear()
                if self.desktop.stop.is_set():break
                try:
                    if not getattr(self.desktop,'settings',{'enabled':True}).get('enabled',False):
                        self.publish('unknown',{},self.snapshot()[0]);context=None;tool=None;examined=None
                        continue
                    # The menu has its own input context; retain the PS guard for return.
                    if self.desktop._is_menu_window(win32gui.GetForegroundWindow()):continue
                    with self.lock:serial=self.serial;refresh=self.refresh_serial
                    if 'uia_error_type' in self.report and (refresh!=client_refresh or time.monotonic()>=next_uia_check):
                        client_refresh=refresh
                        next_uia_check=time.monotonic()+1
                        try:
                            self.accessibility=FocusAccessibility()
                            self.report.pop('uia_error_type',None)
                        except Exception as error:self.report['uia_error_type']=type(error).__name__
                    sample=self.sample()
                    self.report['observation']={**{k:sample[k] for k in ('state','root','focus','pid','class','layout','workspace','blocked','foreign_focus') if k in sample},
                                                'accessibility':dict(sample.get('accessibility',{})),
                                                'observed_at_ms':round(time.time()*1000)}
                    self.report['uia_available']=self.accessibility is not None
                    if sample.get('foreign_focus') and self.desktop._is_menu_window(sample['foreign_focus']):continue
                    event_reader=getattr(self.desktop,'input_tool_event',None)
                    event=event_reader() if sample.get('root') and event_reader else None
                    if event and event!=tool_event:
                        tool_event=event
                        self.protect() if event[2]=='enter' else self.edit_end()
                        self.report['tool_events']=self.report.get('tool_events',0)+1
                        continue
                    key=(tuple(sample.get('identity',[])),sample.get('focus'),sample.get('layout'),serial)
                    if key!=context:context=key;tool=None;examined=None
                    row=sample.get('accessibility',{})
                    need_tool=(sample.get('workspace') and sample.get('state')=='unknown' and
                               control_state(row,sample.get('pid'))!='input' and
                               (row.get(30003) in CONTAINERS or self.accessibility is None and self.snapshot()[3] is not None))
                    reconcile=getattr(self.desktop,'input_language_reconcile',None)
                    if need_tool and key==examined and tool is None and not self.busy() and not sample.get('blocked') and reconcile and reconcile():examined=None
                    if need_tool and (key!=examined or refresh!=refreshed) and not self.busy() and not sample.get('blocked'):
                        examined=key;refreshed=refresh
                        self.report['tool_reads']+=1
                        try:
                            tool=self.desktop.input_language_tool(identity=sample['identity'])
                            self.report.pop('tool_error_type',None)
                        except (RuntimeError,OSError,ValueError) as error:
                            tool=None;self.report['tool_error_type']=type(error).__name__
                        sample=self.sample()
                        if (tuple(sample.get('identity',[])),sample.get('focus'),sample.get('layout'),self.snapshot()[0])!=key:continue
                    if not need_tool:tool=None;examined=None
                    state=self.focus_state(sample,tool) if sample.get('root') else 'unknown'
                    if sample.get('blocked') or self.user.GetAsyncKeyState(1)&0x8000:state='unknown'
                    fresh=self.native_sample()
                    if (tuple(fresh.get('identity',[])),fresh.get('focus'),fresh.get('layout'))!=key[:3]:continue
                    if state=='input':
                        with self.lock:self.menu_origin=None
                    elif state=='unknown' and self.menu_return(fresh,serial):state='non-input'
                    with self.lock:
                        if self.verification and refresh<self.verification[2]:continue
                    self.publish(state,sample,serial)
                    self.complete_verification(state,sample,serial,refresh)
                    self.report.update(tool=tool.get('classID') if isinstance(tool,dict) else None,
                                       tool_modal=tool.get('isModal') if isinstance(tool,dict) else None)
                    self.report.pop('error_type',None)
                except Exception as error:
                    try:fresh=self.native_sample()
                    except Exception:fresh={}
                    self.publish('non-input' if self.menu_return(fresh,self.snapshot()[0]) else 'unknown',fresh,self.snapshot()[0])
                    self.report['error_type']=type(error).__name__
                    self.complete_verification('unknown',fresh,self.snapshot()[0])
                    examined=None
        finally:
            self.invalidate();self.accessibility=None;pythoncom.CoUninitialize()
