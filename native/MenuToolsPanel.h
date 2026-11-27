#include "MenuTextGroups.h"
#include "FontSampleRaster.h"
#include "FontSampleWork.h"
#include "CanvasTransparency.h"
#include "CanvasGlyphTarget.h"
#include "CanvasViewTransform.h"
#include "TextStyleDraft.h"
#include <optional>
#include <set>

struct MenuToolsPanel {
    bool active=false,opening=false,ready=false,pending=false,focus=false,vectorObject=false;
    std::string mode,error,source,sourceDisplay,revision,layerName;
    char name[801]{},grouping[4001]{};
    char draft[80001]{},find[801]{},replacement[2001]{},separator[801]{},joiner[801]=" ";
    char listCustom[9]{},listStart[9]="1";
    inline static constexpr const char* listKinds[]={"bullet","numbered","remove"},*listMarkers[]={"auto","disc","circle","square","dash","check","custom"},*listStyles[]={"decimal","right-paren","parens","lower-alpha","upper-alpha","lower-roman","upper-roman","hierarchical","mixed"};
    Json draftList=nullptr;bool draftListChanged=false;
    Json sourceStyles=nullptr;
    struct FontFace {std::string family,style,id;};
    std::vector<FontFace> fonts;
    std::vector<std::pair<std::string,std::string>> families;
    struct FontSample {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> image;
        FontSampleRaster::Result status=FontSampleRaster::Result::unavailable;
    };
    FontSampleRaster fontRaster;
    std::map<std::string,FontSample> fontSamples;
    std::wstring fontSampleText;
    ID3D11Device* fontSampleDevice=nullptr;
    int fontSampleFrame=-1,fontSampleDrawn=0,fontSampleWidth=0,fontSampleHeight=0;
    float fontSampleEm=0;
    uint64_t fontSampleScope=0;
    ImGuiTextFilter fontFilter;
    bool fontsAvailable=false,fontsRequested=false;
    std::string fontCandidate,fontPreview;
    double fontHoverSince=0;
    bool scopeSupported=false,paragraphIndentsSupported=false,paragraphAlignmentSupported=false,extendedTypography=false,managedList=false;
    bool canvasAvailable=false,proofAvailable=false,objectProofAvailable=false,proofCurrent=false;
    bool isolatedProofAvailable=false;
    bool canvasTextFrame=false,canvasTextActive=false,canvasWrap=true,canvasPanning=false,boxShiftSupported=false;
    enum class CanvasState {idle,waiting,ready,limited,failed,stale};
    CanvasState canvasState=CanvasState::idle;
    std::string canvasMessage;
    bool fallbackDraftActive=false,firstEditable=false,imeComposing=false;
    bool autoFocusPending=false;
    ULONGLONG openedAt=0;
    std::array<float,4> textBox{};
    std::array<float,2> sourceBoxSize{},draftBoxSize{};
    float draftBoxOffset=0;
    Json objectTextFrame=nullptr;
    int resizeHandle=0;
    ImVec2 resizeGestureStart{};
    std::array<float,2> resizeStartSize{};
    float resizeStartOffset=0;
    bool trackedResize=false;
    float canvasEm=0;
    float canvasSourceWidth=0,canvasSourceHeight=0;
    ImU32 canvasInk=0;
    std::string proofAttempt;
    int maxProofScale=1,wantedProofScale=1,sharpAttempt=1,sharpProofScale=1,pendingProofScale=1;
    ImVec2 proofNativeSize{};
    double proofZoomChanged=0;
    double canvasInteraction=0;
    bool tableObject=false;
    std::vector<std::vector<std::string>> cells;
    std::array<float,3> tableCanvas{};
    ImVec2 tablePosition{.5f,.5f};
    char tableWidth[7]{},tableHeight[7]{};
    int tableAlign=0,tableCell=-1,tableLayoutGeneration=-1;
    bool tablePlaced=false,tableFocus=false,tableEditing=false;
    std::vector<FontSampleRaster::DraftLayout> tableGlyphs;
    inline static constexpr const char* tableAlignKeys[]={"left","center","right"};
    float canvasZoom=1,canvasAngle=0;
    ImVec2 canvasPan{},panGestureStart{},panBefore{};
    bool canvasZoomMode=false,canvasZoomDragging=false;
    ImVec2 canvasZoomStart{};
    int styleScope=0,leadingMode=0,paragraphAlign=0,backgroundMode=0;
    inline static constexpr const char* alignmentKeys[]={"left","center","right","justifyLeft","justifyCenter","justifyRight","justifyAll"};
    inline static constexpr const char* enumKeys[]={"autoKern","fontCaps","baseline","underline","strikethrough","figureStyle",
        "kinsokuSetName","mojiKumiName","burasagari","preferredKinsokuOrder"};
    inline static constexpr const char* enumValues[][6]={
        {"","metricsKern","opticalKern",""},{"","normal","allCaps","smallCaps"},{"","normal","superScript","subScript"},
        {"","underlineOff","underlineOnLeftInVertical",""},{"","strikethroughOff","xHeightStrikethroughOn",""},
        {"","normal","proportionalLining","tabularLining","proportionalOldStyle","tabularOldStyle"},
        {"","None","Soft","Hard"},
        {"","Photoshop6MojiKumiNone","Photoshop6MojiKumiSet1","Photoshop6MojiKumiSet2","Photoshop6MojiKumiSet3","Photoshop6MojiKumiSet4"},
        {"","burasagariNone","burasagariStandard","burasagariStrong"},
        {"","pushIn","pushOutFirst","pushOut"}
    };
    inline static constexpr const char* switchKeys[]={"syntheticBold","syntheticItalic","ligature","noBreak","hyphenate",
        "contextualLigatures","stylisticAlternates","swash","titling","ornaments","fractions","ordinals"};
    int styleEnums[IM_ARRAYSIZE(enumKeys)]{},styleSwitches[IM_ARRAYSIZE(switchKeys)]{};
    bool advancedTypography=false,eastAsianParagraphSupported=false,textRgb=false,textColorChanged=false;
    bool openTypeSupported=false,kerningSupported=false,scopeExplicit=false;
    Json sourceKerns=Json::array(),sourceFontRuns=Json::array();
    std::map<int,Json> kerningEdits;
    std::string kerningAnchor;
    int kerningFieldAt=-1;
    bool kerningFieldInvalid=false;
    char kerningField[12]{};
    float textRgbDraft[3]={0,0,0},textRgbOriginal[3]={0,0,0};
    float backgroundRgb[3]={1.f,1.f,1.f};
    inline static constexpr const char* styleKeys[]={"fontPostScriptName","size","leading","tracking","baselineShift","horizontalScale","verticalScale","spaceBefore","spaceAfter","startIndent","endIndent","firstLineIndent"};
    inline static constexpr const char* styleLabels[]={"字体 PostScript 名称","字号 (pt)","行距 (pt)","字距 (1/1000 em)","基线偏移 (pt)","水平缩放 (%)","垂直缩放 (%)","段前距 (pt)","段后距 (pt)","左缩进 (pt)","右缩进 (pt)","首行缩进 (pt)"};
    static constexpr int leadingField=2;
    char styleValues[IM_ARRAYSIZE(styleKeys)][256]{};
    std::array<int,2> styleBytes{-1,-1};
    bool styleEndOfText=false;
    std::string styleAnchor;
    bool scopedStyles=false;
    TextStyleDraft styleDraft;
    std::array<int,4> editorStyleRange{-1,-1,-1,-1};
    std::map<std::string,Json> nextCharacterStyles;
    int nextCharacterAt=-1;
    std::map<std::string,std::array<char,80>> compositionFields;
    std::set<std::string> invalidComposition;
    Json styleLibrary=Json::object();
    char styleName[193]{};
    int stylePresetKind=0;
    std::string stylePresetError;
    struct DraftState {
        std::string text;
        TextStyleDraft styles;
        std::map<int,Json> kerning;
        std::array<float,2> box;
        float boxOffset;
        std::map<std::string,Json> typingStyles;
        int typingAt;
        Json list;
        bool listChanged;
    };
    std::vector<DraftState> undoDrafts,redoDrafts;
    DraftState draftState(const char* text)const {
        return {text,styleDraft,kerningEdits,draftBoxSize,draftBoxOffset,nextCharacterStyles,nextCharacterAt,draftList,draftListChanged};
    }
    void checkpointDraft(const char* prior=nullptr) {
        if(!scopedStyles)return;
        if(undoDrafts.size()>=24)undoDrafts.erase(undoDrafts.begin());
        undoDrafts.push_back(draftState(prior?prior:draft));redoDrafts.clear();
    }
    void restoreDraft(bool redo) {
        auto& source=redo?redoDrafts:undoDrafts;
        auto& target=redo?undoDrafts:redoDrafts;
        if(source.empty())return;
        target.push_back(draftState(draft));
        auto state=std::move(source.back());source.pop_back();
        std::snprintf(draft,sizeof(draft),"%s",state.text.c_str());
        styleAnchor=kerningAnchor=draft;
        styleDraft=std::move(state.styles);kerningEdits=std::move(state.kerning);draftBoxSize=state.box;draftBoxOffset=state.boxOffset;
        nextCharacterStyles=std::move(state.typingStyles);nextCharacterAt=state.typingAt;
        draftList=std::move(state.list);draftListChanged=state.listChanged;
        selectionStart=selectionEnd=cursorPos=std::min(cursorPos,int(state.text.size()));
        while(cursorPos>0 && (static_cast<unsigned char>(draft[cursorPos])&0xc0)==0x80)--cursorPos;
        selectionStart=selectionEnd=lastSelectionStart=lastSelectionEnd=cursorPos;
        ++draftBufferId;restoreDraftFocus=canvasTextActive || fallbackDraftActive;
        editorStyleRange={-1,-1,-1,-1};kerningFieldAt=-1;kerningFieldInvalid=false;
        changedText();
    }
    bool viewDirty=false;
    double lastViewChange=0;

    bool listParagraphsManaged()const{return draftListChanged?draftList.is_object():managedList;}
    void loadViewSettings(const Json& value) {
        backgroundMode=value.value("background",0);
        const auto rgb=value.value("rgb",std::array<float,3>{1,1,1});
        std::copy(rgb.begin(),rgb.end(),backgroundRgb);
        canvasZoom=value.value("zoom",1.f);canvasPan={};canvasAngle=0;
        canvasZoomMode=canvasZoomDragging=false;
        const int scope=value.value("scope",0);
        scopeExplicit=scope!=0;styleScope=std::max(0,scope-1);
        editorStyleRange={-1,-1,-1,-1};
        viewDirty=false;
    }
    void changedView(){viewDirty=true;lastViewChange=ImGui::GetTime();}
    void saveViewSettings(bool reset=false) {
        if(!viewDirty && !reset)return;
        const Json value=reset?Json(nullptr):Json{{"background",backgroundMode},
            {"rgb",std::array<float,3>{backgroundRgb[0],backgroundRgb[1],backgroundRgb[2]}},
            {"zoom",canvasZoom},{"scope",scopeExplicit?styleScope+1:0}};
        sendMenuTool("menu.tools.preferences",Json{{"view_settings",value}});
        viewDirty=false;
    }

    static bool eastAsianParagraphProperty(std::string_view key) {
        return key=="kinsokuSetName" || key=="mojiKumiName" || key=="burasagari" || key=="preferredKinsokuOrder";
    }
    static bool paragraphProperty(std::string_view key) {
        return key=="align" || key.rfind("hyphenat",0)==0 || key.rfind("justify",0)==0 ||
            key=="hangingRoman" || key=="textEveryLineComposer" || key=="spaceBefore" || key=="spaceAfter" ||
            key=="startIndent" || key=="endIndent" || key=="firstLineIndent" || eastAsianParagraphProperty(key);
    }
    std::array<int,2> activeStyleRange(bool paragraph)const {
        const int length=int(std::strlen(draft));
        if(scopeExplicit && styleScope==0)return {0,length+1};
        const bool selected=selectionStart!=selectionEnd;
        auto range=selected || (scopeExplicit && styleScope==1)?selectedStyleBytes(1):
            std::array<int,2>{std::clamp(cursorPos,0,length),std::clamp(cursorPos,0,length)};
        if(paragraph || (scopeExplicit && styleScope==2)){
            if(scopeExplicit && styleScope==2)range={std::clamp(cursorPos,0,length),std::clamp(cursorPos,0,length)};
            int first=range[0],last=range[1]>first?range[1]-1:first;
            while(first>0 && draft[first-1]!='\n')--first;
            while(last<length && draft[last]!='\n')++last;
            return {first,last+1};
        }
        return range;
    }

    static std::string displayStyle(std::string_view key,const Json& value) {
        if(value.is_boolean()){
            if(key=="textEveryLineComposer")return value.get<bool>()?"整段":"逐行";
            return value.get<bool>()?"开启":"关闭";
        }
        if(key.rfind("justify",0)==0 && value.is_array()){
            char digits[80]{};
            std::snprintf(digits,sizeof(digits),"%.6g, %.6g, %.6g",value.at(0).get<double>(),value.at(1).get<double>(),value.at(2).get<double>());
            return digits;
        }
        if(key=="color" && value.is_array()){
            char hex[8]{};std::snprintf(hex,sizeof(hex),"#%02X%02X%02X",value.at(0).get<unsigned>(),
                value.at(1).get<unsigned>(),value.at(2).get<unsigned>());return hex;
        }
        if(key=="leading" && value=="auto")return "自动";
        if(key=="align"){
            const char* labels[]={"左对齐","居中","右对齐","两端对齐，末行左","两端对齐，末行居中","两端对齐，末行右","全部两端对齐"};
            for(int i=0;i<IM_ARRAYSIZE(alignmentKeys);++i)if(value==alignmentKeys[i])return labels[i];
        }
        const char* aliases[][6]={{"","度量","视觉"},{"","正常","大写","小型大写"},
            {"","正常","上标","下标"},{"","关闭","开启"},{"","关闭","开启"},
            {"","默认","比例等高","等宽等高","比例旧式","等宽旧式"},
            {"","关闭","JIS 弱","JIS 最大"},{"","关闭","组合 1","组合 2","组合 3","组合 4"},
            {"","关闭","常规","强制"},{"","优先挤入","优先推出","仅推出"}};
        static_assert(IM_ARRAYSIZE(enumKeys)==IM_ARRAYSIZE(aliases));
        for(int i=0;i<IM_ARRAYSIZE(enumKeys);++i)if(key==enumKeys[i]){
            for(int j=1;j<IM_ARRAYSIZE(enumValues[i]) && enumValues[i][j] && enumValues[i][j][0];++j)
                if(value==enumValues[i][j])return aliases[i][j];
        }
        return value.is_string()?value.get<std::string>():value.dump();
    }
    std::string currentStyle(const char* key)const {
        if(!scopedStyles)return sourceStyles.value(key,std::string("沿用原值"));
        const auto range=activeStyleRange(paragraphProperty(key));
        if(range[0]==range[1] && nextCharacterAt==range[0]){
            const auto it=nextCharacterStyles.find(key);
            if(it!=nextCharacterStyles.end())return displayStyle(key,it->second);
        }
        return styleDraft.current(key,range,displayStyle);
    }
    Json currentStyleOverride(const char* key)const {
        const auto range=activeStyleRange(paragraphProperty(key));
        if(range[0]!=range[1])return styleDraft.overrideFor(key,range);
        const auto it=nextCharacterStyles.find(key);
        return nextCharacterAt==range[0] && it!=nextCharacterStyles.end()?it->second:Json(nullptr);
    }
    Json captureStylePreset(bool paragraph)const {
        auto fields=Json::object();
        for(const auto& [key,runs]:styleDraft.source){
            if(paragraphProperty(key)!=paragraph || (eastAsianParagraphProperty(key) && !eastAsianParagraphSupported))continue;
            auto value=currentStyleOverride(key.c_str());
            if(!value.is_null()){fields[key]=std::move(value);continue;}
            const auto shown=currentStyle(key.c_str());
            if(shown=="继承" || shown=="混合")continue;
            if(key=="fontPostScriptName")value=shown;
            else if(key=="leading" && shown=="自动")value="auto";
            else if(key=="color" && shown.size()==7 && shown[0]=='#'){
                unsigned int rgb=0;
                if(std::sscanf(shown.c_str(),"#%x",&rgb)==1)value=Json::array({(rgb>>16)&255,(rgb>>8)&255,rgb&255});
            }else if(key=="textEveryLineComposer")value=shown=="整段";
            else if(key.rfind("justify",0)==0){
                float a=0,b=0,c=0;
                if(std::sscanf(shown.c_str()," %f , %f , %f",&a,&b,&c)==3)value=Json::array({a,b,c});
            }else if(key=="align"){
                for(const char* name:alignmentKeys)if(displayStyle(key,name)==shown)value=name;
            }else{
                bool enumeration=false;
                for(int i=0;i<IM_ARRAYSIZE(enumKeys);++i)if(key==enumKeys[i]){
                    enumeration=true;
                    for(int j=1;j<IM_ARRAYSIZE(enumValues[i]) && enumValues[i][j] && enumValues[i][j][0];++j)
                        if(displayStyle(key,enumValues[i][j])==shown)value=enumValues[i][j];
                }
                if(!enumeration){
                    if(shown=="开启" || shown=="关闭")value=shown=="开启";
                    else{
                        char* end=nullptr;const double number=std::strtod(shown.c_str(),&end);
                        if(end!=shown.c_str() && !*end && std::isfinite(number))value=number;
                    }
                }
            }
            if(!value.is_null())fields[key]=std::move(value);
        }
        return fields;
    }
    void drawStylePresets(float scale){
        if(!scopedStyles || !MenuControls::section("样式预设",false))return;
        const char* kinds[]={"字符","段落"};
        MenuControls::segments("##preset-kind",kinds,stylePresetKind,scale);
        const char* kind=stylePresetKind?"paragraph":"character";
        std::vector<std::pair<std::string,std::string>> rows;
        for(const auto& [name,entry]:styleLibrary.items())if(entry.value("kind",std::string())==kind)rows.emplace_back(name,name);
        const int selected=MenuControls::listChoice("##saved-styles","应用已保存样式",rows);
        if(selected>=0){
            checkpointDraft();
            for(const auto& [key,value]:styleLibrary.at(rows[selected].second).at("values").items()){
                if((key=="color" && !textRgb) || (eastAsianParagraphProperty(key) && !eastAsianParagraphSupported) ||
                   (paragraphProperty(key) && key!="spaceBefore" && key!="spaceAfter" &&
                    ((!paragraphIndentsSupported && key!="align") || !paragraphAlignmentSupported || listParagraphsManaged())))continue;
                stageStyle(key.c_str(),value,false);
            }
            editorStyleRange={-1,-1,-1,-1};
        }
        MenuControls::input("##style-name","样式名称",styleName,sizeof(styleName));
        ImGui::BeginDisabled(!styleName[0]);
        if(MenuControls::button("保存当前样式",scale)){
            const auto fields=captureStylePreset(stylePresetKind!=0);
            if(fields.empty())stylePresetError="当前范围没有统一的可保存样式";
            else{
                stylePresetError.clear();
                sendMenuTool("menu.tools.preferences",Json{{"style_name",styleName},{"style",Json{{"kind",kind},{"values",fields}}}});
            }
        }
        ImGui::EndDisabled();
        if(!stylePresetError.empty()){MenuTextStyle note(MenuTextRole::Secondary);ImGui::TextWrapped("%s",stylePresetError.c_str());}
    }
    void loadStyleRanges(const Json& profiles) {
        styleDraft={};scopedStyles=mode=="typeset" && profiles.is_object();
        editorStyleRange={-1,-1,-1,-1};
        nextCharacterStyles.clear();nextCharacterAt=-1;
        undoDrafts.clear();redoDrafts.clear();
        styleAnchor=draft;
        if(!scopedStyles)return;
        std::vector<int> offsets{0};
        for(int i=0;draft[i];){
            unsigned int point=0;
            const int bytes=ImTextCharFromUtf8(&point,draft+i,nullptr);
            if(bytes<=0)break;
            if(point>0xffff)offsets.push_back(i);
            i+=bytes;offsets.push_back(i);
        }
        offsets.push_back(int(std::strlen(draft))+1);
        for(const auto& rows:profiles)for(const auto& row:rows){
            const int first=row.at("from").get<int>(),last=row.at("to").get<int>();
            if(first<0 || last<=first || last>=int(offsets.size()))continue;
            for(const auto& [key,value]:row.at("values").items())
                styleDraft.source[key].push_back({offsets[first],offsets[last],value});
        }
    }
    void stageStyle(const char* key,Json value,bool remember=true) {
        if(!scopedStyles){changedStyle();return;}
        const auto range=activeStyleRange(paragraphProperty(key));
        if(remember)checkpointDraft();
        if(range[0]==range[1]){
            if(nextCharacterAt!=range[0])nextCharacterStyles.clear();
            nextCharacterAt=range[0];
            if(value.is_null())nextCharacterStyles.erase(key);else nextCharacterStyles[key]=std::move(value);
            return;
        }
        styleDraft.set(key,range,std::move(value));
        changedText();
    }
    void refreshStyleEditor() {
        if(!scopedStyles)return;
        const auto character=activeStyleRange(false),paragraph=activeStyleRange(true);
        const std::array<int,4> stamp={character[0],character[1],paragraph[0],paragraph[1]};
        if(stamp==editorStyleRange)return;
        editorStyleRange=stamp;
        invalidComposition.clear();
        for(auto& [key,buffer]:compositionFields){
            const auto value=currentStyleOverride(key.c_str());
            std::snprintf(buffer.data(),buffer.size(),"%s",value.is_null()?"":displayStyle(key,value).c_str());
        }
        for(int i=0;i<IM_ARRAYSIZE(styleKeys);++i){
            const auto value=currentStyleOverride(styleKeys[i]);
            const auto text=value.is_null() || value=="auto"?std::string():displayStyle(styleKeys[i],value);
            std::snprintf(styleValues[i],sizeof(styleValues[i]),"%s",text.c_str());
        }
        const auto leading=currentStyleOverride("leading");
        leadingMode=leading.is_null()?0:leading=="auto"?2:1;
        for(int i=0;i<IM_ARRAYSIZE(enumKeys);++i){
            styleEnums[i]=0;
            const auto value=currentStyleOverride(enumKeys[i]);
            for(int j=1;j<IM_ARRAYSIZE(enumValues[i]) && enumValues[i][j] && enumValues[i][j][0];++j)
                if(value==enumValues[i][j])styleEnums[i]=j;
        }
        for(int i=0;i<IM_ARRAYSIZE(switchKeys);++i){
            const auto value=currentStyleOverride(switchKeys[i]);
            styleSwitches[i]=value.is_boolean()?(value.get<bool>()?2:1):0;
        }
        paragraphAlign=0;
        const auto align=currentStyleOverride("align");
        for(int i=0;i<IM_ARRAYSIZE(alignmentKeys);++i)if(align==alignmentKeys[i])paragraphAlign=i+1;
        const auto color=currentStyleOverride("color");
        textColorChanged=color.is_array();
        const auto shown=currentStyle("color");
        unsigned int rgb=0;
        if(shown.size()==7 && shown[0]=='#')std::sscanf(shown.c_str(),"#%x",&rgb);
        for(int i=0;i<3;++i)textRgbDraft[i]=((rgb>>(16-8*i))&255)/255.f;
    }
    std::array<int,2> selectedStyleBytes(int scope)const {
        const int length=int(std::strlen(draft));
        if(!scope)return {-1,-1};
        const bool selected=selectionStart!=selectionEnd;
        int start=std::clamp(selected?std::min(selectionStart,selectionEnd):lastSelectionStart,0,length);
        int end=std::clamp(selected?std::max(selectionStart,selectionEnd):lastSelectionEnd,0,length);
        if(scope==2){
            start=end=std::clamp(cursorPos,0,length);
            while(start>0 && draft[start-1]!='\n')--start;
            while(end<length && draft[end]!='\n')++end;
            ++end;
        }
        return {start,end};
    }
    Json styleChanges()const {
        Json changes=Json::object();
        for(int i=0;i<IM_ARRAYSIZE(styleKeys);++i)if(styleValues[i][0])changes[styleKeys[i]]=styleValues[i];
        if(extendedTypography){
            if(leadingMode==2)changes["leading"]="auto";
            if(paragraphAlign)changes["align"]=alignmentKeys[paragraphAlign-1];
        }
        if(advancedTypography){
            for(int i=0;i<IM_ARRAYSIZE(enumKeys);++i)if(styleEnums[i])changes[enumKeys[i]]=enumValues[i][styleEnums[i]];
            for(int i=0;i<IM_ARRAYSIZE(switchKeys);++i)if(styleSwitches[i])changes[switchKeys[i]]=styleSwitches[i]==2;
            if(textColorChanged)changes["color"]={int(std::lround(textRgbDraft[0]*255)),int(std::lround(textRgbDraft[1]*255)),int(std::lround(textRgbDraft[2]*255))};
        }
        const auto range=changes.empty() && !fontPreview.empty()
            ? selectedStyleBytes(!scopeExplicit && selectionStart!=selectionEnd?1:styleScope)
            : std::array<int,2>{styleBytes[0],styleBytes[1]+int(styleEndOfText)};
        if(!fontPreview.empty())changes["fontPostScriptName"]=fontPreview;
        if(range[0]>=0 && !changes.empty()){
            if(range[0]>=range[1])return Json::object();
            const int length=int(std::strlen(draft));
            changes["range"]={draftUnits(range[0]),draftUnits(std::min(range[1],length))+(range[1]>length?1:0)};
        }
        return changes;
    }
    void restoreList(const Json& value) {
        if(!value.is_object())return;
        const auto kind=value.value("kind",std::string()),marker=value.value("marker",std::string()),numbering=value.value("numbering",std::string());
        const auto k=std::find(std::begin(listKinds),std::end(listKinds),kind),m=std::find(std::begin(listMarkers),std::end(listMarkers),marker),n=std::find(std::begin(listStyles),std::end(listStyles),numbering);
        if(k==std::end(listKinds) || k-std::begin(listKinds)>1 || m==std::end(listMarkers) || n==std::end(listStyles) || !value.contains("start") || !value.at("start").is_number_integer())return;
        const auto start=value.at("start").get<int64_t>();const auto custom=value.value("custom_marker",std::string());
        if(start<1 || start>9999 || custom.size()>=sizeof(listCustom))return;
        listMode=int(k-std::begin(listKinds));listMarker=int(m-std::begin(listMarkers));listNumbering=int(n-std::begin(listStyles));
        std::snprintf(listStart,sizeof(listStart),"%d",int(start));std::snprintf(listCustom,sizeof(listCustom),"%s",custom.c_str());textTab=4;
    }
    int listMarker=0,listNumbering=0,indentCommand=0,selectionStart=0,selectionEnd=0,lastSelectionStart=0,lastSelectionEnd=0,cursorPos=0,draftBufferId=0;
    bool restoreDraftFocus=false;
    bool showCanvasSource=false;
    bool editingCanvasResult=false;
    int textTab=0,replaceMode=0,caseMode=0,tidyMode=0,listMode=0,splitMode=0,splitLayout=0,draftGeneration=0,previewGeneration=0,resultCount=0;
    std::string pendingAction,planId,resultValue,resultPreview,resultKind;
    std::vector<int> resultDraftOrigins;
    std::vector<MenuControls::TextRange> textMatches;std::string matchSource;
    bool textDirty=false;double textChanged=0;
    static std::pair<int,int> pairSpan(std::string_view text,int at){
        const auto trail=[](char value){return (static_cast<unsigned char>(value)&0xc0)==0x80;};
        if(at<1 || at>=int(text.size()) || trail(text[at]) || text[at]=='\n' || text[at-1]=='\n')return {-1,-1};
        int from=at-1,to=at+1;
        while(from>0 && trail(text[from]))--from;
        while(to<int(text.size()) && trail(text[to]))++to;
        return {from,to};
    }
    void remapKerning(){
        if(kerningEdits.empty() || kerningAnchor==draft)return;
        const std::string_view newer(draft),older(kerningAnchor);
        const size_t limit=std::min(older.size(),newer.size());
        size_t prefix=0,suffix=0;
        while(prefix<limit && older[prefix]==newer[prefix])++prefix;
        while(suffix<limit-prefix && older[older.size()-suffix-1]==newer[newer.size()-suffix-1])++suffix;
        std::map<int,Json> kept;
        for(const auto& [at,value]:kerningEdits){
            const auto [start,end]=pairSpan(older,at);
            if(start<0)continue;
            const int mapped=end<=int(prefix)?at:start>=int(older.size()-suffix)?at+int(newer.size())-int(older.size()):-1;
            const auto [mappedStart,mappedEnd]=pairSpan(newer,mapped);
            if(mappedStart>=0 && newer.substr(mappedStart,mappedEnd-mappedStart)==older.substr(start,end-start))kept.emplace(mapped,value);
        }
        kerningEdits=std::move(kept);kerningAnchor=draft;kerningFieldAt=-1;
    }
    void remapStyleRange(){
        if((!scopedStyles && styleBytes[0]<0) || styleAnchor==draft)return;
        const std::string_view old(styleAnchor),next(draft);
        int prefix=0,suffix=0;
        const int oldSize=int(old.size()),nextSize=int(next.size());
        while(prefix<std::min(oldSize,nextSize) && old[prefix]==next[prefix])++prefix;
        while(prefix>0 && prefix<nextSize && (static_cast<unsigned char>(next[prefix])&0xc0)==0x80)--prefix;
        while(suffix<std::min(oldSize,nextSize)-prefix && old[oldSize-1-suffix]==next[nextSize-1-suffix])++suffix;
        while(suffix>0 && (static_cast<unsigned char>(next[nextSize-suffix])&0xc0)==0x80)--suffix;
        const int oldEnd=oldSize-suffix,newEnd=nextSize-suffix;
        const auto remap=[&](int offset,bool end){
            if(offset<prefix || (end && offset==prefix))return offset;
            if(offset>=oldEnd)return offset+newEnd-oldEnd;
            return end?newEnd:prefix;
        };
        if(scopedStyles){
            styleDraft.remap(remap);
            if(oldEnd==prefix && newEnd>prefix && nextCharacterAt==prefix){
                for(const auto& [key,value]:nextCharacterStyles)styleDraft.set(key.c_str(),{prefix,newEnd},value);
                nextCharacterAt=newEnd;
            }else{nextCharacterStyles.clear();nextCharacterAt=-1;}
            editorStyleRange={-1,-1,-1,-1};
        }else{
            styleBytes={remap(styleBytes[0],false),remap(styleBytes[1],true)};
            styleBytes[1]=std::max(styleBytes[0],styleBytes[1]);
        }
        styleAnchor=draft;
    }
    void changedText(){remapKerning();remapStyleRange();proofCurrent=false;proofAttempt.clear();sharpAttempt=sharpProofScale=1;++draftGeneration;planId.clear();resultValue.clear();resultPreview.clear();resultKind.clear();resultDraftOrigins.clear();textMatches.clear();matchSource.clear();error.clear();textDirty=true;textChanged=ImGui::GetTime();}
    void changedStyle(){
        styleBytes=selectedStyleBytes(styleScope);styleAnchor=draft;
        styleEndOfText=styleBytes[1]>int(styleAnchor.size());
        if(styleEndOfText)styleBytes[1]=int(styleAnchor.size());
        changedText();
    }
    void setDraft(const std::string& value){std::snprintf(draft,sizeof(draft),"%s",value.c_str());kerningEdits.clear();kerningFieldAt=-1;kerningFieldInvalid=false;++draftBufferId;indentCommand=selectionStart=selectionEnd=lastSelectionStart=lastSelectionEnd=cursorPos=0;restoreDraftFocus=false;changedText();}
    int draftUnits(int bytes)const{return bytes>0?MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,draft,bytes,nullptr,0):0;}
    static std::vector<int> textUnitPositions(std::string_view value){
        std::vector<int> positions;
        for(int at=0;at<int(value.size());){
            unsigned code=0;
            const int bytes=ImTextCharFromUtf8(&code,value.data()+at,value.data()+value.size());
            positions.push_back(at);
            if(bytes==4)positions.push_back(at);
            at+=std::max(1,bytes);
        }
        positions.push_back(int(value.size()));
        positions.push_back(int(value.size())+1);
        return positions;
    }
    int mapResultByte(int byte,const std::vector<int>& before,const std::vector<int>& after)const{
        const auto unit=std::lower_bound(before.begin(),before.end(),byte)-before.begin();
        const auto mapped=std::lower_bound(resultDraftOrigins.begin(),resultDraftOrigins.end(),unit)-resultDraftOrigins.begin();
        return after[mapped];
    }
    bool hasStyles()const{return (scopedStyles?!styleDraft.edits.empty():!styleChanges().empty()) || !kerningEdits.empty() || sourceBoxSize!=draftBoxSize || draftBoxOffset!=0;}
    static int draftCallback(ImGuiInputTextCallbackData* data) {
        auto& panel=*static_cast<MenuToolsPanel*>(data->UserData);
        if(data->EventFlag==ImGuiInputTextFlags_CallbackCharFilter) {
            const auto& io=ImGui::GetIO();
            if(panel.textTab==4 && data->EventChar=='\t' && ImGui::IsKeyDown(ImGuiKey_Tab) && !io.KeyCtrl && !io.KeyAlt && !io.KeySuper){panel.indentCommand=1;return 1;}
            return 0;
        }
        if(panel.restoreDraftFocus){data->SelectionStart=panel.selectionStart;data->SelectionEnd=panel.selectionEnd;data->CursorPos=panel.cursorPos;panel.restoreDraftFocus=false;}
        const auto& io=ImGui::GetIO();
        if(panel.pickFromLayout && std::strcmp(data->Buf,panel.draft)==0){
            data->SelectionStart=panel.canvasSelectionAnchor;data->SelectionEnd=panel.canvasPickedByte;
            data->CursorPos=panel.canvasPickedByte;
        }
        if(panel.textTab==4)ImGui::SetKeyOwner(ImGuiKey_Tab,data->ID);
        if(panel.textTab==4 && io.KeyShift && !io.KeyCtrl && !io.KeyAlt && !io.KeySuper && ImGui::IsKeyPressed(ImGuiKey_Tab))panel.indentCommand=-1;
        if(panel.indentCommand) {
            const auto result=menuIndentLines(data->Buf,data->SelectionStart,data->SelectionEnd,data->CursorPos,panel.indentCommand);panel.indentCommand=0;
            if(!result.error.empty()){panel.error=result.error;requireText(panel.error);}
            else if(result.text!=data->Buf){data->DeleteChars(0,data->BufTextLen);data->InsertChars(0,result.text.c_str());data->SelectionStart=result.start;data->SelectionEnd=result.end;data->CursorPos=result.cursor;}
        }
        const bool selectionChanged=panel.selectionStart!=data->SelectionStart || panel.selectionEnd!=data->SelectionEnd;
        const bool rangeChanged=selectionChanged && data->SelectionStart!=data->SelectionEnd;
        if(rangeChanged){
            panel.lastSelectionStart=std::min(data->SelectionStart,data->SelectionEnd);
            panel.lastSelectionEnd=std::max(data->SelectionStart,data->SelectionEnd);
        }
        if(selectionChanged || panel.cursorPos!=data->CursorPos)panel.canvasInteraction=ImGui::GetTime();
        panel.selectionStart=data->SelectionStart;panel.selectionEnd=data->SelectionEnd;panel.cursorPos=data->CursorPos;
        return 0;
    }
    bool textDue(double now)const{return textDirty && active && ready && !pending && !resizeHandle && !revision.empty() && canvasState!=CanvasState::stale && !kerningFieldInvalid && (kerningEdits.empty() || textTab==0) && (textTab!=1 || find[0] || editingCanvasResult) && (textTab!=3 || splitMode==0 || separator[0]) && now-textChanged>=.12;}
    bool canWriteText()const{return !pending && !revision.empty() && canvasState!=CanvasState::stale && !kerningFieldInvalid && invalidComposition.empty() && draft[0];}
    bool composingText()const{
        if(imeComposing)return true;
        const auto focused=GetFocus();
        if(!focused)return false;
        const auto context=ImmGetContext(focused);
        if(!context)return false;
        const bool composing=ImmGetCompositionStringW(context,GCS_COMPSTR,nullptr,0)>0;
        ImmReleaseContext(focused,context);
        return composing;
    }
    std::string textStatus()const {
        if(!error.empty())return textTab==3 && mode!="typeset"?error+"；仍可写回当前文字":error;
        if(!fontCandidate.empty() || !fontPreview.empty())return "字体预览 · 点击候选可选定";
        if(pending && pendingAction=="text.preview")return "正在自动预览 · 可继续编辑，尚未写入PS";
        if(pending && (pendingAction=="text.write" || pendingAction=="text.apply" || pendingAction=="text.vector"))return pendingAction=="text.apply" && resultKind=="split"?"正在拆成多个图层…":pendingAction=="text.vector" || vectorObject?"正在生成矢量文字对象…":"正在写回文字…";
        if(planId.empty()) {
            if(textTab==1 && !find[0])return "请输入查找内容后自动预览";
            if(textTab==3 && splitMode!=0 && !separator[0])return "请输入分隔符或正则；仍可写回当前文字";
            return "等待自动预览 · 尚未写入PS";
        }
        return resultKind=="split"?"拆分预览 · "+std::to_string(resultCount)+" 个文字层":"处理结果 · 确认后写入PS";
    }
    Json textParams()const {
        Json args={{"revision",revision},{"draft",draft},{"operation","edit"}};
        if(textTab==1 && !editingCanvasResult){args["operation"]="replace";args["find"]=find;args["replacement"]=replacement;args["regex"]=replaceMode==1;args["ignore_case"]=caseMode==1;}
        if(textTab==2 && !editingCanvasResult){const char* operations[]={"trim","remove-empty","space","join","upper","lower"};args["operation"]=operations[tidyMode];if(tidyMode==3)args["separator"]=joiner;}
        if(textTab==3){const char* modes[]={"lines","literal","regex"};args["operation"]="split";args["mode"]=modes[splitMode];args["groups"]=grouping[0]?std::string(grouping):menuDefaultGroups(draft);args["separator"]=separator;args["layout"]=splitLayout==0?"original":"lines";}
        if(textTab==4){args["operation"]="list";args["kind"]=listKinds[listMode];args["marker"]=listMarkers[listMarker];args["custom_marker"]=listMarker==6?listCustom:"";args["numbering"]=listStyles[listNumbering];args["start"]=listMode==1?listStart:"1";}
        else if(textTab!=3 && draftListChanged)args["list_options"]=draftList;
        if(textTab!=3){
            if(scopedStyles){
                auto styles=styleDraft.serialize(int(std::strlen(draft)),[&](int at){return draftUnits(at);});
                if(!styles.empty())args["typography_runs"]=std::move(styles);
            }else{
                const auto styles=styleChanges();
                if(!styles.empty())args["typography"]=styles;
                if(!fontPreview.empty())args["font_preview"]=true;
            }
        }
        if(textTab!=3 && (sourceBoxSize!=draftBoxSize || draftBoxOffset!=0)){
            args["box_size"]=draftBoxSize;
            if(draftBoxOffset!=0)args["box_shift_x"]=draftBoxOffset;
        }
        if(textTab==0 && !kerningEdits.empty()){
            auto pairs=Json::array();
            for(const auto& [at,value]:kerningEdits)pairs.push_back({draftUnits(at),value});
            args["kerning"]=std::move(pairs);
        }
        return args;
    }
    ID3D11ShaderResourceView* thumbnail=nullptr;
    ID3D11ShaderResourceView* proofImage=nullptr;
    ID3D11ShaderResourceView* sharpProofImage=nullptr;
    CanvasTransparency transparency;
    int localDraftGeneration=-1;
    std::string localLayoutPlan;
    float localDraftWidth=0,localDraftHeight=0,localDraftEm=0;
    bool localDraftReady=false;
    std::vector<FontSampleRaster::DraftLayout> localLayout;
    int canvasSelectionAnchor=0,canvasPickedByte=0;
    bool canvasPicking=false,pickFromLayout=false;
    ImVec2 thumbnailSize{};
    int previewHovered=-1,previewTarget=-1,previewRequest=0;
    double previewAt=0;
    bool previewPending=false,previewVisible=false;
    std::string previewQuery,previewMessage;
    void clearPreview(){if(thumbnail){thumbnail->Release();thumbnail=nullptr;}thumbnailSize={};previewTarget=-1;previewVisible=false;previewMessage.clear();}
    void clearProof(){
        for(auto** image:{&proofImage,&sharpProofImage})if(*image){(*image)->Release();*image=nullptr;}
        proofCurrent=false;sharpProofScale=sharpAttempt=1;
    }
    bool loadPreview(const Json& result) {
        clearPreview();
        if(!result.value("available",false)){previewMessage=result.value("reason",std::string("没有可显示的内容"));return false;}
        const int w=result.value("width",0),h=result.value("height",0);
        if(w<1 || h<1 || w>128 || h>128 || !result.contains("pixels") || !result.at("pixels").is_array() || result.at("pixels").size()!=size_t(w*h*4))return false;
        std::vector<unsigned char> pixels;pixels.reserve(w*h*4);
        for(const auto& item:result.at("pixels")){if(!item.is_number_integer() || item.get<int64_t>()<0 || item.get<int64_t>()>255)return false;pixels.push_back(item.get<unsigned char>());}
        return loadPixels(w,h,pixels.data());
    }
    bool loadPixels(int w,int h,const void* pixels,ID3D11ShaderResourceView** proof=nullptr){
        D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{pixels,UINT(w*4),0};ID3D11Texture2D* texture=nullptr;
        if(!device || FAILED(device->CreateTexture2D(&desc,&data,&texture)))return false;
        ID3D11ShaderResourceView* view=nullptr;
        const auto created=device->CreateShaderResourceView(texture,nullptr,&view);texture->Release();
        if(FAILED(created))return false;
        if(proof){if(*proof)(*proof)->Release();*proof=view;}
        else {thumbnail=view;thumbnailSize={float(w),float(h)};}
        return true;
    }
    static std::wstring wideText(std::string_view text){
        const int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0);
        std::wstring result(length,L'\0');
        if(length)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),result.data(),length);
        return result;
    }
    bool layoutLocalDraft(float width,float height,float em,bool processed=false){
        auto* painter=scopedStyles?canvasGlyphTarget.prepare(device):nullptr;
        if(!painter)return false;
        const std::string_view plan=processed?std::string_view(planId):std::string_view{};
        if(localDraftGeneration==draftGeneration && std::string_view(localLayoutPlan)==plan && width==localDraftWidth && height==localDraftHeight && localDraftEm==em)
            return localDraftReady;
        localDraftGeneration=draftGeneration;localDraftWidth=width;localDraftHeight=height;localDraftEm=em;
        localLayoutPlan=plan;
        const std::string_view text=processed?std::string_view(resultValue):std::string_view(draft);
        const auto positions=textUnitPositions(text);
        const auto unitsAt=[&](int byte){return int(std::lower_bound(positions.begin(),positions.end(),byte)-positions.begin());};
        TextStyleDraft resultStyles;
        const auto* styles=&styleDraft;
        if(processed && !resultDraftOrigins.empty()){
            const auto before=textUnitPositions(draft);
            resultStyles=styleDraft;
            resultStyles.remap([&](int byte,bool){return mapResultByte(byte,before,positions);});
            styles=&resultStyles;
        }
        const bool listed=processed && textTab==4?listMode!=2:listParagraphsManaged();
        std::vector<FontSampleRaster::DraftParagraph> paragraphs;
        const int length=int(text.size());
        const auto number=[](const std::string& text,float fallback){
            char* end=nullptr;const float n=std::strtof(text.c_str(),&end);
            return end==text.c_str() || *end || !std::isfinite(n)?fallback:n;
        };
        const auto at=[&](const char* key,int byte,bool edited=true){
            return styles->current(key,{byte,byte},displayStyle,edited);
        };
        const float point=em/std::max(1.f,number(styleDraft.current("size",{0,0},displayStyle,false),12.f));
        const auto hover=activeStyleRange(false);
        std::vector<int> breaks{0,length};
        for(const auto* map:{&styles->source,&styles->edits})for(const auto& [key,runs]:*map){
            if(paragraphProperty(key))continue;
            for(const auto& run:runs){breaks.push_back(std::min(length,run.first));breaks.push_back(std::min(length,run.last));}
        }
        if(!processed && !fontPreview.empty()){breaks.push_back(hover[0]);breaks.push_back(std::min(length,hover[1]));}
        std::sort(breaks.begin(),breaks.end());breaks.erase(std::unique(breaks.begin(),breaks.end()),breaks.end());
        struct Feature {const char* key;DWRITE_FONT_FEATURE_TAG tag;};
        static constexpr Feature features[]={
            {"ligature",DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES},{"contextualLigatures",DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES},
            {"stylisticAlternates",DWRITE_FONT_FEATURE_TAG_STYLISTIC_ALTERNATES},{"swash",DWRITE_FONT_FEATURE_TAG_SWASH},
            {"titling",DWRITE_FONT_FEATURE_TAG_TITLING},{"ornaments",static_cast<DWRITE_FONT_FEATURE_TAG>(DWRITE_MAKE_OPENTYPE_TAG('o','r','n','m'))},
            {"fractions",DWRITE_FONT_FEATURE_TAG_FRACTIONS},{"ordinals",DWRITE_FONT_FEATURE_TAG_ORDINALS},
        };
        for(int first=0;first<=length;){
            int last=first;while(last<length && text[last]!='\n')++last;
            FontSampleRaster::DraftParagraph paragraph;
            paragraph.text=wideText(text.substr(first,last-first));
            paragraph.left=number(at("startIndent",first),0)*point;
            paragraph.right=number(at("endIndent",first),0)*point;
            paragraph.first=number(at("firstLineIndent",first),0)*point;
            paragraph.before=number(at("spaceBefore",first),0)*point;paragraph.after=number(at("spaceAfter",first),0)*point;
            paragraph.leading=number(at("leading",first),0)*point;
            const auto align=at("align",first);
            if(align=="居中")paragraph.align=DWRITE_TEXT_ALIGNMENT_CENTER;
            else if(align=="右对齐")paragraph.align=DWRITE_TEXT_ALIGNMENT_TRAILING;
            else if(align.find("两端对齐")!=std::string::npos)paragraph.align=DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
            if(listed && paragraph.align==DWRITE_TEXT_ALIGNMENT_LEADING){
                const auto leading=paragraph.text.find_first_not_of(L" \t");
                const auto delimiter=paragraph.text.find_first_of(L" \t",leading);
                if(delimiter!=std::wstring::npos && delimiter-leading<=32 &&
                   paragraph.text[delimiter]==L'\t' && delimiter+1<paragraph.text.size()){
                    paragraph.markerEnd=UINT32(delimiter+1);
                    paragraph.listLevel=std::min<size_t>(5,(leading+3)/4);
                }
            }
            const int firstUnit=unitsAt(first);
            for(int offset=first;offset<last;){
                const int end=std::min(last,*std::upper_bound(breaks.begin(),breaks.end(),offset));
                unsigned int rgb=0;
                const auto color=at("color",offset);
                if(color.size()==7 && color[0]=='#')std::sscanf(color.c_str(),"#%x",&rgb);
                const auto font=!processed && !fontPreview.empty() && offset>=hover[0] && offset<hover[1]?fontPreview:at("fontPostScriptName",offset);
                const float size=std::clamp(number(at("size",offset),12.f)*point,.01f,65536.f);
                const int unit=unitsAt(offset);
                FontSampleRaster::DraftRun run{{UINT32(unit-firstUnit),UINT32(unitsAt(end)-unit)},
                    wideText(font),size,number(at("tracking",offset),0)*size/1000,
                    {float((rgb>>16)&255)/255,float((rgb>>8)&255)/255,float(rgb&255)/255},
                    at("syntheticBold",offset)=="开启",at("syntheticItalic",offset)=="开启",
                    at("underline",offset)=="开启",at("strikethrough",offset)=="开启",{}};
                for(const auto& feature:features){
                    const auto value=at(feature.key,offset);
                    if(value=="开启" || value=="关闭")run.features.push_back({feature.tag,value=="开启"?1u:0u});
                }
                const auto caps=at("fontCaps",offset),baseline=at("baseline",offset),figures=at("figureStyle",offset);
                if(caps=="小型大写")run.features.push_back({DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS,1});
                if(baseline=="上标")run.features.push_back({DWRITE_FONT_FEATURE_TAG_SUPERSCRIPT,1});
                if(baseline=="下标")run.features.push_back({DWRITE_FONT_FEATURE_TAG_SUBSCRIPT,1});
                if(figures=="比例等高" || figures=="比例旧式")run.features.push_back({DWRITE_FONT_FEATURE_TAG_PROPORTIONAL_FIGURES,1});
                if(figures=="等宽等高" || figures=="等宽旧式")run.features.push_back({DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES,1});
                if(figures=="比例等高" || figures=="等宽等高")run.features.push_back({DWRITE_FONT_FEATURE_TAG_LINING_FIGURES,1});
                if(figures=="比例旧式" || figures=="等宽旧式")run.features.push_back({DWRITE_FONT_FEATURE_TAG_OLD_STYLE_FIGURES,1});
                paragraph.runs.push_back(std::move(run));offset=end;
            }
            paragraphs.push_back(std::move(paragraph));
            if(last==length)break;
            first=last+1;
        }
        return localDraftReady=fontRaster.layoutDraft(paragraphs,width,height,em,canvasWrap,*painter,localLayout);
    }
    int canvasByteAt(ImVec2 point,std::string_view text)const{
        const auto row=std::lower_bound(localLayout.begin(),localLayout.end(),point.y,
            [](const auto& para,float y){return para.bottom<y;});
        if(row==localLayout.end())return int(text.size());
        const auto& span=row->marker.text && point.y<=row->firstLineBottom && point.x<row->x?
            row->marker:static_cast<const FontSampleRaster::DraftFragment&>(*row);
        DWRITE_HIT_TEST_METRICS hit{};BOOL trailing=FALSE,inside=FALSE;
        if(FAILED(span.text->HitTestPoint(point.x-span.x,point.y-span.y,&trailing,&inside,&hit)))return cursorPos;
        const auto unit=span.start+std::min(span.length,hit.textPosition+(trailing?hit.length:0));
        const char* const end=text.data()+text.size();
        const char* byte=text.data();
        for(UINT32 next=0;next<unit && byte<end;){
            unsigned int ch=0;
            const int consumed=ImTextCharFromUtf8(&ch,byte,end);
            next+=ch>0xffff?2:1;
            if(!consumed || next>unit)break;
            byte+=consumed;
        }
        return int(byte-text.data());
    }
    int canvasByteAt(ImVec2 point)const{return canvasByteAt(point,draft);}
    static void drawGlyphs(ImDrawList& draw,const CanvasViewTransform& view,ImVec2 at,ImVec2 factor,
                           float width,float height,const std::vector<FontSampleRaster::DraftLayout>& layout){
        struct GlyphDraw {
            const std::vector<FontSampleRaster::DraftLayout>* paragraphs;
            ImVec2 at,right,down;
            float width,height;
        };
        const auto origin=view.apply(at),right=view.apply({at.x+factor.x,at.y}),down=view.apply({at.x,at.y+factor.y});
        GlyphDraw glyphs{&layout,origin,{right.x-origin.x,right.y-origin.y},{down.x-origin.x,down.y-origin.y},
            width,height};
        draw.AddCallback([](const ImDrawList*,const ImDrawCmd* command){
            const auto& glyphs=*static_cast<const GlyphDraw*>(command->UserCallbackData);
            const auto& data=*ImGui::GetDrawData();
            const auto* state=static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
            auto* gpu=state->DeviceContext;
            Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;
            gpu->OMGetRenderTargets(1,view.GetAddressOf(),nullptr);
            gpu->OMSetRenderTargets(0,nullptr,nullptr);
            if(auto* painter=canvasGlyphTarget.prepare(state->Device);painter && canvasGlyphTarget.bind(view.Get())){
                const auto scale=data.FramebufferScale;
                const auto screen=[&](float x,float y){return D2D1::Point2F((x-data.DisplayPos.x)*scale.x,(y-data.DisplayPos.y)*scale.y);};
                const auto min=screen(command->ClipRect.x,command->ClipRect.y),max=screen(command->ClipRect.z,command->ClipRect.w);
                const auto origin=screen(glyphs.at.x,glyphs.at.y);
                Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> ink;
                if(min.x<max.x && min.y<max.y && SUCCEEDED(painter->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),ink.GetAddressOf()))){
                    painter->BeginDraw();
                    painter->SetTransform(D2D1::Matrix3x2F::Identity());
                    painter->PushAxisAlignedClip(D2D1::RectF(min.x,min.y,max.x,max.y),D2D1_ANTIALIAS_MODE_ALIASED);
                    painter->SetTransform(D2D1::Matrix3x2F(glyphs.right.x*scale.x,glyphs.right.y*scale.y,
                        glyphs.down.x*scale.x,glyphs.down.y*scale.y,origin.x,origin.y));
                    Microsoft::WRL::ComPtr<ID2D1Factory> factory;
                    Microsoft::WRL::ComPtr<ID2D1RectangleGeometry> box;
                    painter->GetFactory(factory.GetAddressOf());
                    const auto layoutBounds=D2D1::RectF(0,0,glyphs.width,glyphs.height);
                    const bool masked=SUCCEEDED(factory->CreateRectangleGeometry(layoutBounds,box.GetAddressOf()));
                    if(masked)painter->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),box.Get()),nullptr);
                    for(const auto& paragraph:*glyphs.paragraphs){
                        const auto& marker=paragraph.marker;
                        if(marker.text)painter->DrawTextLayout({marker.x,marker.y},marker.text.Get(),ink.Get(),D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
                        painter->DrawTextLayout({paragraph.x,paragraph.y},paragraph.text.Get(),ink.Get(),D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
                    }
                    if(masked)painter->PopLayer();
                    painter->PopAxisAlignedClip();
                    if(FAILED(painter->EndDraw()))canvasGlyphTarget.detach();
                }
            }
            gpu->OMSetRenderTargets(1,view.GetAddressOf(),nullptr);
        },&glyphs,sizeof(glyphs));
        draw.AddCallback(ImDrawCallback_ResetRenderState,nullptr);
    }
    void drawCanvasSelection(ImDrawList& draw,const CanvasViewTransform& view,ImVec2 at,ImVec2 factor,ImU32 ink,bool focus){
        const UINT32 lo=draftUnits(std::min(selectionStart,selectionEnd)),hi=draftUnits(std::max(selectionStart,selectionEnd));
        const auto transform=[&](float x,float y){return ImVec2{at.x+x*factor.x,at.y+y*factor.y};};
        if(focus && hi>lo){
            const auto background=ImGui::GetColorU32(ImGuiCol_TextSelectedBg);
            for(const auto& para:localLayout)for(const auto* fragment:{
                static_cast<const FontSampleRaster::DraftFragment*>(&para),&para.marker}){
                const auto& span=*fragment;
                const UINT32 first=std::max(lo,span.start),last=std::min(hi,span.start+span.length);
                if(first>=last)continue;
                UINT32 count=0;
                span.text->HitTestTextRange(first-span.start,last-first,span.x,span.y,nullptr,0,&count);
                std::vector<DWRITE_HIT_TEST_METRICS> boxes(count);
                if(SUCCEEDED(span.text->HitTestTextRange(first-span.start,last-first,span.x,span.y,boxes.data(),count,&count)))
                    for(const auto& box:boxes){
                        const ImRect clipped({std::max(0.f,box.left),std::max(0.f,box.top)},
                            {std::min(localDraftWidth,box.left+box.width),std::min(localDraftHeight,box.top+box.height)});
                        if(clipped.Min.x>=clipped.Max.x || clipped.Min.y>=clipped.Max.y)continue;
                        const auto points=view.quad(transform(clipped.Min.x,clipped.Min.y),transform(clipped.Max.x,clipped.Max.y));
                        draw.AddQuadFilled(points[0],points[1],points[2],points[3],background);
                    }
            }
        }
        drawGlyphs(draw,view,at,factor,localDraftWidth,localDraftHeight,localLayout);
        if(!focus)return;
        const UINT32 unit=draftUnits(cursorPos);
        const auto row=std::lower_bound(localLayout.begin(),localLayout.end(),unit,
            [](const auto& para,UINT32 at){return para.start+para.length<at;});
        if(row==localLayout.end())return;
        const auto& span=row->marker.text && unit<row->start?row->marker:static_cast<const FontSampleRaster::DraftFragment&>(*row);
        const auto pos=unit-span.start;
        const bool trailing=pos && pos==span.length;
        float x=0,y=0;DWRITE_HIT_TEST_METRICS hit{};
        if(FAILED(span.text->HitTestTextPosition(pos-(trailing?1:0),trailing,&x,&y,&hit)))return;
        const auto top=view.apply(transform(span.x+x,span.y+y)),bottom=view.apply(transform(span.x+x,span.y+y+hit.height));
        const auto& state=GImGui->InputTextState;
        if(!ImGui::GetIO().ConfigInputTextCursorBlink || state.CursorAnim<=0 || std::fmod(state.CursorAnim,1.2f)<=.8f)
            draw.AddLine(top,bottom,ink);
        if(GImGui->PlatformImeData.WantTextInput){
            const auto min=draw.GetClipRectMin(),max=draw.GetClipRectMax();
            GImGui->PlatformImeData.InputPos={std::clamp(top.x,min.x,max.x),std::clamp(top.y,min.y,max.y)};
            GImGui->PlatformImeData.InputLineHeight=hit.height*factor.y;
        }
    }
    bool loadProofPixels(BYTE* bytes,DWORD length,int width,int height,ID3D11ShaderResourceView** image){
        IWICImagingFactory* factory=nullptr;IWICStream* stream=nullptr;IWICBitmapDecoder* decoder=nullptr;
        IWICBitmapFrameDecode* frame=nullptr;IWICFormatConverter* converter=nullptr;
        HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
        if(SUCCEEDED(hr))hr=factory->CreateStream(&stream);
        if(SUCCEEDED(hr))hr=stream->InitializeFromMemory(bytes,length);
        if(SUCCEEDED(hr))hr=factory->CreateDecoderFromStream(stream,nullptr,WICDecodeMetadataCacheOnLoad,&decoder);
        if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);
        UINT w=0,h=0;
        if(SUCCEEDED(hr))hr=frame->GetSize(&w,&h);
        if(SUCCEEDED(hr) && (w!=UINT(width)||h!=UINT(height)))hr=E_INVALIDARG;
        if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(&converter);
        if(SUCCEEDED(hr))hr=converter->Initialize(frame,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
        bool loaded=false;
        if(SUCCEEDED(hr)){
            std::vector<BYTE> pixels(size_t(w)*h*4);
            if(SUCCEEDED(converter->CopyPixels(nullptr,w*4,UINT(pixels.size()),pixels.data())))loaded=loadPixels(width,height,pixels.data(),image);
        }
        if(converter)converter->Release();if(frame)frame->Release();if(decoder)decoder->Release();
        if(stream)stream->Release();if(factory)factory->Release();
        return loaded;
    }
    bool loadCanvas(const Json& value){
        const bool proof=value.value("encoding",std::string())=="png";
        if(!proof)clearPreview();
        const auto key=value.value("mapping",std::string());
        const int w=value.value("width",0),h=value.value("height",0);
        const int limit=proof?8192:value.value("background",false)?2048:640;
        if(key.size()!=32 || key.find_first_not_of("0123456789abcdef")!=std::string::npos || w<1 || h<1 || w>limit || h>limit || size_t(w)*h>16777216)return false;
        const int bytes=proof?value.value("bytes",0):w*h*4;
        if(bytes<=0||bytes>80*1024*1024)return false;
        HANDLE file=OpenFileMappingA(FILE_MAP_READ,FALSE,("Local\\151500px-menu-canvas-"+key).c_str());
        if(!file)return false;
        void* view=MapViewOfFile(file,FILE_MAP_READ,0,0,bytes);
        const bool loaded=view && (proof?loadProofPixels(static_cast<BYTE*>(view),DWORD(bytes),w,h,
            value.value("scale",1)>1?&sharpProofImage:&proofImage):loadPixels(w,h,view));
        if(view)UnmapViewOfFile(view);
        CloseHandle(file);return loaded;
    }
    bool searchDirty=false;
    double searchChanged=0;
    void changeSearch(double now){clearPreview();previewHovered=-1;searchDirty=true;searchChanged=now;results=Json();error.clear();}
    bool searchDue(double now)const{return searchDirty && !pending && !previewPending && ready && now-searchChanged>=.18;}
    int kind=0,request=0;
    Json results;
    MenuSplitPreview split;
    bool dragging=false,maximized=false;
    POINT dragPointer{};
    RECT restoredRect{};
    enum WindowEdge {windowLeft=1,windowTop=2,windowRight=4,windowBottom=8};
    int windowResize=0;
    POINT windowResizeStart{};
    RECT windowResizeRect{};
    static POINT dragPosition(RECT own,RECT limit,POINT delta) {
        return {std::clamp(own.left+delta.x,limit.left,std::max(limit.left,limit.right-(own.right-own.left))),
                std::clamp(own.top+delta.y,limit.top,std::max(limit.top,limit.bottom-(own.bottom-own.top)))};
    }
    static bool windowBounds(RECT& limit){
        HWND owner=GetWindow(window,GW_OWNER);
        RECT ownerRect{};
        MONITORINFO monitor{sizeof(MONITORINFO)};
        return owner && GetWindowRect(owner,&ownerRect)
            && GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor)
            && IntersectRect(&limit,&ownerRect,&monitor.rcWork);
    }
    static int windowEdges(RECT bounds,POINT pointer,LONG grip){
        if(!PtInRect(&bounds,pointer))return 0;
        return (pointer.x-bounds.left<grip?windowLeft:pointer.x>=bounds.right-grip?windowRight:0) |
            (pointer.y-bounds.top<grip?windowTop:pointer.y>=bounds.bottom-grip?windowBottom:0);
    }
    static RECT typesetFrame(RECT bounds,float scale){
        const LONG inset=std::min(std::lround(menuFloatingInset*scale),
            std::max(0L,(std::min(bounds.right-bounds.left,bounds.bottom-bounds.top)-1)/2));
        InflateRect(&bounds,-inset,-inset);
        return bounds;
    }
    static RECT resizedWindow(RECT bounds,RECT limit,POINT delta,int edge,SIZE minimum){
        minimum.cx=std::min(minimum.cx,limit.right-limit.left);
        minimum.cy=std::min(minimum.cy,limit.bottom-limit.top);
        if(edge&windowLeft)bounds.left=std::clamp(bounds.left+delta.x,limit.left,std::max(limit.left,bounds.right-minimum.cx));
        if(edge&windowRight)bounds.right=std::clamp(bounds.right+delta.x,std::min(limit.right,bounds.left+minimum.cx),limit.right);
        if(edge&windowTop)bounds.top=std::clamp(bounds.top+delta.y,limit.top,std::max(limit.top,bounds.bottom-minimum.cy));
        if(edge&windowBottom)bounds.bottom=std::clamp(bounds.bottom+delta.y,std::min(limit.bottom,bounds.top+minimum.cy),limit.bottom);
        return bounds;
    }
    ImGuiMouseCursor canvasCursor=ImGuiMouseCursor_None;
    bool nativeCanvasCursor=false;
    static HCURSOR canvasCursorHandle(ImGuiMouseCursor cursor){
        switch(cursor){
        case ImGuiMouseCursor_ResizeEW:return LoadCursorW(nullptr,IDC_SIZEWE);
        case ImGuiMouseCursor_ResizeNS:return LoadCursorW(nullptr,IDC_SIZENS);
        case ImGuiMouseCursor_ResizeNWSE:return LoadCursorW(nullptr,IDC_SIZENWSE);
        case ImGuiMouseCursor_ResizeNESW:return LoadCursorW(nullptr,IDC_SIZENESW);
        case ImGuiMouseCursor_ResizeAll:return LoadCursorW(nullptr,IDC_SIZEALL);
        default:return nullptr;
        }
    }
    bool applyCanvasCursor(HWND hwnd){
        if(!active || mode!="typeset" || !IsWindowVisible(hwnd) || (!nativeCanvasCursor && canvasCursor==ImGuiMouseCursor_None))return false;
        const HWND foreground=GetForegroundWindow();
        const HWND owner=GetWindow(hwnd,GW_OWNER);
        POINT pointer{};RECT bounds{};
        if((foreground!=hwnd && (!owner || !IsWindowEnabled(owner) ||
           (foreground!=owner && GetAncestor(foreground,GA_ROOTOWNER)!=owner))) || !GetCursorPos(&pointer) ||
           (WindowFromPoint(pointer)!=hwnd && !(GetCapture()==hwnd && (resizeHandle || canvasZoomDragging)))){
            nativeCanvasCursor=false;
            return false;
        }
        const float dpi=ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
        if(windowResize || (!maximized && !resizeHandle && GetWindowRect(hwnd,&bounds) &&
           windowEdges(typesetFrame(bounds,dpi),pointer,std::max(1L,std::lround(5*dpi))))){
            nativeCanvasCursor=false;
            return false;
        }
        const HCURSOR cursor=canvasCursorHandle(canvasCursor);
        if(!cursor && !nativeCanvasCursor)return false;
        SetCursor(cursor?cursor:LoadCursorW(nullptr,IDC_ARROW));
        nativeCanvasCursor=cursor!=nullptr;
        return true;
    }
    bool windowResizeMessage(HWND hwnd,UINT message,WPARAM wp){
        if(message==WM_CAPTURECHANGED){windowResize=0;return false;}
        const bool sizingMessage=message==WM_SETCURSOR || message==WM_LBUTTONDOWN || message==WM_MOUSEMOVE;
        if(!windowResize && (!sizingMessage || !active || mode!="typeset" || maximized))return false;
        const HWND foreground=GetForegroundWindow();
        const bool available=active && mode=="typeset" && !maximized && IsWindowVisible(hwnd) &&
            (foreground==hwnd || foreground==GetWindow(hwnd,GW_OWNER));
        if(windowResize && (!available || message==WM_CANCELMODE || (message==WM_SHOWWINDOW && !wp) || message==WM_LBUTTONUP)){
            windowResize=0;
            if(GetCapture()==hwnd)ReleaseCapture();
            return message==WM_LBUTTONUP;
        }
        if(!available || !sizingMessage)return false;
        POINT pointer{};RECT bounds{};
        if(!GetCursorPos(&pointer) || !GetWindowRect(hwnd,&bounds))return false;
        const float dpi=ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
        const int edge=windowResize?windowResize:windowEdges(typesetFrame(bounds,dpi),pointer,std::max(1L,std::lround(5*dpi)));
        if(!edge)return false;
        if(message==WM_SETCURSOR){
            const bool horizontal=(edge&(windowLeft|windowRight))!=0,vertical=(edge&(windowTop|windowBottom))!=0;
            const bool diagonal=(edge==(windowLeft|windowTop) || edge==(windowRight|windowBottom));
            SetCursor(LoadCursorW(nullptr,horizontal&&vertical?(diagonal?IDC_SIZENWSE:IDC_SIZENESW):horizontal?IDC_SIZEWE:IDC_SIZENS));
            return true;
        }
        if(message==WM_LBUTTONDOWN){
            SetCapture(hwnd);
            if(GetCapture()!=hwnd)return false;
            windowResize=edge;windowResizeStart=pointer;windowResizeRect=bounds;dragging=false;
            return true;
        }
        if(!windowResize || GetCapture()!=hwnd)return false;
        RECT limit{};
        if(windowBounds(limit)){
            const auto sized=resizedWindow(windowResizeRect,limit,{pointer.x-windowResizeStart.x,pointer.y-windowResizeStart.y},
                edge,{std::lround(520*dpi),std::lround(360*dpi)});
            if(!EqualRect(&bounds,&sized))SetWindowPos(hwnd,nullptr,sized.left,sized.top,sized.right-sized.left,sized.bottom-sized.top,
                SWP_NOZORDER|SWP_NOACTIVATE);
        }
        return true;
    }
    void toggleMaximized(){
        RECT limit{};
        if(!windowBounds(limit))return;
        RECT bounds=limit;
        if(maximized){
            auto at=dragPosition(restoredRect,limit,{});
            bounds={at.x,at.y,std::min(limit.right,at.x+restoredRect.right-restoredRect.left),
                             std::min(limit.bottom,at.y+restoredRect.bottom-restoredRect.top)};
        }else if(!GetWindowRect(window,&restoredRect))return;
        if(SetWindowPos(window,nullptr,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,
                        SWP_NOZORDER|SWP_NOACTIVATE)){
            maximized=!maximized;dragging=false;
        }
    }

    void reset(){closeFontSamples();clearPreview();clearProof();*this=MenuToolsPanel{};}
    void open(const char* value){reset();opening=true;mode=value;sendMenuTool("menu.tools.open",Json{{"mode",value}});}
    void traceTypeset(const char* stage)const{
        if(mode=="typeset" && openedAt && !selfTest && !stabilityInspection)
            std::cerr<<"Typeset epoch="<<menuEpoch<<" request="<<request<<" stage="<<stage
                     <<" elapsed_ms="<<GetTickCount64()-openedAt<<std::endl;
    }
    void markEditable(bool available){
        if(mode=="typeset" && available && !firstEditable){firstEditable=true;traceTypeset("editable");}
    }
    void submit(const char* action,Json args) {
        if(!ready || pending)return;
        pending=true;pendingAction=action;if(pendingAction=="text.preview"){previewGeneration=draftGeneration;planId.clear();textMatches.clear();}
        error.clear();sendMenuTool("menu.tools.request",Json{{"request",++request},{"action",action},{"args",args}});
    }
    void regroup(){split=menuSplitPreview(source,grouping[0]?std::string(grouping):menuDefaultGroups(source));}
    void loadFonts(const Json& rows){
        fonts.clear();families.clear();
        if(!rows.is_array())return;
        for(const auto& row:rows)if(row.is_array() && (row.size()==2 || row.size()==3) &&
            std::all_of(row.begin(),row.end(),[](const auto& value){return value.is_string();})){
            const auto& id=row.back().get_ref<const std::string&>();
            if(!id.empty() && id.size()<sizeof(styleValues[0]))
                fonts.push_back({row[0].get<std::string>(),row.size()==3?row[1].get<std::string>():"",id});
        }
        for(const auto& face:fonts)if(families.empty() || families.back().first!=face.family)
            families.emplace_back(face.family,face.id);
    }
    std::string status() const {
        if(pending)return mode=="search"?"正在查找图层…":"正在处理…";
        if(mode!="search" || results.is_null())return "";
        return "找到图层："+std::to_string(results.at("items").size())+" · 已扫描："+std::to_string(results.value("scanned",0))+
            (results.value("truncated",false)?" · 请缩小条件":"");
    }
    void receive(const Json& event) {
        if(event.value("epoch",LPARAM(-1))!=menuEpoch || !IsWindowVisible(window))return;
        if(event.value("type",std::string())=="menu.tools.opened") {
            if(!event.value("ok",true)){reset();active=true;error=event.value("error",std::string("面板未能打开"));requireText(error);return;}
            reset();active=ready=focus=true;mode=event.at("mode").get<std::string>();
            if(mode=="typeset"){
                openedAt=GetTickCount64();
                traceTypeset("opened");
                loadViewSettings(event.value("view_settings",Json::object()));
                styleLibrary=event.value("style_library",Json::object());
            }
            if(mode=="split" || mode=="typeset")submit("text.read",Json::object());
            else changeSearch(ImGui::GetTime()-.18);
            return;
        }
        if(event.value("type",std::string())=="menu.tools.styles"){
            if(active && mode=="typeset"){
                if(event.value("ok",false)){styleLibrary=event.at("styles");styleName[0]='\0';stylePresetError.clear();}
                else stylePresetError=event.value("message","排版预设保存失败");
            }
            return;
        }
        if(event.value("type",std::string())=="menu.tools.preview.result") {
            if(!active || !previewPending || event.value("request",-1)!=previewRequest)return;
            previewPending=false;
            if(results.is_null() || results.value("search_id",std::string())!=previewQuery)return;
            const int selected=previewTarget;
            if(!event.value("ok",false))previewMessage=event.value("error",std::string("预览未能读取"));
            else if(!loadPreview(event.at("result")) && previewMessage.empty())previewMessage="预览像素不可用";
            previewTarget=selected;previewVisible=true;requireText(previewMessage);return;
        }
        if(!active || !pending || event.value("request",-1)!=request || event.value("action",std::string())!=pendingAction)return;
        pending=false;
        if((pendingAction=="text.preview" || pendingAction=="text.proof") && previewGeneration!=draftGeneration)return;
        if(event.value("action",std::string())=="layers.search" && searchDirty)return;
        if(!event.value("ok",false)){
            const auto message=event.value("error",std::string("操作失败"));
            requireText(message);
            if(pendingAction=="text.canvas"){
                const auto state=event.value("canvas_state",std::string());
                canvasState=state=="stale"?CanvasState::stale:state=="limited"?CanvasState::limited:CanvasState::failed;
                if(canvasState==CanvasState::stale){clearPreview();clearProof();canvasTextFrame=false;changedText();}
                canvasMessage=message;
                traceTypeset("background-unavailable");
            }else{
                error=message;
                if(pendingAction=="text.read")traceTypeset("text-unavailable");
            }
            return;
        }
        const auto action=event.value("action",std::string());const auto& result=event.at("result");
        if(action=="layers.search") {
            results=result;
            for(const auto& row:results.at("items")){requireText(row.at("name"));requireText(row.at("document_name"));}
        } else if(action=="text.read") {
            traceTypeset("text-received");
            autoFocusPending=mode=="typeset" && revision.empty();
            showCanvasSource=editingCanvasResult=false;
            clearPreview();
            canvasState=CanvasState::idle;canvasMessage.clear();fallbackDraftActive=false;
            canvasSourceWidth=canvasSourceHeight=0;
            fontRaster.setPhotoshopOwner(GetWindow(window,GW_OWNER));
            vectorObject=result.value("vector_object",false);
            clearProof();proofAvailable=result.value("proof_available",false);
            objectProofAvailable=result.value("object_proof",false);proofAttempt.clear();
            isolatedProofAvailable=result.value("isolated_proof",false) && (proofAvailable || objectProofAvailable);
            maxProofScale=wantedProofScale=1;proofNativeSize={};
            const auto proofSize=result.value("proof_size",Json(nullptr));
            if((proofAvailable || objectProofAvailable) && proofSize.is_array() && proofSize.size()==2
               && proofSize[0].is_number_integer() && proofSize[1].is_number_integer()){
                const int width=proofSize[0].get<int>(),height=proofSize[1].get<int>();
                if(width>0 && height>0 && width<=8192 && height<=8192){
                    proofNativeSize={float(width),float(height)};
                    while(maxProofScale<8 && width*(maxProofScale+1)<=8192 && height*(maxProofScale+1)<=8192
                          && uint64_t(width)*height*(maxProofScale+1)*(maxProofScale+1)<=16777216)++maxProofScale;
                }
            }
            canvasTextFrame=canvasTextActive=false;
            const auto boxSize=result.value("box_size",Json(nullptr));
            sourceBoxSize=draftBoxSize=boxSize.is_array()?boxSize.get<std::array<float,2>>():std::array<float,2>{};draftBoxOffset=0;
            boxShiftSupported=result.value("box_shift_supported",false);
            objectTextFrame=result.value("object_text_frame",Json(nullptr));resizeHandle=0;
            tableObject=result.contains("table");
            tableCanvas=result.value(tableObject?"table_canvas":"table_document",std::array<float,3>{});
            tableAlign=0;
            const auto align=result.value("table_align",std::string());
            for(int index=1;index<IM_ARRAYSIZE(tableAlignKeys);++index)if(align==tableAlignKeys[index])tableAlign=index;
            tableWidth[0]=tableHeight[0]=0;
            tablePosition={.5f,.5f};tablePlaced=tableObject;tableFocus=tableEditing=false;tableCell=-1;tableLayoutGeneration=-1;
            if(tableObject)autoFocusPending=false;
            if(tableObject){
                cells=result.at("table").get<decltype(cells)>();
                for(const auto& row:cells)for(const auto& cell:row)requireText(cell);
                textTab=6;
            }else{
                cells.assign(2,std::vector<std::string>(2));
                if(textTab==6)textTab=0;
            }
            sourceStyles=result.value("styles",Json(nullptr));std::memset(styleValues,0,sizeof(styleValues));
            styleBytes={-1,-1};styleEndOfText=false;styleAnchor.clear();
            fontFilter.Clear();fontsAvailable=result.value("fonts_available",false);fontsRequested=false;
            fontCandidate.clear();fontPreview.clear();
            loadFonts(result.value("fonts",Json::array()));
            scopeSupported=result.value("style_scope_supported",false);
            if(mode!="typeset" || !scopeSupported){styleScope=0;scopeExplicit=false;}
            kerningSupported=proofAvailable && result.value("kerning_supported",false);
            openTypeSupported=proofAvailable && result.value("opentype_supported",false);
            sourceKerns=result.value("kerning_pairs",Json::array());sourceFontRuns=result.value("font_runs",Json::array());
            paragraphIndentsSupported=result.value("paragraph_indents_supported",false);
            extendedTypography=result.value("extended_typography",false);
            eastAsianParagraphSupported=result.value("east_asian_paragraph_supported",false);
            advancedTypography=result.value("advanced_typography",false);textRgb=result.value("text_rgb",false);
            textColorChanged=false;std::fill_n(styleEnums,IM_ARRAYSIZE(styleEnums),0);std::fill_n(styleSwitches,IM_ARRAYSIZE(styleSwitches),0);
            unsigned int rgb=0;
            const auto originalColor=sourceStyles.is_object()?sourceStyles.value("color",std::string()):std::string();
            if(originalColor.size()==7)std::sscanf(originalColor.c_str(),"#%x",&rgb);
            for(int i=0;i<3;++i)textRgbOriginal[i]=textRgbDraft[i]=((rgb>>(16-8*i))&255)/255.f;
            paragraphAlignmentSupported=result.value("paragraph_alignment_supported",false);
            leadingMode=paragraphAlign=0;
            managedList=result.value("list_options",Json(nullptr)).is_object();
            canvasAvailable=result.value("canvas_available",false);
            if((textTab==5 || textTab==7) && !sourceStyles.is_object())textTab=0;
            source=result.at("text").get<std::string>();sourceDisplay=source;std::replace(sourceDisplay.begin(),sourceDisplay.end(),'\r','\n');
            revision=result.at("revision").get<std::string>();layerName=result.at("name").get<std::string>();
            grouping[0]=0;splitLayout=source.find_first_of("\r\n")==std::string::npos?1:0;regroup();requireText(source);requireText(layerName);
            scopedStyles=false;setDraft(sourceDisplay);
            loadStyleRanges(result.value("style_ranges",Json(nullptr)));
            draftList=nullptr;draftListChanged=false;
            if(result.contains("list_options"))restoreList(result.at("list_options"));
            if(result.contains("list_warning")){error=result.at("list_warning").get<std::string>();requireText(error);}
            if(mode=="typeset"){
                if(!tableObject && textTab!=4)textTab=0;
                requestCanvas();
            }
        } else if(action=="text.fonts"){
            loadFonts(result.value("fonts",Json::array()));
        } else if(action=="text.canvas"){
            const auto actualRevision=result.value("revision",std::string());
            if(actualRevision.empty()){
                canvasState=CanvasState::failed;canvasMessage="画布回执缺少文字版本，请重试背景";return;
            }
            if(revision!=actualRevision){revision=actualRevision;changedText();}
            if(!loadCanvas(result)){
                canvasState=CanvasState::failed;canvasMessage="画布图像已过期，请重试背景";
                traceTypeset("background-image-unavailable");
            }
            else{
                canvasState=CanvasState::ready;canvasMessage.clear();traceTypeset("background-received");
                canvasSourceWidth=result.value("source_width",0.f);
                canvasSourceHeight=result.value("source_height",0.f);
                tableLayoutGeneration=-1;
                const auto frame=objectTextFrame.is_object()?objectTextFrame:result.value("text_frame",Json(nullptr));
                canvasTextFrame=frame.is_object();
                if(canvasTextFrame){
                    canvasWrap=frame.at("wrap").get<bool>();
                    textBox=frame.at("box").get<decltype(textBox)>();
                    canvasEm=frame.at("em").get<float>();
                    const auto color=frame.at("color").get<std::array<int,3>>();
                    canvasInk=ImGui::ColorConvertFloat4ToU32({color[0]/255.f,color[1]/255.f,color[2]/255.f,1.f});
                }
            }
        } else if(action=="text.proof"){
            if(result.value("isolated",false)!=(backgroundMode!=0))return;
            if(result.value("table",false)){
                if(!objectProofAvailable || textTab!=6)return;
            }else if(!proofAvailable || result.value("plan_id",std::string())!=planId)return;
            const int receivedScale=result.value("scale",1);
            if(receivedScale!=pendingProofScale)return;
            if(receivedScale==1){
                proofCurrent=loadCanvas(result);
                if(!proofCurrent)error="草稿图像读取失败，请重新预览";
            }else if(loadCanvas(result))sharpProofScale=receivedScale;
            else error="高清草稿图像读取失败";
        } else if(action=="text.preview") {
            textMatches.clear();matchSource.clear();
            for(size_t i=0;draft[i];++i){if(draft[i]=='\r'){matchSource+='\n';if(draft[i+1]=='\n')++i;}else matchSource+=draft[i];}
            std::vector<MenuControls::TextRange> validated;
            const auto matches=result.value("matches",Json::array());int last=0;const int length=int(matchSource.size());
            if(!matches.is_array()){error="匹配范围无效，请重新预览";return;}
            for(const auto& span:matches) {
                if(!span.is_array() || span.size()!=2 || !span[0].is_number_integer() || !span[1].is_number_integer()){error="匹配范围无效，请重新预览";return;}
                const auto start=span[0].get<int64_t>(),end=span[1].get<int64_t>();
                if(start<last || end<=start || end>length || (static_cast<unsigned char>(matchSource[start])&0xc0)==0x80 || (static_cast<unsigned char>(matchSource[end])&0xc0)==0x80){textMatches.clear();error="匹配范围无效，请重新预览";return;}
                validated.emplace_back(int(start),int(end));last=int(end);
            }
            textMatches=std::move(validated);
            resultValue=result.at("value").get<std::string>();
            std::replace(resultValue.begin(),resultValue.end(),'\r','\n');
            resultDraftOrigins=result.value("draft_origins",std::vector<int>());
            if(result.contains("draft_origins")){
                const int originalUnits=int(textUnitPositions(matchSource).size())-2;
                if(resultDraftOrigins.size()+1!=textUnitPositions(resultValue).size() || resultDraftOrigins.empty() ||
                    resultDraftOrigins.front()<0 || resultDraftOrigins.back()!=originalUnits ||
                    !std::is_sorted(resultDraftOrigins.begin(),resultDraftOrigins.end())){
                    planId.clear();resultDraftOrigins.clear();error="结果位置无效，请重新预览";return;
                }
            }
            planId=result.at("plan_id").get<std::string>();
            resultPreview=result.at("preview").get<std::string>();resultKind=result.at("kind").get<std::string>();resultCount=result.at("count").get<int>();requireText(resultPreview);
        } else if(action=="text.table" || result.value("continue_editing",false)){
            source.clear();revision.clear();clearPreview();clearProof();
            if(result.value("continue_editing",false))submit("text.read",Json::object());
            else send("hide");
        } else if(action=="text.split" || action=="text.apply" || action=="text.write" || action=="text.vector") {source.clear();revision.clear();clearPreview();clearProof();changedText();}
    }
    static int tableDimension(const char* field){
        if(!field[0])return 0;
        char* end=nullptr;
        const long value=std::strtol(field,&end,10);
        return end!=field && !*end && value>=1 && value<=30000?int(value):-1;
    }
    void editTableCell(size_t row,size_t column){
        char value[513];
        auto& cell=cells[row][column];
        std::snprintf(value,sizeof(value),"%s",cell.c_str());
        MenuControls::input("##cell","",value,sizeof(value));
        if(ImGui::IsItemEdited()){
            const char* end=value+std::strlen(value);
            const char* limit=value;
            for(int count=0;limit<end && count<128;++count){
                unsigned codepoint=0;
                limit+=ImTextCharFromUtf8(&codepoint,limit,end);
            }
            cell.assign(value,size_t(limit-value));requireText(cell);changedText();
        }
    }
    void selectTableCell(int index,bool edit){
        tableCell=index;tableFocus=tableEditing=edit;
    }
    static ImVec2 tableDropPosition(ImVec2 pointer,ImVec2 origin,ImVec2 travel,float fit){
        return {travel.x>0?std::clamp((pointer.x-origin.x)/fit/travel.x,0.f,1.f):0.f,
                travel.y>0?std::clamp((pointer.y-origin.y)/fit/travel.y,0.f,1.f):0.f};
    }
    void drawTable(float scale){
        if(cells.empty() || cells.front().empty())return;
        const int cols=int(cells.front().size());
        const int rows=int(cells.size());
        const int selectedRow=tableCell<0?rows-1:std::min(tableCell/8,rows-1);
        const int selectedCol=tableCell<0?cols-1:std::min(tableCell%8,cols-1);
        ImGui::BeginDisabled(pending && pendingAction!="text.proof");
        if(!tableObject){
            ImGui::BeginDisabled(tableCanvas[0]<=0 || tableCanvas[1]<=0);
            const char* label=tablePlaced?"拖动表格":"拖放表格";
            if(MenuControls::button(label,scale) && !tablePlaced){
                tablePlaced=true;tablePosition={.5f,.5f};selectTableCell(0,true);
            }
            if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover)){
                ImGui::SetDragDropPayload("TYPESET_TABLE",nullptr,0);
                MenuTextStyle text(MenuTextRole::Control);
                ImGui::TextUnformatted("拖到画布放置表格");
                ImGui::EndDragDropSource();
            }
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))MenuControls::tip("拖到画布放置表格",scale);
            ImGui::EndDisabled();
            if(tableCanvas[0]<=0 || tableCanvas[1]<=0){
                MenuTextStyle text(MenuTextRole::Secondary);
                ImGui::TextWrapped("表格需要 RGB 8 位文档和最新的 Photoshop 插件。");
                ImGui::EndDisabled();return;
            }
        }
        ImGui::BeginDisabled(rows>=12);
        if(MenuControls::iconButton("在下方添加行",95,scale)){
            cells.emplace(cells.begin()+selectedRow+1,cols);
            selectTableCell((selectedRow+1)*8+selectedCol,false);changedText();
        }
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(rows==1);
        if(MenuControls::iconButton("删除行",96,scale)){
            cells.erase(cells.begin()+selectedRow);
            selectTableCell(std::min(selectedRow,rows-2)*8+selectedCol,false);changedText();
        }
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(cols>=8);
        if(MenuControls::iconButton("在右侧添加列",97,scale)){
            for(auto& row:cells)row.emplace(row.begin()+selectedCol+1);
            selectTableCell(selectedRow*8+selectedCol+1,false);changedText();
        }
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(cols==1);
        if(MenuControls::iconButton("删除列",98,scale)){
            for(auto& row:cells)row.erase(row.begin()+selectedCol);
            selectTableCell(selectedRow*8+std::min(selectedCol,cols-2),false);changedText();
        }
        ImGui::EndDisabled();
        const auto dimensions=tableSize();
        const auto widthHint="宽度 · "+std::to_string(std::lround(dimensions.x))+" px";
        const auto heightHint="高度 · "+std::to_string(std::lround(dimensions.y))+" px";
        MenuControls::input("##table-width",widthHint.c_str(),tableWidth,sizeof(tableWidth));
        if(ImGui::IsItemEdited())changedText();
        MenuControls::input("##table-height",heightHint.c_str(),tableHeight,sizeof(tableHeight));
        if(ImGui::IsItemEdited())changedText();
        const char* align[]={"左对齐","居中","右对齐"};
        if(MenuControls::segments("##table-align",align,tableAlign,scale))changedText();
        if(ImGui::BeginTable("##table-cells",int(cells.front().size()),ImGuiTableFlags_ScrollX|ImGuiTableFlags_SizingFixedFit)){
            for(size_t i=0;i<cells.front().size();++i)ImGui::TableSetupColumn("",ImGuiTableColumnFlags_WidthFixed,100*scale);
            for(size_t y=0;y<cells.size();++y){
                ImGui::TableNextRow();
                for(size_t x=0;x<cells[y].size();++x){
                    ImGui::TableSetColumnIndex(int(x));ImGui::PushID(int(y*8+x));
                    editTableCell(y,x);
                    if(ImGui::IsItemActivated())selectTableCell(int(y)*8+int(x),false);
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();
    }
    ImVec2 tableSize()const{
        const float density=tableCanvas[2]>0?tableCanvas[2]/72.f:1.f;
        const float width=tableObject?tableCanvas[0]:std::round(cells.front().size()*110*density);
        const float height=tableObject?tableCanvas[1]:std::round(cells.size()*32*density);
        const auto dimension=[&](const char* field,float automatic,float limit){
            const int specified=tableDimension(field);
            const float requested=std::max(1.f,specified>0?float(specified):automatic);
            return !tableObject && limit>0?std::min(limit,requested):requested;
        };
        return {dimension(tableWidth,width,tableCanvas[0]),dimension(tableHeight,height,tableCanvas[1])};
    }
    void drawTableCanvas(float scale,ImU32 pasteboard,bool available){
        if(cells.empty() || cells.front().empty() || tableCanvas[0]<=0 || tableCanvas[1]<=0)return;
        const auto p=ImGui::GetCursorScreenPos(),extent=ImGui::GetContentRegionAvail();
        if(extent.x<=1 || extent.y<=1)return;
        const ImVec2 end={p.x+extent.x,p.y+extent.y},size=tableSize();
        const ImVec2 document=tableObject?size:ImVec2{tableCanvas[0],tableCanvas[1]};
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##table-canvas",extent);
        const auto& io=ImGui::GetIO();
        const bool over=ImGui::IsItemHovered();
        if(over && available && io.MouseWheel){
            ImGui::SetKeyOwner(ImGuiKey_MouseWheelY,ImGui::GetCurrentWindow()->ID);
            const float zoom=std::clamp(canvasZoom*std::pow(1.2f,io.MouseWheel),.25f,8.f);
            const float shift=1.f-zoom/canvasZoom;
            canvasPan.x+=(io.MousePos.x-p.x-extent.x*.5f-canvasPan.x)*shift;
            canvasPan.y+=(io.MousePos.y-p.y-extent.y*.5f-canvasPan.y)*shift;
            canvasZoom=zoom;changedView();
        }
        if(over && available && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)){
            canvasPanning=true;panGestureStart=io.MousePos;panBefore=canvasPan;
        }
        if(!ImGui::IsMouseDown(ImGuiMouseButton_Middle))canvasPanning=false;
        if(canvasPanning)canvasPan={panBefore.x+io.MousePos.x-panGestureStart.x,panBefore.y+io.MousePos.y-panGestureStart.y};
        const float fit=std::min(extent.x/document.x,extent.y/document.y)*canvasZoom;
        const ImVec2 top={p.x+(extent.x-document.x*fit)*.5f+canvasPan.x,p.y+(extent.y-document.y*fit)*.5f+canvasPan.y};
        const ImVec2 documentEnd={top.x+document.x*fit,top.y+document.y*fit};
        const ImVec2 travel={document.x-size.x,document.y-size.y};
        auto previewPosition=tablePosition;
        bool draggingTable=false;
        if(!tableObject && available && ImRect(top,documentEnd).Contains(io.MousePos) && ImGui::BeginDragDropTarget()){
            if(const auto* payload=ImGui::AcceptDragDropPayload("TYPESET_TABLE",ImGuiDragDropFlags_AcceptPeekOnly)){
                previewPosition=tableDropPosition(io.MousePos,top,travel,fit);
                draggingTable=!payload->IsDelivery();
                if(payload->IsDelivery()){
                    tablePosition=previewPosition;tablePlaced=true;selectTableCell(0,true);
                }
            }
            ImGui::EndDragDropTarget();
        }
        auto& draw=*ImGui::GetWindowDrawList();
        draw.PushClipRect(p,end,true);
        draw.AddRectFilled(p,end,pasteboard);
        const CanvasViewTransform upright({},0);
        const auto page=upright.quad(top,documentEnd);
        transparency.draw(draw,page,device,[&](const ImU32* pixels,ID3D11ShaderResourceView** image){
            return loadPixels(2,2,pixels,image);
        });
        if(thumbnail && !tableObject)draw.AddImage((ImTextureID)(intptr_t)thumbnail,top,documentEnd);
        if(!tablePlaced && !draggingTable){draw.PopClipRect();return;}
        const ImVec2 origin={top.x+previewPosition.x*travel.x*fit,top.y+previewPosition.y*travel.y*fit};
        const ImVec2 cell={size.x/float(cells.front().size()),size.y/float(cells.size())};
        const float density=tableCanvas[2]>0?tableCanvas[2]/72.f:1.f;
        const float rule=std::max(.5f,std::min({density,cell.x/12,cell.y/12}));
        const float inset=std::max(rule,std::min({4*density,cell.x/8,cell.y/8}));
        const auto ink=ImGui::ColorConvertFloat4ToU32({.125f,.125f,.125f,1});
        for(size_t i=0;i<=cells.front().size();++i){
            const float x=origin.x+std::min(i*cell.x,size.x-rule)*fit;
            draw.AddRectFilled({x,origin.y},{x+rule*fit,origin.y+size.y*fit},ink);
        }
        for(size_t i=0;i<=cells.size();++i){
            const float y=origin.y+std::min(i*cell.y,size.y-rule)*fit;
            draw.AddRectFilled({origin.x,y},{origin.x+size.x*fit,y+rule*fit},ink);
        }
        if(draggingTable){
            draw.AddRect(origin,{origin.x+size.x*fit,origin.y+size.y*fit},ImGui::GetColorU32(ImGuiCol_NavCursor),
                0,0,menuControlBorder*scale);
        }else if(tableCell>=0){
            const ImVec2 selected={origin.x+(tableCell%8)*cell.x*fit,origin.y+(tableCell/8)*cell.y*fit};
            draw.AddRect(selected,{selected.x+cell.x*fit,selected.y+cell.y*fit},ImGui::GetColorU32(ImGuiCol_NavCursor),
                0,0,menuControlBorder*scale);
        }
        if(auto* painter=canvasGlyphTarget.prepare(device)){
            if(tableLayoutGeneration!=draftGeneration){
                tableLayoutGeneration=draftGeneration;tableGlyphs.clear();
                const float em=std::min(12*density,cell.y*.55f);
                for(size_t y=0;y<cells.size();++y)for(size_t x=0;x<cells[y].size();++x){
                    const auto& text=cells[y][x];
                    if(text.empty() || cell.x<=2*inset || cell.y<=2*inset)continue;
                    FontSampleRaster::DraftParagraph para;para.text=wideText(text);
                    para.runs.push_back({{0,UINT32(para.text.size())},L"MicrosoftYaHei",em,0,{.125f,.125f,.125f},false,false,false,false,{}});
                    std::vector<FontSampleRaster::DraftLayout> lines;
                    const auto layout=[&]{return fontRaster.layoutDraft({para},cell.x,cell.y,em,false,*painter,lines);};
                    DWRITE_TEXT_METRICS metrics{};
                    if(!layout() || FAILED(lines.front().text->GetMetrics(&metrics)))continue;
                    const float shrink=std::min({1.f,(cell.x-2*inset)/std::max(1.f,metrics.width),
                        (cell.y-2*inset)/std::max(1.f,metrics.height)});
                    if(shrink<1){
                        para.runs.front().em*=shrink*.98f;
                        if(!layout() || FAILED(lines.front().text->GetMetrics(&metrics)))continue;
                    }
                    auto& line=lines.front();
                    line.x=x*cell.x+(tableAlign==1?(cell.x-metrics.width)*.5f:tableAlign==2?cell.x-inset-metrics.width:inset);
                    line.y=y*cell.y+(cell.y-metrics.height)*.5f;
                    tableGlyphs.push_back(std::move(line));
                }
            }
            if(!tableGlyphs.empty())drawGlyphs(draw,upright,origin,{fit,fit},size.x,size.y,tableGlyphs);
        }
        draw.PopClipRect();
        if(draggingTable)return;
        if(over && available && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
            const auto mouse=io.MousePos;
            if(mouse.x>=origin.x && mouse.x<origin.x+size.x*fit && mouse.y>=origin.y && mouse.y<origin.y+size.y*fit){
                selectTableCell(int((mouse.y-origin.y)/(cell.y*fit))*8+int((mouse.x-origin.x)/(cell.x*fit)),true);
            }else selectTableCell(-1,false);
        }
        if(tableEditing && tableCell>=0 && size_t(tableCell/8)<cells.size() &&
           size_t(tableCell%8)<cells.front().size() && cell.x*fit>=menuControlHeight*scale){
            ImGui::PushClipRect(p,end,true);
            const float x=origin.x+(tableCell%8)*cell.x*fit;
            const float y=origin.y+(tableCell/8+.5f)*cell.y*fit-ImGui::GetFrameHeight()*.5f;
            ImGui::SetCursorScreenPos({x,y});ImGui::PushID(tableCell);
            ImGui::BeginDisabled(!available);
            if(tableFocus){ImGui::SetKeyboardFocusHere();tableFocus=false;}
            ImGui::SetNextItemWidth(cell.x*fit);editTableCell(tableCell/8,tableCell%8);
            if(ImGui::IsItemDeactivated())tableEditing=false;
            ImGui::EndDisabled();ImGui::PopID();ImGui::PopClipRect();
        }
    }
    void requestCanvas(){
        if(pending || revision.empty() || canvasState==CanvasState::stale)return;
        canvasState=CanvasState::waiting;canvasMessage.clear();
        submit("text.canvas",Json{{"revision",revision},{"background",!tableObject}});
    }
    bool draftInCanvas()const{
        return mode=="typeset" && textTab!=6 && !revision.empty() && (!thumbnail || fallbackDraftActive);
    }
    bool editingOnCanvas()const{
        return mode=="typeset" && canvasTextFrame && thumbnail && textTab!=6 && !fallbackDraftActive;
    }
    bool drawDraft(float height) {
        ImGui::PushID(int(menuEpoch));
        ImGui::PushID(draftBufferId);
        if(restoreDraftFocus)ImGui::SetKeyboardFocusHere();
        if(MenuControls::multiline("##draft",draft,sizeof(draft),height,draftCallback,this)){
            checkpointDraft(styleAnchor.c_str());changedText();
        }
        const bool active=ImGui::IsItemActive();
        ImGui::PopID();ImGui::PopID();
        return active;
    }
    void maybeProof(bool available,const std::string& key,Json args){
        const auto now=ImGui::GetTime();
        if(!available || pending || resizeHandle || now-std::max(textChanged,canvasInteraction)<(fontPreview.empty()?.35:.12) || composingText())return;
        if(proofAttempt!=key){
            proofAttempt=key;pendingProofScale=1;
        }else if(proofCurrent && wantedProofScale>sharpAttempt && now-proofZoomChanged>=.4){
            pendingProofScale=sharpAttempt=wantedProofScale;args["scale"]=pendingProofScale;
        }else return;
        if(backgroundMode)args["isolated"]=true;
        previewGeneration=draftGeneration;submit("text.proof",std::move(args));
    }
    std::string sourceFont()const{
        if(scopedStyles)return currentStyle("fontPostScriptName");
        const auto whole=sourceStyles.value("fontPostScriptName",std::string("沿用原值"));
        if(sourceDisplay!=draft || sourceFontRuns.empty() || (scopeExplicit&&styleScope==0))return whole;
        const int start=draftUnits(std::min(selectionStart,selectionEnd));
        const int end=draftUnits(std::max(selectionStart,selectionEnd));
        if(start==end)return whole;
        std::string result;
        for(const auto& row:sourceFontRuns)if(row[0].get<int>()<end && row[1].get<int>()>start){
            const auto& name=row[2].get_ref<const std::string&>();
            if(!result.empty() && result!=name)return "混合";
            result=name;
        }
        return result.empty()?whole:result;
    }
    std::wstring chosenFontSample()const {
        const int length=int(std::strlen(draft)),start=std::clamp(std::min(selectionStart,selectionEnd),0,length);
        const int selectionEndByte=std::clamp(std::max(selectionStart,selectionEnd),0,length);
        const int from=start==selectionEndByte?0:start;
        int to=std::min(from+96,start==selectionEndByte?length:selectionEndByte);
        while(to>from && to<length && (static_cast<unsigned char>(draft[to])&0xc0)==0x80)--to;
        const int count=to==from?0:MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,draft+from,to-from,nullptr,0);
        if(count<=0)return L"Aa";
        std::wstring text(count,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,draft+from,to-from,text.data(),count);
        for(auto& ch:text)if(ch<32)ch=L' ';
        const auto first=text.find_first_not_of(L' ');
        return first==std::wstring::npos?L"Aa":text.substr(first);
    }
    void closeFontSamples(){
        if(fontSampleScope){fontSampleWork().cancel(fontSampleScope);fontSampleScope=0;}
    }
    void drawFontSample(const std::string& id,const std::wstring& sample,ImVec2 at,ImVec2 end){
        const int width=int(std::round(end.x-at.x)),height=int(std::round(end.y-at.y));
        const float em=ImGui::GetFontSize();
        if(width<8 || height<8 || !device)return;
        const auto frame=ImGui::GetFrameCount();
        if(frame!=fontSampleFrame){
            fontSampleFrame=frame;fontSampleDrawn=0;
            if(fontSamples.size()>96 || fontSampleText!=sample || fontSampleWidth!=width || fontSampleHeight!=height ||
               fontSampleEm!=em || fontSampleDevice!=device){closeFontSamples();fontSamples.clear();}
            fontSampleText=sample;fontSampleWidth=width;fontSampleHeight=height;fontSampleEm=em;fontSampleDevice=device;
            if(fontSampleScope)fontSampleWork().nextFrame(fontSampleScope,frame);
        }
        auto found=fontSamples.find(id);
        if(found==fontSamples.end() && fontSampleDrawn<3){
            if(!fontSampleScope)fontSampleScope=fontSampleWork().begin();
            auto rendered=fontSampleWork().request(fontSampleScope,frame,id,wideText(id),sample,width,height,em);
            if(!rendered)return;
            ++fontSampleDrawn;
            FontSample image;image.status=rendered->status;
            if(image.status==FontSampleRaster::Result::pixels && !loadPixels(width,height,rendered->pixels.data(),image.image.GetAddressOf()))
                image.status=FontSampleRaster::Result::unavailable;
            found=fontSamples.emplace(id,std::move(image)).first;
        }
        if(found==fontSamples.end())return;
        auto* draw=ImGui::GetWindowDrawList();
        if(found->second.image)draw->AddImage((ImTextureID)(intptr_t)found->second.image.Get(),at,end,{0,0},{1,1},ImGui::GetColorU32(ImGuiCol_Text));
        else {
            draw->PushClipRect(at,end,true);
            draw->AddText(at,ImGui::GetColorU32(ImGuiCol_TextDisabled),found->second.status==FontSampleRaster::Result::missingGlyph?"部分缺字":"仅画布预览");
            draw->PopClipRect();
        }
    }
    void setFont(const std::string& id){
        if(!scopedStyles && scopeSupported && !scopeExplicit && selectionStart!=selectionEnd)styleScope=1;
        std::snprintf(styleValues[0],sizeof(styleValues[0]),"%s",id.c_str());stageStyle(styleKeys[0],id);
    }
    void updateFontPreview(std::string candidate){
        const auto now=ImGui::GetTime();
        if(candidate!=fontCandidate){fontCandidate=std::move(candidate);fontHoverSince=now;}
        if(fontCandidate!=fontPreview && (fontCandidate.empty() || now-fontHoverSince>=.20)){
            fontPreview=fontCandidate;
            if(scopedStyles)localDraftGeneration=-1;
            else changedText();
        }
    }
    void drawManualKerning(float scale){
        MenuTextStyle role(MenuTextRole::Control);
        const int at=cursorPos;
        const auto [first,last]=pairSpan(draft,at);
        if(first<0 || selectionStart!=selectionEnd){
            kerningFieldAt=-1;kerningFieldInvalid=false;
            ImGui::BeginDisabled();
            MenuControls::fieldIcon("手动字偶微调 (1/1000 em)",62,scale,menuCompactGap*scale);
            char empty[1]{};MenuControls::input("##kerning-inactive","光标置于两字之间",empty,sizeof(empty));
            ImGui::EndDisabled();
            return;
        }
        const std::string left(draft+first,draft+at),right(draft+at,draft+last);
        const std::string pair=left+" | "+right;
        const bool flat=mode=="typeset";
        if(!flat)ImGui::TextUnformatted(pair.c_str());
        const auto edit=kerningEdits.find(at);
        Json value=edit==kerningEdits.end()?Json(nullptr):edit->second;
        if(edit==kerningEdits.end() && sourceDisplay==draft){
            const int offset=draftUnits(at);
            for(const auto& row:sourceKerns)if(row[0].get<int>()==offset){value=row[1];break;}
        }
        if(kerningFieldAt!=at){
            kerningFieldAt=at;kerningFieldInvalid=false;
            if(value.is_number_integer())std::snprintf(kerningField,sizeof(kerningField),"%d",value.get<int>());
            else kerningField[0]='\0';
        }
        const auto stage=[&](Json change){
            if(kerningEdits.size()>=256 && edit==kerningEdits.end()){
                error="字偶草稿已达 256 对，请先写回";
                return;
            }
            checkpointDraft();
            kerningAnchor=draft;kerningEdits[at]=std::move(change);changedText();
        };
        ImGui::PushID(at);
        MenuControls::fieldIcon("手动字偶微调 (1/1000 em)",62,scale,menuCompactGap*scale);
        if(flat)MenuControls::propertyInput("##manual-kerning","自动",kerningField,sizeof(kerningField),"‰",scale);
        else MenuControls::input("##manual-kerning","自动",kerningField,sizeof(kerningField));
        if(ImGui::IsItemEdited()){
            char* end=nullptr;
            const long amount=std::strtol(kerningField,&end,10);
            kerningFieldInvalid=end==kerningField || *end!='\0' || amount<-1000 || amount>1000;
            if(!kerningFieldInvalid)stage(int(amount));
            if(!kerningField[0]){
                kerningFieldInvalid=false;
                if(!value.is_null())stage(nullptr);
            }
        }
        if(kerningFieldInvalid){MenuTextStyle hint(MenuTextRole::Secondary);ImGui::TextWrapped("请输入 −1000 到 1000 的整数");}
        if(flat)MenuControls::detailLine(pair.c_str(),scale);
        if(!value.is_null() && MenuControls::iconButton("恢复自动字偶",88,scale)){
            kerningField[0]='\0';kerningFieldInvalid=false;stage(nullptr);
        }
        ImGui::PopID();
    }
    void drawTypography(float scale) {
        if(!sourceStyles.is_object() || revision.empty() || tableObject){closeFontSamples();updateFontPreview({});return;}
        std::string hoveredFont;
        ImGui::BeginDisabled(textTab==3 || textTab==6);
        const char* scopes[]={"整层","文字选区","当前段落"};
        const char* automaticScopes[]={"自动","整层","文字选区","当前段落"};
        const bool flat=mode=="typeset";
        if(scopedStyles){
            int selected=scopeExplicit?styleScope+1:0;
            if(MenuControls::segments("##style-scope",automaticScopes,selected,scale)){
                scopeExplicit=selected!=0;styleScope=std::max(0,selected-1);
                updateFontPreview({});
                changedView();
            }
            refreshStyleEditor();
        }else if(scopeSupported && MenuControls::segments("##style-scope",scopes,styleScope,scale)){scopeExplicit=true;}
        drawStylePresets(scale);
        const auto label=[&](const char* title){ImGui::TextWrapped("%s",title);};
        const auto section=[&](const char* title,bool first=false,bool expanded=false){
            if(!flat)return MenuControls::section(title,first||expanded);
            if(!first || scopeSupported)ImGui::Spacing();
            {
                MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextUnformatted(title);
            }
            ImGui::SameLine();
            const auto at=ImGui::GetCursorScreenPos();
            const float remaining=ImGui::GetContentRegionAvail().x;
            if(remaining>0){
                const float y=(ImGui::GetItemRectMin().y+ImGui::GetItemRectMax().y)*.5f;
                ImGui::GetWindowDrawList()->AddLine({at.x,y},{at.x+remaining,y},
                    ImGui::GetColorU32(ImGuiCol_Border),menuControlBorder*scale);
            }
            ImGui::NewLine();
            return true;
        };
        const auto icon=[&](const char* title,int glyph){MenuControls::fieldIcon(title,glyph,scale,flat?menuCompactGap*scale:-1);};
        const auto original=[&](const char* key){return currentStyle(key);};
        const auto field=[&](int index){
            ImGui::PushID(styleKeys[index]);
            const int glyphs[]={57,59,60,61,63,64,65,66,67,68,69,70};
            const char* units[]={"","pt","pt","‰","pt","%","%","pt","pt","pt","pt","pt"};
            icon(styleLabels[index],glyphs[index]);
            const auto hint=original(styleKeys[index]);
            requireText(hint);
            if(index==leadingField && extendedTypography){
                const char* modes[]={"沿用原行距","固定行距","自动行距"};
                const char* compactModes[]={"沿用","固定","自动"};
                size_t selected=leadingMode;
                ImGui::SetNextItemWidth(-1);
                if(flat?MenuControls::choice("##leading-mode",compactModes,selected):MenuControls::choice("##leading-mode",modes,selected)){
                    leadingMode=int(selected);styleValues[index][0]='\0';
                    stageStyle(styleKeys[index],leadingMode==2?Json("auto"):Json(nullptr));
                }
                if(flat && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))MenuControls::tip(modes[selected],scale);
                if(leadingMode==1)icon("固定行距 (pt)",60);
            }
            if(index!=leadingField || !extendedTypography || leadingMode==1){
                if(flat && index)MenuControls::propertyInput("##style",hint.c_str(),styleValues[index],sizeof(styleValues[index]),units[index],scale);
                else MenuControls::input("##style",hint.c_str(),styleValues[index],sizeof(styleValues[index]));
                if(ImGui::IsItemEdited())stageStyle(styleKeys[index],styleValues[index][0]?Json(styleValues[index]):Json(nullptr));
            }
            ImGui::PopID();
        };
        const auto select=[&](const char* title,const char* key,const auto& choices,int& index,int glyph,bool inlineLabel=false){
            ImGui::PushID(key);
            const auto hint=original(key);
            const char* preview=flat && index==0?hint.c_str():choices[index];
            requireText(preview);
            if(!flat){
                label(title);
                if(index==0){MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("%s",hint.c_str());}
            }
            icon(title,glyph);
            float indent=0;
            if(flat && inlineLabel){
                const float nameWidth=ImGui::CalcTextSize(title).x+menuCompactGap*scale;
                if(ImGui::GetContentRegionAvail().x>=nameWidth+ImGui::CalcTextSize(preview).x+
                    ImGui::GetFrameHeight()+2*ImGui::GetStyle().FramePadding.x)MenuControls::fieldLabel(title,nameWidth);
                else{label(title);indent=(figmaDisplayIconSize+menuCompactGap)*scale;ImGui::Indent(indent);}
            }
            size_t choice=index;
            ImGui::SetNextItemWidth(-1);
            if(MenuControls::choice("##option",choices,choice,preview)){
                index=int(choice);
                Json value=nullptr;
                if(index){
                    const auto it=std::find(std::begin(enumKeys),std::end(enumKeys),key);
                    value=it==std::end(enumKeys)?Json(index==2):Json(enumValues[it-std::begin(enumKeys)][index]);
                }
                stageStyle(key,std::move(value));
            }
            if(flat && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)){
                const char* value=index?choices[index]:hint.c_str();
                MenuControls::tip((std::string(title)+"\n当前值 · "+value).c_str(),scale);
            }
            if(indent)ImGui::Unindent(indent);
            ImGui::PopID();
        };
        const auto pair=[&](const char* id,const auto& left,const auto& right){
            MenuControls::propertyPair(id,scale,flat,left,right);
        };
        const auto unavailableFace=[&](const char* value){
            ImGui::BeginDisabled();
            icon("样式／字重",58);
            MenuControls::detailLine(value,scale);
            ImGui::EndDisabled();
        };
        const char* switches[]={"沿用","关闭","开启"};
        struct StyleToggle {const char* title;const char* key;int glyph;int& draftValue;};
        const auto toggles=[&](const char* id,const auto& rows){
            constexpr int count=std::extent_v<std::remove_reference_t<decltype(rows)>>;
            const bool edited=std::any_of(std::begin(rows),std::end(rows),[](const StyleToggle& row){return row.draftValue!=0;});
            MenuControls::iconGroup<count+1>(id,scale,false,[&](int i)->MenuControls::IconGroupItem{
                if(i==count)return {"恢复本组原值",88,false,edited};
                const auto& row=rows[i];
                std::string value=row.draftValue?switches[row.draftValue]:original(row.key);
                const bool on=value=="开启",known=on || value=="关闭";
                return {row.title,row.glyph,on,true,!known,std::move(value)};
            },[&](int i){
                if(i==count){
                    if(scopedStyles)checkpointDraft();
                    for(const auto& row:rows){
                        if(scopedStyles && row.draftValue)stageStyle(row.key,nullptr,false);
                        row.draftValue=0;
                    }
                    if(!scopedStyles)changedText();
                    return;
                }
                const auto& row=rows[i];
                const auto inherited=scopedStyles?styleDraft.current(row.key,activeStyleRange(false),displayStyle,false):original(row.key);
                const bool on=row.draftValue?row.draftValue==2:original(row.key)=="开启";
                const int next=on?1:2;
                row.draftValue=inherited==switches[next]?0:next;
                if(scopedStyles){
                    Json value=nullptr;
                    if(row.draftValue){
                        const auto it=std::find(std::begin(enumKeys),std::end(enumKeys),row.key);
                        value=it==std::end(enumKeys)?Json(next==2):Json(enumValues[it-std::begin(enumKeys)][next]);
                    }
                    stageStyle(row.key,std::move(value));
                }else if(row.draftValue)changedStyle();
                else changedText();
            });
        };
        if(section("字符",true)){
            if(mode=="typeset" && (fontsAvailable || !fonts.empty())){
                const auto id=styleValues[0][0]?std::string(styleValues[0]):sourceFont();
                const auto face=std::find_if(fonts.begin(),fonts.end(),[&](const auto& row){return row.id==id;});
                icon("字体",57);
                const char* family=face==fonts.end()?(id=="沿用原值"?"浏览 Photoshop 字体":id.c_str()):face->family.c_str();
                requireText(family);
                bool opened=false;
                int familyHover=-1;
                const auto familyFace=[&](int index)->const std::string&{
                    const auto& picked=families[index];
                    auto preferred=std::find_if(fonts.begin(),fonts.end(),[&](const auto& f){return f.family==picked.first &&
                        f.style==(face==fonts.end()?"Regular":face->style);});
                    return preferred==fonts.end()?picked.second:preferred->id;
                };
                std::optional<std::wstring> sample;
                const auto drawSample=[&](const std::string& face,ImVec2 at,ImVec2 end){
                    if(!sample)sample=chosenFontSample();
                    drawFontSample(face,*sample,at,end);
                };
                const int chosen=MenuControls::listChoice("##font-family",family,families,&fontFilter,&opened,&familyHover,
                    [&](int row,ImVec2 at,ImVec2 end){drawSample(familyFace(row),at,end);},scale);
                if(opened && fonts.empty() && !fontsRequested && !pending){
                    fontsRequested=true;submit("text.fonts",Json{{"revision",revision}});
                }
                if(chosen>=0)setFont(familyFace(chosen));
                else if(familyHover>=0 && familyFace(familyHover)!=id)hoveredFont=familyFace(familyHover);
                if(face!=fonts.end()){
                    icon("样式／字重",58);
                    const auto shown=face->style.empty()?face->id:face->style;
                    requireText(shown);
                    std::vector<std::pair<std::string,std::string>> faces;
                    for(const auto& row:fonts)if(row.family==face->family)faces.emplace_back(row.style.empty()?row.id:row.style,row.id);
                    int faceHover=-1;
                    const int picked=MenuControls::listChoice("##font-face",shown.c_str(),faces,nullptr,nullptr,&faceHover,
                        [&](int row,ImVec2 at,ImVec2 end){drawSample(faces[row].second,at,end);},scale);
                    if(picked>=0){
                        setFont(faces[picked].second);
                    }else if(faceHover>=0 && faces[faceHover].second!=id)hoveredFont=faces[faceHover].second;
                }else unavailableFace(id=="混合"?"混合":"沿用原值");
                if(styleValues[0][0] && MenuControls::iconButton("沿用原字体",88,scale)){styleValues[0][0]='\0';stageStyle(styleKeys[0],nullptr);}
            }else if(flat){
                icon("字体",57);
                MenuControls::detailLine(original(styleKeys[0]).c_str(),scale);
                unavailableFace("沿用原值");
            }else field(0);
            pair("##size-leading",[&]{field(1);},[&]{field(2);});
            pair("##kerning-tracking",[&]{
                if(advancedTypography){
                    const char* kern[]={"沿用","度量","视觉"};
                    select("字偶间距",enumKeys[0],kern,styleEnums[0],62);
                }
            },[&]{field(3);});
            if(advancedTypography && textRgb){
                icon("文字颜色",71);
                if(MenuControls::colorSwatch("##文字颜色",textRgbDraft)){
                    textColorChanged=true;
                    stageStyle("color",Json::array({int(std::lround(textRgbDraft[0]*255)),int(std::lround(textRgbDraft[1]*255)),int(std::lround(textRgbDraft[2]*255))}));
                }
                const auto current=original("color");
                if(!textColorChanged){ImGui::SameLine();MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextUnformatted(current.c_str());}
                else{
                    ImGui::SameLine();
                    if(MenuControls::iconButton("沿用原文字颜色",88,scale)){
                        std::copy_n(textRgbOriginal,3,textRgbDraft);textColorChanged=false;stageStyle("color",nullptr);
                        editorStyleRange={-1,-1,-1,-1};
                    }
                }
            }
        }
        if(section("段落",false,true)){
            const bool paragraphEditable=!listParagraphsManaged() && textTab!=4;
            const bool canAlign=extendedTypography && paragraphAlignmentSupported && paragraphEditable;
            const bool canHyphenate=advancedTypography && paragraphIndentsSupported && paragraphEditable;
            if(flat || canAlign){
                ImGui::BeginDisabled(!canAlign);
                if(!flat){label("对齐");MenuControls::detailLine(original("align").c_str(),scale);}
                int selected=paragraphAlign-1;
                const char* basic[]={"左对齐","居中","右对齐"};
                const int basicIcons[]={50,51,52};
                const char* full[]={"左对齐","居中","右对齐","两端对齐，末行左","两端对齐，末行居中","两端对齐，末行右","全部两端对齐"};
                const int fullIcons[]={50,51,52,53,54,55,56};
                const bool changed=advancedTypography && paragraphIndentsSupported
                    ?MenuControls::iconSegments("##align",full,fullIcons,selected,scale)
                    :MenuControls::iconSegments("##align",basic,basicIcons,selected,scale);
                if(changed){paragraphAlign=selected+1;stageStyle("align",alignmentKeys[selected]);}
                if(paragraphAlign && MenuControls::iconButton("沿用原对齐",88,scale)){paragraphAlign=0;stageStyle("align",nullptr);}
                ImGui::EndDisabled();
            }
            if(flat || paragraphIndentsSupported){
                if(flat || paragraphEditable){
                    const auto indent=[&](int index){
                        ImGui::BeginDisabled(!paragraphIndentsSupported || (!paragraphEditable && index!=10));
                        field(index);
                        ImGui::EndDisabled();
                    };
                    pair("##indents",[&]{indent(9);},[&]{indent(10);});indent(11);
                }else field(10);
            }
            pair("##paragraph-spacing",[&]{field(7);},[&]{field(8);});
            if(flat || canHyphenate){
                ImGui::BeginDisabled(!canHyphenate);
                select("自动断字",switchKeys[4],switches,styleSwitches[4],73);
                ImGui::EndDisabled();
            }
        }
        if(scopedStyles && paragraphIndentsSupported && !listParagraphsManaged() && textTab!=4 &&
           MenuControls::section("高级段落排版",false)){
            const auto choice=[&](const char* key,const char* title,const char* const (&values)[3]){
                const auto value=currentStyleOverride(key);
                int selection=value.is_boolean()?(value.get<bool>()?2:1):0;
                select(title,key,values,selection,73,true);
            };
            const char* composers[]={"沿用","逐行","整段"};
            choice("textEveryLineComposer","排版方式",composers);
            choice("hangingRoman","罗马标点悬挂",switches);
            choice("hyphenateCapitalized","断开大写单词",switches);
            if(eastAsianParagraphSupported){
                const auto eastAsian=[&](std::string_view key,const char* title,const auto& values){
                    const auto it=std::find_if(std::begin(enumKeys),std::end(enumKeys),
                        [key](const char* candidate){return key==candidate;});
                    if(it!=std::end(enumKeys))select(title,*it,values,styleEnums[it-std::begin(enumKeys)],73,true);
                };
                const char* kinsoku[]={"沿用","关闭","JIS 弱","JIS 最大"};
                const char* mojikumi[]={"沿用","关闭","组合 1","组合 2","组合 3","组合 4"};
                const char* burasagari[]={"沿用","关闭","常规","强制"};
                const char* order[]={"沿用","优先挤入","优先推出","仅推出"};
                eastAsian("kinsokuSetName","避头尾规则",kinsoku);
                eastAsian("mojiKumiName","日文标点间距",mojikumi);
                ImGui::BeginDisabled(original("kinsokuSetName")=="关闭");
                eastAsian("preferredKinsokuOrder","避头尾方式",order);
                eastAsian("burasagari","东亚标点悬挂",burasagari);
                ImGui::EndDisabled();
            }
            const auto edit=[&](const char* key,const char* title,const char* unit,bool triple=false){
                ImGui::PushID(key);label(title);
                auto& buffer=compositionFields[key];
                const auto hint=original(key);requireText(hint);
                if(*unit)MenuControls::propertyInput("##composition",hint.c_str(),buffer.data(),buffer.size(),unit,scale);
                else MenuControls::input("##composition",hint.c_str(),buffer.data(),buffer.size());
                if(ImGui::IsItemEdited()){
                    Json value=nullptr;
                    bool valid=true;
                    if(buffer[0]){
                        if(triple){
                            float a=0,b=0,c=0;char extra=0;
                            valid=std::sscanf(buffer.data()," %f , %f , %f %c",&a,&b,&c,&extra)==3 &&
                                  std::isfinite(a) && std::isfinite(b) && std::isfinite(c) && a<=b && b<=c;
                            if(valid)value=Json::array({a,b,c});
                        }else value=buffer.data();
                    }
                    if(valid){invalidComposition.erase(key);stageStyle(key,std::move(value));}
                    else invalidComposition.insert(key);
                }
                if(invalidComposition.count(key)){
                    MenuTextStyle note(MenuTextRole::Secondary);
                    ImGui::TextWrapped("输入最小、理想、最大三个值，用逗号隔开");
                }
                ImGui::PopID();
            };
            pair("##hyphen-lengths",[&]{edit("hyphenateWordSize","最短单词","");},[&]{edit("hyphenateLimit","连续断字上限","");});
            pair("##hyphen-sides",[&]{edit("hyphenatePreLength","断字前字符数","");},[&]{edit("hyphenatePostLength","断字后字符数","");});
            edit("hyphenationZone","断字区域","pt");
            edit("justifyWord","词距 · 最小／理想／最大","%",true);
            edit("justifyLetter","字距 · 最小／理想／最大","%",true);
            edit("justifyGlyph","字形缩放 · 最小／理想／最大","%",true);
        }
        if(extendedTypography && section(flat?"进阶字符":"更多字符设置")){
            if(!flat)field(0);
            pair("##scale-pair",[&]{field(5);},[&]{field(6);});field(4);
            if(advancedTypography){
                const char* caps[]={"沿用","正常","大写","小型大写"};
                const char* baseline[]={"沿用","正常","上标","下标"};
                pair("##caps-baseline",[&]{select("大小写",enumKeys[1],caps,styleEnums[1],89);},
                     [&]{select("上下标",enumKeys[2],baseline,styleEnums[2],90);});
                const StyleToggle chars[]={{"仿粗体",switchKeys[0],58,styleSwitches[0]},
                    {"仿斜体",switchKeys[1],93,styleSwitches[1]},{"下划线",enumKeys[3],91,styleEnums[3]},
                    {"删除线",enumKeys[4],92,styleEnums[4]},{"标准连字",switchKeys[2],94,styleSwitches[2]},
                    {"禁止换行",switchKeys[3],74,styleSwitches[3]}};
                if(flat)toggles("##character-switches",chars);
                else for(const auto& item:chars)select(item.title,item.key,switches,item.draftValue,item.glyph);
            }
        }
        if(kerningSupported && textTab==0 && section("手动字偶"))drawManualKerning(scale);
        if(openTypeSupported && section("OpenType")){
            const char* figures[]={"沿用","默认","比例等高","等宽等高","比例旧式","等宽旧式"};
            select("数字样式",enumKeys[5],figures,styleEnums[5],101,true);
            const StyleToggle features[]={{"上下文连字",switchKeys[5],102,styleSwitches[5]},
                {"样式替代",switchKeys[6],103,styleSwitches[6]},{"花体",switchKeys[7],104,styleSwitches[7]},
                {"标题替代",switchKeys[8],105,styleSwitches[8]},{"装饰字形",switchKeys[9],106,styleSwitches[9]},
                {"分数",switchKeys[10],107,styleSwitches[10]},{"序数",switchKeys[11],108,styleSwitches[11]}};
            if(flat)toggles("##opentype-switches",features);
            else for(const auto& item:features)select(item.title,item.key,switches,item.draftValue,item.glyph);
        }
        ImGui::EndDisabled();
        updateFontPreview(mode=="typeset" && (scopedStyles || proofAvailable) && textTab==0?std::move(hoveredFont):std::string());
        if(fontSampleFrame!=ImGui::GetFrameCount())closeFontSamples();
    }
    bool canWriteTable(bool available)const{
        if(!available || canvasState==CanvasState::stale || !tablePlaced || cells.empty() || cells.front().empty() ||
           tableCanvas[0]<=0 || tableCanvas[1]<=0)return false;
        const int width=tableDimension(tableWidth),height=tableDimension(tableHeight);
        const auto size=tableSize();
        return width>=0 && height>=0 && (!width || width<=size.x) && (!height || height<=size.y) &&
            size.x<=30000.f && size.y<=30000.f && size.x*size.y<=64000000.f &&
            (tableObject || (draft==sourceDisplay && !hasStyles() && !draftListChanged));
    }
    bool usesCanvasResult()const{return mode=="typeset" && textTab>=1 && textTab<=4;}
    bool showsCanvasResult()const{return usesCanvasResult() && !showCanvasSource && !editingCanvasResult && !planId.empty() &&
        (textTab==3?resultKind=="split":resultKind=="replace") &&
        (!(canvasTextActive || fallbackDraftActive) || !composingText());}
    void continueTextResult(bool keepTool=false){
        checkpointDraft();
        if(textTab==4){
            const auto params=textParams();
            draftList=nullptr;
            if(listMode!=2){
                draftList=Json::object();
                for(const char* key:{"kind","marker","custom_marker","numbering","start"})draftList[key]=params.at(key);
            }
            draftListChanged=true;
        }
        const auto value=resultValue;
        int caret=0,first=0,last=0;
        if(!resultDraftOrigins.empty()){
            const auto before=textUnitPositions(draft),after=textUnitPositions(value);
            const auto remap=[&](int byte,bool){return mapResultByte(byte,before,after);};
            caret=remap(cursorPos,false);first=remap(selectionStart,false);last=remap(selectionEnd,true);
            if(scopedStyles){
                styleDraft.remap(remap);nextCharacterStyles.clear();nextCharacterAt=-1;
                editorStyleRange={-1,-1,-1,-1};
            }else if(styleBytes[0]>=0){styleBytes={remap(styleBytes[0],false),remap(styleBytes[1],true)};}
            styleAnchor=value;
        }
        setDraft(value);if(!keepTool)textTab=0;
        editingCanvasResult=keepTool;
        cursorPos=caret;selectionStart=first;selectionEnd=last;
        restoreDraftFocus=true;
    }
    void drawTypesetAction(float scale,bool available,bool alignRight=false){
        if(revision.empty())return;
        const bool split=textTab==3,table=textTab==6;
        const char* label=table?(tableObject?"更新表格":"创建独立表格"):
            split?"拆成多个图层":vectorObject?"更新矢量智能对象":"生成矢量智能对象";
        const bool eligible=available && !pending && (table?canWriteTable(available):
            split?!vectorObject && !hasStyles() && !planId.empty() && resultKind=="split":
            fontCandidate.empty() && fontPreview.empty() && canWriteText() && !planId.empty() && resultKind=="replace" &&
                (kerningEdits.empty() || textTab==0));
        if(alignRight){
            const auto w=ImGui::CalcTextSize(label).x+2*ImGui::GetStyle().FramePadding.x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,ImGui::GetContentRegionAvail().x-w));
        }
        ImGui::BeginDisabled(!eligible);
        if(MenuControls::button(label,scale)){
            if(table){
                Json layout={{"align",tableAlignKeys[tableAlign]}};
                if(const int width=tableDimension(tableWidth);width>0)layout["width"]=width;
                if(const int height=tableDimension(tableHeight);height>0)layout["height"]=height;
                Json args={{"revision",revision},{"cells",cells},{"layout",layout}};
                if(!tableObject)args["position"]={tablePosition.x,tablePosition.y};
                submit("text.table",std::move(args));
            }
            else submit(split?"text.apply":"text.vector",Json{{"revision",revision},{"plan_id",planId}});
        }
        ImGui::EndDisabled();
    }
    void drawText(float scale,bool available,bool pinnedAction=false) {
        ImGui::BeginDisabled(pending);
        if(MenuControls::button(revision.empty()?"重新读取文字":"重新读取文字（覆盖草稿）",scale)){
            source.clear();revision.clear();canvasState=CanvasState::idle;canvasMessage.clear();
            clearPreview();clearProof();changedText();submit("text.read",Json::object());
        }
        ImGui::EndDisabled();
        if(revision.empty()){
            MenuTextStyle text(MenuTextRole::Secondary);
            ImGui::TextWrapped("%s",error.empty()?"正在读取文字…":error.c_str());
            return;
        }
        {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("%s",layerName.c_str());}
        const char* tabs[]={"编辑","查找替换","整理","拆分","列表"};
        const char* styleTabs[]={"编辑","查找替换","整理","拆分","列表","排版"};
        if(mode=="typeset"){
            if(!tableObject){
                const char* choices[]={"查找替换","整理","拆分","列表","表格"};
                const int glyphs[]={80,81,82,83,86};
                const int selected=textTab==6?4:textTab-1;
                int tab=selected;
                if(MenuControls::iconSegments("##typeset-tabs",choices,glyphs,tab,scale,false,true)){
                    textTab=tab==selected?0:tab==4?6:tab+1;
                    showCanvasSource=editingCanvasResult=false;
                    changedText();
                }
            }
        }else{
            if(canvasAvailable){
                ImGui::BeginDisabled(pending || draft!=sourceDisplay || hasStyles() || draftListChanged);
                if(MenuControls::button("打开画布排版",scale)){open("typeset");ImGui::EndDisabled();return;}
                ImGui::EndDisabled();
            }
            if(tableObject)return;
            if(sourceStyles.is_object()?MenuControls::segments("##text-tabs",styleTabs,textTab,scale):MenuControls::segments("##text-tabs",tabs,textTab,scale))changedText();
        }
        if(textTab==6){
            if(!error.empty()){
                {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("%s",error.c_str());}
            }
            drawTable(scale);
            if(!pinnedAction)drawTypesetAction(scale,available);
            return;
        }
        if(!editingOnCanvas() && !draftInCanvas()){
            {MenuTextStyle text(MenuTextRole::Body);ImGui::TextUnformatted("编辑文字");}
            drawDraft(110*scale);
            markEditable(available);
        }
        const auto updateTool=[&]{editingCanvasResult=false;changedText();};
        if(textTab==1) {
            const char* modes[]={"普通查找","正则表达式"};if(MenuControls::segments("##replace-mode",modes,replaceMode,scale))updateTool();
            const char* cases[]={"区分大小写","忽略大小写"};if(MenuControls::segments("##replace-case",cases,caseMode,scale))updateTool();
            MenuControls::input("##find","查找内容",find,sizeof(find));if(ImGui::IsItemEdited())updateTool();
            MenuControls::input("##replacement","替换为（可留空）",replacement,sizeof(replacement));if(ImGui::IsItemEdited())updateTool();
            if(replaceMode==1){MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("Python 正则；替换可用 \\1 或 \\g<名称> 引用分组");}
        } else if(textTab==2) {
            const char* tidy[]={"去首尾空白","删空行","合并空格","合并行","大写","小写"};
            if(MenuControls::segments("##tidy",tidy,tidyMode,scale))updateTool();
            if(tidyMode==3){MenuControls::input("##joiner","合并行的连接符（可留空）",joiner,sizeof(joiner));if(ImGui::IsItemEdited())updateTool();}
        } else if(textTab==3) {
            const char* modes[]={"按行分组","分隔符","正则分隔"};if(MenuControls::segments("##split-mode",modes,splitMode,scale))changedText();
            if(splitMode==0){MenuControls::input("##groups","留空自动按行分组；自定义如 1-2,3",grouping,sizeof(grouping));if(ImGui::IsItemEdited())changedText();}
            else {MenuControls::input("##separator",splitMode==1?"分隔符，例如逗号或空格":"正则分隔符，例如 [,，]",separator,sizeof(separator));if(ImGui::IsItemEdited())changedText();}
            const char* layouts[]={"保留位置","逐行排列"};if(MenuControls::segments("##split-layout",layouts,splitLayout,scale))changedText();
            {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("留空按实际换行拆分，最多64组。保留位置需要原有换行边界。");}
        } else if(textTab==4) {
            const char* lists[]={"项目列表","编号列表","移除列表"};const int listGlyphs[]={83,84,85};
            if(MenuControls::iconSegments("##list-mode",lists,listGlyphs,listMode,scale))updateTool();
            if(listMode==0) {
                const char* markers[]={"随层级 • ◦ ▪","•","◦","▪","–","✓","自定义"};if(MenuControls::segments("##list-markers",markers,listMarker,scale))updateTool();
                if(listMarker==6){MenuControls::input("##list-custom","项目符号（一个字符）",listCustom,sizeof(listCustom));if(ImGui::IsItemEdited())updateTool();}
            } else if(listMode==1) {
                const char* styles[]={"1.","1)","(1)","a)","A)","i)","I)","1.1","1.a.i"};if(MenuControls::segments("##list-numbering",styles,listNumbering,scale))updateTool();
                MenuControls::input("##list-start","起始编号 1—9999",listStart,sizeof(listStart));if(ImGui::IsItemEdited())updateTool();
            }
            int indent=-1;const char* actions[]={"减少缩进","增加缩进"};const int indentGlyphs[]={99,100};
            if(MenuControls::iconSegments("##list-indent",actions,indentGlyphs,indent,scale)){
                const int delta=indent==0?-1:1;
                if(showsCanvasResult()){
                    const auto changed=menuIndentLines(draft,selectionStart,selectionEnd,cursorPos,delta);
                    if(!changed.error.empty()){error=changed.error;requireText(error);}
                    else if(changed.text!=draft && changed.text.size()<sizeof(draft)){
                        checkpointDraft();++draftBufferId;
                        std::snprintf(draft,sizeof(draft),"%s",changed.text.c_str());
                        selectionStart=changed.start;selectionEnd=changed.end;cursorPos=changed.cursor;
                        changedText();
                    }
                }else{indentCommand=delta;restoreDraftFocus=true;}
            }
            {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("缩进作用于选中行或当前行，最多6级；Tab增加、Shift+Tab减少。预览自动更新编号，空行保留；每级写入4个空格，写回PS后不自动续号。");}
        }
        if(textTab==5)drawTypography(scale);
        if(available && textDue(ImGui::GetTime())){textDirty=false;submit("text.preview",textParams());}
        if(textTab==1 && !planId.empty()) {
            {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("匹配 %d 处 · 强调色为匹配内容",resultCount);}
            MenuControls::textPreview("##text-matches",matchSource.c_str(),80*scale,textMatches);
        }
        {
            MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("%s",textStatus().c_str());
        }
        const bool showResult=mode!="typeset";
        if(showResult)
            MenuControls::textPreview("##text-result",planId.empty()?draft:resultPreview.c_str(),130*scale,{},textTab==4 && !planId.empty() && listMode!=2);
        const bool processed=textTab!=3 && !planId.empty() && resultKind=="replace";
        const bool typeset=mode=="typeset";
        ImGui::BeginDisabled(!processed || pending);
        if(textTab!=3 && (showResult || usesCanvasResult()) && MenuControls::button("将结果继续编辑",scale))
            continueTextResult(usesCanvasResult());
        ImGui::EndDisabled();
        if(typeset){
            if(!pinnedAction)drawTypesetAction(scale,available);
            return;
        }
        ImGui::BeginDisabled(!available || !fontCandidate.empty() || !fontPreview.empty() || !canWriteText() ||
            (hasStyles() && !processed) || (!kerningEdits.empty() && textTab!=0));
        if(MenuControls::button(vectorObject?"更新矢量文字对象":processed?"写回预览结果":"写回当前文字",scale)){
            if(processed)submit("text.apply",Json{{"revision",revision},{"plan_id",planId}});
            else {Json args={{"revision",revision},{"draft",draft}};if(draftListChanged)args["list_options"]=draftList;submit("text.write",args);}
        }
        if(!vectorObject && textTab!=3 && MenuControls::button("生成矢量智能对象",scale)){
            if(processed)submit("text.vector",Json{{"revision",revision},{"plan_id",planId}});
            else {Json args={{"revision",revision},{"draft",draft}};if(draftListChanged)args["list_options"]=draftList;submit("text.vector",args);}
        }
        ImGui::EndDisabled();
        if(textTab==3) {
            ImGui::BeginDisabled(vectorObject || hasStyles() || planId.empty() || resultKind!="split" || pending);
            if(MenuControls::button("拆成多个图层",scale))submit("text.apply",Json{{"revision",revision},{"plan_id",planId}});
            ImGui::EndDisabled();
        }
    }
    void draw(float scale,ImVec2 center,bool available,bool hostForeground,ImU32 pasteboardColor) {
        canvasCursor=ImGuiMouseCursor_None;
        const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
        const bool typeset=mode=="typeset";
        MenuControls::PanelStyle controls(scale,false,false,typeset);
        MenuControls::IconStyle icons(typeset);
        const float gap=menuControlPadding*scale,h=menuControlHeight*scale;
        const float margin=typeset && maximized?0.f:std::min(menuFloatingInset*scale,
            std::max(0.f,(std::min(size.x,size.y)-1.f)*.5f));
        const float width=std::max(1.f,typeset?size.x-margin*2:std::min((mode=="search"?420:620)*scale,size.x-margin*2));
        const char* kinds[]={"全部","像素","文字","组","形状","智能对象","调整","填充","其他"};
        const float filters=MenuControls::segmentsHeight(kinds,width-2*ImGui::GetStyle().WindowPadding.x,scale);
        const int rows=results.is_null()?0:std::min(8,int(results.at("items").size()));
        const float contentHeight=mode=="search"?(140+30*std::max(0,rows-1))*scale+filters-figmaDisplayWidgetHeight*scale:size.y-margin*2;
        const float height=std::max(1.f,typeset?size.y-margin*2:std::min(contentHeight,size.y-60*scale-margin*2));
        const ImVec2 at={pos.x+margin,pos.y+margin};
        menuShadow::draw(ImGui::GetWindowDrawList(),at,{at.x+width,at.y+height},ImGui::GetStyle().FrameRounding,scale);
        ImGui::SetCursorScreenPos(at);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
        ImGui::BeginChild("##caps-tools",{width,height},ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding,
            typeset?ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse:0);
        ImGui::PopStyleColor();
        if(typeset){
            const char* resize=maximized?"还原":"最大化";
            const float dragWidth=std::max(1.f,ImGui::GetContentRegionAvail().x-2*(h+gap)-.5f);
            const float titleHeight=figmaDisplayWidgetHeight*scale;
            if(MenuControls::windowDragBar("画布排版",titleHeight,dragWidth) && !maximized){
                dragging=true;GetCursorPos(&dragPointer);
            }
            ImGui::SameLine();
            if(MenuControls::iconButton(resize,maximized?49:48,scale,false,titleHeight))toggleMaximized();
            ImGui::SameLine();
            if(MenuControls::iconButton("关闭",41,scale,false,titleHeight)){
                saveViewSettings();send("hide");ImGui::EndChild();reset();return;
            }
            const auto& io=ImGui::GetIO();
            if(scopedStyles && available && hostForeground && !composingText() && (canvasTextActive || fallbackDraftActive || !ImGui::IsAnyItemActive()) &&
               io.KeyCtrl && !io.KeyAlt && !io.KeySuper){
                if(ImGui::IsKeyPressed(ImGuiKey_Z,false))restoreDraft(io.KeyShift);
                else if(!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Y,false))restoreDraft(true);
            }
        }
        if(opening || !ready){ImGui::TextWrapped("%s",error.empty()?"正在准备输入保护…":error.c_str());if(!typeset && MenuControls::button("关闭",scale))send("hide");ImGui::EndChild();return;}
        const bool editableRead=typeset && (pendingAction=="text.canvas" || pendingAction=="text.fonts");
        ImGui::BeginDisabled((pending && mode!="search" && !editableRead && pendingAction!="text.preview" && pendingAction!="text.proof") || !available);
        if(mode=="search") {
            if(focus){ImGui::SetKeyboardFocusHere();focus=false;}
            ImGui::PushID(int(menuEpoch));
            const bool enter=MenuControls::input("##layer","搜索图层…",name,sizeof(name),MenuControls::Emphasis::Neutral);
            ImGui::PopID();
            if(ImGui::IsItemEdited())changeSearch(ImGui::GetTime());
            const char* ids[]={"all","pixel","text","group","shape","smart-object","adjustment","fill","other"};
            const int icons[]={7,35,36,2,37,38,6,1,40};
            if(MenuControls::segments("##types",kinds,kind,scale))changeSearch(ImGui::GetTime()-.18);
            if(enter && !pending)changeSearch(ImGui::GetTime()-.18);
            if(available && searchDue(ImGui::GetTime())) {
                searchDirty=false;submit("layers.search",Json{{"name",name},{"type",ids[kind]}});
            }
            if(!results.is_null()) {
                const float rows=std::max(menuSearchRowHeight*scale,ImGui::GetContentRegionAvail().y-h-gap);
                ImGui::BeginChild("##results",{0,rows},ImGuiChildFlags_None);
                int index=0,hover=-1;bool previewInView=false;ImVec2 anchor{},selectedAnchor{};
                for(const auto& row:results.at("items")) {
                    ImGui::PushID(index);ImGui::BeginDisabled(pending || previewPending);
                    const auto type=row.value("type",std::string("other"));int typeIndex=8;
                    for(int i=0;i<IM_ARRAYSIZE(ids);++i)if(type==ids[i])typeIndex=i;
                    const bool canPreview=type!="adjustment" && (layerPreviewReady && type!="group");
                    const bool textSource=type=="smart-object" && row.value("text_source",false);
                    const bool editable=type=="text" || textSource;
                    bool over=false,previewClick=false,editClick=false;ImVec2 point{};
                    const bool focusClick=MenuControls::resultRow("##result",textSource?36:icons[typeIndex],
                        row.at("name").get_ref<const std::string&>().c_str(),row.at("document_name").get_ref<const std::string&>().c_str(),
                        scale,over,point,&previewClick,canPreview,editable?&editClick:nullptr);
                    if(editClick || focusClick)submit(editClick?"layers.edit-text":"layers.focus",
                        Json{{"search_id",results.at("search_id")},{"index",index}});
                    if(previewClick && !previewPending) {
                        if(previewTarget==index && previewVisible)previewVisible=false;
                        else {
                            clearPreview();previewTarget=index;previewVisible=true;
                            if(!layerPreviewReady)previewMessage="请更新执行插件以读取缩略图";
                            else {
                                previewPending=true;previewQuery=results.at("search_id").get<std::string>();
                                sendMenuTool("menu.tools.preview",Json{{"request",++previewRequest},{"search_id",previewQuery},{"index",index}});
                            }
                        }
                    }
                    if(over){hover=index;anchor=point;}
                    if(canPreview && previewTarget==index){selectedAnchor=point;previewInView=ImGui::IsRectVisible(point,{point.x+menuSearchRowHeight*scale,point.y+menuSearchRowHeight*scale});}
                    ImGui::EndDisabled();ImGui::PopID();++index;
                }
                if(hover!=previewHovered){previewHovered=hover;previewAt=ImGui::GetTime();}
                if(previewVisible && previewTarget>=0 && previewInView)
                    MenuControls::previewTile(thumbnail,thumbnailSize,previewMessage.empty()?"正在生成预览…":previewMessage.c_str(),selectedAnchor,{at.x,at.y,at.x+width,at.y+height},scale);
                else if(hover>=0 && ImGui::GetTime()-previewAt>=.25)MenuControls::tip("点击预览图层",scale);
                ImGui::EndChild();
            }
        } else {
            const bool canIsolate=typeset && editingOnCanvas() && textTab!=3;
            bool focusText=false;
            if(typeset){
                const float iconWidth=figmaDisplayIconWidth*scale;
                const auto toolbarStart=ImGui::GetCursorScreenPos();
                const float toolbarEnd=toolbarStart.x+ImGui::GetContentRegionAvail().x;
                const auto joinToolbar=[&](float width){
                    if(ImGui::GetItemRectMax().x+gap+width<=toolbarEnd)ImGui::SameLine();
                };
                const auto toolbarWidth=[&](const auto& labels){
                    float width=0;
                    for(const auto* label:labels)width+=ImGui::CalcTextSize(label).x+2*ImGui::GetStyle().FramePadding.x;
                    return width;
                };
                ImGui::PushID("##canvas-toolbar");
                const char* zoomLabels[]={"缩小","放大","适合窗口"};const int zoomGlyphs[]={75,76,77};int zoomAction=-1;
                if(MenuControls::iconSegments("##canvas-zoom",zoomLabels,zoomGlyphs,zoomAction,scale,true)){
                    if(zoomAction==2){canvasZoom=1;canvasPan={};}
                    else canvasZoom=std::clamp(zoomAction==0?canvasZoom/1.2f:canvasZoom*1.2f,.25f,8.f);
                    changedView();
                }
                if(canIsolate){
                    joinToolbar(3*iconWidth);
                    const char* backgrounds[]={"文档背景","透明背景","纯色背景"};
                    const int backgroundGlyphs[]={78,79,13};
                    if(MenuControls::iconSegments("##canvas-background",backgrounds,backgroundGlyphs,backgroundMode,scale,true))changedView();
                    if(backgroundMode==2){
                        joinToolbar(ImGui::GetFrameHeight());
                        if(MenuControls::colorSwatch("##canvas-bg-color",backgroundRgb,false))changedView();
                        if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))MenuControls::tip("背景颜色",scale);
                    }
                }
                if(thumbnail && !draftInCanvas() && textTab!=6){
                    ImGui::BeginDisabled(!hostForeground || resizeHandle || composingText());
                    const char* zoomRegion[]={"框选放大"};
                    joinToolbar(toolbarWidth(zoomRegion));
                    MenuControls::textSegments("##canvas-region-zoom",zoomRegion,scale,
                        [&](size_t){return canvasZoomMode;},[&](size_t){
                            canvasZoomMode=!canvasZoomMode;canvasZoomDragging=false;
                        },true);
                    ImGui::EndDisabled();
                }else canvasZoomMode=canvasZoomDragging=false;
                if(editingOnCanvas() && scopedStyles && canvasGlyphTarget.prepare(device)){
                    ImGui::BeginDisabled(resizeHandle || composingText());
                    const char* viewActions[]={"左转","回正","右转","聚焦文字"};
                    joinToolbar(toolbarWidth(viewActions));
                    MenuControls::textSegments("##canvas-orientation",viewActions,scale,[](size_t){return false;},[&](size_t action){
                        if(action==3)focusText=true;
                        else canvasAngle=action==1?0:std::remainder(canvasAngle+(action==0?-90.f:90.f),360.f);
                    },true);
                    ImGui::EndDisabled();
                }else canvasAngle=0;
                const char* historyActions[]={"撤销","重做"};
                joinToolbar(iconWidth+(scopedStyles?toolbarWidth(historyActions)+2*gap:0.f));
                ImGui::BeginGroup();
                if(MenuControls::iconButton("恢复排版视图默认设置",88,scale)){
                    loadViewSettings(Json::object());saveViewSettings(true);
                }
                if(scopedStyles){
                    ImGui::SameLine();
                    ImGui::BeginDisabled(undoDrafts.empty());
                    if(MenuControls::button("撤销",scale))restoreDraft(false);
                    ImGui::EndDisabled();ImGui::SameLine();
                    ImGui::BeginDisabled(redoDrafts.empty());
                    if(MenuControls::button("重做",scale))restoreDraft(true);
                    ImGui::EndDisabled();
                }
                ImGui::EndGroup();
                if(usesCanvasResult()){
                    const char* content[]={"处理结果","原草稿"};
                    joinToolbar(toolbarWidth(content));
                    ImGui::BeginDisabled(!hostForeground || composingText());
                    MenuControls::textSegments("##canvas-text-content",content,scale,[&](size_t i){return i==size_t(showCanvasSource || editingCanvasResult);},
                        [&](size_t i){
                            if(!i && editingCanvasResult)changedText();
                            editingCanvasResult=false;showCanvasSource=i==1;
                            if(showCanvasSource)restoreDraftFocus=true;
                        },true);
                    ImGui::EndDisabled();
                }
                ImGui::PopID();
            }
            const auto body=ImGui::GetContentRegionAvail();
            const float bodyHeight=std::max(h,body.y-(typeset?0.f:h+gap));
            if(typeset){
                const float left=std::floor(std::min(250*scale,body.x*.31f));
                const float right=std::floor(std::min(200*scale,body.x*.29f));
                const float centerWidth=std::max(1.f,body.x-left-right-2*gap);
                const auto origin=ImGui::GetCursorScreenPos();
                for(const float x:{origin.x+left+gap*.5f,origin.x+left+centerWidth+gap*1.5f})
                    ImGui::GetWindowDrawList()->AddLine({x,origin.y},{x,origin.y+bodyHeight},ImGui::GetColorU32(ImGuiCol_Border),menuControlBorder*scale);
                {
                    MenuControls::PropertyRowSpacing spacing(scale);
                    ImGui::BeginChild("##layout-typography",{left,bodyHeight},ImGuiChildFlags_None);
                    drawTypography(scale);
                    ImGui::EndChild();
                }
                ImGui::SameLine();
                ImGui::BeginChild("##layout-canvas",{centerWidth,bodyHeight},
                    ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
                const bool processedCanvas=showsCanvasResult();
                const bool shapedResult=processedCanvas && textTab!=3 && editingOnCanvas() && scopedStyles && canvasGlyphTarget.prepare(device);
                const int shownBackground=canIsolate?backgroundMode:0;
                if(thumbnail && !fallbackDraftActive && textTab!=3 && textTab!=6 && (!processedCanvas || shapedResult)){
                    MenuTextStyle status(MenuTextRole::Secondary);
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("%s · %.0f%%",processedCanvas?"处理结果":"本地草稿",canvasZoom*100);
                    ImGui::SameLine();ImGui::Dummy({0,menuControlHeight*scale});
                }
                if(canvasState==CanvasState::limited || canvasState==CanvasState::failed){
                    ImGui::BeginDisabled(pending || !hostForeground);
                    if(MenuControls::button("重试背景",scale))requestCanvas();
                    ImGui::EndDisabled();
                }
                if(canvasState==CanvasState::waiting || !canvasMessage.empty()){
                    MenuTextStyle status(MenuTextRole::Secondary);
                    ImGui::TextWrapped("%s",canvasState==CanvasState::waiting?"正在读取背景…":canvasMessage.c_str());
                }
                const auto extent=ImGui::GetContentRegionAvail(),p=ImGui::GetCursorScreenPos();
                if(autoFocusPending && available && hostForeground && canvasState!=CanvasState::waiting && !revision.empty()){
                    autoFocusPending=false;
                    if(!ImGui::IsAnyItemActive() && (selfTest || GetForegroundWindow()==window ||
                       (IsWindowVisible(window) && SetForegroundWindow(window) && GetForegroundWindow()==window))){
                        restoreDraftFocus=true;
                        focusText=canIsolate;
                    }
                }
                if(textTab==6){
                    canvasTextActive=fallbackDraftActive=false;
                    drawTableCanvas(scale,pasteboardColor,available && hostForeground && !pending);
                }else if(processedCanvas && !shapedResult){
                    canvasTextActive=fallbackDraftActive=false;
                    MenuControls::textPreview("##canvas-processed-text",resultPreview.c_str(),std::max(h,extent.y-gap),
                        {},textTab==4 && listMode!=2);
                    if(available && hostForeground && ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
                        if(textTab==3){showCanvasSource=true;restoreDraftFocus=true;}
                        else continueTextResult(true);
                    }
                }else if(draftInCanvas()){
                    fallbackDraftActive=drawDraft(std::max(h,extent.y-gap));
                    markEditable(available && hostForeground);
                }else if(thumbnail && extent.x>1 && extent.y>1){
                    if(editingOnCanvas())markEditable(available && hostForeground);
                    const ImVec2 end={p.x+extent.x,p.y+extent.y};
                    const auto& io=ImGui::GetIO();
                    const auto pointer=io.MousePos;
                    const bool hovered=ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows|ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
                        && ImGui::IsMouseHoveringRect(p,end);
                    if(!hostForeground || (canvasZoomMode && !composingText() &&
                       (ImGui::IsKeyPressed(ImGuiKey_Escape,false) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))))
                        canvasZoomMode=canvasZoomDragging=false;
                    const bool zooming=canvasZoomMode;
                    const ImVec2 center={p.x+extent.x*.5f,p.y+extent.y*.5f};
                    const ImVec2 zoomEnd={std::clamp(pointer.x,p.x,end.x),std::clamp(pointer.y,p.y,end.y)};
                    if(zooming && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
                        canvasZoomDragging=true;canvasZoomStart=zoomEnd;
                    }
                    if(zooming && (hovered || canvasZoomDragging))canvasCursor=ImGuiMouseCursor_ResizeAll;
                    if(canvasZoomDragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left)){
                        canvasZoomDragging=false;
                        const float width=std::abs(canvasZoomStart.x-zoomEnd.x),height=std::abs(canvasZoomStart.y-zoomEnd.y);
                        if(ImGui::IsMousePosValid(&pointer) && width>=6*scale && height>=6*scale){
                            const float next=std::clamp(canvasZoom*std::min(extent.x/width,extent.y/height),.25f,8.f);
                            canvasPan={(center.x+canvasPan.x-(canvasZoomStart.x+zoomEnd.x)*.5f)*next/canvasZoom,
                                (center.y+canvasPan.y-(canvasZoomStart.y+zoomEnd.y)*.5f)*next/canvasZoom};
                            canvasZoom=next;canvasZoomMode=false;changedView();
                        }
                    }
                    if(hovered){
                        ImGui::SetKeyOwner(ImGuiKey_MouseWheelY,ImGui::GetCurrentWindow()->ID);
                        if(!zooming && io.MouseWheel && !resizeHandle && io.KeyAlt && editingOnCanvas() && scopedStyles && !composingText()){
                            canvasAngle=std::remainder(canvasAngle+io.MouseWheel*15.f,360.f);
                        }else if(!zooming && io.MouseWheel && !resizeHandle){
                            const float next=std::clamp(canvasZoom*std::pow(1.2f,io.MouseWheel),.25f,8.f);
                            const float shift=1.f-next/canvasZoom;
                            canvasPan.x+=(io.MousePos.x-p.x-extent.x*.5f-canvasPan.x)*shift;
                            canvasPan.y+=(io.MousePos.y-p.y-extent.y*.5f-canvasPan.y)*shift;
                            canvasZoom=next;
                            changedView();
                        }
                    }
                    if(!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle))canvasPanning=false;
                    if(canvasPanning && (ImGui::IsMouseDragging(0,2*scale) || ImGui::IsMouseDragging(2,2*scale))){
                        canvasPan={panBefore.x+io.MousePos.x-panGestureStart.x,panBefore.y+io.MousePos.y-panGestureStart.y};
                    }
                    const auto visibleSize=ImVec2{canvasSourceWidth>0?canvasSourceWidth:thumbnailSize.x,
                        canvasSourceHeight>0?canvasSourceHeight:thumbnailSize.y};
                    const CanvasViewTransform rotation({},canvasAngle);
                    const auto rotatedSize=[&](ImVec2 size){
                        return ImVec2{std::abs(rotation.cosine)*size.x+std::abs(rotation.sine)*size.y,
                            std::abs(rotation.sine)*size.x+std::abs(rotation.cosine)*size.y};
                    };
                    const auto rotatedCanvas=rotatedSize(visibleSize);
                    const float baseFit=std::min(extent.x/rotatedCanvas.x,extent.y/rotatedCanvas.y);
                    if(focusText){
                        const float width=textBox[2]-textBox[0],height=textBox[3]-textBox[1];
                        const float unitsX=sourceBoxSize[0]>0?width/sourceBoxSize[0]:0,unitsY=sourceBoxSize[1]>0?height/sourceBoxSize[1]:0;
                        const auto textSize=rotatedSize({(unitsX?draftBoxSize[0]*unitsX:width)*visibleSize.x,
                            (unitsY?draftBoxSize[1]*unitsY:height)*visibleSize.y});
                        if(textSize.x>0 && textSize.y>0){
                            canvasZoom=std::clamp(.85f*std::min(extent.x/textSize.x,extent.y/textSize.y)/baseFit,.25f,8.f);
                            const auto offset=rotation.apply({(textBox[0]+draftBoxOffset*unitsX+
                                (unitsX?draftBoxSize[0]*unitsX:width)*.5f-.5f)*visibleSize.x*baseFit*canvasZoom,
                                (textBox[1]+(unitsY?draftBoxSize[1]*unitsY:height)*.5f-.5f)*visibleSize.y*baseFit*canvasZoom});
                            canvasPan={-offset.x,-offset.y};
                            changedView();
                        }
                    }
                    const float fit=baseFit*canvasZoom;
                    const ImVec2 imageSize={visibleSize.x*fit,visibleSize.y*fit};
                    const CanvasViewTransform view({center.x+canvasPan.x,center.y+canvasPan.y},canvasAngle);
                    const ImVec2 min={view.center.x-imageSize.x*.5f,view.center.y-imageSize.y*.5f};
                    const auto planePointer=view.unapply(pointer);
                    if(proofNativeSize.x>0){
                        const float demand=std::max(imageSize.x/proofNativeSize.x,imageSize.y/proofNativeSize.y);
                        const int scale=std::min(maxProofScale,demand>4.f?8:demand>2.f?4:demand>1.15f?2:1);
                        if(scale!=wantedProofScale){wantedProofScale=scale;proofZoomChanged=ImGui::GetTime();}
                    }
                    const ImVec2 max={min.x+imageSize.x,min.y+imageSize.y};
                    const ImVec2 inputStart={min.x+textBox[0]*imageSize.x,min.y+textBox[1]*imageSize.y};
                    const ImVec2 inputEnd={min.x+textBox[2]*imageSize.x,min.y+textBox[3]*imageSize.y};
                    ImVec2 textAt=inputStart,textEnd=inputEnd;
                    std::array<ImVec2,5> handles{};
                    const bool resizable=editingOnCanvas() && canvasWrap && sourceBoxSize[0]>0 && sourceBoxSize[1]>0 &&
                        inputEnd.x>inputStart.x && inputEnd.y>inputStart.y && (!pending || pendingAction=="text.preview" || pendingAction=="text.proof");
                    if(!resizable || !ImGui::IsMouseDown(ImGuiMouseButton_Left))resizeHandle=0;
                    const float grip=3.f*scale;
                    int gripHover=0;
                    if(resizable){
                        const ImVec2 pixelsPerUnit={(inputEnd.x-inputStart.x)/sourceBoxSize[0],(inputEnd.y-inputStart.y)/sourceBoxSize[1]};
                        const auto updateHandles=[&]{
                            textAt.x=inputStart.x+draftBoxOffset*pixelsPerUnit.x;
                            textEnd={textAt.x+draftBoxSize[0]*pixelsPerUnit.x,textAt.y+draftBoxSize[1]*pixelsPerUnit.y};
                            handles={{{textEnd.x,(textAt.y+textEnd.y)*.5f},{(textAt.x+textEnd.x)*.5f,textEnd.y},
                                textEnd,{textAt.x,(textAt.y+textEnd.y)*.5f},{textAt.x,textEnd.y}}};
                        };
                        updateHandles();
                        if(resizeHandle && (trackedResize || ImGui::IsMouseDragging(0,2*scale))){
                            if(!trackedResize){checkpointDraft();trackedResize=true;}
                            const auto before=draftBoxSize;const float offsetBefore=draftBoxOffset;
                            const float dx=(planePointer.x-resizeGestureStart.x)/pixelsPerUnit.x;
                            const float dy=(planePointer.y-resizeGestureStart.y)/pixelsPerUnit.y;
                            if(resizeHandle&1)draftBoxSize[0]=std::clamp(resizeStartSize[0]+dx,1.f,100000.f);
                            if(resizeHandle&4){
                                const float right=resizeStartOffset+resizeStartSize[0];
                                draftBoxSize[0]=std::clamp(resizeStartSize[0]-dx,std::max(1.f,right-100000.f),std::min(100000.f,right+100000.f));
                                draftBoxOffset=right-draftBoxSize[0];
                            }
                            if(resizeHandle&2)draftBoxSize[1]=std::clamp(resizeStartSize[1]+dy,1.f,100000.f);
                            if(draftBoxSize!=before || draftBoxOffset!=offsetBefore){updateHandles();changedText();canvasInteraction=ImGui::GetTime();}
                        }
                        if(hovered && !canvasPanning && !zooming){
                            const float hit=2*grip;
                            if(planePointer.y>=textAt.y-hit && planePointer.y<=textEnd.y+hit){
                                if(boxShiftSupported && std::abs(planePointer.x-textAt.x)<=hit)gripHover=4;
                                if(std::abs(planePointer.x-textEnd.x)<=hit && (!gripHover || planePointer.x>=(textAt.x+textEnd.x)*.5f))gripHover=1;
                            }
                            if(planePointer.x>=textAt.x-hit && planePointer.x<=textEnd.x+hit && std::abs(planePointer.y-textEnd.y)<=hit)gripHover|=2;
                        }
                        if(gripHover && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
                            resizeHandle=gripHover;trackedResize=false;
                            resizeGestureStart=planePointer;resizeStartSize=draftBoxSize;resizeStartOffset=draftBoxOffset;
                        }
                    }
                    const bool onText=editingOnCanvas() && ImRect(textAt,textEnd).Contains(planePointer);
                    if(hovered && !resizeHandle && !zooming && (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                       (!onText && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))){
                        canvasPanning=true;panGestureStart=io.MousePos;panBefore=canvasPan;
                    }
                    if((hovered && onText && ImGui::IsMouseDown(ImGuiMouseButton_Left)) || restoreDraftFocus ||
                       (canvasTextActive && (io.InputQueueCharacters.Size || ImGui::IsKeyPressed(ImGuiKey_Backspace) ||
                          ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Enter) ||
                          (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Z) || ImGui::IsKeyPressed(ImGuiKey_Y) ||
                                         ImGui::IsKeyPressed(ImGuiKey_V) || ImGui::IsKeyPressed(ImGuiKey_X))))))
                        canvasInteraction=ImGui::GetTime();
                    auto* draw=ImGui::GetWindowDrawList();
                    draw->PushClipRect(p,end,true);
                    draw->AddRectFilled(p,end,pasteboardColor);
                    const auto canvasPoints=view.quad(min,max),textPoints=view.quad(textAt,textEnd);
                    if(shownBackground==2)draw->AddQuadFilled(canvasPoints[0],canvasPoints[1],canvasPoints[2],canvasPoints[3],
                        ImGui::ColorConvertFloat4ToU32({backgroundRgb[0],backgroundRgb[1],backgroundRgb[2],1.f}));
                    else transparency.draw(*draw,canvasPoints,device,[&](const ImU32* pixels,ID3D11ShaderResourceView** image){
                        return loadPixels(2,2,pixels,image);
                    });
                    if(shownBackground==0)draw->AddImageQuad((ImTextureID)(intptr_t)thumbnail,canvasPoints[0],canvasPoints[1],canvasPoints[2],canvasPoints[3]);
                    if(editingOnCanvas())draw->AddPolyline(textPoints.data(),int(textPoints.size()),ImGui::GetColorU32(ImGuiCol_NavCursor),
                        ImDrawFlags_Closed,menuControlBorder*scale);
                    if(resizable)for(size_t i=0;i<(boxShiftSupported?handles.size():3);++i){
                        const auto pos=view.apply(handles[i]);
                        constexpr int gripMasks[]={1,2,3,4,6};
                        const float radius=grip*((resizeHandle?resizeHandle:gripHover)==gripMasks[i]?1.4f:1.f);
                        draw->AddRectFilled({pos.x-radius,pos.y-radius},{pos.x+radius,pos.y+radius},ImGui::GetColorU32(ImGuiCol_NavCursor));
                    }
                    draw->PopClipRect();
                    ImGui::Dummy(extent);
                    const bool wasTextActive=canvasTextActive;
                    canvasTextActive=false;
                    ImRect rotatedText(textPoints[0],textPoints[0]);
                    for(const auto point:textPoints)rotatedText.Add(point);
                    if(editingOnCanvas() && (rotatedText.Overlaps(ImRect(p,end)) || wasTextActive)
                       && textEnd.x>textAt.x && textEnd.y>textAt.y){
                        ImGui::PushClipRect(p,end,true);
                        const float layoutHeight=canvasSourceHeight>0?canvasSourceHeight:thumbnailSize.y;
                        const float draftEm=canvasEm>0?canvasEm*layoutHeight:menuFont(MenuTextRole::Body)->LegacySize;
                        const float boxWidth=(textBox[2]-textBox[0])*visibleSize.x;
                        const float boxHeight=(textBox[3]-textBox[1])*layoutHeight;
                        const bool styled=layoutLocalDraft(resizable?boxWidth*draftBoxSize[0]/sourceBoxSize[0]:boxWidth,
                            resizable?boxHeight*draftBoxSize[1]/sourceBoxSize[1]:boxHeight,draftEm,processedCanvas);
                        const ImVec2 factor=styled?ImVec2{(textEnd.x-textAt.x)/localDraftWidth,(textEnd.y-textAt.y)/localDraftHeight}:ImVec2{1,1};
                        const auto ink=canvasEm>0?canvasInk:ImGui::GetColorU32(ImGuiCol_Text);
                        if(processedCanvas){
                            canvasPicking=pickFromLayout=false;
                            if(styled){
                                drawCanvasSelection(*draw,view,textAt,factor,ink,false);
                                if(available && hostForeground && hovered && onText && !resizeHandle && !zooming &&
                                   ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
                                    const int byte=canvasByteAt({(planePointer.x-textAt.x)/factor.x,(planePointer.y-textAt.y)/factor.y},resultValue);
                                    continueTextResult(true);
                                    cursorPos=selectionStart=selectionEnd=std::clamp(byte,0,int(std::strlen(draft)));
                                }
                            }
                            else{
                                ImGui::SetCursorScreenPos(p);
                                MenuControls::textPreview("##canvas-result-fallback",resultPreview.c_str(),extent.y,{},textTab==4 && listMode!=2);
                            }
                        }else{
                            const bool rotated=canvasAngle!=0;
                            ImGui::SetCursorScreenPos(rotated?p:textAt);
                            ImGui::PushID(int(menuEpoch));ImGui::PushID(draftBufferId);
                            const float em=canvasEm>0?std::clamp(canvasEm*imageSize.y,4.f,192.f):menuFont(MenuTextRole::Body)->LegacySize;
                            if(!io.MouseDown[0] || resizeHandle || !styled || zooming)canvasPicking=false;
                            if(styled && !resizeHandle && !zooming && hovered && onText && ImGui::IsMouseClicked(0)){
                                canvasPicking=ImGui::GetMouseClickedCount(0)==1;
                                const int at=canvasByteAt({(planePointer.x-textAt.x)/factor.x,(planePointer.y-textAt.y)/factor.y});
                                canvasSelectionAnchor=io.KeyShift?(cursorPos==selectionStart?selectionEnd:selectionStart):at;
                                if(rotated && !composingText()){
                                    selectionStart=canvasSelectionAnchor;selectionEnd=cursorPos=at;
                                    restoreDraftFocus=true;
                                }
                            }
                            pickFromLayout=canvasPicking && !composingText();
                            if(pickFromLayout)canvasPickedByte=canvasByteAt({(planePointer.x-textAt.x)/factor.x,(planePointer.y-textAt.y)/factor.y});
                            if(restoreDraftFocus)ImGui::SetKeyboardFocusHere();
                            if(rotated && !zooming && !onText && hovered && ImGui::IsMouseClicked(0) && wasTextActive && !composingText())
                                ImGui::ClearActiveID();
                            // A rotated text box uses the layout hit-test above. Hide
                            // its backing input's unrotated pointer hit area.
                            if(rotated || zooming)ImGui::GetIO().MousePos={-FLT_MAX,-FLT_MAX};
                            ImGui::BeginDisabled(resizeHandle!=0);
                            if(MenuControls::canvasText("##draft",draft,sizeof(draft),{textEnd.x-textAt.x,textEnd.y-textAt.y},
                                em,ink,canvasWrap,!styled,draftCallback,this,scopedStyles)){
                                checkpointDraft(styleAnchor.c_str());changedText();
                            }
                            canvasTextActive=ImGui::IsItemActive();
                            if(rotated || zooming)ImGui::GetIO().MousePos=pointer;
                            pickFromLayout=false;
                            ImGui::EndDisabled();
                            if(styled){
                                draw->PushClipRect(p,end,true);
                                drawCanvasSelection(*draw,view,textAt,factor,ink,canvasTextActive && localDraftGeneration==draftGeneration);
                                draw->PopClipRect();
                            }
                            ImGui::PopID();ImGui::PopID();
                        }
                        ImGui::PopClipRect();
                    }
                    if(canvasZoomDragging){
                        draw->PushClipRect(p,end,true);
                        const ImVec2 first={std::min(canvasZoomStart.x,zoomEnd.x),std::min(canvasZoomStart.y,zoomEnd.y)};
                        const ImVec2 last={std::max(canvasZoomStart.x,zoomEnd.x),std::max(canvasZoomStart.y,zoomEnd.y)};
                        draw->AddRectFilled(first,last,ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
                        draw->AddRect(first,last,ImGui::GetColorU32(ImGuiCol_NavCursor),0,0,menuControlBorder*scale);
                        draw->PopClipRect();
                    }
                    const int gripCursor=resizeHandle?resizeHandle:gripHover;
                    if(gripCursor)canvasCursor=view.resizeCursor(gripCursor);
                    if(canvasCursor!=ImGuiMouseCursor_None)ImGui::SetMouseCursor(canvasCursor);
                }
                ImGui::EndChild();ImGui::SameLine();
            }
            const bool canEdit=available && (!typeset || hostForeground);
            ImGui::BeginChild("##text-body",{0,bodyHeight},ImGuiChildFlags_None);
            if(typeset)ImGui::BeginChild("##text-options",{0,std::max(h,bodyHeight-h-gap)},ImGuiChildFlags_None);
            drawText(scale,canEdit,typeset);
            if(typeset){ImGui::EndChild();drawTypesetAction(scale,canEdit,true);}
            ImGui::EndChild();
        }
        ImGui::EndDisabled();
        if(typeset && viewDirty && ImGui::GetTime()-lastViewChange>=.25)saveViewSettings();
        if(!typeset && MenuControls::button("关闭",scale))send("hide");
        if(!typeset && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive()) {
            dragging=true;GetCursorPos(&dragPointer);
        }
        if((mode!="typeset" && (!available || pending)) || !ImGui::IsMouseDown(ImGuiMouseButton_Left))dragging=false;
        if(dragging && (typeset || !pending)) {
            POINT pointer{};GetCursorPos(&pointer);
            RECT own{},limit{};GetWindowRect(window,&own);
            if(windowBounds(limit)) {
                const auto at=dragPosition(own,limit,{pointer.x-dragPointer.x,pointer.y-dragPointer.y});
                SetWindowPos(window,nullptr,at.x,at.y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
            }
            dragPointer=pointer;
        }
        ImGui::EndChild();
    }
};
static MenuToolsPanel menuTools;
