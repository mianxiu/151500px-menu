#pragma once
#include "FrameKeyProtocol.h"
static HWND frameHost=nullptr,frameLayerClick=nullptr;
static ULONGLONG frameInputEpoch=1,framePressId=0,framePressTick=0;
static bool frameKeyOwned=false,frameDrainUp=false,frameTypePending=false;
static UINT frameBeginMessage=0,frameCommitMessage=0,frameRestoreMessage=0,frameReleaseMessage=0;
static bool frameLayerList(HWND window) {
    if(!window || !IsWindowVisible(window) || GetAncestor(window,GA_ROOTOWNER)!=root)return false;
    wchar_t name[32]{};GetClassNameW(window,name,32);
    if(wcscmp(name,L"PSViewC")!=0 || GetDlgCtrlID(window)!=0)return false;
    HWND palette=GetParent(window);
    for(int i=0;palette && palette!=root && i<32;++i,palette=GetParent(palette)) {
        GetClassNameW(palette,name,32);if(wcscmp(name,L"OWL.Palette")!=0)continue;
        struct Controls {unsigned mask=0;int visited=0;} controls;
        EnumChildWindows(palette,[](HWND child,LPARAM value)->BOOL {
            auto& c=*reinterpret_cast<Controls*>(value);if(++c.visited>256)return FALSE;
            wchar_t type[32]{};GetClassNameW(child,type,32);const auto id=GetDlgCtrlID(child);
            if(wcscmp(type,L"ComboBox")==0){if(id==2490)c.mask|=1;if(id==2493)c.mask|=2;}
            if(wcscmp(type,L"Edit")==0){if(id==6093)c.mask|=4;if(id==6097)c.mask|=8;if(id==6092)c.mask|=32;}
            if(wcscmp(type,L"ScrollBar")==0){if(id==6098)c.mask|=16;if(id==6094)c.mask|=64;}
            return TRUE;
        },reinterpret_cast<LPARAM>(&controls));
        return controls.visited<=256 && ((controls.mask&31)==31 || (controls.mask&103)==103);
    }
    return false;
}
static void frameInputChanged(const CWPSTRUCT& event) {
    if(event.hwnd==control)return;
    if(event.message==WM_KEYDOWN && event.wParam!='F'){
        frameLayerClick=nullptr;
        if(event.wParam=='T' && !(GetKeyState(VK_CONTROL)&0x8000) && !(GetKeyState(VK_MENU)&0x8000)){typeToolKnown=false;frameTypePending=true;}
        ++frameInputEpoch;if(control)SetPropW(control,frameCurrentProp,reinterpret_cast<HANDLE>(frameInputEpoch));return;
    }
    if(event.message==WM_LBUTTONDOWN || event.message==WM_LBUTTONDBLCLK || event.message==WM_RBUTTONDOWN) {
        frameLayerClick=frameLayerList(event.hwnd)?event.hwnd:nullptr;
    } else if(event.message!=WM_SETFOCUS && event.message!=WM_KILLFOCUS && event.message!=WM_INPUTLANGCHANGE && event.message!=WM_ACTIVATEAPP && event.message!=WM_IME_STARTCOMPOSITION && event.message!=WM_IME_ENDCOMPOSITION)return;
    ++frameInputEpoch;
    if(control)SetPropW(control,frameCurrentProp,reinterpret_cast<HANDLE>(frameInputEpoch));
}
static bool frameNonInput(bool* executable=nullptr) {
    if(executable)*executable=false;
    if(!focusPolicy || !frameHost || !policyForeground())return false;
    GUITHREADINFO info{};info.cbSize=sizeof(info);
    if(!GetGUIThreadInfo(0,&info) || !info.hwndFocus || info.hwndCaret || info.hwndMenuOwner || info.hwndMoveSize)return false;
    const HWND focus=info.hwndFocus;
    if(!ownFocus(focus))return false;
    wchar_t name[32]{};GetClassNameW(focus,name,32);
    if(wcscmp(name,L"Edit")==0 || wcscmp(name,L"RichEdit20W")==0 || wcscmp(name,L"RichEdit50W")==0 || wcscmp(name,L"ComboBox")==0)return false;
    if(compositionBusy(focus))return false;
    const bool layer=frameLayerList(focus) || (frameLayerClick && frameLayerList(frameLayerClick) && (focus==root || canvasTarget(focus)));
    const bool known=layer || (protectedFocus(focus) && typeToolKnown && !canvasTextEditing && !entryPending && !compositionActive);
    if(executable)*executable=known && !info.hwndCapture && !canvasTextEditing && !entryPending;
    return known;
}
static void frameEnd(bool restore,bool released=false) {
    if(!frameKeyOwned)return;
    SetPropW(control,frameReleasedProp,reinterpret_cast<HANDLE>(released?framePressId:0));
    PostMessageW(frameHost,restore?frameRestoreMessage:frameCommitMessage,WPARAM(framePressId),reinterpret_cast<LPARAM>(control));
    frameKeyOwned=false;
}
static bool frameKeyboard(int code,WPARAM key,LPARAM data,DWORD messageTime=DWORD(GetMessageTime())) {
    if(code!=HC_ACTION || key!='F')return false;
    const auto bits=static_cast<ULONG_PTR>(data);const bool up=(bits&0x80000000U)!=0,repeat=(bits&0x40000000U)!=0;
    if(up && !frameKeyOwned) {
        const auto active=reinterpret_cast<ULONG_PTR>(GetPropW(frameHost,frameActiveProp));
        if(active){SetPropW(control,frameReleaseTimeProp,reinterpret_cast<HANDLE>(ULONG_PTR(messageTime)));PostMessageW(frameHost,frameReleaseMessage,active,reinterpret_cast<LPARAM>(control));}
        if(frameDrainUp){frameDrainUp=false;return 1;}
    }
    if(!up && !repeat)frameDrainUp=false;
    if(frameKeyOwned){if(up)frameEnd(!policyForeground() || DWORD(messageTime-DWORD(framePressTick))>=250,true);return 1;}
    bool executable=false;
    const bool plain=!(GetKeyState(VK_CONTROL)&0x8000) && !(GetKeyState(VK_SHIFT)&0x8000) && !(GetKeyState(VK_MENU)&0x8000) && !(GetKeyState(VK_CAPITAL)&0x8000) && !(GetKeyState(VK_LWIN)&0x8000) && !(GetKeyState(VK_RWIN)&0x8000);
    if(up || !plain || !frameNonInput(&executable))return false;
    // Known non-input F never falls back to PS, including notification failure and repeats.
    if(repeat)return 1;
    SetPropW(control,frameReleasedProp,nullptr);
    frameKeyOwned=true;const auto now=GetTickCount64();framePressTick=now-DWORD(DWORD(now)-messageTime);framePressId=now>framePressId?now:framePressId+1;
    if(!executable)return 1;
    SetPropW(control,frameRootProp,root);SetPropW(control,frameFocusProp,GetFocus());
    SetPropW(control,frameIdProp,reinterpret_cast<HANDLE>(framePressId));
    SetPropW(control,frameEpochProp,reinterpret_cast<HANDLE>(frameInputEpoch));
    SetPropW(control,frameTickProp,reinterpret_cast<HANDLE>(framePressTick));
    SetPropW(control,frameMessageTimeProp,reinterpret_cast<HANDLE>(ULONG_PTR(messageTime)));
    PostMessageW(frameHost,frameBeginMessage,WPARAM(framePressId),reinterpret_cast<LPARAM>(control));
    return 1;
}
static bool configureFrameHost(HWND host) {
    if(host==frameHost)return true;
    if(!host){frameEnd(true);frameHost=nullptr;SetPropW(control,frameHostProp,nullptr);return true;}
    wchar_t name[64]{};GetClassNameW(host,name,64);
    if(wcscmp(name,frameEndpointClass)!=0 || reinterpret_cast<ULONG_PTR>(GetPropW(host,frameOwnerProp))!=ownerId)return false;
    frameBeginMessage=RegisterWindowMessageW(frameBeginName);frameCommitMessage=RegisterWindowMessageW(frameCommitName);frameRestoreMessage=RegisterWindowMessageW(frameRestoreName);frameReleaseMessage=RegisterWindowMessageW(frameReleaseName);
    if(!frameBeginMessage || !frameCommitMessage || !frameRestoreMessage)return false;
    frameHost=host;SetPropW(control,frameRootProp,root);SetPropW(control,frameHostProp,host);SetPropW(control,frameCurrentProp,reinterpret_cast<HANDLE>(frameInputEpoch));return true;
}
