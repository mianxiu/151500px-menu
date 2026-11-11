#include "ScreenSample.h"
// Frame-coalesced paint and mask updates share one rollbackable transaction.
struct ShapeColorEditor {
    bool available=false,dirty=false,vector=true,livePreview=false,batchMask=false,gradientEditor=false;
    std::string nativeModel;
    int batchCount=0;Json maskValues,maskMixed;
    int channel=0,stop=0;
    Json original,draft,patterns,lastPreview;
    std::array<Json,2> modes;
    std::array<Json,2> lastStops;
    std::array<bool,2> methodExplicit{};
    char hexInput[8]{};
    std::string hexKey;
    ULONGLONG copiedAt=0;
    bool copyFailed=false;
    RECT bounds{},maskBounds{},copyBounds{},gradientBounds{},featherBounds{},densityBounds{},hexBounds{};
    bool gradientDragging=false;
    std::array<ImGuiID,2> maskInputIDs{};
    std::array<ImGuiID,4> rgbInputIDs{};
    std::array<RECT,4> rgbBounds{};
    float gradientDragOffset=0;
    void prepare(const Json& data) {
        if(ImGui::GetCurrentContext()) {
            const auto active=ImGui::GetActiveID();
            // A fresh target must not inherit an old numeric edit buffer.
            if(active && (std::find(maskInputIDs.begin(),maskInputIDs.end(),active)!=maskInputIDs.end() ||
                          std::find(rgbInputIDs.begin(),rgbInputIDs.end(),active)!=rgbInputIDs.end()))
                ImGui::ClearActiveID();
        }
        maskPreviewStarted=false;shapeDraftOnly=false;livePreview=false;batchMask=false;batchCount=0;maskValues=maskMixed=Json::object();
        lastPreview=Json();maskInputIDs={};rgbInputIDs={};rgbBounds={};methodExplicit={};
        available=false;dirty=false;gradientEditor=false;nativeModel.clear();bounds=maskBounds=copyBounds=gradientBounds=featherBounds=densityBounds=hexBounds={};gradientDragging=false;gradientDragOffset=0;channel=stop=0;modes={};hexInput[0]=0;hexKey.clear();copiedAt=0;copyFailed=false;
        if(!data.value("available",false))return;
        if(data.value("mask_batch",false)) {
            batchCount=data.at("layer_ids").size();if(batchCount<2 || batchCount>128)return;
            maskValues=data.at("mask");maskMixed=data.at("mixed");
            original=draft=Json{{"revision",data.at("revision")},{"mask_batch",true},{"mask",Json::object()}};
            batchMask=true;vector=true;livePreview=data.value("live_preview",false);shapeDraftOnly=!livePreview;available=true;return;
        }
        if(!data.at("revision").is_string() || !data.at("fill").is_object() || !data.at("stroke").is_object())return;
        original=draft=Json{{"revision",data.at("revision")},{"fill",data.at("fill")},{"stroke",data.at("stroke")}};
        patterns=data.value("patterns",Json::array());
        for(const auto& item:patterns)if(item.is_string())requireText(item.get<std::string>());
        for(int i=0;i<2;++i){const auto& p=draft.at(i?"stroke":"fill");modes[i]=Json::object();modes[i][p.at("kind").get<std::string>()]=p;lastStops[i]=p.value("stops",Json());}
        vector=data.value("vector",true);livePreview=data.value("live_preview",false);
        nativeModel=data.value("native_model",std::string());
        gradientEditor=data.value("gradient_editor",false);
        shapeDraftOnly=!livePreview;available=true;
        if(vector && data.contains("mask") && data["mask"].is_object())original["mask"]=draft["mask"]=data["mask"];
    }
    Json& current(){return draft.at(channel?"stroke":"fill");}
    bool nativePaint(){const auto& p=current();return !nativeModel.empty() || p.contains("native") || p.contains("stops") && std::any_of(p["stops"].begin(),p["stops"].end(),[](const Json& s){return s.contains("native");});}
    static Json colorOf(const Json& p){return p.contains("native")?Json{{"native",p["native"]}}:Json{{"rgb",p.value("rgb",Json::array({0,0,0}))}};}
    static Json nativeWhite(const std::string& model){return Json{{"native",{{"model",model},{"values",model=="cmyk"?Json::array({0,0,0,0}):model=="lab"?Json::array({100,0,0}):Json::array({0})}}}};}
    bool ready() const {return available && holdMode && panelKind==2 && layerAware && (batchMask ? layerCount==batchCount : layerKind==6 && layerCount==1)
        && !layerLocked && !layerBackground && !inputFocused
        && (canExecute() || maskPreviewStarted && allowed && !taskBusy && !busyFeedback) && !holdFinished;}
    void changed(){
        if(!batchMask)for(int i=0;i<2;++i){
            auto& p=draft[i?"stroke":"fill"];
            const auto stops=p.value("stops",Json());
            if(p.value("kind","")=="gradient" && stops!=lastStops[i] && !methodExplicit[i])p["method"]="perceptual";
            if(p.value("kind","")!="gradient")methodExplicit[i]=false;
            lastStops[i]=stops;
        }
        dirty=draft!=original;
    }
    void mode(int next) {
        const char* names[]={"solid","gradient","pattern"};
        auto& p=current();const std::string old=p.at("kind");modes[channel][old]=p;
        const bool enabled=p.value("enabled",true);const double width=p.value("width",1.);
        if(modes[channel].contains(names[next]))p=modes[channel][names[next]];
        else {
            Json color=colorOf(p.contains("stops")?p["stops"][0]:p);
            if(!nativeModel.empty() && !p.contains("rgb") && !p.contains("native") && !p.contains("stops"))color=nativeWhite(nativeModel);
            p=Json{{"kind",names[next]},{"enabled",enabled}};
            if(next==0)p.update(color);
            if(next==1){Json white=color.contains("native")?nativeWhite(color["native"]["model"]):Json{{"rgb",Json::array({255,255,255})}};color["location"]=0;white["location"]=4096;p["stops"]=Json::array({color,white});p["method"]="perceptual";}
            if(next==2)p["name"]=patterns.empty()?"":patterns[0].get<std::string>();
            if(channel)p["width"]=width;
        }
        stop=0;gradientDragging=false;changed();
    }
    bool valid() const {
        if(!available)return false;
        if(batchMask)return !draft.at("mask").empty();
        for(const char* name:{"fill","stroke"})if(draft.at(name).value("kind","")=="pattern" && draft.at(name).value("name","").empty())return false;
        return true;
    }
    bool hasMask() const {return vector && draft.contains("mask");}
    bool hit(POINT point) const {return PtInRect(&bounds,point) || PtInRect(&maskBounds,point);}
};
static ShapeColorEditor shapeEditor;
// Presentation only: a hidden panel retains its request, draft and rollback.
// A new menu session always starts with the ordinary operations visible.
static bool shapePanelOpen=false,shapeReturning=false,shapeHoverReady=false;
static RECT shapeModeBounds{},shapeHoverBounds{};
static std::array<RECT,1> shapeModeRows{};
static bool shapePanelVisible(){return shapePanelOpen && shapeEditor.ready();}
static bool shapeClipboardReady=false,shapeClipboardAvailable=false;
static int shapeTipCandidate=-1;
static constexpr int shapeModeTipBase=25+perceptual::wheelCount;
static constexpr int shapeEyedropTip=shapeModeTipBase+1;
static constexpr int shapeGradientEditorTip=shapeEyedropTip+1;
static constexpr int shapeGradientConfirmTip=shapeGradientEditorTip+1;
static constexpr int shapeTipCount=shapeGradientConfirmTip+1;
static std::array<RECT,shapeTipCount> shapeTipRows{};
static const char* shapeTipNames[shapeTipCount]={"填充","描边","纯色","渐变","图案","无填充／无描边","描边宽度：双击输入","复制当前Hex颜色","黑色","白色","交换填充与描边","复制形状填充与描边","粘贴到当前选中的形状图层","羽化：双击数值输入","密度：双击数值输入",
    "","","","","","","","切换渐变插值：感知／线性／经典",perceptualPickerTips[0],perceptualPickerTips[1],
    perceptualPickerTips[2],perceptualPickerTips[3],perceptualPickerTips[4],perceptualPickerTips[5],"展开／收起","屏幕取色","打开独立渐变编辑","确认当前调整并打开独立渐变编辑"};
static void shapeItemTip(int index,bool allowHover=true) {
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();shapeTipRows[index]={LONG(a.x),LONG(a.y),LONG(b.x),LONG(b.y)};
    if(allowHover && ImGui::IsItemHovered() && !ImGui::IsItemActive())shapeTipCandidate=200+index;
}
static Json testedShapePaint;
static size_t testedShapeGrants=0;
static std::vector<Json> testedMaskEvents;
static void emitMaskPreview(bool commit=false,bool returnToMenu=false) {
    if(stabilityInspection)return;
    if(!shapeEditor.livePreview)return;
    maskPreviewStarted=true;
    shapeEditor.lastPreview=shapeEditor.draft;
    Json event={{"type","action"},{"action",commit?"mask.preview.commit":"mask.preview.update"},
        {"session",menuEpoch},{"params",shapeEditor.draft}};
    if(returnToMenu)event["return_to_menu"]=true;
    if(selfTest){testedMaskEvents.push_back(event);return;}
    std::cout<<event.dump()<<std::endl;
}
static std::string testedCopiedHex;
static std::string shapeHex(const Json& rgb) {
    const char* digits="0123456789ABCDEF";std::string result="#";
    for(const auto& channel:rgb){const int value=std::clamp(int(std::lround(channel.get<double>())),0,255);result+=digits[value>>4];result+=digits[value&15];}
    return result;
}
static bool parseShapeHex(const std::string& text,Json& rgb) {
    const size_t start=text.size()==7 && text[0]=='#'?1:0;if(text.size()-start!=6)return false;
    int values[3]={};
    for(int i=0;i<6;++i){char c=text[start+i];int digit=c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:c>='A' && c<='F'?c-'A'+10:-1;
        if(digit<0)return false;values[i/2]=(values[i/2]<<4)|digit;}
    rgb=Json::array({values[0],values[1],values[2]});return true;
}
static void copyShapeHex(const Json& rgb) {
    const std::string value=shapeHex(rgb);
    if(selfTest)testedCopiedHex=value;
    else {
        ImGui::SetClipboardText(value.c_str());const char* readback=ImGui::GetClipboardText();
        if(!readback || value!=readback){shapeEditor.copyFailed=true;shapeEditor.copiedAt=0;return;}
    }
    shapeEditor.copyFailed=false;
    shapeEditor.copiedAt=GetTickCount64();
}
static void commitShapeDraft(bool returnToMenu=false) {
    if(stabilityInspection)return;
    if(!shapeEditor.ready() || (!shapeEditor.dirty && !maskPreviewStarted) || !shapeEditor.valid()){send("hide");return;}
    // Both draft and preview confirmation return from the foreground menu to
    // its own backend. Preserve the same bounded focus grant for both paths.
    if(selfTest)++testedShapeGrants;
    else if(backendParent)AllowSetForegroundWindow(backendParent);
    if(maskPreviewStarted){
        emitMaskPreview(true);
        if(selfTest){testedActions.emplace_back("shape.paint");testedShapePaint=shapeEditor.draft;}
        return;
    }
    if(selfTest){testedActions.emplace_back("shape.paint");testedShapePaint=shapeEditor.draft;return;}
    std::cout<<Json{{"type","action"},{"action","shape.paint"},{"session",menuEpoch},{"params",shapeEditor.draft},{"return_to_menu",returnToMenu}}.dump()<<std::endl;
}
static bool shapeSegment(const char* id,const std::vector<std::string>& names,int& selected,float width,bool lockSelection=false,const std::vector<int>& icons={},int tipBase=-1,const std::vector<bool>& enabled={},const std::vector<int>& tips={}) {
    const ImVec2 start=ImGui::GetCursorScreenPos();const float height=(icons.empty()?menuControlHeight:figmaDisplayWidgetHeight)*scale,part=width/names.size();
    const float rounding=ImGui::GetStyle().FrameRounding;
    auto* draw=ImGui::GetWindowDrawList();bool changed=false;
    ImGui::PushID(id);
    for(int i=0;i<int(names.size());++i){
        ImGui::PushID(i);ImVec2 a={start.x+part*i,start.y},b={a.x+part,a.y+height};
        const bool disabled=(lockSelection && selected!=i) || (!enabled.empty() && !enabled[i]);
        ImGui::BeginDisabled(disabled);
        ImGui::SetCursorScreenPos(a);bool click=ImGui::InvisibleButton("segment",{part,height});
        const bool itemDisabled=(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)!=0;
        const auto mouse=ImGui::GetIO().MousePos;
        bool divider=(i && mouse.x<a.x+scale) || (i+1<int(names.size()) && mouse.x>=b.x-scale);
        if(click && !divider && selected!=i){selected=i;changed=true;}
        auto color=ImGui::GetColorU32(!itemDisabled && selected==i?ImGuiCol_FrameBgActive:ImGui::IsItemHovered()?ImGuiCol_FrameBgHovered:ImGuiCol_FrameBg);
        const auto ink=ImGui::GetColorU32(itemDisabled?ImGuiCol_TextDisabled:ImGuiCol_Text);
        const bool swatch=!itemDisabled && (icons.size()>i && (icons[i]==19 || icons[i]==20));
        ImDrawFlags corners=i==0?ImDrawFlags_RoundCornersLeft:i+1==int(names.size())?ImDrawFlags_RoundCornersRight:ImDrawFlags_RoundCornersNone;
        draw->AddRectFilled(a,b,color,rounding,corners);
        if(icons.empty()){const auto size=ImGui::CalcTextSize(names[i].c_str());draw->AddText({a.x+(part-size.x)*.5f,a.y+(height-size.y)*.5f},ink,names[i].c_str());}
        else drawStyledMenuIcon(draw,icons[i],{a.x+(part-figmaDisplayIconSize*scale)*.5f,a.y+(height-figmaDisplayIconSize*scale)*.5f},figmaDisplayIconSize*scale,swatch?IM_COL32(37,37,37,255):ink,swatch?IM_COL32_WHITE:ImGui::GetColorU32(ImGuiCol_FrameBg));
        if(tipBase>=0 || !tips.empty())shapeItemTip(tips.empty()?tipBase+i:tips[i],!divider);
        if(i)draw->AddLine(a,{a.x,b.y},ImGui::GetColorU32(ImGuiCol_Border),scale);
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    draw->AddRect(start,{start.x+width,start.y+height},ImGui::GetColorU32(ImGuiCol_Border),rounding,0,scale);
    ImGui::PopID();ImGui::SetCursorScreenPos({start.x,start.y+height+menuPanelGap*scale});return changed;
}
static bool shapeIconButton(const char* id,int icon,int tip,float width,bool enabled=true,bool swatch=false,float logicalHeight=menuControlHeight) {
    ImGui::BeginDisabled(!enabled);const auto at=ImGui::GetCursorScreenPos();
    const float height=logicalHeight*scale;
    const bool clicked=ImGui::InvisibleButton(id,{width,height});shapeItemTip(tip);
    const bool itemDisabled=(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)!=0;
    auto* draw=ImGui::GetWindowDrawList();const auto color=ImGui::GetColorU32(ImGui::IsItemHovered()?ImGuiCol_FrameBgHovered:ImGuiCol_FrameBg);
    draw->AddRectFilled(at,{at.x+width,at.y+height},color,ImGui::GetStyle().FrameRounding);
    draw->AddRect(at,{at.x+width,at.y+height},ImGui::GetColorU32(ImGuiCol_Border),ImGui::GetStyle().FrameRounding,0,scale);
    drawStyledMenuIcon(draw,icon,{at.x+(width-figmaDisplayIconSize*scale)*.5f,at.y+(height-figmaDisplayIconSize*scale)*.5f},figmaDisplayIconSize*scale,
        swatch && !itemDisabled?IM_COL32(37,37,37,255):ImGui::GetColorU32(itemDisabled?ImGuiCol_TextDisabled:ImGuiCol_Text),swatch && !itemDisabled?IM_COL32_WHITE:color);
    ImGui::EndDisabled();return clicked;
}
static void shapePaintToolbar(float width) {
    const bool idle=shapeClipboardReady && shapeEditor.nativeModel.empty() && !shapeEditor.dirty && !maskPreviewStarted && canExecute();
    int selected=-1;
    if(shapeSegment("paint-clipboard",{"复制形状填充与描边","粘贴形状填充与描边"},selected,std::min(width,2*figmaDisplayIconWidth*scale),false,{16,17},11,
        {idle && !shapeEditor.batchMask && shapeEditor.vector,idle && shapeClipboardAvailable && shapeEditor.vector})){
        holdFinished=true;send(selected?"shape.pastePaint":"shape.copyPaint");
    }
}
static bool shapeRgbInputs(Json& rgb,float width,const std::string& model="") {
    const auto at=ImGui::GetCursorScreenPos();auto* draw=ImGui::GetWindowDrawList();
    const int count=int(rgb.size());const bool native=!model.empty();
    const float height=menuControlHeight*scale,part=native?width:width/count,totalHeight=height*(native?count:1),rounding=ImGui::GetStyle().FrameRounding;
    draw->AddRectFilled(at,{at.x+width,at.y+totalHeight},ImGui::GetColorU32(ImGuiCol_FrameBg),rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,0.f);ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0.f);
    const auto pad=ImGui::GetStyle().FramePadding;ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{menuControlPadding*scale,pad.y});
    for(auto role:{ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive})ImGui::PushStyleColor(role,ImVec4(0,0,0,0));
    draw->ChannelsSplit(2);bool changed=false;const char* formats[]={"R:%d","G:%d","B:%d"};
    const char* cmyk[]={"C: %.2f %%","M: %.2f %%","Y: %.2f %%","K: %.2f %%"};const char* lab[]={"L*: %.2f","a*: %.2f","b*: %.2f"};
    for(int i=0;i<count;++i){
        ImGui::PushID(i);draw->ChannelsSetCurrent(1);const ImVec2 a={at.x+(native?0:i*part),at.y+(native?i*height:0)},b={a.x+part,a.y+height};
        ImGui::SetCursorScreenPos(a);ImGui::SetNextItemWidth(part);
        shapeEditor.rgbInputIDs[i]=ImGui::GetID("##rgb");
        if(native){double value=rgb[i].get<double>(),low=model=="lab" && i?-128.:0.,high=model=="lab" && i?127.:100.;
            if(ImGui::DragScalar("##rgb",ImGuiDataType_Double,&value,.1f,&low,&high,model=="cmyk"?cmyk[i]:model=="lab"?lab[i]:"灰度: %.2f %%",ImGuiSliderFlags_AlwaysClamp)){rgb[i]=value;changed=true;}}
        else {int value=std::clamp(int(std::lround(rgb[i].get<double>())),0,255);
            if(ImGui::DragInt("##rgb",&value,1.f,0,255,formats[i],ImGuiSliderFlags_AlwaysClamp)){rgb[i]=value;changed=true;}}
        shapeEditor.rgbBounds[i]={LONG(a.x),LONG(a.y),LONG(b.x),LONG(b.y)};
        if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
            draw->ChannelsSetCurrent(0);draw->AddRectFilled(a,b,ImGui::GetColorU32(ImGui::IsItemActive()?ImGuiCol_HeaderActive:ImGuiCol_HeaderHovered),rounding,
                i==0?(native?ImDrawFlags_RoundCornersTop:ImDrawFlags_RoundCornersLeft):i==count-1?(native?ImDrawFlags_RoundCornersBottom:ImDrawFlags_RoundCornersRight):ImDrawFlags_RoundCornersNone);
        }
        ImGui::PopID();
    }
    draw->ChannelsMerge();ImGui::PopStyleColor(3);ImGui::PopStyleVar(3);
    for(int i=1;i<count;++i)draw->AddLine(native?ImVec2{at.x,at.y+i*height}:ImVec2{at.x+i*part,at.y},native?ImVec2{at.x+width,at.y+i*height}:ImVec2{at.x+i*part,at.y+height},ImGui::GetColorU32(ImGuiCol_Border),scale);
    draw->AddRect(at,{at.x+width,at.y+totalHeight},ImGui::GetColorU32(ImGuiCol_Border),rounding,0,scale);
    ImGui::SetCursorScreenPos({at.x,at.y+totalHeight+menuPanelGap*scale});return changed;
}
static void shapeHexInput(Json& rgb,float width) {
    const auto at=ImGui::GetCursorScreenPos();auto* draw=ImGui::GetWindowDrawList();
    const float height=menuControlHeight*scale,copyWidth=figmaDisplayIconWidth*scale,inputWidth=width-copyWidth,rounding=ImGui::GetStyle().FrameRounding;
    const auto surface=ImGui::GetColorU32(ImGuiCol_FrameBg);
    shapeEditor.hexBounds={LONG(at.x),LONG(at.y),LONG(at.x+width),LONG(at.y+height)};
    draw->AddRectFilled(at,{at.x+width,at.y+height},surface,rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,0.f);ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0.f);
    const auto pad=ImGui::GetStyle().FramePadding;ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{menuControlPadding*scale,pad.y});
    for(auto role:{ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive})ImGui::PushStyleColor(role,ImVec4(0,0,0,0));
    draw->ChannelsSplit(2);draw->ChannelsSetCurrent(1);
    const float prefixWidth=ImGui::CalcTextSize("#").x;
    ImGui::SetCursorScreenPos(at);if(ImGui::InvisibleButton("##hex-prefix",{prefixWidth,height}))ImGui::SetKeyboardFocusHere();
    ImGui::SetCursorScreenPos({at.x+prefixWidth,at.y});ImGui::SetNextItemWidth(inputWidth-prefixWidth);
    if(ImGui::InputText("##hex",shapeEditor.hexInput,7,ImGuiInputTextFlags_CharsUppercase|ImGuiInputTextFlags_CharsHexadecimal)){
        Json parsed;if(parseShapeHex(shapeEditor.hexInput,parsed)){rgb=parsed;shapeEditor.current()["enabled"]=true;shapeEditor.changed();}
    }
    if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
        draw->ChannelsSetCurrent(0);draw->AddRectFilled(at,{at.x+inputWidth,at.y+height},ImGui::GetColorU32(ImGui::IsItemActive()?ImGuiCol_HeaderActive:ImGuiCol_HeaderHovered),rounding,ImDrawFlags_RoundCornersLeft);
    }
    draw->ChannelsMerge();ImGui::PopStyleColor(3);ImGui::PopStyleVar(3);
    draw->AddText({at.x+menuControlPadding*scale,at.y+pad.y},ImGui::GetColorU32(ImGuiCol_Text),"#");
    Json parsed;ImGui::BeginDisabled(!parseShapeHex(shapeEditor.hexInput,parsed));
    ImGui::SetCursorScreenPos({at.x+inputWidth+scale,at.y+scale});
    const bool clicked=ImGui::InvisibleButton("##copy-hex",{copyWidth-2*scale,height-2*scale});shapeItemTip(7);
    if(ImGui::IsItemHovered())draw->AddRectFilled({at.x+inputWidth,at.y},{at.x+width,at.y+height},ImGui::GetColorU32(ImGuiCol_HeaderHovered),rounding,ImDrawFlags_RoundCornersRight);
    const bool disabled=(ImGui::GetItemFlags()&ImGuiItemFlags_Disabled)!=0;
    drawStyledMenuIcon(draw,16,{at.x+inputWidth+(copyWidth-figmaDisplayIconSize*scale)*.5f,at.y+(height-figmaDisplayIconSize*scale)*.5f},figmaDisplayIconSize*scale,ImGui::GetColorU32(disabled?ImGuiCol_TextDisabled:ImGuiCol_Text),surface);
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();shapeEditor.copyBounds={LONG(a.x),LONG(a.y),LONG(b.x),LONG(b.y)};
    if(clicked)copyShapeHex(rgb);ImGui::EndDisabled();
    draw->AddLine({at.x+inputWidth,at.y},{at.x+inputWidth,at.y+height},ImGui::GetColorU32(ImGuiCol_Border),scale);
    draw->AddRect(at,{at.x+width,at.y+height},ImGui::GetColorU32(ImGuiCol_Border),rounding,0,scale);
    ImGui::SetCursorScreenPos({at.x,at.y+height+menuPanelGap*scale});
}
static int gradientPosition(float x,float left,float width) {
    return width>0?std::clamp(int(std::lround((x-left)/width*4096.f)),0,4096):0;
}
static void moveGradientStop(int position) {
    auto& stops=shapeEditor.current().at("stops");const int index=shapeEditor.stop;
    const int low=index?stops[index-1].at("location").get<int>():0;
    const int high=index+1<int(stops.size())?stops[index+1].at("location").get<int>():4096;
    stops[index]["location"]=std::clamp(position,low,high);shapeEditor.changed();
}
static PerceptualPickerState perceptualPicker;
static bool drawShapePicker(Json& rgb,float width) {
    const auto identity=std::to_string(menuEpoch)+":"+std::to_string(shapeEditor.channel)+":"+std::to_string(shapeEditor.stop);
    const bool changed=drawPerceptualPicker(perceptualPicker,rgb,width,identity);
    shapeTipRows[23]=perceptualPicker.hueBounds;shapeTipRows[24]=perceptualPicker.squareBounds;
    for(int i=0;i<perceptual::wheelCount;++i){
        const auto p=perceptualPicker.points[i];const float hit=(PerceptualPickerState::markerExtent(i)+2.5f)*scale;
        shapeTipRows[25+i]={LONG(p.x-hit),LONG(p.y-hit),LONG(p.x+hit),LONG(p.y+hit)};
    }
    if(perceptualPicker.hoverPart>=0)shapeTipCandidate=223+perceptualPicker.hoverPart;
    return changed;
}
static void drawGradientBar(float width) {
    auto& stops=shapeEditor.current().at("stops");
    shapeEditor.stop=std::clamp(shapeEditor.stop,0,int(stops.size())-1);
    const auto at=ImGui::GetCursorScreenPos();const float marker=menuGradientHandleSize*scale;
    const float left=at.x+marker*.5f,barWidth=width-marker,barHeight=menuGradientBarHeight*scale;
    const ImVec2 a={left,at.y},b={left+barWidth,at.y+barHeight};
    shapeEditor.gradientBounds={LONG(at.x),LONG(at.y),LONG(at.x+width),LONG(at.y+menuGradientEditorHeight*scale)};
    ImGui::InvisibleButton("##gradient-bar",{width,menuGradientEditorHeight*scale});
    const auto mouse=ImGui::GetIO().MousePos;const bool enabled=!(GImGui->LastItemData.ItemFlags&ImGuiItemFlags_Disabled);
    auto stopX=[&](int i){return left+stops[i].at("location").get<float>()/4096.f*barWidth;};
    if(enabled && ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
        int nearest=shapeEditor.stop;float distance=std::abs(mouse.x-stopX(nearest));
        for(int i=0;i<int(stops.size());++i){const float d=std::abs(mouse.x-stopX(i));if(d<distance-.01f){nearest=i;distance=d;}}
        // Coincident stops remain independently selectable without reordering.
        if(distance<marker*.5f)for(int step=1;step<int(stops.size());++step){const int i=(shapeEditor.stop+step)%stops.size();
            if(std::abs(mouse.x-stopX(i))<=distance+.01f){nearest=i;break;}}
        shapeEditor.stop=nearest;
        shapeEditor.gradientDragging=mouse.y>=b.y && distance<=marker*.6f;
        shapeEditor.gradientDragOffset=mouse.x-stopX(nearest);
    }
    if(enabled && shapeEditor.gradientDragging && ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        moveGradientStop(gradientPosition(mouse.x-shapeEditor.gradientDragOffset,left,barWidth));
    if(!enabled || !ImGui::IsMouseDown(ImGuiMouseButton_Left))shapeEditor.gradientDragging=false;
    auto* draw=ImGui::GetWindowDrawList();
    const bool native=shapeEditor.nativePaint();
    auto color=[&](int i){return native?ImGui::GetColorU32(ImGuiCol_FrameBg):PerceptualPickerState::ink(PerceptualPickerState::color(stops[i].at("rgb")));};
    auto segment=[&](float x1,float x2,ImU32 c1,ImU32 c2){
        if(x2<=x1)return;
        ImDrawFlags corners=ImDrawFlags_RoundCornersNone;
        if(x1<=a.x+.01f)corners=ImDrawFlags_RoundCornersLeft;
        if(x2>=b.x-.01f)corners=corners==ImDrawFlags_RoundCornersNone?ImDrawFlags_RoundCornersRight:ImDrawFlags_RoundCornersAll;
        const int begin=draw->VtxBuffer.Size;
        draw->AddRectFilled({x1,a.y},{x2,b.y},IM_COL32_WHITE,ImGui::GetStyle().FrameRounding,corners);
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw,begin,draw->VtxBuffer.Size,{x1,a.y},{x2,a.y},c1,c2);
    };
    segment(a.x,stopX(0),color(0),color(0));
    const auto method=shapeEditor.current().value("method",std::string("classic"));
    for(int i=0;i+1<int(stops.size());++i){
        if(native){segment(stopX(i),stopX(i+1),color(i),color(i+1));continue;}
        const auto c1=PerceptualPickerState::color(stops[i].at("rgb")),c2=PerceptualPickerState::color(stops[i+1].at("rgb"));
        const int count=method=="classic"?1:std::max(1,int(std::ceil((stopX(i+1)-stopX(i))/(4*scale))));
        for(int n=0;n<count;++n){double t1=double(n)/count,t2=double(n+1)/count;
            segment(stopX(i)+(stopX(i+1)-stopX(i))*float(t1),stopX(i)+(stopX(i+1)-stopX(i))*float(t2),
                PerceptualPickerState::ink(perceptual::mix(c1,c2,t1,method)),PerceptualPickerState::ink(perceptual::mix(c1,c2,t2,method)));}
    }
    segment(stopX(int(stops.size())-1),b.x,color(int(stops.size())-1),color(int(stops.size())-1));
    draw->AddRect(a,b,ImGui::GetColorU32(ImGuiCol_Border),ImGui::GetStyle().FrameRounding,0,scale);
    auto handle=[&](int i){
        const float x=stopX(i),y=b.y+menuCompactGap*scale;
        const ImVec2 points[]={{x,y},{x+marker*.5f,y+marker*.35f},{x+marker*.5f,y+marker},{x-marker*.5f,y+marker},{x-marker*.5f,y+marker*.35f}};
        draw->AddConvexPolyFilled(points,5,color(i));
        draw->AddPolyline(points,5,ImGui::GetColorU32(i==shapeEditor.stop?ImGuiCol_CheckMark:ImGuiCol_Text),ImDrawFlags_Closed,(i==shapeEditor.stop?2.f:1.f)*scale);
    };
    for(int i=0;i<int(stops.size());++i)if(i!=shapeEditor.stop)handle(i);handle(shapeEditor.stop);
    ImGui::SetCursorScreenPos({at.x,at.y+menuGradientEditorHeight*scale+menuPanelGap*scale});
}
// Screen sampling consumes its own click; it never forwards a click to PS.
static bool shapeEyedropActive=false,shapeEyedropDown=false;
static LPARAM shapeEyedropEpoch=0;
static std::string shapeEyedropRevision;
static int shapeEyedropChannel=0,shapeEyedropStop=0;
static void stopShapeEyedrop() {
    const bool owned=shapeEyedropActive;
    shapeEyedropActive=shapeEyedropDown=false;
    if(owned)shapeHoverReady=false;
    RemovePropW(window,screenSampleProperty);
    if(owned && GetCapture()==window)ReleaseCapture();
}
static bool shapeEyedropAllowed() {
    if(!shapeEditor.ready() || !shapePanelOpen || holdFinished || !IsWindowVisible(window))return false;
    const HWND foreground=GetForegroundWindow(),owner=GetWindow(window,GW_OWNER);
    return selfTest || foreground==window || (!shapeEyedropActive && owner && foreground==owner);
}
static bool beginShapeEyedrop() {
    if(!shapeEyedropAllowed() || shapeEditor.batchMask || shapeEditor.nativePaint() || shapeEditor.current().value("kind","")=="pattern")return false;
    if(!selfTest && GetForegroundWindow()!=window){SetForegroundWindow(window);if(GetForegroundWindow()!=window)return false;}
    shapeEyedropEpoch=menuEpoch;shapeEyedropRevision=shapeEditor.draft.value("revision",std::string());shapeEyedropChannel=shapeEditor.channel;shapeEyedropStop=shapeEditor.stop;
    shapeEyedropActive=true;shapeEyedropDown=false;shapeHoverReady=false;
    hoverTip.reset();tipVisual.reset();renderedHit=pendingHit=-1;
    SetCursor(LoadCursorW(nullptr,IDC_CROSS));
    if(!selfTest){
        SetCapture(window);
        if(GetCapture()!=window || !SetPropW(window,screenSampleProperty,window)){stopShapeEyedrop();return false;}
    }
    return true;
}
static bool applyShapeSample(COLORREF pixel) {
    const bool valid=shapeEyedropActive && pixel!=CLR_INVALID && shapeEyedropAllowed() && menuEpoch==shapeEyedropEpoch &&
        shapeEditor.channel==shapeEyedropChannel && shapeEditor.stop==shapeEyedropStop && shapeEditor.draft.value("revision",std::string())==shapeEyedropRevision;
    stopShapeEyedrop();
    if(!valid)return false;
    auto& p=shapeEditor.current();const auto kind=p.value("kind","");
    if(kind!="solid" && kind!="gradient")return false;
    auto& rgb=kind=="solid"?p["rgb"]:p["stops"][shapeEditor.stop]["rgb"];
    rgb=Json::array({GetRValue(pixel),GetGValue(pixel),GetBValue(pixel)});p["enabled"]=true;shapeEditor.changed();
    return true;
}
static bool shapeEyedropMessage(UINT msg,WPARAM wp,LPARAM lp) {
    if(!shapeEyedropActive)return false;
    if(!shapeEyedropAllowed()){stopShapeEyedrop();return false;}
    if(msg==WM_SETCURSOR){SetCursor(LoadCursorW(nullptr,IDC_CROSS));return true;}
    if(msg==WM_CAPTURECHANGED || msg==WM_CANCELMODE){stopShapeEyedrop();return false;}
    // Esc must continue to the existing menu cancellation/rollback path.
    if(msg==WM_KEYDOWN && wp==VK_ESCAPE){stopShapeEyedrop();return false;}
    if(msg==WM_RBUTTONDOWN || msg==WM_RBUTTONUP){stopShapeEyedrop();return true;}
    if(msg==WM_LBUTTONDOWN){shapeEyedropDown=true;return true;}
    if(msg==WM_LBUTTONUP){
        if(shapeEyedropDown){
            POINT at{LONG(short(LOWORD(lp))),LONG(short(HIWORD(lp)))};ClientToScreen(window,&at);
            HDC screen=GetDC(nullptr);COLORREF pixel=screen?GetPixel(screen,at.x,at.y):CLR_INVALID;
            if(screen)ReleaseDC(nullptr,screen);
            if(!applyShapeSample(pixel))notice="屏幕取色失败，请重试";
        }
        return true;
    }
    // Mouse motion still updates ImGui so returning to the panel is immediate.
    return msg==WM_LBUTTONDBLCLK || msg==WM_MOUSEWHEEL || msg==WM_MOUSEHWHEEL;
}
static const char* shapeModeLabel(bool expanded) {
    if(shapeEditor.batchMask)return expanded?"收起羽化／密度":"展开羽化／密度";
    return expanded?"收起颜色／羽化":"展开颜色／羽化";
}
static void drawShapeModeSwitch() {
    shapeModeBounds={};shapeModeRows={};
    if(!shapeEditor.ready())return;
    auto* font=menuFont(MenuTextRole::Control);const float textSize=font->LegacySize;
    const float labelWidth=std::max(font->CalcTextSizeA(textSize,FLT_MAX,0,shapeModeLabel(false)).x,font->CalcTextSizeA(textSize,FLT_MAX,0,shapeModeLabel(true)).x);
    const auto row=holdRows[0];const float width=std::ceil(labelWidth+basicTextOffset()+figmaBasicRightPadding*scale),height=menuControlHeight*scale;
    const float x=std::round((row.left+row.right-width)*.5f),y=row.top-menuPanelGap*scale-height;
    shapeModeBounds={LONG(x),LONG(y),LONG(x+width),LONG(y+height)};shapeModeRows[0]=shapeModeBounds;
    const auto saved=ImGui::GetCursorScreenPos();ImGui::SetCursorScreenPos({x,y});
    ImGui::PushFont(font);
    ImGui::BeginChild("editor-mode",{width,height},ImGuiChildFlags_None,
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    auto* draw=ImGui::GetWindowDrawList();const float rounding=ImGui::GetStyle().FrameRounding;
    const auto mouse=ImGui::GetIO().MousePos;
    const bool hovered=mouse.x>=x && mouse.x<x+width && mouse.y>=y && mouse.y<y+height;
    const bool editing=ImGui::IsAnyItemActive() || ImGui::GetIO().WantTextInput || ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1);
    const bool canSwitch=!pieAnimating && !holdFinished && !editing && !shapeEyedropActive && !gradientLaunching;
    const bool entered=hovered && shapeHoverReady;
    if(!hovered)shapeHoverReady=true;
    else if(editing || shapeEyedropActive)shapeHoverReady=false;
    // One transition per entry. Holding over the icon cannot oscillate, and
    // an entry during dragging/input/sampling is consumed rather than deferred.
    if(canSwitch && entered){
        const bool close=shapePanelOpen;shapeHoverReady=false;
        if(close && shapeEditor.dirty && !shapeEditor.livePreview){commitShapeDraft(true);shapeReturning=true;updateBusy(true);pinned=true;}
        if(close && maskPreviewStarted){emitMaskPreview(true,true);shapeReturning=true;updateBusy(true);pinned=true;}
        shapePanelOpen=!close;shapeEditor.gradientDragging=false;
        if(close){shapeHoverBounds={};shapeHoverReady=false;}
        shapeEditor.copyBounds=shapeEditor.gradientBounds=shapeEditor.featherBounds=shapeEditor.densityBounds=shapeEditor.hexBounds={};
        shapeEditor.rgbBounds={};shapeTipCandidate=-1;hoverTip.reset();tipVisual.reset();renderedHit=pendingHit=-1;
    }
    const ImVec2 a={x,y},b={x+width,y+height};
    const auto surface=tint(shapePanelOpen?ImGuiCol_ButtonActive:hovered?ImGuiCol_ButtonHovered:ImGuiCol_Button);
    draw->AddRectFilled(a,b,surface,rounding);draw->AddRect(a,b,tint(ImGuiCol_Border),rounding,0,scale);
    const float iconSize=figmaBasicIconSize*scale;
    drawStyledMenuIcon(draw,shapePanelOpen?33:32,{x+figmaBasicLeftPadding*scale,y+(height-iconSize)*.5f},iconSize,tint(ImGuiCol_Text),surface);
    draw->AddText(font,textSize,{x+basicTextOffset(),std::round(y+(height-textSize)*.5f)},tint(ImGuiCol_Text),shapeModeLabel(shapePanelOpen));
    shapeTipRows[shapeModeTipBase]=shapeModeBounds;
    if(hovered && canSwitch)shapeTipCandidate=200+shapeModeTipBase;
    ImGui::Dummy({0,0});
    ImGui::EndChild();ImGui::PopFont();ImGui::SetCursorScreenPos(saved);
}
static void drawShapeEditor() {
    shapeTipCandidate=-1;shapeTipRows={};
    shapeEditor.bounds=shapeEditor.maskBounds=shapeEditor.copyBounds=shapeEditor.gradientBounds=shapeEditor.featherBounds=shapeEditor.densityBounds=shapeEditor.hexBounds={};shapeEditor.rgbBounds={};
    drawShapeModeSwitch();
    if(shapeEyedropActive && !shapeEyedropAllowed())stopShapeEyedrop();
    if(shapeEyedropActive)SetCursor(LoadCursorW(nullptr,IDC_CROSS));
    if(!shapePanelVisible())return;
    const auto windowPos=ImGui::GetWindowPos(),windowSize=ImGui::GetWindowSize();
    const float rightStart=pieCenter.x+65*scale;
    const float available=shapePanelRight?windowPos.x+windowSize.x-rightStart-16*scale:
        shapeModeBounds.left-windowPos.x-(menuPanelGap+16)*scale;
    if(available<140*scale)return;
    auto* panelFont=menuFont(MenuTextRole::Control);
    const float gap=menuPanelGap*scale;
    auto textWidth=[&](const char* value){return panelFont->CalcTextSizeA(panelFont->LegacySize,FLT_MAX,0,value).x;};
    const float rgbMinimum=std::max({textWidth("R:255"),textWidth("G:255"),textWidth("B:255")})+2*menuControlPadding*scale;
    float hexGlyph=0;for(char ch:std::string("0123456789ABCDEF")){char text[]={ch,0};hexGlyph=std::max(hexGlyph,textWidth(text));}
    const float hexMinimum=textWidth("#")+6*hexGlyph+(2*menuControlPadding+figmaDisplayIconWidth+2)*scale;
    const float valuesWidth=3*rgbMinimum+gap+hexMinimum;
    const float width=std::floor(std::min(shapeEditor.batchMask?menuPanelWidth*scale:std::max(menuPanelWidth*scale,valuesWidth+2*(menuControlPadding+1)*scale),available));
    const float row=menuControlHeight*scale+gap;
    const auto kind=shapeEditor.batchMask?std::string("mask"):shapeEditor.current().value("kind",std::string("solid"));
    const float iconRow=figmaDisplayWidgetHeight*scale+gap;
    const float colorSquare=perceptualSquareSide(width-2*menuControlPadding*scale);
    const float cell=figmaDisplayIconWidth*scale;
    const bool wrapTools=width-2*menuControlPadding*scale<(shapeEditor.vector?6:4)*cell+gap;
    const bool wrapActions=shapeEditor.vector && kind!="pattern" && width-2*menuControlPadding*scale<6*cell+3*gap;
    const bool wrapValues=width-2*menuControlPadding*scale<valuesWidth;
    const bool nativeChannels=!shapeEditor.batchMask && shapeEditor.nativePaint();
    const auto& currentColor=shapeEditor.batchMask?shapeEditor.draft:kind=="gradient"?shapeEditor.current()["stops"][std::clamp(shapeEditor.stop,0,int(shapeEditor.current()["stops"].size())-1)]:shapeEditor.current();
    const int nativeRows=currentColor.contains("native")?int(currentColor["native"]["values"].size()):1;
    const float content=2*menuControlPadding*scale+iconRow*(wrapTools?2:1)+(shapeEditor.vector && shapeEditor.channel?row:0)+
        (kind=="pattern"?row:(nativeChannels?(nativeRows+2)*row:colorSquare+(wrapValues?2:1)*row)+(kind=="gradient"?menuGradientEditorHeight*scale+gap:0))+iconRow*(wrapActions?2:1);
    const float maskHeight=shapeEditor.hasMask()?2*menuControlPadding*scale+2*row+(shapeEditor.batchMask?iconRow:0):0;
    const float separation=shapeEditor.hasMask() && !shapeEditor.batchMask?gap:0;
    const float height=shapeEditor.batchMask?0:std::min(content,windowSize.y-24*scale-maskHeight-separation);
    // Reserve vertical placement for every editable channel/type. Switching
    // stroke to fill or gradient to solid must not move the header down and
    // turn a stationary click into a hover exit/implicit confirmation.
    const bool layoutWrapActions=shapeEditor.vector && width-2*menuControlPadding*scale<6*cell+3*gap;
    const float largestContent=!shapeEditor.batchMask?
        2*menuControlPadding*scale+iconRow*(wrapTools?2:1)+(shapeEditor.vector?row:0)+(nativeChannels?6*row:colorSquare+(wrapValues?2:1)*row)+
        menuGradientEditorHeight*scale+gap+iconRow*(layoutWrapActions?2:1):content;
    const float placementHeight=shapeEditor.batchMask?maskHeight:
        std::min(largestContent,windowSize.y-24*scale-maskHeight-separation)+separation+maskHeight;
    const float panelX=shapePanelRight?rightStart:std::max(windowPos.x+16*scale,shapeModeBounds.left-menuPanelGap*scale-width);
    ImVec2 at={panelX,std::max(windowPos.y+12*scale,float(shapeModeBounds.top))};
    at.y=std::max(windowPos.y+12*scale,std::min(at.y,windowPos.y+windowSize.y-placementHeight-12*scale));

    at.x=std::round(at.x);at.y=std::round(at.y);
    if(!shapeEditor.batchMask)shapeEditor.bounds={LONG(at.x),LONG(at.y),LONG(at.x+width),LONG(at.y+height)};
    const auto saved=ImGui::GetCursorScreenPos();ImGui::SetCursorScreenPos(at);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,ImGui::GetStyle().FrameRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{menuControlPadding*scale,menuControlPadding*scale});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{gap,gap});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing,{gap,gap});
    const float controlTextSize=panelFont->LegacySize;
    ImGui::PushFont(panelFont);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{menuControlTextInset*scale,std::max(0.f,(menuControlHeight*scale-controlTextSize)*.5f)});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,scale);
    float inner=width-2*menuControlPadding*scale;
    if(!shapeEditor.batchMask) {
    ImGui::BeginChild("shape-color",{width,height},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetWindowFontScale(1.f);
    if(selfTest && std::abs(ImGui::GetFontSize()-controlTextSize)>.01f)
        throw std::runtime_error("Shape controls must share the menu panel body font");
    ImGui::BeginDisabled(pieAnimating || shapeEyedropActive || gradientLaunching);
    inner=ImGui::GetContentRegionAvail().x;
    if(!shapeEditor.vector)shapeEditor.channel=0;
    const auto toolsAt=ImGui::GetCursorScreenPos();const float channelWidth=(shapeEditor.vector?2:1)*figmaDisplayIconWidth*scale;
    if(shapeSegment("channel",shapeEditor.vector?std::vector<std::string>{"填充","描边"}:std::vector<std::string>{"填充"},shapeEditor.channel,channelWidth,false,shapeEditor.vector?std::vector<int>{11,12}:std::vector<int>{11},0)){shapeEditor.stop=0;shapeEditor.gradientDragging=false;}
    auto& p=shapeEditor.current();std::string paintKind=p.at("kind");int mode=paintKind=="solid"?0:paintKind=="gradient"?1:2;
    if(shapeEditor.vector && !ImGui::IsAnyItemActive() && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Tab,false) && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyAlt){
        mode=(mode+(ImGui::GetIO().KeyShift?2:1))%3;shapeEditor.mode(mode);
        ImGui::SetKeyOwner(ImGuiKey_Tab,ImGui::GetID("shape-kind"),ImGuiInputFlags_LockThisFrame);
    }
    const float kindWidth=(shapeEditor.vector?4:3)*cell;
    ImGui::SetCursorScreenPos({wrapTools?toolsAt.x:toolsAt.x+inner-kindWidth,toolsAt.y+(wrapTools?iconRow:0)});
    int paintChoice=p.value("enabled",true)?mode+1:0;
    if(shapeEditor.vector){
        if(shapeSegment("kind",{"无","纯色","渐变","图案"},paintChoice,kindWidth,false,{23,13,14,15},-1,{}, {5,2,3,4})){
            if(paintChoice){if(paintChoice!=mode+1)shapeEditor.mode(paintChoice-1);p["enabled"]=true;}
            else p["enabled"]=false;
            shapeEditor.changed();
        }
    } else if(shapeSegment("kind",{"纯色","渐变","图案"},mode,kindWidth,true,{13,14,15},2))shapeEditor.mode(mode);
    ImGui::SetCursorScreenPos({toolsAt.x,toolsAt.y+iconRow*(wrapTools?2:1)});
    paintKind=p.at("kind");
    if(shapeEditor.channel){float strokeWidth=p.value("width",1.f);ImGui::SetNextItemWidth(std::max(55*scale,inner-18*scale));
        if(ImGui::DragFloat("px",&strokeWidth,.25f,0.f,1000.f,"%.2f")){p["width"]=strokeWidth;shapeEditor.changed();}shapeItemTip(6);}
    if(paintKind=="pattern"){
        std::string name=p.value("name","");ImGui::SetNextItemWidth(inner);
        if(ImGui::BeginCombo("##pattern",name.empty()?"选择图案":name.c_str())){
            for(const auto& item:shapeEditor.patterns){const std::string value=item;
                if(ImGui::Selectable(value.c_str(),value==name)){p["name"]=value;p["enabled"]=true;shapeEditor.changed();}}
            ImGui::EndCombo();
        }
        if(shapeEditor.patterns.empty())ImGui::TextWrapped("没有已加载的图案预设");
    } else {
        if(paintKind=="gradient"){
            const auto gradientAt=ImGui::GetCursorScreenPos();
            const float tools=shapeEditor.gradientEditor?2*(cell+gap):cell+gap;
            drawGradientBar(inner-tools);
            if(shapeEditor.gradientEditor){
                const auto nextAt=ImGui::GetCursorScreenPos();
                ImGui::SetCursorScreenPos({gradientAt.x+inner-2*cell-gap,gradientAt.y});
                const bool confirm=shapeEditor.dirty || maskPreviewStarted;
                const bool ready=(canExecute() || maskPreviewStarted && shapeEditor.ready()) && shapeEditor.valid() && !ImGui::GetIO().WantTextInput;
                if(shapeIconButton("gradient-editor",14,confirm?shapeGradientConfirmTip:shapeGradientEditorTip,cell,ready,false,figmaDisplayWidgetHeight)){
                    gradientLaunching=true;
                    send("gradient.open",{{"channel",shapeEditor.channel?"stroke":"fill"},
                        {"draft",confirm?shapeEditor.draft:Json()},{"preview",maskPreviewStarted}});
                }
                ImGui::SetCursorScreenPos(nextAt);
            }
            const auto nextAt=ImGui::GetCursorScreenPos();ImGui::SetCursorScreenPos({gradientAt.x+inner-cell,gradientAt.y});
            if(shapeIconButton("gradient-method",31,22,cell,true,false,figmaDisplayWidgetHeight)){
                const auto method=p.value("method",std::string("classic"));p["method"]=method=="perceptual"?"linear":method=="linear"?"classic":"perceptual";shapeEditor.methodExplicit[shapeEditor.channel]=true;shapeEditor.changed();}
            ImGui::SetCursorScreenPos(nextAt);
        }
        if(shapeEditor.nativePaint()){
            Json& color=paintKind=="solid"?p:p["stops"][shapeEditor.stop];
            const auto model=color.contains("native")?color["native"]["model"].get<std::string>():std::string();
            {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("%s",model=="cmyk"?"CMYK 原生通道":model=="lab"?"Lab 原生通道":model=="gray"?"灰度原生通道":"RGB 原始颜色");}
            if(shapeRgbInputs(color.contains("native")?color["native"]["values"]:color["rgb"],inner,model)){p["enabled"]=true;shapeEditor.changed();}
            {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("确认后更新PS；颜色以画布为准。");}
        } else {
        Json& c=paintKind=="solid"?p["rgb"]:p["stops"][shapeEditor.stop]["rgb"];
        const float reserve=(wrapValues?ImGui::GetFrameHeightWithSpacing():0)+iconRow*(wrapActions?2:1);
        const float pickerWidth=std::min(inner,std::max(120*scale,ImGui::GetContentRegionAvail().y-reserve+(2*menuPanelGap+menuHueBarWidth+menuHarmonyLaneWidth)*scale));
        ImGui::SetNextItemWidth(pickerWidth);
        if(drawShapePicker(c,pickerWidth)){p["enabled"]=true;shapeEditor.changed();}
        const std::string hexKey=std::to_string(shapeEditor.channel)+paintKind+std::to_string(shapeEditor.stop);
        const ImGuiID hexId=ImGui::GetID("##hex");
        if(shapeEditor.hexKey!=hexKey){if(ImGui::GetActiveID()==hexId)ImGui::ClearActiveID();shapeEditor.hexKey=hexKey;}
        if(ImGui::GetActiveID()!=hexId){const auto value=shapeHex(c).substr(1);std::copy(value.begin(),value.end(),shapeEditor.hexInput);shapeEditor.hexInput[6]=0;}
        const auto valuesAt=ImGui::GetCursorScreenPos();const float rgbWidth=wrapValues?inner:inner-gap-hexMinimum;
        if(shapeRgbInputs(c,rgbWidth)){p["enabled"]=true;shapeEditor.changed();const auto hex=shapeHex(c).substr(1);std::copy(hex.begin(),hex.end(),shapeEditor.hexInput);shapeEditor.hexInput[6]=0;}
        ImGui::SetCursorScreenPos(wrapValues?ImVec2{valuesAt.x,valuesAt.y+row}:ImVec2{valuesAt.x+rgbWidth+gap,valuesAt.y});
        shapeHexInput(c,wrapValues?inner:hexMinimum);
        ImGui::SetCursorScreenPos({valuesAt.x,valuesAt.y+row*(wrapValues?2:1)});
        }
    }
    const auto actionsAt=ImGui::GetCursorScreenPos();
    if(paintKind!="pattern" && !shapeEditor.nativePaint()){
        int quickColor=-1;
        if(shapeSegment("quick-color",{"黑色","白色"},quickColor,2*cell,false,{19,20},8)){
            auto& c=paintKind=="solid"?p["rgb"]:p["stops"][shapeEditor.stop]["rgb"];
            c=quickColor?Json::array({255,255,255}):Json::array({0,0,0});p["enabled"]=true;shapeEditor.changed();}
    }
    if(paintKind!="pattern" && !shapeEditor.nativePaint()){
        ImGui::SetCursorScreenPos({actionsAt.x+2*cell+gap,actionsAt.y});
        if(shapeIconButton("##screen-eyedrop",34,shapeEyedropTip,cell,true,false,figmaDisplayWidgetHeight) && !beginShapeEyedrop())notice="无法开始屏幕取色，请保持菜单在前台";
    }
    if(shapeEditor.vector)ImGui::SetCursorScreenPos({actionsAt.x+(paintKind=="pattern"?0:wrapActions?inner-cell:3*cell+2*gap),actionsAt.y});
    if(shapeEditor.vector && shapeIconButton("##swap-paint",18,10,cell,true,false,figmaDisplayWidgetHeight)){
        const double widthValue=shapeEditor.draft["stroke"].value("width",1.);
        Json fill=shapeEditor.draft["fill"],stroke=shapeEditor.draft["stroke"];stroke.erase("width");
        fill["width"]=widthValue;shapeEditor.draft["fill"]=stroke;shapeEditor.draft["stroke"]=fill;
        shapeEditor.modes={};for(int i=0;i<2;++i){const auto& v=shapeEditor.draft[i?"stroke":"fill"];shapeEditor.modes[i]=Json::object();shapeEditor.modes[i][v.at("kind").get<std::string>()]=v;}
        shapeEditor.stop=0;shapeEditor.changed();
    }
    if(shapeEditor.vector){ImGui::SetCursorScreenPos({actionsAt.x+inner-2*cell,actionsAt.y+(wrapActions?iconRow:0)});shapePaintToolbar(inner);}
    ImGui::SetCursorScreenPos({actionsAt.x,actionsAt.y+iconRow*(wrapActions?2:1)});
    ImGui::Dummy({0,0});
    ImGui::EndDisabled();ImGui::EndChild();
    }
    if(shapeEditor.hasMask()) {
        const ImVec2 maskAt={at.x,at.y+height+separation};
        shapeEditor.maskBounds={LONG(maskAt.x),LONG(maskAt.y),LONG(maskAt.x+width),LONG(maskAt.y+maskHeight)};
        ImGui::SetCursorScreenPos(maskAt);
        ImGui::BeginChild("shape-mask",{width,maskHeight},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::BeginDisabled(pieAnimating || shapeEyedropActive || gradientLaunching);
        if(shapeEditor.batchMask)shapePaintToolbar(inner);
        inner=ImGui::GetContentRegionAvail().x;
        auto slider=[&](const char* label,const char* id,const char* key,float maximum,const char* format,ImGuiSliderFlags flags,RECT& bounds){
            float value=shapeEditor.draft["mask"].contains(key)?shapeEditor.draft["mask"][key].get<float>():shapeEditor.maskValues[key].get<float>();
            const auto fieldAt=ImGui::GetCursorScreenPos();const auto fieldId=ImGui::GetID(id);
            shapeEditor.maskInputIDs[std::string(key)=="feather"?0:1]=fieldId;
            const bool enter=ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(fieldAt,{fieldAt.x+inner,fieldAt.y+menuControlHeight*scale}) && !pieAnimating;
            if(enter){GImGui->NavActivateId=fieldId;GImGui->NavActivateFlags=ImGuiActivateFlags_PreferInput;}
            if(shapeEditor.batchMask && !shapeEditor.draft["mask"].contains(key) && shapeEditor.maskMixed.value(key,false) && !enter && !ImGui::TempInputIsActive(fieldId))format="混合";ImGui::SetNextItemWidth(inner);
            if(ImGui::SliderFloat(id,&value,0.f,maximum,format,flags|ImGuiSliderFlags_AlwaysClamp)){
                shapeEditor.draft["mask"][key]=value;shapeEditor.changed();}
            shapeItemTip(std::string(key)=="feather"?13:14);
            const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();bounds={LONG(a.x),LONG(a.y),LONG(b.x),LONG(b.y)};
        };
        slider("羽化","##mask-feather","feather",1000.f,"%.2f px",ImGuiSliderFlags_Logarithmic,shapeEditor.featherBounds);
        slider("密度","##mask-density","density",100.f,"%.1f%%",ImGuiSliderFlags_None,shapeEditor.densityBounds);
        ImGui::EndDisabled();ImGui::EndChild();
    }
    shapeHoverBounds=shapeEditor.batchMask?shapeEditor.maskBounds:shapeEditor.bounds;
    if(shapeEditor.hasMask())shapeHoverBounds.bottom=shapeEditor.maskBounds.bottom;
    // The envelope/bridge retain hover only; separate panel rectangles remain the only commit hits.
    // No writes for channel/stop selection or copying; one latest draft per frame.
    if(shapeEditor.livePreview && !pieAnimating && !gradientLaunching && shapeEditor.valid() && (shapeEditor.dirty || maskPreviewStarted) &&
       shapeEditor.draft!=shapeEditor.lastPreview)emitMaskPreview();
    ImGui::PopStyleVar(6);ImGui::PopFont();ImGui::PopStyleColor();ImGui::SetCursorScreenPos(saved);
}
