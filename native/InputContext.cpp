// Runs only in the verified PS UI thread. No text or document data is read.
#include <windows.h>
#include <imm.h>
#include <tlhelp32.h>
#include "InputContextTsf.h"

static const wchar_t* controlClass=L"PhotoshopMenu.InputContext.v1";
static const wchar_t* controlProperty=L"PhotoshopMenu.InputContext.Window.v1";
static const wchar_t* nonceProperty=L"PhotoshopMenu.InputContext.Nonce.v1";
static const wchar_t* ownerProperty=L"PhotoshopMenu.InputContext.Owner.v1";
static const wchar_t* revisionProperty=L"PhotoshopMenu.InputContext.Revision.v1";
static const wchar_t* policyTickProperty=L"PhotoshopMenu.InputContext.PolicyTick.v1";
static constexpr UINT contextChangedMessage=WM_APP+51;
static UINT attachMessage=0,controlMessage=0;
static HMODULE module=nullptr;
static HWND control=nullptr,root=nullptr,disabledWindow=nullptr;
static HIMC originalContext=nullptr;
static HANDLE owner=nullptr;
static DWORD ownerId=0;
static ULONG_PTR nonce=0;
static ULONGLONG deadline=0;
static bool restoreFailed=false,changing=false;
static bool restored=false;
static ULONG_PTR revision=1;
static InputContextTsf tsf;
static bool menuHandoff=false;
#ifdef INPUT_CONTEXT_SELF_TEST
static bool testTrace=false;
#endif
static HANDLE menuProcess=nullptr;
static DWORD menuPid=0;

// Canvas text editing: clicking the PS canvas with a type tool, double-clicking
// any text, or pressing Enter on a selected text layer enters text editing
// without a focus change. The queue hook restores the canvas HIMC before PS
// processes that message; Esc/Ctrl+Enter post a disable that runs after PS has
// handled the key. Background UXP evidence only corrects a missed entry/exit.
static const wchar_t* canvasEntryTickProperty=L"PhotoshopMenu.InputContext.Canvas.EntryTick.v1";
static const wchar_t* canvasEntryTriggerProperty=L"PhotoshopMenu.InputContext.Canvas.EntryTrigger.v1";
static const wchar_t* canvasExitTickProperty=L"PhotoshopMenu.InputContext.Canvas.ExitTick.v1";
static const wchar_t* canvasExitTriggerProperty=L"PhotoshopMenu.InputContext.Canvas.ExitTrigger.v1";
static const wchar_t* canvasEpochProperty=L"PhotoshopMenu.InputContext.Canvas.Epoch.v1";
static const wchar_t* canvasCorrectionsProperty=L"PhotoshopMenu.InputContext.Canvas.Corrections.v1";
static const wchar_t* policyTraceCountProperty=L"PhotoshopMenu.InputContext.PolicyTrace.Count.v1";
enum CanvasTrigger : DWORD {
    CanvasNone=0,CanvasClickText=1,CanvasDoubleClick=2,CanvasEnterText=3,
    CanvasCorrectionEnter=4,CanvasEsc=5,CanvasCtrlEnter=6,CanvasNumpadEnter=7,
    CanvasCorrectionExit=8,CanvasTimeoutExit=9,CanvasFocusLeave=10
};
static bool canvasTextEditing=false,typeToolActive=false,typeToolKnown=false;
static bool compositionActive=false,entryPending=false;
static ULONGLONG canvasEntryTick=0,canvasEntryWallMs=0,canvasExitTick=0,canvasExitWallMs=0;
static DWORD canvasEntryTrigger=CanvasNone,canvasExitTrigger=CanvasNone;
static DWORD canvasEpoch=1,canvasCorrections=0;
static constexpr int traceCapacity=20;
static ULONGLONG policyTrace[traceCapacity]{};
static unsigned policyTraceCount=0;
#ifdef INPUT_CONTEXT_SELF_TEST
static int testCtrlHeld=-1,testAltHeld=-1;
#endif

// Focus policy: the PS UI thread synchronously detaches the IME context of the
// verified non-input focus target (root window or its PSViewC canvas) in the
// WM_SETFOCUS callback. Every other focus target restores the saved association
// before its own window procedure runs. A short save list keeps the original
// HIMC per window; associations PS has replaced itself are never overwritten.
enum PolicyReason : DWORD {
    PolicyNone=0,PolicyDisableRoot=1,PolicyDisableView=2,PolicyRestoreOther=3,
    PolicyRestoreForeground=4,PolicyReapplyLanguage=5,PolicyRestoreOwner=6,
    PolicyOff=7,PolicyReapplyDrift=8,PolicyEnable=9,PolicyWindowGone=10,
    PolicyRestoreCanvasText=11,PolicyDisableCanvasExit=12,PolicyCorrectionEnter=13,
    PolicyCorrectionExit=14,PolicyCanvasFocus=15
};
enum PolicyTraceAction : DWORD {TraceDisable=1,TraceRestore=2,TraceEnable=3,TraceOff=4,TraceCorrection=5,TraceCanvasEnter=6,TraceCanvasExit=7};
struct FocusEntry { HWND window=nullptr; HIMC context=nullptr; };
static constexpr int focusCapacity=8;
static FocusEntry focusEntries[focusCapacity];
static int focusCount=0;
static bool focusPolicy=false,policyRestoreFailed=false;
static DWORD policyReason=0;
static ULONGLONG policyTick=0;

static bool ownMenu(HWND window) {
    if(!window || !owner || WaitForSingleObject(owner,0)!=WAIT_TIMEOUT)return false;
    window=GetAncestor(window,GA_ROOT);
    wchar_t name[64]{};GetClassNameW(window,name,64);
    if(wcscmp(name,L"PhotoshopMenuImGui")!=0)return false;
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(menuProcess && menuPid==pid && WaitForSingleObject(menuProcess,0)==WAIT_TIMEOUT)return true;
    if(menuProcess){CloseHandle(menuProcess);menuProcess=nullptr;menuPid=0;}
    const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)return false;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);bool valid=false;
    for(BOOL found=Process32FirstW(snapshot,&entry);found;found=Process32NextW(snapshot,&entry))
        if(entry.th32ProcessID==pid){valid=entry.th32ParentProcessID==ownerId && _wcsicmp(entry.szExeFile,L"MenuHost.exe")==0;break;}
    CloseHandle(snapshot);if(!valid)return false;
    const auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
    FILETIME created{},exited{},kernel{},user{},ownerCreated{};
    if(!process)return false;
    valid=GetProcessTimes(process,&created,&exited,&kernel,&user) && GetProcessTimes(owner,&ownerCreated,&exited,&kernel,&user) &&
        CompareFileTime(&created,&ownerCreated)>=0 && WaitForSingleObject(process,0)==WAIT_TIMEOUT;
    if(!valid){CloseHandle(process);return false;}
    menuProcess=process;menuPid=pid;return true;
}
static bool ownMenuThread(DWORD thread) {
    bool found=false;
    if(thread)EnumThreadWindows(thread,[](HWND window,LPARAM data)->BOOL{
        if(!ownMenu(window))return TRUE;
        *reinterpret_cast<bool*>(data)=true;return FALSE;
    },reinterpret_cast<LPARAM>(&found));
    return found;
}
static bool menuReturning() {
    const auto window=GetForegroundWindow(),focus=GetFocus();
    return menuHandoff && (!window || window==root) && (!focus || focus==root || focus==disabledWindow);
}
static bool menuActive() {
    const auto window=GetForegroundWindow();
    return ownMenu(window) || (window==root && ownMenu(GetFocus()));
}

static void contextChanged() {
    // Defer mutation until the TSF notification callback has returned.
    if(!changing && control && disabledWindow)PostMessageW(control,contextChangedMessage,0,0);
}

static HIMC associated(HWND window) {
    const auto context=ImmGetContext(window);
    if(context)ImmReleaseContext(window,context);
    return context;
}
static ULONGLONG wallMilliseconds() {
    FILETIME value{};
#ifdef INPUT_CONTEXT_SELF_TEST
    GetSystemTimeAsFileTime(&value);
#else
    GetSystemTimePreciseAsFileTime(&value);
#endif
    const ULONGLONG ticks=(static_cast<ULONGLONG>(value.dwHighDateTime)<<32)|value.dwLowDateTime;
    return ticks/10000ULL-11644473600000ULL;
}
static unsigned windowKind(HWND window) {
    if(!window)return 0;
    if(window==root)return 1;
    wchar_t name[64]{};GetClassNameW(window,name,64);
    if(wcscmp(name,L"PSViewC")==0)return 2;
    if(wcscmp(name,L"Edit")==0)return 3;
    if(wcscmp(name,L"PSFloatC")==0)return 4;
    return 5;
}
static void recordPolicyTrace(DWORD action,DWORD reason,HWND window) {
    const ULONGLONG tick=GetTickCount64();
    const ULONGLONG packed=((tick&0xffffffffffULL)<<24)|(ULONGLONG(windowKind(window)&0xf)<<20)|
        (ULONGLONG(reason&0xff)<<8)|(action&0xff);
    const unsigned index=policyTraceCount%traceCapacity;
    policyTrace[index]=packed;++policyTraceCount;
    if(control) {
        wchar_t name[64]{};
        wsprintfW(name,L"PhotoshopMenu.InputContext.PolicyTrace.%u.v1",index);
        SetPropW(control,name,reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(packed)));
        SetPropW(control,policyTraceCountProperty,reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(policyTraceCount)));
    }
}
static void publishCanvas() {
    if(!control)return;
    SetPropW(control,canvasEntryTickProperty,reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(canvasEntryTick)));
    SetPropW(control,canvasEntryTriggerProperty,reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(canvasEntryTrigger)));
    SetPropW(control,canvasExitTickProperty,reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(canvasExitTick)));
    SetPropW(control,canvasExitTriggerProperty,reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(canvasExitTrigger)));
    SetPropW(control,canvasEpochProperty,reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(canvasEpoch)));
    SetPropW(control,canvasCorrectionsProperty,reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(canvasCorrections)));
}
static bool compositionBusy(HWND window) {
    if(compositionActive)return true;
    if(!window)return false;
    const auto context=associated(window);
    if(!context)return false;
    return ImmGetCompositionStringW(context,GCS_COMPSTR,nullptr,0)>0;
}
static void clearCanvasState(DWORD exitReason) {
    if(canvasTextEditing || entryPending) {
        canvasTextEditing=false;entryPending=false;
        canvasExitTick=GetTickCount64();canvasExitWallMs=wallMilliseconds();
        if(exitReason!=CanvasNone)canvasExitTrigger=exitReason;
        canvasEpoch=canvasEpoch==0xffffffff?1:canvasEpoch+1;
        publishCanvas();
    }
    compositionActive=false;
}
static void policyChanged(DWORD reason,HWND window=nullptr) {
    policyReason=reason;policyTick=GetTickCount64();
    if(control)SetPropW(control,policyTickProperty,reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(policyTick)));
    if(reason!=PolicyNone) {
        DWORD action=TraceDisable;
        if(reason==PolicyEnable)action=TraceEnable;
        else if(reason==PolicyOff||reason==PolicyRestoreOwner)action=TraceOff;
        else if(reason==PolicyRestoreOther||reason==PolicyRestoreForeground||reason==PolicyReapplyLanguage||reason==PolicyRestoreCanvasText)action=TraceRestore;
        else if(reason==PolicyCorrectionEnter||reason==PolicyCorrectionExit)action=TraceCorrection;
        recordPolicyTrace(action,reason,window);
    }
}
static bool protectedFocus(HWND window) {
    if(!window || !root || !IsWindow(window))return false;
    if(window==root)return true;
    wchar_t name[64]{};GetClassNameW(window,name,64);
    // A PSViewC inside a floating dialog (PSFloatC) is a different top-level
    // window and must keep its IME; only the canvas below the verified PS root
    // counts as a non-input shortcut target.
    return wcscmp(name,L"PSViewC")==0 && GetAncestor(window,GA_ROOT)==root;
}
static int focusIndex(HWND window) {
    for(int i=0;i<focusCount;++i)if(focusEntries[i].window==window)return i;
    return -1;
}
static DWORD focusReason(HWND window);
static bool disableFocus(HWND window,DWORD reason) {
    if(!focusPolicy || !protectedFocus(window))return false;
    const auto current=associated(window);
    auto index=focusIndex(window);
    if(index<0) {
        if(focusCount>=focusCapacity)return false;
        index=focusCount++;
        focusEntries[index].window=window;
        focusEntries[index].context=current;
    } else if(current && current!=focusEntries[index].context) {
        // PS reassociated while the same non-input window stayed focused; the
        // newest association is the one to restore on leaving that focus.
        focusEntries[index].context=current;
    }
    if(current) {
        changing=true;ImmAssociateContext(window,nullptr);changing=false;
    }
    if(associated(window)) {
        focusEntries[index]=focusEntries[--focusCount];focusEntries[focusCount]={};
        policyRestoreFailed=true;
        return false;
    }
    policyChanged(reason,window);
    return true;
}
static bool restoreFocus(DWORD reason) {
    if(!focusCount){policyRestoreFailed=false;return true;}
    bool ok=true;int keep=0;FocusEntry next[focusCapacity]{};
    for(int i=focusCount-1;i>=0;--i) {
        const auto entry=focusEntries[i];
        focusEntries[i]={};
        if(!entry.window || !IsWindow(entry.window))continue;
        const auto current=associated(entry.window);
        // PS replaced the association itself: leave its choice untouched.
        if(current && current!=entry.context)continue;
        if(current!=entry.context) {
            changing=true;ImmAssociateContext(entry.window,entry.context);changing=false;
            if(associated(entry.window)!=entry.context) {
                next[keep++]=entry;ok=false;continue;
            }
        }
    }
    focusCount=keep;
    for(int i=0;i<keep;++i)focusEntries[i]=next[i];
    policyRestoreFailed=!ok;
    policyChanged(reason);
    return ok;
}
static bool canvasTarget(HWND window) {
    if(!window || !root || !IsWindow(window))return false;
    if(GetAncestor(window,GA_ROOT)!=root)return false;
    wchar_t name[64]{};GetClassNameW(window,name,64);
    return wcscmp(name,L"PSViewC")==0;
}
// Restore the canvas association synchronously, before PS handles the message
// that enters text editing. focusCount>0 means our policy currently owns it.
static bool canvasEntry(HWND target,DWORD trigger) {
    if(!focusPolicy || !canvasTarget(target) || !focusCount)return false;
    if(canvasTextEditing)return true;
    restoreFocus(PolicyRestoreCanvasText);
    canvasTextEditing=true;entryPending=true;
    canvasEntryTick=GetTickCount64();canvasEntryWallMs=wallMilliseconds();
    canvasEntryTrigger=trigger;compositionActive=false;
    canvasEpoch=canvasEpoch==0xffffffff?1:canvasEpoch+1;
    publishCanvas();
    recordPolicyTrace(TraceCanvasEnter,trigger,target);
    return true;
}
static void postCanvasExit(DWORD trigger) {
    if(!control)return;
    canvasExitTrigger=trigger;
    const ULONGLONG packed=(static_cast<ULONGLONG>(trigger&0xff)<<32)|(canvasEpoch&0xffffffff);
    PostMessageW(control,controlMessage,(nonce<<8)|7,static_cast<LPARAM>(packed));
}
// Runs after PS processed Esc/Ctrl+Enter. A new entry bumps the epoch, so a
// stale posted exit cannot disable a later editing round.
static void canvasExitApply(ULONGLONG epoch,DWORD trigger) {
    if(!focusPolicy || !canvasTextEditing || epoch!=canvasEpoch)return;
    const auto focus=GetFocus();
    if(!protectedFocus(focus)) {
        clearCanvasState(CanvasFocusLeave);
        return;
    }
    if(compositionBusy(focus))return;
    if(disableFocus(focus,PolicyDisableCanvasExit)) {
        canvasTextEditing=false;entryPending=false;compositionActive=false;
        canvasExitTick=GetTickCount64();canvasExitWallMs=wallMilliseconds();
        canvasExitTrigger=trigger;
        canvasEpoch=canvasEpoch==0xffffffff?1:canvasEpoch+1;
        publishCanvas();
        recordPolicyTrace(TraceCanvasExit,trigger,focus);
    }
}
// Background UXP correction, timestamped in Unix milliseconds so a late event
// from an earlier round cannot affect the current one.
static void canvasCorrect(bool enter,ULONGLONG stamp) {
    if(!focusPolicy)return;
    ++canvasCorrections;publishCanvas();
    if(enter) {
        if(canvasTextEditing || stamp<canvasExitWallMs)return;
        const auto focus=GetFocus();
        if(!protectedFocus(focus))return;
        restoreFocus(PolicyCorrectionEnter);
        canvasTextEditing=true;entryPending=false;compositionActive=false;
        canvasEntryTick=GetTickCount64();canvasEntryWallMs=stamp;
        canvasEntryTrigger=CanvasCorrectionEnter;
        canvasEpoch=canvasEpoch==0xffffffff?1:canvasEpoch+1;
        publishCanvas();recordPolicyTrace(TraceCorrection,PolicyCorrectionEnter,focus);
        return;
    }
    if(!canvasTextEditing || stamp<canvasEntryWallMs)return;
    const auto focus=GetFocus();
    if(!protectedFocus(focus)){clearCanvasState(CanvasFocusLeave);return;}
    if(compositionBusy(focus))return;
    if(disableFocus(focus,PolicyCorrectionExit)) {
        canvasTextEditing=false;entryPending=false;compositionActive=false;
        canvasExitTick=GetTickCount64();canvasExitWallMs=stamp;
        canvasExitTrigger=CanvasCorrectionExit;
        canvasEpoch=canvasEpoch==0xffffffff?1:canvasEpoch+1;
        publishCanvas();recordPolicyTrace(TraceCorrection,PolicyCorrectionExit,focus);
    }
}
static bool restore() {
    if(!disabledWindow)return true;
    changing=true;const bool tsfRestored=tsf.restore();changing=false;
    if(!tsfRestored){restoreFailed=true;return false;}
    if(!IsWindow(disabledWindow)){disabledWindow=nullptr;originalContext=nullptr;restored=true;return true;}
    const auto current=associated(disabledWindow);
    // Do not overwrite a context PS has already replaced itself.
    if(current && current!=originalContext){disabledWindow=nullptr;originalContext=nullptr;restored=true;return true;}
    changing=true;ImmAssociateContext(disabledWindow,originalContext);changing=false;
    if(associated(disabledWindow)!=originalContext){restoreFailed=true;return false;}
    disabledWindow=nullptr;originalContext=nullptr;restoreFailed=false;restored=true;menuHandoff=false;return true;
}
static bool foreground() {
    const auto window=GetForegroundWindow();
    return window?(GetAncestor(window,GA_ROOTOWNER)==root || window==root || ownMenu(window)):menuReturning();
}
static bool policyForeground() {
    const auto window=GetForegroundWindow();
    if(!window)return menuHandoff;
    return window==root || GetAncestor(window,GA_ROOTOWNER)==root || ownMenu(window);
}
static bool ownFocus(HWND window) {
    DWORD pid=0;
    return window && GetWindowThreadProcessId(window,&pid)==GetCurrentThreadId() && pid==GetCurrentProcessId() && GetFocus()==window;
}
static void verifyContext() {
    if(focusPolicy) {
        const auto focus=GetFocus();
        if(canvasTextEditing) {
            if(canvasTarget(focus)) {
                // Keeping the canvas open is the whole point of this state; the
                // 100 ms watchdog must not detach it again mid-edit.
                if(focusCount)restoreFocus(PolicyRestoreCanvasText);
                return;
            }
            clearCanvasState(CanvasFocusLeave);
        }
        if(!protectedFocus(focus)) {
            if(focusCount)restoreFocus(PolicyRestoreOther);
            return;
        }
        if(focusIndex(focus)<0)disableFocus(focus,focusReason(focus));
        // Drop entries whose window disappeared. A non-null association that PS
        // restored itself is left untouched (and dropped when focus leaves),
        // matching restore()'s existing replacement protection.
        for(int i=focusCount-1;i>=0;--i) {
            const auto window=focusEntries[i].window;
            if(IsWindow(window) && protectedFocus(window))continue;
            focusEntries[i]=focusEntries[--focusCount];focusEntries[focusCount]={};
        }
        return;
    }
    if(disabledWindow && (associated(disabledWindow) || !tsf.currentDisabled())) {
        const bool immChanged=associated(disabledWindow)!=nullptr;
        const bool tsfChanged=!tsf.currentDisabled();
#ifdef INPUT_CONTEXT_SELF_TEST
        if(testTrace)std::fprintf(stderr,"restore: changed context associated=%p tsf_current=%d handoff=%d menu=%d foreground=%p focus=%p disabled=%p\n",associated(disabledWindow),tsf.currentDisabled(),int(menuHandoff),int(menuActive()),GetForegroundWindow(),GetFocus(),disabledWindow);
#endif
        const auto focus=GetFocus();
        if(!immChanged && tsfChanged && menuHandoff && foreground() && (focus==disabledWindow || focus==root)) {
            // IMM remains detached, so the non-input shortcut protection is
            // intact. Try to rebind TSF on the same PS focus after a menu or
            // activation round-trip; never restore only because TSF focus moved.
            GUITHREADINFO info{};info.cbSize=sizeof(info);
            if(GetGUIThreadInfo(0,&info) && !info.hwndCaret && !info.hwndCapture && !info.hwndMenuOwner && !info.hwndMoveSize) {
                changing=true;const bool rebound=tsf.restore() && tsf.start() && tsf.currentDisabled();changing=false;
                if(rebound)return;
            }
            return;
        }
        revision=revision==0x7fffffff?1:revision+1;
        SetPropW(control,revisionProperty,reinterpret_cast<HANDLE>(revision));
        GUITHREADINFO info{};info.cbSize=sizeof(info);
        const auto activeFocus=GetFocus();const auto activeContext=activeFocus?associated(activeFocus):nullptr;
        const bool handoff=menuHandoff && !associated(disabledWindow) && foreground() &&
            (activeFocus==disabledWindow || activeFocus==root) && GetGUIThreadInfo(0,&info) &&
            !info.hwndCaret && !info.hwndCapture && !info.hwndMenuOwner && !info.hwndMoveSize &&
            (!activeContext || ImmGetCompositionStringW(activeContext,GCS_COMPSTR,nullptr,0)<=0);
        if(handoff) {
            changing=true;const bool rebound=tsf.restore() && tsf.start() && tsf.currentDisabled();changing=false;
            if(rebound)return;
        }
        restore();
    }
}
static LRESULT state(HWND focus) {
    DWORD pid=0;
    if(focus && (GetWindowThreadProcessId(focus,&pid)!=GetCurrentThreadId() || pid!=GetCurrentProcessId()))return 9;
    const bool protectedTarget=protectedFocus(focus);
    const bool canvasOpen=canvasTextEditing && canvasTarget(focus) && associated(focus)!=nullptr;
    LRESULT value=1 | (disabledWindow?2:0) |
        (!disabledWindow && focus && ((!protectedTarget && (associated(focus) || InputContextTsf::available())) || canvasOpen)?4:0) |
        (restoreFailed?8:0) | (restored?16:0) | (tsf.keyboard && tsf.currentDisabled()?32:0) | (tsf.watching()?64:0) |
        (((disabledWindow || (focusPolicy && focusCount))) && (menuActive() || menuReturning())?128:0);
    if(focusPolicy)value|=0x100;
    if(policyRestoreFailed || (!focusPolicy && focusCount) ||
       (focusPolicy && protectedTarget && focusIndex(focus)<0 && !canvasOpen))value|=0x200;
    value|=LRESULT(focusCount&0xff)<<16;
    value|=LRESULT(policyReason&0xff)<<24;
    value|=LRESULT(canvasTextEditing?1:0)<<32;
    value|=LRESULT(typeToolActive?1:0)<<33;
    value|=LRESULT(typeToolKnown?1:0)<<34;
    value|=LRESULT(compositionActive?1:0)<<35;
    value|=LRESULT(entryPending?1:0)<<36;
    return value;
}
static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    if(message==controlMessage && (wParam>>8)==nonce) {
        const auto focus=reinterpret_cast<HWND>(lParam);const auto operation=wParam&255;
        if(operation==3){restore();restoreFocus(PolicyOff);clearCanvasState(CanvasNone);return state(focus);}
        if(operation==1)return state(focus);
        if(operation==6) {
            // Background push: current tool is a type tool (horizontal,
            // vertical or mask variant). Stored only; no window is touched.
            typeToolActive=lParam!=0;typeToolKnown=true;
            return state(GetFocus());
        }
        if(operation==7) {
            canvasExitApply(static_cast<ULONGLONG>(lParam)&0xffffffffULL,DWORD((static_cast<ULONGLONG>(lParam)>>32)&0xff));
            return state(GetFocus());
        }
        if(operation==8) {
            const bool enter=(lParam&1)!=0;
            canvasCorrect(enter,static_cast<ULONGLONG>(lParam)>>1);
            return state(GetFocus());
        }
        if(operation==4){
            if(!focusPolicy)verifyContext();
            if(disabledWindow==focus && foreground() && (ownFocus(focus) || menuActive()))deadline=GetTickCount64()+1500;
            return state(focus);
        }
        if(operation==5) {
            if(lParam) {
                if(!focusPolicy) {
                    // A lease from the previous mechanism is restored before the
                    // policy takes over; no pending request is reissued here.
                    if(!restore())return state(GetFocus())|8;
                    focusPolicy=true;policyRestoreFailed=false;
                    clearCanvasState(CanvasNone);typeToolKnown=false;
                    policyChanged(PolicyEnable);
                    const auto target=GetFocus();
                    if(protectedFocus(target))disableFocus(target,focusReason(target));
                    else restoreFocus(PolicyEnable);
                }
            } else if(focusPolicy) {
                focusPolicy=false;
                clearCanvasState(CanvasNone);
                restoreFocus(PolicyOff);
                restore();
                policyChanged(PolicyOff);
            }
            return state(GetFocus());
        }
        if(operation==2) {
            if(focusPolicy)return state(focus)|8;
            GUITHREADINFO info{};info.cbSize=sizeof(info);
            if(disabledWindow || !GetForegroundWindow() || menuActive() || !foreground() || !ownFocus(focus) || !GetGUIThreadInfo(0,&info) || info.hwndCaret || info.hwndCapture || info.hwndMenuOwner || info.hwndMoveSize)return state(focus)|8;
            const auto context=associated(focus);
            if(!context && !InputContextTsf::available())return state(focus)|8;
            if(context && ImmGetCompositionStringW(context,GCS_COMPSTR,nullptr,0)>0)return state(focus)|8;
            restored=false;originalContext=context;disabledWindow=focus;deadline=GetTickCount64()+1500;menuHandoff=false;
            changing=true;const bool tsfDisabled=tsf.start();const auto previous=ImmAssociateContext(focus,nullptr);changing=false;
            if(!tsfDisabled || previous!=context || associated(focus) || !tsf.currentDisabled() || !ownFocus(focus)){restore();return state(focus)|8;}
            const auto status=state(focus);
            return status&2?status:status|8;
        }
        return state(focus)|8;
    }
    if(message==contextChangedMessage){verifyContext();return 0;}
    if(message==WM_TIMER) {
        const bool exited=!owner || WaitForSingleObject(owner,0)!=WAIT_TIMEOUT;
        if(focusPolicy) {
            if(exited) {
#ifdef INPUT_CONTEXT_SELF_TEST
                if(testTrace)std::fprintf(stderr,"restore: owner exit policy windows=%d\n",focusCount);
#endif
                clearCanvasState(CanvasNone);
                focusPolicy=false;restoreFocus(PolicyRestoreOwner);
            } else if(!policyForeground()) {
                clearCanvasState(CanvasFocusLeave);
                restoreFocus(PolicyRestoreForeground);menuHandoff=false;
            }
        } else if(disabledWindow && (exited || !foreground() || (GetFocus()!=disabledWindow && !menuActive() && !menuReturning()) || GetTickCount64()>deadline)) {
#ifdef INPUT_CONTEXT_SELF_TEST
            if(testTrace)std::fprintf(stderr,"restore: timer exited=%d foreground=%p focus=%p disabled=%p expired=%d\n",exited,GetForegroundWindow(),GetFocus(),disabledWindow,GetTickCount64()>deadline);
#endif
            restore();
        }
        verifyContext();
        if(!focusPolicy && focusCount && policyRestoreFailed && GetTickCount64()-policyTick>1000)restoreFocus(PolicyOff);
        if(exited && !disabledWindow && !focusCount)DestroyWindow(window);
        return 0;
    }
    if(message==WM_DESTROY) {
        focusPolicy=false;
        clearCanvasState(CanvasNone);
        typeToolActive=false;typeToolKnown=false;
        restoreFocus(PolicyRestoreOwner);
        restore();
        KillTimer(window,1);RemovePropW(root,controlProperty);RemovePropW(window,nonceProperty);RemovePropW(window,ownerProperty);RemovePropW(window,revisionProperty);RemovePropW(window,policyTickProperty);
        if(owner)CloseHandle(owner);
        if(menuProcess){CloseHandle(menuProcess);menuProcess=nullptr;menuPid=0;}
        owner=nullptr;control=nullptr;root=nullptr;ownerId=0;focusCount=0;
        return 0;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}
[[maybe_unused]] static bool namedProcess(HANDLE process,const wchar_t* expected) {
    wchar_t path[32768]{};DWORD length=32768;
    if(!QueryFullProcessImageNameW(process,0,path,&length))return false;
    const auto name=wcsrchr(path,L'\\');return _wcsicmp(name?name+1:path,expected)==0;
}
static DWORD focusReason(HWND window) {
    if(window==root)return PolicyDisableRoot;
    wchar_t name[64]{};GetClassNameW(window,name,64);
    return wcscmp(name,L"PSViewC")==0?PolicyDisableView:PolicyDisableRoot;
}
static void applyFocusPolicy(HWND window,DWORD reason) {
    if(!focusPolicy)return;
    if(canvasTextEditing && canvasTarget(window)) {
        // The canvas is the text-edit carrier; a focus notification for it
        // while editing must not detach the association again.
        restoreFocus(PolicyRestoreCanvasText);
        return;
    }
    if(protectedFocus(window))disableFocus(window,reason);
    else restoreFocus(PolicyRestoreOther);
}
static bool canvasControlHeld() {
#ifdef INPUT_CONTEXT_SELF_TEST
    if(testCtrlHeld>=0)return testCtrlHeld!=0;
#endif
    return (GetKeyState(VK_CONTROL)&0x8000)!=0;
}
static bool canvasAltHeld() {
#ifdef INPUT_CONTEXT_SELF_TEST
    if(testAltHeld>=0)return testAltHeld!=0;
#endif
    return (GetKeyState(VK_MENU)&0x8000)!=0;
}
// Entry/exit state machine fed by both the queued-input and sent-message hooks.
// Entry restores before PS processes the message; exit only posts a control
// message so the key is handled first (Q5 ordering).
static void canvasInputMessage(const CWPSTRUCT& event) {
    if(!focusPolicy)return;
    if(event.message==WM_IME_STARTCOMPOSITION){compositionActive=true;return;}
    if(event.message==WM_IME_ENDCOMPOSITION){compositionActive=false;return;}
    if(!canvasTextEditing) {
        if(!canvasTarget(event.hwnd) || !focusCount)return;
        const bool control=canvasControlHeld(),alt=canvasAltHeld();
        if(event.message==WM_LBUTTONDOWN && typeToolActive)canvasEntry(event.hwnd,CanvasClickText);
        else if(event.message==WM_LBUTTONDBLCLK)canvasEntry(event.hwnd,CanvasDoubleClick);
        else if(event.message==WM_KEYDOWN && event.wParam==VK_RETURN && typeToolActive && !control && !alt)
            canvasEntry(event.hwnd,CanvasEnterText);
        return;
    }
    const auto focus=GetFocus();
    if(!canvasTarget(focus) || !canvasTarget(event.hwnd)) {
        if(event.message==WM_KEYDOWN)clearCanvasState(CanvasFocusLeave);
        return;
    }
    const bool control=canvasControlHeld(),alt=canvasAltHeld();
    if(event.message==WM_KEYDOWN && event.wParam==VK_ESCAPE && !control && !alt) {
        if(!compositionBusy(focus))postCanvasExit(CanvasEsc);
        return;
    }
    if(event.message==WM_KEYDOWN && event.wParam==VK_RETURN && !alt) {
        if(control)postCanvasExit(CanvasCtrlEnter);
        else if(event.lParam&(1<<24))postCanvasExit(CanvasNumpadEnter);
    }
}
static void observeMessage(const CWPSTRUCT& event) {
        if((disabledWindow || focusPolicy) && event.message==WM_ACTIVATEAPP && !event.wParam)
            menuHandoff=ownMenuThread(DWORD(event.lParam)) || ownMenu(GetForegroundWindow());
        if(focusPolicy && event.message==WM_ACTIVATEAPP && event.wParam && !ownMenu(GetForegroundWindow()))menuHandoff=false;
        if((disabledWindow || focusPolicy) && event.message==WM_KILLFOCUS && ownMenu(reinterpret_cast<HWND>(event.wParam)))menuHandoff=true;
        if(disabledWindow && menuHandoff && event.message==WM_KEYDOWN && !ownMenu(event.hwnd) &&
           (event.wParam!='T' || !(GetKeyState(VK_CONTROL)&0x8000)) && event.wParam!=VK_RETURN && event.wParam!=VK_F2){verifyContext();menuHandoff=false;}
        if(focusPolicy && event.hwnd!=control && !ownMenu(event.hwnd)) {
            canvasInputMessage(event);
            if(event.message==WM_SETFOCUS) {
                const auto target=event.hwnd;
                if(canvasTextEditing && !canvasTarget(target))clearCanvasState(CanvasFocusLeave);
                applyFocusPolicy(target,focusReason(target));
                return;
            }
            if(event.message==WM_INPUTLANGCHANGE) {
                // Keep the user's chosen IME/layout; only re-detach the current
                // non-input PS association after a manual language switch.
                if(canvasTextEditing && canvasTarget(GetFocus()))return;
                applyFocusPolicy(GetFocus(),PolicyReapplyLanguage);
                return;
            }
            if(event.message==WM_ACTIVATEAPP && !event.wParam && !ownMenu(GetForegroundWindow()) && !ownMenuThread(DWORD(event.lParam))) {
                clearCanvasState(CanvasFocusLeave);
                restoreFocus(PolicyRestoreForeground);
                menuHandoff=false;
                return;
            }
        }
        if(!focusPolicy && !changing && disabledWindow && event.hwnd!=control && !ownMenu(event.hwnd) &&
            ((event.message==WM_KILLFOCUS && reinterpret_cast<HWND>(event.wParam)!=disabledWindow && !ownMenu(reinterpret_cast<HWND>(event.wParam)) && !ownMenu(GetForegroundWindow()) && !menuReturning()) || event.message==WM_INPUTLANGCHANGE || event.message==WM_LBUTTONDOWN || event.message==WM_LBUTTONDBLCLK || event.message==WM_RBUTTONDOWN ||
             (event.message==WM_ACTIVATEAPP && !event.wParam && !ownMenu(GetForegroundWindow()) && !ownMenuThread(DWORD(event.lParam))) ||
             (event.message==WM_KEYDOWN && ((event.wParam=='T' && (GetKeyState(VK_CONTROL)&0x8000)) || event.wParam==VK_RETURN || event.wParam==VK_F2)))){
#ifdef INPUT_CONTEXT_SELF_TEST
            if(testTrace)std::fprintf(stderr,"restore: message=%x hwnd=%p disabled=%p w=%p foreground=%p\n",event.message,event.hwnd,disabledWindow,reinterpret_cast<void*>(event.wParam),GetForegroundWindow());
#endif
            restore();
        }
}
// Attach this in-process context helper to a Photoshop root window. Called by
// the injected queue/call hooks (legacy transport) and directly by the TSF
// service (InputService.dll), which is registered in HKLM instead of injected.
static bool attach(HWND rootWindow,DWORD ownerProcess,ULONG_PTR nonceValue) {
    if(!rootWindow || !ownerProcess)return false;
    wchar_t name[64]{};GetClassNameW(rootWindow,name,64);
    if(wcscmp(name,L"Photoshop")!=0 || rootWindow!=GetAncestor(rootWindow,GA_ROOTOWNER))return false;
    if(control)return ownerId==ownerProcess;
    const auto candidate=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,ownerProcess);
    if(!candidate)return false;
#ifdef INPUT_CONTEXT_SELF_TEST
    const bool valid=ownerProcess==GetCurrentProcessId();
#else
    const bool valid=namedProcess(GetCurrentProcess(),L"Photoshop.exe") && (namedProcess(candidate,L"151500px-menu.exe") || namedProcess(candidate,L"PhotoshopMenu.exe"));
#endif
    if(!valid){CloseHandle(candidate);return false;}
    // The control window must remain valid until PS exits, even after unhooking.
    HMODULE pinned=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                       reinterpret_cast<LPCWSTR>(&attach),&pinned);
    WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=module;wc.lpszClassName=controlClass;RegisterClassW(&wc);
    root=rootWindow;ownerId=ownerProcess;owner=candidate;nonce=nonceValue;revision=1;tsf.notify=contextChanged;
    focusCount=0;focusPolicy=false;policyRestoreFailed=false;policyReason=0;policyTick=0;
    canvasTextEditing=false;entryPending=false;compositionActive=false;
    typeToolActive=false;typeToolKnown=false;
    canvasEntryTick=canvasExitTick=0;canvasEntryTrigger=canvasExitTrigger=CanvasNone;
    canvasEntryWallMs=canvasExitWallMs=0;canvasEpoch=1;canvasCorrections=0;
    policyTraceCount=0;for(auto& entry:policyTrace)entry=0;
    control=CreateWindowExW(0,controlClass,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,module,nullptr);
    if(!control){CloseHandle(owner);owner=nullptr;ownerId=0;return false;}
    if(!SetPropW(control,nonceProperty,reinterpret_cast<HANDLE>(nonce)) ||
       !SetPropW(control,ownerProperty,reinterpret_cast<HANDLE>(ULONG_PTR(ownerId))) ||
       !SetPropW(control,revisionProperty,reinterpret_cast<HANDLE>(revision)) ||
       !SetPropW(root,controlProperty,control) || !SetTimer(control,1,100,nullptr)) {
        DestroyWindow(control);control=nullptr;return false;
    }
    publishCanvas();
    return true;
}
static bool detach() {
    if(!control)return true;
    focusPolicy=false;
    restoreFocus(PolicyOff);
    restore();
    DestroyWindow(control);
    control=nullptr;
    return true;
}
extern "C" __declspec(dllexport) BOOL WINAPI InputContextAttach(HWND rootWindow,DWORD ownerProcess,ULONG_PTR nonceValue) {
    return attach(rootWindow,ownerProcess,nonceValue)?TRUE:FALSE;
}
extern "C" __declspec(dllexport) BOOL WINAPI InputContextDetach() {
    return detach()?TRUE:FALSE;
}
extern "C" __declspec(dllexport) DWORD WINAPI InputContextOwner() {
    return ownerId;
}
extern "C" __declspec(dllexport) LRESULT CALLBACK InputContextQueueHook(int code,WPARAM wParam,LPARAM lParam) {
    if(code>=0 && wParam==PM_REMOVE) {
        const auto& message=*reinterpret_cast<MSG*>(lParam);
        const CWPSTRUCT event{message.lParam,message.wParam,message.message,message.hwnd};
        observeMessage(event);
    }
    return CallNextHookEx(nullptr,code,wParam,lParam);
}
extern "C" __declspec(dllexport) LRESULT CALLBACK InputContextHook(int code,WPARAM wParam,LPARAM lParam) {
    if(code>=0) {
        if(!attachMessage){attachMessage=RegisterWindowMessageW(L"PhotoshopMenu.InputContext.Attach.v1");controlMessage=RegisterWindowMessageW(L"PhotoshopMenu.InputContext.Control.v1");}
        const auto& event=*reinterpret_cast<CWPSTRUCT*>(lParam);
        observeMessage(event);
        if(event.message==attachMessage && event.wParam && event.lParam>0)
            attach(event.hwnd,DWORD(event.wParam),ULONG_PTR(event.lParam));
    }
    return CallNextHookEx(nullptr,code,wParam,lParam);
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);}
    return TRUE;
}
