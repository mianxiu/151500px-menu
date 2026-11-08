#define THREAD_F_PROBE_SELF_TEST
#include "ThreadFProbe.cpp"
#include <stdexcept>
#include <cstring>

static void check(bool result,const char* message){if(!result)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::strcmp(argv[1],"--low")==0) {
            DWORD_PTR mask=0,system=0;check(GetProcessAffinityMask(GetCurrentProcess(),&mask,&system),"read affinity");
            DWORD_PTR limited=0;
            for(unsigned bit=0;bit<sizeof(mask)*8 && __builtin_popcountll(limited)<2;++bit)
                if(mask&(DWORD_PTR(1)<<bit))limited|=DWORD_PTR(1)<<bit;
            check(SetProcessAffinityMask(GetCurrentProcess(),limited) &&
                  SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS),"low CPU fixture");
        }
        module=GetModuleHandleW(nullptr);
        WNDCLASSW cls{};cls.lpfnWndProc=DefWindowProcW;cls.hInstance=module;cls.lpszClassName=L"Photoshop";
        check(RegisterClassW(&cls)!=0,"fixture class");
        const auto fixture=CreateWindowW(L"Photoshop",L"F observation fixture",0,0,0,100,100,nullptr,nullptr,module,nullptr);
        check(fixture && initialize(fixture),"probe bootstrap");
        check(SendMessageW(control,command,1,GetCurrentProcessId())==1,"arm all three thread hooks");
        auto previous=count;keyboard(-1,'F',0);queued(-1,0,0);sent(-1,0,0);
        check(count==previous,"negative hook codes must not dereference or record");
        keyboard(HC_NOREMOVE,'F',1);keyboard(HC_NOREMOVE,'F',1);
        check(count==previous+2 && events[previous].message==HC_NOREMOVE,"retain repeated peeks separately");
        keyboard(HC_ACTION,'F',1);keyboard(HC_ACTION,'F',0x40000001);keyboard(HC_ACTION,'F',0xc0000001);
        check(events[count-3].flags==1 && events[count-2].flags==0x40000001 && events[count-1].flags==0xc0000001,"down/repeat/up flags");
        previous=count;keyboard(HC_ACTION,'A',1);
        check(count==previous && !interesting(WM_CHAR,0x4f60) && !interesting(WM_KEYDOWN,'A'),"do not collect text or other letters");
        PostMessageW(fixture,WM_KEYDOWN,'F',1);
        MSG message{};
        check(PeekMessageW(&message,fixture,WM_KEYDOWN,WM_KEYDOWN,PM_NOREMOVE) && message.wParam=='F',"queue peek must preserve F");
        check(PeekMessageW(&message,fixture,WM_KEYDOWN,WM_KEYDOWN,PM_REMOVE) && message.wParam=='F',"remove must preserve F");
        check(!PeekMessageW(&message,fixture,WM_KEYDOWN,WM_KEYDOWN,PM_REMOVE),"exactly one queued F");
        previous=count;SendMessageW(fixture,WM_IME_STARTCOMPOSITION,0,0);
        check(count==previous+1 && events[previous].stage==4,"real sent-message hook");
        expires=GetTickCount64();SendMessageW(control,WM_TIMER,1,0);
        check(!running() && !owner,"expiry must unhook all three hooks and release owner");
        previous=count;SendMessageW(fixture,WM_IME_ENDCOMPOSITION,0,0);
        check(count==previous && SendMessageW(control,command,4,0)==0,"stop acknowledgement and no new events");
        check(SendMessageW(control,command,1,GetCurrentProcessId())==0,"do not reuse pinned old probe");
        check(SendMessageW(control,command,3,0)==LRESULT(count),"write bounded diagnostic output after stop");
        // Simulate the owner exiting while hooks are armed, without any process kill.
        owner=CreateEventW(nullptr,TRUE,TRUE,nullptr);
        keyboardHook=SetWindowsHookExW(WH_KEYBOARD,keyboard,module,GetCurrentThreadId());
        expires=GetTickCount64()+30000;SendMessageW(control,WM_TIMER,1,0);
        check(!running() && !owner,"exited owner must stop observation");
        count=capacity;keyboardHook=SetWindowsHookExW(WH_KEYBOARD,keyboard,module,GetCurrentThreadId());
        record(1,HC_ACTION,1);check(count==capacity+1,"overflow must be counted");stop();
        DestroyWindow(control);DestroyWindow(fixture);
        std::puts("F observer: real queue/sent hooks, negative codes, NOREMOVE, down/repeat/up, privacy, expiry, owner exit, stopped export and overflow passed; no input injected");
        return 0;
    }catch(const std::exception& error){stop();std::fprintf(stderr,"%s\n",error.what());return 1;}
}
