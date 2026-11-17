#pragma once

struct GradientCanvasPanel {
    bool legacyPreview=false;
    struct Point {
        double x=0,y=0;
        Point operator+(Point p) const{return {x+p.x,y+p.y};}
        Point operator-(Point p) const{return {x-p.x,y-p.y};}
        Point operator*(double n) const{return {x*n,y*n};}
        double dot(Point p) const{return x*p.x+y*p.y;}
        double length() const{return std::hypot(x,y);}
    };
    struct View {
        std::array<double,6> m{};
        std::array<double,4> layer{};
        RECT surface{},clip{};
        Point document;
        HWND canvas=nullptr;
        HWND image=nullptr;
        HWND owner=nullptr;
        DWORD pid=0;
        bool ambiguous=false;
        double dpi=1;
        bool haveLayer=false;
        Point fromScreen(Point p) const{
            p=(p-Point{double(surface.left),double(surface.top)})*(1/dpi);
            return {m[0]*p.x+m[2]*p.y+m[4],m[1]*p.x+m[3]*p.y+m[5]};
        }
        Point toScreen(Point p) const{
            p=p-Point{m[4],m[5]};
            const double q=dpi/(m[0]*m[3]-m[1]*m[2]);
            return {surface.left+q*(m[3]*p.x-m[2]*p.y),surface.top+q*(-m[1]*p.x+m[0]*p.y)};
        }
        static RECT rect(const Json& v){
            return {std::lround(v.at(0).get<double>()),std::lround(v.at(1).get<double>()),
                std::lround(v.at(2).get<double>()),std::lround(v.at(3).get<double>())};
        }
        static bool equal(RECT a,RECT b){return EqualRect(&a,&b)!=0;}
        static BOOL CALLBACK match(HWND h,LPARAM data){
            auto& view=*reinterpret_cast<View*>(data);
            RECT actual{};
            if(IsWindowVisible(h) && GradientNavigation::windowClass(h,L"PSViewC") &&
                GetWindowRect(h,&actual) && equal(actual,view.surface)){
                RECT clip{};
                if(const HWND image=GradientNavigation::findImage(h,view.surface,clip)){
                    if(view.canvas){view.ambiguous=true;return FALSE;}
                    view.canvas=h;view.image=image;view.clip=clip;
                }
            }
            return TRUE;
        }
        bool bind(const Json& value,HWND owner){
            *this={};
            m=value.at("matrix").get<decltype(m)>();
            if(!std::isfinite(m[0]*m[3]-m[1]*m[2]) || std::abs(m[0]*m[3]-m[1]*m[2])<1e-8)return false;
            const auto dimensions=value.at("document").get<std::array<double,2>>();
            document={dimensions[0],dimensions[1]};
            surface=rect(value.at("viewport"));
            this->owner=owner;GetWindowThreadProcessId(owner,&pid);
            if(!value.at("layer").is_null()){
                layer=value.at("layer").get<decltype(layer)>();
                haveLayer=layer[2]>layer[0] && layer[3]>layer[1];
            }
            EnumChildWindows(owner,match,reinterpret_cast<LPARAM>(this));
            if(!canvas){
                EnumWindows([](HWND h,LPARAM data)->BOOL{
                    auto& v=*reinterpret_cast<View*>(data);
                    if(h==v.owner || GetAncestor(h,GA_ROOTOWNER)!=v.owner)return TRUE;
                    wchar_t name[32]{};
                    if(!GetClassNameW(h,name,32) || wcscmp(name,L"OWL.DocumentWindow"))return TRUE;
                    EnumChildWindows(h,match,data);
                    return !v.ambiguous;
                },reinterpret_cast<LPARAM>(this));
            }
            if(!canvas || ambiguous)return false;
            dpi=ImGui_ImplWin32_GetDpiScaleForHwnd(canvas);
            const LONG width=clip.right-clip.left,height=clip.bottom-clip.top;
            return dpi>0 && width>0 && height>0 && int64_t(width)*height<=16000000;
        }
        bool current() const{
            RECT bounds{},actual{};
            DWORD currentPid=0;GetWindowThreadProcessId(canvas,&currentPid);
            return currentPid==pid && canvas && IsWindowVisible(canvas) && IsWindowEnabled(canvas) &&
                GetAncestor(canvas,GA_ROOTOWNER)==owner && image && GetParent(image)==canvas &&
                GradientNavigation::windowClass(image,L"Static") && IsWindowVisible(image) &&
                ImGui_ImplWin32_GetDpiScaleForHwnd(canvas)==dpi && GetWindowRect(canvas,&bounds) && equal(bounds,surface) &&
                GetWindowRect(image,&actual) && equal(actual,clip);
        }
    };
    struct Axis {
        Point origin,size,center,unit;
        double span=0,length=0,angle=0;
        bool linear=false,angular=false,radial=false,reflected=false,diamond=false,reverse=false;
        static constexpr double pi=3.14159265358979323846;
        bool read(const View& view,const Json& value,bool vector,double radialAngle) {
            const auto& o=value.at("options");
            if(!o.contains("offset_x") || !o.contains("offset_y"))return false;
            if(vector && o.at("align").get<bool>()){
                if(!view.haveLayer)return false;
                origin={view.layer[0],view.layer[1]};size={view.layer[2]-view.layer[0],view.layer[3]-view.layer[1]};
            }else{origin={};size=view.document;}
            const auto type=o.at("style").get<std::string>();
            linear=type=="linear";angular=type=="angle";radial=type=="radial";reflected=type=="reflected";diamond=type=="diamond";
            reverse=o.at("reverse");
            angle=(radial?radialAngle:o.at("angle").get<double>())*pi/180;
            unit={std::cos(angle),-std::sin(angle)};
            center=origin+Point{size.x*(.5+o.at("offset_x").get<double>()/100),size.y*(.5+o.at("offset_y").get<double>()/100)};
            span=radial||diamond?std::max(size.x,size.y)/2:
                (size.x*std::abs(unit.x)+size.y*std::abs(unit.y))/2;
            length=span*o.at("scale").get<double>()/100;
            return span>0 && length>0;
        }
        Point at(double t) const{return center+unit*(length*(linear?2*t-1:t));}
        Point arc(double t,double radius) const {
            const double a=angle+t*2*pi;return center+Point{std::cos(a),-std::sin(a)}*radius;
        }
    };
    enum Kind {center,range,direction,color,midpoint};
    struct Handle { Point p;Kind kind;int index;ImU32 fill;double radius;int lane=1; };
    View view;
    Axis axis;
    std::vector<Handle> handles;
    GradientPanel* panel=nullptr;
    HWND visual=nullptr,hit=nullptr,editor=nullptr;
    HWND returnFocus=nullptr;
    ULONG_PTR graphics=0;
    int active=-1,channel=-1,appliedDelta=0;
    enum Click { noClick, addStop, addOpacity, sampleColor } click=noClick;
    bool vector=false;
    double radialAngle=0;
    Point pressed{},pressedScreen{};
    Axis startAxis;
    Json startPaint,lastPaint;
    Json rampTracks=Json::array();
    std::array<ImU32,129> rampSamples{};
    bool rampAngular=false;
    HDC surfaceDC=nullptr;
    HBITMAP surfaceBitmap=nullptr;
    HGDIOBJ oldBitmap=nullptr;
    void* surfaceBits=nullptr;
    SIZE surfaceCapacity{};
    RECT lastEditor{};
    RECT pickerInputClip{},pickerInputEditor{};
    bool pickerInput=false;
    std::array<uint32_t,2> lastSelection{};
    int lastColorTarget=-1;
    static constexpr const wchar_t* hitClass=L"PhotoshopGradientHandles";
    bool foreground() const{
        const HWND front=GetForegroundWindow();
        if(front && (front==editor || front==hit))return true;
        const HWND root=GetAncestor(view.canvas,GA_ROOT);
        if(root==front)return true;
        return front==view.owner && root==view.owner;
    }
    static bool navigationKeyHeld(){
        return ((GetAsyncKeyState(VK_SPACE)|GetAsyncKeyState('Z'))&0x8000)!=0;
    }

    void suspendPointer(){
        if(hit && IsWindowVisible(hit))ShowWindow(hit,SW_HIDE);
        if(hit && GetCapture()==hit)ReleaseCapture();
        active=-1;click=noClick;
    }
    void hide(){
        suspendPointer();
        if(visual)ShowWindow(visual,SW_HIDE);
        lastPaint=nullptr;handles.clear();
    }
    void updateView(const View& updated){
        if(view.m!=updated.m || view.layer!=updated.layer || view.haveLayer!=updated.haveLayer ||
            view.dpi!=updated.dpi || view.document.x!=updated.document.x || view.document.y!=updated.document.y ||
            !View::equal(view.surface,updated.surface) || !View::equal(view.clip,updated.clip))
            lastPaint=nullptr;
        view=updated;
    }
    ~GradientCanvasPanel(){
        hide();if(hit)DestroyWindow(hit);if(visual)DestroyWindow(visual);
        if(surfaceBitmap){SelectObject(surfaceDC,oldBitmap);DeleteObject(surfaceBitmap);}
        if(surfaceDC)DeleteDC(surfaceDC);
        if(graphics)Gdiplus::GdiplusShutdown(graphics);
    }
    bool prepareSurface(int width,int height){
        if(!surfaceDC)surfaceDC=CreateCompatibleDC(nullptr);
        if(!surfaceDC)return false;
        if(surfaceBitmap && surfaceCapacity.cx>=width && surfaceCapacity.cy>=height)return true;
        if(surfaceBitmap){SelectObject(surfaceDC,oldBitmap);DeleteObject(surfaceBitmap);surfaceBitmap=nullptr;}
        surfaceCapacity={std::max(surfaceCapacity.cx,LONG((width+63)&~63)),std::max(surfaceCapacity.cy,LONG((height+63)&~63))};
        BITMAPINFO info{};
        info.bmiHeader={sizeof(BITMAPINFOHEADER),surfaceCapacity.cx,-surfaceCapacity.cy,1,32,BI_RGB};
        surfaceBitmap=CreateDIBSection(surfaceDC,&info,DIB_RGB_COLORS,&surfaceBits,nullptr,0);
        if(!surfaceBitmap)return false;
        oldBitmap=SelectObject(surfaceDC,surfaceBitmap);
        return true;
    }
    static LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
        if(msg==WM_NCCREATE){
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,LPARAM(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
        }
        auto* self=reinterpret_cast<GradientCanvasPanel*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(self){
            if(msg==WM_MOUSEACTIVATE){
                if(HIWORD(lp)!=WM_LBUTTONDOWN || !self->foreground() || !self->view.current() ||
                    !self->panel->editable() || navigationKeyHeld())return MA_NOACTIVATE;
                self->returnFocus=GetForegroundWindow();
                // Keep Photoshop focused; activating a layered hit surface can leave no foreground window.
                return MA_NOACTIVATE;
            }
            if(msg==WM_KEYDOWN && wp==VK_ESCAPE){PostMessageW(self->editor,WM_CLOSE,0,0);return 0;}
            if(msg==WM_SETCURSOR && LOWORD(lp)==HTCLIENT && self->foreground() && !navigationKeyHeld()){
                POINT p{};
                if(GetCursorPos(&p) && self->clickAt({double(p.x),double(p.y)})==sampleColor){
                    SetCursor(LoadCursorW(nullptr,IDC_CROSS));return TRUE;
                }
            }
            if(msg==WM_CAPTURECHANGED){self->active=-1;self->click=noClick;return 0;}
            if(msg==WM_LBUTTONDOWN || msg==WM_MOUSEMOVE || msg==WM_LBUTTONUP){
                POINT point{short(LOWORD(lp)),short(HIWORD(lp))};ClientToScreen(hwnd,&point);
                if(msg==WM_LBUTTONDOWN)self->begin({double(point.x),double(point.y)});
                else if(self->active>=0 || self->click!=noClick){
                    const Point at{double(point.x),double(point.y)};
                    if(self->active>=0)self->drag(at);
                    if(msg==WM_LBUTTONUP){
                        self->finishClick(at);
                        if(self->active>=0)self->panel->flush(true);
                        self->click=noClick;
                        self->active=-1;if(GetCapture()==hwnd)ReleaseCapture();
                        if(GetForegroundWindow()==hwnd && IsWindowVisible(self->returnFocus))
                            SetForegroundWindow(self->returnFocus);
                    }
                }
                return 0;
            }
        }
        return DefWindowProcW(hwnd,msg,wp,lp);
    }
    bool create(HWND host,GradientPanel& model){
        if(hit && visual)return true;
        Gdiplus::GdiplusStartupInput input;
        if(Gdiplus::GdiplusStartup(&graphics,&input,nullptr)!=Gdiplus::Ok)return false;
        WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=GetModuleHandleW(nullptr);
        wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=hitClass;
        if(!RegisterClassW(&wc))return false;
        constexpr DWORD style=WS_EX_LAYERED|WS_EX_TOOLWINDOW;
        editor=host;panel=&model;
        // Keeping the window group on this thread prevents Photoshop's modal tool from blocking its cleanup.
        visual=CreateWindowExW(style|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE,hitClass,L"",WS_POPUP,0,0,0,0,host,nullptr,wc.hInstance,nullptr);
        hit=CreateWindowExW(style|WS_EX_NOACTIVATE,hitClass,L"",WS_POPUP,0,0,0,0,host,nullptr,wc.hInstance,this);
        if(hit && visual && SetLayeredWindowAttributes(hit,0,1,LWA_ALPHA)){
            if(legacyPreview)SetPropW(hit,GradientNavigation::canvasInputProperty,editor);
            return true;
        }
        if(hit)DestroyWindow(hit);if(visual)DestroyWindow(visual);
        hit=visual=nullptr;
        UnregisterClassW(hitClass,wc.hInstance);
        Gdiplus::GdiplusShutdown(graphics);graphics=0;
        return false;
    }
    int at(Point p) const {
        int index=-1;double closest=1e10;
        for(int i=0;i<int(handles.size());++i){
            const double d=(p-handles[i].p).length();
            if(d<=handles[i].radius && d<closest){closest=d;index=i;}
        }
        return index;
    }
    double opacityOffset() const{
        return -(menuGradientBarHeight*.25+menuGradientHandleSize*.4+menuPanelGap)*view.dpi;
    }
    Point railPoint(double t,double offset=0) const{
        if(axis.angular){
            const auto base=view.toScreen(axis.arc(t,std::min(axis.size.x,axis.size.y)/3));
            const auto outward=base-view.toScreen(axis.center);
            return base+outward*(offset/std::max(.001,outward.length()));
        }
        const Point delta=view.toScreen(axis.at(1))-view.toScreen(axis.at(0));
        const Point unit=delta*(1/std::max(.001,delta.length()));
        return view.toScreen(axis.at(t))+Point{-unit.y,unit.x}*offset;
    }
    int stripLocation(Point p,int lane=1) const {
        double t=0;
        if(axis.angular){
            const Point radial=view.fromScreen(p)-axis.center;
            t=std::fmod((std::atan2(-radial.y,radial.x)-axis.angle)/(2*Axis::pi)+2,1.);
        }else{
            const Point from=view.toScreen(axis.at(0)),delta=view.toScreen(axis.at(1))-from;
            const double length=delta.dot(delta);
            if(length<.001)return -1;
            t=std::clamp((p-from).dot(delta)/length,0.,1.);
        }
        const double distance=(p-railPoint(t,lane==0?opacityOffset():0)).length();
        const double tolerance=(lane==0?menuGradientHandleSize*.4:menuGradientBarHeight*.25+1)*view.dpi;
        if(distance>tolerance)return -1;
        return int(std::lround((axis.reverse?1-t:t)*GradientPanel::end));
    }
    Click clickAt(Point p) {
        if(!panel->editable() || panel->channel!=channel)return noClick;
        const POINT screen{LONG(p.x),LONG(p.y)};
        if(!PtInRect(&view.clip,screen))return noClick;
        if(GradientNavigation::sampling(editor) && panel->hasRgbPicker())return sampleColor;
        if(at(p)>=0)return noClick;
        if(stripLocation(p)>=0)return addStop;
        if(stripLocation(p,0)>=0)return addOpacity;
        return panel->hasRgbPicker()?sampleColor:noClick;
    }
    static COLORREF screenPixel(POINT p){
        const HDC dc=GetDC(nullptr);
        if(!dc)return CLR_INVALID;
        const auto value=GetPixel(dc,p.x,p.y);
        ReleaseDC(nullptr,dc);
        return value;
    }
    bool paintedPixel(POINT p) const{
        RECT bounds{};
        if(!surfaceBits || !IsWindowVisible(visual) || !GetWindowRect(visual,&bounds) || !PtInRect(&bounds,p))
            return false;
        const auto* pixels=static_cast<const uint32_t*>(surfaceBits);
        return (pixels[(p.y-bounds.top)*surfaceCapacity.cx+p.x-bounds.left]>>24)!=0;
    }
    void finishClick(Point p){
        if(click==noClick || (p-pressedScreen).length()>4*view.dpi || !foreground() || !view.current() ||
           navigationKeyHeld() || clickAt(p)!=click)return;
        POINT screen{LONG(p.x),LONG(p.y)};
        if(WindowFromPoint(screen)!=hit)return;
        if(click==addStop || click==addOpacity){
            const int lane=click==addOpacity?0:1;
            const int location=stripLocation(p,lane);
            if(panel->add(lane,location))panel->cursor=location;
        }else{
            // Suppress our painted guide only when it would contribute to the sample.
            const bool guide=paintedPixel(screen);
            if(guide){ShowWindow(visual,SW_HIDE);DwmFlush();}
            const COLORREF rgb=screenPixel(screen);
            if(guide)ShowWindow(visual,SW_SHOWNOACTIVATE);
            if(GradientNavigation::sampling(editor))SendMessageW(editor,GradientNavigation::cancelSample,0,0);
            if(rgb!=CLR_INVALID)panel->applyRgb(Json::array({GetRValue(rgb),GetGValue(rgb),GetBValue(rgb)}));
        }
    }
    void begin(Point p){
        if(!IsWindowVisible(hit) || !panel->editable() || !foreground() || !view.current())return;
        if(navigationKeyHeld())return;
        const int index=GradientNavigation::sampling(editor)?-1:at(p);
        if(index<0){
            click=clickAt(p);
            if(click==noClick)return;
            pressedScreen=p;
            return;
        }
        const auto handle=handles[index];
        pressed=view.fromScreen(p);startPaint=panel->paint();startAxis=axis;
        active=index;appliedDelta=0;
        if(handle.kind==color){
            panel->select(handle.lane,handle.index,(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0,
                (GetAsyncKeyState(VK_SHIFT)&0x8000)!=0);
            panel->cursor=panel->track(handle.lane)[handle.index]["location"];
            panel->picker.key.clear();
        }
        if(GetCapture()!=hit)SetCapture(hit);
        if(GetCapture()!=hit)active=-1;
    }
    static void excludeDock(HRGN region,RECT bounds,RECT timeline,RECT picker){
        for(auto r:{timeline,picker})if(!IsRectEmpty(&r)){
            OffsetRect(&r,-bounds.left,-bounds.top);
            if(HRGN panelRegion=CreateRectRgnIndirect(&r)){
                CombineRgn(region,region,panelRegion,RGN_DIFF);DeleteObject(panelRegion);
            }
        }
    }
    bool showPickerInput(RECT editorRect,RECT timeline,RECT picker){
        const auto clip=view.clip;
        const int width=clip.right-clip.left,height=clip.bottom-clip.top;
        if(!pickerInput || !View::equal(clip,pickerInputClip) || !View::equal(editorRect,pickerInputEditor)){
            HRGN region=CreateRectRgn(0,0,width,height);
            if(!region)return false;
            excludeDock(region,clip,timeline,picker);
            if(!SetWindowRgn(hit,region,FALSE)){DeleteObject(region);return false;}
            if(!SetWindowPos(hit,visual,clip.left,clip.top,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW)){
                pickerInput=false;return false;
            }
            pickerInput=true;pickerInputClip=clip;pickerInputEditor=editorRect;
        }
        if(!IsWindowVisible(hit))ShowWindow(hit,SW_SHOWNOACTIVATE);
        return IsWindowVisible(hit);
    }
    void drag(Point p){
        if(!foreground() || !view.current() || !panel->editable() || channel!=panel->channel){hide();return;}
        if(active<0 || active>=int(handles.size()))return;
        const auto h=handles[active];const Point now=view.fromScreen(p),delta=now-pressed;
        auto& paint=panel->paint();
        bool changed=false;
        const auto set=[&](Json& target,double value){
            if(target!=value){target=value;changed=true;}
        };
        auto& o=paint["options"];
        const auto& a=startAxis;
        const double sign=a.reverse?-1:1;
        if(h.kind==center){
            for(const auto& entry:std::array<std::pair<const char*,double>,2>{{{"offset_x",delta.x*100/a.size.x},{"offset_y",delta.y*100/a.size.y}}})
                set(o[entry.first],std::clamp(startPaint["options"][entry.first].get<double>()+entry.second,-1000.,1000.));
        }else if(h.kind==range){
            if(a.radial){
                const Point at=a.unit*a.length+delta;
                if(at.length()>.001){
                    const double next=std::atan2(-at.y,at.x)*180/Axis::pi;
                    if(next!=radialAngle){radialAngle=next;lastPaint=nullptr;}
                }
                set(o["scale"],std::clamp(at.length()*100/a.span,1.,1000.));
            }else set(o["scale"],std::clamp((a.length+delta.dot(a.unit))*100/a.span,1.,1000.));
        }else if(h.kind==direction){
            const Point before=pressed-a.center,after=now-a.center;
            if(after.length()>.001 && before.length()>.001){
                double angle=std::remainder(startPaint["options"]["angle"].get<double>()+
                    (std::atan2(-after.y,after.x)-std::atan2(-before.y,before.x))*180/Axis::pi,360.);
                if(GetAsyncKeyState(VK_SHIFT)&0x8000)angle=std::round(angle/45)*45;
                set(o["angle"],angle);
            }
        }else if(a.length>0){
            const Point before=pressed-a.center,after=now-a.center;
            if(a.angular && after.length()<.001)return;
            const double shift=a.angular?sign*std::remainder(std::atan2(-after.y,after.x)-
                std::atan2(-before.y,before.x),2*Axis::pi)*GradientPanel::end/(2*Axis::pi):
                sign*delta.dot(a.unit)*GradientPanel::end/(a.length*(a.linear?2:1));
            if(h.kind==color){
                if(panel->selected(h.lane,h.index)){
                    const int pixels=panel->shift(startPaint,int(std::clamp(std::round(shift),-4096.,4096.)));
                    if(pixels!=appliedDelta){panel->moveBy(startPaint,pixels);appliedDelta=pixels;}
                    panel->cursor=panel->track(h.lane)[h.index]["location"];
                }
            }else{
                auto& stops=panel->track(h.lane);
                const int gap=stops[h.index+1]["location"].get<int>()-stops[h.index]["location"].get<int>();
                const auto& prior=GradientPanel::track(startPaint,h.lane);
                if(gap>0){
                    const int midpoint=int(std::clamp(std::round(prior[h.index]["midpoint"].get<int>()+shift*100/gap),0.,100.));
                    if(stops[h.index]["midpoint"]!=midpoint){stops[h.index]["midpoint"]=midpoint;changed=true;panel->refreshBar=true;}
                }
            }
        }
        if(changed)panel->dirty=true;
    }
    bool draw(bool allowPointer){
        if(!view.current() || !panel->editable()){hide();return false;}
        const bool focused=foreground();
        allowPointer=allowPointer && focused && !navigationKeyHeld();
        if(!allowPointer && IsWindowVisible(hit))suspendPointer();
        MenuControls::PanelStyle style(float(view.dpi));
        RECT editorRect{};GetWindowRect(editor,&editorRect);
        const bool canvasPicker=panel->hasRgbPicker();
        const int colorTarget=panel->colorTarget();
        if(channel==panel->channel && lastPaint==panel->paint() && lastSelection==panel->selection && lastColorTarget==colorTarget &&
            View::equal(editorRect,lastEditor) && (!allowPointer || IsWindowVisible(hit) || (!canvasPicker && !IsWindowVisible(visual))))return true;
        if(channel!=panel->channel && GetCapture()==hit)ReleaseCapture();
        channel=panel->channel;lastEditor=editorRect;lastSelection=panel->selection;lastColorTarget=colorTarget;
        lastPaint=panel->paint();
        if(!axis.read(view,lastPaint,vector,radialAngle)){hide();return false;}
        using namespace Gdiplus;
        const auto clip=view.clip;
        const UINT dpi=std::lround(view.dpi*96);
        auto timeline=panel->dockTimeline(editorRect,dpi);
        auto picker=canvasPicker?panel->dockPicker(editorRect,dpi):RECT{};
        const auto dockTimeline=timeline,dockPicker=picker;
        const auto radius=menuGradientHandleSize*.4*view.dpi,gap=menuPanelGap*view.dpi;
        const auto c=view.toScreen(axis.center),from=view.toScreen(axis.at(0)),to=view.toScreen(axis.at(1));
        const Point delta=to-from;
        const double pixelLength=std::max(.001,delta.length());
        const Point unit=delta*(1/pixelLength),normal={-unit.y,unit.x};
        const double arcRadius=std::min(axis.size.x,axis.size.y)/3;
        const double halfWidth=menuGradientBarHeight*.25*view.dpi;
        const Point end=axis.angular?view.toScreen(axis.arc(0,arcRadius)):to;
        const auto border=ImGui::GetColorU32(ImGuiCol_Border),text=ImGui::GetColorU32(ImGuiCol_Text),
                   accent=ImGui::GetColorU32(ImGuiCol_CheckMark),surface=ImGui::GetColorU32(ImGuiCol_Button);
        const auto ink=[](ImU32 color){return Color(BYTE(color>>24),BYTE(color),BYTE(color>>8),BYTE(color>>16));};
        handles.clear();
        const auto add=[&](Point p,Kind kind,int index,ImU32 fill,double size,int lane=1){handles.push_back({p,kind,index,fill,size,lane});};
        add(c,center,0,accent,radius+view.dpi);
        if(!axis.angular)add(to,range,0,surface,radius);
        if(!axis.radial)add(c+unit*(3*gap)+normal*(halfWidth+radius+3*gap),direction,0,accent,radius);
        for(int lane:{0,1}){
            const auto& stops=panel->track(lane);
            const auto point=[&](double location,bool middle){
                const double raw=location/GradientPanel::end,t=axis.reverse?1-raw:raw;
                const double side=(lane==0?-1.:1.)*(halfWidth+radius+(middle?0:gap));
                if(axis.angular){
                    const auto base=view.toScreen(axis.arc(t,arcRadius));
                    const auto outward=base-c;
                    return base+outward*(side/std::max(.001,outward.length()));
                }
                return view.toScreen(axis.at(t))+normal*side;
            };
            for(int i=0;i<int(stops.size());++i){
                const double where=stops[i]["location"];
                auto at=point(where,false);
                if(axis.angular && where==GradientPanel::end)at=at-unit*(2*radius);
                const float gray=lane==0?stops[i]["opacity"].get<float>()/100:0;
                const auto fill=lane==0?ImGui::ColorConvertFloat4ToU32({gray,gray,gray,1}):GradientPanel::swatch(stops[i],surface);
                add(at,color,i,fill,radius,lane);
                if(i+1<int(stops.size())){
                    const double next=stops[i+1]["location"];
                    if(next>where)add(point(where+(next-where)*stops[i]["midpoint"].get<double>()/100,true),midpoint,i,surface,radius*.72,lane);
                }
            }
        }
        Point low=c,high=c;
        const auto include=[&](Point p,double padding){
            low={std::min(low.x,p.x-padding),std::min(low.y,p.y-padding)};
            high={std::max(high.x,p.x+padding),std::max(high.y,p.y+padding)};
        };
        const double margin=std::abs(opacityOffset())+radius+view.dpi;
        if(axis.angular || axis.radial || axis.diamond){
            const double extent=axis.angular?arcRadius:axis.length;
            const auto h=view.toScreen(axis.center+Point{extent,0})-c;
            const auto v=view.toScreen(axis.center+Point{0,extent})-c;
            const Point half{std::hypot(h.x,v.x),std::hypot(h.y,v.y)};
            include(c-half,margin);include(c+half,margin);
        }
        include(from,margin);include(to,margin);
        if(axis.reflected)include(c-(to-c),margin);
        for(const auto& h:handles)include(h.p,h.radius+2*view.dpi);
        const RECT inkBounds{
            LONG(std::clamp(std::floor(low.x),double(clip.left),double(clip.right))),
            LONG(std::clamp(std::floor(low.y),double(clip.top),double(clip.bottom))),
            LONG(std::clamp(std::ceil(high.x),double(clip.left),double(clip.right))),
            LONG(std::clamp(std::ceil(high.y),double(clip.top),double(clip.bottom)))};
        if(IsRectEmpty(&inkBounds)){
            if(IsWindowVisible(visual))ShowWindow(visual,SW_HIDE);
            if(canvasPicker && allowPointer)return showPickerInput(editorRect,dockTimeline,dockPicker);
            suspendPointer();return true;
        }
        const int width=inkBounds.right-inkBounds.left,height=inkBounds.bottom-inkBounds.top;
        if(!prepareSurface(width,height)){hide();return false;}
        for(auto* rect:{&timeline,&picker})OffsetRect(rect,-inkBounds.left,-inkBounds.top);
        const auto screen=[&](Point p){return PointF(float(p.x-inkBounds.left),float(p.y-inkBounds.top));};
        HRGN region=canvasPicker?nullptr:CreateRectRgn(0,0,0,0);
        if(!canvasPicker && !region){hide();return false;}
        const auto stroke=[&](Graphics& g,Point a,Point b){
            Pen edge(ink(border),float(3*view.dpi)),line(ink(text),float(view.dpi));
            g.DrawLine(&edge,screen(a),screen(b));g.DrawLine(&line,screen(a),screen(b));
        };
        {
            Bitmap image(width,height,surfaceCapacity.cx*4,PixelFormat32bppPARGB,static_cast<BYTE*>(surfaceBits));
            Graphics g(&image);g.Clear(Color(0,0,0,0));g.SetSmoothingMode(SmoothingModeAntiAlias);
            for(const auto& rect:{timeline,picker})
                if(!IsRectEmpty(&rect))g.ExcludeClip(Rect(rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top));
            if(axis.reflected)stroke(g,c-(to-c),c);
            if(axis.angular)stroke(g,c,end);
            if(axis.radial || axis.diamond){
                Point last;
                const int count=axis.diamond?4:64;
                for(int i=0;i<=count;++i){
                    const Point next=view.toScreen(axis.arc(double(i)/count,axis.length));
                    if(i)stroke(g,last,next);last=next;
                }
            }
            // Use the same stops and opacity interpolation as the independent timeline.
            const auto stripPoint=[&](double t){
                return axis.angular?view.toScreen(axis.arc(t,arcRadius)):view.toScreen(axis.at(t));
            };
            const auto& colors=panel->track(1);
            const auto& opacity=panel->track(0);
            const bool rgb=colors.front().contains("rgb");
            if(rgb && (rampAngular!=axis.angular || rampTracks.size()!=2 || rampTracks[0]!=colors || rampTracks[1]!=opacity)){
                rampTracks=Json::array({colors,opacity});
                rampAngular=axis.angular;
                for(int i=0;i<(rampAngular?128:129);++i)
                    rampSamples[i]=GradientPanel::rampColor(colors,opacity,(i+(rampAngular?.5f:0))*(GradientPanel::end/128.f));
            }
            Pen edge(ink(text),float(2*halfWidth+view.dpi));
            GraphicsPath outline;
            const int steps=axis.angular?128:1;
            for(int i=0;i<steps;++i){
                const double a=double(i)/steps,b=double(i+1)/steps;
                const auto from=screen(stripPoint(a)),to=screen(stripPoint(b));
                outline.AddLine(from,to);
            }
            g.DrawPath(&edge,&outline);
            if(colors.size()<32 && region && outline.Widen(&edge)==Ok){
                Region strip(&outline);
                if(HRGN body=strip.GetHRGN(&g)){CombineRgn(region,region,body,RGN_OR);DeleteObject(body);}
            }
            GraphicsPath alphaRail;
            for(int i=0;i<steps;++i)
                alphaRail.AddLine(screen(railPoint(double(i)/steps,opacityOffset())),
                                  screen(railPoint(double(i+1)/steps,opacityOffset())));
            Pen alphaInk(ink(border),float(2*view.dpi));
            g.DrawPath(&alphaInk,&alphaRail);
            if(region){
                Pen hitWidth(ink(text),float(2*radius));
                if(alphaRail.Widen(&hitWidth)==Ok){
                    Region strip(&alphaRail);
                    if(HRGN body=strip.GetHRGN(&g)){CombineRgn(region,region,body,RGN_OR);DeleteObject(body);}
                }
            }
            const int tiles=axis.angular?128:rgb?16:1;
            Pen paper(ink(border),float(2*halfWidth)),paint(ink(text),float(2*halfWidth));
            for(int i=0;i<tiles;++i){
                const double a=double(i)/tiles,b=double(i+1)/tiles;
                const auto from=screen(stripPoint(a)),to=screen(stripPoint(b));
                paper.SetColor(ink(rgb && (axis.angular?i/8:i)%2?surface:border));
                paper.SetStartCap(i?LineCapSquare:LineCapFlat);
                paper.SetEndCap(i+1<tiles?LineCapSquare:LineCapFlat);
                g.DrawLine(&paper,from,to);
                if(axis.angular && rgb){
                    paint.SetColor(ink(rampSamples[axis.reverse?127-i:i]));
                    g.DrawLine(&paint,from,to);
                }
            }
            if(!axis.angular && rgb){
                // One translucent stroke avoids seams where separately antialiased samples meet.
                std::array<Color,129> samples;
                std::array<REAL,129> positions;
                for(int i=0;i<=128;++i){
                    positions[i]=i/128.f;
                    samples[i]=ink(rampSamples[axis.reverse?128-i:i]);
                }
                const auto a=screen(stripPoint(0)),b=screen(stripPoint(1));
                LinearGradientBrush ramp(a,b,samples.front(),samples.back());
                ramp.SetInterpolationColors(samples.data(),positions.data(),int(samples.size()));
                Pen paint(&ramp,float(2*halfWidth));
                g.DrawLine(&paint,a,b);
            }
            for(const auto& h:handles){
                const auto p=screen(h.p);
                const float r=float(h.radius);
                SolidBrush fill(ink(h.fill));
                const bool selected=h.kind==color && (panel->selected(h.lane,h.index) || (h.lane==1 && h.index==colorTarget));
                Pen outline(ink(selected?accent:text),float(view.dpi));
                g.FillEllipse(&fill,p.X-r,p.Y-r,r*2,r*2);
                g.DrawEllipse(&outline,p.X-r,p.Y-r,r*2,r*2);
                if(h.kind==direction){
                    const float inner=r*.58f;
                    Pen arrow(ink(surface),float(view.dpi));
                    g.DrawArc(&arrow,p.X-inner,p.Y-inner,2*inner,2*inner,215.f,255.f);
                    g.DrawLine(&arrow,p.X-inner*.34f,p.Y+inner*.94f,p.X+inner*.28f,p.Y+inner*.6f);
                }
                if(region){
                    HRGN dot=CreateEllipticRgn(int(std::ceil(p.X-r)),int(std::ceil(p.Y-r)),int(std::floor(p.X+r))+1,int(std::floor(p.Y+r))+1);
                    if(dot){CombineRgn(region,region,dot,RGN_OR);DeleteObject(dot);}
                }
            }
        }
        POINT destination={inkBounds.left,inkBounds.top},source={0,0};SIZE size={width,height};
        BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        const bool shown=UpdateLayeredWindow(visual,nullptr,&destination,&size,surfaceDC,&source,0,&blend,ULW_ALPHA)!=0;
        if(!shown){if(region)DeleteObject(region);hide();return false;}
        if(region){
            excludeDock(region,inkBounds,dockTimeline,dockPicker);
            if(HRGN viewportRegion=CreateRectRgn(0,0,width,height)){
                CombineRgn(region,region,viewportRegion,RGN_AND);DeleteObject(viewportRegion);
            }
            if(!SetWindowRgn(hit,region,FALSE)){DeleteObject(region);hide();return false;}
            pickerInput=false;
        }
        SetWindowPos(visual,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW|
            (focused?0:SWP_NOZORDER));
        if(allowPointer){
            if(canvasPicker)return showPickerInput(editorRect,dockTimeline,dockPicker);
            SetWindowPos(hit,visual,destination.x,destination.y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
        }
        return true;
    }
};
