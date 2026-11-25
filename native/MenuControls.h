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
inline void segment(ImDrawList* draw,ImVec2 a,ImVec2 b,bool first,bool last,bool highlighted,float scale,bool vertical=false,ImGuiCol fill=ImGuiCol_ButtonHovered) {
    if(highlighted)draw->AddRectFilled(a,b,ImGui::GetColorU32(fill),ImGui::GetStyle().FrameRounding,
        first && last?ImDrawFlags_RoundCornersAll:first?(vertical?ImDrawFlags_RoundCornersTop:ImDrawFlags_RoundCornersLeft):last?(vertical?ImDrawFlags_RoundCornersBottom:ImDrawFlags_RoundCornersRight):ImDrawFlags_RoundCornersNone);
    if(!first)draw->AddLine(a,vertical?ImVec2{b.x,a.y}:ImVec2{a.x,b.y},ImGui::GetColorU32(ImGuiCol_Border),menuControlBorder*scale);
}
struct PanelStyle {
    bool stableVisual;
    static float padding(float scale,bool compact=false){return (compact?menuDenseGap:menuControlPadding)*scale;}
    explicit PanelStyle(float scale,bool compact=false,bool stableVisual=false,bool propertyPanel=false):stableVisual(stableVisual) {
        const float padding=PanelStyle::padding(scale,compact);
        const float gap=(compact?menuDenseGap:menuPanelGap)*scale;
        ImGui::PushFont(compact?menuFont(MenuTextRole::Shortcut):menuFont(MenuTextRole::Control));
        const float textInset=compact?menuDenseGap:propertyPanel?menuControlPadding:menuControlTextInset;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{textInset*scale,std::max(0.f,((compact?menuShortcutHeight:menuControlHeight)*scale-ImGui::GetFontSize())*.5f)});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,menuControlBorder*scale);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize,menuControlBorder*scale);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,ImGui::GetStyle().FrameRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{padding,padding});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{gap,gap});
        if(stableVisual)ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha,1.f);
    }
    ~PanelStyle(){ImGui::PopStyleVar(stableVisual?7:6);ImGui::PopFont();}
};
struct PropertyRowSpacing {
    explicit PropertyRowSpacing(float scale){ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{menuPanelGap*scale,menuCompactGap*scale});}
    ~PropertyRowSpacing(){ImGui::PopStyleVar();}
};
template<class Left,class Right>
void propertyPair(const char* id,float scale,bool compact,const Left& left,const Right& right){
    const float gap=menuCompactGap*scale;
    const float minimum=figmaDisplayIconSize*scale+gap+ImGui::GetFrameHeight()+ImGui::CalcTextSize("沿用").x+
        2*ImGui::GetStyle().FramePadding.x;
    if(compact && ImGui::GetContentRegionAvail().x<2*minimum+gap){left();right();return;}
    if(compact)ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,{gap*.5f,0});
    const ImGuiTableFlags flags=ImGuiTableFlags_SizingStretchSame|(compact?ImGuiTableFlags_NoPadOuterX:0);
    if(ImGui::BeginTable(id,2,flags)){
        ImGui::TableNextColumn();left();
        ImGui::TableNextColumn();right();
        ImGui::EndTable();
    }
    if(compact)ImGui::PopStyleVar();
}
inline void buttonCursor() {
    if(!ImGui::IsItemHovered() || GetCapture())return;
    POINT at{};
    if(GetCursorPos(&at) && WindowFromPoint(at)==ImGui::GetWindowViewport()->PlatformHandleRaw)
        SetCursor(LoadCursorW(nullptr,IDC_HAND));
}
inline bool button(const char* label,float scale,bool compact=false) {
    const bool clicked=ImGui::Button(label,{0,(compact?menuShortcutHeight:menuControlHeight)*scale});
    buttonCursor();return clicked;
}
inline void fieldLabel(const char* label,float width) {
    const float left=ImGui::GetCursorPosX();
    ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);
    ImGui::SameLine();ImGui::SetCursorPosX(left+width);
}
inline bool number(const char* label,double& value,float width,float step,double low,double high,const char* format) {
    ImGui::SetNextItemWidth(width);
    return ImGui::DragScalar(label,ImGuiDataType_Double,&value,step,&low,&high,format,ImGuiSliderFlags_AlwaysClamp);
}
template<size_t N> bool choice(const char* label,const char* const (&names)[N],size_t& selected,const char* preview=nullptr) {
    if(!ImGui::BeginCombo(label,preview?preview:names[selected]))return false;
    const size_t previous=selected;
    for(size_t i=0;i<N;++i)if(ImGui::Selectable(names[i],i==selected))selected=i;
    ImGui::EndCombo();return selected!=previous;
}
inline bool input(const char* id,const char* hint,char* value,size_t capacity,Emphasis emphasis=Emphasis::Accent,float width=-1) {
    ImGui::SetNextItemWidth(width);
    if(emphasis==Emphasis::Neutral)ImGui::PushStyleColor(ImGuiCol_NavCursor,ImGui::GetStyleColorVec4(ImGuiCol_Border));
    const bool enter=ImGui::InputTextWithHint(id,hint,value,capacity,ImGuiInputTextFlags_EnterReturnsTrue);
    if(emphasis==Emphasis::Neutral)ImGui::PopStyleColor();
    requireText(value);
    return enter;
}
inline void propertyInput(const char* id,const char* hint,char* value,size_t capacity,const char* unit,float scale) {
    const float available=ImGui::GetContentRegionAvail().x,gap=menuCompactGap*scale;
    float unitWidth;
    {
        MenuTextStyle text(MenuTextRole::Secondary);
        unitWidth=ImGui::CalcTextSize(unit).x;
    }
    const bool showUnit=available>=unitWidth+gap+menuControlHeight*scale;
    input(id,hint,value,capacity,Emphasis::Accent,showUnit?available-unitWidth-gap:-1);
    if(showUnit){
        const auto at=ImGui::GetItemRectMax();
        const float center=(ImGui::GetItemRectMin().y+at.y)*.5f;
        MenuTextStyle text(MenuTextRole::Secondary);
        ImGui::GetWindowDrawList()->AddText({at.x+gap,center-ImGui::GetTextLineHeight()*.5f},ImGui::GetColorU32(ImGuiCol_Text),unit);
    }
}
inline void tip(const char* text,float scale);
inline int listChoice(const char* id,const char* current,const std::vector<std::pair<std::string,std::string>>& rows,ImGuiTextFilter* filter=nullptr,bool* open=nullptr,int* hovered=nullptr,
                      const std::function<void(int,ImVec2,ImVec2)>& sample={},float scale=0) {
    ImGui::SetNextItemWidth(-1);
    if(!ImGui::BeginCombo(id,current,ImGuiComboFlags_HeightLarge)){
        if(scale>0 && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) &&
            ImGui::CalcTextSize(current).x>ImGui::GetItemRectSize().x-ImGui::GetFrameHeight()-2*ImGui::GetStyle().FramePadding.x)
            tip(current,scale);
        return -1;
    }
    if(open)*open=true;
    if(rows.empty())ImGui::TextUnformatted("正在读取字体…");
    else if(filter){
        input("##filter","搜索字体",filter->InputBuf,sizeof(filter->InputBuf));
        if(ImGui::IsItemEdited())filter->Build();
    }
    std::vector<int> visible;
    for(int i=0;i<int(rows.size());++i)
        if(!filter || filter->PassFilter(rows[i].first.c_str()) || filter->PassFilter(rows[i].second.c_str()))visible.push_back(i);
    int chosen=-1;
    ImGuiListClipper clipper;
    clipper.Begin(int(visible.size()));
    while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){
        const int index=visible[i];
        const auto& name=rows[index].first;
        requireText(name);
        ImGui::PushID(index);
        const auto at=ImGui::GetCursorScreenPos();
        if(ImGui::Selectable(sample?"##sample-row":name.c_str(),std::string_view(current)==name,0,{0,ImGui::GetTextLineHeight()}))chosen=index;
        if(hovered && ImGui::IsItemHovered())*hovered=index;
        if(sample){
            const auto itemEnd=ImGui::GetItemRectMax();
            const float split=std::round(at.x+(itemEnd.x-at.x)*.57f);
            auto* list=ImGui::GetWindowDrawList();
            list->PushClipRect(at,{split-ImGui::GetStyle().ItemSpacing.x,itemEnd.y},true);
            list->AddText(at,ImGui::GetColorU32(ImGuiCol_Text),name.c_str());
            list->PopClipRect();
            sample(index,{split,at.y},{itemEnd.x-ImGui::GetStyle().FramePadding.x,at.y+ImGui::GetTextLineHeight()});
        }
        ImGui::PopID();
    }
    ImGui::EndCombo();
    return chosen;
}
inline bool colorSwatch(const char* label,float (&rgb)[3],bool nativeTooltip=true) {
    return ImGui::ColorEdit3(label,rgb,ImGuiColorEditFlags_NoInputs|(nativeTooltip?0:ImGuiColorEditFlags_NoTooltip));
}
inline bool section(const char* label,bool expanded=true){return ImGui::CollapsingHeader(label,expanded?ImGuiTreeNodeFlags_DefaultOpen:0);}
inline bool multiline(const char* id,char* value,size_t capacity,ImVec2 size,ImGuiInputTextCallback callback,void* user,bool wrap,bool draftUndo=false) {
    const bool changed=ImGui::InputTextMultiline(id,value,capacity,size,ImGuiInputTextFlags_AllowTabInput|(wrap?ImGuiInputTextFlags_WordWrap:0)|
        (draftUndo?ImGuiInputTextFlags_NoUndoRedo:0)|(callback?ImGuiInputTextFlags_CallbackAlways|ImGuiInputTextFlags_CallbackCharFilter:0),callback,user);
    requireText(value);return changed;
}
inline bool multiline(const char* id,char* value,size_t capacity,float height,ImGuiInputTextCallback callback=nullptr,void* user=nullptr) {
    MenuTextStyle text(MenuTextRole::Body);
    return multiline(id,value,capacity,{-1,height},callback,user,true);
}
inline bool canvasText(const char* id,char* value,size_t capacity,ImVec2 size,float em,ImU32 color,bool wrap,
                       bool showText,ImGuiInputTextCallback callback,void* user,bool draftUndo=false) {
    // Document text uses canvas pixels. Panel typography is independent of canvas zoom.
    ImGui::PushFont(menuFont(MenuTextRole::Body),em);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,0.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImU32(0));
    ImGui::PushStyleColor(ImGuiCol_Text,showText?color:ImU32(0));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg,showText?ImGui::GetColorU32(ImGuiCol_TextSelectedBg):ImU32(0));
    ImGui::PushStyleColor(ImGuiCol_InputTextCursor,showText?ImGui::GetColorU32(ImGuiCol_InputTextCursor):ImU32(0));
    const bool changed=multiline(id,value,capacity,size,callback,user,wrap,draftUndo);
    ImGui::PopStyleColor(4);ImGui::PopStyleVar(3);ImGui::PopFont();
    return changed;
}
inline bool windowDragBar(const char* label,float height,float width) {
    const auto pos=ImGui::GetCursorScreenPos();
    const ImVec2 max={pos.x+width,pos.y+height};
    ImGui::InvisibleButton("##drag-window",{max.x-pos.x,height});
    auto* draw=ImGui::GetWindowDrawList();
    if(ImGui::IsItemHovered() || ImGui::IsItemActive())ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    const auto labelSize=ImGui::CalcTextSize(label);
    draw->AddText({pos.x+(max.x-pos.x-labelSize.x)*.5f,pos.y+(height-labelSize.y)*.5f},ImGui::GetColorU32(ImGuiCol_Text),label);
    return ImGui::IsItemActivated();
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
inline void fieldIcon(const char* label,int icon,float scale,float gap=-1) {
    const auto a=ImGui::GetCursorScreenPos();
    const float h=ImGui::GetFrameHeight(),glyph=figmaDisplayIconSize*scale;
    ImGui::Dummy({glyph,h});
    const bool disabled=(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)!=0;
    drawStyledMenuIcon(ImGui::GetWindowDrawList(),icon,{a.x,a.y+(h-glyph)*.5f},glyph,
        ImGui::GetColorU32(disabled?ImGuiCol_TextDisabled:ImGuiCol_Text),ImGui::GetColorU32(ImGuiCol_Button));
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))tip(label,scale);
    ImGui::SameLine(0,gap);
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
inline bool check(const char* label,bool& value) {
    const bool clicked=ImGui::Checkbox(label,&value);buttonCursor();return clicked;
}
inline bool iconButton(const char* label,int icon,float scale,bool compact=false,float height=0) {
    const auto a=ImGui::GetCursorScreenPos();
    const float side=(compact?menuShortcutHeight:menuControlHeight)*scale,glyph=figmaDisplayIconSize*scale,inset=(side-glyph)*.5f;
    const float h=height>0?height:side;
    const ImVec2 b={a.x+side,a.y+h};
    const bool clicked=ImGui::InvisibleButton(label,{side,h},ImGuiButtonFlags_EnableNav);
    buttonCursor();
    const bool disabled=(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)!=0;
    auto* draw=ImGui::GetWindowDrawList();
    surface(draw,a,b,scale,false);
    if(!disabled && (ImGui::IsItemHovered() || ImGui::IsItemActive()))segment(draw,a,b,true,true,true,scale);
    outline(draw,a,b,scale);
    drawStyledMenuIcon(draw,icon,{a.x+inset,a.y+(h-glyph)*.5f},glyph,ImGui::GetColorU32(disabled?ImGuiCol_TextDisabled:ImGuiCol_Text),ImGui::GetColorU32(ImGuiCol_Button));
    ImGui::RenderNavCursor(ImRect(a,b),ImGui::GetItemID());
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))tip(label,scale);
    return clicked;
}
struct IconGroupItem {
    const char* title;
    int glyph;
    bool selected=false;
    bool enabled=true;
    bool indeterminate=false;
    std::string detail;
    bool showLabel=false;
};
template<size_t N,class ItemAt,class Activate>
bool iconGroup(const char* id,float scale,bool fitContent,const ItemAt& itemAt,const Activate& activate){
    const auto origin=ImGui::GetCursorScreenPos();
    const float width=ImGui::GetContentRegionAvail().x,w=figmaDisplayIconWidth*scale,h=figmaDisplayWidgetHeight*scale,gap=menuPanelGap*scale;
    std::array<IconGroupItem,N> items{};
    std::array<float,N> widths{};
    for(size_t i=0;i<N;++i){
        items[i]=itemAt(int(i));
        widths[i]=w+(items[i].showLabel?ImGui::CalcTextSize(items[i].title).x+menuCompactGap*scale:0);
    }
    auto* draw=ImGui::GetWindowDrawList();bool changed=false;ImGui::PushID(id);
    float y=origin.y,usedWidth=0;
    for(size_t first=0;first<N;y+=h+gap) {
        size_t end=first+1;
        float rowWidth=widths[first];
        while(end<N && rowWidth+widths[end]<=width)rowWidth+=widths[end++];
        usedWidth=std::max(usedWidth,rowWidth);
        const ImVec2 rowEnd={origin.x+rowWidth,y+h};
        surface(draw,{origin.x,y},rowEnd,scale,false);
        float x=origin.x;
        for(size_t i=first;i<end;++i) {
            const auto& item=items[i];
            const float side=widths[i];
            const bool left=i==first,right=i+1==end;
            const ImVec2 at={x,y},itemEnd={x+side,y+h};
            ImGui::PushID(int(i));ImGui::SetCursorScreenPos(at);
            ImGui::BeginDisabled(!item.enabled);
            const bool clicked=ImGui::InvisibleButton(item.title,{side,h},ImGuiButtonFlags_EnableNav);
            const bool disabled=(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)!=0;
            const float mouse=ImGui::GetIO().MousePos.x;
            const bool seam=(!left && mouse<at.x+scale) || (!right && mouse>=itemEnd.x-scale);
            const auto fill=item.selected && !item.detail.empty()?ImGuiCol_ButtonActive:ImGuiCol_ButtonHovered;
            segment(draw,at,itemEnd,left,right,!disabled && ((!seam && ImGui::IsItemHovered()) || item.selected),scale,false,fill);
            const float icon=figmaDisplayIconSize*scale;
            const auto ink=ImGui::GetColorU32(disabled?ImGuiCol_TextDisabled:ImGuiCol_Text);
            drawStyledMenuIcon(draw,item.glyph,{at.x+(w-icon)*.5f,at.y+(h-icon)*.5f},icon,ink,ImGui::GetColorU32(ImGuiCol_Button));
            if(item.showLabel)draw->AddText({at.x+w,at.y+(h-ImGui::GetTextLineHeight())*.5f},ink,item.title);
            if(item.indeterminate){
                const float markRight=itemEnd.x-menuCompactGap*scale,markY=itemEnd.y-menuCompactGap*scale;
                draw->AddLine({markRight-3*scale,markY},{markRight,markY},ink,scale);
            }
            ImGui::RenderNavCursor(ImRect(at,itemEnd),ImGui::GetItemID());
            if(!seam && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)){
                if(item.detail.empty())tip(item.title,scale);
                else tip((std::string(item.title)+"\n"+item.detail).c_str(),scale);
            }
            if(clicked && !disabled && (!seam || !ImGui::IsMouseReleased(ImGuiMouseButton_Left))){activate(int(i));changed=true;}
            ImGui::EndDisabled();
            ImGui::PopID();x=itemEnd.x;
        }
        outline(draw,{origin.x,y},rowEnd,scale);
        first=end;
    }
    ImGui::PopID();ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy({fitContent?usedWidth:width,y-origin.y-gap});return changed;
}
template<size_t N> bool iconSegments(const char* id,const char* const (&names)[N],const int (&icons)[N],int& selected,float scale,bool fitContent=false,bool showLabels=false) {
    return iconGroup<N>(id,scale,fitContent,[&](int i){return IconGroupItem{names[i],icons[i],selected==i,true,false,{},showLabels};},
        [&](int i){selected=i;});
}
inline bool resultRow(const char* id,int icon,const char* title,const char* detail,float scale,bool& previewHover,ImVec2& previewAnchor,bool* previewClick=nullptr,bool previewAvailable=true,bool* editThumbnail=nullptr) {
    const auto at=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x,h=menuSearchRowHeight*scale,pad=menuControlPadding*scale;
    const float actionWidth=std::max(1.f,width-(previewAvailable?h:0));auto* draw=ImGui::GetWindowDrawList();
    const float glyph=figmaBasicIconSize*scale;
    const float thumbnailWidth=editThumbnail?std::min(actionWidth*.5f,2*pad+glyph):0.f;
    const ImVec2 actionAt={at.x+thumbnailWidth,at.y};
    if(editThumbnail){
        ImGui::InvisibleButton("##text-thumbnail",{thumbnailWidth,h});
        const bool over=ImGui::IsItemHovered() && !(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled);
        *editThumbnail=over && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        if(over)draw->AddRectFilled(at,{actionAt.x,at.y+h},ImGui::GetColorU32(ImGuiCol_ButtonHovered),ImGui::GetStyle().FrameRounding);
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))tip("双击编辑文字",scale);
        ImGui::SetCursorScreenPos(actionAt);
    }
    const bool clicked=ImGui::InvisibleButton(id,{actionWidth-thumbnailWidth,h},ImGuiButtonFlags_EnableNav);
    const bool disabled=(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)!=0;
    if(!disabled && ImGui::IsItemHovered())draw->AddRectFilled(actionAt,{at.x+actionWidth,at.y+h},ImGui::GetColorU32(ImGuiCol_ButtonHovered),ImGui::GetStyle().FrameRounding);
    const auto ink=ImGui::GetColorU32(disabled?ImGuiCol_TextDisabled:ImGuiCol_Text),surface=ImGui::GetColorU32(ImGuiCol_Button);
    drawStyledMenuIcon(draw,icon,{at.x+pad,at.y+(h-glyph)*.5f},glyph,ink,surface);
    auto* secondary=menuFont(MenuTextRole::Secondary);auto* body=menuFont(MenuTextRole::Body);
    const float meta=std::min(secondary->CalcTextSizeA(secondary->LegacySize,FLT_MAX,0,detail).x,actionWidth*.3f),end=at.x+actionWidth-pad-meta;
    const ImVec4 clip={at.x+pad+glyph+pad,at.y,std::max(at.x+pad+glyph+pad,end-pad),at.y+h};
    draw->AddText(body,body->LegacySize,{clip.x,at.y+(h-body->LegacySize)*.5f},ink,title,nullptr,0,&clip);
    const ImVec4 metaClip={end,at.y,at.x+actionWidth-pad,at.y+h};
    draw->AddText(secondary,secondary->LegacySize,{end,at.y+(h-secondary->LegacySize)*.5f},ImGui::GetColorU32(disabled?ImGuiCol_TextDisabled:ImGuiCol_PlotLines),detail,nullptr,0,&metaClip);
    ImGui::RenderNavCursor(ImRect(actionAt,{at.x+actionWidth,at.y+h}),ImGui::GetItemID());
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
bool textSegments(const char* id,const char* const (&names)[N],float scale,IsSelected isSelected,Activate activate,bool inlineGroup=false) {
    const auto origin=ImGui::GetCursorScreenPos();
    float width=ImGui::GetContentRegionAvail().x;
    const float h=inlineGroup?ImGui::GetFrameHeight():menuControlHeight*scale;
    if(width<=0){ImGui::Dummy({0,h});return false;}
    const float inset=inlineGroup?ImGui::GetStyle().FramePadding.x:menuControlTextInset*scale;
    std::array<float,N> widths{};float totalWidth=0;
    for(size_t i=0;i<N;++i){widths[i]=std::min(width,ImGui::CalcTextSize(names[i]).x+2*inset);totalWidth+=widths[i];}
    if(inlineGroup)width=std::min(width,totalWidth);
    auto* draw=ImGui::GetWindowDrawList();float y=origin.y;bool changed=false;ImGui::PushID(id);
    for(size_t first=0;first<N;){
        size_t last=first;float total=widths[first];
        while(last+1<N && total+widths[last+1]<=width){++last;total+=widths[last];}
        if(!inlineGroup)surface(draw,{origin.x,y},{origin.x+total,y+h},scale,false);
        float x=origin.x;
        for(size_t i=first;i<=last;++i){
            ImGui::SetCursorScreenPos({x,y});ImGui::PushID(int(i));const bool clicked=ImGui::InvisibleButton(names[i],{widths[i],h},ImGuiButtonFlags_EnableNav);
            const auto mouse=ImGui::GetIO().MousePos;
            const bool seam=i>first && mouse.x<x+scale || i<last && mouse.x>=x+widths[i]-scale;
            if(!seam)buttonCursor();
            const bool disabled=(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)!=0;
            segment(draw,{x,y},{x+widths[i],y+h},i==first,i==last,!disabled && ((!seam && ImGui::IsItemHovered()) || isSelected(i)),scale);
            const auto text=ImGui::CalcTextSize(names[i]);
            draw->AddText({x+(widths[i]-text.x)*.5f,y+(h-text.y)*.5f},ImGui::GetColorU32(disabled?ImGuiCol_TextDisabled:ImGuiCol_Text),names[i]);
            ImGui::RenderNavCursor(ImRect({x,y},{x+widths[i],y+h}),ImGui::GetItemID());
            if(clicked && (!seam || !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) && !disabled){activate(i);changed=true;}
            ImGui::PopID();x+=widths[i];
        }
        if(!inlineGroup)outline(draw,{origin.x,y},{origin.x+total,y+h},scale);
        first=last+1;
        if(first<N)y+=h+menuPanelGap*scale;
    }
    ImGui::PopID();ImGui::SetCursorScreenPos(origin);ImGui::Dummy({width,y-origin.y+h});return changed;
}
template<size_t N> bool segments(const char* id,const char* const (&names)[N],int& selected,float scale,bool inlineGroup=false) {
    return textSegments(id,names,scale,[&](size_t i){return selected==int(i);},[&](size_t i){selected=int(i);},inlineGroup);
}
template<size_t N> bool toggles(const char* id,const char* const (&names)[N],std::array<bool,N>& selected,float scale,bool compact=false) {
    return textSegments(id,names,scale,[&](size_t i){return selected[i];},[&](size_t i){selected[i]=!selected[i];},compact);
}
}
