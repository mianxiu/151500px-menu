// Shared Caps/tool surfaces and widgets; inherit the current PS palette.
namespace MenuControls {
enum class Emphasis {Accent,Neutral};
inline void surface(ImDrawList* draw,ImVec2 a,ImVec2 b,float scale,bool shadow=true) {
    const auto rounding=ImGui::GetStyle().FrameRounding;
    if(shadow)menuShadow::draw(draw,a,b,rounding,scale);
    draw->AddRectFilled(a,b,ImGui::GetColorU32(ImGuiCol_Button),rounding);
}
inline void outline(ImDrawList* draw,ImVec2 a,ImVec2 b,float scale) {
    draw->AddRect(a,b,ImGui::GetColorU32(ImGuiCol_Border),ImGui::GetStyle().FrameRounding,0,menuControlBorder*scale);
}
inline void panelFrame(ImDrawList* draw,ImVec2 a,ImVec2 b,float scale) {
    draw->AddRectFilled(a,b,ImGui::GetColorU32(ImGuiCol_WindowBg),ImGui::GetStyle().FrameRounding);
    const float inset=menuControlBorder*scale*.5f;
    outline(draw,{a.x+inset,a.y+inset},{b.x-inset,b.y-inset},scale);
}
inline void segment(ImDrawList* draw,ImVec2 a,ImVec2 b,bool first,bool last,bool highlighted,float scale,bool vertical=false) {
    if(highlighted)draw->AddRectFilled(a,b,ImGui::GetColorU32(ImGuiCol_ButtonHovered),ImGui::GetStyle().FrameRounding,
        first && last?ImDrawFlags_RoundCornersAll:first?(vertical?ImDrawFlags_RoundCornersTop:ImDrawFlags_RoundCornersLeft):last?(vertical?ImDrawFlags_RoundCornersBottom:ImDrawFlags_RoundCornersRight):ImDrawFlags_RoundCornersNone);
    if(!first)draw->AddLine(a,vertical?ImVec2{b.x,a.y}:ImVec2{a.x,b.y},ImGui::GetColorU32(ImGuiCol_Border),menuControlBorder*scale);
}
struct PanelStyle {
    explicit PanelStyle(float scale) {
        ImGui::PushFont(menuFont(MenuTextRole::Control));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{menuControlTextInset*scale,std::max(0.f,(menuControlHeight*scale-ImGui::GetFontSize())*.5f)});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,menuControlBorder*scale);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize,menuControlBorder*scale);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,ImGui::GetStyle().FrameRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{menuControlPadding*scale,menuControlPadding*scale});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{menuPanelGap*scale,menuPanelGap*scale});
    }
    ~PanelStyle(){ImGui::PopStyleVar(6);ImGui::PopFont();}
};
inline bool button(const char* label,float scale) {return ImGui::Button(label,{0,menuControlHeight*scale});}
inline bool number(const char* label,double& value,float width,float step,double low,double high,const char* format) {
    ImGui::SetNextItemWidth(width);
    return ImGui::DragScalar(label,ImGuiDataType_Double,&value,step,&low,&high,format,ImGuiSliderFlags_AlwaysClamp);
}
template<size_t N> bool choice(const char* label,const char* const (&names)[N],size_t& selected) {
    if(!ImGui::BeginCombo(label,names[selected]))return false;
    const size_t previous=selected;
    for(size_t i=0;i<N;++i)if(ImGui::Selectable(names[i],i==selected))selected=i;
    ImGui::EndCombo();return selected!=previous;
}
inline bool input(const char* id,const char* hint,char* value,size_t capacity,Emphasis emphasis=Emphasis::Accent) {
    ImGui::SetNextItemWidth(-1);
    if(emphasis==Emphasis::Neutral)ImGui::PushStyleColor(ImGuiCol_NavCursor,ImGui::GetStyleColorVec4(ImGuiCol_Border));
    const bool enter=ImGui::InputTextWithHint(id,hint,value,capacity,ImGuiInputTextFlags_EnterReturnsTrue);
    if(emphasis==Emphasis::Neutral)ImGui::PopStyleColor();
    requireText(value);
    return enter;
}
inline bool multiline(const char* id,char* value,size_t capacity,float height,ImGuiInputTextCallback callback=nullptr,void* user=nullptr) {
    MenuTextStyle text(MenuTextRole::Body);
    const bool changed=ImGui::InputTextMultiline(id,value,capacity,{-1,height},ImGuiInputTextFlags_AllowTabInput|ImGuiInputTextFlags_WordWrap|(callback?ImGuiInputTextFlags_CallbackAlways|ImGuiInputTextFlags_CallbackCharFilter:0),callback,user);
    requireText(value);return changed;
}
using TextRange=std::pair<int,int>;
struct PreviewLine {const char* begin;const char* end;float x;};
inline std::vector<PreviewLine> previewLines(const char* value,float width,bool list) {
    std::vector<PreviewLine> rows;const char* end=value+strlen(value),*line=value;float indent=0;bool first=true;
    while(line<end) {
        if(first && list) {
            const char* paragraph=std::find(line,end,'\n'),*body=line;
            while(body<paragraph && (*body==' ' || *body=='\t'))++body;
            while(body<paragraph && *body!=' ' && *body!='\t')++body;
            if(body==paragraph)indent=0;
            else {while(body<paragraph && (*body==' ' || *body=='\t'))++body;indent=std::min(ImGui::CalcTextSize(line,body).x,std::max(0.f,width-ImGui::GetFontSize()));}
        }
        const float x=first?0:indent;
        const char* stop=ImFontCalcWordWrapPositionEx(ImGui::GetFont(),ImGui::GetFontSize(),line,end,std::max(1.f,width-x),ImDrawTextFlags_WrapKeepBlanks);
        if(stop==line && *line!='\n'){unsigned int c;stop=line+ImTextCharFromUtf8(&c,line,end);}
        rows.push_back({line,stop,x});first=stop<end && *stop=='\n';line=first?stop+1:stop;
    }
    if(end>value && end[-1]=='\n')rows.push_back({end,end,0});
    return rows;
}
inline void textPreview(const char* id,const char* value,float height,const std::vector<TextRange>& matches={},bool list=false) {
    MenuTextStyle text(MenuTextRole::Body);
    ImGui::BeginChild(id,{0,height},ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding);
    if(matches.empty() && !list){ImGui::PushTextWrapPos(0);ImGui::TextUnformatted(value);ImGui::PopTextWrapPos();}
    else {
        auto* font=ImGui::GetFont();auto* draw=ImGui::GetWindowDrawList();const float size=ImGui::GetFontSize();
        const auto at=ImGui::GetCursorScreenPos();const float width=std::max(1.f,ImGui::GetContentRegionAvail().x);
        size_t range=0;int row=0;
        for(const auto& layout:previewLines(value,width,list)) {
            const char* line=layout.begin,*stop=layout.end;
            const char* cursor=line;
            while(cursor<stop) {
                const int offset=int(cursor-value);while(range<matches.size() && matches[range].second<=offset)++range;
                const bool accent=range<matches.size() && matches[range].first<=offset;
                const char* next=range<matches.size()?std::min(stop,value+(accent?matches[range].second:matches[range].first)):stop;
                const float x=ImGui::CalcTextSize(line,cursor).x;
                draw->AddText(font,size,{at.x+layout.x+x,at.y+row*size},ImGui::GetColorU32(accent?ImGuiCol_CheckMark:ImGuiCol_Text),cursor,next);
                cursor=next;
            }
            ++row;
        }
        ImGui::Dummy({width,std::max(1,row)*size});
    }
    ImGui::EndChild();
}
struct TipStyle {
    explicit TipStyle(float scale) {
        ImGui::PushFont(menuFont(MenuTextRole::Secondary));
        ImGui::PushStyleColor(ImGuiCol_PopupBg,ImGui::ColorConvertU32ToFloat4(menuTipBackground));
        ImGui::PushStyleColor(ImGuiCol_Border,ImGui::ColorConvertU32ToFloat4(menuTipBorder));
        ImGui::PushStyleColor(ImGuiCol_Text,ImGui::ColorConvertU32ToFloat4(menuTipText));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{menuControlPadding*scale,menuControlPadding*scale});
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding,ImGui::GetStyle().FrameRounding);
    }
    ~TipStyle(){ImGui::PopStyleVar(2);ImGui::PopStyleColor(3);ImGui::PopFont();}
};
inline void tip(const char* text,float scale) {
    TipStyle style(scale);ImGui::BeginTooltip();ImGui::TextUnformatted(text);ImGui::EndTooltip();
}
inline void detailLine(const char* value,float scale,float reserve=0) {
    MenuTextStyle text(MenuTextRole::Secondary);
    const auto at=ImGui::GetCursorScreenPos();
    const float width=std::max(1.f,ImGui::GetContentRegionAvail().x-reserve),height=ImGui::GetTextLineHeight();
    const char* end=value+strcspn(value,"\r\n");
    const auto measured=ImGui::CalcTextSize(value,end);
    ImGui::Dummy({width,height});
    ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(),at,{at.x+width,at.y+height},at.x+width,value,end,&measured);
    if((*end || measured.x>width) && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)){
        TipStyle style(scale);ImGui::BeginTooltip();ImGui::PushTextWrapPos(2*menuPanelWidth*scale);
        ImGui::TextUnformatted(value);ImGui::PopTextWrapPos();ImGui::EndTooltip();
    }
}
inline bool iconButton(const char* label,int icon,float scale) {
    const auto a=ImGui::GetCursorScreenPos();
    const float side=menuControlHeight*scale,glyph=figmaDisplayIconSize*scale,inset=(side-glyph)*.5f;
    const ImVec2 b={a.x+side,a.y+side};
    const bool clicked=ImGui::InvisibleButton(label,{side,side},ImGuiButtonFlags_EnableNav);
    const bool disabled=(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)!=0;
    auto* draw=ImGui::GetWindowDrawList();
    surface(draw,a,b,scale,false);
    if(!disabled && (ImGui::IsItemHovered() || ImGui::IsItemActive()))segment(draw,a,b,true,true,true,scale);
    outline(draw,a,b,scale);
    drawStyledMenuIcon(draw,icon,{a.x+inset,a.y+inset},glyph,ImGui::GetColorU32(disabled?ImGuiCol_TextDisabled:ImGuiCol_Text),ImGui::GetColorU32(ImGuiCol_Button));
    ImGui::RenderNavCursor(ImRect(a,b),ImGui::GetItemID());
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))tip(label,scale);
    return clicked;
}
template<size_t N> bool iconSegments(const char* id,const char* const (&names)[N],const int (&icons)[N],int& selected,float scale) {
    const auto origin=ImGui::GetCursorScreenPos();
    const float width=ImGui::GetContentRegionAvail().x,w=figmaDisplayIconWidth*scale,h=figmaDisplayWidgetHeight*scale,gap=menuPanelGap*scale;
    const int columns=std::max(1,std::min(int(N),int(width/w)));
    auto* draw=ImGui::GetWindowDrawList();bool changed=false;ImGui::PushID(id);
    for(size_t first=0;first<N;first+=columns) {
        const int count=std::min(columns,int(N-first));const float y=origin.y+(first/columns)*(h+gap);
        surface(draw,{origin.x,y},{origin.x+w*count,y+h},scale,false);
        for(int n=0;n<count;++n) {
            const int i=int(first)+n;const ImVec2 at={origin.x+n*w,y};ImGui::PushID(i);ImGui::SetCursorScreenPos(at);
            const bool clicked=ImGui::InvisibleButton(names[i],{w,h},ImGuiButtonFlags_EnableNav);
            const bool disabled=(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)!=0;
            const float mouse=ImGui::GetIO().MousePos.x;
            const bool seam=(n && mouse<at.x+scale) || (n<count-1 && mouse>=at.x+w-scale);
            segment(draw,at,{at.x+w,at.y+h},n==0,n==count-1,!disabled && ((!seam && ImGui::IsItemHovered()) || selected==i),scale);
            const float icon=figmaDisplayIconSize*scale;
            drawStyledMenuIcon(draw,icons[i],{at.x+(w-icon)*.5f,at.y+(h-icon)*.5f},icon,ImGui::GetColorU32(disabled?ImGuiCol_TextDisabled:ImGuiCol_Text),ImGui::GetColorU32(ImGuiCol_Button));
            ImGui::RenderNavCursor(ImRect(at,{at.x+w,at.y+h}),ImGui::GetItemID());
            if(!seam && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))tip(names[i],scale);
            if(clicked && !disabled && (!seam || !ImGui::IsMouseReleased(ImGuiMouseButton_Left))){selected=i;changed=true;}
            ImGui::PopID();
        }
        outline(draw,{origin.x,y},{origin.x+w*count,y+h},scale);
    }
    ImGui::PopID();ImGui::SetCursorScreenPos(origin);ImGui::Dummy({width,((N+columns-1)/columns)*(h+gap)-gap});return changed;
}
inline bool resultRow(const char* id,int icon,const char* title,const char* detail,float scale,bool& previewHover,ImVec2& previewAnchor,bool* previewClick=nullptr,bool previewAvailable=true) {
    const auto at=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x,h=menuSearchRowHeight*scale,pad=menuControlPadding*scale;
    const float actionWidth=std::max(1.f,width-(previewAvailable?h:0));auto* draw=ImGui::GetWindowDrawList();
    const bool clicked=ImGui::InvisibleButton(id,{actionWidth,h},ImGuiButtonFlags_EnableNav);
    const bool disabled=(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)!=0;
    if(!disabled && ImGui::IsItemHovered())draw->AddRectFilled(at,{at.x+actionWidth,at.y+h},ImGui::GetColorU32(ImGuiCol_ButtonHovered),ImGui::GetStyle().FrameRounding);
    const auto ink=ImGui::GetColorU32(disabled?ImGuiCol_TextDisabled:ImGuiCol_Text),surface=ImGui::GetColorU32(ImGuiCol_Button);
    const float glyph=figmaBasicIconSize*scale;
    drawStyledMenuIcon(draw,icon,{at.x+pad,at.y+(h-glyph)*.5f},glyph,ink,surface);
    auto* secondary=menuFont(MenuTextRole::Secondary);auto* body=menuFont(MenuTextRole::Body);
    const float meta=std::min(secondary->CalcTextSizeA(secondary->LegacySize,FLT_MAX,0,detail).x,actionWidth*.3f),end=at.x+actionWidth-pad-meta;
    const ImVec4 clip={at.x+pad+glyph+pad,at.y,std::max(at.x+pad+glyph+pad,end-pad),at.y+h};
    draw->AddText(body,body->LegacySize,{clip.x,at.y+(h-body->LegacySize)*.5f},ink,title,nullptr,0,&clip);
    const ImVec4 metaClip={end,at.y,at.x+actionWidth-pad,at.y+h};
    draw->AddText(secondary,secondary->LegacySize,{end,at.y+(h-secondary->LegacySize)*.5f},ImGui::GetColorU32(disabled?ImGuiCol_TextDisabled:ImGuiCol_PlotLines),detail,nullptr,0,&metaClip);
    ImGui::RenderNavCursor(ImRect(at,{at.x+actionWidth,at.y+h}),ImGui::GetItemID());
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))tip((std::string(title)+"\n"+detail).c_str(),scale);
    previewAnchor={at.x+actionWidth,at.y};
    previewHover=false;if(previewClick)*previewClick=false;
    if(previewAvailable) {
        ImGui::SetCursorScreenPos(previewAnchor);
        const bool opened=ImGui::InvisibleButton("##preview",{h,h},ImGuiButtonFlags_EnableNav);
        if(previewClick)*previewClick=opened && !disabled;
        if(!disabled && ImGui::IsItemHovered())draw->AddRectFilled(previewAnchor,{at.x+width,at.y+h},ImGui::GetColorU32(ImGuiCol_ButtonHovered),ImGui::GetStyle().FrameRounding);
        drawStyledMenuIcon(draw,39,{previewAnchor.x+(h-glyph)*.5f,at.y+(h-glyph)*.5f},glyph,ink,surface);
        previewHover=!disabled && ImGui::IsItemHovered();
    }
    ImGui::SetCursorScreenPos(at);ImGui::Dummy({width,h});return clicked && !disabled;
}
inline void previewTile(ID3D11ShaderResourceView* image,ImVec2 imageSize,const char* message,ImVec2 anchor,ImVec4 limit,float scale) {
    const float padding=menuControlPadding*scale,side=std::max(1.f,std::min(menuLayerPreviewSize*scale,std::min(limit.z-limit.x,limit.w-limit.y)-2*padding));
    const ImVec2 size={side+2*padding,side+2*padding};
    ImGui::SetNextWindowPos({std::clamp(anchor.x-size.x-padding,limit.x,std::max(limit.x,limit.z-size.x)),std::clamp(anchor.y,limit.y,std::max(limit.y,limit.w-size.y))});
    ImGui::SetNextWindowSize(size);TipStyle style(scale);ImGui::BeginTooltip();
    if(image) {
        const float factor=side/std::max(imageSize.x,imageSize.y);const ImVec2 extent={imageSize.x*factor,imageSize.y*factor};
        const auto at=ImGui::GetCursorScreenPos();ImGui::SetCursorScreenPos({at.x+(side-extent.x)*.5f,at.y+(side-extent.y)*.5f});
        ImGui::Image((ImTextureID)(intptr_t)image,extent);
    } else ImGui::TextWrapped("%s",message);
    ImGui::EndTooltip();
}

template<size_t N> float segmentsHeight(const char* const (&names)[N],float width,float scale) {
    width=std::max(1.f,width);float used=0;int rows=1;
    for(const auto* name:names){
        const float item=std::min(width,ImGui::CalcTextSize(name).x+2*menuControlTextInset*scale);
        if(used>0 && used+item>width){++rows;used=0;}
        used+=item;
    }
    return rows*menuControlHeight*scale+(rows-1)*menuPanelGap*scale;
}

template<size_t N,typename IsSelected,typename Activate>
bool textSegments(const char* id,const char* const (&names)[N],float scale,IsSelected isSelected,Activate activate) {
    const auto origin=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x,h=menuControlHeight*scale;
    if(width<=0){ImGui::Dummy({0,h});return false;}
    std::array<float,N> widths{};for(size_t i=0;i<N;++i)widths[i]=std::min(width,ImGui::CalcTextSize(names[i]).x+2*menuControlTextInset*scale);
    auto* draw=ImGui::GetWindowDrawList();float y=origin.y;bool changed=false;ImGui::PushID(id);
    for(size_t first=0;first<N;){
        size_t last=first;float total=widths[first];
        while(last+1<N && total+widths[last+1]<=width){++last;total+=widths[last];}
        surface(draw,{origin.x,y},{origin.x+total,y+h},scale,false);float x=origin.x;
        for(size_t i=first;i<=last;++i){
            ImGui::SetCursorScreenPos({x,y});ImGui::PushID(int(i));const bool clicked=ImGui::InvisibleButton(names[i],{widths[i],h},ImGuiButtonFlags_EnableNav);
            const auto mouse=ImGui::GetIO().MousePos;
            const bool seam=i>first && mouse.x<x+scale || i<last && mouse.x>=x+widths[i]-scale;
            const bool disabled=(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)!=0;
            segment(draw,{x,y},{x+widths[i],y+h},i==first,i==last,!disabled && ((!seam && ImGui::IsItemHovered()) || isSelected(i)),scale);
            const auto text=ImGui::CalcTextSize(names[i]);
            draw->AddText({x+(widths[i]-text.x)*.5f,y+(h-text.y)*.5f},ImGui::GetColorU32(disabled?ImGuiCol_TextDisabled:ImGuiCol_Text),names[i]);
            ImGui::RenderNavCursor(ImRect({x,y},{x+widths[i],y+h}),ImGui::GetItemID());
            if(clicked && (!seam || !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) && !disabled){activate(i);changed=true;}
            ImGui::PopID();x+=widths[i];
        }
        outline(draw,{origin.x,y},{origin.x+total,y+h},scale);first=last+1;
        if(first<N)y+=h+menuPanelGap*scale;
    }
    ImGui::PopID();ImGui::SetCursorScreenPos(origin);ImGui::Dummy({width,y-origin.y+h});return changed;
}
template<size_t N> bool segments(const char* id,const char* const (&names)[N],int& selected,float scale) {
    return textSegments(id,names,scale,[&](size_t i){return selected==int(i);},[&](size_t i){selected=int(i);});
}
template<size_t N> bool toggles(const char* id,const char* const (&names)[N],std::array<bool,N>& selected,float scale) {
    return textSegments(id,names,scale,[&](size_t i){return selected[i];},[&](size_t i){selected[i]=!selected[i];});
}
}
