// Test-only, observe-only PS UI-thread probe. Never packaged or used to route F.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <initializer_list>

static constexpr wchar_t probeClass[]=L"PhotoshopMenu.ThreadFProbe.v1";
static constexpr wchar_t protocol[]=L"PhotoshopMenu.ThreadFProbe.Control.v1";
static constexpr unsigned capacity=4096;
struct Event {
    LONGLONG tick;
    unsigned stage,message,flags,epoch,entryNewer;
    ULONG_PTR focus,layout;
};
static Event events[capacity]{};
static unsigned count=0;
static HMODULE module=nullptr;
static HWND control=nullptr,root=nullptr;
static HHOOK keyboardHook=nullptr,queueHook=nullptr,callHook=nullptr;
static HANDLE owner=nullptr;
static UINT command=0;
static ULONGLONG expires=0;
static LARGE_INTEGER frequency{};
static wchar_t output[32768]{};

static bool running(){return keyboardHook || queueHook || callHook;}
static void record(unsigned stage,unsigned message,unsigned flags) {
    if(!running())return;
    // Saturate instead of silently overwriting early events; report the loss.
    if(count>=capacity){++count;return;}
    const auto context=GetPropW(root,L"PhotoshopMenu.InputContext.Window.v1");
    auto& event=events[count++];LARGE_INTEGER tick{};QueryPerformanceCounter(&tick);
    event={tick.QuadPart,stage,message,flags,
        unsigned(reinterpret_cast<ULONG_PTR>(GetPropW(HWND(context),L"PhotoshopMenu.InputContext.Canvas.Epoch.v1"))),
        unsigned(reinterpret_cast<ULONG_PTR>(GetPropW(HWND(context),L"PhotoshopMenu.InputContext.Canvas.EntryTick.v1"))>
                 reinterpret_cast<ULONG_PTR>(GetPropW(HWND(context),L"PhotoshopMenu.InputContext.Canvas.ExitTick.v1"))),
        reinterpret_cast<ULONG_PTR>(GetFocus()),reinterpret_cast<ULONG_PTR>(GetKeyboardLayout(0))};
}
static bool interesting(UINT message,WPARAM value) {
    if(message==WM_KEYDOWN || message==WM_KEYUP || message==WM_SYSKEYDOWN || message==WM_SYSKEYUP)
        return value=='F' || value==VK_PROCESSKEY;
    // Retain only whether the character was F; never record text or IME strings.
    if(message==WM_CHAR || message==WM_SYSCHAR)return value=='f' || value=='F';
    return message==WM_SETFOCUS || message==WM_KILLFOCUS || message==WM_IME_STARTCOMPOSITION ||
           message==WM_IME_ENDCOMPOSITION || message==WM_IME_COMPOSITION || message==WM_INPUTLANGCHANGE;
}
static unsigned flags(UINT message,WPARAM value,LPARAM bits) {
    if(message>=WM_KEYFIRST && message<=WM_KEYLAST)
        return (unsigned(bits)&0xc000ffffU) | (value==VK_PROCESSKEY?0x1000000U:0);
    return 0;
}
static LRESULT CALLBACK keyboard(int code,WPARAM key,LPARAM bits) {
    if((code==HC_ACTION || code==HC_NOREMOVE) && key=='F')
        record(1,code,unsigned(bits)&0xc000ffffU);
    return CallNextHookEx(nullptr,code,key,bits);
}
static LRESULT CALLBACK queued(int code,WPARAM remove,LPARAM data) {
    if(code>=0) {
        const auto& message=*reinterpret_cast<MSG*>(data);
        if(interesting(message.message,message.wParam))
            record(remove==PM_REMOVE?3:2,message.message,flags(message.message,message.wParam,message.lParam));
    }
    return CallNextHookEx(nullptr,code,remove,data);
}
static LRESULT CALLBACK sent(int code,WPARAM param,LPARAM data) {
    if(code>=0) {
        const auto& message=*reinterpret_cast<CWPSTRUCT*>(data);
        if(interesting(message.message,message.wParam))
            record(4,message.message,flags(message.message,message.wParam,message.lParam));
    }
    return CallNextHookEx(nullptr,code,param,data);
}
static bool stop() {
    for(auto hook:{&keyboardHook,&queueHook,&callHook})
        if(*hook && UnhookWindowsHookEx(*hook))*hook=nullptr;
    if(!running()) {
        KillTimer(control,1);
        if(owner){CloseHandle(owner);owner=nullptr;}
    }
    return !running();
}
static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM operation,LPARAM value) {
    if(message==command) {
        if(operation==1) {
            // One observation per PS lifetime: a pinned older build is never reused.
            if(expires || running() || value<=0)return 0;
            owner=OpenProcess(SYNCHRONIZE,FALSE,DWORD(value));
            if(!owner || WaitForSingleObject(owner,0)!=WAIT_TIMEOUT){stop();return 0;}
            expires=GetTickCount64()+30000;
            if(!SetTimer(control,1,100,nullptr)){stop();return 0;}
            const auto thread=GetCurrentThreadId();
            keyboardHook=SetWindowsHookExW(WH_KEYBOARD,keyboard,module,thread);
            queueHook=SetWindowsHookExW(WH_GETMESSAGE,queued,module,thread);
            callHook=SetWindowsHookExW(WH_CALLWNDPROC,sent,module,thread);
            if(!keyboardHook || !queueHook || !callHook){stop();return 0;}
            return 1;
        }
        if(operation==2)return stop()?1:0;
        if(operation==3) {
            if(running())return -1;
            FILE* file=_wfopen(output,L"wb");if(!file)return -1;
            std::fprintf(file,"frequency\t%lld\ncount\t%u\nlost\t%u\n",frequency.QuadPart,count,count>capacity?count-capacity:0);
            for(unsigned i=0;i<count && i<capacity;++i) {
                const auto& event=events[i];
                std::fprintf(file,"%u\t%lld\t%u\t%u\t%u\t%u\t%u\t%llu\t%llu\n",i,event.tick,event.stage,event.message,
                    event.flags,event.epoch,event.entryNewer,(unsigned long long)event.focus,(unsigned long long)event.layout);
            }
            const bool written=!std::ferror(file);const int closed=std::fclose(file);
            return written && closed==0?LRESULT(count):-1;
        }
        if(operation==4)return running()?1:0;
        return 0;
    }
    if(message==WM_TIMER && (GetTickCount64()>=expires || !owner || WaitForSingleObject(owner,0)!=WAIT_TIMEOUT))stop();
    return DefWindowProcW(window,message,operation,value);
}
static bool initialize(HWND window) {
    if(control)return true;
    DWORD pid=0;const auto thread=GetWindowThreadProcessId(window,&pid);
    wchar_t name[64]{};GetClassNameW(window,name,64);
    if(pid!=GetCurrentProcessId() || thread!=GetCurrentThreadId() || wcscmp(name,L"Photoshop")!=0 ||
       window!=GetAncestor(window,GA_ROOTOWNER))return false;
#ifndef THREAD_F_PROBE_SELF_TEST
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);
    const auto executableName=wcsrchr(path,L'\\');
    if(_wcsicmp(executableName?executableName+1:path,L"Photoshop.exe")!=0)return false;
#endif
    const auto length=GetModuleFileNameW(module,output,32768);
    const auto slash=wcsrchr(output,L'\\');
    if(!length || length>=32768 || !slash || slash-output>32720)return false;
    wcscpy(slash+1,L"thread-f-events.tsv");
    command=RegisterWindowMessageW(protocol);if(!command)return false;
    // The control WndProc remains callable after unhooking; keep the DLL dormant.
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&initialize),&pinned))return false;
    WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=module;cls.lpszClassName=probeClass;
    if(!RegisterClassW(&cls))return false;
    root=window;QueryPerformanceFrequency(&frequency);
    control=CreateWindowExW(0,probeClass,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,module,nullptr);
    return control!=nullptr;
}
extern "C" __declspec(dllexport) LRESULT CALLBACK ThreadFLoader(int code,WPARAM param,LPARAM data) {
    if(code>=0 && !control)initialize(reinterpret_cast<CWPSTRUCT*>(data)->hwnd);
    return CallNextHookEx(nullptr,code,param,data);
}
#ifndef THREAD_F_PROBE_SELF_TEST
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);}
    return TRUE;
}
#endif
