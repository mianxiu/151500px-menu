// An owned, non-activating button. The document and isolation identity are checked again in PS.
class IsolationHintPanel {
    inline static HWND hint=nullptr,owner=nullptr;
    inline static HFONT font=nullptr;
    inline static int fontPixels=0;
    inline static std::string identity,pressed;
    inline static constexpr wchar_t label[]=L"图层隔离中  ·  退出";
    static bool foreground(){return owner && GetAncestor(GetForegroundWindow(),GA_ROOTOWNER)==owner && IsWindowEnabled(owner);}
    static void place(){
        if(!foreground()){ShowWindow(hint,SW_HIDE);return;}
        RECT canvas{};
        EnumChildWindows(owner,[](HWND child,LPARAM data)->BOOL{
            RECT surface{};
            if(IsWindowVisible(child) && GradientNavigation::windowClass(child,L"PSViewC") && GetWindowRect(child,&surface) &&
               GradientNavigation::findImage(child,surface,*reinterpret_cast<RECT*>(data)))return FALSE;
            return TRUE;
        },reinterpret_cast<LPARAM>(&canvas));
        if(IsRectEmpty(&canvas)){ShowWindow(hint,SW_HIDE);return;}
        const float scale=ImGui_ImplWin32_GetDpiScaleForHwnd(owner);
        const int dpi=int(std::lround(scale*96)),pad=int(std::lround(MenuControls::PanelStyle::padding(scale)));
        const int em=MulDiv(int(menuFontEms[menuFontIndex(MenuTextRole::Secondary)]),dpi,96);
        if(!font || em!=fontPixels){
            if(font)DeleteObject(font);
            fontPixels=em;font=CreateFontW(-em,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
        }
        HDC dc=GetDC(hint);auto old=SelectObject(dc,font);SIZE size{};
        GetTextExtentPoint32W(dc,label,lstrlenW(label),&size);SelectObject(dc,old);ReleaseDC(hint,dc);
        int w=size.cx+4*pad,h=std::max(int(size.cy)+2*pad,MulDiv(int(menuControlHeight),dpi,96));
        RECT rect{};GetWindowRect(hint,&rect);
        if(w!=rect.right-rect.left || h!=rect.bottom-rect.top){
            int r=MulDiv(int(figmaCornerRadius*2),dpi,96);SetWindowRgn(hint,CreateRoundRectRgn(0,0,w+1,h+1,r,r),FALSE);
        }
        if(!IsWindowVisible(hint) || rect.left!=canvas.left+pad || rect.top!=canvas.top+pad ||
           w!=rect.right-rect.left || h!=rect.bottom-rect.top)
            SetWindowPos(hint,HWND_TOP,canvas.left+pad,canvas.top+pad,w,h,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    }
    static LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
        switch(msg){
        case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
        case WM_TIMER:place();return 0;
        case WM_LBUTTONDOWN:if(foreground()){pressed=identity;SetCapture(hwnd);}return 0;
        case WM_CAPTURECHANGED:pressed.clear();return 0;
        case WM_LBUTTONUP:{
            RECT r{};GetClientRect(hwnd,&r);POINT p={short(LOWORD(lp)),short(HIWORD(lp))};
            bool clicked=!pressed.empty() && pressed==identity && PtInRect(&r,p) && foreground();
            if(GetCapture()==hwnd)ReleaseCapture();
            if(clicked)std::cout<<Json{{"type","action"},{"action","isolation.exit"},{"params",{{"id",identity}}}}.dump()<<std::endl;
            return 0;
        }
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{
            PAINTSTRUCT ps{};auto dc=BeginPaint(hwnd,&ps);RECT r{};GetClientRect(hwnd,&r);
            SetDCBrushColor(dc,menuTipBackground&0xffffff);FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,menuTipText&0xffffff);
            DrawTextW(dc,label,-1,&r,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);
            SelectObject(dc,old);EndPaint(hwnd,&ps);return 0;
        }}
        return DefWindowProcW(hwnd,msg,wp,lp);
    }
public:
    static void clear(){
        pressed.clear();identity.clear();
        if(hint){DestroyWindow(hint);hint=nullptr;}owner=nullptr;
        if(font){DeleteObject(font);font=nullptr;}
    }
    static void update(const Json& event){
        clear();identity=event.value("id",std::string());owner=reinterpret_cast<HWND>(event.value("owner",uintptr_t(0)));
        if(identity.size()!=32 || !IsWindow(owner))return;
        WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=GetModuleHandleW(nullptr);
        wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32649));wc.lpszClassName=L"PhotoshopMenuIsolationHint";RegisterClassW(&wc);
        hint=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_LAYERED,wc.lpszClassName,label,WS_POPUP,0,0,1,1,owner,nullptr,wc.hInstance,nullptr);
        if(hint){SetLayeredWindowAttributes(hint,0,BYTE(menuTipBackground>>IM_COL32_A_SHIFT),LWA_ALPHA);place();SetTimer(hint,1,300,nullptr);}
    }
};
