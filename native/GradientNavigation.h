#pragma once

namespace GradientNavigation {
inline constexpr wchar_t ownerProperty[]=L"PhotoshopMenu.GradientNavigation.Owner.v1";
inline constexpr wchar_t imageProperty[]=L"PhotoshopMenu.GradientNavigation.Image.v1";
inline constexpr wchar_t canvasInputProperty[]=L"PhotoshopMenu.GradientNavigation.CanvasInput.v1";
inline constexpr wchar_t sampleProperty[]=L"PhotoshopMenu.GradientNavigation.Sample.v1";
inline constexpr UINT cancelSample=WM_APP+34;

inline bool editorClass(const wchar_t* name){
    return wcscmp(name,L"PhotoshopMenuImGui")==0 || wcscmp(name,L"PhotoshopGradientImGui")==0 ||
        wcscmp(name,L"PhotoshopGradientHandles")==0;
}

inline bool sampling(HWND editor){return editor && GetPropW(editor,sampleProperty)==editor;}

inline bool canvasInput(HWND hit,HWND editor){
    return hit && editor && GetPropW(hit,canvasInputProperty)==editor && GetWindow(hit,GW_OWNER)==editor;
}

inline bool postCanvasButton(HWND hit,UINT message,POINT at){
    return ScreenToClient(hit,&at) &&
        PostMessageW(hit,message,message==WM_LBUTTONDOWN?MK_LBUTTON:0,MAKELPARAM(at.x,at.y));
}

inline bool windowClass(HWND hwnd,const wchar_t* expected) {
    wchar_t name[32]{};
    return GetClassNameW(hwnd,name,32) && wcscmp(name,expected)==0;
}

inline HWND findImage(HWND canvas,const RECT& surface,RECT& bounds) {
    HWND image=nullptr;
    for(HWND child=GetWindow(canvas,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
        RECT rect{};
        if(!IsWindowVisible(child) || !windowClass(child,L"Static") || !GetWindowRect(child,&rect) ||
            rect.left!=surface.left || rect.top!=surface.top || rect.right>surface.right ||
            rect.bottom>surface.bottom || IsRectEmpty(&rect))continue;
        if(image)return nullptr;
        image=child;bounds=rect;
    }
    return image;
}

inline bool scrollbar(HWND hwnd,HWND canvas,HWND image) {
    RECT rect{},clip{};
    if(GetParent(hwnd)!=canvas || !windowClass(hwnd,L"ScrollBar") ||
        !GetWindowRect(hwnd,&rect) || !GetWindowRect(image,&clip))return false;
    if(GetWindowLongPtrW(hwnd,GWL_STYLE)&SBS_VERT)
        return rect.left==clip.right && rect.top==clip.top && rect.bottom==clip.bottom;
    return rect.top==clip.bottom && rect.left>=clip.left && rect.right==clip.right;
}

inline bool key(int value,bool ctrl,bool alt,bool windows) {
    if(windows)return true;
    if(value==VK_ESCAPE || value==VK_SPACE || value==VK_CONTROL || value==VK_SHIFT ||
        value==VK_MENU || value==VK_LWIN || value==VK_RWIN)return true;
    if((alt && (value==VK_TAB || value==VK_F4)) || (ctrl && value==VK_ESCAPE))return true;
    if((value=='Z' || value=='R') && !ctrl && !alt)return true;
    return value!=VK_DELETE && MapVirtualKeyW(value,MAPVK_VK_TO_CHAR)==0;
}

inline bool held() {
    return (GetAsyncKeyState(VK_SPACE)&0x8000) ||
        (((GetAsyncKeyState('Z')|GetAsyncKeyState('R'))&0x8000) &&
         !((GetAsyncKeyState(VK_CONTROL)|GetAsyncKeyState(VK_MENU)|GetAsyncKeyState(VK_LWIN)|GetAsyncKeyState(VK_RWIN))&0x8000));
}

inline bool pointer(int button,bool image,bool scrollbar,bool navigating) {
    return navigating && ((button<=0 && (image || scrollbar)) || (button==2 && image));
}
}
