#pragma once

struct GradientCanvasPanel {
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
    int active=-1,channel=-1;
    bool vector=false;
    double radialAngle=0;
    Point pressed{};
    Axis startAxis;
    Json startPaint,lastPaint;
    RECT lastEditor{};
    std::array<uint32_t,2> lastSelection{};
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
        active=-1;
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
        if(graphics)Gdiplus::GdiplusShutdown(graphics);
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
            if(msg==WM_CAPTURECHANGED){self->active=-1;return 0;}
            if(msg==WM_LBUTTONDOWN || msg==WM_MOUSEMOVE || msg==WM_LBUTTONUP){
                POINT point{short(LOWORD(lp)),short(HIWORD(lp))};ClientToScreen(hwnd,&point);
                if(msg==WM_LBUTTONDOWN)self->begin({double(point.x),double(point.y)});
                else if(self->active>=0){
                    self->drag({double(point.x),double(point.y)});
                    if(msg==WM_LBUTTONUP){
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
        if(hit && visual && SetLayeredWindowAttributes(hit,0,1,LWA_ALPHA))return true;
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
    void begin(Point p){
        if(!IsWindowVisible(hit) || !panel->editable() || !foreground() || !view.current())return;
        if(navigationKeyHeld())return;
        const int index=at(p);if(index<0)return;
        const auto handle=handles[index];
        pressed=view.fromScreen(p);startPaint=panel->paint();startAxis=axis;
        active=index;
        if(handle.kind==color){
            panel->select(handle.lane,handle.index,GetKeyState(VK_CONTROL)<0,GetKeyState(VK_SHIFT)<0);
            panel->cursor=panel->track(handle.lane)[handle.index]["location"];
            panel->picker.key.clear();
        }
        if(GetCapture()!=hit)SetCapture(hit);
        if(GetCapture()!=hit)active=-1;
    }
    void drag(Point p){
        if(!foreground() || !view.current() || !panel->editable() || channel!=panel->channel){hide();return;}
        if(active<0 || active>=int(handles.size()))return;
        const auto h=handles[active];const Point now=view.fromScreen(p),delta=now-pressed;
        auto& paint=panel->paint();const auto old=paint;
        auto& o=paint["options"];
        const auto& a=startAxis;
        const double sign=a.reverse?-1:1;
        if(h.kind==center){
            for(const auto& entry:std::array<std::pair<const char*,double>,2>{{{"offset_x",delta.x*100/a.size.x},{"offset_y",delta.y*100/a.size.y}}})
                o[entry.first]=std::clamp(startPaint["options"][entry.first].get<double>()+entry.second,-1000.,1000.);
        }else if(h.kind==range){
            if(a.radial){
                const Point at=a.unit*a.length+delta;
                if(at.length()>.001)radialAngle=std::atan2(-at.y,at.x)*180/Axis::pi;
                o["scale"]=std::clamp(at.length()*100/a.span,1.,1000.);
            }else o["scale"]=std::clamp((a.length+delta.dot(a.unit))*100/a.span,1.,1000.);
        }else if(h.kind==direction){
            const Point before=pressed-a.center,after=now-a.center;
            if(after.length()>.001 && before.length()>.001){
                double angle=std::remainder(startPaint["options"]["angle"].get<double>()+
                    (std::atan2(-after.y,after.x)-std::atan2(-before.y,before.x))*180/Axis::pi,360.);
                if(GetKeyState(VK_SHIFT)<0)angle=std::round(angle/45)*45;
                o["angle"]=angle;
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
                    if(pixels || paint!=startPaint)panel->moveBy(startPaint,pixels);
                    panel->cursor=panel->track(h.lane)[h.index]["location"];
                }
            }else{
                auto& stops=panel->track(h.lane);
                const int gap=stops[h.index+1]["location"].get<int>()-stops[h.index]["location"].get<int>();
                const auto& prior=GradientPanel::track(startPaint,h.lane);
                if(gap>0)stops[h.index]["midpoint"]=int(std::clamp(std::round(prior[h.index]["midpoint"].get<int>()+shift*100/gap),0.,100.));
            }
        }
        if(paint!=old)panel->dirty=panel->refreshBar=true;
    }
    bool draw(bool allowPointer){
        if(!view.current() || !panel->editable()){hide();return false;}
        const bool focused=foreground();
        allowPointer=allowPointer && focused && !navigationKeyHeld();
        if(!allowPointer && IsWindowVisible(hit))suspendPointer();
        MenuControls::PanelStyle style(float(view.dpi));
        RECT editorRect{};GetWindowRect(editor,&editorRect);
        if(channel==panel->channel && lastPaint==panel->paint() && lastSelection==panel->selection &&
            View::equal(editorRect,lastEditor) && (!allowPointer || IsWindowVisible(hit)))return true;
        if(channel!=panel->channel && GetCapture()==hit)ReleaseCapture();
        channel=panel->channel;lastEditor=editorRect;lastSelection=panel->selection;
        lastPaint=panel->paint();
        if(!axis.read(view,lastPaint,vector,radialAngle)){hide();return false;}
        using namespace Gdiplus;
        const auto clip=view.clip;
        const int width=clip.right-clip.left,height=clip.bottom-clip.top;
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;
        info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        void* bits=nullptr;
        HBITMAP bmp=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&bits,nullptr,0);
        HDC dc=CreateCompatibleDC(nullptr);
        if(!bmp || !dc){if(bmp)DeleteObject(bmp);if(dc)DeleteDC(dc);hide();return false;}
        const HGDIOBJ previous=SelectObject(dc,bmp);
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
        const auto screen=[&](Point p){return PointF(float(p.x-clip.left),float(p.y-clip.top));};
        handles.clear();
        const auto add=[&](Point p,Kind kind,int index,ImU32 fill,double size,int lane=1){handles.push_back({p,kind,index,fill,size,lane});};
        add(c,center,0,accent,radius+view.dpi);
        if(!axis.angular)add(to,range,0,surface,radius);
        if(!axis.radial)add(axis.angular?end+unit*(2*gap+radius):end+normal*(2*gap+radius),direction,0,accent,radius);
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
        HRGN region=CreateRectRgn(0,0,0,0);
        const auto stroke=[&](Graphics& g,Point a,Point b){
            Pen edge(ink(border),float(3*view.dpi)),line(ink(text),float(view.dpi));
            g.DrawLine(&edge,screen(a),screen(b));g.DrawLine(&line,screen(a),screen(b));
        };
        {
            Bitmap image(width,height,width*4,PixelFormat32bppPARGB,static_cast<BYTE*>(bits));
            Graphics g(&image);g.Clear(Color(0,0,0,0));g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.ExcludeClip(Rect(editorRect.left-clip.left,editorRect.top-clip.top,editorRect.right-editorRect.left,editorRect.bottom-editorRect.top));
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
            Pen edge(ink(text),float(2*halfWidth+view.dpi));
            GraphicsPath outline;
            for(int i=0;i<128;++i){
                const double a=i/128.,b=(i+1)/128.;
                const auto from=screen(stripPoint(a)),to=screen(stripPoint(b));
                outline.AddLine(from,to);
            }
            g.DrawPath(&edge,&outline);
            for(int i=0;i<128;++i){
                const double a=i/128.,b=(i+1)/128.,middle=(a+b)/2;
                const auto from=screen(stripPoint(a)),to=screen(stripPoint(b));
                Pen paper(ink(colors.front().contains("rgb") && (i/8)%2?surface:border),float(2*halfWidth));
                if(i)paper.SetStartCap(LineCapSquare);
                if(i<127)paper.SetEndCap(LineCapSquare);
                g.DrawLine(&paper,from,to);
                if(axis.angular && colors.front().contains("rgb")){
                    Pen paint(ink(GradientPanel::rampColor(colors,opacity,float((axis.reverse?1-middle:middle)*GradientPanel::end))),float(2*halfWidth));
                    g.DrawLine(&paint,from,to);
                }
            }
            if(!axis.angular && colors.front().contains("rgb")){
                // One translucent stroke avoids seams where separately antialiased samples meet.
                std::array<Color,129> samples;
                std::array<REAL,129> positions;
                for(int i=0;i<=128;++i){
                    positions[i]=i/128.f;
                    samples[i]=ink(GradientPanel::rampColor(colors,opacity,
                        (axis.reverse?1-positions[i]:positions[i])*GradientPanel::end));
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
                Pen outline(ink(h.kind==color && panel->selected(h.lane,h.index)?accent:text),float(view.dpi));
                g.FillEllipse(&fill,p.X-r,p.Y-r,r*2,r*2);
                g.DrawEllipse(&outline,p.X-r,p.Y-r,r*2,r*2);
                HRGN dot=CreateEllipticRgn(int(std::ceil(p.X-r)),int(std::ceil(p.Y-r)),int(std::floor(p.X+r))+1,int(std::floor(p.Y+r))+1);
                if(dot){CombineRgn(region,region,dot,RGN_OR);DeleteObject(dot);}
            }
        }
        HRGN panelRegion=CreateRectRgn(editorRect.left-clip.left,editorRect.top-clip.top,editorRect.right-clip.left,editorRect.bottom-clip.top);
        if(panelRegion){CombineRgn(region,region,panelRegion,RGN_DIFF);DeleteObject(panelRegion);}
        HRGN viewportRegion=CreateRectRgn(0,0,width,height);
        if(viewportRegion){CombineRgn(region,region,viewportRegion,RGN_AND);DeleteObject(viewportRegion);}
        POINT destination={clip.left,clip.top},source={0,0};SIZE size={width,height};
        BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        const bool shown=UpdateLayeredWindow(visual,nullptr,&destination,&size,dc,&source,0,&blend,ULW_ALPHA)!=0;
        SelectObject(dc,previous);DeleteDC(dc);DeleteObject(bmp);
        if(!shown || !region || !SetWindowRgn(hit,region,FALSE)){if(region)DeleteObject(region);hide();return false;}
        SetWindowPos(visual,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW|
            (focused?0:SWP_NOZORDER));
        if(allowPointer)SetWindowPos(hit,visual,destination.x,destination.y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
        return true;
    }
};
