#pragma once

struct GradientLayerTint {
    struct Pixels {
        HDC dc=nullptr;
        HBITMAP image=nullptr;
        HGDIOBJ previous=nullptr;
        uint32_t* data=nullptr;
        SIZE size{};
        ~Pixels(){reset();if(dc)DeleteDC(dc);}
        void reset(){
            if(!image)return;
            SelectObject(dc,previous);DeleteObject(image);
            image=nullptr;data=nullptr;
        }
        bool read(HDC source,int x,int y,int width,int height){
            if(!dc)dc=CreateCompatibleDC(source);
            if(!dc)return false;
            if(!image || width!=size.cx || height!=size.cy){
                reset();
                BITMAPINFO info{};
                info.bmiHeader={sizeof(BITMAPINFOHEADER),width,-height,1,32,BI_RGB};
                void* bits=nullptr;
                image=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
                if(!image)return false;
                previous=SelectObject(dc,image);data=static_cast<uint32_t*>(bits);
                size={width,height};
            }
            return BitBlt(dc,0,0,width,height,source,x,y,SRCCOPY) && GdiFlush();
        }
    } strip, edge;

    HWND window=nullptr,view=nullptr,bar=nullptr;
    bool registered=false;
    RECT painted{};
    ULONGLONG retryAt=0;
    static constexpr wchar_t name[]=L"PhotoshopGradientLayerTint";
    ~GradientLayerTint(){
        if(window)DestroyWindow(window);
        if(registered)UnregisterClassW(name,GetModuleHandleW(nullptr));
    }
    void hide(){if(window && IsWindowVisible(window))ShowWindow(window,SW_HIDE);}
    static bool controls(HWND view){
        return GradientNavigation::windowClass(view,L"PSViewC") &&
            GradientNavigation::windowClass(GetDlgItem(view,2490),L"ComboBox") &&
            GradientNavigation::windowClass(GetDlgItem(view,2493),L"ComboBox");
    }
    bool find(HWND owner){
        if(view && IsWindowVisible(view) && GetAncestor(view,GA_ROOTOWNER)==owner &&
           GetParent(bar)==view && controls(view))return true;
        if(GetTickCount64()<retryAt)return false;
        retryAt=GetTickCount64()+500;
        struct Match {HWND owner,view=nullptr,bar=nullptr;bool ambiguous=false;} match{owner};
        EnumWindows([](HWND top,LPARAM state)->BOOL {
            auto& m=*reinterpret_cast<Match*>(state);
            if(!IsWindowVisible(top) || GetAncestor(top,GA_ROOTOWNER)!=m.owner)return TRUE;
            EnumChildWindows(top,[](HWND hwnd,LPARAM state)->BOOL {
                auto& m=*reinterpret_cast<Match*>(state);
                if(!IsWindowVisible(hwnd) || !controls(hwnd))return TRUE;
                const HWND bar=GetDlgItem(hwnd,6098)?GetDlgItem(hwnd,6098):GetDlgItem(hwnd,6094);
                if(!GradientNavigation::windowClass(bar,L"ScrollBar"))return TRUE;
                if(m.view){m.ambiguous=true;return FALSE;}
                m.view=hwnd;m.bar=bar;return TRUE;
            },state);
            return !m.ambiguous;
        },reinterpret_cast<LPARAM>(&match));
        view=match.ambiguous?nullptr:match.view;bar=match.bar;
        return view!=nullptr;
    }
    struct Band {int top=0,bottom=0;uint32_t color=0;};
    static Band selected(const uint32_t* pixels,int height,int minimum,int maximum,uint32_t gray,uint32_t blue){
        Band range{};
        for(int y=0;y<height;){
            const auto color=pixels[y*3]&0xffffff;
            if((color!=gray && color!=blue) || (pixels[y*3+1]&0xffffff)!=color ||
                (pixels[y*3+2]&0xffffff)!=color){++y;continue;}
            const int first=y;
            do{++y;}while(y<height && (pixels[y*3]&0xffffff)==color &&
                (pixels[y*3+1]&0xffffff)==color && (pixels[y*3+2]&0xffffff)==color);
            if(y-first<minimum)continue;
            if(y-first>maximum || range.bottom)return {};
            range={first,y,color};
        }
        return range;
    }
    RECT row(HWND owner,const std::string& theme,bool preview){
        RECT client{},scroll{};
        POINT origin{};
        if(!find(owner) || !GetClientRect(view,&client) || !ClientToScreen(view,&origin) ||
            !GetWindowRect(bar,&scroll))return {};
        OffsetRect(&client,origin.x,origin.y);
        const int right=scroll.left-origin.x,top=scroll.top-origin.y,height=scroll.bottom-scroll.top;
        const UINT dpi=std::lround(ImGui_ImplWin32_GetDpiScaleForHwnd(view)*96);
        if(!dpi || right<MulDiv(70,dpi,96) || right>client.right-origin.x ||
           top<0 || scroll.bottom>client.bottom || height<1 || height>8192)return {};
        const int n=theme=="light"?0:theme=="light-gray"?1:theme=="dark-gray"?2:3;
        constexpr uint32_t gray[]={0xb5b5b5,0x8a8a8a,0x6b6b6b,0x4a4a4a};
        constexpr uint32_t blue[]={0xb3cbe7,0x8aa3c1,0x50667c,0x314960};
        HDC source=GetDC(view);
        if(!source)return {};
        RECT result{};
        // shortcut: recognize the four shipped themes, missing or modified styles leave the row untouched.
        if(strip.read(source,right-3,top,3,height)){
            const auto band=selected(strip.data,height,MulDiv(8,dpi,96),MulDiv(96,dpi,96),gray[n],blue[n]);
            if(band.bottom && edge.read(source,0,top+band.top,right,1)){
                int left=right;
                while(left>0 && (edge.data[left-1]&0xffffff)==band.color)--left;
                // The native highlight begins just after the visibility column.
                if(left>=MulDiv(12,dpi,96) && left<=MulDiv(52,dpi,96))
                    result={origin.x+left,scroll.top+band.top,scroll.left-3,scroll.top+band.bottom};
            }
        }
        ReleaseDC(view,source);
        if(!IsRectEmpty(&result)){
            const HWND visible=WindowFromPoint({scroll.left-2,(result.top+result.bottom)/2});
            // Photoshop can disable Layers input during its modal preview without hiding the list.
            if(visible!=view && !IsChild(view,visible) && !(preview && visible==GetAncestor(view,GA_ROOT)))return {};
        }
        return result;
    }
    void update(HWND editor,HWND owner,const std::string& theme,COLORREF color,bool preview){
        const auto bounds=row(owner,theme,preview);
        if(IsRectEmpty(&bounds)){hide();return;}
        if(!window){
            const HINSTANCE instance=GetModuleHandleW(nullptr);
            if(!registered){
                WNDCLASSW wc{};wc.hInstance=instance;wc.lpszClassName=name;wc.lpfnWndProc=DefWindowProcW;
                wc.hbrBackground=CreateSolidBrush(color);
                if(!wc.hbrBackground)return;
                if(!RegisterClassW(&wc)){DeleteObject(wc.hbrBackground);return;}
                registered=true;
            }
            window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,name,L"",
                WS_POPUP,0,0,0,0,editor,nullptr,instance,nullptr);
            if(!window)return;
            if(!SetLayeredWindowAttributes(window,0,82,LWA_ALPHA)){
                DestroyWindow(window);window=nullptr;return;
            }
        }
        if(IsWindowVisible(window) && EqualRect(&bounds,&painted))return;
        const HWND top=GetAncestor(view,GA_ROOT);
        HWND above=GetWindow(top,GW_HWNDPREV);
        while(above && GetAncestor(above,GA_ROOTOWNER)==editor)above=GetWindow(above,GW_HWNDPREV);
        if(SetWindowPos(window,above?above:HWND_TOP,bounds.left,bounds.top,bounds.right-bounds.left,
            bounds.bottom-bounds.top,SWP_NOACTIVATE|SWP_SHOWWINDOW))painted=bounds;
        else hide();
    }
};
