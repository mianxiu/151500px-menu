#pragma once
#include "PerceptualColor.h"
#include "HarmonyMarkers.h"
static constexpr const char* perceptualPickerTips[]={"色相","饱和度／明度","主色","邻近色 −30°","邻近色 +30°","互补色"};
struct PerceptualPickerState {
    perceptual::Color base{},last{},layoutBase{};
    std::string key;int active=0,drag=-1,hoverPart=-1;
    bool meshReady=false;
    std::array<ImU32,193> mesh{};
    RECT wheelBounds{},hueBounds{},lightnessBounds{},squareBounds{};
    float svHue=0;
    std::array<ImVec2,perceptual::wheelCount> points{},rawPoints{};
    ImVec2 layoutCenter{},dragOffset{},dragStart{},dragLast{};
    float layoutRadius=-1,layoutScale=-1;
    bool dragHandle=false;
    size_t meshBuilds=0,layoutBuilds=0;
    static perceptual::Color color(const Json& rgb){return {rgb[0].get<double>(),rgb[1].get<double>(),rgb[2].get<double>()};}
    static ImU32 ink(perceptual::Color c){return IM_COL32(int(std::lround(c[0])),int(std::lround(c[1])),int(std::lround(c[2])),255);}
    static float markerRadius(int index){return index==0?9.f:4.5f;}
    static const HarmonyMarkerGeometry& geometry(int index){return harmonyMarkers[harmonyMarkerRoles[index]];}
    static float markerExtent(int index){return geometry(index).extent*markerRadius(index)/4.5f;}
    static bool markerHit(int index,ImVec2 at,ImVec2 mouse,float uiScale){
        const auto& g=geometry(index);const float unit=markerRadius(index)*uiScale/4.5f;
        const float x=(mouse.x-at.x)/unit+4.5f,y=(mouse.y-at.y)/unit+4.5f,padding=2.5f*uiScale/unit;
        bool inside=false;
        for(int i=0,j=g.count-1;i<g.count;j=i++){
            const auto a=g.points[j],b=g.points[i];
            if((a.y>y)!=(b.y>y) && x<(b.x-a.x)*(y-a.y)/(b.y-a.y)+a.x)inside=!inside;
            const float dx=b.x-a.x,dy=b.y-a.y,t=std::clamp(((x-a.x)*dx+(y-a.y)*dy)/(dx*dx+dy*dy),0.f,1.f);
            if(std::hypot(x-a.x-t*dx,y-a.y-t*dy)<=padding)return true;
        }
        return inside;
    }
    static void drawMarker(ImDrawList* draw,int index,ImVec2 at,float uiScale,ImU32 color,bool selected){
        const auto& g=geometry(index);const float unit=markerRadius(index)*uiScale/4.5f;
        std::array<ImVec2,32> vertices{};
        for(int i=0;i<g.count;++i)vertices[i]={at.x+(g.points[i].x-4.5f)*unit,at.y+(g.points[i].y-4.5f)*unit};
        // Outer strokes sit outside the real color. The same polygon drives hits.
        if(selected)draw->AddPolyline(vertices.data(),g.count,ImGui::GetColorU32(ImGuiCol_CheckMark),ImDrawFlags_Closed,5*uiScale);
        draw->AddPolyline(vertices.data(),g.count,IM_COL32(255,255,255,240),ImDrawFlags_Closed,3*uiScale);
        draw->AddPolyline(vertices.data(),g.count,IM_COL32(0,0,0,230),ImDrawFlags_Closed,uiScale);
        draw->AddConvexPolyFilled(vertices.data(),g.count,color);
    }
    void reset(const Json& rgb,const std::string& identity){
        auto current=color(rgb);
        if(key!=identity || current!=last){base=perceptual::lch(current,base[2]);key=identity;last=current;active=0;drag=-1;layoutRadius=-1;}
    }
    bool apply(Json& rgb){
        auto value=perceptual::wheelColor(base,active);for(auto& c:value)c=std::round(c);
        const auto previous=color(rgb);bool equal=true;for(int i=0;i<3;++i)equal&=std::abs(value[i]-previous[i])<.001;
        if(equal){last=previous;return false;}last=value;rgb=value;return true;
    }
    void dragPoint(double angle,double /*fraction*/,int index){
        active=index;base[2]=perceptual::hue(angle-perceptual::wheelOffsets[index]);
    }
    static double stripHue(perceptual::Color rgb,double fallback){
        float h,s,v;ImGui::ColorConvertRGBtoHSV(float(rgb[0]/255),float(rgb[1]/255),float(rgb[2]/255),h,s,v);
        if(s<.0001f){const auto c=perceptual::fromLch(.72,.18,fallback);
            ImGui::ColorConvertRGBtoHSV(float(c[0]/255),float(c[1]/255),float(c[2]/255),h,s,v);}
        return h*360.;
    }
    void dragHue(double hsvAngle,int index){
        // The strip is standard HSV; harmony relations stay in OKLCH. Invert
        // their monotonic hue mapping so the picked hue and its marker agree.
        const double lightness=base[1]<.00001?.72:base[0],chroma=base[1]<.00001?.18:base[1];
        auto hueAt=[&](double h){return stripHue(perceptual::fromLch(lightness,chroma,h),h);};
        const double start=hueAt(0),desired=perceptual::hue(hsvAngle-start);
        double lo=0,hi=360;
        for(int n=0;n<22;++n){const double mid=(lo+hi)*.5;
            if(perceptual::hue(hueAt(mid)-start)<desired)lo=mid;else hi=mid;}
        dragPoint((lo+hi)*.5,1,index);
    }
    // Markers occupy a separate lane outside the hue strip. Pack only their
    // displayed positions; raw hue anchors and drag offsets remain exact.
    void layout(ImVec2 center,float radius,float uiScale){
        if(layoutBase==base && layoutCenter.x==center.x && layoutCenter.y==center.y && layoutRadius==radius && layoutScale==uiScale)return;
        layoutBase=base;layoutCenter=center;layoutRadius=radius;layoutScale=uiScale;++layoutBuilds;
        const float top=center.y-radius,bottom=center.y+radius;
        std::array<int,perceptual::wheelCount> order{};
        for(int i=0;i<perceptual::wheelCount;++i){
            order[i]=i;rawPoints[i]={center.x,top+float(stripHue(perceptual::wheelColor(base,i),perceptual::hue(base[2]+perceptual::wheelOffsets[i]))/360)*2*radius};
            points[i]=rawPoints[i];
        }
        std::stable_sort(order.begin(),order.end(),[&](int a,int b){return rawPoints[a].y<rawPoints[b].y;});
        for(int n=0;n<perceptual::wheelCount;++n){
            const int i=order[n];const float edge=(markerExtent(i)+2.5f)*uiScale;
            points[i].y=std::clamp(points[i].y,top+edge,bottom-edge);
            if(n){const int j=order[n-1];points[i].y=std::max(points[i].y,points[j].y+(markerExtent(i)+markerExtent(j)+5.5f)*uiScale);}
        }
        for(int n=perceptual::wheelCount-1;n>=0;--n){
            const int i=order[n];points[i].y=std::min(points[i].y,bottom-(markerExtent(i)+2.5f)*uiScale);
            if(n+1<perceptual::wheelCount){const int j=order[n+1];points[i].y=std::min(points[i].y,points[j].y-(markerExtent(i)+markerExtent(j)+5.5f)*uiScale);}
        }
    }
    int hit(ImVec2 mouse,float uiScale) const {
        for(int i=0;i<perceptual::wheelCount;++i)if(markerHit(i,points[i],mouse,uiScale))return i;
        return -1;
    }
    void cache(){
        if(meshReady)return;
        meshReady=true;++meshBuilds;
        for(int a=0;a<=192;++a){float r,g,b;ImGui::ColorConvertHSVtoRGB(a/192.f,1,1,r,g,b);
            mesh[a]=ImGui::ColorConvertFloat4ToU32({r,g,b,1});}
    }
};
static float perceptualSquareSide(float width) {
    return std::max(40*scale,width-(2*menuPanelGap+menuHueBarWidth+menuHarmonyLaneWidth)*scale);
}
static bool drawPerceptualPicker(PerceptualPickerState& state,Json& rgb,float width,const std::string& identity) {
    using State=PerceptualPickerState;
    const auto at=ImGui::GetCursorScreenPos();auto* draw=ImGui::GetWindowDrawList();
    const float gap=menuPanelGap*scale,side=perceptualSquareSide(width),barWidth=menuHueBarWidth*scale;
    const ImVec2 lo=at,hi={at.x+side,at.y+side},barLo={hi.x+gap,lo.y},barHi={hi.x+gap+barWidth,hi.y};
    const ImVec2 lane={barHi.x+gap+menuHarmonyLaneWidth*scale*.5f,at.y+side*.5f};
    state.reset(rgb,identity);state.layout(lane,side*.5f,scale);bool changed=false;
    ImGui::InvisibleButton("##perceptual-hue-sv",{width,side});
    state.wheelBounds={LONG(at.x),LONG(at.y),LONG(at.x+width),LONG(at.y+side)};
    state.squareBounds={LONG(lo.x),LONG(lo.y),LONG(hi.x),LONG(hi.y)};
    state.hueBounds={LONG(barLo.x),LONG(barLo.y),LONG(barHi.x),LONG(barHi.y)};state.lightnessBounds={};
    const auto mouse=ImGui::GetIO().MousePos;
    const int hovered=ImGui::IsItemHovered()?state.hit(mouse,scale):-1;
    auto inSquare=[&](ImVec2 p){return p.x>=lo.x && p.x<=hi.x && p.y>=lo.y && p.y<=hi.y;};
    auto inHue=[&](ImVec2 p){return p.x>=barLo.x && p.x<=barHi.x && p.y>=barLo.y && p.y<=barHi.y;};
    state.hoverPart=-1;
    if(ImGui::IsItemHovered() && !ImGui::IsItemActive())
        state.hoverPart=hovered>=0?2+hovered:inSquare(mouse)?1:inHue(mouse)?0:-1;
    if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)){
        state.dragHandle=hovered>=0;
        state.drag=hovered>=0?hovered:inSquare(mouse)?-2:inHue(mouse)?0:-1;
        state.dragStart=mouse;state.dragLast={-FLT_MAX,-FLT_MAX};state.dragOffset=hovered>=0?ImVec2{0,mouse.y-state.rawPoints[hovered].y}:ImVec2{};
        if(hovered>=0 && state.active!=hovered){state.active=hovered;changed|=state.apply(rgb);}
        float h,s,v;ImGui::ColorConvertRGBtoHSV(rgb[0].get<float>()/255,rgb[1].get<float>()/255,rgb[2].get<float>()/255,h,s,v);
        if(s>.0001f)state.svHue=h;
        else {const auto chromatic=perceptual::fromLch(.72,.18,state.base[2]);ImGui::ColorConvertRGBtoHSV(float(chromatic[0]/255),float(chromatic[1]/255),float(chromatic[2]/255),state.svHue,s,v);}
    }
    if(ImGui::IsItemActive() && ImGui::IsMouseDown(0) && state.drag!=-1 && (mouse.x!=state.dragLast.x || mouse.y!=state.dragLast.y) &&
       (!state.dragHandle || std::hypot(mouse.x-state.dragStart.x,mouse.y-state.dragStart.y)>.5f)){
        state.dragLast=mouse;
        if(state.drag==-2){
            float r,g,b;ImGui::ColorConvertHSVtoRGB(state.svHue,std::clamp((mouse.x-lo.x)/side,0.f,1.f),std::clamp(1-(mouse.y-lo.y)/side,0.f,1.f),r,g,b);
            Json next=Json::array({int(std::lround(r*255)),int(std::lround(g*255)),int(std::lround(b*255))});
            if(next!=rgb){rgb=next;state.last=State::color(rgb);state.base=perceptual::lch(state.last,state.base[2]);state.active=0;changed=true;}
        }else{
            const double fraction=std::clamp(double(mouse.y-state.dragOffset.y-barLo.y)/side,0.,1.);
            state.dragHue(fraction*360,state.drag);changed|=state.apply(rgb);
        }
    }
    if(!ImGui::IsMouseDown(0))state.drag=-1;
    state.cache();state.layout(lane,side*.5f,scale);
    for(int a=0;a<192;++a){
        const ImVec2 p={barLo.x,barLo.y+side*a/192.f},q={barHi.x,barLo.y+side*(a+1)/192.f};
        draw->AddRectFilledMultiColor(p,q,state.mesh[a],state.mesh[a],state.mesh[a+1],state.mesh[a+1]);
    }
    draw->AddRect(barLo,barHi,ImGui::GetColorU32(ImGuiCol_Border),ImGui::GetStyle().FrameRounding,0,scale);
    float hue,saturation,value;ImGui::ColorConvertRGBtoHSV(rgb[0].get<float>()/255,rgb[1].get<float>()/255,rgb[2].get<float>()/255,hue,saturation,value);
    if(state.drag==-2)hue=state.svHue;
    else if(saturation<.0001f){const auto chromatic=perceptual::fromLch(.72,.18,state.base[2]);float ignoredS,ignoredV;
        ImGui::ColorConvertRGBtoHSV(float(chromatic[0]/255),float(chromatic[1]/255),float(chromatic[2]/255),hue,ignoredS,ignoredV);}
    float r,g,b;ImGui::ColorConvertHSVtoRGB(hue,1,1,r,g,b);const ImU32 hueColor=ImGui::ColorConvertFloat4ToU32({r,g,b,1});
    draw->AddRectFilledMultiColor(lo,hi,IM_COL32_WHITE,hueColor,hueColor,IM_COL32_WHITE);
    draw->AddRectFilledMultiColor(lo,hi,IM_COL32(0,0,0,0),IM_COL32(0,0,0,0),IM_COL32_BLACK,IM_COL32_BLACK);
    draw->AddRect(lo,hi,ImGui::GetColorU32(ImGuiCol_Border),ImGui::GetStyle().FrameRounding,0,scale);
    const ImVec2 selected={lo.x+saturation*side,hi.y-value*side};
    draw->AddCircle(selected,4*scale,IM_COL32_BLACK,24,2*scale);draw->AddCircle(selected,5*scale,IM_COL32_WHITE,24,scale);
    for(int i=0;i<perceptual::wheelCount;++i){
        const auto p=state.points[i],anchor=state.rawPoints[i];
        draw->AddLine({barHi.x,anchor.y},{p.x-State::markerExtent(i)*scale-2*scale,p.y},ImGui::GetColorU32(ImGuiCol_TextDisabled),scale);
        State::drawMarker(draw,i,p,scale,State::ink(perceptual::wheelColor(state.base,i)),i==state.active);
    }
    ImGui::SetCursorScreenPos({at.x,at.y+side+gap});
    return changed;
}
