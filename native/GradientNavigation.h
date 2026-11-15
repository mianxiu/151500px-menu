#pragma once

namespace GradientNavigation {
inline constexpr wchar_t ownerProperty[]=L"PhotoshopMenu.GradientNavigation.Owner.v1";
inline constexpr wchar_t imageProperty[]=L"PhotoshopMenu.GradientNavigation.Image.v1";
inline constexpr wchar_t toolProperty[]=L"PhotoshopMenu.GradientNavigation.Tool.v1";

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
    if(value=='Z' && !ctrl && !alt)return true;
    return value!=VK_DELETE && MapVirtualKeyW(value,MAPVK_VK_TO_CHAR)==0;
}

inline bool pointer(int button,bool image,bool scrollbar,bool navigating) {
    if(button<0)return image || scrollbar;
    if(button==0)return scrollbar || (image && navigating);
    return button==2 && image && navigating;
}
}
