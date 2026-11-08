// Independent hook-cost measurement harness. It statically includes one
// HotkeyHost.cpp revision (0.4.311 baseline or the current candidate), installs
// the real low-level hooks, and measures each callback around CallNextHookEx.
// A separate measurement hook is installed after the target so it runs first
// in the chain and its CallNextHookEx includes the target callback.
//
// Synthetic input is sent only to a self-owned placeholder window or to a
// foreground window independently verified as Photoshop.exe. No clicks are
// sent, no service entry is replaced, and CPU affinity/priority are restored.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifndef HOTKEY_MEASURE_SOURCE
#error "Define HOTKEY_MEASURE_SOURCE to the HotkeyHost.cpp revision under test"
#endif
#define main hotkey_host_original_main
#include HOTKEY_MEASURE_SOURCE
#undef main

static const wchar_t* measureClass=L"PhotoshopMenu.HookMeasure.v1";
static constexpr int measureMaxSamples=8192;
static ULONGLONG measureMouseSamples[measureMaxSamples]{};
static ULONGLONG measureKeySamples[measureMaxSamples]{};
static std::atomic<unsigned> measureMouseCount{0},measureKeyCount{0};
static std::atomic<bool> measureCollect{false},measureDone{false};
static LARGE_INTEGER measureFrequency{};
static HWND measureWindow=nullptr;
static DWORD measurePid=0;
static HWND measurePreviousForeground=nullptr;
static POINT measurePreviousCursor{};
static bool measureCursorSaved=false;
static DWORD_PTR measureOriginalAffinity=0,measureAppliedAffinity=0;
static DWORD measureOriginalPriority=0;
static bool measureLow=false,measureBaseline=false;

// Called by the instrumented CallbackTiming just before the target hook's own
// CallNextHookEx, so older hooks (including the still-running service) are not
// part of the measured callback time.
void measureHookSample(unsigned long long elapsed,int kind) {
    if(!measureCollect.load(std::memory_order_relaxed))return;
    if(kind==2) {
        const auto index=measureMouseCount.fetch_add(1,std::memory_order_relaxed);
        if(index<measureMaxSamples)measureMouseSamples[index]=elapsed;
    } else if(kind==1) {
        const auto index=measureKeyCount.fetch_add(1,std::memory_order_relaxed);
        if(index<measureMaxSamples)measureKeySamples[index]=elapsed;
    }
}
static LRESULT CALLBACK measureWindowProcedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    if(message==WM_CLOSE){DestroyWindow(window);return 0;}
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,wParam,lParam);
}
static bool measureSetForeground(HWND window) {
    if(GetForegroundWindow()==window)return true;
    const HWND previous=GetForegroundWindow();
    const DWORD targetThread=GetWindowThreadProcessId(window,nullptr);
    const DWORD sourceThread=previous?GetWindowThreadProcessId(previous,nullptr):0;
    bool attached=false;
    if(sourceThread && targetThread && sourceThread!=targetThread)
        attached=AttachThreadInput(sourceThread,targetThread,TRUE)!=0;
    SetForegroundWindow(window);
    if(attached)AttachThreadInput(sourceThread,targetThread,FALSE);
    if(GetForegroundWindow()!=window)SwitchToThisWindow(window,TRUE);
    const ULONGLONG end=GetTickCount64()+800;
    while(GetForegroundWindow()!=window && GetTickCount64()<end) {
        MSG message{};
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        Sleep(1);
    }
    return GetForegroundWindow()==window;
}
static bool measureIsPhotoshopForeground(HWND& root,DWORD& pid) {
    root=GetForegroundWindow();pid=0;
    if(!root)return false;
    wchar_t name[64]{};GetClassNameW(root,name,64);
    if(wcscmp(name,L"Photoshop")!=0)return false;
    GetWindowThreadProcessId(root,&pid);
    if(!pid)return false;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process)return false;
    wchar_t path[1024]{};DWORD length=1024;
    const bool ok=QueryFullProcessImageNameW(process,0,path,&length)!=0;
    CloseHandle(process);
    if(!ok)return false;
    const wchar_t* file=wcsrchr(path,L'\\');
    return _wcsicmp(file?file+1:path,L"Photoshop.exe")==0;
}
static HWND measureFindPhotoshopRoot() {
    struct Search {HWND root=nullptr;};
    Search search;
    EnumWindows([](HWND window,LPARAM data)->BOOL{
        wchar_t name[64]{};GetClassNameW(window,name,64);
        if(wcscmp(name,L"Photoshop")!=0)return TRUE;
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
        if(!process)return TRUE;
        wchar_t path[1024]{};DWORD length=1024;
        const bool ok=QueryFullProcessImageNameW(process,0,path,&length)!=0;
        CloseHandle(process);
        const wchar_t* file=wcsrchr(path,L'\\');
        if(ok && _wcsicmp(file?file+1:path,L"Photoshop.exe")==0 && IsWindowVisible(window)) {
            reinterpret_cast<Search*>(data)->root=window;return FALSE;
        }
        return TRUE;
    },reinterpret_cast<LPARAM>(&search));
    return search.root;
}
static DWORD measureIntegrityRid() {
    HANDLE token=nullptr;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return 0;
    BYTE buffer[256]{};DWORD size=0;
    DWORD rid=0;
    if(GetTokenInformation(token,TokenIntegrityLevel,buffer,sizeof(buffer),&size)) {
        auto label=reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer);
        const DWORD count=*GetSidSubAuthorityCount(label->Label.Sid);
        if(count)rid=*GetSidSubAuthority(label->Label.Sid,count-1);
    }
    CloseHandle(token);return rid;
}
static ULONGLONG measureCpuTimeUs() {
    FILETIME created{},exited{},kernel{},user{};
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))return 0;
    const ULONGLONG value=(static_cast<ULONGLONG>(kernel.dwHighDateTime)<<32)|kernel.dwLowDateTime;
    const ULONGLONG userValue=(static_cast<ULONGLONG>(user.dwHighDateTime)<<32)|user.dwLowDateTime;
    return (value+userValue)/10;
}
static void measurePump(unsigned expected) {
    MSG message{};
    const ULONGLONG deadline=GetTickCount64()+3000;
    for(;;) {
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
#ifndef MEASURE_BASELINE
            if(message.message==foregroundMessage) {
                foregroundRefreshPosted.store(false,std::memory_order_release);
                updateForegroundScope(reinterpret_cast<HWND>(message.lParam),true);
                continue;
            }
#endif
            TranslateMessage(&message);DispatchMessageW(&message);
        }
        const unsigned count=measureMouseCount.load(std::memory_order_relaxed)+
                             measureKeyCount.load(std::memory_order_relaxed);
        // SendInput may return before this thread receives every low-level
        // hook callback when the target is another process; keep collecting
        // until the expected count or a bounded grace period.
        if(count>=expected && measureDone.load(std::memory_order_relaxed))break;
        if(GetTickCount64()>deadline)break;
        Sleep(0);
    }
}
static bool measureSendMoves(int count) {
    RECT rect{};GetWindowRect(measureWindow,&rect);
    const int width=rect.right-rect.left,height=rect.bottom-rect.top;
    const int left=rect.left+width/4,top=rect.top+height/2;
    const int right=rect.left+width*3/4;
    const int screenWidth=GetSystemMetrics(SM_CXSCREEN),screenHeight=GetSystemMetrics(SM_CYSCREEN);
    int sent=0;
    for(int i=0;i<count;++i) {
        INPUT input{};
        input.type=INPUT_MOUSE;
        input.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE;
        input.mi.dx=(i&1?right:left)*65535/std::max(1,screenWidth-1);
        input.mi.dy=top*65535/std::max(1,screenHeight-1);
        if(SendInput(1,&input,sizeof(INPUT))!=1)return false;
        ++sent;
    }
    return sent==count;
}
static bool measureSendKeys(int presses) {
    for(int i=0;i<presses;++i) {
        INPUT inputs[2]{};
        inputs[0].type=INPUT_KEYBOARD;inputs[0].ki.wVk=VK_F24;
        inputs[1].type=INPUT_KEYBOARD;inputs[1].ki.wVk=VK_F24;
        inputs[1].ki.dwFlags=KEYEVENTF_KEYUP;
        if(SendInput(2,inputs,sizeof(INPUT))!=2)return false;
    }
    return true;
}
struct MeasureResult {
    unsigned count=0;
    ULONGLONG p50=0,p95=0,maximum=0,minimum=0,mean=0;
    std::vector<ULONGLONG> samples;
};
static MeasureResult measureSummarize(const ULONGLONG* values,unsigned count) {
    MeasureResult result;result.count=count;
    if(!count)return result;
    result.samples.assign(values,values+count);
    std::sort(result.samples.begin(),result.samples.end());
    result.p50=result.samples[(result.samples.size()-1)/2];
    result.p95=result.samples[(result.samples.size()*95+99)/100-1];
    result.maximum=result.samples.back();result.minimum=result.samples.front();
    unsigned long long total=0;for(auto value:result.samples)total+=value;
    result.mean=total/result.samples.size();
    return result;
}
static bool measureWantedForeground(bool photoshop,HWND& foreground) {
    foreground=GetForegroundWindow();
    if(photoshop) {
        HWND root=nullptr;DWORD pid=0;
        const bool ok=measureIsPhotoshopForeground(root,pid) && root==foreground;
        if(!ok) {
            wchar_t name[64]{};GetClassNameW(foreground,name,64);
            DWORD foregroundPid=0;GetWindowThreadProcessId(foreground,&foregroundPid);
            std::cerr<<"foreground mismatch: hwnd="<<reinterpret_cast<uintptr_t>(foreground)
                     <<" class=";std::wcerr<<name;
            std::cerr<<" pid="<<foregroundPid<<" psRoot="<<reinterpret_cast<uintptr_t>(root)
                     <<" psPid="<<pid<<"\n";
        }
        return ok;
    }
    if(foreground!=measureWindow)return false;
    DWORD pid=0;GetWindowThreadProcessId(foreground,&pid);
    return pid==measurePid;
}
static void measureApplyLowCpu() {
    DWORD_PTR available=0,system=0;
    if(!GetProcessAffinityMask(GetCurrentProcess(),&available,&system))return;
    measureOriginalAffinity=available;
    measureOriginalPriority=GetPriorityClass(GetCurrentProcess());
    DWORD_PTR mask=0;int picked=0;
    for(int bit=int(sizeof(DWORD_PTR)*8)-1;bit>=0 && picked<2;--bit)
        if(system&(DWORD_PTR(1)<<bit)){mask|=DWORD_PTR(1)<<bit;++picked;}
    if(picked==2 && SetProcessAffinityMask(GetCurrentProcess(),mask))measureAppliedAffinity=mask;
    SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
}
static void measureRestoreCpu() {
    if(measureOriginalAffinity)SetProcessAffinityMask(GetCurrentProcess(),measureOriginalAffinity);
    if(measureOriginalPriority)SetPriorityClass(GetCurrentProcess(),measureOriginalPriority);
}
static const char* measureTriggerName(bool photoshop) {return photoshop?"ps-foreground":"placeholder-foreground";}
int main(int argc,char** argv) {
    bool photoshop=false,direct=false;int moves=2000,keys=200;std::string output;
    for(int i=1;i<argc;++i) {
        const std::string argument=argv[i];
        if(argument=="--low")measureLow=true;
        else if(argument=="--baseline")measureBaseline=true;
        else if(argument=="--photoshop")photoshop=true;
        else if(argument=="--direct")direct=true;
        else if(argument=="--moves" && i+1<argc)moves=std::stoi(argv[++i]);
        else if(argument=="--keys" && i+1<argc)keys=std::stoi(argv[++i]);
        else if(argument=="--output" && i+1<argc)output=argv[++i];
        else {std::cerr<<"Unknown argument: "<<argument<<"\n";return 2;}
    }
    if(output.empty()){std::cerr<<"--output is required\n";return 2;}
    QueryPerformanceFrequency(&measureFrequency);
    measurePid=GetCurrentProcessId();
    measurePreviousForeground=GetForegroundWindow();
    measureCursorSaved=GetCursorPos(&measurePreviousCursor)!=0;
    if(measureLow)measureApplyLowCpu();
    WNDCLASSW windowClass{};windowClass.lpfnWndProc=measureWindowProcedure;windowClass.hInstance=GetModuleHandleW(nullptr);
    windowClass.lpszClassName=measureClass;
    if(!RegisterClassW(&windowClass)){std::cerr<<"window class registration failed\n";return 3;}
    measureWindow=CreateWindowW(measureClass,L"Hook measurement placeholder",WS_OVERLAPPEDWINDOW,
        120,120,520,320,nullptr,nullptr,windowClass.hInstance,nullptr);
    if(!measureWindow){std::cerr<<"placeholder window failed\n";return 3;}
    ShowWindow(measureWindow,SW_SHOWNOACTIVATE);
    // Initialise the hook globals used by the real input path.
    counterFrequency=measureFrequency.QuadPart;
    hookThread=GetCurrentThreadId();
    available=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    config.enabled=true;config.overlay=nullptr;
    config.bindings[0]={Toggle,{VK_CAPITAL},1};
    if(photoshop) {
        HWND root=measureFindPhotoshopRoot();
        std::cout<<"ps-root="<<reinterpret_cast<uintptr_t>(root)<<" foreground="
                 <<reinterpret_cast<uintptr_t>(GetForegroundWindow())<<std::endl;
        if(!root || !measureSetForeground(root)){std::cerr<<"cannot verify Photoshop foreground\n";return 4;}
#ifndef MEASURE_BASELINE
        updateForegroundScope(root,true);
#endif
    } else {
        if(!measureSetForeground(measureWindow)){std::cerr<<"cannot verify placeholder foreground\n";return 4;}
#ifndef MEASURE_BASELINE
        updateForegroundScope(measureWindow,true);
#endif
    }
    HHOOK targetKey=nullptr,targetMouse=nullptr;
    if(!direct) {
        targetKey=SetWindowsHookExW(WH_KEYBOARD_LL,keyboard,GetModuleHandleW(nullptr),0);
        targetMouse=SetWindowsHookExW(WH_MOUSE_LL,pointer,GetModuleHandleW(nullptr),0);
        if(!targetKey || !targetMouse) {
            std::cerr<<"hook installation failed\n";return 5;
        }
    }
    auto runBatch=[&](bool mouse,int count,MeasureResult& result,ULONGLONG& cpuDelta) {
        HWND foreground=nullptr;
        if(!measureWantedForeground(photoshop,foreground)) {
            std::cout<<"foreground verification failed before batch fg="
                     <<reinterpret_cast<uintptr_t>(foreground)<<std::endl;result={};return false;
        }
        if(direct) {
            // Synthetic input cannot be delivered to another Medium-integrity
            // process in this environment (verified independently: no hook
            // callbacks and no Notepad text). Invoke the real callback with the
            // verified foreground instead. The measured span still covers the
            // actual decision path; only OS hook delivery is excluded.
            measureMouseCount.store(0);measureKeyCount.store(0);
            measurementKind=mouse?2:1;
#ifndef MEASURE_BASELINE
            updateForegroundScope(foreground,true);
#endif
            const ULONGLONG cpuBefore=measureCpuTimeUs();
            measureDirectMode=true;measureCollect.store(true);
            if(mouse) {
                MSLLHOOKSTRUCT event{};event.flags=LLMHF_INJECTED;event.time=GetTickCount();
                RECT rect{};GetWindowRect(measureWindow,&rect);
                event.pt.x=rect.left+40;event.pt.y=rect.top+40;
                for(int i=0;i<count;++i) {
                    event.pt.x=rect.left+40+(i&1?12:0);
                    pointer(0,WM_MOUSEMOVE,reinterpret_cast<LPARAM>(&event));
                }
            } else {
                KBDLLHOOKSTRUCT event{};event.vkCode=VK_F24;event.flags=LLKHF_INJECTED;
                for(int i=0;i<count;++i) {
                    event.time=GetTickCount();
                    keyboard(0,WM_KEYDOWN,reinterpret_cast<LPARAM>(&event));
                    keyboard(0,WM_KEYUP,reinterpret_cast<LPARAM>(&event));
                }
            }
            measureCollect.store(false);measureDirectMode=false;
            const ULONGLONG cpuAfter=measureCpuTimeUs();cpuDelta=cpuAfter-cpuBefore;
            result=mouse?measureSummarize(measureMouseSamples,measureMouseCount.load()):
                         measureSummarize(measureKeySamples,measureKeyCount.load());
            return result.count>0;
        }
        measureMouseCount.store(0);measureKeyCount.store(0);
        measurementKind=mouse?2:1;
        const ULONGLONG cpuBefore=measureCpuTimeUs();
        measureDone.store(false);measureCollect.store(true);
        std::thread sender([&]{
            const bool ok=mouse?measureSendMoves(count):measureSendKeys(count);
            measureDone.store(true);
            std::cout<<"sent kind="<<(mouse?"mouse":"key")<<" count="<<count<<" ok="<<ok
                     <<" sample="<<(mouse?measureMouseCount.load():measureKeyCount.load())<<std::endl;
        });
        measurePump(mouse?static_cast<unsigned>(count):static_cast<unsigned>(count*2));
        sender.join();
        measureCollect.store(false);
        const ULONGLONG cpuAfter=measureCpuTimeUs();
        cpuDelta=cpuAfter-cpuBefore;
        HWND after=nullptr;
        if(!measureWantedForeground(photoshop,after))std::cerr<<"foreground changed during batch\n";
        result=mouse?measureSummarize(measureMouseSamples,measureMouseCount.load()):
                     measureSummarize(measureKeySamples,measureKeyCount.load());
        return result.count>0;
    };
    MeasureResult mouseResult,keyResult;ULONGLONG mouseCpu=0,keyCpu=0;
    const bool mouseOk=runBatch(true,moves,mouseResult,mouseCpu);
    const bool keyOk=runBatch(false,keys,keyResult,keyCpu);

    const std::string samplesPath=output+".samples.csv";
    {
        std::ofstream samples(samplesPath,std::ios::binary);
        samples<<"kind,us\n";
        for(auto value:mouseResult.samples)samples<<"mouse,"<<value<<"\n";
        for(auto value:keyResult.samples)samples<<"key,"<<value<<"\n";
    }
    const char* version=measureBaseline?"0.4.311":"0.4.312";
    std::ofstream report(output,std::ios::binary);
    report<<"{";
    report<<"\"version\":\""<<version<<"\",\"baseline\":"<<(measureBaseline?"true":"false");
    report<<",\"low_cpu\":"<<(measureLow?"true":"false")<<",\"foreground\":\""<<(photoshop?"ps":"placeholder")<<"\"";
    report<<",\"direct\":"<<(direct?"true":"false")<<",\"integrity_rid\":"<<measureIntegrityRid();
    report<<",\"logical_processors\":"<<GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    report<<",\"affinity_mask\":"<<static_cast<unsigned long long>(measureAppliedAffinity);
    report<<",\"priority_class\":"<<GetPriorityClass(GetCurrentProcess());
    report<<",\"trigger\":\""<<measureTriggerName(photoshop)<<"\"";
    report<<",\"moves_requested\":"<<moves<<",\"keys_requested\":"<<keys;
    auto row=[&](const char* name,const MeasureResult& result,ULONGLONG cpu,bool ok){
        report<<",\""<<name<<"\":"<<"{\"ok\":"<<(ok?"true":"false")<<",\"count\":"<<result.count
              <<",\"p50_us\":"<<result.p50<<",\"p95_us\":"<<result.p95
              <<",\"min_us\":"<<result.minimum<<",\"max_us\":"<<result.maximum
              <<",\"mean_us\":"<<result.mean<<",\"cpu_delta_us\":"<<cpu<<"}";
    };
    row("mouse",mouseResult,mouseCpu,mouseOk);
    row("key",keyResult,keyCpu,keyOk);
    std::string samplesJson=samplesPath;
    for(auto& c:samplesJson)if(c=='\\')c='/';
    report<<",\"samples_file\":\""<<samplesJson<<"\""; // Path is local evidence only.
    report<<"}\n";
    report.close();
    std::cout<<version<<' '<<(measureLow?"low":"normal")<<' '<<(photoshop?"ps":"placeholder")
             <<" mouse n="<<mouseResult.count<<" p50="<<mouseResult.p50
             <<" p95="<<mouseResult.p95<<" max="<<mouseResult.maximum
             <<" key n="<<keyResult.count<<" p50="<<keyResult.p50
             <<" p95="<<keyResult.p95<<" max="<<keyResult.maximum<<"\n";
    UnhookWindowsHookEx(targetMouse);UnhookWindowsHookEx(targetKey);
    if(measureCursorSaved)SetCursorPos(measurePreviousCursor.x,measurePreviousCursor.y);
    if(measurePreviousForeground)measureSetForeground(measurePreviousForeground);
    DestroyWindow(measureWindow);
    if(available)CloseHandle(available);
    if(measureLow)measureRestoreCpu();
    return (mouseOk&&keyOk)?0:1;
}
