#define INPUT_CONTEXT_SELF_TEST
#include <cstdio>
#include "../native/InputContext.cpp"
#include <cstdio>
#include <stdexcept>
#include <string>

static HWND childEdit=nullptr;
static HIMC childContext=nullptr;
static HWND policyCanvas=nullptr,policyDialog=nullptr,policyDialogView=nullptr;
static HIMC policyCanvasContext=nullptr,policyDialogContext=nullptr,policyForeignContext=nullptr;
static bool canvasSawContext=false;
// The window procedure stands in for PS: it must observe the restored HIMC
// while handling the very click that enters canvas text editing.
static LRESULT CALLBACK canvasProcedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    if(message==WM_LBUTTONDOWN)canvasSawContext=associated(window)!=nullptr;
    return DefWindowProcW(window,message,wParam,lParam);
}
static PROCESS_INFORMATION fixtureChild{};
static void closeChild() {
    if(!fixtureChild.hProcess)return;
    if(WaitForSingleObject(fixtureChild.hProcess,0)==WAIT_TIMEOUT) {
        EnumWindows([](HWND window,LPARAM data)->BOOL{
            DWORD pid=0;GetWindowThreadProcessId(window,&pid);
            wchar_t name[64]{};GetClassNameW(window,name,64);
            if(pid==DWORD(data) && wcscmp(name,L"PhotoshopMenuImGui")==0)SendMessageW(window,WM_CLOSE,0,0);
            return TRUE;
        },LPARAM(fixtureChild.dwProcessId));
        if(WaitForSingleObject(fixtureChild.hProcess,5000)!=WAIT_OBJECT_0)std::fprintf(stderr,"Owned menu fixture did not exit normally\n");
    }
    CloseHandle(fixtureChild.hThread);CloseHandle(fixtureChild.hProcess);fixtureChild={};
}
static LRESULT CALLBACK menuProcedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    if(message==WM_APP+53)return associated(childEdit)==childContext && ImmGetOpenStatus(childContext);
    if(message==WM_APP+54){SetFocus(childEdit);return 1;}
    if(message==WM_APP+55){
        const auto target=reinterpret_cast<HWND>(wParam);DWORD pid=0;GetWindowThreadProcessId(target,&pid);
        AllowSetForegroundWindow(pid);return SetForegroundWindow(target);
    }
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,wParam,lParam);
}
static int childMenu() {
    WNDCLASSW wc{};wc.lpfnWndProc=menuProcedure;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"PhotoshopMenuImGui";
    if(!RegisterClassW(&wc))return 2;
    auto window=CreateWindowW(wc.lpszClassName,L"Owned input fixture",WS_OVERLAPPEDWINDOW,320,0,240,120,nullptr,nullptr,wc.hInstance,nullptr);
    childEdit=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE,0,0,120,24,window,nullptr,wc.hInstance,nullptr);
    childContext=ImmCreateContext();if(!window || !childEdit || !childContext)return 3;
    ImmAssociateContext(childEdit,childContext);ImmSetOpenStatus(childContext,TRUE);ShowWindow(window,SW_SHOWNOACTIVATE);
    MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    ImmDestroyContext(childContext);return 0;
}

static void check(bool value,const char* message){if(!value){wchar_t name[128]{};GetClassNameW(GetForegroundWindow(),name,128);std::fprintf(stderr,"root=%p foreground=%p class=%ls focus=%p owned=%p assoc_focus=%p assoc_owned=%p restore_failed=%d tsf=%d policy=%d count=%d\n",root,GetForegroundWindow(),name,GetFocus(),disabledWindow,associated(GetFocus()),disabledWindow?associated(disabledWindow):nullptr,restoreFailed,tsf.keyboard!=nullptr,int(focusPolicy),focusCount);throw std::runtime_error(message);}}
static bool forceForeground(HWND window) {
    if(GetForegroundWindow()==window)return true;
    const auto previous=GetForegroundWindow();
    const auto targetThread=GetWindowThreadProcessId(window,nullptr);
    const auto foregroundThread=previous?GetWindowThreadProcessId(previous,nullptr):0;
    bool attached=false;
    if(foregroundThread && targetThread && foregroundThread!=targetThread)
        attached=AttachThreadInput(foregroundThread,targetThread,TRUE)!=0;
    SetForegroundWindow(window);
    if(attached)AttachThreadInput(foregroundThread,targetThread,FALSE);
    if(GetForegroundWindow()!=window)SwitchToThisWindow(window,TRUE);
    return GetForegroundWindow()==window;
}
static HWND initialForeground=nullptr;
static void settledForeground(HWND window) {
    const auto end=GetTickCount64()+500;
    while(GetForegroundWindow()!=window && GetTickCount64()<end) {
        forceForeground(window);
        MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(1);
    }
    check(GetForegroundWindow()==window,"fixture foreground did not settle");
}
int main(int argc,char** argv) {
    if(argc==2 && strcmp(argv[1],"--owned-menu")==0)return childMenu();
    try {
        bool low=false,queuedOnly=false;
        for(int i=1;i<argc;++i) {
            if(strcmp(argv[i],"--low")==0)low=true;
            else if(strcmp(argv[i],"--queued-only")==0)queuedOnly=true;
            else throw std::runtime_error("Unknown fixture argument");
        }
        if(low){
            DWORD_PTR available=0,system=0;
            check(GetProcessAffinityMask(GetCurrentProcess(),&available,&system)!=0,"fixture affinity");
            DWORD_PTR mask=0;int picked=0;
            for(int bit=int(sizeof(DWORD_PTR)*8)-1;bit>=0 && picked<2;--bit)
                if(system&(DWORD_PTR(1)<<bit)){mask|=DWORD_PTR(1)<<bit;++picked;}
            check(picked==2 && SetProcessAffinityMask(GetCurrentProcess(),mask)!=0,"fixture low CPU affinity");
            check(SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS)!=0,"fixture low CPU priority");
            DWORD_PTR affinity=0;int count=0;
            check(GetProcessAffinityMask(GetCurrentProcess(),&affinity,&system)!=0,"fixture affinity readback");
            for(auto bits=affinity;bits;bits&=bits-1)count++;
            check(count==2 && affinity==mask && GetPriorityClass(GetCurrentProcess())==BELOW_NORMAL_PRIORITY_CLASS,"fixture low CPU not applied");
        }
        initialForeground=GetForegroundWindow();
        module=GetModuleHandleW(nullptr);
        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=module;wc.lpszClassName=L"Photoshop";
        check(RegisterClassW(&wc)!=0,"fixture class");
        auto fixture=CreateWindowW(L"Photoshop",L"Owned input context test",WS_OVERLAPPEDWINDOW,0,0,300,180,nullptr,nullptr,module,nullptr);
        auto focus=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,0,0,40,40,fixture,nullptr,module,nullptr);
        auto edit=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE,50,0,100,24,fixture,nullptr,module,nullptr);
        WNDCLASSW viewClass{};viewClass.lpfnWndProc=DefWindowProcW;viewClass.hInstance=module;viewClass.lpszClassName=L"PSViewC";
        check(RegisterClassW(&viewClass)!=0,"policy view class");
        WNDCLASSW floatClass{};floatClass.lpfnWndProc=DefWindowProcW;floatClass.hInstance=module;floatClass.lpszClassName=L"PSFloatC";
        check(RegisterClassW(&floatClass)!=0,"policy dialog class");
        policyCanvas=CreateWindowW(L"PSViewC",L"",WS_CHILD|WS_VISIBLE,160,0,60,60,fixture,nullptr,module,nullptr);
        if(policyCanvas)SetWindowLongPtrW(policyCanvas,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(canvasProcedure));
        policyDialog=CreateWindowExW(WS_EX_TOOLWINDOW,L"PSFloatC",L"Policy dialog fixture",WS_OVERLAPPEDWINDOW,420,0,180,120,fixture,nullptr,module,nullptr);
        policyDialogView=CreateWindowW(L"PSViewC",L"",WS_CHILD|WS_VISIBLE,0,0,60,40,policyDialog,nullptr,module,nullptr);
        check(policyCanvas && policyDialog && policyDialogView,"policy focus fixtures");
        policyCanvasContext=ImmCreateContext();policyDialogContext=ImmCreateContext();policyForeignContext=ImmCreateContext();
        check(policyCanvasContext && policyDialogContext && policyForeignContext,"policy IME contexts");
        ImmAssociateContext(policyCanvas,policyCanvasContext);ImmAssociateContext(policyDialogView,policyDialogContext);
        auto context=ImmCreateContext();check(context!=nullptr,"IME context");ImmAssociateContext(focus,context);
        ShowWindow(fixture,SW_SHOWNORMAL);forceForeground(fixture);SetFocus(focus);LockSetForegroundWindow(LSFW_LOCK);
        check(GetForegroundWindow()==fixture,"fixture foreground denied");
        const auto layout=GetKeyboardLayout(0);
        auto hook=SetWindowsHookExW(WH_CALLWNDPROC,InputContextHook,nullptr,GetCurrentThreadId());check(hook!=nullptr,"thread hook");
        auto queuedHook=SetWindowsHookExW(WH_GETMESSAGE,InputContextQueueHook,nullptr,GetCurrentThreadId());check(queuedHook!=nullptr,"queued input hook");
        auto attach=RegisterWindowMessageW(L"PhotoshopMenu.InputContext.Attach.v1");
        SendMessageW(fixture,attach,GetCurrentProcessId(),12345);
        check(control && GetPropW(fixture,controlProperty)==control,"in-thread handshake");
        check(GetPropW(control,ownerProperty)==reinterpret_cast<HANDLE>(ULONG_PTR(GetCurrentProcessId())),"service ownership binding");
        const auto command=[&](UINT op,HWND target=0){return SendMessageW(control,controlMessage,(12345<<8)|op,reinterpret_cast<LPARAM>(target?target:focus));};
        const auto dispatchContextEvents=[&](){MSG event{};bool seen=false;while(PeekMessageW(&event,control,contextChangedMessage,contextChangedMessage,PM_REMOVE)){DispatchMessageW(&event);seen=true;}check(seen,"TSF change event was not posted");};
        check(command(1)==5,"association read");
        SendMessageW(control,controlMessage,(54321<<8)|2,reinterpret_cast<LPARAM>(focus));check(associated(focus)==context,"foreign nonce changed context");
        check((command(2)&15)==3 && associated(focus)==nullptr,"disable association readback");
        check(GetKeyboardLayout(0)==layout,"keyboard profile changed");
        SendMessageW(focus,WM_KEYDOWN,'T',0);check(disabledWindow==focus && associated(focus)==nullptr,"idle type tool released protection before editing");
        SendMessageW(focus,WM_LBUTTONDOWN,0,0);check(associated(focus)==context && !disabledWindow,"mouse input did not restore before editing");
        check((command(2)&15)==3,"queued input disable");
        check(PostMessageW(focus,WM_LBUTTONDOWN,0,0)!=0,"queued mouse message");
        MSG queued{};while(PeekMessageW(&queued,nullptr,0,0,PM_REMOVE)){TranslateMessage(&queued);DispatchMessageW(&queued);}
        check(associated(focus)==context && !disabledWindow,"queued mouse input did not restore before editing");
        check((command(2)&15)==3,"queued keyboard disable");
        PostMessageW(focus,WM_KEYDOWN,'T',0);
        check(PeekMessageW(&queued,focus,WM_KEYDOWN,WM_KEYDOWN,PM_REMOVE) && disabledWindow==focus,"queued idle T released protection");
        DispatchMessageW(&queued);
        PostMessageW(focus,WM_KEYDOWN,VK_RETURN,0);
        check(PeekMessageW(&queued,focus,WM_KEYDOWN,WM_KEYDOWN,PM_NOREMOVE) && disabledWindow==focus,"peek without removal changed input context");
        check(PeekMessageW(&queued,focus,WM_KEYDOWN,WM_KEYDOWN,PM_REMOVE) && !disabledWindow && associated(focus)==context,"queued Enter did not restore before translation");
        TranslateMessage(&queued);DispatchMessageW(&queued);
        if(queuedOnly) {
            check(GetForegroundWindow()==fixture,"queued fixture foreground changed");
            UnhookWindowsHookEx(queuedHook);UnhookWindowsHookEx(hook);DestroyWindow(control);DestroyWindow(fixture);ImmDestroyContext(context);
            std::puts("Queued PS-thread input passed: mouse restoration, idle T retained, Enter restored before TranslateMessage, PM_NOREMOVE unchanged, original IMM and HKL preserved");return 0;
        }
        check((command(2)&15)==3,"second disable");SetFocus(edit);check(associated(focus)==context && !disabledWindow,"focus input restoration");
        SetFocus(focus);check((command(2)&15)==3,"watchdog disable");
        deadline=GetTickCount64()-1;forceForeground(fixture);SetFocus(focus);SendMessageW(control,WM_TIMER,1,0);
        check(GetForegroundWindow()==fixture && GetFocus()==focus,"foreground changed during lease-expiry fixture");
        check(associated(focus)==context && !disabledWindow,"expired lease restoration");
        check((command(2)&15)==3,"explicit restore disable");check(command(3)==21 && associated(focus)==context,"explicit original-context restoration");
        check(GetKeyboardLayout(0)==layout,"restore changed keyboard profile");
        check((command(2)&15)==3,"external IMM reassociation disable");ImmAssociateContext(focus,context);
        check(command(1)&2,"read-only state unexpectedly restored ownership");
        check(command(4)==21 && associated(focus)==context,"external IMM reassociation survived lease validation");
        check((command(2)&15)==3,"profile notification disable");SendMessageW(focus,WM_INPUTLANGCHANGE,0,reinterpret_cast<LPARAM>(layout));
        check(command(1)==21 && associated(focus)==context && GetKeyboardLayout(0)==layout,"profile notification did not restore original context");
        wchar_t ownPath[32768]{};check(GetModuleFileNameW(nullptr,ownPath,32768)!=0,"fixture path");
        std::wstring folder=ownPath;folder=folder.substr(0,folder.find_last_of(L"\\"))+L"\\input-context-menu-fixture";
        check(CreateDirectoryW(folder.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS,"fixture directory");
        std::wstring childPath=folder+L"\\MenuHost.exe";check(CopyFileW(ownPath,childPath.c_str(),FALSE)!=0,"menu fixture copy");
        std::wstring childCommand=L"\""+childPath+L"\" --owned-menu";STARTUPINFOW startup{};startup.cb=sizeof(startup);auto& child=fixtureChild;
        check(CreateProcessW(childPath.c_str(),childCommand.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder.c_str(),&startup,&child)!=0,"menu fixture process");
        WaitForInputIdle(child.hProcess,3000);HWND menu=nullptr;
        const auto findMenu=[](HWND window,LPARAM data)->BOOL {
            auto args=reinterpret_cast<std::pair<DWORD,HWND*>*>(data);DWORD pid=0;GetWindowThreadProcessId(window,&pid);
            wchar_t name[64]{};GetClassNameW(window,name,64);
            if(pid==args->first && wcscmp(name,L"PhotoshopMenuImGui")==0){*args->second=window;return FALSE;}return TRUE;
        };
        std::pair<DWORD,HWND*> search{child.dwProcessId,&menu};const auto readyEnd=GetTickCount64()+3000;
        do {EnumWindows(findMenu,reinterpret_cast<LPARAM>(&search));if(menu)break;Sleep(10);}while(GetTickCount64()<readyEnd && WaitForSingleObject(child.hProcess,0)==WAIT_TIMEOUT);
        check(menu!=nullptr,"child menu window readiness");check(ownMenu(menu),"trusted child menu identity");
        forceForeground(fixture);SetFocus(focus);check((command(2)&15)==3,"menu origin disable");
        testTrace=true;
        for(int i=0;i<50;++i) {
            forceForeground(menu);settledForeground(menu);SendMessageW(menu,WM_APP+54,0,0);
            check((command(1)&130)==130 && associated(focus)==nullptr && SendMessageW(menu,WM_APP+53,0,0),"own menu released PS protection or changed menu IME");
            command(4);SendMessageW(control,WM_TIMER,1,0);check(disabledWindow==focus,"own menu renewal lost lease");
            SendMessageW(menu,WM_APP+55,reinterpret_cast<WPARAM>(fixture),0);SetFocus(focus);settledForeground(fixture);
            check(GetForegroundWindow()==fixture,"PS return foreground changed");
            {const auto returnState=command(1);const auto returnAssociation=associated(focus);if((returnState&15)!=3 || returnAssociation)std::fprintf(stderr,"loop return state=%llx assoc=%p focus=%p disabled=%p\n",(unsigned long long)returnState,returnAssociation,GetFocus(),disabledWindow);}
            check((command(1)&15)==3 && associated(focus)==nullptr,"PS return exposed an IME gap");
        }
        testTrace=false;
        forceForeground(menu);SendMessageW(menu,WM_APP+54,0,0);deadline=GetTickCount64()-1;SendMessageW(control,WM_TIMER,1,0);
        check(!disabledWindow && associated(focus)==context,"own menu bypassed lease expiry");
        SendMessageW(menu,WM_APP+55,reinterpret_cast<WPARAM>(fixture),0);settledForeground(fixture);SetFocus(focus);check((command(2)&15)==3,"foreign application fixture disable");
        auto foreign=CreateWindowW(L"STATIC",L"Foreign window",WS_OVERLAPPEDWINDOW,580,0,120,100,nullptr,nullptr,module,nullptr);
        ShowWindow(foreign,SW_SHOW);forceForeground(foreign);SetFocus(foreign);SendMessageW(control,WM_TIMER,1,0);
        check(!disabledWindow && associated(focus)==context && !ownMenu(foreign),"foreign application retained PS protection");
        DestroyWindow(foreign);SendMessageW(menu,WM_CLOSE,0,0);check(WaitForSingleObject(child.hProcess,5000)==WAIT_OBJECT_0,"menu fixture normal exit");
        closeChild();settledForeground(fixture);SetFocus(focus);LockSetForegroundWindow(LSFW_LOCK);
        check(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)),"TSF fixture COM");
        ITfThreadMgr* manager=nullptr;ITfDocumentMgr* document=nullptr;ITfContext* text=nullptr;ITfCompartmentMgr* compartments=nullptr;ITfCompartment* keyboard=nullptr;TfClientId client=0;TfEditCookie cookie=0;
        check(SUCCEEDED(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_ITfThreadMgr,reinterpret_cast<void**>(&manager))),"TSF manager");
        check(SUCCEEDED(manager->Activate(&client)),"TSF client");
        const auto associate=[&](ITfDocumentMgr* value){ITfDocumentMgr* previous=nullptr;const auto result=manager->AssociateFocus(focus,value,&previous);if(previous)previous->Release();check(SUCCEEDED(result),"TSF HWND association");};
        check(SUCCEEDED(manager->CreateDocumentMgr(&document)) && SUCCEEDED(document->CreateContext(client,0,nullptr,&text,&cookie)) && SUCCEEDED(document->Push(text)) && SUCCEEDED(manager->SetFocus(document)),"TSF focused context");
        associate(document);check(SUCCEEDED(manager->SetFocus(document)),"TSF associated focus");
        check(SUCCEEDED(text->QueryInterface(IID_ITfCompartmentMgr,reinterpret_cast<void**>(&compartments))) && SUCCEEDED(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED,&keyboard)),"TSF keyboard compartment");
        VARIANT prior{},value{};check(SUCCEEDED(keyboard->GetValue(&prior)),"TSF prior value");
        check((command(2)&35)==35 && SUCCEEDED(keyboard->GetValue(&value)) && value.vt==VT_I4 && value.lVal==1,"TSF actual disabled readback");VariantClear(&value);
        check((command(1)&64) && tsf.watching(),"TSF subscriptions not active");
        check((command(3)&15)==5,"TSF explicit restore");keyboard->Release();keyboard=nullptr;
        check(SUCCEEDED(compartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED,&keyboard)) && SUCCEEDED(keyboard->GetValue(&value)) && value.vt==prior.vt && (value.vt==VT_EMPTY || value.lVal==prior.lVal),"TSF original value restoration");
        VariantClear(&value);
        check((command(2)&35)==35,"TSF reset disable");
        check(SUCCEEDED(manager->SetFocus(nullptr)) && (command(4)&35)==35,"temporary absent TSF focus discarded original protection");
        check(SUCCEEDED(manager->SetFocus(document)),"TSF focus return");value.vt=VT_I4;value.lVal=0;
        const auto resetRevision=GetPropW(control,revisionProperty);
        check(SUCCEEDED(keyboard->SetValue(client,&value)),"external TSF reset");VariantClear(&value);
        check((command(1)&35)==3 && disabledWindow,"TSF drift read changed ownership or reported disabled");
        dispatchContextEvents();
        check((command(1)&51)==17 && !disabledWindow,"external TSF reset survived notification handling");
        check(GetPropW(control,revisionProperty)!=resetRevision && !tsf.watching() && tsf.references==1,"TSF reset did not invalidate evidence or release subscriptions");
        check((command(2)&35)==35,"TSF renewed disable");
        ITfDocumentMgr* secondDocument=nullptr;ITfContext* secondText=nullptr;ITfCompartmentMgr* secondCompartments=nullptr;ITfCompartment* secondKeyboard=nullptr;
        check(SUCCEEDED(manager->CreateDocumentMgr(&secondDocument)) && SUCCEEDED(secondDocument->CreateContext(client,0,nullptr,&secondText,&cookie)) && SUCCEEDED(secondDocument->Push(secondText)) && SUCCEEDED(manager->SetFocus(secondDocument)),"second TSF context");
        associate(secondDocument);check(SUCCEEDED(manager->SetFocus(secondDocument)),"second associated focus");
        check(SUCCEEDED(secondText->QueryInterface(IID_ITfCompartmentMgr,reinterpret_cast<void**>(&secondCompartments))) && SUCCEEDED(secondCompartments->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED,&secondKeyboard)),"second keyboard compartment");
        VARIANT secondPrior{};check(SUCCEEDED(secondKeyboard->GetValue(&secondPrior)),"second TSF original");
        dispatchContextEvents();
        check((command(1)&51)==17 && !disabledWindow,"notification accepted a different TSF context");
        check(SUCCEEDED(keyboard->GetValue(&value)) && value.vt==VT_I4 && value.lVal==0,"old TSF context not restored");VariantClear(&value);
        check(SUCCEEDED(secondKeyboard->GetValue(&value)) && value.vt==secondPrior.vt && (value.vt==VT_EMPTY || value.lVal==secondPrior.lVal),"unverified new TSF context modified");VariantClear(&value);
        check((command(2)&35)==35 && (command(3)&15)==5,"fresh TSF context cannot be independently protected and restored");
        for(int i=0;i<20;i++)check((command(2)&99)==99 && (command(3)&15)==5 && tsf.references==1 && !tsf.watching(),"repeated lease leaked subscriptions or failed readback");
        VariantClear(&secondPrior);secondKeyboard->Release();secondCompartments->Release();associate(document);manager->SetFocus(document);secondDocument->Pop(TF_POPF_ALL);secondText->Release();secondDocument->Release();
        VariantClear(&prior);VariantClear(&value);keyboard->Release();compartments->Release();manager->SetFocus(nullptr);document->Pop(TF_POPF_ALL);text->Release();document->Release();manager->Deactivate();manager->Release();CoUninitialize();
        // Focus policy: synchronous IMM detach in WM_SETFOCUS, no lease, no
        // mouse/edit-key restoration, PSFloatC dialogs excluded.
        const auto policy=[&](int enable){return SendMessageW(control,controlMessage,(12345<<8)|5,enable);};
        forceForeground(fixture);settledForeground(fixture);SetFocus(policyCanvas);
        check(associated(policyCanvas)==policyCanvasContext,"policy fixture association");
        check((policy(1)&0x100) && focusPolicy,"focus policy enable");
        check(associated(policyCanvas)==nullptr && focusCount==1,"canvas sync disable");
        check((command(1)>>16&0xff)==1 && ((command(1)>>24)&0xff)==2,"policy state count or reason");
        check(GetKeyboardLayout(0)==layout,"policy changed keyboard profile");
        SendMessageW(policyCanvas,WM_LBUTTONDOWN,0,0);
        check(associated(policyCanvas)==nullptr && focusCount==1,"mouse down released policy protection");
        for(int key:{int('T'),int(VK_RETURN),int(VK_F2)}){SendMessageW(control,controlMessage,(12345<<8)|6,0);SendMessageW(policyCanvas,WM_KEYDOWN,key,0);}
        check(associated(policyCanvas)==nullptr && focusCount==1,"edit key released policy protection");
        deadline=GetTickCount64()-1;forceForeground(fixture);SetFocus(policyCanvas);SendMessageW(control,WM_TIMER,1,0);
        check(associated(policyCanvas)==nullptr && focusCount==1,"policy lease expiry released protection");
        SetFocus(edit);
        check(associated(policyCanvas)==policyCanvasContext && focusCount==0,"edit focus did not restore policy context");
        SetFocus(policyCanvas);
        check(associated(policyCanvas)==nullptr && focusCount==1,"canvas refocus not disabled");
        ImmAssociateContext(policyCanvas,policyCanvasContext);
        check(associated(policyCanvas)==policyCanvasContext,"policy reapply precondition");
        SendMessageW(policyCanvas,WM_INPUTLANGCHANGE,0,reinterpret_cast<LPARAM>(layout));
        check(associated(policyCanvas)==nullptr && focusCount==1,"input language change did not reapply policy");
        check(GetKeyboardLayout(0)==layout,"language reapply changed keyboard profile");
        ShowWindow(policyDialog,SW_SHOW);forceForeground(policyDialog);settledForeground(policyDialog);SetFocus(policyDialogView);
        check(associated(policyCanvas)==policyCanvasContext && focusCount==0,"float dialog PSViewC restored policy context");
        check(associated(policyDialogView)==policyDialogContext,"float dialog lost its IME context");
        forceForeground(fixture);settledForeground(fixture);SetFocus(policyCanvas);
        check(associated(policyCanvas)==nullptr && focusCount>=1,"return to canvas did not reapply policy");
        auto policyForeign=CreateWindowW(L"STATIC",L"Foreign policy window",WS_OVERLAPPEDWINDOW,620,0,120,100,nullptr,nullptr,module,nullptr);
        ShowWindow(policyForeign,SW_SHOW);forceForeground(policyForeign);SetFocus(policyForeign);SendMessageW(control,WM_TIMER,1,0);
        check(focusPolicy && focusCount==0 && associated(policyCanvas)==policyCanvasContext,"foreground loss did not restore policy");
        forceForeground(fixture);settledForeground(fixture);SetFocus(policyCanvas);
        check(associated(policyCanvas)==nullptr && focusCount>=1,"foreground return did not reapply policy");
        ImmAssociateContext(policyCanvas,policyForeignContext);
        SetFocus(edit);
        check(associated(policyCanvas)==policyForeignContext && focusCount==0,"policy overwrote PS replacement");
        SetFocus(policyCanvas);
        check(associated(policyCanvas)==nullptr && focusCount==1,"replacement canvas not disabled");
        SetFocus(edit);
        check(associated(policyCanvas)==policyForeignContext && focusCount==0,"policy did not restore newest PS association");
        auto rootContext=ImmCreateContext();check(rootContext!=nullptr,"policy root context");ImmAssociateContext(fixture,rootContext);
        SetFocus(policyCanvas);check(associated(policyCanvas)==nullptr && focusCount==1,"root fixture precondition");
        SetFocus(fixture);
        check(associated(fixture)==nullptr && focusCount==2,"root window was not disabled");
        SetFocus(edit);
        check(associated(fixture)==rootContext && associated(policyCanvas)==policyForeignContext && focusCount==0,"root/edit focus did not restore all policy contexts");
        SetFocus(policyCanvas);check(associated(policyCanvas)==nullptr,"policy off precondition");
        check((policy(0)&0x300)==0 && !focusPolicy && focusCount==0 && associated(policyCanvas)==policyForeignContext,"policy disable did not confirm restoration");
        check((policy(1)&0x100) && associated(policyCanvas)==nullptr,"policy re-enable persistence");
        {
            WNDCLASSW hostClass{};hostClass.lpfnWndProc=DefWindowProcW;hostClass.hInstance=module;hostClass.lpszClassName=frameEndpointClass;RegisterClassW(&hostClass);
            const auto host=CreateWindowW(frameEndpointClass,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,module,nullptr);
            SetPropW(host,frameOwnerProp,reinterpret_cast<HANDLE>(ULONG_PTR(GetCurrentProcessId())));
            check(configureFrameHost(host),"F endpoint binding failed");
            typeToolKnown=true;typeToolActive=false;frameLayerClick=nullptr;
            check(frameNonInput(),"known canvas non-input F not eligible");
            check(frameKeyboard(HC_ACTION,'F',1)==1 && frameKeyOwned,"F down not owned");
            const auto id=framePressId;
            check(frameKeyboard(HC_ACTION,'F',0x40000001)==1 && framePressId==id,"F repeat submitted twice");
            check(frameKeyboard(HC_ACTION,'F',0xc0000001)==1 && !frameKeyOwned,"F up not drained");
            SetFocus(edit);check(!frameNonInput() && !frameKeyboard(HC_ACTION,'F',1),"F input was swallowed");SetFocus(policyCanvas);
            compositionActive=true;check(!frameNonInput(),"composition eligible for F");compositionActive=false;
            WNDCLASSW paletteClass{};paletteClass.lpfnWndProc=DefWindowProcW;paletteClass.hInstance=module;paletteClass.lpszClassName=L"OWL.Palette";RegisterClassW(&paletteClass);
            const auto palette=CreateWindowW(L"OWL.Palette",L"",WS_CHILD|WS_VISIBLE,0,0,200,200,fixture,nullptr,module,nullptr);
            const auto list=CreateWindowW(L"PSViewC",L"",WS_CHILD|WS_VISIBLE,0,0,100,100,palette,nullptr,module,nullptr);
            HWND fields[5]{};int n=0;
            for(auto value:{std::pair<const wchar_t*,int>{L"ComboBox",2490},{L"ComboBox",2493},{L"Edit",6093},{L"Edit",6097},{L"ScrollBar",6098}})
                fields[n++]=CreateWindowW(value.first,L"",WS_CHILD|WS_VISIBLE,0,0,10,10,palette,reinterpret_cast<HMENU>(INT_PTR(value.second)),module,nullptr);
            check(frameLayerList(list) && !canvasTarget(list),"Layers PSViewC confused with text canvas");
            SetFocus(list);typeToolActive=true;SendMessageW(list,WM_LBUTTONDOWN,0,0);
            check(!canvasTextEditing && frameNonInput(),"type-tool Layers click entered canvas text editing");
            DestroyWindow(fields[4]);check(!frameLayerList(list),"incomplete Layers signature accepted");
            SetFocus(policyCanvas);typeToolActive=false;frameLayerClick=nullptr;DestroyWindow(palette);
            DestroyWindow(host);
            check(frameKeyboard(HC_ACTION,'F',1)==1 && frameKeyOwned,"failed notification leaked native F");
            frameKeyboard(HC_ACTION,'F',0xc0000001);check(!frameKeyOwned,"failed notification pair not drained");
            configureFrameHost(nullptr);
        }
        // Canvas text editing: type-tool click, double-click, and Enter restore
        // before PS handles the message; Esc/Ctrl+Enter/numpad Enter post the
        // disable; a composition keeps the first Esc local; UXP corrections are
        // timestamped so late evidence cannot hit a later round.
        const auto canvasPump=[&](){MSG queued{};while(PeekMessageW(&queued,nullptr,0,0,PM_REMOVE)){TranslateMessage(&queued);DispatchMessageW(&queued);}};
        const auto canvasPush=[&](UINT operation,LPARAM value){return SendMessageW(control,controlMessage,(12345<<8)|operation,value);};
        const auto canvasStatus=[&](){return SendMessageW(control,controlMessage,(12345<<8)|1,reinterpret_cast<LPARAM>(policyCanvas));};
        check((canvasPush(6,1)>>33&1) && (canvasPush(6,1)>>34&1),"type tool push not visible in state");
        canvasSawContext=false;
        SendMessageW(policyCanvas,WM_LBUTTONDOWN,0,0);
        check(canvasSawContext && associated(policyCanvas)==policyForeignContext && canvasTextEditing && focusCount==0,
              "type-tool click did not restore before PS handled it");
        check((canvasStatus()>>32&1) && (canvasStatus()>>35&1)==0,"canvas editing state bits");
        compositionActive=true;
        SendMessageW(policyCanvas,WM_KEYDOWN,VK_ESCAPE,0);canvasPump();
        check(canvasTextEditing && associated(policyCanvas)==policyForeignContext,"Esc with an active composition left editing");
        compositionActive=false;
        SendMessageW(policyCanvas,WM_KEYDOWN,VK_ESCAPE,0);canvasPump();
        check(!canvasTextEditing && focusCount==1 && associated(policyCanvas)==nullptr,"Esc without a composition did not re-disable");
        check(GetPropW(control,canvasExitTriggerProperty)==reinterpret_cast<HANDLE>(ULONG_PTR(CanvasEsc)),"Esc exit trigger not published");
        canvasPush(6,0);
        SendMessageW(policyCanvas,WM_LBUTTONDOWN,0,0);
        check(!canvasTextEditing && focusCount==1 && associated(policyCanvas)==nullptr,"non-type single click restored protection");
        SendMessageW(policyCanvas,WM_LBUTTONDBLCLK,0,0);
        check(canvasTextEditing && associated(policyCanvas)==policyForeignContext,"any-tool double click did not restore");
        SendMessageW(policyCanvas,WM_KEYDOWN,VK_ESCAPE,0);canvasPump();
        check(!canvasTextEditing && focusCount==1,"double-click round did not exit");
        canvasPush(6,1);
        SendMessageW(policyCanvas,WM_KEYDOWN,VK_RETURN,0);
        check(canvasTextEditing && associated(policyCanvas)==policyForeignContext,"type-tool Enter did not restore");
        check(GetPropW(control,canvasEntryTriggerProperty)==reinterpret_cast<HANDLE>(ULONG_PTR(CanvasEnterText)),"Enter entry trigger not published");
        testCtrlHeld=1;SendMessageW(policyCanvas,WM_KEYDOWN,VK_RETURN,0);testCtrlHeld=-1;canvasPump();
        check(!canvasTextEditing && focusCount==1 && associated(policyCanvas)==nullptr,"Ctrl+Enter did not re-disable");
        check(GetPropW(control,canvasExitTriggerProperty)==reinterpret_cast<HANDLE>(ULONG_PTR(CanvasCtrlEnter)),"Ctrl+Enter exit trigger not published");
        canvasPush(6,0);
        SendMessageW(policyCanvas,WM_LBUTTONDBLCLK,0,0);
        SendMessageW(policyCanvas,WM_KEYDOWN,VK_RETURN,1<<24);canvasPump();
        check(!canvasTextEditing && focusCount==1,"numpad Enter did not re-disable");
        check(GetPropW(control,canvasExitTriggerProperty)==reinterpret_cast<HANDLE>(ULONG_PTR(CanvasNumpadEnter)),"numpad Enter exit trigger not published");
        canvasPush(6,1);SendMessageW(policyCanvas,WM_LBUTTONDOWN,0,0);
        check(canvasTextEditing,"correction fixture entry");
        canvasPush(8,static_cast<LPARAM>(((wallMilliseconds()+1)<<1)|0));
        check(!canvasTextEditing && focusCount==1,"async not-entered correction did not roll back");
        check(GetPropW(control,canvasCorrectionsProperty)==reinterpret_cast<HANDLE>(ULONG_PTR(1)),"correction count not published");
        SendMessageW(policyCanvas,WM_LBUTTONDOWN,0,0);
        check(canvasTextEditing,"stale correction fixture entry");
        canvasPush(8,static_cast<LPARAM>(((canvasEntryWallMs-10000)<<1)|0));
        check(canvasTextEditing,"late correction hit a newer canvas round");
        SendMessageW(policyCanvas,WM_KEYDOWN,VK_ESCAPE,0);canvasPump();
        check(!canvasTextEditing && focusCount==1,"stale-correction round did not exit");
        const ULONGLONG correctionEnter=wallMilliseconds();
        canvasPush(8,static_cast<LPARAM>(((correctionEnter+1)<<1)|1));
        check(canvasTextEditing && associated(policyCanvas)==policyForeignContext && focusCount==0,"missed-entry correction did not restore");
        SendMessageW(policyCanvas,WM_KEYDOWN,VK_ESCAPE,0);canvasPump();
        check(!canvasTextEditing && focusCount==1,"correction-entry round did not exit");
        check(GetPropW(control,policyTraceCountProperty)!=nullptr,"policy trace was not published");
        check(GetPropW(control,canvasEpochProperty)!=nullptr,"canvas epoch was not published");
        HANDLE exited=CreateEventW(nullptr,TRUE,TRUE,nullptr);check(exited!=nullptr,"policy owner exit event");
        HANDLE previousOwner=owner;owner=exited;if(previousOwner)CloseHandle(previousOwner);
        SendMessageW(control,WM_TIMER,1,0);
        check(control==nullptr && !focusPolicy && focusCount==0 && associated(policyCanvas)==policyForeignContext,"owner exit did not restore and detach policy");
        UnhookWindowsHookEx(queuedHook);UnhookWindowsHookEx(hook);
        LockSetForegroundWindow(LSFW_UNLOCK);
        if(initialForeground)SetForegroundWindow(initialForeground);
        if(control)DestroyWindow(control);
        DestroyWindow(policyDialog);DestroyWindow(policyForeign);DestroyWindow(fixture);
        ImmDestroyContext(policyCanvasContext);ImmDestroyContext(policyDialogContext);ImmDestroyContext(policyForeignContext);ImmDestroyContext(rootContext);ImmDestroyContext(context);
        std::puts("PS-thread context: nonce, real association, focus/mouse/profile restore, IMM reassociation, TSF event reset/context replacement without timer or renew, 50 cross-process menu returns with independent menu IME, 20 subscription lifecycles, lease expiry, original context and unchanged HKL passed; focus policy sync canvas/root disable, Edit restore, PSFloatC exclusion, mouse/edit-key/lease immunity, input-language reapply, foreground return, PS replacement protection, owner exit restoration passed; canvas text editing type-tool click/double-click/Enter restore-before-window-procedure, non-type click stays disabled, composition-aware Esc, Ctrl/numpad Enter posted exit, timestamped async corrections and trigger/trace diagnostics passed");return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());LockSetForegroundWindow(LSFW_UNLOCK);if(initialForeground)SetForegroundWindow(initialForeground);closeChild();return 1;}
}
