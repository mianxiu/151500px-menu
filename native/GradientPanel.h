#pragma once

struct GradientPanel {
    static constexpr int end=4096;
    static constexpr int minWidth=560,maxWidth=900,overlayHeight=138;
    static constexpr int pickerBodyWidth=216;
    static constexpr int pickerWidth=int(pickerBodyWidth+2*menuDenseGap);
    static constexpr int pickerHeight=218;
    RECT moveGrip{};
    static constexpr float fieldWidth=76,minSpan=64;
    Json draft;
    Json* embeddedPaint=nullptr;
    std::string phase,message;
    bool live=false,pending=false,dirty=false,released=false,terminal=false,dismissRequested=false;
    bool overlay=false,viewWaiting=false,showParameters=false;
    int channel=0,cursor=end/2,anchorLane=-1,anchorIndex=-1,activeLane=1;
    int primaryColor=-1;
    PerceptualPickerState picker;
    float start=0,span=end;
    std::array<uint32_t,2> selection{};
    enum Gesture { none, move, box, pan } gesture=none;
    ImVec2 press{};
    int pressLane=-1,appliedDelta=0;
    bool dragged=false;
    float panStart=0,overviewOffset=0;
    enum OverviewDrag { viewportPan, viewportLeft, viewportRight } overviewDrag=viewportPan;
    Json original;
    std::array<uint32_t,2> boxBase{};
    ULONGLONG lastSent=0;
    bool refreshBar=true;
    float barStart=-1,barSpan=0;
    std::array<ImU32,129> barColors{};

    Json& paint(){return embeddedPaint?*embeddedPaint:draft[channel==0?"fill":"stroke"];}
    Json& track(int lane){return paint()[lane==0?"opacity_stops":"stops"];}
    static const Json& track(const Json& paint,int lane){return paint.at(lane==0?"opacity_stops":"stops");}
    bool ready() const {return draft.is_object();}
    bool editable() const {return !terminal && (phase=="editing" || phase=="preview");}
    static uint32_t bit(int index){return uint32_t(1)<<index;}
    static RECT dockBounds(RECT clip,UINT dpi,RECT work){
        const LONG gap=MulDiv(int(menuPanelGap),dpi,96);
        const LONG side=MulDiv(pickerWidth,dpi,96)+gap;
        const LONG available=std::min(clip.right-clip.left-2*gap,work.right-work.left-side-2*gap);
        const LONG width=std::clamp<LONG>(available,MulDiv(minWidth,dpi,96),MulDiv(maxWidth,dpi,96))+side;
        const LONG height=MulDiv(std::max(overlayHeight,pickerHeight),dpi,96);
        const LONG left=std::clamp(clip.left+(clip.right-clip.left-(width-side))/2,work.left,std::max(work.left,work.right-width));
        return {left,clip.bottom-gap-height,left+width,clip.bottom-gap};
    }
    static RECT dockTimeline(RECT frame,UINT dpi){
        return {frame.left,frame.bottom-MulDiv(overlayHeight,dpi,96),
            frame.right-MulDiv(pickerWidth+int(menuPanelGap),dpi,96),frame.bottom};
    }
    static RECT dockPicker(RECT frame,UINT dpi){
        return {frame.right-MulDiv(pickerWidth,dpi,96),frame.bottom-MulDiv(pickerHeight,dpi,96),frame.right,frame.bottom};
    }
    bool editingOpacity() const{return activeLane==0 && selection[0];}
    bool canSampleColor(){return hasRgbPicker() && !editingOpacity();}
    bool hasRgbPicker(){return track(1).front().contains("rgb");}
    bool selected(int lane,int index) const {return (selection[lane]&bit(index))!=0;}
    int colorTarget(){
        auto& stops=track(1);
        if(primaryColor>=0 && primaryColor<int(stops.size()) && selected(1,primaryColor))return primaryColor;
        for(int i=0;i<int(stops.size());++i)if(selected(1,i))return i;
        if(!overlay)return -1;
        const auto distance=[&](const Json& stop){return std::abs(stop.at("location").get<int>()-cursor);};
        return int(std::min_element(stops.begin(),stops.end(),[&](const Json& a,const Json& b){return distance(a)<distance(b);})-stops.begin());
    }
    void applyRgb(const Json& rgb){
        const int index=colorTarget();
        if(index<0)return;
        auto& stops=track(1);
        for(int i=0;i<int(stops.size());++i)if(selection[1]?selected(1,i):i==index){
            stops[i]["rgb"]=rgb;stops[i]["source"]="userStop";
        }
        dirty=refreshBar=true;
    }
    void open(const Json& state){
        draft=state.at("draft");live=state.value("live",false);
        channel=state.value("channel",std::string())=="stroke" || draft.at("fill").at("kind")!="gradient"?1:0;
        receive(state);
    }
    void receive(const Json& state){
        phase=state.at("phase").get<std::string>();message=state.value("message","");
        if(state.value("ack",false))pending=false;
        if(phase=="error")terminal=dismissRequested=false;
    }
    void emit(const char* action){
        pending=true;dirty=released=false;lastSent=GetTickCount64();
        std::cout<<Json{{"type",action},{"draft",draft}}.dump()<<std::endl;
    }
    void finish(bool apply){
        if(!ready())return;
        if(!apply)dismissRequested=true;
        if(terminal || (apply && !editable()))return;
        terminal=true;gesture=none;
        phase=apply?"applying":"closing";
        emit(apply?"gradient.apply":"gradient.cancel");
    }
    void flush(bool pointerReleased=false){
        // Retain the final sample while an earlier update is awaiting its ACK.
        released|=pointerReleased && dirty;
        if(editable() && dirty && !pending && (released || GetTickCount64()-lastSent>=33))emit("gradient.update");
    }

    void select(int lane,int index,bool ctrl,bool shift){
        activeLane=lane;
        if(shift && anchorLane==lane){
            if(!ctrl)selection={};
            for(int i=std::min(index,anchorIndex);i<=std::max(index,anchorIndex);++i)selection[lane]|=bit(i);
        }else{
            if(ctrl)selection[lane]^=bit(index);
            else if(!selected(lane,index)){selection={};selection[lane]=bit(index);}
            anchorLane=lane;anchorIndex=index;
        }
        if(lane==1)primaryColor=index;
    }
    static float mixAmount(const Json& a,const Json& b,float at){
        const int lo=a.at("location"),hi=b.at("location");
        const float t=hi>lo?std::clamp((at-lo)/(hi-lo),0.f,1.f):1.f;
        const float middle=std::clamp(a.value("midpoint",50)/100.f,.0001f,.9999f);
        return t<=middle?.5f*t/middle:.5f+.5f*(t-middle)/(1-middle);
    }
    static std::pair<const Json&,const Json&> sampleNeighbors(const Json& stops,float at){
        const auto right=std::upper_bound(stops.begin(),stops.end(),at,[](float x,const Json& stop){return x<stop.at("location").get<int>();});
        const auto& left=right==stops.begin()?*right:*std::prev(right);
        return {left,right==stops.end()?left:*right};
    }
    static double interpolate(double from,double to,float t){return from+(to-from)*t;}
    static Json sample(const Json& stops,int lane,float at){
        const auto [left,right]=sampleNeighbors(stops,at);
        Json value=left;
        if(&left!=&right){
            const float t=mixAmount(left,right,at);
            if(lane==0)value["opacity"]=interpolate(left.at("opacity"),right.at("opacity"),t);
            else if(left.contains("rgb") && right.contains("rgb")){
                for(int i=0;i<3;++i){
                    value["rgb"][i]=std::round(interpolate(left.at("rgb")[i],right.at("rgb")[i],t));
                }
            }else if(left.contains("native") && right.contains("native") && left["native"]["model"]==right["native"]["model"]){
                for(size_t i=0;i<left["native"]["values"].size();++i){
                    value["native"]["values"][i]=interpolate(left["native"]["values"][i],right["native"]["values"][i],t);
                }
            }
        }
        return value;
    }
    bool add(int lane,int location){
        auto& stops=track(lane);
        if(stops.size()>=32)return false;
        auto value=sample(stops,lane,location);
        value["location"]=location;value["midpoint"]=50;
        if(lane==1){value.erase("origin");value["source"]="userStop";}
        auto at=std::upper_bound(stops.begin(),stops.end(),location,[](int x,const Json& s){return x<s.at("location").get<int>();});
        const int index=int(at-stops.begin());stops.insert(at,std::move(value));
        activeLane=lane;selection={};selection[lane]=bit(index);anchorLane=lane;anchorIndex=index;
        if(lane==1){primaryColor=index;picker.key.clear();}
        dirty=refreshBar=true;return true;
    }
    bool canRemove(){
        if(!(selection[0] || selection[1]))return false;
        for(int lane=0;lane<2;++lane){
            int remaining=0;
            for(int i=0;i<int(track(lane).size());++i)remaining+=!selected(lane,i);
            if(remaining<2)return false;
        }
        return true;
    }
    void remove(){
        if(!canRemove())return;
        for(int lane=0;lane<2;++lane){auto& nodes=track(lane);for(int i=int(nodes.size())-1;i>=0;--i)if(selected(lane,i))nodes.erase(nodes.begin()+i);}
        selection={};anchorLane=primaryColor=-1;picker.key.clear();dirty=refreshBar=true;
    }
    int shift(const Json& baseline,int delta){
        int low=-end,high=end;
        for(int lane=0;lane<2;++lane){
            const auto& nodes=track(baseline,lane);
            for(int i=0;i<int(nodes.size());++i)if(selected(lane,i)){
                const int pos=nodes[i].at("location");
                low=std::max(low,(i>0 && !selected(lane,i-1)?nodes[i-1].at("location").get<int>():0)-pos);
                high=std::min(high,(i+1<int(nodes.size()) && !selected(lane,i+1)?nodes[i+1].at("location").get<int>():end)-pos);
            }
        }
        return std::clamp(delta,low,high);
    }
    void moveBy(const Json& baseline,int delta){
        for(int lane=0;lane<2;++lane){
            auto& nodes=track(lane);const auto& prior=track(baseline,lane);
            for(int i=0;i<int(nodes.size());++i)if(selected(lane,i))nodes[i]["location"]=prior[i].at("location").get<int>()+delta;
        }
        dirty=refreshBar=true;
    }
    void zoom(float factor,float anchor){
        const float next=std::clamp(span/factor,minSpan,float(end));
        start=std::clamp(start+std::clamp(anchor,0.f,1.f)*(span-next),0.f,end-next);span=next;
    }
    void resizeOverview(OverviewDrag edge,float at){
        if(edge==viewportLeft){
            const float right=start+span;
            start=std::clamp(at,0.f,std::max(0.f,right-minSpan));
            span=right-start;
        }else span=std::clamp(at-start,minSpan,std::max(minSpan,end-start));
    }
    static float rulerStep(float span,float width,float labelSpacing){
        const float ideal=span*100.f/end*labelSpacing/std::max(width,labelSpacing);
        for(float step:{.25f,.5f,1.f,2.f,5.f,10.f,20.f,25.f,50.f,100.f})
            if(step>=ideal)return step;
        return 100.f;
    }
    static ImU32 swatch(const Json& stop,ImU32 fallback,int alpha=255){
        if(!stop.contains("rgb"))return fallback;
        const auto& rgb=stop.at("rgb");
        return ImGui::ColorConvertFloat4ToU32({rgb[0].get<float>()/255,rgb[1].get<float>()/255,rgb[2].get<float>()/255,alpha/255.f});
    }
    static ImU32 opacitySwatch(const Json& stop){
        return ImGui::ColorConvertFloat4ToU32({1,1,1,stop.at("opacity").get<float>()/100});
    }
    static ImU32 rampColor(const Json& colors,const Json& opacity,float at){
        const auto [left,right]=sampleNeighbors(colors,at);
        const auto [low,high]=sampleNeighbors(opacity,at);
        const float t=mixAmount(left,right,at);
        const auto& a=left.at("rgb");const auto& b=right.at("rgb");
        const int alpha=std::lround(float(interpolate(low.at("opacity"),high.at("opacity"),mixAmount(low,high,at)))*2.55f);
        return ImGui::ColorConvertFloat4ToU32({
            float(std::round(interpolate(a[0],b[0],t))/255.),
            float(std::round(interpolate(a[1],b[1],t))/255.),
            float(std::round(interpolate(a[2],b[2],t))/255.),alpha/255.f});
    }
    void timeline(float scale){
        auto* draw=ImGui::GetWindowDrawList();const auto& io=ImGui::GetIO();
        const ImVec2 at=ImGui::GetCursorScreenPos();const float height=(overlay?13.f:18.f)*scale;
        const float marker=menuGradientHandleSize*scale,width=std::max(marker*3,ImGui::GetContentRegionAvail().x),left=at.x+marker;
        const float trackWidth=width-marker*2,trackRight=left+trackWidth;
        const float rows[]={at.y+height,at.y+height*3};
        const float laneCenter[]={rows[0]+height*.5f,rows[1]+height*.5f};
        const ImVec2 bottom={at.x+width,at.y+height*4};
        ImGui::InvisibleButton("##gradient-timeline",{width,height*4},ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonMiddle);
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
        const bool hovered=ImGui::IsItemHovered();
        const auto mouse=io.MousePos;
        auto x=[&](float pos){return left+(pos-start)*trackWidth/span;};
        auto location=[&](float px){return std::clamp(int(std::lround(start+(px-left)*span/trackWidth)),0,end);};
        auto laneAt=[&](float y){return y>=rows[0] && y<rows[0]+height?0:y>=rows[1] && y<rows[1]+height?1:-1;};
        if(hovered && gesture==none && io.MouseWheel!=0){
            if(io.KeyShift)start=std::clamp(start-io.MouseWheel*span*.1f,0.f,end-span);
            else zoom(std::pow(1.2f,io.MouseWheel),(mouse.x-left)/trackWidth);
        }
        if(hovered && gesture==none && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)){
            gesture=pan;press=mouse;panStart=start;
        }else if(hovered && gesture==none && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
            press=mouse;cursor=location(mouse.x);pressLane=laneAt(mouse.y);dragged=false;
            if(pressLane>=0)activeLane=pressLane;
            int hit=-1;float distance=marker*.55f;
            if(pressLane>=0)for(int i=0;i<int(track(pressLane).size());++i){
                const float px=x(track(pressLane)[i].at("location").get<int>());
                if(px<left-.1f || px>trackRight+.1f)continue;
                const float offset=std::abs(mouse.x-px);
                if(offset<=distance){hit=i;distance=offset;}
            }
            if(hit>=0){select(pressLane,hit,io.KeyCtrl,io.KeyShift);gesture=move;original=paint();appliedDelta=0;}
            else {gesture=box;boxBase=io.KeyCtrl?selection:std::array<uint32_t,2>{};}
        }
        if(gesture==pan){
            if(ImGui::IsMouseDown(ImGuiMouseButton_Middle))start=std::clamp(panStart-(mouse.x-press.x)*span/trackWidth,0.f,end-span);
            else gesture=none;
        }else if(gesture==move || gesture==box){
            if(!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)){gesture=none;}
            else if(ImGui::IsMouseDown(ImGuiMouseButton_Left)){
                dragged=dragged || ImGui::IsMouseDragging(ImGuiMouseButton_Left);
                if(dragged && gesture==move && (selection[0] || selection[1])){
                    const int delta=shift(original,location(mouse.x)-location(press.x));
                    if(delta!=appliedDelta){appliedDelta=delta;moveBy(original,delta);}
                }
                if(dragged && gesture==box){
                    selection=boxBase;const int a=location(press.x),b=location(mouse.x);
                    for(int lane=0;lane<2;++lane)if(std::max(press.y,mouse.y)>=rows[lane] && std::min(press.y,mouse.y)<=rows[lane]+height)
                        for(int i=0;i<int(track(lane).size());++i){const int pos=track(lane)[i].at("location");if(pos>=std::min(a,b) && pos<=std::max(a,b))selection[lane]|=bit(i);}
                }
            }else{
                if(gesture==move && ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
                    (dragged || std::hypot(mouse.x-press.x,mouse.y-press.y)>=io.MouseDragThreshold) &&
                    (selection[0] || selection[1])){
                    const int delta=shift(original,location(mouse.x)-location(press.x));
                    if(delta!=appliedDelta)moveBy(original,delta);
                }else if(gesture==box && !dragged && hovered){
                    if(pressLane>=0 && laneAt(mouse.y)==pressLane && !io.KeyCtrl && !io.KeyShift)add(pressLane,cursor);
                    else selection=boxBase;
                }
                gesture=none;
            }
        }
        const auto border=ImGui::GetColorU32(ImGuiCol_Border),active=ImGui::GetColorU32(ImGuiCol_CheckMark);
        const auto neutral=ImGui::GetColorU32(ImGuiCol_Button);
        const auto text=ImGui::GetColorU32(ImGuiCol_Text);
        draw->PushClipRect(at,bottom,true);
        {
            MenuTextStyle secondary(MenuTextRole::Secondary);
            const float step=rulerStep(span,trackWidth,ImGui::CalcTextSize("100.00%").x+2*menuPanelGap*scale);
            const int divisions=step==2.f?4:5;
            const float minor=step*end/(100.f*divisions);
            const int first=int(std::ceil(start/minor-.0001f)),last=int(std::floor((start+span)/minor+.0001f));
            for(int i=first;i<=last;++i){
                const float px=std::clamp(x(i*minor),left,trackRight);
                const bool major=i%divisions==0;
                if(major || !overlay)draw->AddLine({px,at.y+height-(major?(overlay?4:8):4)*scale},{px,at.y+height},border);
                if(!major)continue;
                char label[16];std::snprintf(label,sizeof(label),"%.*f%%",step<.5f?2:step<1.f?1:0,i*step/divisions);
                const auto size=ImGui::CalcTextSize(label);
                draw->AddText({std::clamp(px-size.x*.5f,at.x,std::max(at.x,bottom.x-size.x)),at.y},text,label);
            }
        }
        const ImVec2 strip={left,at.y+height*2};
        ImGui::RenderColorRectWithAlphaCheckerboard(draw,strip,{trackRight,strip.y+height},0,height*.5f,{0,0});
        auto& colors=track(1);auto& alpha=track(0);
        const bool rgb=colors.front().contains("rgb");
        if(rgb){
            if(refreshBar || start!=barStart || span!=barSpan){
                for(int i=0;i<129;++i){
                    const float pos=start+i*span/128;
                    barColors[i]=rampColor(colors,alpha,pos);
                }
                barStart=start;barSpan=span;refreshBar=false;
            }
            for(int i=0;i<128;++i){
                const float lo=left+i*trackWidth/128,hi=left+(i+1)*trackWidth/128;
                draw->AddRectFilledMultiColor({lo,strip.y},{hi,strip.y+height},barColors[i],barColors[i+1],barColors[i+1],barColors[i]);
            }
        }
        draw->AddRect(strip,{trackRight,strip.y+height},border);
        const int target=overlay?colorTarget():-1;
        for(int lane=0;lane<2;++lane){
            if(!overlay)draw->AddLine({left,laneCenter[lane]},{trackRight,laneCenter[lane]},border);
            for(int i=0;i<int(track(lane).size());++i){
                const auto& stop=track(lane)[i];const float px=x(stop.at("location").get<int>());
                if(px<left-.1f || px>trackRight+.1f)continue;
                const float radius=marker*.5f,y=laneCenter[lane];
                const ImVec2 a={px-radius,y-radius},b={px+radius,y+radius};
                const float rounding=ImGui::GetStyle().FrameRounding;
                if(lane==0)ImGui::RenderColorRectWithAlphaCheckerboard(draw,a,b,opacitySwatch(stop),radius,{0,0},rounding);
                else draw->AddRectFilled(a,b,swatch(stop,neutral),rounding);
                const bool activeStop=selected(lane,i) || (lane==1 && i==target);
                draw->AddRect(a,b,activeStop?active:border,rounding,0,activeStop?2*scale:scale);
            }
        }
        if(gesture==box && dragged){
            const ImVec2 a={std::min(press.x,mouse.x),std::min(press.y,mouse.y)},b={std::max(press.x,mouse.x),std::max(press.y,mouse.y)};
            draw->AddRectFilled(a,b,ImGui::GetColorU32(ImGuiCol_CheckMark,.16f));draw->AddRect(a,b,active);
        }
        draw->PopClipRect();
    }

    void overview(float reserve=0){
        const auto at=ImGui::GetCursorScreenPos();
        const float inset=menuGradientHandleSize*scale,height=menuShortcutHeight*.6f*scale;
        const float fullWidth=std::max(inset*3,ImGui::GetContentRegionAvail().x-reserve),width=fullWidth-inset*2,left=at.x+inset;
        ImGui::InvisibleButton("##gradient-viewport",{fullWidth,height});
        const float point=(ImGui::GetIO().MousePos.x-left)*end/width;
        const float grip=std::min(7*scale*end/width,span*.32f);
        const auto hoveredEdge=std::abs(point-start)<=grip?viewportLeft:
            std::abs(point-start-span)<=grip?viewportRight:viewportPan;
        if(ImGui::IsItemActivated()){
            overviewDrag=hoveredEdge;
            if(overviewDrag==viewportPan)
                overviewOffset=point>=start && point<=start+span?point-start:span*.5f;
            else overviewOffset=point-(overviewDrag==viewportLeft?start:start+span);
        }
        if(ImGui::IsItemActive() && ImGui::IsMouseDown(0)){
            if(overviewDrag==viewportPan)start=std::clamp(point-overviewOffset,0.f,std::max(0.f,end-span));
            else resizeOverview(overviewDrag,point-overviewOffset);
        }
        if((ImGui::IsItemHovered() && hoveredEdge!=viewportPan) || (ImGui::IsItemActive() && overviewDrag!=viewportPan))
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        auto* draw=ImGui::GetWindowDrawList();
        const float railHeight=menuGradientOverviewRailHeight*scale,rounding=railHeight*.5f;
        const ImVec2 railA={left,at.y+(height-railHeight)*.5f},railB={left+width,railA.y+railHeight};
        const auto position=[&](float location){return left+location*width/end;};
        const ImVec2 visibleA={position(start),railA.y},visibleB={position(start+span),railB.y};
        draw->AddRectFilled(railA,railB,ImGui::GetColorU32(ImGuiCol_FrameBg),rounding);
        draw->AddRectFilled(visibleA,visibleB,ImGui::GetColorU32(ImGuiCol_CheckMark,.35f),rounding);
        const auto neutral=ImGui::GetColorU32(ImGuiCol_Text),border=ImGui::GetColorU32(ImGuiCol_Border);
        const float half=1.5f*scale,stroke=menuControlBorder*scale;
        for(const auto& stop:track(1)){
            const float x=position(stop.at("location").get<int>());
            const ImVec2 a={x-half,railA.y-stroke},b={x+half,railB.y+stroke};
            draw->AddRectFilled(a,b,swatch(stop,neutral),stroke);
            draw->AddRect(a,b,border,stroke,0,stroke);
        }
        const auto handle=ImGui::GetColorU32(ImGuiCol_CheckMark);
        for(float x:{visibleA.x,visibleB.x})
            draw->AddRectFilled({x-1.5f*scale,at.y+scale},{x+1.5f*scale,at.y+height-scale},handle,scale);
    }
    void opacityValue(){
        selectionValue("不透明度",0,0,100,.2f,"%.2f%%",[](const Json& stop){return stop.at("opacity").get<double>();},
            [](Json& stop,double value){stop["opacity"]=value;});
    }

    static double angleAt(ImVec2 center,ImVec2 point,bool snap){
        constexpr double degrees=180./3.14159265358979323846;
        const double angle=std::atan2(center.y-point.y,point.x-center.x)*degrees;
        return snap?std::round(angle/45.)*45.:std::round(angle*4.)/4.;
    }
    void direction(){
        const auto at=ImGui::GetCursorScreenPos();
        const float side=menuShortcutHeight*scale,radius=side*.5f;
        const ImVec2 center={at.x+radius,at.y+radius};
        ImGui::InvisibleButton("##gradient-direction",{side,side});
        auto& value=paint()["options"]["angle"];
        const auto& io=ImGui::GetIO();
        if(ImGui::IsItemActive() && ImGui::IsMouseDown(0) && std::hypot(io.MousePos.x-center.x,io.MousePos.y-center.y)>scale){
            const double next=angleAt(center,io.MousePos,io.KeyShift);
            if(next!=value){value=next;dirty=true;}
        }
        const double theta=value.get<double>()*(3.14159265358979323846/180.);
        const ImVec2 axis={float(std::cos(theta)),float(-std::sin(theta))};
        const ImVec2 tip={center.x+axis.x*radius*.72f,center.y+axis.y*radius*.72f};
        const ImVec2 tail={center.x-axis.x*radius*.52f,center.y-axis.y*radius*.52f};
        const ImVec2 base={tip.x-axis.x*4*scale,tip.y-axis.y*4*scale};
        auto* draw=ImGui::GetWindowDrawList();
        const auto ink=ImGui::GetColorU32(ImGuiCol_Text);
        draw->AddCircleFilled(center,radius,ImGui::GetColorU32(ImGui::IsItemActive()?ImGuiCol_ButtonActive:ImGui::IsItemHovered()?ImGuiCol_ButtonHovered:ImGuiCol_Button),32);
        draw->AddCircle(center,radius,ImGui::GetColorU32(ImGuiCol_Border),32,scale);
        draw->AddLine(tail,tip,ink,scale);
        draw->AddTriangleFilled(tip,{base.x-axis.y*3*scale,base.y+axis.x*3*scale},{base.x+axis.y*3*scale,base.y-axis.x*3*scale},ink);
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))MenuControls::tip("拖动调整角度，按 Shift 吸附到 45°",scale);
    }

    template<typename Read,typename Write>
    void selectionValue(const char* label,int onlyLane,double low,double high,float step,const char* format,Read read,Write write){
        bool found=false,mixed=false;double value=0;
        for(int lane=0;lane<2;++lane)if(onlyLane<0 || lane==onlyLane)
            for(int i=0;i<int(track(lane).size());++i)if(selected(lane,i)){
                const double next=read(track(lane)[i]);if(found)mixed|=value!=next;else{value=next;found=true;}
            }
        ImGui::PushID(label);
        if(label[0]!='#')MenuControls::fieldLabel(label,ImGui::CalcTextSize("起始角度").x+menuDenseGap*scale);
        ImGui::BeginDisabled(!found);
        const float width=std::min(fieldWidth*scale,ImGui::GetContentRegionAvail().x);
        const std::string placeholder=(label[0]=='#'?std::string(label+2)+" ":std::string())+(!found?"—":"混合");
        if(MenuControls::number("##value",value,width,step,low,high,!found || mixed?placeholder.c_str():format)){
            for(int lane=0;lane<2;++lane)if(onlyLane<0 || lane==onlyLane)
                for(int i=0;i<int(track(lane).size());++i)if(selected(lane,i))write(track(lane)[i],value);
            dirty=refreshBar=true;
        }
        ImGui::EndDisabled();ImGui::PopID();
    }
    void optionValue(const char* label,const char* key,double low,double high,float step,const char* format){
        auto& option=paint()["options"][key];double value=option;
        ImGui::PushID(label);MenuControls::fieldLabel(label,ImGui::CalcTextSize("起始角度").x+menuDenseGap*scale);
        if(MenuControls::number("##value",value,fieldWidth*scale,step,low,high,format)){
            option=value;dirty=refreshBar=true;
        }
        ImGui::PopID();
    }
    template<size_t N> void choice(const char* label,Json& value,const char* const (&keys)[N],const char* const (&names)[N]){
        size_t index=0;while(index+1<N && value!=keys[index])++index;
        ImGui::PushID(label);
        MenuControls::fieldLabel(label,ImGui::CalcTextSize("起始角度").x+menuDenseGap*scale);
        ImGui::SetNextItemWidth(fieldWidth*scale);
        if(MenuControls::choice("##value",names,index)){value=keys[index];dirty=refreshBar=true;}
        ImGui::PopID();
    }
    void types(){
        const char* styles[]={"linear","radial","angle","reflected","diamond"},*names[]={"线性","径向","角度","对称","菱形"};
        auto& type=paint()["options"]["style"];int index=0;
        while(index+1<IM_ARRAYSIZE(styles) && type!=styles[index])++index;
        if(MenuControls::segments("##gradient-type",names,index,scale,true)){type=styles[index];dirty=true;}
    }
    void parameters(){
        if(ImGui::BeginTable("##gradient-parameters",3,ImGuiTableFlags_NoSavedSettings|ImGuiTableFlags_SizingStretchSame)){
            ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);
            const int lane=selection[activeLane]?activeLane:1-activeLane;
            int selectedIndex=0;while(selectedIndex<int(track(lane).size()) && !selected(lane,selectedIndex))++selectedIndex;
            const int location=selectedIndex<int(track(lane).size())?track(lane)[selectedIndex]["location"].get<int>():0;
            double position=location*100./end;
            ImGui::BeginDisabled(!selection[lane]);
            MenuControls::fieldLabel("位置",ImGui::CalcTextSize("起始角度").x+menuDenseGap*scale);
            if(MenuControls::number("##node-position",position,fieldWidth*scale,.1f,0,100,"%.1f%%")){
                const Json base=paint();const int delta=shift(base,int(std::lround(position*end/100))-location);if(delta)moveBy(base,delta);
            }
            ImGui::EndDisabled();
            ImGui::TableSetColumnIndex(1);
            selectionValue("中点",-1,0,100,.2f,"%.0f%%",[](const Json& stop){return stop.at("midpoint").get<double>();},[](Json& stop,double value){stop["midpoint"]=int(std::lround(value));});
            ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);
            const char* methods[]={"classic","linear","perceptual"},*methodNames[]={"经典","线性","感知"};
            choice("插值",paint()["method"],methods,methodNames);
            const std::string type=paint()["options"]["style"];
            const bool radial=type=="radial",angular=type=="angle";
            ImGui::TableSetColumnIndex(1);
            if(!radial){optionValue(angular?"起始角度":"方向","angle",-360,360,.25f,"%.1f°");ImGui::SameLine();direction();}
            ImGui::TableSetColumnIndex(2);
            if(!angular)optionValue(radial?"半径缩放":"缩放","scale",1,1000,.25f,"%.1f%%");
            ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);
            if(paint()["options"].contains("offset_x"))optionValue("中心 X","offset_x",-1000,1000,.25f,"%.1f%%");
            ImGui::TableSetColumnIndex(1);
            if(paint()["options"].contains("offset_y"))optionValue("中心 Y","offset_y",-1000,1000,.25f,"%.1f%%");
            ImGui::TableSetColumnIndex(2);
            auto& smoothness=paint()["options"]["smoothness"];double smooth=smoothness.get<double>()*100/end;
            MenuControls::fieldLabel("平滑度",ImGui::CalcTextSize("起始角度").x+menuDenseGap*scale);
            if(MenuControls::number("##smoothness",smooth,fieldWidth*scale,.25f,0,100,"%.0f%%")){
                smoothness=int(std::lround(smooth*end/100));dirty=refreshBar=true;
            }
            ImGui::EndTable();
        }
        const char* names[]={"反向","仿色","与图层对齐"},*keys[]={"reverse","dither","align"};
        auto& options=paint()["options"];
        std::array<bool,3> flags={options["reverse"],options["dither"],options["align"]};
        if(MenuControls::toggles("##gradient-options",names,flags,scale,true)){
            for(size_t i=0;i<flags.size();++i)options[keys[i]]=flags[i];
            dirty=refreshBar=true;
        }
    }
    void colorPicker(float width){
        auto& stops=track(1);
        const int index=colorTarget();
        Json rgb=stops[index<0?0:index].at("rgb");
        const auto identity=std::to_string(channel)+":"+std::to_string(index);
        ImGui::BeginDisabled(index<0);
        if(drawPerceptualPicker(picker,rgb,width,identity) && index>=0)applyRgb(rgb);
        if(picker.hoverPart>=0 && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            MenuControls::tip(perceptualPickerTips[picker.hoverPart],scale);
        ImGui::EndDisabled();
        if(!overlay){
            if(index<0){MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextDisabled("选择色标后调色");}
            else ImGui::Dummy({0,0});
        }
    }
    void opacityPicker(float width){
        const auto at=ImGui::GetCursorScreenPos();
        const float height=perceptualSquareSide(width);
        ImGui::InvisibleButton("##gradient-opacity-picker",{width,height});
        auto* draw=ImGui::GetWindowDrawList();
        const ImVec2 bottom{at.x+width,at.y+height};
        const float rounding=ImGui::GetStyle().FrameRounding;
        ImGui::RenderColorRectWithAlphaCheckerboard(draw,at,bottom,0,2*menuPanelGap*scale,{0,0},rounding);
        const int first=draw->VtxBuffer.Size;
        draw->AddRectFilled(at,bottom,ImGui::GetColorU32(IM_COL32_WHITE),rounding);
        for(int i=first;i<draw->VtxBuffer.Size;++i){
            auto& vertex=draw->VtxBuffer[i];
            const float opacity=std::clamp((vertex.pos.x-at.x)/width,0.f,1.f);
            vertex.col=(vertex.col & ~IM_COL32_A_MASK) |
                (ImU32(std::lround((vertex.col>>IM_COL32_A_SHIFT)*opacity))<<IM_COL32_A_SHIFT);
        }
        if(ImGui::IsItemActive() && ImGui::IsMouseDown(0)){
            const double value=std::clamp(double((ImGui::GetIO().MousePos.x-at.x)/width),0.,1.)*100;
            for(int i=0;i<int(track(0).size());++i)if(selected(0,i) && track(0)[i]["opacity"]!=value){
                track(0)[i]["opacity"]=value;dirty=refreshBar=true;
            }
        }
        for(int i=0;i<int(track(0).size());++i)if(selected(0,i)){
            const float x=at.x+width*track(0)[i]["opacity"].get<float>()/100;
            draw->AddLine({x,at.y},{x,at.y+height},ImGui::GetColorU32(ImGuiCol_CheckMark),2*scale);
        }
        draw->AddRect(at,bottom,ImGui::GetColorU32(ImGuiCol_Border),rounding,0,scale);
        opacityValue();
    }
    void colorValues(){
        const auto& first=track(1).front();const bool native=first.contains("native");
        const std::string model=native?first.at("native").at("model").get<std::string>():"rgb";
        const char* const rgb[]={"R","G","B"},*const cmyk[]={"C","M","Y","K"},*const lab[]={"L*","a*","b*"},*const gray[]={"灰度"};
        const auto names=model=="cmyk"?cmyk:model=="lab"?lab:model=="gray"?gray:rgb;
        const int count=model=="cmyk"?4:model=="gray"?1:3;
        if(ImGui::BeginTable("##gradient-color-values",count,ImGuiTableFlags_NoSavedSettings|ImGuiTableFlags_SizingStretchSame)){
            ImGui::TableNextRow();
            for(int i=0;i<count;++i){
                ImGui::TableSetColumnIndex(i);
                const std::string id=std::string("##")+names[i];
                const std::string format=std::string(names[i])+(native?" %.2f":" %.0f");
                selectionValue(id.c_str(),1,model=="lab" && i>0?-128:0,!native?255:model=="lab" && i>0?127:100,.25f,format.c_str(),
                    [=](const Json& stop){return (native?stop.at("native").at("values"):stop.at("rgb"))[i].get<double>();},
                    [=](Json& stop,double value){if(native)stop["native"]["values"][i]=value;else stop["rgb"][i]=int(std::lround(value));stop["source"]="userStop";});
            }
            ImGui::EndTable();
        }
    }
    void draw(bool checkLayout=false,bool viewBlocked=false,bool loading=false,bool awaitingPointer=false){
        moveGrip={};
        const bool canEdit=editable() && !viewBlocked;
        auto* viewport=ImGui::GetMainViewport();
        const ImVec2 origin=viewport->Pos;
        const RECT frame{0,0,LONG(viewport->Size.x),LONG(viewport->Size.y)};
        const UINT dpi=std::lround(96*scale);
        const auto timelineRect=overlay || loading?dockTimeline(frame,dpi):frame;
        const auto pickerRect=dockPicker(frame,dpi);
        constexpr ImGuiWindowFlags flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
            ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
        const ImVec2 at{origin.x+timelineRect.left,origin.y+timelineRect.top};
        const ImVec2 size{float(timelineRect.right-timelineRect.left),float(timelineRect.bottom-timelineRect.top)};
        MenuControls::PanelStyle style(scale,true,viewBlocked && editable());
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowPos(at);
        ImGui::SetNextWindowSize(size);
        ImGui::Begin("##gradient-dock",nullptr,flags);
        if(loading || (ready() && !overlay)){
            MenuControls::panelFrame(ImGui::GetBackgroundDrawList(viewport),at,{at.x+size.x,at.y+size.y},scale);
            MenuControls::detailLine(awaitingPointer?"移入此面板编辑渐变":"正在核对 Photoshop 画布…",scale);
            if(!awaitingPointer && MenuControls::button("关闭",scale,true))PostMessageW(window,WM_CLOSE,0,0);
            ImGui::End();return;
        }
        if(!ready()){ImGui::End();return;}
        MenuControls::panelFrame(ImGui::GetBackgroundDrawList(viewport),at,{at.x+size.x,at.y+size.y},scale);
        {
            const auto header=ImGui::GetCursorScreenPos();
            const float side=menuShortcutHeight*scale,closeX=header.x+ImGui::GetContentRegionAvail().x-side;
            const auto actions=ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos({closeX,header.y});
            if(MenuControls::iconButton("关闭并保留草稿",41,scale,true))finish(false);
            ImGui::SetCursorScreenPos({closeX-menuDenseGap*scale-ImGui::CalcTextSize("应用").x-2*menuDenseGap*scale,header.y});
            if(ImGui::IsKeyPressed(ImGuiKey_Escape) && !ImGui::IsAnyItemActive())finish(false);
            ImGui::BeginDisabled(!canEdit);
            if(MenuControls::button("应用",scale,true))finish(true);
            const float applyLeft=ImGui::GetItemRectMin().x;
            ImGui::EndDisabled();
            ImGui::SetCursorScreenPos(actions);
            ImGui::BeginDisabled(!canEdit);
            types();
            const auto timelineZoom=[&]{
                char fit[16];std::snprintf(fit,sizeof(fit),"%.0f%%",end*100.f/span);
                const char* zoomNames[]={"−",fit,"+"};int zoomAction=-1;
                if(MenuControls::segments("##gradient-zoom",zoomNames,zoomAction,scale,true)){
                    if(zoomAction==1){start=0;span=end;}
                    else zoom(zoomAction==0?1.f/1.5f:1.5f,.5f);
                }
            };
            const bool bothChannels=draft["fill"]["kind"]=="gradient" && draft["stroke"]["kind"]=="gradient";
            const auto channels=[&]{
                const char* names[]={"填充","描边"};
                if(MenuControls::segments("##gradient-channel",names,channel,scale,true)){selection={};anchorLane=primaryColor=-1;gesture=none;refreshBar=true;}
            };
            const auto nodeButtons=[&]{
                ImGui::BeginDisabled(track(1).size()>=32);if(MenuControls::button("加色标",scale,true))add(1,cursor);ImGui::EndDisabled();
                ImGui::SameLine();ImGui::BeginDisabled(track(0).size()>=32);if(MenuControls::button("加透明度",scale,true))add(0,cursor);ImGui::EndDisabled();
                ImGui::SameLine();ImGui::BeginDisabled(!canRemove());if(MenuControls::button("删除",scale,true))remove();ImGui::EndDisabled();
            };
            if(bothChannels){ImGui::SameLine();channels();}
            ImGui::SameLine();
            if(MenuControls::button(showParameters?"时间轴":"参数",scale,true))showParameters=!showParameters;
            moveGrip={LONG(ImGui::GetItemRectMax().x-origin.x+menuPanelGap*scale),LONG(header.y-origin.y),
                LONG(applyLeft-origin.x-menuPanelGap*scale),LONG(ImGui::GetItemRectMax().y-origin.y)};
            if(!showParameters){
                timelineZoom();ImGui::SameLine();nodeButtons();
                if(checkLayout && ImGui::GetItemRectMax().x+menuCompactGap*scale>closeX)
                    throw std::runtime_error("Gradient dock toolbar overlaps its close button");
                timeline(scale);
                overview();
            }
            if(canEdit && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive()){
                const auto& io=ImGui::GetIO();
                if(ImGui::IsKeyPressed(ImGuiKey_Delete))remove();
                if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A))for(int lane=0;lane<2;++lane){selection[lane]=0;for(int i=0;i<int(track(lane).size());++i)selection[lane]|=bit(i);}
                for(int sign:{-1,1})if((selection[0] || selection[1]) && ImGui::IsKeyPressed(sign<0?ImGuiKey_LeftArrow:ImGuiKey_RightArrow)){
                    const Json base=paint();const int delta=shift(base,sign*(io.KeyShift?41:4));if(delta)moveBy(base,delta);
                }
            }
            if(showParameters)parameters();
            ImGui::EndDisabled();
            if(phase=="error")MenuControls::detailLine(message.c_str(),scale);
            if(checkLayout && (ImGui::GetCursorPosY()-ImGui::GetStyle().ItemSpacing.y+ImGui::GetStyle().WindowPadding.y>size.y ||
                ImGui::GetScrollMaxY()>0 || ImGui::GetScrollMaxX()>0))
                throw std::runtime_error("Gradient controls exceeded the minimum client size: bottom="+std::to_string(ImGui::GetCursorPosY())+
                    ", height="+std::to_string(size.y)+", scroll="+std::to_string(ImGui::GetScrollMaxX())+","+std::to_string(ImGui::GetScrollMaxY()));
        }
        ImGui::End();
        if(overlay){
            const ImVec2 pickerAt{origin.x+pickerRect.left,origin.y+pickerRect.top};
            const ImVec2 pickerSize{float(pickerRect.right-pickerRect.left),float(pickerRect.bottom-pickerRect.top)};
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::SetNextWindowPos(pickerAt);
            ImGui::SetNextWindowSize(pickerSize);
            ImGui::Begin("##gradient-docked-picker",nullptr,flags);
            MenuControls::panelFrame(ImGui::GetBackgroundDrawList(viewport),pickerAt,{pickerAt.x+pickerSize.x,pickerAt.y+pickerSize.y},scale);
            {
                ImGui::BeginDisabled(!canEdit);
                if(editingOpacity())opacityPicker(pickerBodyWidth*scale);
                else if(hasRgbPicker())colorPicker(pickerBodyWidth*scale);
                if(!editingOpacity())colorValues();
                ImGui::EndDisabled();
                if(checkLayout && (ImGui::GetCursorPosY()-ImGui::GetStyle().ItemSpacing.y+ImGui::GetStyle().WindowPadding.y>pickerSize.y ||
                    ImGui::GetScrollMaxY()>0 || ImGui::GetScrollMaxX()>0))
                    throw std::runtime_error("Docked gradient picker exceeded its bounds: bottom="+std::to_string(ImGui::GetCursorPosY())+
                        ", height="+std::to_string(pickerSize.y)+", scroll="+std::to_string(ImGui::GetScrollMaxX())+","+
                        std::to_string(ImGui::GetScrollMaxY())+", opacity="+std::to_string(editingOpacity()));
            }
            ImGui::End();
        }
        if(canEdit)flush(ImGui::IsMouseReleased(ImGuiMouseButton_Left));
    }
};
