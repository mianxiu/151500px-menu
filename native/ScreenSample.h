#pragma once
// USER32-owned state avoids a backend round trip between arming and clicking.
// A marker alone never authorizes outside clicks: the visible foreground menu
// must still own Win32 capture on its own GUI thread.
static constexpr wchar_t screenSampleProperty[]=L"PhotoshopMenu.ScreenSample.Capture";
static bool screenSampleCaptured(HWND menu,HWND foreground) {
    if(!menu || foreground!=menu || !IsWindowVisible(menu) || GetPropW(menu,screenSampleProperty)!=menu)return false;
    GUITHREADINFO info{};info.cbSize=sizeof(info);
    const DWORD thread=GetWindowThreadProcessId(menu,nullptr);
    return thread && GetGUIThreadInfo(thread,&info) && info.hwndCapture==menu;
}
struct ScreenSampleClicks {
    std::array<bool,2> held{};
    // 0: ordinary processing; 1: let capture deliver to menu; 2: consume up.
    int route(int index,bool up,bool captured) {
        if(index<0 || index>1)return 0;
        if(up && held[index]){held[index]=false;return index==1 || !captured?2:1;}
        if(!up && captured){held[index]=true;return 1;}
        return 0;
    }
};
