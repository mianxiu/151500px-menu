// Isolated Win32 prototype, using the real InputContext policy on its own HWNDs.
// No hook is installed into Photoshop and no input is injected by this program.
#define INPUT_CONTEXT_SELF_TEST
#include <cstdio>
#include <initializer_list>
#include "../native/InputContext.cpp"
#include "../native/ThreadFKey.h"
#include <cstring>
#include <stdexcept>

static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static void pairingTests() {
    const ThreadFTicket ticket{1,2,3,4};ThreadFKey route;unsigned sent=0;
    const auto notify=[&](){++sent;return true;};
    check(!route.arm(ticket,true),"do not arm while F is physically held");
    check(!route.arm({1,2,0,4},false) && route.arm(ticket,false),"valid identity required");
    check(!route.key(-1,'F',1,ticket,true,true,notify) && !route.key(3,'F',1,ticket,true,true,notify) && sent==0 && !route.owned,"negative/NOREMOVE must not claim or notify");
    for(const auto current:{ThreadFTicket{9,2,3,4},ThreadFTicket{1,9,3,4},ThreadFTicket{1,2,9,4},ThreadFTicket{1,2,3,9}})
        check(!route.key(0,'F',1,current,true,true,notify),"stale identity must pass");
    check(!route.key(0,'F',1,ticket,false,true,notify) && !route.key(0,'F',1,ticket,true,false,notify),"input/unknown/modifiers must pass");
    check(!route.key(0,'F',0x40000001,ticket,true,true,notify) && !route.key(0,'F',0xc0000001,ticket,true,true,notify),"unowned repeat/up must pass");
    check(route.key(0,'F',1,ticket,true,true,notify) && sent==1,"one notification per accepted down");
    check(!route.stop() && !route.arm(ticket,false),"stop and rearm must wait for owned release");
    check(route.key(0,'F',0x40000001,{},false,false,notify) && route.key(0,'F',0xc0000001,{},false,false,notify) && sent==1,"owned repeat/up drain despite changed input evidence");
    check(route.stop() && !route.key(0,'F',1,ticket,true,true,notify),"stopped route cannot consume new down");
    check(route.arm(ticket,false) && !route.key(0,'F',1,ticket,true,true,[](){return false;}) && !route.owned && !route.armed,"failed post must not swallow or retry");
    check(route.arm(ticket,false) && route.key(0,'F',1,ticket,true,true,notify),"focus-leave setup");
    route.focusLeft();check(!route.owned && !route.armed && !route.key(0,'F',0xc0000001,ticket,true,true,notify),"focus leave cannot consume another app's release");
    // Simulate the two consumers in their actual order: global, then UI thread.
    for(unsigned cycle=0;cycle<1000;++cycle) {
        ThreadFKey global,local;unsigned globals=0,locals=0;
        check(global.arm(ticket,false) && local.arm(ticket,false),"handoff arm");
        bool published=false;
        const auto feed=[&](std::uint32_t bits){
            if(global.key(0,'F',bits,ticket,!published,true,[&](){++globals;return true;}))return true;
            return local.key(0,'F',bits,ticket,published,true,[&](){++locals;return true;});
        };
        check(feed(1),"old global down");published=true;
        check(feed(0x40000001) && feed(0xc0000001) && globals==1 && locals==0,"old pair stays global during publish");
        check(feed(1) && feed(0x40000001) && globals==1 && locals==1,"new pair belongs to thread only");
        check(!local.stop(),"unpublish waits for local owned pair");
        check(feed(0xc0000001) && local.stop(),"thread drain before unpublish");published=false;
        check(feed(1) && feed(0xc0000001) && globals==2 && locals==1,"safe return to global");
    }
    std::puts("Thread F pairing: 1000 exclusive handoffs, stale identities, input/modifiers, NOREMOVE, repeats, post failure, drain and focus leave passed");
}

static HWND routeRoot=nullptr,routeCanvas=nullptr,routeEdit=nullptr;
static HHOOK routeHook=nullptr,contextCall=nullptr,contextQueue=nullptr;
static ThreadFKey route;
static unsigned notifications=0,accepted=0,rawDown=0,rawUp=0,peeks=0,keyboardCalls=0,rejected=0;
static bool closing=false;
static unsigned routeGeneration=1;
static FILE* report=nullptr;
struct RouteEvent {int code;unsigned bits,consumed,sequence;std::uint64_t epoch;LONGLONG tick,cost;};
static RouteEvent routeEvents[4096]{};
static unsigned routeEventCount=0;
static HIMC originalCanvas=nullptr,originalEdit=nullptr;
static bool restoredFixture=false;
static bool closeFixture() {
    closing=true;if(!route.stop())return false;
    bool unhooked=true;
    for(auto hook:{&routeHook,&contextQueue,&contextCall})
        if(*hook){if(UnhookWindowsHookEx(*hook))*hook=nullptr;else unhooked=false;}
    SendMessageW(control,controlMessage,(nonce<<8)|5,0);
    restoredFixture=unhooked && !focusPolicy && !focusCount && associated(routeCanvas)==originalCanvas && associated(routeEdit)==originalEdit;
    restoredFixture=detach() && restoredFixture;
    DestroyWindow(routeRoot);return true;
}
static ThreadFTicket currentTicket(){return {reinterpret_cast<std::uintptr_t>(root),reinterpret_cast<std::uintptr_t>(GetFocus()),nonce,(std::uint64_t(routeGeneration)<<32)|canvasEpoch};}
static bool routeEligible() {
    GUITHREADINFO info{};info.cbSize=sizeof(info);const auto focus=GetFocus();
    return GetForegroundWindow()==root && protectedFocus(focus) && focusPolicy && focusIndex(focus)>=0 &&
        !policyRestoreFailed && !associated(focus) && !canvasTextEditing && !entryPending && !compositionActive &&
        typeToolKnown && !typeToolActive && GetGUIThreadInfo(0,&info) && !info.hwndCaret && !info.hwndCapture && !info.hwndMenuOwner && !info.hwndMoveSize;
}
static void title() {
    wchar_t value[160]{};
    wsprintfW(value,L"Thread F isolated fixture | sent %u accepted %u raw %u/%u peeks %u",notifications,accepted,rawDown,rawUp,peeks);
    SetWindowTextW(routeRoot,value);
}
static LRESULT CALLBACK routeKeyboard(int code,WPARAM key,LPARAM bits) {
    LARGE_INTEGER started{},finished{};QueryPerformanceCounter(&started);
    if(key=='F' && code>=0){++keyboardCalls;if(code==HC_NOREMOVE)++peeks;}
    const bool plain=!(GetKeyState(VK_CONTROL)&0x8000) && !(GetKeyState(VK_MENU)&0x8000) && !(GetKeyState(VK_SHIFT)&0x8000) && !(GetKeyState(VK_CAPITAL)&0x8000) && !(GetKeyState(VK_LWIN)&0x8000) && !(GetKeyState(VK_RWIN)&0x8000);
    const bool consumed=route.key(code,unsigned(key),std::uint32_t(bits),currentTicket(),routeEligible(),plain,[](){
        if(!PostMessageW(routeRoot,WM_APP+83,nonce,LPARAM(currentTicket().epoch)))return false;
        ++notifications;return true;
    });
    QueryPerformanceCounter(&finished);
    if(key=='F' && code>=0 && routeEventCount<4096)
        routeEvents[routeEventCount++]={code,unsigned(bits),unsigned(consumed),notifications,currentTicket().epoch,started.QuadPart,finished.QuadPart-started.QuadPart};
    if(consumed)return 1;
    return CallNextHookEx(nullptr,code,key,bits);
}
static LRESULT CALLBACK routeWindow(HWND window,UINT message,WPARAM param,LPARAM data) {
    if(message==WM_LBUTTONDOWN && window==routeCanvas)SetFocus(routeCanvas);
    if(message==WM_KEYDOWN && param=='F')++rawDown;
    if(message==WM_KEYUP && param=='F')++rawUp;
    if(message==WM_APP+83) {
        if(param==nonce && std::uint64_t(data)==currentTicket().epoch && route.armed && route.granted==currentTicket() && routeEligible())++accepted;
        else ++rejected;
        title();return 0;
    }
    if(message==WM_KILLFOCUS && window==routeCanvas){route.focusLeft();if(++routeGeneration==0)++routeGeneration;}
    if(message==WM_SETFOCUS && window==routeCanvas) {
        if(++routeGeneration==0)++routeGeneration;
        if(focusPolicy && !route.armed && !route.owned)route.arm(currentTicket(),(GetAsyncKeyState('F')&0x8000)!=0);
    }
    if(message==WM_TIMER && window==routeRoot) {
        if(param==2){closing=true;route.stop();}
        if(closing && !route.owned){closeFixture();return 0;}
        title();return 0;
    }
    if(message==WM_CLOSE && window==routeRoot){closeFixture();return 0;}
    if(message==WM_DESTROY && window==routeRoot){PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,param,data);
}
static void windowChecks() {
    check(routeEligible(),"isolated foreground/focus not verified; no input checks performed");
    const auto pump=[](){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_QUIT){PostQuitMessage(0);break;}TranslateMessage(&message);DispatchMessageW(&message);}};
    check(routeKeyboard(HC_NOREMOVE,'F',1)==0 && routeKeyboard(HC_NOREMOVE,'F',1)==0 && notifications==0,"peeks must not notify");
    check(routeKeyboard(HC_ACTION,'F',1)==1 && routeKeyboard(HC_ACTION,'F',0x40000001)==1 && notifications==1,"consume down/repeat once");
    check(routeKeyboard(HC_ACTION,'F',0xc0000001)==1,"consume matching release");pump();
    check(accepted==1 && rawDown==0 && rawUp==0,"one asynchronous local notification");
    SetFocus(routeEdit);
    check(!routeEligible() && !route.armed && !focusCount && associated(routeCanvas)==originalCanvas && associated(routeEdit)==originalEdit,"standard Edit restores IME and cancels F grant");
    check(routeKeyboard(HC_ACTION,'F',1)==0 && notifications==1,"F in Edit must pass");
    SetFocus(routeCanvas);check(routeEligible() && route.armed,"canvas return re-arms a new generation");
    check(routeKeyboard(HC_ACTION,'F',1)==1,"delayed notification setup");
    SetFocus(routeEdit);SetFocus(routeCanvas);pump();
    check(accepted==1 && rejected==1 && !route.owned,"old notification cannot execute after same-canvas return");
    SendMessageW(routeCanvas,WM_IME_STARTCOMPOSITION,0,0);
    check(compositionActive && !routeEligible() && routeKeyboard(HC_ACTION,'F',1)==0,"composition signal preserves F");
    SendMessageW(routeCanvas,WM_IME_ENDCOMPOSITION,0,0);
    SendMessageW(control,controlMessage,(nonce<<8)|6,1);
    SendMessageW(routeCanvas,WM_LBUTTONDOWN,0,0);
    check(canvasTextEditing && !routeEligible() && associated(routeCanvas)==originalCanvas && routeKeyboard(HC_ACTION,'F',1)==0,"canvas editing restores IME and passes F");
    SendMessageW(routeCanvas,WM_KEYDOWN,VK_ESCAPE,0);pump();
    SendMessageW(control,controlMessage,(nonce<<8)|6,0);
    check(!canvasTextEditing && routeEligible() && !route.key(0,'F',1,currentTicket(),true,true,[](){return true;}),"old canvas epoch cannot authorize exit first key");
    check(route.stop() && route.arm(currentTicket(),false),"fresh native exit epoch re-arms");
    check(routeKeyboard(HC_ACTION,'F',1)==1 && !route.stop(),"stop must wait for owned pair");pump();
    check(rejected==2 && accepted==1,"stop cancels queued notification");
    check(routeKeyboard(HC_ACTION,'F',0x40000001)==1 && routeKeyboard(HC_ACTION,'F',0xc0000001)==1 && route.stop(),"drain despite disabled grant");
    check(closeFixture(),"normal fixture close after drain");
    std::puts("Window checks: real InputContext IMM restore, Edit/IME/canvas guards, native epoch invalidation, stale PostMessage rejection and drained exit passed; F callbacks invoked directly, no keyboard input injected");
}
static void interactive(const wchar_t* path,bool automated) {
    report=_wfopen(path,L"wb");check(report,"open isolated report");
    module=GetModuleHandleW(nullptr);
    attachMessage=RegisterWindowMessageW(L"PhotoshopMenu.InputContext.Attach.v1");
    controlMessage=RegisterWindowMessageW(L"PhotoshopMenu.InputContext.Control.v1");
    for(const auto name:{L"Photoshop",L"PSViewC"}) {
        WNDCLASSW cls{};cls.lpfnWndProc=routeWindow;cls.hInstance=module;cls.lpszClassName=name;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hbrBackground=HBRUSH(COLOR_WINDOW+1);
        check(RegisterClassW(&cls),"isolated fixture class");
    }
    routeRoot=CreateWindowW(L"Photoshop",L"Thread F isolated fixture",WS_OVERLAPPEDWINDOW,40,40,760,320,nullptr,nullptr,module,nullptr);
    check(routeRoot,"isolated fixture root");
    routeCanvas=CreateWindowW(L"PSViewC",L"Non-input canvas",WS_CHILD|WS_VISIBLE|WS_BORDER,20,25,700,110,routeRoot,nullptr,module,nullptr);
    routeEdit=CreateWindowW(L"Edit",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,20,175,700,32,routeRoot,nullptr,module,nullptr);
    check(routeCanvas && routeEdit,"isolated canvas and Edit");
    originalCanvas=associated(routeCanvas);originalEdit=associated(routeEdit);
    check(attach(routeRoot,GetCurrentProcessId(),12345),"real InputContext attach to isolated fixture");
    contextCall=SetWindowsHookExW(WH_CALLWNDPROC,InputContextHook,module,GetCurrentThreadId());
    contextQueue=SetWindowsHookExW(WH_GETMESSAGE,InputContextQueueHook,module,GetCurrentThreadId());
    routeHook=SetWindowsHookExW(WH_KEYBOARD,routeKeyboard,module,GetCurrentThreadId());
    check(contextCall && contextQueue && routeHook,"isolated thread hooks");
    ShowWindow(routeRoot,SW_SHOW);SetForegroundWindow(routeRoot);SetFocus(routeCanvas);
    SendMessageW(control,controlMessage,(nonce<<8)|5,1);SendMessageW(control,controlMessage,(nonce<<8)|6,0);
    check(route.arm(currentTicket(),(GetAsyncKeyState('F')&0x8000)!=0),"arm isolated thread route");
    const auto initialLayout=GetKeyboardLayout(0);DWORD_PTR affinity=0,system=0;
    check(GetProcessAffinityMask(GetCurrentProcess(),&affinity,&system),"read actual fixture limits");
    std::fprintf(report,"start pid=%lu root=%llu layout=%llu affinity=%llu priority=%lu samples=%s\n",GetCurrentProcessId(),(unsigned long long)routeRoot,(unsigned long long)initialLayout,(unsigned long long)affinity,GetPriorityClass(GetCurrentProcess()),automated?"direct-callback":"windows-hook");std::fflush(report);
    if(automated)windowChecks();
    else {SetTimer(routeRoot,1,100,nullptr);SetTimer(routeRoot,2,60000,nullptr);}
    MSG message{};
    for(;;) {
        // Exercise real repeated PeekMessage observations before removal.
        PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE);PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE);
        const auto result=GetMessageW(&message,nullptr,0,0);
        check(result>=0,"fixture message pump");if(!result)break;
        TranslateMessage(&message);DispatchMessageW(&message);
    }
    check(route.stop(),"cannot exit while a consumed pair is outstanding");
    check(restoredFixture,"restore original associations and unhook before destroying fixture");
    if(automated)check(GetKeyboardLayout(0)==initialLayout,"window checks must preserve selected layout");
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    std::fprintf(report,"frequency %lld\n",frequency.QuadPart);
    for(unsigned i=0;i<routeEventCount;++i) {
        const auto& e=routeEvents[i];
        std::fprintf(report,"key %d %08x %u %u %llu %lld %lld\n",e.code,e.bits,e.consumed,e.sequence,(unsigned long long)e.epoch,e.tick,e.cost);
    }
    std::fprintf(report,"final sent=%u accepted=%u raw_down=%u raw_up=%u peeks=%u callbacks=%u rejected=%u normal_exit=1 restored=1 layout=%llu\n",notifications,accepted,rawDown,rawUp,peeks,keyboardCalls,rejected,(unsigned long long)GetKeyboardLayout(0));
    std::fclose(report);report=nullptr;
}
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc>=2 && wcscmp(argv[1],L"--low")==0) {
            DWORD_PTR available=0,system=0,selected=0;check(GetProcessAffinityMask(GetCurrentProcess(),&available,&system),"read affinity");
            for(unsigned bit=0;bit<sizeof(available)*8 && __builtin_popcountll(selected)<2;++bit)if(available&(DWORD_PTR(1)<<bit))selected|=DWORD_PTR(1)<<bit;
            check(SetProcessAffinityMask(GetCurrentProcess(),selected) && SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS),"limit fixture only");
            --argc;++argv;
        }
        pairingTests();
        if(argc==3 && (wcscmp(argv[1],L"--interactive")==0 || wcscmp(argv[1],L"--window-check")==0))
            interactive(argv[2],wcscmp(argv[1],L"--window-check")==0);
        return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());if(report){std::fprintf(report,"failure %s\n",error.what());std::fclose(report);}return 1;}
}
