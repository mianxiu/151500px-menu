"""PS-thread IME association lease; keyboard layouts and conversion modes stay intact."""
import ctypes
from ctypes import wintypes as W
import os
import secrets
import hashlib
from .paths import resource_root,runtime_dir


class NativeImeContext:
    def __init__(self,user):
        self.user=user;self.hooks={};self.controls={};self.library=None
        self.user.RegisterWindowMessageW.argtypes=[W.LPCWSTR];self.user.RegisterWindowMessageW.restype=W.UINT
        self.user.GetPropW.argtypes=[W.HWND,W.LPCWSTR];self.user.GetPropW.restype=W.HANDLE
        self.user.SetWindowsHookExW.argtypes=[ctypes.c_int,ctypes.c_void_p,W.HINSTANCE,W.DWORD];self.user.SetWindowsHookExW.restype=W.HANDLE
        self.user.UnhookWindowsHookEx.argtypes=[W.HANDLE]
        self.attach=self.user.RegisterWindowMessageW('PhotoshopMenu.InputContext.Attach.v1')
        self.message=self.user.RegisterWindowMessageW('PhotoshopMenu.InputContext.Control.v1')
        if not self.attach or not self.message:raise OSError('Cannot register IME context protocol')

    def control(self,sample):
        import win32gui,win32process
        root=sample['root'];thread,pid=sample['identity'][3],sample['identity'][1]
        window=self.controls.get(tuple(sample['identity']))
        if window and win32gui.IsWindow(window) and win32gui.GetClassName(window)=='PhotoshopMenu.InputContext.v1' and list(win32process.GetWindowThreadProcessId(window))==[thread,pid]:
            if sample.get('state')=='outside' or self.user.GetPropW(window,'PhotoshopMenu.InputContext.Owner.v1')==os.getpid():return window
        # The TSF service publishes the same control window on the root window,
        # so both transports are consumed through one lookup.
        published=self.user.GetPropW(root,'PhotoshopMenu.InputContext.Window.v1')
        if published and win32gui.IsWindow(published) and win32gui.GetClassName(published)=='PhotoshopMenu.InputContext.v1' \
           and list(win32process.GetWindowThreadProcessId(published))==[thread,pid] \
           and self.user.GetPropW(published,'PhotoshopMenu.InputContext.Owner.v1')==os.getpid():
            self.controls[tuple(sample['identity'])]=published;return published
        if sample.get('state')=='outside':return None
        # A machine TSF service can only be loaded when it is selected as an
        # input method, which would replace the user's IME, so this system keeps
        # the in-process transport as the working channel. If a TSF-provided
        # window ever appears the lookup above already uses it.
        # Both sent focus notifications and queued input stay in the PS UI thread.
        if self.library is None:
            content=(resource_root()/'native/InputContext.dll').read_bytes()
            folder=runtime_dir()/'input-context'/hashlib.sha256(content).hexdigest();folder.mkdir(parents=True,exist_ok=True)
            library=folder/'InputContext.dll'
            if not library.exists():
                temporary=folder/'InputContext.tmp';temporary.write_bytes(content);os.replace(temporary,library)
            if library.read_bytes()!=content:raise RuntimeError('PS输入保护组件校验失败')
            self.library=ctypes.WinDLL(str(library))
        identity=tuple(sample['identity'])
        if identity not in self.hooks:
            hook=self.user.SetWindowsHookExW(4,ctypes.cast(self.library.InputContextHook,ctypes.c_void_p),self.library._handle,thread)
            if not hook:raise ctypes.WinError()
            queued=self.user.SetWindowsHookExW(3,ctypes.cast(self.library.InputContextQueueHook,ctypes.c_void_p),self.library._handle,thread)
            if not queued:
                error=ctypes.get_last_error();self.user.UnhookWindowsHookEx(hook);raise ctypes.WinError(error)
            self.hooks[identity]=(hook,queued)
        nonce=secrets.randbelow(0x7ffffffe)+1
        win32gui.SendMessageTimeout(root,self.attach,os.getpid(),nonce,2,200)
        window=self.user.GetPropW(root,'PhotoshopMenu.InputContext.Window.v1')
        if not window or not win32gui.IsWindow(window) or win32gui.GetClassName(window)!='PhotoshopMenu.InputContext.v1' or list(win32process.GetWindowThreadProcessId(window))!=[thread,pid] or self.user.GetPropW(window,'PhotoshopMenu.InputContext.Owner.v1')!=os.getpid():raise RuntimeError('PS线程内输入保护未就绪或由另一服务持有')
        self.controls[identity]=window;return window

    def command(self,window,operation,focus,timeout=1000):
        import win32gui
        nonce=self.user.GetPropW(window,'PhotoshopMenu.InputContext.Nonce.v1')
        if not nonce:raise RuntimeError('PS输入保护身份已失效')
        return win32gui.SendMessageTimeout(window,self.message,(nonce<<8)|operation,focus,2,timeout)[1]

    def read(self,sample):
        value={**sample,'ime':None,'ime_open':None}
        if not sample.get('root'):return value
        window=self.control(sample)
        if not window:return value
        status=self.command(window,1,sample['focus'])
        if not status&1 or status&8:raise RuntimeError('PS输入上下文恢复待核对')
        tick=self.user.GetPropW(window,'PhotoshopMenu.InputContext.PolicyTick.v1') or 0
        value.update(ime=window,ime_open=int(bool(status&4)),native_restored=bool(status&16),tsf_disabled=bool(status&32),native_events=bool(status&64),
                     native_menu_held=bool(status&128),context_nonce=self.user.GetPropW(window,'PhotoshopMenu.InputContext.Nonce.v1'),
                     context_revision=self.user.GetPropW(window,'PhotoshopMenu.InputContext.Revision.v1') or 0,
                     focus_policy=bool(status&0x100),policy_residual=bool(status&0x200),
                     policy_windows=(status>>16)&0xff,policy_reason=(status>>24)&0xff,policy_tick=tick,
                     canvas_text_editing=bool(status>>32&1),type_tool_active=bool(status>>33&1),
                     type_tool_known=bool(status>>34&1),canvas_composition=bool(status>>35&1),
                     canvas_entry_pending=bool(status>>36&1))
        return value

    def set_canvas_tool(self,window,active):
        import win32gui
        nonce=self.user.GetPropW(window,'PhotoshopMenu.InputContext.Nonce.v1')
        if not nonce:raise RuntimeError('PS输入保护身份已失效')
        status=win32gui.SendMessageTimeout(window,self.message,(nonce<<8)|6,1 if active else 0,2,250)[1]
        if type(status) is not int or not status&1:raise RuntimeError('工具状态推送未确认')
        return status

    def canvas_correct(self,window,enter,stamp_ms):
        import win32gui
        nonce=self.user.GetPropW(window,'PhotoshopMenu.InputContext.Nonce.v1')
        if not nonce:raise RuntimeError('PS输入保护身份已失效')
        packed=(int(stamp_ms)<<1)|(1 if enter else 0)
        status=win32gui.SendMessageTimeout(window,self.message,(nonce<<8)|8,packed,2,250)[1]
        if type(status) is not int or not status&1:raise RuntimeError('画布文字校正未确认')
        return status

    def canvas_diagnostics(self,window,status,focus):
        import win32gui
        if type(status) is not int or not status&1:return {'available':False}
        def prop(name):
            value=self.user.GetPropW(window,name) or 0
            return int(value)
        row={'available':True,'canvas_text_editing':bool(status>>32&1),'type_tool_active':bool(status>>33&1),
             'type_tool_known':bool(status>>34&1),'composition_active':bool(status>>35&1),
             'entry_pending':bool(status>>36&1),
             'entry_tick_ms':prop('PhotoshopMenu.InputContext.Canvas.EntryTick.v1'),
             'entry_trigger':prop('PhotoshopMenu.InputContext.Canvas.EntryTrigger.v1'),
             'exit_tick_ms':prop('PhotoshopMenu.InputContext.Canvas.ExitTick.v1'),
             'exit_trigger':prop('PhotoshopMenu.InputContext.Canvas.ExitTrigger.v1'),
             'corrections':prop('PhotoshopMenu.InputContext.Canvas.Corrections.v1'),
             'epoch':prop('PhotoshopMenu.InputContext.Canvas.Epoch.v1'),
             'focus':focus}
        count=prop('PhotoshopMenu.InputContext.PolicyTrace.Count.v1')
        row['policy_trace_count']=count
        row['policy_trace']=[]
        head=count%20
        for offset in range(min(count,20)):
            index=(head+offset)%20
            packed=prop('PhotoshopMenu.InputContext.PolicyTrace.%d.v1'%index)
            if not packed:continue
            row['policy_trace'].append({'tick_ms':packed>>24,'window_kind':(packed>>20)&0xf,
                                        'reason':(packed>>8)&0xff,'action':packed&0xff})
        return row

    def set_policy(self,window,enabled):
        import win32gui
        nonce=self.user.GetPropW(window,'PhotoshopMenu.InputContext.Nonce.v1')
        if not nonce:raise RuntimeError('PS输入保护身份已失效')
        return win32gui.SendMessageTimeout(window,self.message,(nonce<<8)|5,1 if enabled else 0,2,1000)[1]

    def policy_state(self,window,status):
        """Describe the native focus-policy reply without mutating it."""
        if type(status) is not int:return {'focus_policy':False,'policy_windows':0,'policy_reason':0,'policy_residual':True}
        tick=self.user.GetPropW(window,'PhotoshopMenu.InputContext.PolicyTick.v1') or 0
        return {'focus_policy':bool(status&0x100),'policy_windows':(status>>16)&0xff,
                'policy_reason':(status>>24)&0xff,'policy_residual':bool(status&0x200),'policy_tick':tick}

    def renew(self,owned,sample):
        if (sample.get('state')=='non-input' and sample.get('identity')==owned['identity'] and sample.get('layout')==owned['layout'] and
            sample.get('ime')==owned['ime'] and sample.get('focus')==owned['focus'] and sample.get('context_nonce')==owned['context_nonce']):
            self.command(owned['ime'],4,owned['focus'])

    def send(self,request):
        if self.user.GetPropW(request['ime'],'PhotoshopMenu.InputContext.Nonce.v1')!=request['context_nonce']:raise RuntimeError('PS输入保护会话已变化；保留原记录')
        status=self.command(request['ime'],2 if request['wanted']==0 else 3,request['focus'])
        if request['wanted']==0 and status&1 and status&8 and not status&2:return {'rejected':True}
        if not status&1 or status&8:return None
        if request['wanted']==0 and status&2 and not status&4:return {**request,'ime_open':0,'tsf_disabled':bool(status&32),'state':'non-input'}
        if request['wanted']==1 and not status&2 and status&4:return {**request,'ime_open':1,'tsf_disabled':False,'state':'input' if request['input_restore'] else 'outside'}

    def observe_owned(self,owned):
        from .input_method import ImeContext
        self.controls[tuple(owned['identity'])]=owned['ime']
        return ImeContext.observe_owned(self,owned)

    def close(self):
        for hooks in self.hooks.values():
            for hook in hooks:self.user.UnhookWindowsHookEx(hook)
        self.hooks.clear()
