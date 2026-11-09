// Win32 hooks: no allocation, pipe I/O or PS calls; transitions use one bounded decision.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <array>
#include <atomic>
#include <string>
#include <regex>
#include <thread>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include "ScreenSample.h"
#include "FrameKeyProtocol.h"

enum Action { None, Toggle, Hide, Select, Color, Top, Bottom, Context, Dismiss, Ready, Error, Configured, PanelClick, HoldBegin, HoldRelease, HoldCancel, HoldPin, CapsTrace, HookHealth, CapsState, ActivationTrace, SelectAbove, SelectBelow, MoveUp, MoveDown, CopyStyle, PasteStyle, EditStyle, TriggerTrace, ExtendAbove, ExtendBelow, ConvertSmartObject, HoldReveal, SearchLayers, LanguageProtect, LanguageActivity, LanguageEditEnd, SearchTrace, FrameLayers, FrameCommit, FrameRestore };
static const char* names[] = {"", "toggle", "hide", "select", "color", "moveToTop", "moveToBottom", "context", "dismiss", "hooks", "configuration", "settings", "panel.click", "hold_begin", "hold_release", "hold_cancel", "hold_pin", "caps.trace", "hook.health", "caps.state", "activation.trace", "selectAbove", "selectBelow", "moveUp", "moveDown", "copyStyle", "pasteStyle", "editStyle", "trigger.trace", "extendAbove", "extendBelow", "convertSmartObject", "hold_reveal", "tools.search", "input.language.protect", "input.language.activity", "input.language.edit_end", "search.trace", "layer.frame", "layer.frame.commit", "layer.frame.restore"};
struct Binding { Action action=None; std::array<int,6> keys{}; int count=0; };
struct Config { bool enabled=false, textInput=false,colorEditor=false,diagnostic=false,remoteCompatibility=false,searchAllowed=false,editorInput=false,imeReady=false; HWND overlay=nullptr,guardOwner=nullptr,guardPanel=nullptr,searchRoot=nullptr,searchFocus=nullptr,imeRoot=nullptr,imeFocus=nullptr; ULONGLONG sequence=0,searchEpoch=0,imeEpoch=0; std::array<Binding,16> bindings{}; bool evidenceFresh=true,frameThread=false; };
// Opt-in remote clicks qualify only on a matching release in the same verified
// PS workspace. No hover, taskbar or injected Alt+Tab activation is added.
struct RemoteClickIntent {
    uintptr_t workspace=0; ULONGLONG tick=0;
    bool event(bool down,bool enabled,uintptr_t clicked,uintptr_t foreground,ULONGLONG now) {
        if(down) {workspace=enabled?clicked:0;tick=now;return false;}
        auto previous=std::exchange(workspace,0);
        return enabled && previous && clicked==previous && foreground==previous && now-tick<=2000;
    }
};
static RemoteClickIntent remoteClick;
static int alias(int key) {
    if (key==160 || key==161) return 16;
    if (key==162 || key==163) return 17;
    if (key==164 || key==165) return 18;
    return key;
}
struct KeyMachine {
    std::array<bool,256> physical{}, held{}, consumed{};
    int pending=-1; bool used=false,searchConsumed=false,frameNative=false;
    ULONGLONG pressTime=0,frameId=0,frameStart=0; int framePhase=2;
    void cancelGesture() { pending=-1; used=true; }
    void seed(int raw) { physical[raw]=held[alias(raw)]=true; }
    void reconcileModifiers(unsigned down) {
        for(int group=0;group<3;++group) {
            const int key=VK_SHIFT+group,left=VK_LSHIFT+2*group;
            // A consumed modifier may not reach Windows' async state.
            if(consumed[key])continue;
            physical[key]=false;physical[left]=(down&(1u<<(group*2)))!=0;physical[left+1]=(down&(2u<<(group*2)))!=0;
            held[key]=physical[left] || physical[left+1];
        }
    }
    bool modified() const { return held[16] || held[17] || held[18] || held[91] || held[92]; }
    bool event(int raw,bool down,bool scope,const Config& cfg,bool visible,Action& action,ULONGLONG now=0,bool reserveOnly=false) {
        action=None; if (raw<0 || raw>=256) return false;
        int key=alias(raw); bool repeated=physical[raw]; physical[raw]=down;
        held[key]=false;
        // At most two physical keys share an alias; bounded, stack-only work.
        if (key==16 || key==17 || key==18) {
            int left=160+(key==17?2:key==18?4:0);
            held[key]=physical[key] || physical[left] || physical[left+1];
        } else held[key]=physical[raw];
        if (!down && consumed[key]) {
            consumed[key]=false;
            if(key=='F'){
                if(!searchConsumed && frameId && framePhase==0){framePhase=scope && !reserveOnly && now-frameStart<250?1:2;action=framePhase==1?FrameCommit:FrameRestore;}
                searchConsumed=false;
            }
            if (key==pending) {
                action=scope && !reserveOnly && !used?(now && now-pressTime<=220?HoldPin:HoldRelease):HoldCancel;
                pending=-1;
            }
            return true;
        }
        // A duplicate/orphan CapsLock up is not an outside-owned press. Never
        // leak it in reserved PS scope or let it submit a gesture. A known
        // outside down still owns its matching up (repeated=true) unchanged.
        if (!down && !repeated && scope && key==VK_CAPITAL) {
            for (const auto& b:cfg.bindings) for (int i=0;i<b.count;++i)
                if (b.keys[i]==VK_CAPITAL) return true;
        }
        // A physical press owns its whole repeat/release sequence. Never
        // reinterpret it after focus, modifiers or configuration changes.
        if (down && repeated) return consumed[key];
        if (!scope || !down) return down && consumed[key];
        if(key==VK_ESCAPE && (consumed['F'] || frameNative) && !searchConsumed && framePhase==0){framePhase=2;consumed[key]=true;action=FrameRestore;return true;}
        // PS-owned dialogs reserve CapsLock without launching commands or
        // interfering with Tab/navigation/input in the dialog.
        if (reserveOnly) {
            if (key==VK_CAPITAL) for (const auto& b:cfg.bindings) for (int i=0;i<b.count;++i)
                if (b.keys[i]==VK_CAPITAL) {consumed[key]=true;return true;}
            return false;
        }
        if(key=='F' && cfg.frameThread && !modified() && !held[VK_CAPITAL] && !visible)return false;
        if(key=='F' && (held[VK_CONTROL] || !visible) && !held[VK_SHIFT] && !held[VK_MENU] && !held[VK_LWIN] && !held[VK_RWIN] && !held[VK_CAPITAL]) {
            if(!cfg.searchAllowed || cfg.textInput || !cfg.evidenceFresh || (cfg.colorEditor && visible))return false;
            consumed[key]=true;searchConsumed=held[VK_CONTROL];action=searchConsumed?SearchLayers:FrameLayers;
            if(!searchConsumed){frameId=now>frameId?now:frameId+1;frameStart=now;framePhase=0;frameNative=false;}return true;
        }
        const Binding* match=nullptr;
        bool tabPrefix=false;
        for (const auto& b:cfg.bindings) {
            if ((cfg.textInput || (cfg.colorEditor && visible)) && ((b.action>=Select && b.action<=Bottom) || b.action>=SelectAbove)) continue;
            if (!b.count || (!visible && b.action==Hide)) continue;
            for (int i=0;i<b.count-1;++i) if (b.keys[i]==9) tabPrefix=true;
            if (b.keys[b.count-1]!=key) continue;
            bool matches=true;
            for (int i=0;i<b.count;++i) if (!held[b.keys[i]] && !(b.keys[i]==VK_CAPITAL && visible && cfg.evidenceFresh && !cfg.textInput && !cfg.colorEditor)) matches=false;
            for (int m:{16,17,18,91,92}) {
                bool requested=false;
                for (int i=0;i<b.count;++i) if (b.keys[i]==m) requested=true;
                if (held[m]!=requested) matches=false;
            }
            if (matches && (!match || b.count>match->count)) match=&b;
        }
        if (match) {
            consumed[key]=true;
            if (match->action==Toggle) { if (!repeated) {
                if (visible) {pending=-1;action=cfg.colorEditor?Toggle:Hide;}
                else {pending=key;used=false;pressTime=now;action=HoldBegin;}
            } }
            else { if (pending!=-1) used=true; if (!repeated) action=match->action; }
            return true;
        }
        if (key==9 && !modified() && tabPrefix) { consumed[key]=true; return true; }
        // A configured CapsLock prefix is reserved in PS even if additional
        // modifiers prevent a command match. Letting that down through toggles
        // the global CapsLock state. Do not synthesize a corrective keypress.
        if (key==VK_CAPITAL) {
            for (const auto& b:cfg.bindings) for (int i=0;i<b.count;++i)
                if (b.keys[i]==VK_CAPITAL) {consumed[key]=true;return true;}
        }
        if (pending!=-1 && key!=pending && !(cfg.colorEditor && visible)) used=true;
        return consumed[key];
    }
};
struct WinRightMachine {
    bool physical=false,mapped=false;
    bool event(bool down,bool scoped,DWORD& mouse) {
        mouse=0;
        if (down) {
            if (physical) return mapped;
            physical=true;mapped=scoped;
            if (mapped) mouse=MOUSEEVENTF_RIGHTDOWN;
            return mapped;
        }
        physical=false;
        bool consumed=mapped;mapped=false;
        if (consumed) mouse=MOUSEEVENTF_RIGHTUP;
        return consumed;
    }
};
static WinRightMachine winRight;
static bool rightInjected=false;
static constexpr UINT winRightMessage=WM_APP+20;
static bool injectRight(DWORD flags) {
    INPUT input{};input.type=INPUT_MOUSE;input.mi.dwFlags=flags;
    return SendInput(1,&input,sizeof(input))==1;
}
static Config config;
static Config inputEvidence;
static SRWLOCK inputEvidenceLock=SRWLOCK_INIT;
static ULONGLONG diagnosticUntil=0;
static KeyMachine keys;
static HWND frameEndpoint=nullptr,nativeFrameControl=nullptr;
static DWORD frameParent=0;
static ULONGLONG nativeFrameId=0,nativeFrameCommonId=0;
static UINT frameBeginMessage=0,frameCommitMessage=0,frameRestoreMessage=0,frameReleaseMessage=0;
static bool frameRawReleased=false,frameLastUpKnown=false;
static DWORD frameLastUpTime=0;
struct PassedFrameKey {DWORD time=0;HWND root=nullptr;bool plain=false,used=true;};
static std::array<PassedFrameKey,64> passedFrameKeys{};static unsigned passedFrameCursor=0;
static bool claimPassedFrame(DWORD time,HWND root) {
    for(auto& value:passedFrameKeys)if(!value.used && value.time==time && value.root==root){value.used=true;return value.plain;}
    return false;
}
static HWND target=nullptr;
static bool dismissPending=false;
static std::array<bool,3> mouseConsumed{};
static ScreenSampleClicks sampleClicks;
static bool previewPointer(int index,bool up,bool hit) {
    if(index<0 || index>=3)return false;
    if(up){bool owned=mouseConsumed[index];mouseConsumed[index]=false;return owned;}
    if(hit){mouseConsumed[index]=true;return true;}return false;
}
static std::atomic<bool> closing{false};
static DWORD hookThread;
static HANDLE available;
static ULONGLONG lastInputTick=0,lastScanUs=0;
static ULONGLONG inputEpoch=0;
// The hook only reads the last published input state. It never waits for a
// host answer: a publication that predates the newest input generation cannot
// authorise a single-key action, and an unknown state never disables chords.
static void advanceInput(){++inputEpoch;}
struct ActivationGate {
    bool active=false,allowed=false,switching=false,taskbarSwitching=false,directSwitching=false;
    ULONGLONG clicked=0;
    void observe(bool scoped,ULONGLONG now) {
        if (!scoped) {active=false;allowed=false;return;}
        if (!active) {allowed=clicked && now-clicked<=2000;active=true;clicked=0;}
    }
    void observeForeground(bool scoped,bool sameOrigin,bool taskbar,ULONGLONG now,bool transition=false) {
        // GetForegroundWindow can temporarily return NULL during activation.
        // That is no foreign application and must not consume a real click.
        // Qualification still requires a scoped HWND and bounded click age.
        if (transition) return;
        if (!scoped && !switching && !taskbarSwitching && !directSwitching && clicked && !sameOrigin && !taskbar)
            clicked=0;
        observe(scoped,now);
    }
    void click(bool photoshop,ULONGLONG now) {
        if (photoshop) {clicked=active?0:now;if (active) allowed=true;}
        else {clicked=0;allowed=false;}
    }
    void switchIntent(ULONGLONG now) {clicked=now;}
    void beginSwitch(ULONGLONG now) {switching=true;switchIntent(now);}
    void beginTaskbar(ULONGLONG now) {taskbarSwitching=true;switchIntent(now);}
    void beginDirect(ULONGLONG now) {directSwitching=true;click(true,now);switchIntent(now);}
    void clickTarget(bool photoshop,bool ownedMenu,ULONGLONG now) {
        if (photoshop) beginDirect(now);
        // Clicking our own menu belongs to the existing PS session. It must
        // neither revoke that session nor grant a new one from outside PS.
        else if (!ownedMenu) click(false,now);
    }
    void finishDirect(bool scoped,ULONGLONG now) {
        directSwitching=false;
        if (scoped) {active=true;allowed=true;clicked=0;}
        else switchIntent(now);
    }
    void finishTaskbar(bool scoped,ULONGLONG now) {
        taskbarSwitching=false;
        if (scoped) {active=true;allowed=true;clicked=0;}
        else switchIntent(now);
    }
    void finishSwitch(bool scoped,ULONGLONG now) {
        switching=false;
        if (scoped) {active=true;allowed=true;clicked=0;}
        else switchIntent(now);
    }
};
static ActivationGate activation;
static HWND activationOrigin=nullptr;
static LONGLONG counterFrequency=1;
static std::atomic<ULONGLONG> droppedEvents{0},callbackPeakUs{0};
static ULONGLONG microseconds() {
    LARGE_INTEGER value{};QueryPerformanceCounter(&value);
    return static_cast<ULONGLONG>((value.QuadPart/counterFrequency)*1000000+
        (value.QuadPart%counterFrequency)*1000000/counterFrequency);
}
#ifdef HOTKEY_MEASURE
// Measurement harness only (tools/measure-hook-scope.cpp). Never defined in the
// shipped HotkeyHost.exe.
extern void measureHookSample(unsigned long long,int);
static int measurementKind=0;
static bool measureDirectMode=false;
#endif
struct CallbackTiming {
    ULONGLONG start=microseconds();
    ULONGLONG elapsed() const {return microseconds()-start;}
    ~CallbackTiming() {
        auto elapsed=microseconds()-start;
        if (elapsed>callbackPeakUs.load(std::memory_order_relaxed)) callbackPeakUs.store(elapsed,std::memory_order_relaxed);
    }
};
// Single producer (hook thread), single consumer (pipe writer).
struct HookEvent {
    Action action=None; HWND hwnd=nullptr,root=nullptr; POINT cursor{};
    DWORD pid=0,flags=0,time=0;UINT message=0; int key=0,modifiers=0,matched=0;
    bool command=false,process=false,enabled=false,repeat=false,owned=false,blocked=false;
    ULONGLONG decisionUs=0,scanUs=0,renewUs=0,gapMs=0,peakUs=0,dropped=0;
    DWORD ageMs=0;
    bool attempted=false,toggleOn=false,previousToggle=false;
    bool activationAllowed=false,activationActive=false,taskbarSwitching=false,directSwitching=false;
    ULONGLONG activationIntent=0,inputEpoch=0,guardEpoch=0,frameId=0; int framePhase=2;
    bool guardAllowed=false;
    wchar_t windowClass[64]{};
};
static std::array<HookEvent,1024> queue{};
static std::atomic<unsigned> head{0}, tail{0};
static POINT intentOrigin{};
static bool intentRevealed=false;
static bool directCommand(Action a) {
    return a==Select || a==Color || a==Top || a==Bottom || a==SelectAbove || a==SelectBelow ||
        a==ExtendAbove || a==ExtendBelow || a==MoveUp || a==MoveDown || a==CopyStyle || a==PasteStyle || a==EditStyle || a==ConvertSmartObject;
}
// Coalesce only input already queued together; never wait for a future chord.
// Drain the queue before revealing the prefix so Caps alone opens immediately.
struct InvocationGate {
    bool active=false,shown=false;
    HookEvent begin{};
    std::vector<HookEvent> feed(const HookEvent& e) {
        if(e.action==HoldBegin){active=true;shown=false;begin=e;return {};}
        if(e.action==HoldReveal){if(active && !shown){shown=true;return {begin};}return {};}
        if(e.action==HoldPin || e.action==HoldRelease){
            if(!active)return {};
            active=false;if(!shown)return {begin,e};return {e};
        }
        if(e.action==HoldCancel){bool close=active && shown;active=false;return close?std::vector<HookEvent>{e}:std::vector<HookEvent>{};}
        if(directCommand(e.action)){
            bool close=active && shown;active=false;
            if(close){HookEvent hide{};hide.action=HoldCancel;return {hide,e};}
        }
        if(e.action==Hide || e.action==Dismiss)active=false;
        return {e};
    }
    bool due() const {return active && !shown;}
    HookEvent reveal(){shown=true;return begin;}
    DWORD wait() const {return due()?0:250;}
};
static void emit(Action action,HWND hwnd=nullptr,HWND root=nullptr,bool nativeFrame=false) {
    if (action==None) return;
    unsigned next=(head.load(std::memory_order_relaxed)+1)%queue.size();
    if (next==tail.load(std::memory_order_acquire)) {droppedEvents.fetch_add(1,std::memory_order_relaxed);return;}
    POINT cursor{};
    if (action==HoldRelease && !GetCursorPos(&cursor)) action=HoldCancel;
    queue[head.load(std::memory_order_relaxed)]={action,hwnd,root,cursor};
    queue[head.load(std::memory_order_relaxed)].inputEpoch=inputEpoch;
    queue[head.load(std::memory_order_relaxed)].frameId=keys.frameId;
    queue[head.load(std::memory_order_relaxed)].guardAllowed=nativeFrame;
    if(action==FrameLayers && frameEndpoint){SetPropW(frameEndpoint,frameActiveProp,reinterpret_cast<HANDLE>(keys.frameId));SetPropW(frameEndpoint,frameActiveRootProp,root);}
    if(action==Configured)queue[head.load(std::memory_order_relaxed)].gapMs=config.sequence;
    if(action==HoldBegin){queue[head.load(std::memory_order_relaxed)].gapMs=GetTickCount64();GetCursorPos(&intentOrigin);queue[head.load(std::memory_order_relaxed)].cursor=intentOrigin;intentRevealed=false;}
    head.store(next,std::memory_order_release); SetEvent(available);
}
static bool photoshop(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd) || IsIconic(hwnd) || !IsWindowVisible(hwnd)) return false;
    wchar_t name[64]{}; GetClassNameW(hwnd,name,64);
    bool main=wcscmp(name,L"Photoshop")==0;
    if (!main && wcscmp(name,L"OWL.DocumentWindow")!=0 && wcscmp(name,L"OWL.Dock")!=0) return false;
    HWND root=main?hwnd:GetAncestor(hwnd,GA_ROOTOWNER);
    wchar_t rootName[64]{};GetClassNameW(root,rootName,64);
    if (wcscmp(rootName,L"Photoshop")!=0 || IsIconic(root) || !IsWindowVisible(root) ||
        !IsWindowEnabled(root) || !IsWindowEnabled(hwnd)) return false;
    DWORD pid=0,owner=0;GetWindowThreadProcessId(hwnd,&pid);GetWindowThreadProcessId(root,&owner);
    return pid && pid==owner;
}
static bool menuWindow(HWND hwnd);
static bool scope(HWND foreground=GetForegroundWindow()) {
    if (!config.enabled) return false;
    if (photoshop(foreground)) { target=foreground; return true; }
    // Hide and foreground restoration are separate operations. A just-hidden
    // menu may still own foreground; it remains part of this PS key scope.
    return menuWindow(foreground) && photoshop(target);
}
struct ProcessIdentity { DWORD pid=0;HANDLE handle=nullptr; };
static std::array<ProcessIdentity,32> photoshopProcesses{};
// Non-Photoshop foregrounds are cached with a held handle: a live handle
// proves the same process (PID reuse exits the old object), so the message
// thread does not reopen the process on every WinEvent/focus notification.
static std::array<ProcessIdentity,32> nonPhotoshopProcesses{};
static std::atomic<ULONG> processQueries{0};
// Foreground identity snapshot. The low-level hooks only compare the current
// foreground HWND with this snapshot; they never open a process or build a
// process image path. A mismatch is conservative (not PS) and wakes the
// message thread to recompute.
static std::atomic<HWND> scopeForeground{nullptr};
static std::atomic<bool> scopeIsPhotoshop{false};
static std::atomic<bool> scopeKnown{false};
static std::atomic<bool> foregroundRefreshPosted{false};
static constexpr UINT foregroundMessage=WM_APP+8;
static bool processAlive(HANDLE handle) {return handle && WaitForSingleObject(handle,0)==WAIT_TIMEOUT;}
static bool namedImage(HANDLE process,const wchar_t* expected) {
    static wchar_t path[32768];
    DWORD length=32768;
    if(!QueryFullProcessImageNameW(process,0,path,&length))return false;
    const wchar_t* name=wcsrchr(path,L'\\');name=name?name+1:path;
    return _wcsicmp(name,expected)==0;
}
struct ProcessSnapshot {
    std::array<ProcessIdentity,32> entries{};
    ULONGLONG scanUs=0;
    ~ProcessSnapshot() {for (auto& p:entries) if (p.handle) CloseHandle(p.handle);}
};
static std::unique_ptr<ProcessSnapshot> scanPhotoshopProcesses() {
    auto result=std::make_unique<ProcessSnapshot>();
    auto started=microseconds();
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if (snapshot==INVALID_HANDLE_VALUE) return nullptr;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if (Process32FirstW(snapshot,&entry)) do {
        if (_wcsicmp(entry.szExeFile,L"Photoshop.exe")!=0) continue;
        processQueries.fetch_add(1,std::memory_order_relaxed);
        HANDLE handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,entry.th32ProcessID);
        if (!handle) continue;
        bool valid=namedImage(handle,L"Photoshop.exe") && processAlive(handle);
        bool stored=false;
        if (valid) for (auto& p:result->entries) if (!p.handle) {p={entry.th32ProcessID,handle};stored=true;break;}
        if (!stored) CloseHandle(handle);
    } while (Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);
    result->scanUs=microseconds()-started;
    return result;
}
static void applyProcesses(ProcessSnapshot& snapshot) {
    // Only this bounded handle exchange runs on the input thread. Worker
    // snapshots own their handles until the message thread takes ownership.
    for (auto& p:photoshopProcesses) if (p.handle) CloseHandle(p.handle);
    for (size_t i=0;i<photoshopProcesses.size();++i) photoshopProcesses[i]=std::exchange(snapshot.entries[i],{});
    lastScanUs=snapshot.scanUs;
}
static void refreshPhotoshopProcesses() {
    auto snapshot=scanPhotoshopProcesses();
    if (snapshot) applyProcesses(*snapshot);
}
static bool photoshopProcessWindow(HWND hwnd) {
    DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);
    if(!pid)return false;
    // A held handle keeps the process object alive, so a verified PID entry
    // cannot be a recycled PID and needs no time-based revalidation.
    for (const auto& p:photoshopProcesses)
        if (p.pid==pid) return processAlive(p.handle);
    for (auto& p:nonPhotoshopProcesses)
        if (p.pid==pid) {
            if(processAlive(p.handle))return false;
            CloseHandle(p.handle);p={};
        }
    // A miss verifies exactly one process: no full system enumeration, no
    // background scanner, no time window in which a running Photoshop is
    // unknown after it starts or restarts. The negative entry intentionally
    // keeps the handle so a repeated non-PS foreground costs no query.
    processQueries.fetch_add(1,std::memory_order_relaxed);
    HANDLE handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
    if(!handle)return false;
    const bool valid=namedImage(handle,L"Photoshop.exe") && processAlive(handle);
    if(!valid) {
        for (auto& p:nonPhotoshopProcesses) if (!p.handle) {p={pid,handle};return false;}
        for (auto& p:nonPhotoshopProcesses) if (!processAlive(p.handle)) {CloseHandle(p.handle);p={pid,handle};return false;}
        CloseHandle(nonPhotoshopProcesses[0].handle);nonPhotoshopProcesses[0]={pid,handle};
        return false;
    }
    for (auto& p:photoshopProcesses) if (!p.handle) {p={pid,handle};return true;}
    for (auto& p:photoshopProcesses)
        if (!processAlive(p.handle)) {CloseHandle(p.handle);p={pid,handle};return true;}
    CloseHandle(photoshopProcesses[0].handle);photoshopProcesses[0]={pid,handle};
    return true;
}
// Hook-safe identity: only the already verified positive cache is consulted.
// No OpenProcess, QueryFullProcessImageNameW or allocation.
static bool photoshopProcessWindowFast(HWND hwnd) {
    if(!hwnd)return false;
    DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);
    if(!pid)return false;
    for (const auto& p:photoshopProcesses)
        if (p.pid==pid) return processAlive(p.handle);
    return false;
}
static void publishForegroundScope(HWND foreground,bool photoshop) {
    // Store order matters: a reader that observes a new HWND before the new
    // verdict must see the conservative older false, never a stale true.
    if(photoshop) {
        scopeForeground.store(foreground,std::memory_order_release);
        scopeIsPhotoshop.store(true,std::memory_order_release);
    } else {
        scopeIsPhotoshop.store(false,std::memory_order_release);
        scopeForeground.store(foreground,std::memory_order_release);
    }
    scopeKnown.store(true,std::memory_order_release);
}
static bool photoshopKeyScopeSlow(HWND foreground) {
    return config.enabled && (photoshopProcessWindow(foreground) ||
        (menuWindow(foreground) && photoshopProcessWindow(target)));
}
static void updateForegroundScope(HWND foreground,bool force=false) {
    if(!foreground)return;
    if(!force && scopeKnown.load(std::memory_order_acquire) &&
       scopeForeground.load(std::memory_order_acquire)==foreground)return;
    publishForegroundScope(foreground,photoshopKeyScopeSlow(foreground));
}
static void requestForegroundRefresh(HWND foreground) {
    if(!foreground || !hookThread)return;
    if(foregroundRefreshPosted.exchange(true,std::memory_order_acq_rel))return;
    if(!PostThreadMessageW(hookThread,foregroundMessage,0,reinterpret_cast<LPARAM>(foreground)))
        foregroundRefreshPosted.store(false,std::memory_order_release);
}
// Called only from the low-level hooks. Never opens a process.
static bool photoshopKeyScope(HWND foreground=GetForegroundWindow()) {
    if(!config.enabled || !foreground)return false;
    if(scopeForeground.load(std::memory_order_acquire)!=foreground) {
        requestForegroundRefresh(foreground);
        return false;
    }
    return scopeIsPhotoshop.load(std::memory_order_acquire);
}
// Only the frozen, process-verified Layers list. No tree enumeration, COM,
// UIA, filesystem, allocations or blocking messages in the mouse hook.
static bool previewGuardWindow(HWND clicked) {
    HWND owner=config.guardOwner,panel=config.guardPanel;
    if(!owner || !panel || !clicked || IsWindowVisible(config.overlay) ||
       !IsWindow(owner) || IsIconic(owner) || !IsWindowVisible(owner) ||
       !IsWindow(panel) || !IsWindowVisible(panel) ||
       (clicked!=panel && !IsChild(panel,clicked)) || !photoshopProcessWindowFast(owner))return false;
    DWORD ownerPid=0,panelPid=0,clickPid=0;
    GetWindowThreadProcessId(owner,&ownerPid);GetWindowThreadProcessId(panel,&panelPid);GetWindowThreadProcessId(clicked,&clickPid);
    wchar_t ownerClass[32]{},panelClass[32]{};GetClassNameW(owner,ownerClass,32);GetClassNameW(panel,panelClass,32);
    return ownerPid && ownerPid==panelPid && ownerPid==clickPid && wcscmp(ownerClass,L"Photoshop")==0 &&
        wcscmp(panelClass,L"PSViewC")==0 && GetDlgCtrlID(panel)==0 && GetAncestor(panel,GA_ROOTOWNER)==owner;
}
static HWND remoteWorkspace(HWND child) {
    if(!child || !IsWindowVisible(child) || !IsWindowEnabled(child))return nullptr;
    HWND root=GetAncestor(child,GA_ROOT);
    if(!photoshop(root) || !photoshopProcessWindowFast(root))return nullptr;
    DWORD childPid=0,rootPid=0;
    GetWindowThreadProcessId(child,&childPid);GetWindowThreadProcessId(root,&rootPid);
    return childPid && childPid==rootPid?GetAncestor(root,GA_ROOTOWNER):nullptr;
}
static bool taskbarWindow(HWND hwnd) {
    if (!hwnd) return false;
    wchar_t name[64]{};GetClassNameW(GetAncestor(hwnd,GA_ROOT),name,64);
    return wcscmp(name,L"Shell_TrayWnd")==0 || wcscmp(name,L"Shell_SecondaryTrayWnd")==0 ||
           wcscmp(name,L"TaskListThumbnailWnd")==0;
}
static void observeActivation(HWND foreground,bool scoped,ULONGLONG now) {
    // If a taskbar/Alt+Tab intent activates some other application, consume
    // it there; a subsequent hover into PS must not inherit that intent.
    bool taskbar=taskbarWindow(foreground);
    ULONGLONG before=activation.clicked;
    activation.observeForeground(scoped,foreground==activationOrigin,taskbar,now,!foreground);
    static HWND previous=reinterpret_cast<HWND>(-1);
    static bool previousAllowed=false,previousActive=false;
    static ULONGLONG previousIntent=0;
    if (foreground!=previous || activation.allowed!=previousAllowed || activation.active!=previousActive || activation.clicked!=previousIntent) {
        unsigned next=(head.load(std::memory_order_relaxed)+1)%queue.size();
        if (next!=tail.load(std::memory_order_acquire)) {
            HookEvent event{};event.action=ActivationTrace;event.hwnd=foreground;event.root=activationOrigin;
            event.time=static_cast<DWORD>(now);event.process=scoped;event.command=taskbar;event.gapMs=before;
            GetWindowThreadProcessId(foreground,&event.pid);GetClassNameW(foreground,event.windowClass,64);
            event.activationAllowed=activation.allowed;event.activationActive=activation.active;
            event.taskbarSwitching=activation.taskbarSwitching;event.directSwitching=activation.directSwitching;
            event.activationIntent=activation.clicked;
            queue[head.load(std::memory_order_relaxed)]=event;head.store(next,std::memory_order_release);SetEvent(available);
        } else droppedEvents.fetch_add(1,std::memory_order_relaxed);
        previous=foreground;previousAllowed=activation.allowed;previousActive=activation.active;previousIntent=activation.clicked;
    }
}
static bool inside(HWND hwnd,POINT point) {
    RECT rect{}; return GetWindowRect(hwnd,&rect) && PtInRect(&rect,point);
}
static bool menuWindow(HWND hwnd) {
    if (!hwnd || !config.overlay || !IsWindow(hwnd) || !IsWindow(config.overlay)) return false;
    DWORD pid=0,owner=0;
    GetWindowThreadProcessId(hwnd,&pid);GetWindowThreadProcessId(config.overlay,&owner);
    return pid && pid==owner;
}
static bool textLanguageIntent(int key,const KeyMachine& state) {
    return ((key==VK_F2 && !state.held[VK_CONTROL]) || (key=='T' && state.held[VK_CONTROL])) && !state.held[VK_MENU] &&
        !state.held[VK_CAPITAL] && !state.held[VK_LWIN] && !state.held[VK_RWIN];
}
static bool languageActivityKey(int key,const KeyMachine& state) {
    return (key==VK_RETURN || key==VK_ESCAPE || key==VK_TAB) &&
        !state.held[VK_CONTROL] && !state.held[VK_MENU] && !state.held[VK_CAPITAL] &&
        !state.held[VK_LWIN] && !state.held[VK_RWIN];
}
static bool languageEditEndKey(int key,const KeyMachine& state) {
    return (key==VK_ESCAPE || (key==VK_RETURN && state.held[VK_CONTROL])) && !state.held[VK_MENU] &&
        !state.held[VK_CAPITAL] && !state.held[VK_LWIN] && !state.held[VK_RWIN];
}
static bool languageEditEndEvent(int key,bool down,const KeyMachine& state,int& pending) {
    if(down && languageEditEndKey(key,state)){pending=key;return true;}
    if(!down && key==pending){pending=0;return true;}
    return false;
}
static bool resumeSearchContext(bool fromMenu,const Config& cfg,HWND foreground);
static void CALLBACK languageFocusEvent(HWINEVENTHOOK,DWORD event,HWND hwnd,LONG,LONG,DWORD,DWORD) {
    if(!hwnd)return;
    if(event==EVENT_SYSTEM_FOREGROUND) {
        static HWND previous=nullptr;
        const bool fromMenu=menuWindow(std::exchange(previous,hwnd));
        // Foreground identity is decided here, on the message thread, so the
        // low-level hooks can answer from the snapshot without OpenProcess.
        updateForegroundScope(hwnd,true);
        if(!menuWindow(hwnd)) {
            if(!resumeSearchContext(fromMenu,config,hwnd))advanceInput();
            emit(LanguageActivity,hwnd);
        }
        return;
    }
    if(photoshopProcessWindow(hwnd))emit(LanguageActivity,hwnd);
}
static bool searchContextMatches(const Config& cfg,HWND root,HWND focus,ULONGLONG epoch) {
    return cfg.searchAllowed && root && focus && root==cfg.searchRoot && focus==cfg.searchFocus &&
        cfg.searchEpoch==epoch;
}
static bool searchInputAllowed(const Config& cfg,HWND foreground) {
    if(cfg.textInput || cfg.colorEditor)return false;
    const auto thread=GetWindowThreadProcessId(foreground,nullptr);
    GUITHREADINFO info{};info.cbSize=sizeof(info);
    if(!thread || !GetGUIThreadInfo(thread,&info) || !info.hwndFocus || info.hwndCapture || info.hwndMenuOwner || info.hwndMoveSize ||
       info.hwndCaret)return false;
    HWND focus=info.hwndFocus;
    const HWND root=GetAncestor(foreground,GA_ROOTOWNER);
    // Windows can report our hidden menu's focus while activation returns to PS.
    if(cfg.overlay && !IsWindowVisible(cfg.overlay) &&
       (focus==cfg.overlay || IsChild(cfg.overlay,focus)) && IsWindow(cfg.searchFocus) &&
       GetAncestor(cfg.searchFocus,GA_ROOTOWNER)==root)focus=cfg.searchFocus;
    wchar_t name[32]{};GetClassNameW(focus,name,32);
    if(wcscmp(name,L"Edit")==0 || wcscmp(name,L"RichEdit20W")==0 || wcscmp(name,L"RichEdit50W")==0 || wcscmp(name,L"ComboBox")==0)return false;
    return searchContextMatches(cfg,root,focus,inputEpoch);
}
static void finishNativeFrame() {
    if(nativeFrameControl && IsWindow(nativeFrameControl)) {
        const auto nonce=reinterpret_cast<ULONG_PTR>(GetPropW(nativeFrameControl,L"PhotoshopMenu.InputContext.Nonce.v1"));
        const auto message=RegisterWindowMessageW(L"PhotoshopMenu.InputContext.Control.v1");
        if(nonce && message)PostMessageW(nativeFrameControl,message,(nonce<<8)|10,LPARAM(nativeFrameId));
    }
    nativeFrameControl=nullptr;nativeFrameId=nativeFrameCommonId=0;keys.frameNative=false;
}
static void observedFrameRelease() {
    if(keys.framePhase!=0 || !keys.frameId)return;
    frameRawReleased=true;keys.physical['F']=keys.held['F']=false;
    keys.framePhase=photoshopKeyScope(GetForegroundWindow()) && GetTickCount64()-keys.frameStart<250?1:2;
    finishNativeFrame();emit(keys.framePhase==1?FrameCommit:FrameRestore);
}
static LRESULT CALLBACK frameProcedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    if(message==WM_INPUT) {
        RAWINPUT input{};UINT bytes=sizeof(input);
        if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam),RID_INPUT,&input,&bytes,sizeof(RAWINPUTHEADER))!=UINT(-1) &&
           input.header.dwType==RIM_TYPEKEYBOARD && input.data.keyboard.VKey=='F' && (input.data.keyboard.Flags&RI_KEY_BREAK) &&
           static_cast<int32_t>(DWORD(GetMessageTime())-DWORD(keys.frameStart))>=0)observedFrameRelease();
        return DefWindowProcW(window,message,wParam,lParam);
    }
    if(message==frameReleaseMessage && wParam==keys.frameId && keys.framePhase==0) {
        const HWND control=reinterpret_cast<HWND>(lParam);wchar_t name[64]{};GetClassNameW(control,name,64);
        if(wcscmp(name,L"PhotoshopMenu.InputContext.v1")==0 && GetPropW(control,frameHostProp)==frameEndpoint && GetPropW(control,frameRootProp)==GetPropW(frameEndpoint,frameActiveRootProp) &&
           reinterpret_cast<ULONG_PTR>(GetPropW(control,L"PhotoshopMenu.InputContext.Owner.v1"))==frameParent &&
           static_cast<int32_t>(DWORD(reinterpret_cast<ULONG_PTR>(GetPropW(control,frameReleaseTimeProp)))-DWORD(keys.frameStart))>=0)observedFrameRelease();
        return 0;
    }
    if(message==frameBeginMessage || message==frameCommitMessage || message==frameRestoreMessage) {
        const HWND control=reinterpret_cast<HWND>(lParam);
        if(message!=frameBeginMessage) {
            if(control==nativeFrameControl && wParam==nativeFrameId && keys.frameId==nativeFrameCommonId) {
                if(reinterpret_cast<ULONGLONG>(GetPropW(control,frameReleasedProp))==nativeFrameId){keys.physical['F']=keys.held['F']=false;frameRawReleased=true;}
                keys.framePhase=message==frameCommitMessage?1:2;emit(keys.framePhase==1?FrameCommit:FrameRestore);finishNativeFrame();
            }
            return 0;
        }
        wchar_t name[64]{};GetClassNameW(control,name,64);
        if(!config.enabled || keys.consumed['F'] || wcscmp(name,L"PhotoshopMenu.InputContext.v1")!=0 ||
           reinterpret_cast<ULONG_PTR>(GetPropW(control,L"PhotoshopMenu.InputContext.Owner.v1"))!=frameParent ||
           GetPropW(control,frameHostProp)!=frameEndpoint || reinterpret_cast<ULONGLONG>(GetPropW(control,frameIdProp))!=wParam ||
           GetPropW(control,frameEpochProp)!=GetPropW(control,frameCurrentProp))return 0;
        const HWND root=static_cast<HWND>(GetPropW(control,frameRootProp)),focus=static_cast<HWND>(GetPropW(control,frameFocusProp)),foreground=GetForegroundWindow();
        if(DWORD(GetTickCount64())-DWORD(reinterpret_cast<ULONG_PTR>(GetPropW(control,frameMessageTimeProp)))>2000)return 0;
        if(!claimPassedFrame(DWORD(reinterpret_cast<ULONG_PTR>(GetPropW(control,frameMessageTimeProp))),root))return 0;
        if(GetAncestor(foreground,GA_ROOTOWNER)!=root)return 0;
        updateForegroundScope(foreground,true);
        DWORD pid=0,owner=0;const auto thread=GetWindowThreadProcessId(control,&pid);
        GUITHREADINFO info{};info.cbSize=sizeof(info);GetWindowThreadProcessId(root,&owner);
        if(!root || !focus || !photoshopKeyScope(foreground) || GetAncestor(foreground,GA_ROOTOWNER)!=root || pid!=owner ||
           !GetGUIThreadInfo(thread,&info) || info.hwndFocus!=focus || info.hwndCaret || info.hwndCapture || info.hwndMenuOwner || info.hwndMoveSize)return 0;
        nativeFrameControl=control;nativeFrameId=wParam;
        keys.frameId=wParam>keys.frameId?wParam:keys.frameId+1;nativeFrameCommonId=keys.frameId;
        keys.frameStart=reinterpret_cast<ULONGLONG>(GetPropW(control,frameTickProp));keys.framePhase=0;keys.searchConsumed=false;keys.frameNative=true;
        // The down passed this hook: its up must reach Windows too. PS consumes the pair in its queue.
        emit(FrameLayers,focus,root,true);
        if(!keys.physical['F'] && frameLastUpKnown && DWORD(frameLastUpTime-DWORD(keys.frameStart))<2000) {
            keys.framePhase=DWORD(frameLastUpTime-DWORD(keys.frameStart))<250?1:2;
            emit(keys.framePhase==1?FrameCommit:FrameRestore);finishNativeFrame();
        }
        return 0;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}
static bool photoshopInputFocus(HWND foreground) {
    // Hook path: the foreground snapshot, never a process query or handle wait.
    if(!photoshopKeyScope(foreground) || menuWindow(foreground))return false;
    DWORD pid=0,focusPid=0;const auto thread=GetWindowThreadProcessId(foreground,&pid);
    GUITHREADINFO info{};info.cbSize=sizeof(info);
    if(!GetGUIThreadInfo(thread,&info) || !info.hwndFocus)return false;
    GetWindowThreadProcessId(info.hwndFocus,&focusPid);return pid && focusPid==pid;
}
static bool resumeSearchContext(bool fromMenu,const Config& cfg,HWND foreground) {
    if(!fromMenu || IsWindowVisible(cfg.overlay) || cfg.textInput || !cfg.searchAllowed ||
       cfg.searchEpoch!=inputEpoch || !cfg.searchFocus || !IsWindow(cfg.searchFocus) ||
       GetAncestor(foreground,GA_ROOTOWNER)!=cfg.searchRoot || !photoshopKeyScope(foreground))return false;
    wchar_t name[32]{};GetClassNameW(cfg.searchFocus,name,32);
    // Keep the generation during handoff; each key still checks the actual focus/caret.
    return wcscmp(name,L"Edit")!=0 && wcscmp(name,L"RichEdit20W")!=0 &&
        wcscmp(name,L"RichEdit50W")!=0 && wcscmp(name,L"ComboBox")!=0;
}
static bool routeKey(KeyMachine& state,int raw,bool down,bool commandScope,bool processScope,
                     const Config& cfg,bool visible,Action& action,ULONGLONG now) {
    // Reserve only CapsLock in PS-owned non-workspace windows. Other keys keep
    // their normal dialog/navigation behavior, and cannot launch scripts here.
    bool reserve=raw==VK_CAPITAL && processScope && !commandScope;
    return state.event(raw,down,commandScope || reserve,cfg,visible,action,now,reserve);
}
static unsigned physicalModifiers(bool altDown=false) {
    unsigned down=0;
    for(int i=0;i<6;++i)if(GetAsyncKeyState(VK_LSHIFT+i)&0x8000)down|=1u<<i;
    if(altDown && !(down&48))down|=16;
    return down;
}
static LRESULT CALLBACK keyboard(int code,WPARAM message,LPARAM data) {
    CallbackTiming timing;
    if (code>=0) {
        lastInputTick=GetTickCount64();
        const auto& input=*reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        if(input.vkCode=='F' && (message==WM_KEYUP || message==WM_SYSKEYUP)){frameLastUpTime=input.time;frameLastUpKnown=true;}
        if(input.vkCode=='F' && (message==WM_KEYDOWN || message==WM_SYSKEYDOWN) && frameRawReleased) {
            keys.consumed['F']=false;keys.physical['F']=keys.held['F']=false;frameRawReleased=false;
        }
        const int logical=alias(input.vkCode);
        // The current hook event has not updated async state yet.
        if((message==WM_KEYDOWN || message==WM_SYSKEYDOWN) && logical!=VK_SHIFT && logical!=VK_CONTROL && logical!=VK_MENU && logical!=VK_LWIN && logical!=VK_RWIN)
            keys.reconcileModifiers(physicalModifiers((input.flags&LLKHF_ALTDOWN)!=0));
        Action action;
        HWND foreground=GetForegroundWindow();
        bool processScope=photoshopKeyScope(foreground);
        observeActivation(foreground,processScope,lastInputTick);
        // An explicit task switch is activation intent, unlike mouse hover.
        if (!(input.flags&LLKHF_INJECTED) && (message==WM_KEYDOWN || message==WM_SYSKEYDOWN) &&
            input.vkCode==VK_TAB && (keys.held[VK_MENU] || (input.flags&LLKHF_ALTDOWN))) {
            activationOrigin=foreground;activation.beginSwitch(lastInputTick);
        }
        if (!(input.flags&LLKHF_INJECTED) && alias(input.vkCode)==VK_MENU &&
            (message==WM_KEYUP || message==WM_SYSKEYUP) && activation.switching) {
            activationOrigin=foreground;activation.finishSwitch(processScope,lastInputTick);
        }
        // A Photoshop workspace (or our own menu over it) is the only
        // qualification. The old activation heuristic is kept for diagnostics
        // and the optional remote path; it must never decide whether a key
        // works, because activation by Win+Tab, a taskbar thumbnail click, a
        // remote session or a service restart is indistinguishable to a hook.
        bool commandScope=scope(foreground);
        bool repeat=input.vkCode<256 && keys.physical[input.vkCode];
        bool owned=input.vkCode<256 && keys.consumed[alias(input.vkCode)];
        if (input.vkCode==VK_LWIN && !(input.flags&LLKHF_INJECTED)) {
            DWORD mouse=0;
            if (winRight.event(message==WM_KEYDOWN || message==WM_SYSKEYDOWN,processScope,mouse)) {
                // Inject from the message loop, never recursively in a hook.
                if (mouse && !PostThreadMessageW(hookThread,winRightMessage,mouse,
                    reinterpret_cast<LPARAM>(GetForegroundWindow()))) emit(Error);
                return 1;
            }
        }
        const bool searchOwned=keys.searchConsumed;
        Config localConfig=config;
        Config evidence;
        AcquireSRWLockShared(&inputEvidenceLock);evidence=inputEvidence;ReleaseSRWLockShared(&inputEvidenceLock);
        localConfig.textInput=evidence.textInput;localConfig.editorInput=evidence.editorInput;
        localConfig.searchAllowed=evidence.searchAllowed;localConfig.searchRoot=evidence.searchRoot;localConfig.searchFocus=evidence.searchFocus;localConfig.searchEpoch=evidence.searchEpoch;
        localConfig.imeReady=evidence.imeReady;localConfig.imeRoot=evidence.imeRoot;localConfig.imeFocus=evidence.imeFocus;localConfig.imeEpoch=evidence.imeEpoch;
        localConfig.colorEditor=evidence.colorEditor && menuWindow(foreground);
        // The hook never waits for the host. A publication authorises a
        // single-key action only when it was produced for this exact input
        // generation, workspace root and focus window; otherwise the key is
        // passed through and no command runs. Explicit Caps chords are not
        // gated by staleness (holding Caps is not typing).
        localConfig.evidenceFresh=false;
        if(commandScope) {
            GUITHREADINFO evidenceFocus{};evidenceFocus.cbSize=sizeof(evidenceFocus);
            const auto thread=GetWindowThreadProcessId(foreground,nullptr);
            localConfig.evidenceFresh=thread!=0 && localConfig.searchEpoch==inputEpoch &&
                GetGUIThreadInfo(thread,&evidenceFocus) && evidenceFocus.hwndFocus &&
                evidenceFocus.hwndFocus==localConfig.searchFocus &&
                GetAncestor(foreground,GA_ROOTOWNER)==localConfig.searchRoot;
        }
        if(input.vkCode=='F') {
            const HWND nativeControl=static_cast<HWND>(GetPropW(GetAncestor(foreground,GA_ROOTOWNER),L"PhotoshopMenu.InputContext.Window.v1"));
            localConfig.frameThread=processScope && nativeControl && GetPropW(nativeControl,frameHostProp)==frameEndpoint && reinterpret_cast<ULONG_PTR>(GetPropW(nativeControl,L"PhotoshopMenu.InputContext.Owner.v1"))==frameParent;
            localConfig.searchAllowed=searchInputAllowed(localConfig,foreground);
            localConfig.evidenceFresh=localConfig.searchAllowed;
        }
        bool unusedPrefix=keys.pending!=-1 && !keys.used;
        bool blocked=routeKey(keys,input.vkCode,message==WM_KEYDOWN || message==WM_SYSKEYDOWN,
                     commandScope,processScope,localConfig,IsWindowVisible(config.overlay),action,GetTickCount64());
        if(input.vkCode=='F' && !blocked && !repeat && (message==WM_KEYDOWN || message==WM_SYSKEYDOWN) && commandScope) {
            passedFrameKeys[passedFrameCursor++%passedFrameKeys.size()]={input.time,GetAncestor(foreground,GA_ROOTOWNER),!keys.modified() && !keys.held[VK_CAPITAL],false};
        }
        static int pendingEditEnd=0;
        const bool down=message==WM_KEYDOWN || message==WM_SYSKEYDOWN;
        if(!processScope)pendingEditEnd=0;
        const bool inputEvent=input.vkCode=='F' || input.vkCode=='T' || input.vkCode==VK_RETURN || input.vkCode==VK_ESCAPE || input.vkCode==VK_TAB || input.vkCode==VK_F2;
        const bool psInput=inputEvent && photoshopInputFocus(foreground);
        if(inputEvent && !psInput)pendingEditEnd=0;
        if(psInput && !blocked && (!down || !repeat)) {
            if(languageEditEndEvent(input.vkCode,down,keys,pendingEditEnd)){advanceInput();emit(LanguageEditEnd);}
            else if(down && textLanguageIntent(input.vkCode,keys)){advanceInput();emit(LanguageProtect);}
            else if(down && languageActivityKey(input.vkCode,keys)){
                advanceInput();emit(LanguageActivity);
            }
            else if(down && ((input.vkCode=='F' && !keys.modified()) || input.vkCode=='T'))emit(LanguageActivity,foreground);
        }
        bool traceConfigured=false;
        if(config.diagnostic && GetTickCount64()<diagnosticUntil && input.vkCode=='F')traceConfigured=true;
        if(config.diagnostic && GetTickCount64()<diagnosticUntil) for(const auto& b:config.bindings)
            if(b.action==Toggle && b.count && b.keys[b.count-1]==alias(input.vkCode))traceConfigured=true;
        if (input.vkCode==VK_CAPITAL || traceConfigured) {
            unsigned next=(head.load(std::memory_order_relaxed)+1)%queue.size();
            if (next!=tail.load(std::memory_order_acquire)) {
                HookEvent event{};event.action=traceConfigured?(input.vkCode=='F'?SearchTrace:TriggerTrace):CapsTrace;event.hwnd=foreground;
                event.key=alias(input.vkCode);event.matched=action;
                event.modifiers=(keys.held[16]?1:0)|(keys.held[17]?2:0)|(keys.held[18]?4:0)|(keys.held[20]?8:0);
                event.attempted=localConfig.textInput;
                GetWindowThreadProcessId(foreground,&event.pid);
                GetClassNameW(foreground,event.windowClass,64);
                event.flags=input.flags;event.time=input.time;event.message=static_cast<UINT>(message);
                event.command=commandScope;event.process=processScope;event.enabled=config.enabled;
                event.repeat=repeat;event.owned=owned;event.blocked=blocked;
                event.inputEpoch=inputEpoch;event.guardEpoch=localConfig.searchEpoch;event.guardAllowed=localConfig.searchAllowed;
                event.activationAllowed=activation.allowed;event.activationActive=activation.active;
                event.taskbarSwitching=activation.taskbarSwitching;event.activationIntent=activation.clicked;
                event.decisionUs=microseconds()-timing.start;
                event.ageMs=GetTickCount()-input.time;
                event.dropped=droppedEvents.load(std::memory_order_relaxed);
                queue[head.load(std::memory_order_relaxed)]=event;
                head.store(next,std::memory_order_release);SetEvent(available);
            } else droppedEvents.fetch_add(1,std::memory_order_relaxed);
        }
        if (blocked) {
            // Other hooks need the F pair too, or they can mistake Ctrl+F for double Ctrl.
            if(input.vkCode=='F' && (searchOwned || keys.searchConsumed))CallNextHookEx(nullptr,code,message,data);
            if((action==FrameCommit || action==FrameRestore) && nativeFrameCommonId==keys.frameId)finishNativeFrame();
            emit(action,(action==SearchLayers || action==FrameLayers)?localConfig.searchFocus:nullptr,(action==SearchLayers || action==FrameLayers)?localConfig.searchRoot:nullptr); return 1;
        }
        if(unusedPrefix && keys.used && action==None)emit(HoldCancel);
    }
#ifdef HOTKEY_MEASURE
    measureHookSample(timing.elapsed(),measurementKind);
    if(measureDirectMode)return 0;
#endif
    return CallNextHookEx(nullptr,code,message,data);
}
static LRESULT CALLBACK pointer(int code,WPARAM message,LPARAM data) {
    CallbackTiming timing;
    if (code>=0) {
        lastInputTick=GetTickCount64();
        HWND foreground=GetForegroundWindow();
        observeActivation(foreground,photoshopKeyScope(foreground),lastInputTick);
        int index=message==WM_LBUTTONUP || message==WM_LBUTTONDOWN?0:
                  message==WM_RBUTTONUP || message==WM_RBUTTONDOWN?1:
                  message==WM_MBUTTONUP || message==WM_MBUTTONDOWN?2:-1;
        bool up=message==WM_LBUTTONUP || message==WM_RBUTTONUP || message==WM_MBUTTONUP;
        const auto& mouse=*reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
        const bool captured=config.enabled && !config.textInput && config.colorEditor &&
            index>=0 && index<2 && screenSampleCaptured(config.overlay,foreground);
        const int sampleRoute=sampleClicks.route(index,up,captured);
        if(sampleRoute==2)return 1;
        // Leave sampling to the menu's real capture. It is neither an outside
        // dismiss, a taskbar activation nor a right-click menu cancellation.
        if(sampleRoute==1)return CallNextHookEx(nullptr,code,message,data);
        if(message==WM_MOUSEMOVE && keys.pending!=-1 && !keys.used && !intentRevealed && !(mouse.flags&LLMHF_INJECTED)) {
            const long long dx=mouse.pt.x-intentOrigin.x,dy=mouse.pt.y-intentOrigin.y;
            if(dx*dx+dy*dy>=144){intentRevealed=true;emit(HoldReveal);}
        }
        if(index==0) {
            if(mouse.flags&LLMHF_INJECTED) {
                bool enabled=config.enabled && config.remoteCompatibility;
                HWND clicked=enabled?remoteWorkspace(WindowFromPoint(mouse.pt)):nullptr;
                HWND current=enabled && photoshop(foreground)?remoteWorkspace(foreground):nullptr;
                if(remoteClick.event(!up,enabled,reinterpret_cast<uintptr_t>(clicked),
                    reinterpret_cast<uintptr_t>(current),lastInputTick)) {
                    activationOrigin=foreground;
                    activation.clickTarget(true,false,lastInputTick);
                    activation.finishDirect(true,lastInputTick);
                }
            } else remoteClick.workspace=0;
        }
        // Complete the physical taskbar click before cancellation can consume
        // its up. Win11 can change foreground between down and up; observing
        // only the down loses intent, or never grants an already-active PS.
        if (message==WM_LBUTTONUP && activation.taskbarSwitching &&
            !(reinterpret_cast<const MSLLHOOKSTRUCT*>(data)->flags&LLMHF_INJECTED)) {
            activation.finishTaskbar(photoshopKeyScope(foreground),lastInputTick);
            observeActivation(foreground,photoshopKeyScope(foreground),lastInputTick);
        }
        if (message==WM_LBUTTONUP && activation.directSwitching &&
            !(reinterpret_cast<const MSLLHOOKSTRUCT*>(data)->flags&LLMHF_INJECTED)) {
            activation.finishDirect(photoshopKeyScope(foreground),lastInputTick);
        }
        if (up && previewPointer(index,true,false))return 1;
        if (index>=0 && !up) {
            POINT point=reinterpret_cast<const MSLLHOOKSTRUCT*>(data)->pt;
            if(previewPointer(index,false,previewGuardWindow(WindowFromPoint(point))))return 1;
            if (index==0 && !(reinterpret_cast<const MSLLHOOKSTRUCT*>(data)->flags&LLMHF_INJECTED)) {
                HWND clicked=WindowFromPoint(point);
                activationOrigin=foreground;
                if (taskbarWindow(clicked)) activation.beginTaskbar(lastInputTick);
                else activation.clickTarget(photoshopProcessWindowFast(clicked),
                    menuWindow(clicked) && photoshopProcessWindowFast(target),lastInputTick);
            }
            bool visible=IsWindowVisible(config.overlay);
            if (visible && index==1 && scope()) {
                // Explicit right-click cancellation owns the click, so return
                // focus through Hide rather than outside-click Dismiss.
                keys.cancelGesture();
                emit(Hide);mouseConsumed[index]=true;return 1;
            }
            if (config.enabled) {
                HWND child=WindowFromPoint(point),root=GetAncestor(child,GA_ROOT);
                if (photoshop(root) && photoshopProcessWindowFast(root)){advanceInput();emit(PanelClick,child,root);}
            }
            if (visible && !inside(config.overlay,point) && !menuWindow(GetAncestor(WindowFromPoint(point),GA_ROOT))) {
                bool scoped=scope();
                if (!dismissPending) { dismissPending=true; emit(Dismiss); }
                if (scoped && index!=1) { mouseConsumed[index]=true; return 1; }
            }
        }
        if (message==WM_LBUTTONDOWN || message==WM_LBUTTONUP) {
            unsigned next=(head.load(std::memory_order_relaxed)+1)%queue.size();
            if (next!=tail.load(std::memory_order_acquire)) {
                HookEvent event{};event.action=ActivationTrace;event.hwnd=foreground;
                event.root=WindowFromPoint(reinterpret_cast<const MSLLHOOKSTRUCT*>(data)->pt);
                event.message=static_cast<UINT>(message);event.time=GetTickCount();
                event.flags=mouse.flags;
                event.process=photoshopProcessWindowFast(event.root);
                event.activationAllowed=activation.allowed;event.activationActive=activation.active;
                event.directSwitching=activation.directSwitching;event.taskbarSwitching=activation.taskbarSwitching;
                event.activationIntent=activation.clicked;
                queue[head.load(std::memory_order_relaxed)]=event;
                head.store(next,std::memory_order_release);SetEvent(available);
            } else droppedEvents.fetch_add(1,std::memory_order_relaxed);
        }
    }
#ifdef HOTKEY_MEASURE
    measureHookSample(timing.elapsed(),measurementKind);
    if(measureDirectMode)return 0;
#endif
    return CallNextHookEx(nullptr,code,message,data);
}
static void stop() {
    closing.store(true); PostThreadMessageW(hookThread,WM_QUIT,0,0); SetEvent(available);
}
// Restricted internal JSON schema, parsed on the pipe thread, never in a hook.
static Config parse(const std::string& line) {
    Config next;
    std::smatch match;
    auto field=[&](const char* name,const char* pattern) {
        if (!std::regex_search(line,match,std::regex(std::string("\"")+name+"\"\\s*:\\s*"+pattern)))
            throw std::runtime_error("Missing configuration field");
        return match[1].str();
    };
    next.enabled=field("enabled","(true|false)")=="true";
    if (std::regex_search(line,match,std::regex("\"text_input\"\\s*:\\s*(true|false)"))) next.textInput=match[1]=="true";
    if (std::regex_search(line,match,std::regex("\"search_non_input\"\\s*:\\s*(true|false)"))) next.searchAllowed=match[1]=="true";
    if (std::regex_search(line,match,std::regex("\"editor_input\"\\s*:\\s*(true|false)"))) next.editorInput=match[1]=="true";
    if (std::regex_search(line,match,std::regex("\"ime_ready\"\\s*:\\s*(true|false)"))) next.imeReady=match[1]=="true";
    if (std::regex_search(line,match,std::regex("\"ime_root\"\\s*:\\s*([0-9]+)"))) next.imeRoot=reinterpret_cast<HWND>(std::stoull(match[1]));
    if (std::regex_search(line,match,std::regex("\"ime_focus\"\\s*:\\s*([0-9]+)"))) next.imeFocus=reinterpret_cast<HWND>(std::stoull(match[1]));
    if (std::regex_search(line,match,std::regex("\"ime_epoch\"\\s*:\\s*([0-9]+)"))) next.imeEpoch=std::stoull(match[1]);
    if (std::regex_search(line,match,std::regex("\"search_root\"\\s*:\\s*([0-9]+)"))) next.searchRoot=reinterpret_cast<HWND>(std::stoull(match[1]));
    if (std::regex_search(line,match,std::regex("\"search_focus\"\\s*:\\s*([0-9]+)"))) next.searchFocus=reinterpret_cast<HWND>(std::stoull(match[1]));
    if (std::regex_search(line,match,std::regex("\"search_epoch\"\\s*:\\s*([0-9]+)"))) next.searchEpoch=std::stoull(match[1]);
    if (std::regex_search(line,match,std::regex("\"color_editor\"\\s*:\\s*(true|false)"))) next.colorEditor=match[1]=="true";
    if (std::regex_search(line,match,std::regex("\"diagnostic_trace\"\\s*:\\s*(true|false)"))) next.diagnostic=match[1]=="true";
    if (std::regex_search(line,match,std::regex("\"remote_compatibility\"\\s*:\\s*(true|false)"))) next.remoteCompatibility=match[1]=="true";
    next.overlay=reinterpret_cast<HWND>(std::stoull(field("hwnd","([0-9]+)")));
    if(std::regex_search(line,match,std::regex("\"configuration_sequence\"\\s*:\\s*([0-9]+)")))next.sequence=std::stoull(match[1]);
    if(std::regex_search(line,match,std::regex("\"preview_guard_owner\"\\s*:\\s*([0-9]+)")))next.guardOwner=reinterpret_cast<HWND>(std::stoull(match[1]));
    if(std::regex_search(line,match,std::regex("\"preview_guard_panel\"\\s*:\\s*([0-9]+)")))next.guardPanel=reinterpret_cast<HWND>(std::stoull(match[1]));
    const Action actions[]={Toggle,Hide,Select,Color,Top,Bottom,SelectAbove,SelectBelow,MoveUp,MoveDown,CopyStyle,PasteStyle,EditStyle,ExtendAbove,ExtendBelow,ConvertSmartObject};
    int index=0;
    for (Action a:actions) {
        Binding& b=next.bindings[index++]; b.action=a;
        if (!std::regex_search(line,match,std::regex(std::string("\"")+names[a]+"\"\\s*:\\s*\\[([^\\]]*)\\]"))) {
            if(a>=SelectAbove)continue;
            throw std::runtime_error("Missing binding");
        }
        auto list=match[1].str();
        size_t offset=0;
        while (offset<list.size()) {
            size_t consumed=0; int key=std::stoi(list.substr(offset),&consumed);
            if (key<0 || key>=256 || b.count==6) throw std::runtime_error("Invalid binding");
            b.keys[b.count++]=key; offset+=consumed;
            while (offset<list.size() && isspace(static_cast<unsigned char>(list[offset]))) ++offset;
            if (offset==list.size()) break;
            if (list[offset++]!=',') throw std::runtime_error("Invalid binding separator");
        }
    }
    return next;
}
static void readSettings() {
    std::string line;
    while (std::getline(std::cin,line)) {
        if (line.find("\"quit\"")!=std::string::npos) break;
        // Verification replies are no longer requested. Ignore legacy or late
        // lines instead of parsing them as configuration.
        if(line.find("\"verify_reply\"")!=std::string::npos)continue;
        try {
            auto next=std::make_unique<Config>(parse(line));
            AcquireSRWLockExclusive(&inputEvidenceLock);inputEvidence=*next;ReleaseSRWLockExclusive(&inputEvidenceLock);
            if (!PostThreadMessageW(hookThread,WM_APP,0,reinterpret_cast<LPARAM>(next.get()))) break;
            next.release();
        } catch (...) {
            // One malformed or partial line must not remove every hook. Keep
            // the last good configuration and report the failure.
            PostThreadMessageW(hookThread,WM_APP+1,0,0);
        }
    }
    PostThreadMessageW(hookThread,WM_QUIT,0,0);
}
static void writeActions() {
    HANDLE output=GetStdHandle(STD_OUTPUT_HANDLE);
    InvocationGate intent;
    std::vector<HookEvent> outgoing;size_t nextOutput=0;
    while (!closing.load() || tail.load()!=head.load()) {
        unsigned cursor=tail.load(std::memory_order_relaxed);
        while (cursor!=head.load(std::memory_order_acquire) || nextOutput<outgoing.size() || (!closing.load() && intent.due())) {
            if(nextOutput==outgoing.size()) {
                if(cursor!=head.load(std::memory_order_acquire)) {
                    auto input=queue[cursor];cursor=(cursor+1)%queue.size();tail.store(cursor,std::memory_order_release);
                    outgoing=intent.feed(input);nextOutput=0;continue;
                }
                outgoing={intent.reveal()};nextOutput=0;
            }
            auto event=outgoing[nextOutput++];Action action=event.action;
            const char* type=action==Ready?"ready":action==Error?"error":action==Configured?"configured":"action";
            std::string line;
            if(action==Ready) {
                line="{\"type\":\"ready\",\"input_epoch\":"+std::to_string(event.inputEpoch)+",\"frame_hwnd\":"+std::to_string(reinterpret_cast<uintptr_t>(frameEndpoint))+"}\n";
                DWORD written=0;
                if(!WriteFile(output,line.data(),static_cast<DWORD>(line.size()),&written,nullptr) || written!=line.size()){stop();return;}
                continue;
            }
            if (action==ActivationTrace) {
                std::wstring wideClass(event.windowClass);std::string windowClass(wideClass.begin(),wideClass.end());
                windowClass=std::regex_replace(windowClass,std::regex("[^A-Za-z0-9_.# -]"),"_");
                line=std::string("{\"type\":\"")+(event.message?"activation.trace":"activation.foreground")+"\",\"foreground\":"+std::to_string(reinterpret_cast<uintptr_t>(event.hwnd))+
                    ",\"clicked\":"+std::to_string(reinterpret_cast<uintptr_t>(event.root))+",\"message\":"+std::to_string(event.message)+
                    ",\"pid\":"+std::to_string(event.pid)+",\"window_class\":\""+windowClass+"\",\"taskbar\":"+(event.command?"true":"false")+
                    ",\"flags\":"+std::to_string(event.flags)+
                    ",\"intent_before\":"+std::to_string(event.gapMs)+
                    ",\"tick_ms\":"+std::to_string(event.time)+",\"clicked_ps\":"+(event.process?"true":"false")+
                    ",\"allowed\":"+(event.activationAllowed?"true":"false")+",\"active\":"+(event.activationActive?"true":"false")+
                    ",\"direct_switching\":"+(event.directSwitching?"true":"false")+
                    ",\"taskbar_switching\":"+(event.taskbarSwitching?"true":"false")+
                    ",\"intent_tick\":"+std::to_string(event.activationIntent)+"}\n";
                DWORD written=0;
                if (!WriteFile(output,line.data(),static_cast<DWORD>(line.size()),&written,nullptr) || written!=line.size()) {stop();return;}
                continue;
            }
            if (action==CapsState) {
                line="{\"type\":\"caps.state\",\"tick_ms\":"+std::to_string(event.gapMs)+
                    ",\"toggle_on\":"+(event.toggleOn?"true":"false")+",\"previous\":"+(event.previousToggle?"true":"false")+
                    ",\"baseline\":"+(event.enabled?"true":"false")+",\"hwnd\":"+std::to_string(reinterpret_cast<uintptr_t>(event.hwnd))+
                    ",\"pid\":"+std::to_string(event.pid)+",\"process_scope\":"+(event.process?"true":"false")+
                    ",\"tracked_down\":"+(event.repeat?"true":"false")+",\"owned\":"+(event.owned?"true":"false")+
                    ",\"dropped_total\":"+std::to_string(event.dropped)+",\"source\":\"GetKeyState; hook message queue\"}\n";
                DWORD written=0;
                if (!WriteFile(output,line.data(),static_cast<DWORD>(line.size()),&written,nullptr) || written!=line.size()) {stop();return;}
                continue;
            }
            if (action==HookHealth) {
                line="{\"type\":\"hook.health\",\"scan_us\":"+std::to_string(event.scanUs)+
                    ",\"renew_us\":"+std::to_string(event.renewUs)+",\"timer_gap_ms\":"+std::to_string(event.gapMs)+
                    ",\"callback_peak_us\":"+std::to_string(event.peakUs)+",\"dropped_total\":"+std::to_string(event.dropped)+
                    ",\"renew_ok\":"+(event.blocked?"true":"false")+",\"renew_attempted\":"+(event.attempted?"true":"false")+
                    ",\"input_epoch\":"+std::to_string(event.inputEpoch)+",\"frame_id\":"+std::to_string(event.frameId)+",\"frame_phase\":"+std::to_string(event.framePhase)+",\"scan_on_worker\":true}\n";
                DWORD written=0;
                if (!WriteFile(output,line.data(),static_cast<DWORD>(line.size()),&written,nullptr) || written!=line.size()) {stop();return;}
                continue;
            }
            if (action==CapsTrace || action==TriggerTrace || action==SearchTrace) {
                std::string windowClass;
                for (wchar_t c:event.windowClass) {
                    if (!c) break;
                    if (c==L'"' || c==L'\\') windowClass+='\\';
                    windowClass+=c>=32 && c<=126?static_cast<char>(c):'?';
                }
                line=std::string("{\"type\":\"")+(action==SearchTrace?"search.trace":action==TriggerTrace?"trigger.trace":"caps.trace")+"\",\"key\":"+std::to_string(event.key)+",\"modifiers\":"+std::to_string(event.modifiers)+",\"matched_action\":\""+names[event.matched]+"\",\"text_input\":"+(event.attempted?"true":"false")+",\"input_epoch\":"+std::to_string(event.inputEpoch)+",\"guard_epoch\":"+std::to_string(event.guardEpoch)+",\"guard_allowed\":"+(event.guardAllowed?"true":"false")+",\"hwnd\":"+std::to_string(reinterpret_cast<uintptr_t>(event.hwnd))+
                    ",\"pid\":"+std::to_string(event.pid)+",\"window_class\":\""+windowClass+"\",\"message\":"+std::to_string(event.message)+
                    ",\"flags\":"+std::to_string(event.flags)+",\"key_time_ms\":"+std::to_string(event.time)+
                    ",\"command_scope\":"+(event.command?"true":"false")+",\"process_scope\":"+(event.process?"true":"false")+
                    ",\"enabled\":"+(event.enabled?"true":"false")+",\"repeat\":"+(event.repeat?"true":"false")+
                    ",\"owned_before\":"+(event.owned?"true":"false")+",\"blocked\":"+(event.blocked?"true":"false")+
                    ",\"activation_allowed\":"+(event.activationAllowed?"true":"false")+
                    ",\"activation_active\":"+(event.activationActive?"true":"false")+
                    ",\"taskbar_switching\":"+(event.taskbarSwitching?"true":"false")+
                    ",\"activation_intent_tick\":"+std::to_string(event.activationIntent)+
                    ",\"decision_us\":"+std::to_string(event.decisionUs)+",\"delivery_age_ms\":"+std::to_string(event.ageMs)+
                    ",\"dropped_total\":"+std::to_string(event.dropped)+"}\n";
                DWORD written=0;
                if (!WriteFile(output,line.data(),static_cast<DWORD>(line.size()),&written,nullptr) || written!=line.size()) {stop();return;}
                continue;
            }
            if (action==LanguageProtect) line="{\"type\":\"input.language.protect\",\"input_epoch\":"+std::to_string(event.inputEpoch)+"}\n";
            else if (action==LanguageActivity) line="{\"type\":\"input.language.activity\",\"input_epoch\":"+std::to_string(event.inputEpoch)+",\"hwnd\":"+std::to_string(reinterpret_cast<uintptr_t>(event.hwnd))+"}\n";
            else if (action==LanguageEditEnd) line="{\"type\":\"input.language.edit_end\",\"input_epoch\":"+std::to_string(event.inputEpoch)+"}\n";
            else if (action==PanelClick) line="{\"type\":\"panel.click\",\"input_epoch\":"+std::to_string(event.inputEpoch)+",\"hwnd\":"+std::to_string(reinterpret_cast<uintptr_t>(event.hwnd))+",\"root\":"+std::to_string(reinterpret_cast<uintptr_t>(event.root))+"}\n";
            else {
                line=std::string("{\"type\":\"")+type+"\",\"action\":\""+names[action]+"\"";
                if(action==Configured)line+=",\"configuration_sequence\":"+std::to_string(event.gapMs);
                if(action==SearchLayers || action==FrameLayers)line+=",\"input_guard\":{\"epoch\":"+std::to_string(event.inputEpoch)+",\"root\":"+std::to_string(reinterpret_cast<uintptr_t>(event.root))+",\"focus\":"+std::to_string(reinterpret_cast<uintptr_t>(event.hwnd))+"}";
                if(action==FrameLayers)line+=",\"native_non_input\":"+std::string(event.guardAllowed?"true":"false");
                if(action==FrameLayers || action==FrameCommit || action==FrameRestore)line+=",\"frame_id\":"+std::to_string(event.frameId);
                if (action==HoldRelease || action==HoldBegin) line+=",\"cursor\":["+std::to_string(event.cursor.x)+","+std::to_string(event.cursor.y)+"]";
                line+="}\n";
            }
            DWORD written=0;
            if (!WriteFile(output,line.data(),static_cast<DWORD>(line.size()),&written,nullptr) || written!=line.size()) { stop(); return; }
        }
        if (!closing.load()) WaitForSingleObject(available,intent.wait());
    }
}
static void checkAt(bool value,int line) { if (!value) throw std::runtime_error("Native hook self-test failed at line "+std::to_string(line)); }
#define check(value) checkAt(value,__LINE__)
static bool renewalAllowed(ULONGLONG now,ULONGLONG last,bool keyHeld,bool winHeld,bool mouseHeld) {
    return !keyHeld && !winHeld && !mouseHeld && now-last>=250;
}
static void sampleCapsState(bool force=false) {
    static bool sampled=false,previous=false;
    // The low bit is the toggle bit. GetAsyncKeyState's low bit is NOT a
    // toggle state. Sample only from the pumping message thread, not a hook.
    bool current=(GetKeyState(VK_CAPITAL)&1)!=0;
    if (sampled && current==previous && !force) return;
    unsigned next=(head.load(std::memory_order_relaxed)+1)%queue.size();
    if (next!=tail.load(std::memory_order_acquire)) {
        HookEvent event{};event.action=CapsState;event.hwnd=GetForegroundWindow();
        GetWindowThreadProcessId(event.hwnd,&event.pid);
        event.toggleOn=current;event.previousToggle=previous;event.enabled=!sampled || force;
        event.process=photoshopKeyScopeSlow(event.hwnd);event.repeat=keys.physical[VK_CAPITAL];event.owned=keys.consumed[VK_CAPITAL];
        event.gapMs=GetTickCount64();event.dropped=droppedEvents.load(std::memory_order_relaxed);
        queue[head.load(std::memory_order_relaxed)]=event;
        head.store(next,std::memory_order_release);SetEvent(available);
        sampled=true;previous=current;
    } else droppedEvents.fetch_add(1,std::memory_order_relaxed);
}
static int selfTest() {
    {
        Config c;c.enabled=true;c.bindings[0]={Toggle,{VK_CAPITAL},1};Action action;
        for(int side=0;side<6;++side) {
            KeyMachine state;state.seed(VK_LSHIFT+side);
            check(state.event(VK_CAPITAL,true,true,c,false,action,1000) && action==None);
            check(state.event(VK_CAPITAL,false,true,c,false,action,1010) && action==None);
            state.reconcileModifiers(0);
            check(!state.modified() && !state.physical[VK_LSHIFT+side]);
            check(state.event(VK_CAPITAL,true,true,c,false,action,1020) && action==HoldBegin);
            check(state.event(VK_CAPITAL,false,true,c,false,action,1030) && action==HoldPin);
            state.reconcileModifiers(1u<<side);
            check(state.modified() && state.physical[VK_LSHIFT+side]);
            check(state.event(VK_CAPITAL,true,true,c,false,action,1040) && action==None);
            check(state.event(VK_CAPITAL,false,true,c,false,action,1050) && action==None);
        }
        KeyMachine both;both.reconcileModifiers(48);both.event(VK_LMENU,false,false,c,false,action);
        check(both.held[VK_MENU] && both.physical[VK_RMENU]);both.reconcileModifiers(0);check(!both.modified());
        both.seed(VK_LMENU);both.consumed[VK_MENU]=true;both.reconcileModifiers(0);check(both.held[VK_MENU]);
        check(both.event(VK_LMENU,false,false,c,false,action) && !both.held[VK_MENU]);
        both.seed(VK_LWIN);both.reconcileModifiers(0);check(both.held[VK_LWIN]);
        KeyMachine real;real.seed(VK_LMENU);const auto sampled=physicalModifiers();real.reconcileModifiers(sampled);
        check(real.held[VK_MENU]==bool(sampled&48));
        std::cout<<"Modifier reconciliation passed: missed releases, real holds, left/right pairs, consumed modifiers, Caps recovery and OS sampling\n";
    }
    KeyMachine languageKeys;
    check(!textLanguageIntent('T',languageKeys) && textLanguageIntent(VK_F2,languageKeys) && !textLanguageIntent('A',languageKeys));
    check(!languageActivityKey('A',languageKeys) && !languageActivityKey('V',languageKeys) && languageActivityKey(VK_ESCAPE,languageKeys) && !languageActivityKey(VK_SHIFT,languageKeys));
    check(!languageActivityKey('F',languageKeys) && !languageActivityKey('F',languageKeys));
    check(!languageActivityKey('A',languageKeys) && !languageActivityKey('V',languageKeys));
    check(!textLanguageIntent('T',languageKeys) && languageActivityKey(VK_RETURN,languageKeys) && languageActivityKey(VK_TAB,languageKeys));
    check(languageEditEndKey(VK_ESCAPE,languageKeys) && !languageEditEndKey(VK_RETURN,languageKeys));
    languageKeys.held[VK_CONTROL]=true;check(languageEditEndKey(VK_RETURN,languageKeys) && !languageActivityKey(VK_RETURN,languageKeys));
    languageKeys.held[VK_MENU]=true;check(!languageEditEndKey(VK_RETURN,languageKeys));languageKeys.held[VK_MENU]=false;languageKeys.held[VK_CONTROL]=false;
    int pendingEdit=0;languageKeys.held[VK_CONTROL]=true;
    check(languageEditEndEvent(VK_RETURN,true,languageKeys,pendingEdit));
    languageKeys.held[VK_CONTROL]=false;
    check(!languageEditEndEvent(VK_CONTROL,false,languageKeys,pendingEdit));
    check(languageEditEndEvent(VK_RETURN,false,languageKeys,pendingEdit) && pendingEdit==0);
    check(!languageEditEndEvent(VK_RETURN,false,languageKeys,pendingEdit));
    for(int modifier:{VK_CONTROL,VK_MENU,VK_CAPITAL,VK_LWIN,VK_RWIN}) {
        languageKeys.held[modifier]=true;check(textLanguageIntent('T',languageKeys)==(modifier==VK_CONTROL));languageKeys.held[modifier]=false;
        languageKeys.held[modifier]=true;check(!languageActivityKey('A',languageKeys));languageKeys.held[modifier]=false;
    }
    ScreenSampleClicks samples;
    for(int n=0;n<1000;++n){
        check(samples.route(0,false,false)==0); // Ordinary outside click still dismisses.
        check(samples.route(0,false,true)==1 && samples.route(0,true,true)==1);
        check(samples.route(0,true,true)==0); // No orphan release ownership.
        check(samples.route(0,false,true)==1 && samples.route(0,true,false)==2);
        check(samples.route(1,false,true)==1 && samples.route(1,true,false)==2);
        check(samples.route(1,false,true)==1 && samples.route(1,true,true)==2);
        check(samples.route(2,false,true)==0 && samples.route(-1,false,true)==0);
    }
    check(!screenSampleCaptured(nullptr,GetForegroundWindow()));
    std::cout<<"Screen sample hook routing passed: captured clicks bypass outside/right dismiss, lost capture and right-cancel releases consumed, ordinary outside unchanged\n";
    for(int button=0;button<3;++button){
        check(!previewPointer(button,true,false));
        check(!previewPointer(button,false,false));
        check(previewPointer(button,false,true));
        // A known terminal/config change between down and up cannot leak a
        // half click. No replay and no synthetic release are needed.
        check(previewPointer(button,true,false));
        check(!previewPointer(button,true,false));
        check(!previewPointer(button,false,false));
    }
    for(Action command:{CopyStyle,PasteStyle,EditStyle,SelectAbove,SelectBelow,ExtendAbove,ExtendBelow,ConvertSmartObject}) {
        for(int i=0;i<1000;++i){
            InvocationGate gate;HookEvent begin{},action{},up{};
            begin.action=HoldBegin;begin.gapMs=1000;action.action=command;up.action=HoldCancel;
            check(gate.feed(begin).empty() && gate.due() && gate.wait()==0);
            auto result=gate.feed(action);check(result.size()==1 && result[0].action==command);
            check(gate.feed(up).empty() && !gate.due());
        }
    }
    {
        InvocationGate gate;HookEvent begin{},up{},move{},command{};
        begin.action=HoldBegin;begin.gapMs=1000;begin.cursor={-400,200};up.action=HoldPin;move.action=HoldReveal;command.action=CopyStyle;
        check(gate.feed(begin).empty());auto tap=gate.feed(up);
        check(tap.size()==2 && tap[0].action==HoldBegin && tap[0].cursor.x==-400 && tap[1].action==HoldPin);
        gate.feed(begin);check(gate.due() && gate.wait()==0 && gate.reveal().action==HoldBegin && !gate.due());
        auto selected=gate.feed(command);check(selected.size()==2 && selected[0].action==HoldCancel && selected[1].action==CopyStyle);
        gate.feed(begin);auto moved=gate.feed(move);check(moved.size()==1 && moved[0].action==HoldBegin);
        up.action=HoldRelease;auto released=gate.feed(up);check(released.size()==1 && released[0].action==HoldRelease);
        gate.feed(begin);up.action=HoldCancel;check(gate.feed(up).empty() && !gate.due());
    }
    {
        Config c;c.bindings[0]={ConvertSmartObject,{VK_F5},1};KeyMachine state;Action a;
        check(state.event(VK_F5,true,true,c,false,a,1000) && a==ConvertSmartObject);
        check(state.event(VK_F5,true,true,c,false,a,1010) && a==None);
        check(state.event(VK_F5,false,true,c,false,a,1020) && a==None);
        c.textInput=true;check(!state.event(VK_F5,true,true,c,false,a,1030) && a==None);
        state.event(VK_F5,false,true,c,false,a,1040);c.textInput=false;
        check(!routeKey(state,VK_F5,true,false,true,c,false,a,1050) && a==None);
    }
    {
        Config c;c.searchAllowed=true;c.bindings[0]={Toggle,{VK_CAPITAL},1};c.bindings[1]={EditStyle,{VK_CAPITAL,'F'},2};Action action;
        KeyMachine bare;check(bare.event('F',true,true,c,false,action) && action==FrameLayers);
        check(bare.event('F',true,true,c,false,action) && action==None);
        check(bare.event('F',false,false,c,false,action) && action==FrameRestore);
        KeyMachine tap;check(tap.event('F',true,true,c,false,action,1000) && action==FrameLayers);
        check(tap.event('F',false,true,c,false,action,1100) && action==FrameCommit && tap.framePhase==1);
        check(!tap.event('F',false,true,c,false,action,1101) && action==None);
        KeyMachine hold;check(hold.event('F',true,true,c,false,action,1200) && action==FrameLayers);
        check(hold.event('F',true,true,c,false,action,1500) && action==None);
        check(hold.event('F',false,true,c,false,action,1600) && action==FrameRestore && hold.framePhase==2);
        check(hold.event('F',true,true,c,false,action,1700) && action==FrameLayers);
        check(hold.event(VK_ESCAPE,true,true,c,false,action,1800) && action==FrameRestore);
        check(hold.event('F',false,true,c,false,action,1900) && action==None);
        KeyMachine guardedBare;c.searchAllowed=false;check(!guardedBare.event('F',true,true,c,false,action));c.searchAllowed=true;
        KeyMachine f;f.seed(VK_CONTROL);
        check(f.event('F',true,true,c,false,action) && action==SearchLayers);
        check(f.searchConsumed);
        check(f.event('F',true,true,c,false,action) && action==None);
        check(f.event('F',false,false,c,false,action) && action==None);
        check(!f.searchConsumed);
        for(int modifier:{VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN}) {
            KeyMachine modified;modified.seed(VK_CONTROL);modified.event(modifier,true,true,c,false,action);
            check(!modified.event('F',true,true,c,false,action) && action==None);
            modified.event(modifier,false,true,c,false,action);
            check(!modified.event('F',false,true,c,false,action));
        }
        for(int guarded=0;guarded<3;++guarded) {
            KeyMachine input;input.seed(VK_CONTROL);c.textInput=guarded==0;c.colorEditor=guarded==1;
            check(!routeKey(input,'F',true,guarded!=2,true,c,true,action,1000) && action==None);
            check(!input.event('F',false,false,c,true,action));
        }
        c.textInput=c.colorEditor=false;KeyMachine chord;
        check(chord.event(VK_CAPITAL,true,true,c,false,action) && action==HoldBegin);
        check(chord.event('F',true,true,c,false,action) && action==EditStyle);
        check(chord.event('F',false,true,c,false,action));
        check(chord.event(VK_CAPITAL,false,true,c,false,action) && action==HoldCancel);
        c.searchAllowed=false;KeyMachine unverified;unverified.seed(VK_CONTROL);
        check(!unverified.event('F',true,true,c,false,action) && action==None);
        check(!unverified.event('F',false,true,c,false,action));
        c.searchAllowed=true;c.searchRoot=reinterpret_cast<HWND>(10);c.searchFocus=reinterpret_cast<HWND>(11);c.searchEpoch=100;
        check(searchContextMatches(c,c.searchRoot,c.searchFocus,100));
        check(!searchContextMatches(c,c.searchRoot,c.searchFocus,101));
        check(!searchContextMatches(c,c.searchRoot,c.searchFocus,99));
        check(!searchContextMatches(c,reinterpret_cast<HWND>(12),c.searchFocus,100));
        check(!searchContextMatches(c,c.searchRoot,reinterpret_cast<HWND>(12),100));
    }
    {
        HWND root=reinterpret_cast<HWND>(10);
        passedFrameKeys[0]={123,root,true,false};passedFrameKeys[1]={124,root,false,false};
        check(!claimPassedFrame(123,reinterpret_cast<HWND>(11)));
        check(claimPassedFrame(123,root) && !claimPassedFrame(123,root));
        check(!claimPassedFrame(124,root) && passedFrameKeys[1].used);
    }
    {
        Config cfg;cfg.searchAllowed=true;cfg.frameThread=true;cfg.bindings[0]={Toggle,{VK_CAPITAL},1};cfg.bindings[1]={EditStyle,{VK_CAPITAL,'F'},2};Action result;
        KeyMachine thread;check(!thread.event('F',true,true,cfg,false,result,1000) && result==None);check(!thread.event('F',false,true,cfg,false,result,1100));
        KeyMachine ctrl;ctrl.seed(VK_CONTROL);check(ctrl.event('F',true,true,cfg,false,result,1200) && result==SearchLayers);
        KeyMachine caps;caps.seed(VK_CAPITAL);check(caps.event('F',true,true,cfg,false,result,1200) && result==EditStyle);
        KeyMachine owned;cfg.frameThread=false;check(owned.event('F',true,true,cfg,false,result,1300) && result==FrameLayers);cfg.frameThread=true;
        check(owned.event('F',true,true,cfg,false,result,1400) && result==None);check(owned.event('F',false,true,cfg,false,result,1600) && result==FrameRestore);
        KeyMachine native;native.frameNative=true;native.frameId=1700;native.frameStart=1700;native.framePhase=0;
        check(!native.event('F',true,true,cfg,false,result,1700));
        check(!native.event('F',true,true,cfg,false,result,1800));
        check(!native.event('F',false,true,cfg,false,result,1900));
        check(!native.event('F',true,true,cfg,false,result,2000));
        check(native.event(VK_ESCAPE,true,true,cfg,false,result,2100) && result==FrameRestore);
        check(native.event(VK_ESCAPE,false,true,cfg,false,result,2200));
        check(!native.event('F',false,true,cfg,false,result,2300));
    }
    // Complete remote click pairs only: opt-in, same valid workspace and 2s.
    RemoteClickIntent remote;
    check(!remote.event(true,false,10,10,100));
    check(!remote.event(false,false,10,10,101));
    check(!remote.event(true,true,10,0,200));
    check(remote.event(false,true,10,10,201));
    check(!remote.event(false,true,10,10,202)); // release cannot replay
    for(auto pair:std::array<std::pair<uintptr_t,uintptr_t>,4>{{{0,10},{10,0},{11,10},{10,11}}}) {
        remote.event(true,true,10,10,300);
        check(!remote.event(false,true,pair.first,pair.second,301));
    }
    remote.event(true,true,10,10,400);
    check(!remote.event(false,true,10,10,2401));
    remote.event(true,true,10,10,3000);
    check(!remote.event(false,false,10,10,3001));
    remote.event(true,true,0,10,4000); // taskbar/foreign/modal not a workspace
    check(!remote.event(false,true,10,10,4001));
    ActivationGate gate;
    gate.observe(false,100);gate.observe(true,200);check(!gate.allowed);
    gate.click(true,201);check(gate.allowed);
    gate.observe(true,1000);check(gate.allowed);
    gate.observe(false,1100);check(!gate.allowed);
    gate.observe(true,1200);check(!gate.allowed);
    gate.observe(false,1300);gate.click(true,1400);gate.observe(true,1401);check(gate.allowed);
    gate.observe(false,1500);gate.click(true,1600);gate.observe(true,2200);check(gate.allowed);
    gate.observe(false,2300);gate.click(true,2400);gate.observe(true,4500);check(!gate.allowed);
    gate.switchIntent(4600);gate.observe(false,4601);gate.observe(true,4700);check(gate.allowed);
    gate.observe(false,4800);gate.beginSwitch(4900);
    gate.observe(false,5000);check(gate.switching && !gate.allowed);
    gate.observe(true,10000);check(!gate.allowed);
    gate.finishSwitch(true,10100);check(gate.allowed && !gate.switching && !gate.clicked);
    gate.observe(false,10200);gate.observe(true,10300);check(!gate.allowed);
    gate.observe(false,10400);gate.beginSwitch(10500);gate.finishSwitch(false,10600);
    gate.observe(true,10601);check(gate.allowed);
    // Taskbar clicking an already-foreground, unqualified PS must qualify it.
    gate.observe(false,11000);gate.observe(true,11001);check(!gate.allowed);
    gate.beginTaskbar(11002);gate.observe(true,11003);check(gate.taskbarSwitching);
    gate.finishTaskbar(true,11004);check(gate.allowed && !gate.taskbarSwitching);
    // Temporary taskbar foreground does not end the click gesture.
    gate.observe(false,11100);gate.beginTaskbar(11101);gate.observe(false,11102);
    check(gate.taskbarSwitching);gate.finishTaskbar(true,11103);check(gate.allowed);
    // Delayed final PS activation remains bounded; ordinary hover cannot reuse it.
    gate.observe(false,11200);gate.beginTaskbar(11201);gate.finishTaskbar(false,11202);
    gate.observe(true,11203);check(gate.allowed);
    gate.observe(false,11204);gate.observe(true,11205);check(!gate.allowed);
    gate.beginTaskbar(11300);gate.finishTaskbar(false,11301);
    gate.observe(true,14000);check(!gate.allowed);
    gate.observe(false,14100);gate.beginTaskbar(14101);
    gate.observeForeground(false,false,false,14102);check(gate.clicked && gate.taskbarSwitching);
    gate.finishTaskbar(true,14103);check(gate.allowed);
    gate.observe(false,14200);gate.beginTaskbar(14201);gate.finishTaskbar(false,14202);
    gate.observeForeground(false,false,false,14203);check(!gate.clicked);
    gate.observe(true,14204);check(!gate.allowed); // Other app won; hover must not inherit.
    // Taskbar release -> NULL activation gap -> PS must retain the click.
    gate.observe(false,14300);gate.beginTaskbar(14301);gate.finishTaskbar(false,14302);
    gate.observeForeground(false,false,true,14303);
    gate.observeForeground(false,false,false,14304,true);check(gate.clicked==14302);
    gate.observeForeground(true,false,false,14305);check(gate.allowed);
    // A later transient NULL must not revoke an already-qualified PS either.
    gate.observeForeground(false,false,false,14306,true);check(gate.active && gate.allowed);
    gate.observeForeground(true,false,false,14307);check(gate.allowed);
    gate.observeForeground(false,false,false,14308);check(!gate.allowed);
    gate.observeForeground(true,false,false,14309);check(!gate.allowed);
    // NULL gaps cannot extend the original two-second activation intent.
    gate.observe(false,14400);gate.beginTaskbar(14401);gate.finishTaskbar(false,14402);
    gate.observeForeground(false,false,false,17000,true);
    gate.observeForeground(true,false,false,17001);check(!gate.allowed);
    // A PS click can pass through an intermediate foreground before its up.
    gate.observe(false,15000);gate.beginDirect(15001);
    gate.observeForeground(false,false,false,15002);
    check(gate.directSwitching && gate.clicked);
    gate.finishDirect(true,15003);check(gate.allowed && !gate.directSwitching);
    gate.observe(false,15004);gate.observe(true,15005);check(!gate.allowed);
    // Also restore an already-foreground PS whose earlier qualification was lost.
    gate.beginDirect(15100);gate.finishDirect(true,15101);check(gate.allowed);
    gate.observe(false,15200);gate.beginDirect(15201);gate.finishDirect(false,15202);
    gate.observeForeground(false,false,false,15203);check(!gate.clicked);
    gate.observe(true,15204);check(!gate.allowed);
    gate.clickTarget(true,false,15300);gate.finishDirect(true,15301);check(gate.allowed);
    for (int i=0;i<10000;++i) {
        gate.clickTarget(false,true,15302+i); // Own menu click then return to PS.
        gate.observe(true,15303+i);check(gate.allowed);
    }
    gate.clickTarget(false,false,26000);check(!gate.allowed);
    gate.observe(false,26001);gate.clickTarget(false,true,26002);
    gate.observe(true,26003);check(!gate.allowed); // Own menu never grants new scope.
    for (int i=0;i<10000;++i) {
        ULONGLONG last=1000+i*20;
        check(!renewalAllowed(last+20,last,false,false,false));
        check(!renewalAllowed(last+600,last,true,false,false));
        check(!renewalAllowed(last+600,last,false,true,false));
        check(!renewalAllowed(last+600,last,false,false,true));
        check(renewalAllowed(last+250,last,false,false,false));
    }
    for (const auto pair:std::array<std::pair<int,Action>,5>{{{67,CopyStyle},{86,PasteStyle},{70,EditStyle},{87,SelectAbove},{83,SelectBelow}}}) {
        Config ref;ref.searchAllowed=true;ref.bindings[0]={Toggle,{20},1};ref.bindings[1]={pair.second,{20,pair.first},2};
        for(bool pinned:{false,true}) {
            KeyMachine machine;Action result;
            if(!pinned)check(machine.event(20,true,true,ref,false,result,1000) && result==HoldBegin);
            const Action expected=pair.second;
            check(machine.event(pair.first,true,true,ref,pinned,result,1100) && result==expected);
            check(machine.event(pair.first,true,true,ref,pinned,result,1110) && result==None);
            check(machine.event(pair.first,false,true,ref,pinned,result,1120) && result==None);
            if(!pinned)check(machine.event(20,false,true,ref,pinned,result,1200) && result==HoldCancel);
        }
        for(int guarded:{0,1,2}) {
            KeyMachine machine;Action result;ref.textInput=guarded==1;ref.colorEditor=guarded==2;
            check(!machine.event(pair.first,true,guarded!=0,ref,true,result,1000) && result==None);
        }
    }
    for(int shift:{VK_LSHIFT,VK_RSHIFT}) for(int key:{87,83}) for(bool shiftFirst:{false,true}) {
        Config cfg;cfg.bindings[0]={Toggle,{20},1};
        cfg.bindings[1]={key==87?SelectAbove:SelectBelow,{20,key},2};
        cfg.bindings[2]={key==87?ExtendAbove:ExtendBelow,{16,20,key},3};
        KeyMachine machine;Action result;
        machine.event(shiftFirst?shift:20,true,true,cfg,false,result,1000);
        machine.event(shiftFirst?20:shift,true,true,cfg,false,result,1010);
        check(machine.event(key,true,true,cfg,false,result,1020) && result==(key==87?ExtendAbove:ExtendBelow));
        check(machine.event(key,true,true,cfg,false,result,1030) && result==None);
        check(machine.event(key,false,true,cfg,false,result,1040) && result==None);
        machine.event(shift,false,true,cfg,false,result,1050);
        check(machine.event(20,false,true,cfg,false,result,1060) && result!=HoldRelease && result!=HoldPin);
        cfg.textInput=true;KeyMachine guarded;
        guarded.event(shift,true,true,cfg,true,result);guarded.event(20,true,true,cfg,true,result);
        check(!guarded.event(key,true,true,cfg,true,result) && result==None);
    }
    for(int shift:{VK_LSHIFT,VK_RSHIFT}) for(bool releaseShiftFirst:{false,true}) {
        Config cfg;cfg.enabled=true;cfg.bindings[0]={Toggle,{VK_SHIFT,90},2};KeyMachine machine;Action action;
        check(!machine.event(shift,true,true,cfg,false,action,1000));
        check(machine.event(90,true,true,cfg,false,action,1010) && action==HoldBegin);
        check(machine.event(90,true,true,cfg,false,action,1020) && action==None);
        if(releaseShiftFirst)check(!machine.event(shift,false,true,cfg,true,action,1030));
        check(machine.event(90,false,true,cfg,true,action,1100) && action==HoldPin);
        if(!releaseShiftFirst)check(!machine.event(shift,false,true,cfg,true,action,1110));
        KeyMachine outside;check(!outside.event(shift,true,false,cfg,false,action));
        check(!outside.event(90,true,false,cfg,false,action) && action==None);
    }
    Config cfg; cfg.bindings[0]={Toggle,{20},1}; cfg.bindings[1]={Select,{20,65},2};
    KeyMachine state; Action action;
    WinRightMachine remap;DWORD mouse=0;
    for (int i=0;i<10000;++i) {
        check(remap.event(true,true,mouse) && mouse==MOUSEEVENTF_RIGHTDOWN);
        check(remap.event(true,false,mouse) && mouse==0);
        check(remap.event(false,false,mouse) && mouse==MOUSEEVENTF_RIGHTUP);
    }
    check(!remap.event(true,false,mouse) && mouse==0);
    check(!remap.event(true,true,mouse) && mouse==0);
    check(!remap.event(false,true,mouse) && mouse==0);
    remap.physical=true; // Startup while already held: do not capture mid-press.
    check(!remap.event(true,true,mouse) && mouse==0);
    check(!remap.event(false,true,mouse) && mouse==0);
    KeyMachine dialog;
    check(dialog.event(20,true,true,cfg,false,action,1000,true) && action==None);
    check(dialog.event(20,true,true,cfg,false,action,1010,true) && action==None);
    check(!dialog.event(9,true,true,cfg,false,action,1020,true) && action==None);
    check(dialog.event(20,false,false,cfg,false,action,1030,true) && action==None);
    KeyMachine processCaps;
    check(routeKey(processCaps,20,false,true,true,cfg,false,action,900) && action==None);
    check(routeKey(processCaps,20,false,false,true,cfg,false,action,901) && action==None);
    check(!routeKey(processCaps,20,false,false,false,cfg,false,action,902) && action==None);
    Config noCaps;noCaps.bindings[0]={Toggle,{9},1};
    check(!routeKey(processCaps,20,false,true,true,noCaps,false,action,903));
    for (int i=0;i<10000;++i) {
        check(routeKey(processCaps,20,true,false,true,cfg,false,action,1000) && action==None);
        check(routeKey(processCaps,20,true,false,true,cfg,false,action,1010) && action==None);
        check(!routeKey(processCaps,65,true,false,true,cfg,false,action,1020) && action==None);
        check(!routeKey(processCaps,65,false,false,true,cfg,false,action,1025) && action==None);
        check(routeKey(processCaps,20,false,false,false,cfg,false,action,1030) && action==None);
    }
    check(routeKey(processCaps,20,true,true,true,cfg,false,action,1000) && action==HoldBegin);
    check(routeKey(processCaps,20,false,false,true,cfg,true,action,1050) && action==HoldCancel);
    check(!routeKey(processCaps,20,true,false,false,cfg,false,action,1100));
    check(!routeKey(processCaps,20,true,true,true,cfg,false,action,1120));
    check(!routeKey(processCaps,20,false,true,true,cfg,false,action,1130));
    check(routeKey(processCaps,20,true,true,true,cfg,false,action,1200) && action==HoldBegin);
    check(routeKey(processCaps,20,false,true,true,cfg,true,action,1250) && action==HoldPin);
    check(routeKey(processCaps,20,false,true,true,cfg,true,action,1260) && action==None);
    check(!routeKey(processCaps,20,true,false,false,cfg,false,action,1300));
    check(!routeKey(processCaps,20,false,true,true,cfg,false,action,1310) && action==None);
    KeyMachine cancelled;
    check(cancelled.event(20,true,true,cfg,false,action,1000) && action==HoldBegin);
    cancelled.cancelGesture();
    check(cancelled.event(20,false,true,cfg,false,action,1050) && action==None);
    check(cancelled.event(20,true,true,cfg,false,action,1200) && action==HoldBegin);
    check(cancelled.event(20,false,true,cfg,true,action,1250) && action==HoldPin);
    KeyMachine taps;
    check(taps.event(20,true,true,cfg,false,action,1000) && action==HoldBegin);
    check(taps.event(20,false,true,cfg,false,action,1050) && action==HoldPin);
    check(taps.event(20,true,true,cfg,false,action,1200) && action==HoldBegin);
    check(taps.event(20,true,true,cfg,true,action,1220) && action==None);
    check(taps.event(20,false,true,cfg,true,action,1250) && action==HoldPin);
    check(taps.event(20,true,true,cfg,false,action,1600) && action==HoldBegin);
    check(taps.event(20,false,true,cfg,true,action,2000) && action==HoldRelease);
    check(taps.event(20,true,true,cfg,true,action,2010) && action==Hide);
    check(taps.event(20,false,true,cfg,false,action,2050) && action==None);
    check(taps.event(20,true,true,cfg,false,action,2100) && action==HoldBegin);
    check(taps.event(20,false,false,cfg,false,action,2150) && action==HoldCancel);
    check(taps.event(20,true,true,cfg,false,action,2200) && action==HoldBegin);
    for (int i=0;i<10000;++i) {
        check(state.event(20,true,true,cfg,false,action) && action==HoldBegin);
        check(state.event(20,true,true,cfg,false,action) && action==None);
        check(state.event(20,false,true,cfg,false,action) && action==HoldRelease);
    }
    state.event(20,true,true,cfg,false,action);
    check(state.event(65,true,true,cfg,false,action) && action==Select);
    check(state.event(65,true,true,cfg,false,action) && action==None);
    check(state.event(65,false,true,cfg,false,action));
    check(state.event(20,false,true,cfg,false,action) && action==HoldCancel);
    state.event(20,true,true,cfg,false,action);
    check(state.event(20,false,false,cfg,false,action) && action==HoldCancel);
    cfg.textInput=true;
    state.event(20,true,true,cfg,false,action);
    check(!state.event(65,true,true,cfg,false,action) && action==None);
    state.event(65,false,true,cfg,false,action);
    check(state.event(20,false,true,cfg,false,action) && action==HoldCancel);
    cfg.textInput=false;
    check(!state.event(20,true,false,cfg,false,action));
    check(!state.event(20,false,false,cfg,false,action));
    state.event(162,true,true,cfg,false,action);
    check(state.event(20,true,true,cfg,false,action) && action==None);
    check(state.event(20,false,true,cfg,false,action) && action==None);
    state.event(163,true,true,cfg,false,action); state.event(162,false,true,cfg,false,action);
    check(state.modified()); state.event(163,false,true,cfg,false,action); check(!state.modified());
    for (int modifier:{160,161,162,163,164,165,91,92}) {
        KeyMachine modifiedState;
        modifiedState.event(modifier,true,true,cfg,false,action);
        check(modifiedState.event(20,true,true,cfg,false,action) && action==None);
        modifiedState.event(modifier,false,true,cfg,false,action);
        check(modifiedState.event(20,true,true,cfg,false,action) && action==None);
        check(modifiedState.event(20,false,false,cfg,false,action) && action==None);
    }
    // Presses begun outside PS stay passed through until release, even when
    // auto-repeat arrives after focus returns; no unmatched release is eaten.
    check(!state.event(20,true,false,cfg,false,action));
    check(!state.event(20,true,true,cfg,false,action) && action==None);
    check(!state.event(20,false,true,cfg,false,action));
    // Conversely a captured press remains captured through disable/rebind.
    check(state.event(20,true,true,cfg,false,action) && action==HoldBegin);
    Config empty;
    check(state.event(20,true,false,empty,false,action) && action==None);
    check(state.event(20,false,false,empty,false,action) && action==HoldCancel);
    check(!state.event(20,true,true,empty,false,action));
    check(!state.event(20,false,true,empty,false,action));
    Config paintConfig=cfg;paintConfig.colorEditor=true;
    KeyMachine paintKeys;
    check(paintKeys.event(20,true,true,paintConfig,false,action,3000) && action==HoldBegin);
    for(int local:{9,65,67,87,83,49,70}) {
        check(!paintKeys.event(local,true,true,paintConfig,true,action,3100) && action==None);
        check(!paintKeys.event(local,false,true,paintConfig,true,action,3200) && action==None);
    }
    check(paintKeys.event(20,false,true,paintConfig,true,action,3400) && action==HoldRelease);
    check(paintKeys.event(20,true,true,paintConfig,true,action,3500) && action==Toggle);
    check(paintKeys.event(20,false,true,paintConfig,true,action,3510) && action==None);
    cfg.bindings[0]={Toggle,{9},1}; cfg.bindings[1]={Top,{9,87},2};
    for (int i=0;i<10000;++i) {
        check(state.event(9,true,true,cfg,false,action) && action==HoldBegin);
        check(state.event(9,false,true,cfg,false,action) && action==HoldRelease);
    }
    state.event(9,true,true,cfg,false,action);
    check(state.event(87,true,true,cfg,false,action) && action==Top);
    state.event(87,false,true,cfg,false,action);
    check(state.event(9,false,true,cfg,false,action) && action==HoldCancel);
    std::cout<<"Native hotkey self-test passed: 10000 CapsLock and Tab pairs, repeat, scope, modifiers, combinations\n";
    return 0;
}
static int foregroundScopeSelfTest() {
    HWND window=CreateWindowExW(0,L"STATIC",L"Foreground scope test",WS_OVERLAPPEDWINDOW,0,0,240,120,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    HWND other=CreateWindowExW(0,L"STATIC",L"Foreground scope other",WS_OVERLAPPEDWINDOW,260,0,120,80,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    check(window && other);
    hookThread=GetCurrentThreadId();config.enabled=true;config.overlay=nullptr;
    for(auto& p:photoshopProcesses)if(p.handle)CloseHandle(p.handle);
    for(auto& p:nonPhotoshopProcesses)if(p.handle)CloseHandle(p.handle);
    photoshopProcesses={};nonPhotoshopProcesses={};
    try {
        // First non-PS foreground costs one query; a repeated foreground uses
        // the held negative handle with no OpenProcess/QueryFullProcessImageName.
        processQueries.store(0);
        check(!photoshopProcessWindow(window));
        const ULONG afterFirst=processQueries.load();
        for(int i=0;i<10000;++i)check(!photoshopProcessWindow(window));
        check(processQueries.load()==afterFirst);
        // Seen as the verified positive cache, the hook path must never query.
        HANDLE self=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,GetCurrentProcessId());
        check(self!=nullptr);
        photoshopProcesses[0]={GetCurrentProcessId(),self};
        check(photoshopProcessWindowFast(window));
        publishForegroundScope(window,true);
        processQueries.store(0);
        for(int i=0;i<10000;++i)check(photoshopKeyScope(window));
        check(processQueries.load()==0);
        check(GetKeyboardLayout(0)!=0);
        // A different foreground is conservative and wakes the message thread;
        // no query happens inside the hook itself.
        processQueries.store(0);foregroundRefreshPosted.store(false);
        check(!photoshopKeyScope(other));
        check(processQueries.load()==0);
        check(foregroundRefreshPosted.load());
        MSG pending{};check(PeekMessageW(&pending,nullptr,foregroundMessage,foregroundMessage,PM_REMOVE));
        check(pending.message==foregroundMessage && reinterpret_cast<HWND>(pending.lParam)==other);
        foregroundRefreshPosted.store(false);
        publishForegroundScope(window,false);
        check(!photoshopKeyScope(window));
        updateForegroundScope(window,true);
        check(photoshopKeyScope(window));
        // An exited held handle cannot keep a recycled PID verified.
        HANDLE ended=CreateEventW(nullptr,TRUE,TRUE,nullptr);check(ended!=nullptr);
        CloseHandle(photoshopProcesses[0].handle);photoshopProcesses[0]={GetCurrentProcessId(),ended};
        check(!photoshopProcessWindowFast(window));
        std::cout<<"Foreground scope self-test passed: one process query per non-PS foreground, 10000 hook decisions with zero queries, conservative mismatch refresh, exited-handle rejection\n";
    } catch(...) {
        for(auto& p:photoshopProcesses)if(p.handle)CloseHandle(p.handle);
        for(auto& p:nonPhotoshopProcesses)if(p.handle)CloseHandle(p.handle);
        photoshopProcesses={};nonPhotoshopProcesses={};
        DestroyWindow(other);DestroyWindow(window);throw;
    }
    for(auto& p:photoshopProcesses)if(p.handle)CloseHandle(p.handle);
    for(auto& p:nonPhotoshopProcesses)if(p.handle)CloseHandle(p.handle);
    photoshopProcesses={};nonPhotoshopProcesses={};
    DestroyWindow(other);DestroyWindow(window);return 0;
}
static int searchInputSelfTest() {
    HWND window=CreateWindowExW(0,L"STATIC",L"Search input protection test",WS_OVERLAPPEDWINDOW,0,0,240,140,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    check(window!=nullptr);
    try {
        HWND focus=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,0,0,40,40,window,nullptr,nullptr,nullptr);
        HWND edit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE,50,0,120,24,window,nullptr,nullptr,nullptr);
        check(focus && edit);ShowWindow(window,SW_SHOW);SetForegroundWindow(window);SetFocus(focus);check(GetForegroundWindow()==window);
        config.enabled=true;config.overlay=window;publishForegroundScope(window,true);
        check(!photoshopInputFocus(window));
        config.overlay=nullptr;check(photoshopInputFocus(window));
        Config cfg;cfg.searchAllowed=true;cfg.searchRoot=window;cfg.searchFocus=focus;cfg.searchEpoch=0;inputEpoch=0;
        check(resumeSearchContext(true,cfg,window));
        check(!resumeSearchContext(false,cfg,window));
        cfg.colorEditor=true;check(resumeSearchContext(true,cfg,window));cfg.colorEditor=false;
        cfg.textInput=true;check(!resumeSearchContext(true,cfg,window));cfg.textInput=false;
        cfg.overlay=window;check(!resumeSearchContext(true,cfg,window));cfg.overlay=nullptr;
        const auto route=[&](){
            Config local=cfg;local.searchAllowed=searchInputAllowed(cfg,window);
            KeyMachine machine;machine.seed(VK_CONTROL);Action action;const bool blocked=routeKey(machine,'F',true,true,true,local,false,action,GetTickCount64());
            check(machine.event('F',false,true,local,false,action)==blocked);return blocked;
        };
        check(route());
        HWND menu=CreateWindowExW(0,L"STATIC",L"Search return fixture",WS_OVERLAPPEDWINDOW,0,0,80,80,nullptr,nullptr,nullptr,nullptr);
        check(menu!=nullptr);cfg.overlay=menu;
        SetFocus(menu);check(GetFocus()==menu);check(route());
        ShowWindow(menu,SW_SHOW);check(!route());ShowWindow(menu,SW_HIDE);SetFocus(menu);check(route());
        cfg.textInput=true;check(!route());cfg.textInput=false;
        ++inputEpoch;check(!route());cfg.searchEpoch=inputEpoch;
        cfg.searchFocus=edit;check(!route());cfg.searchFocus=focus;
        SetFocus(edit);check(!route());SetFocus(focus);check(route());
        DestroyWindow(menu);cfg.overlay=nullptr;
        KeyMachine burst;Action action;
        for(int i=0;i<1000;++i) {
            cfg.searchEpoch=inputEpoch;
            for(int key:{'A','V','T'}) {
                check(!routeKey(burst,key,true,true,true,cfg,false,action,GetTickCount64()));
                check(!languageActivityKey(key,burst));
                check(!routeKey(burst,key,false,true,true,cfg,false,action,GetTickCount64()));
            }
            Config fresh=cfg;fresh.searchAllowed=searchInputAllowed(cfg,window);
            burst.seed(VK_CONTROL);
            check(routeKey(burst,'F',true,true,true,fresh,false,action,GetTickCount64()) && action==SearchLayers);
            check(routeKey(burst,'F',false,true,true,fresh,false,action,GetTickCount64()));
            burst.event(VK_CONTROL,false,true,fresh,false,action);
        }
        check(CreateCaret(focus,nullptr,1,14)!=0 && ShowCaret(focus)!=0);check(!route());check(resumeSearchContext(true,cfg,window));
        check(!languageActivityKey('A',burst));
        HideCaret(focus);cfg.searchAllowed=false;check(!route());
        cfg.searchAllowed=true;check(!route());DestroyCaret();check(route());
        ++inputEpoch;check(!route());check(!resumeSearchContext(true,cfg,window));
        for(int i=0;i<1000;++i)for(int key:{'A','V','T'})
            check(!languageActivityKey(key,burst));
        check(!route());
        check(!languageActivityKey('F',burst));
        cfg.searchEpoch=inputEpoch;check(route());
        SetFocus(edit);cfg.searchFocus=edit;cfg.searchEpoch=inputEpoch;check(!route());check(!resumeSearchContext(true,cfg,window));
        SetFocus(focus);cfg.searchFocus=edit;check(!route());check(!resumeSearchContext(true,cfg,window));
        cfg.searchFocus=focus;cfg.searchEpoch=inputEpoch;check(route());
        // Removing the host round trip must not authorise a single-key action
        // from a publication that predates the current generation or focus.
        // An explicit Caps chord stays deterministic in the same state.
        {
            Config stale=cfg;stale.searchAllowed=true;stale.searchRoot=window;stale.searchFocus=focus;
            stale.searchEpoch=inputEpoch-1;stale.evidenceFresh=false;
            KeyMachine machine;machine.seed(VK_CONTROL);
            check(!machine.event('F',true,true,stale,false,action) && action==None);
            machine.event(VK_CONTROL,false,true,stale,false,action);
            Config bindings;bindings.enabled=true;bindings.bindings[0]={Toggle,{VK_CAPITAL},1};
            bindings.bindings[1]={SelectAbove,{VK_CAPITAL,'W'},2};
            Action chordAction=None;
            check(machine.event(VK_CAPITAL,true,true,bindings,false,chordAction) && chordAction==HoldBegin);
            check(machine.event('W',true,true,bindings,false,chordAction) && chordAction==SelectAbove);
            // A stale publication also cannot make a pinned menu treat a bare
            // letter as a Caps chord.
            Config pinned=bindings;pinned.evidenceFresh=false;
            KeyMachine staleBare;
            check(!staleBare.event('W',true,true,pinned,true,action) && action==None);
            pinned.evidenceFresh=true;
            KeyMachine freshBare;
            check(freshBare.event('W',true,true,pinned,true,action) && action==SelectAbove);
        }
        DestroyWindow(window);std::cout<<"Search input self-test passed: bounded first-key decisions, stale publications cannot authorise a single-key action or a pinned bare letter, explicit chords stay deterministic, 1000 rapid A/V/T/F sequences, visible/hidden caret, native Edit, stale focus and input generations\n";return 0;
    } catch(...) {DestroyCaret();DestroyWindow(window);throw;}
}
static bool renewHooks(HHOOK& keyHook,HHOOK& mouseHook) {
    // Install replacements first. Preserve KeyMachine and mouseConsumed so a
    // held press keeps its original suppression decision through renewal.
    HHOOK nextKey=SetWindowsHookExW(WH_KEYBOARD_LL,keyboard,GetModuleHandleW(nullptr),0);
    HHOOK nextMouse=SetWindowsHookExW(WH_MOUSE_LL,pointer,GetModuleHandleW(nullptr),0);
    if (!nextKey || !nextMouse) {
        if (nextKey) UnhookWindowsHookEx(nextKey);
        if (nextMouse) UnhookWindowsHookEx(nextMouse);
        return false;
    }
    if (keyHook) UnhookWindowsHookEx(keyHook);
    if (mouseHook) UnhookWindowsHookEx(mouseHook);
    keyHook=nextKey;mouseHook=nextMouse;return true;
}
static void applySelfTestLowCpu() {
    DWORD_PTR available=0,system=0;
    if(!GetProcessAffinityMask(GetCurrentProcess(),&available,&system))throw std::runtime_error("self-test affinity");
    DWORD_PTR mask=0;int picked=0;
    for(int bit=int(sizeof(DWORD_PTR)*8)-1;bit>=0 && picked<2;--bit)
        if(system&(DWORD_PTR(1)<<bit)){mask|=DWORD_PTR(1)<<bit;++picked;}
    if(picked!=2 || !SetProcessAffinityMask(GetCurrentProcess(),mask))throw std::runtime_error("self-test low CPU affinity");
    if(!SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS))throw std::runtime_error("self-test low CPU priority");
    DWORD_PTR readback=0;int count=0;
    if(!GetProcessAffinityMask(GetCurrentProcess(),&readback,&system))throw std::runtime_error("self-test affinity readback");
    for(auto bits=readback;bits;bits&=bits-1)count++;
    if(count!=2 || readback!=mask || GetPriorityClass(GetCurrentProcess())!=BELOW_NORMAL_PRIORITY_CLASS)
        throw std::runtime_error("self-test low CPU not applied");
}
int main(int argc,char** argv) {
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);counterFrequency=frequency.QuadPart;
    bool selfTestLow=false;
    int kept=1;
    for(int i=1;i<argc;++i) {
        if(std::string(argv[i])=="--low")selfTestLow=true;
        else argv[kept++]=argv[i];
    }
    argc=kept;
    if(selfTestLow) {
        try{applySelfTestLowCpu();}
        catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
    }
    // MSLLHOOKSTRUCT uses physical per-monitor coordinates. GetWindowRect and
    // WindowFromPoint must use the same coordinate space at 125/150/200% DPI.
    auto dpi=reinterpret_cast<BOOL(WINAPI*)(HANDLE)>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext"));
    if (!dpi || !dpi(reinterpret_cast<HANDLE>(-4))) SetProcessDPIAware();
    if (argc==3 && std::string(argv[1])=="--inspect-window") {
        HWND hwnd=reinterpret_cast<HWND>(std::stoull(argv[2]));RECT rect{};
        if (!GetWindowRect(hwnd,&rect)) return 1;
        std::cout<<"["<<rect.left<<","<<rect.top<<","<<rect.right<<","<<rect.bottom<<"]\n";return 0;
    }
    if (argc==3 && std::string(argv[1])=="--inspect-workspace") {
        HWND hwnd=reinterpret_cast<HWND>(std::stoull(argv[2]));
        std::cout<<(photoshop(hwnd)?"true":"false")<<"\n";return 0;
    }
    if (argc==3 && std::string(argv[1])=="--inspect-win-scope") {
        HWND hwnd=reinterpret_cast<HWND>(std::stoull(argv[2]));
        refreshPhotoshopProcesses();
        std::cout<<(photoshopProcessWindow(hwnd)?"true":"false")<<"\n";return 0;
    }
    if (argc==2 && std::string(argv[1])=="--self-test") return selfTest();
    if (argc==2 && std::string(argv[1])=="--self-test-search-input") return searchInputSelfTest();
    if (argc==2 && std::string(argv[1])=="--self-test-foreground-scope") return foregroundScopeSelfTest();
    if(argc==4 && std::string(argv[1])=="--inspect-preview-guard") {
        refreshPhotoshopProcesses();config.guardOwner=reinterpret_cast<HWND>(std::stoull(argv[2]));config.guardPanel=reinterpret_cast<HWND>(std::stoull(argv[3]));
        std::cout<<(previewGuardWindow(config.guardPanel)?"true":"false")<<"\n";
        for(auto& p:photoshopProcesses)if(p.handle)CloseHandle(p.handle);
        return 0;
    }
    if (argc==3 && std::string(argv[1])=="--self-test-process-scope") {
        bool expected=std::string(argv[2])=="true";
        refreshPhotoshopProcesses();
        for (const wchar_t* name:{L"Photoshop",L"PSFloatC",L"FutureUnknownPSPopup"}) {
            WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=name;
            check(RegisterClassW(&wc)!=0);
            HWND hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,name,L"Isolated process scope test",
                WS_POPUP,-10000,-10000,10,10,nullptr,nullptr,wc.hInstance,nullptr);
            check(hwnd!=nullptr);
            check(photoshopProcessWindow(hwnd)==expected);
            ShowWindow(hwnd,SW_SHOWNOACTIVATE);
            check((remoteWorkspace(hwnd)!=nullptr)==(expected && wcscmp(name,L"Photoshop")==0));
            WNDCLASSW pc{};pc.lpfnWndProc=DefWindowProcW;pc.hInstance=wc.hInstance;pc.lpszClassName=L"PSViewC";RegisterClassW(&pc);
            HWND list=CreateWindowW(L"PSViewC",L"Owned layer list",WS_CHILD|WS_VISIBLE,0,0,10,10,hwnd,nullptr,wc.hInstance,nullptr);
            HWND row=CreateWindowW(L"STATIC",L"Owned row",WS_CHILD|WS_VISIBLE,0,0,5,5,list,nullptr,wc.hInstance,nullptr);
            config.guardOwner=hwnd;config.guardPanel=list;
            bool guarded=expected && wcscmp(name,L"Photoshop")==0;
            check(previewGuardWindow(list)==guarded && previewGuardWindow(row)==guarded);
            check(!previewGuardWindow(hwnd));
            config.guardPanel=nullptr;check(!previewGuardWindow(list));config.guardPanel=list;
            ShowWindow(list,SW_HIDE);check(!previewGuardWindow(list));ShowWindow(list,SW_SHOWNOACTIVATE);
            config.guardOwner=nullptr;config.guardPanel=nullptr;DestroyWindow(list);UnregisterClassW(L"PSViewC",wc.hInstance);
            EnableWindow(hwnd,FALSE);check(photoshopProcessWindow(hwnd)==expected);
            check(remoteWorkspace(hwnd)==nullptr);
            auto previous=photoshopProcesses;
            for (auto& p:photoshopProcesses) p={};
            HANDLE ended=CreateEventW(nullptr,TRUE,TRUE,nullptr);
            photoshopProcesses[0]={GetCurrentProcessId(),ended};
            check(!photoshopProcessWindow(hwnd));
            CloseHandle(ended);photoshopProcesses=previous;
            DestroyWindow(hwnd);check(!photoshopProcessWindow(hwnd));
            UnregisterClassW(name,wc.hInstance);
        }
        check(!photoshopProcessWindow(nullptr));
        for (auto& p:photoshopProcesses) if (p.handle) CloseHandle(p.handle);
        std::cout<<"Process scope self-test passed: classes, disabled windows, executable identity, stale PID\n";return 0;
    }
    if (argc==2 && std::string(argv[1])=="--self-test-hooks") {
        HHOOK key=nullptr,mouse=nullptr;
        Config cfg;cfg.bindings[0]={Toggle,{20},1};Action action;
        check(keys.event(20,true,true,cfg,false,action) && action==HoldBegin);
        for (int i=0;i<30;++i) {
            if (!renewHooks(key,mouse)) return 1;
            check(keys.event(20,true,true,cfg,false,action) && action==None);
            // Simulate Windows removing hooks without ending the host process.
            UnhookWindowsHookEx(key);UnhookWindowsHookEx(mouse);
        }
        if (!renewHooks(key,mouse)) return 1;
        check(keys.event(20,false,false,cfg,false,action) && action==HoldCancel);
        UnhookWindowsHookEx(key);UnhookWindowsHookEx(mouse);
        std::cout<<"Hook renewal passed: 30 silent-removal cycles, no input injection\n";return 0;
    }
    DWORD parent=argc==3 && std::string(argv[1])=="--parent"?std::stoul(argv[2]):0;
    hookThread=GetCurrentThreadId(); MSG message; PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE);
    frameParent=parent;frameBeginMessage=RegisterWindowMessageW(frameBeginName);frameCommitMessage=RegisterWindowMessageW(frameCommitName);frameRestoreMessage=RegisterWindowMessageW(frameRestoreName);frameReleaseMessage=RegisterWindowMessageW(frameReleaseName);
    WNDCLASSW frameClass{};frameClass.lpfnWndProc=frameProcedure;frameClass.hInstance=GetModuleHandleW(nullptr);frameClass.lpszClassName=frameEndpointClass;RegisterClassW(&frameClass);
    frameEndpoint=CreateWindowW(frameEndpointClass,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,frameClass.hInstance,nullptr);
    if(!frameEndpoint || !SetPropW(frameEndpoint,frameOwnerProp,reinterpret_cast<HANDLE>(ULONG_PTR(parent))))return 1;
    RAWINPUTDEVICE keyboardInput{1,6,RIDEV_INPUTSINK,frameEndpoint};
    if(!RegisterRawInputDevices(&keyboardInput,1,sizeof(keyboardInput)))return 1;

    available=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!available)return 1;
    for (int vk:{160,161,162,163,164,165,91,92}) if (GetAsyncKeyState(vk)&0x8000) keys.seed(vk);
    winRight.physical=(GetAsyncKeyState(VK_LWIN)&0x8000)!=0;
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);
    HHOOK keyHook=SetWindowsHookExW(WH_KEYBOARD_LL,keyboard,GetModuleHandleW(nullptr),0);
    HHOOK mouseHook=SetWindowsHookExW(WH_MOUSE_LL,pointer,GetModuleHandleW(nullptr),0);
    std::thread writer(writeActions);
    if (!keyHook || !mouseHook) {
        emit(Error); closing.store(true); SetEvent(available); writer.join();
        if (keyHook) UnhookWindowsHookEx(keyHook);
        if (mouseHook) UnhookWindowsHookEx(mouseHook);
        CloseHandle(available); return 1;
    }
    HWINEVENTHOOK languageForeground=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,languageFocusEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    HWINEVENTHOOK languageFocus=SetWinEventHook(EVENT_OBJECT_FOCUS,EVENT_OBJECT_FOCUS,nullptr,languageFocusEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    std::thread(readSettings).detach();
    if (parent) std::thread([parent]{
        HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,parent);
        if (process) { WaitForSingleObject(process,INFINITE); CloseHandle(process); }
        stop();
    }).detach();
    emit(Ready);
    sampleCapsState(true);
    // CapsLock can only change through a key this hook already sees, so a
    // quarter-second diagnostic sample is enough; 16 ms was a 62 Hz idle wake.
    UINT_PTR capsTimer=SetTimer(nullptr,0,250,nullptr);
    UINT_PTR renewal=SetTimer(nullptr,0,1000,nullptr);
    if (!renewal || !capsTimer) {stop();}
    unsigned renewalFailures=0,healthPasses=0;ULONGLONG reportedDrops=0;
    ULONGLONG previousMaintenance=GetTickCount64();
    while (GetMessageW(&message,nullptr,0,0)>0) {
        if (message.message==WM_TIMER && message.wParam==capsTimer) {
            HWND foreground=GetForegroundWindow();
            updateForegroundScope(foreground);
            observeActivation(foreground,photoshopKeyScopeSlow(foreground),GetTickCount64());
            sampleCapsState();
        } else if (message.message==winRightMessage) {
            if (message.wParam==MOUSEEVENTF_RIGHTDOWN) {
                // Do not send a new click to a window that gained foreground
                // after the physical down. Ups still complete any sent down.
                if (!rightInjected && GetForegroundWindow()==reinterpret_cast<HWND>(message.lParam) && photoshopKeyScopeSlow(GetForegroundWindow())) {
                    rightInjected=injectRight(MOUSEEVENTF_RIGHTDOWN);
                    if (!rightInjected) emit(Error);
                }
            } else if (message.wParam==MOUSEEVENTF_RIGHTUP && rightInjected) {
                if (injectRight(MOUSEEVENTF_RIGHTUP)) rightInjected=false;
                else emit(Error);
            }
        } else if (message.message==WM_TIMER && message.wParam==renewal) {
            auto now=GetTickCount64(),started=microseconds();
            if(now-lastInputTick>=250)keys.reconcileModifiers(physicalModifiers());
            bool mouseHeld=sampleClicks.held[0] || sampleClicks.held[1];for (bool held:mouseConsumed) mouseHeld=mouseHeld || held;
            bool attempted=renewalAllowed(now,lastInputTick,keys.physical[VK_CAPITAL] || keys.physical['F'] || keys.modified(),winRight.physical,mouseHeld);
            // Unhooking while Windows has pending hook calls can lose an
            // event. Keep the same hook throughout holds and rapid input.
            bool renewed=!attempted || renewHooks(keyHook,mouseHook);
            auto finished=microseconds();
            if (renewed) renewalFailures=0;
            else {emit(Error);if (++renewalFailures>=3) stop();}
            const auto dropped=droppedEvents.load(std::memory_order_relaxed);
            // Report every two seconds (the diagnostics liveness window is
            // 3.5 s), or at once when something got worse. One health line per
            // second per idle host was pure log noise.
            bool report=!renewed || dropped!=reportedDrops || (++healthPasses%2)==0;
            if(report){
                unsigned next=(head.load(std::memory_order_relaxed)+1)%queue.size();
                if (next!=tail.load(std::memory_order_acquire)) {
                    HookEvent event{};event.action=HookHealth;event.scanUs=lastScanUs;event.renewUs=finished-started;event.attempted=attempted;
                    event.inputEpoch=inputEpoch;
                    event.frameId=keys.frameId;event.framePhase=keys.framePhase;
                    event.gapMs=now-previousMaintenance;event.peakUs=callbackPeakUs.exchange(0,std::memory_order_relaxed);
                    event.dropped=dropped;event.blocked=renewed;
                    queue[head.load(std::memory_order_relaxed)]=event;
                    head.store(next,std::memory_order_release);SetEvent(available);
                } else droppedEvents.fetch_add(1,std::memory_order_relaxed);
                reportedDrops=dropped;
            } else callbackPeakUs.store(0,std::memory_order_relaxed);
            previousMaintenance=now;
            sampleCapsState();
        } else if (message.message==WM_APP) {
            std::unique_ptr<Config> next(reinterpret_cast<Config*>(message.lParam));
            const bool beginDiagnostic=next->diagnostic && !config.diagnostic;
            if(!next->enabled || !next->remoteCompatibility)remoteClick.workspace=0;
            config=*next;
            if(beginDiagnostic)diagnosticUntil=GetTickCount64()+35000;
            else if(!config.diagnostic)diagnosticUntil=0;
            if (!IsWindowVisible(config.overlay)) dismissPending=false;
            updateForegroundScope(GetForegroundWindow(),true);
            emit(Configured);
        } else if (message.message==foregroundMessage) {
            foregroundRefreshPosted.store(false,std::memory_order_release);
            updateForegroundScope(reinterpret_cast<HWND>(message.lParam),true);
        } else if (message.message==WM_APP+1) emit(Error);
        else { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if (renewal) KillTimer(nullptr,renewal);
    if (capsTimer) KillTimer(nullptr,capsTimer);
    if (languageForeground) UnhookWinEvent(languageForeground);
    if (languageFocus) UnhookWinEvent(languageFocus);
    closing.store(true);
    UnhookWindowsHookEx(keyHook); UnhookWindowsHookEx(mouseHook);
    if (rightInjected) injectRight(MOUSEEVENTF_RIGHTUP);
    for (auto& p:photoshopProcesses) if (p.handle) CloseHandle(p.handle);
    closing.store(true); SetEvent(available); writer.join();
    // Detached pipe/parent threads are terminated with this process.
    return 0;
}
