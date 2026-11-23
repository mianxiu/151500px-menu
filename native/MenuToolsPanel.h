#include "MenuTextGroups.h"
#include "FontSampleRaster.h"
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
    ImGuiTextFilter fontFilter;
    bool fontsAvailable=false,fontsRequested=false;
    std::string fontCandidate,fontPreview;
    double fontHoverSince=0;
    bool scopeSupported=false,paragraphIndentsSupported=false,paragraphAlignmentSupported=false,extendedTypography=false,managedList=false;
    bool canvasAvailable=false,proofAvailable=false,objectProofAvailable=false,proofCurrent=false;
    bool isolatedProofAvailable=false;
    bool canvasEditAvailable=false,canvasTextFrame=false,canvasTextActive=false,canvasWrap=true,canvasPanning=false,boxShiftSupported=false;
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
    ImU32 canvasInk=0;
    std::string proofAttempt;
    int maxProofScale=1,wantedProofScale=1,sharpAttempt=1,sharpProofScale=1,pendingProofScale=1;
    ImVec2 proofNativeSize{};
    double proofZoomChanged=0;
    double canvasInteraction=0;
    bool tableObject=false;
    std::vector<std::vector<std::string>> cells;
    std::vector<std::vector<std::string>> savedCells;
    float canvasZoom=1;
    ImVec2 canvasPan{},panGestureStart{},panBefore{};
    int styleScope=0,leadingMode=0,paragraphAlign=0,backgroundMode=0;
    inline static constexpr const char* alignmentKeys[]={"left","center","right","justifyLeft","justifyCenter","justifyRight","justifyAll"};
    inline static constexpr const char* enumKeys[]={"autoKern","fontCaps","baseline","underline","strikethrough","figureStyle"};
    inline static constexpr const char* enumValues[][6]={
        {"","metricsKern","opticalKern",""},{"","normal","allCaps","smallCaps"},{"","normal","superScript","subScript"},
        {"","underlineOff","underlineOnLeftInVertical",""},{"","strikethroughOff","xHeightStrikethroughOn",""},
        {"","normal","proportionalLining","tabularLining","proportionalOldStyle","tabularOldStyle"}
    };
    inline static constexpr const char* switchKeys[]={"syntheticBold","syntheticItalic","ligature","noBreak","hyphenate",
        "contextualLigatures","stylisticAlternates","swash","titling","ornaments","fractions","ordinals"};
    int styleEnums[IM_ARRAYSIZE(enumKeys)]{},styleSwitches[IM_ARRAYSIZE(switchKeys)]{};
    bool advancedTypography=false,textRgb=false,textColorChanged=false;
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
    };
    std::vector<DraftState> undoDrafts,redoDrafts;
    DraftState draftState(const char* text)const {
        return {text,styleDraft,kerningEdits,draftBoxSize,draftBoxOffset,nextCharacterStyles,nextCharacterAt};
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
        selectionStart=selectionEnd=cursorPos=std::min(cursorPos,int(state.text.size()));
        while(cursorPos>0 && (static_cast<unsigned char>(draft[cursorPos])&0xc0)==0x80)--cursorPos;
        selectionStart=selectionEnd=lastSelectionStart=lastSelectionEnd=cursorPos;
        ++draftBufferId;restoreDraftFocus=canvasTextActive;
        editorStyleRange={-1,-1,-1,-1};kerningFieldAt=-1;kerningFieldInvalid=false;
        changedText();
    }
    bool viewDirty=false;
    double lastViewChange=0;

    void loadViewSettings(const Json& value) {
        backgroundMode=value.value("background",0);
        const auto rgb=value.value("rgb",std::array<float,3>{1,1,1});
        std::copy(rgb.begin(),rgb.end(),backgroundRgb);
        canvasZoom=value.value("zoom",1.f);canvasPan={};
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

    static bool paragraphProperty(std::string_view key) {
        return key=="align" || key.rfind("hyphenat",0)==0 || key.rfind("justify",0)==0 ||
            key=="hangingRoman" || key=="textEveryLineComposer" || key=="spaceBefore" || key=="spaceAfter" ||
            key=="startIndent" || key=="endIndent" || key=="firstLineIndent";
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
            {"","默认","比例等高","等宽等高","比例旧式","等宽旧式"}};
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
            if(paragraphProperty(key)!=paragraph)continue;
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
                if((key=="color" && !textRgb) || (paragraphProperty(key) && key!="spaceBefore" && key!="spaceAfter" &&
                    ((!paragraphIndentsSupported && key!="align") || !paragraphAlignmentSupported || managedList)))continue;
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
    int textTab=0,replaceMode=0,caseMode=0,tidyMode=0,listMode=0,splitMode=0,splitLayout=0,draftGeneration=0,previewGeneration=0,resultCount=0;
    std::string pendingAction,planId,resultValue,resultPreview,resultKind;
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
    void changedText(){remapKerning();remapStyleRange();proofCurrent=false;proofAttempt.clear();sharpAttempt=sharpProofScale=1;++draftGeneration;planId.clear();resultValue.clear();resultPreview.clear();resultKind.clear();textMatches.clear();matchSource.clear();error.clear();textDirty=true;textChanged=ImGui::GetTime();}
    void changedStyle(){
        styleBytes=selectedStyleBytes(styleScope);styleAnchor=draft;
        styleEndOfText=styleBytes[1]>int(styleAnchor.size());
        if(styleEndOfText)styleBytes[1]=int(styleAnchor.size());
        changedText();
    }
    void setDraft(const std::string& value){std::snprintf(draft,sizeof(draft),"%s",value.c_str());kerningEdits.clear();kerningFieldAt=-1;kerningFieldInvalid=false;++draftBufferId;indentCommand=selectionStart=selectionEnd=lastSelectionStart=lastSelectionEnd=cursorPos=0;restoreDraftFocus=false;changedText();}
    int draftUnits(int bytes)const{return bytes>0?MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,draft,bytes,nullptr,0):0;}
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
    bool textDue(double now)const{return textDirty && active && ready && !pending && !resizeHandle && !revision.empty() && !kerningFieldInvalid && (kerningEdits.empty() || textTab==0) && (textTab!=1 || find[0]) && (textTab!=3 || splitMode==0 || separator[0]) && now-textChanged>=.12;}
    bool canWriteText()const{return !pending && !revision.empty() && !kerningFieldInvalid && invalidComposition.empty() && draft[0];}
    static bool composingText(){
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
        if(textTab==1){args["operation"]="replace";args["find"]=find;args["replacement"]=replacement;args["regex"]=replaceMode==1;args["ignore_case"]=caseMode==1;}
        if(textTab==2){const char* operations[]={"trim","remove-empty","space","join","upper","lower"};args["operation"]=operations[tidyMode];if(tidyMode==3)args["separator"]=joiner;}
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
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> localDraft;
    int localDraftGeneration=-1,localDraftWidth=0,localDraftHeight=0;
    float localDraftEm=0;
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
    bool renderLocalDraft(float width,float height,float em){
        if(!scopedStyles)return false;
        const float ratio=std::min({1.f,1800.f/width,1800.f/height});
        const int w=int(std::ceil(width*ratio)),h=int(std::ceil(height*ratio));
        if(localDraftGeneration==draftGeneration && w==localDraftWidth && h==localDraftHeight && localDraftEm==em)return bool(localDraft);
        localDraftGeneration=draftGeneration;localDraftWidth=w;localDraftHeight=h;localDraftEm=em;localDraft.Reset();
        std::vector<FontSampleRaster::DraftParagraph> paragraphs;
        const int length=int(std::strlen(draft));
        const auto number=[](const std::string& text,float fallback){
            char* end=nullptr;const float n=std::strtof(text.c_str(),&end);
            return end==text.c_str() || *end || !std::isfinite(n)?fallback:n;
        };
        const auto at=[&](const char* key,int byte,bool edited=true){
            return styleDraft.current(key,{byte,byte},displayStyle,edited);
        };
        const float point=em*ratio/std::max(1.f,number(at("size",0,false),12.f));
        const auto hover=activeStyleRange(false);
        std::vector<int> breaks{0,length};
        for(const auto* map:{&styleDraft.source,&styleDraft.edits})for(const auto& [key,runs]:*map){
            if(paragraphProperty(key))continue;
            for(const auto& run:runs){breaks.push_back(std::min(length,run.first));breaks.push_back(std::min(length,run.last));}
        }
        if(!fontPreview.empty()){breaks.push_back(hover[0]);breaks.push_back(std::min(length,hover[1]));}
        std::sort(breaks.begin(),breaks.end());breaks.erase(std::unique(breaks.begin(),breaks.end()),breaks.end());
        struct Feature {const char* key;DWRITE_FONT_FEATURE_TAG tag;};
        static constexpr Feature features[]={
            {"ligature",DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES},{"contextualLigatures",DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES},
            {"stylisticAlternates",DWRITE_FONT_FEATURE_TAG_STYLISTIC_ALTERNATES},{"swash",DWRITE_FONT_FEATURE_TAG_SWASH},
            {"titling",DWRITE_FONT_FEATURE_TAG_TITLING},{"ornaments",static_cast<DWRITE_FONT_FEATURE_TAG>(DWRITE_MAKE_OPENTYPE_TAG('o','r','n','m'))},
            {"fractions",DWRITE_FONT_FEATURE_TAG_FRACTIONS},{"ordinals",DWRITE_FONT_FEATURE_TAG_ORDINALS},
        };
        for(int first=0;first<=length;){
            int last=first;while(last<length && draft[last]!='\n')++last;
            FontSampleRaster::DraftParagraph paragraph;
            paragraph.text=wideText(std::string_view(draft+first,last-first));
            paragraph.left=number(at("startIndent",first),0)*point;
            paragraph.right=number(at("endIndent",first),0)*point;
            paragraph.first=number(at("firstLineIndent",first),0)*point;
            paragraph.before=number(at("spaceBefore",first),0)*point;paragraph.after=number(at("spaceAfter",first),0)*point;
            paragraph.leading=number(at("leading",first),0)*point;
            const auto align=at("align",first);
            if(align=="居中")paragraph.align=DWRITE_TEXT_ALIGNMENT_CENTER;
            else if(align=="右对齐")paragraph.align=DWRITE_TEXT_ALIGNMENT_TRAILING;
            else if(align.find("两端对齐")!=std::string::npos)paragraph.align=DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
            const int firstUnit=draftUnits(first);
            for(int offset=first;offset<last;){
                const int end=std::min(last,*std::upper_bound(breaks.begin(),breaks.end(),offset));
                unsigned int rgb=0;
                const auto color=at("color",offset);
                if(color.size()==7 && color[0]=='#')std::sscanf(color.c_str(),"#%x",&rgb);
                const auto font=!fontPreview.empty() && offset>=hover[0] && offset<hover[1]?fontPreview:at("fontPostScriptName",offset);
                const float size=std::clamp(number(at("size",offset),12.f)*point,1.f,768.f);
                FontSampleRaster::DraftRun run{{UINT32(draftUnits(offset)-firstUnit),UINT32(draftUnits(end)-draftUnits(offset))},
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
        std::vector<unsigned char> pixels;
        return fontRaster.renderDraft(paragraphs,w,h,em*ratio,canvasWrap,pixels,localLayout) &&
            loadPixels(w,h,pixels.data(),localDraft.GetAddressOf());
    }
    int canvasByteAt(ImVec2 point)const{
        const auto row=std::lower_bound(localLayout.begin(),localLayout.end(),point.y,
            [](const auto& para,float y){return para.y+para.height<y;});
        if(row==localLayout.end())return int(std::strlen(draft));
        DWRITE_HIT_TEST_METRICS hit{};BOOL trailing=FALSE,inside=FALSE;
        if(FAILED(row->text->HitTestPoint(point.x-row->x,point.y-row->y,&trailing,&inside,&hit)))return cursorPos;
        const auto unit=row->start+std::min(row->length,hit.textPosition+(trailing?hit.length:0));
        const char* const end=draft+std::strlen(draft);
        const char* byte=draft;
        for(UINT32 next=0;next<unit && byte<end;){
            unsigned int ch=0;
            const int consumed=ImTextCharFromUtf8(&ch,byte,end);
            next+=ch>0xffff?2:1;
            if(!consumed || next>unit)break;
            byte+=consumed;
        }
        return int(byte-draft);
    }
    void drawCanvasSelection(ImDrawList& draw,ImVec2 at,ImVec2 factor,ImU32 ink,bool focus){
        const UINT32 lo=draftUnits(std::min(selectionStart,selectionEnd)),hi=draftUnits(std::max(selectionStart,selectionEnd));
        const auto transform=[&](float x,float y){return ImVec2{at.x+x*factor.x,at.y+y*factor.y};};
        if(focus && hi>lo){
            const auto background=ImGui::GetColorU32(ImGuiCol_TextSelectedBg);
            for(const auto& para:localLayout){
                const UINT32 first=std::max(lo,para.start),last=std::min(hi,para.start+para.length);
                if(first>=last)continue;
                UINT32 count=0;
                para.text->HitTestTextRange(first-para.start,last-first,para.x,para.y,nullptr,0,&count);
                std::vector<DWRITE_HIT_TEST_METRICS> boxes(count);
                if(SUCCEEDED(para.text->HitTestTextRange(first-para.start,last-first,para.x,para.y,boxes.data(),count,&count)))
                    for(const auto& box:boxes)draw.AddRectFilled(transform(box.left,box.top),transform(box.left+box.width,box.top+box.height),background);
            }
        }
        const ImVec2 end=transform(float(localDraftWidth),float(localDraftHeight));
        draw.AddImage((ImTextureID)(intptr_t)localDraft.Get(),at,end);
        if(!focus)return;
        const UINT32 unit=draftUnits(cursorPos);
        const auto row=std::lower_bound(localLayout.begin(),localLayout.end(),unit,
            [](const auto& para,UINT32 at){return para.start+para.length<at;});
        if(row==localLayout.end())return;
        const auto pos=unit-row->start;
        const bool trailing=pos && pos==row->length;
        float x=0,y=0;DWRITE_HIT_TEST_METRICS hit{};
        if(FAILED(row->text->HitTestTextPosition(pos-(trailing?1:0),trailing,&x,&y,&hit)))return;
        const auto top=transform(row->x+x,row->y+y),bottom=transform(row->x+x,row->y+y+hit.height);
        const auto& state=GImGui->InputTextState;
        if(!ImGui::GetIO().ConfigInputTextCursorBlink || state.CursorAnim<=0 || std::fmod(state.CursorAnim,1.2f)<=.8f)
            draw.AddLine(top,bottom,ink);
        if(GImGui->PlatformImeData.WantTextInput){
            GImGui->PlatformImeData.InputPos={std::clamp(top.x,at.x,end.x),std::clamp(top.y,at.y,end.y)};
            GImGui->PlatformImeData.InputLineHeight=bottom.y-top.y;
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
    static RECT resizedWindow(RECT bounds,RECT limit,POINT delta,int edge,SIZE minimum){
        minimum.cx=std::min(minimum.cx,limit.right-limit.left);
        minimum.cy=std::min(minimum.cy,limit.bottom-limit.top);
        if(edge&windowLeft)bounds.left=std::clamp(bounds.left+delta.x,limit.left,std::max(limit.left,bounds.right-minimum.cx));
        if(edge&windowRight)bounds.right=std::clamp(bounds.right+delta.x,std::min(limit.right,bounds.left+minimum.cx),limit.right);
        if(edge&windowTop)bounds.top=std::clamp(bounds.top+delta.y,limit.top,std::max(limit.top,bounds.bottom-minimum.cy));
        if(edge&windowBottom)bounds.bottom=std::clamp(bounds.bottom+delta.y,std::min(limit.bottom,bounds.top+minimum.cy),limit.bottom);
        return bounds;
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
        const int edge=windowResize?windowResize:windowEdges(bounds,pointer,std::max(1L,std::lround(5*dpi)));
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

    void reset(){clearPreview();clearProof();*this=MenuToolsPanel{};}
    void open(const char* value){reset();opening=true;mode=value;sendMenuTool("menu.tools.open",Json{{"mode",value}});}
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
        if(!active || !pending || event.value("request",-1)!=request)return;
        pending=false;
        if((pendingAction=="text.preview" || pendingAction=="text.proof") && previewGeneration!=draftGeneration)return;
        if(event.value("action",std::string())=="layers.search" && searchDirty)return;
        if(!event.value("ok",false)){error=event.value("error",std::string("操作失败"));requireText(error);return;}
        const auto action=event.value("action",std::string());const auto& result=event.at("result");
        if(action=="layers.search") {
            results=result;
            for(const auto& row:results.at("items")){requireText(row.at("name"));requireText(row.at("document_name"));}
        } else if(action=="text.read") {
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
            canvasEditAvailable=result.value("canvas_edit_available",false);
            canvasTextFrame=canvasTextActive=false;
            const auto boxSize=result.value("box_size",Json(nullptr));
            sourceBoxSize=draftBoxSize=boxSize.is_array()?boxSize.get<std::array<float,2>>():std::array<float,2>{};draftBoxOffset=0;
            boxShiftSupported=result.value("box_shift_supported",false);
            objectTextFrame=result.value("object_text_frame",Json(nullptr));resizeHandle=0;
            tableObject=result.contains("table");
            if(tableObject){
                cells=result.at("table").get<decltype(cells)>();
                for(const auto& row:cells)for(const auto& cell:row)requireText(cell);
                textTab=6;
            }else{
                cells.assign(2,std::vector<std::string>(2));
                if(textTab==6)textTab=0;
            }
            savedCells=cells;
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
            advancedTypography=result.value("advanced_typography",false);textRgb=result.value("text_rgb",false);
            textColorChanged=false;std::fill_n(styleEnums,IM_ARRAYSIZE(styleEnums),0);std::fill_n(styleSwitches,IM_ARRAYSIZE(styleSwitches),0);
            unsigned int rgb=0;
            const auto originalColor=sourceStyles.is_object()?sourceStyles.value("color",std::string()):std::string();
            if(originalColor.size()==7)std::sscanf(originalColor.c_str(),"#%x",&rgb);
            for(int i=0;i<3;++i)textRgbOriginal[i]=textRgbDraft[i]=((rgb>>(16-8*i))&255)/255.f;
            paragraphAlignmentSupported=result.value("paragraph_alignment_supported",false);
            leadingMode=paragraphAlign=0;
            managedList=result.contains("list_options");
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
            if(revision!=result.at("revision").get<std::string>()){
                revision=result.at("revision").get<std::string>();changedText();
            }
            if(!loadCanvas(result))error="画布图像已过期，请重新读取";
            else{
                const auto frame=objectTextFrame.is_object()?objectTextFrame:result.value("text_frame",Json(nullptr));
                canvasTextFrame=frame.is_object() || (vectorObject && result.contains("box") && result.at("box").is_array());
                if(canvasTextFrame){
                    canvasWrap=!frame.is_object() || frame.at("wrap").get<bool>();
                    textBox=(frame.is_object()?frame.at("box"):result.at("box")).get<decltype(textBox)>();
                    canvasEm=frame.is_object()?frame.at("em").get<float>():0;
                    const auto color=frame.is_object()?frame.at("color").get<std::array<int,3>>():std::array<int,3>{};
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
            planId=result.at("plan_id").get<std::string>();resultValue=result.at("value").get<std::string>();
            std::replace(resultValue.begin(),resultValue.end(),'\r','\n');
            resultPreview=result.at("preview").get<std::string>();resultKind=result.at("kind").get<std::string>();resultCount=result.at("count").get<int>();requireText(resultPreview);
        } else if(action=="text.table" || result.value("continue_editing",false)){
            source.clear();revision.clear();clearPreview();clearProof();
            if(result.value("continue_editing",false))submit("text.read",Json::object());
            else send("hide");
        } else if(action=="text.split" || action=="text.apply" || action=="text.write" || action=="text.vector") {source.clear();revision.clear();clearPreview();clearProof();changedText();}
    }
    void drawTable(float scale){
        if(cells.empty() || cells.front().empty())return;
        const int cols=int(cells.front().size());
        ImGui::BeginDisabled(pending && pendingAction!="text.proof");
        ImGui::BeginDisabled(cells.size()>=12);
        if(MenuControls::iconButton("添加行",95,scale)){cells.emplace_back(cols);changedText();}
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(cells.size()==1);
        if(MenuControls::iconButton("移除末行",96,scale)){cells.pop_back();changedText();}
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(cols>=8);
        if(MenuControls::iconButton("添加列",97,scale)){for(auto& row:cells)row.emplace_back();changedText();}
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(cols==1);
        if(MenuControls::iconButton("移除末列",98,scale)){for(auto& row:cells)row.pop_back();changedText();}
        ImGui::EndDisabled();
        if(ImGui::BeginTable("##table-cells",int(cells.front().size()),ImGuiTableFlags_ScrollX|ImGuiTableFlags_SizingFixedFit)){
            for(size_t i=0;i<cells.front().size();++i)ImGui::TableSetupColumn("",ImGuiTableColumnFlags_WidthFixed,100*scale);
            for(size_t y=0;y<cells.size();++y){
                ImGui::TableNextRow();
                for(size_t x=0;x<cells[y].size();++x){
                    ImGui::TableSetColumnIndex(int(x));ImGui::PushID(int(y*8+x));
                    char value[513];auto& cell=cells[y][x];std::snprintf(value,sizeof(value),"%s",cell.c_str());
                    MenuControls::input("##cell","",value,sizeof(value));
                    if(ImGui::IsItemEdited()){cell=value;requireText(cell);changedText();}
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();
    }
    void requestCanvas(){
        clearPreview();
        canvasTextFrame=false;resizeHandle=0;
        submit("text.canvas",Json{{"revision",revision},{"background",canvasEditAvailable}});
    }
    bool editingOnCanvas()const{
        return mode=="typeset" && canvasTextFrame && thumbnail && textTab!=6;
    }
    void drawDraft(float height) {
        ImGui::PushID(int(menuEpoch));
        ImGui::PushID(draftBufferId);
        if(restoreDraftFocus)ImGui::SetKeyboardFocusHere();
        if(MenuControls::multiline("##draft",draft,sizeof(draft),height,draftCallback,this)){
            checkpointDraft(styleAnchor.c_str());changedText();
        }
        ImGui::PopID();ImGui::PopID();
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
    void drawFontSample(const std::string& id,const std::wstring& sample,ImVec2 at,ImVec2 end){
        const int width=int(std::round(end.x-at.x)),height=int(std::round(end.y-at.y));
        const float em=ImGui::GetFontSize();
        if(width<8 || height<8 || !device)return;
        const auto frame=ImGui::GetFrameCount();
        if(frame!=fontSampleFrame){
            fontSampleFrame=frame;fontSampleDrawn=0;
            if(fontSamples.size()>96 || fontSampleText!=sample || fontSampleWidth!=width || fontSampleHeight!=height ||
               fontSampleEm!=em || fontSampleDevice!=device)fontSamples.clear();
            fontSampleText=sample;fontSampleWidth=width;fontSampleHeight=height;fontSampleEm=em;fontSampleDevice=device;
        }
        auto found=fontSamples.find(id);
        if(found==fontSamples.end() && fontSampleDrawn<3){
            ++fontSampleDrawn;
            const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,id.data(),int(id.size()),nullptr,0);
            if(count<=0)return;
            std::wstring face(count,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,id.data(),int(id.size()),face.data(),count);
            std::vector<unsigned char> pixels;
            FontSample image;image.status=fontRaster.render(face,sample,width,height,em,pixels);
            if(image.status==FontSampleRaster::Result::pixels && !loadPixels(width,height,pixels.data(),image.image.GetAddressOf()))
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
        if(!sourceStyles.is_object() || revision.empty() || tableObject){updateFontPreview({});return;}
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
            const bool paragraphEditable=!managedList && textTab!=4;
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
        if(scopedStyles && paragraphIndentsSupported && !managedList && textTab!=4 &&
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
    }
    bool canWriteTable(bool available)const{
        return available && (tableObject || (draft==sourceDisplay && !hasStyles() && !draftListChanged));
    }
    bool needsTableProof()const{
        return mode=="typeset" && objectProofAvailable && (!tableObject || cells!=savedCells || wantedProofScale>1 || backgroundMode);
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
            if(table)submit("text.table",Json{{"revision",revision},{"cells",cells}});
            else submit(split?"text.apply":"text.vector",Json{{"revision",revision},{"plan_id",planId}});
        }
        ImGui::EndDisabled();
    }
    void drawText(float scale,bool available,bool pinnedAction=false) {
        ImGui::BeginDisabled(pending);
        if(MenuControls::button("重新读取（覆盖草稿）",scale)){source.clear();revision.clear();changedText();submit("text.read",Json::object());}
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
                const char* choices[]={"编辑","查找替换","整理","拆分","列表","表格"};
                const int glyphs[]={36,80,81,82,83,86};
                int tab=textTab==6?5:textTab;
                if(MenuControls::iconSegments("##typeset-tabs",choices,glyphs,tab,scale,false,true)){
                    textTab=tab==5?6:tab;
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
        if(!editingOnCanvas() && !(mode=="typeset" && !thumbnail && pending && pendingAction=="text.canvas")){
            {MenuTextStyle text(MenuTextRole::Body);ImGui::TextUnformatted("编辑文字");}
            drawDraft(110*scale);
        }
        if(textTab==1) {
            const char* modes[]={"普通查找","正则表达式"};if(MenuControls::segments("##replace-mode",modes,replaceMode,scale))changedText();
            const char* cases[]={"区分大小写","忽略大小写"};if(MenuControls::segments("##replace-case",cases,caseMode,scale))changedText();
            MenuControls::input("##find","查找内容",find,sizeof(find));if(ImGui::IsItemEdited())changedText();
            MenuControls::input("##replacement","替换为（可留空）",replacement,sizeof(replacement));if(ImGui::IsItemEdited())changedText();
            if(replaceMode==1){MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("Python 正则；替换可用 \\1 或 \\g<名称> 引用分组");}
        } else if(textTab==2) {
            const char* tidy[]={"去首尾空白","删空行","合并空格","合并行","大写","小写"};
            if(MenuControls::segments("##tidy",tidy,tidyMode,scale))changedText();
            if(tidyMode==3){MenuControls::input("##joiner","合并行的连接符（可留空）",joiner,sizeof(joiner));if(ImGui::IsItemEdited())changedText();}
        } else if(textTab==3) {
            const char* modes[]={"按行分组","分隔符","正则分隔"};if(MenuControls::segments("##split-mode",modes,splitMode,scale))changedText();
            if(splitMode==0){MenuControls::input("##groups","留空自动按行分组；自定义如 1-2,3",grouping,sizeof(grouping));if(ImGui::IsItemEdited())changedText();}
            else {MenuControls::input("##separator",splitMode==1?"分隔符，例如逗号或空格":"正则分隔符，例如 [,，]",separator,sizeof(separator));if(ImGui::IsItemEdited())changedText();}
            const char* layouts[]={"保留位置","逐行排列"};if(MenuControls::segments("##split-layout",layouts,splitLayout,scale))changedText();
            {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("留空按实际换行拆分，最多64组。保留位置需要原有换行边界。");}
        } else if(textTab==4) {
            const char* lists[]={"项目列表","编号列表","移除列表"};const int listGlyphs[]={83,84,85};
            if(MenuControls::iconSegments("##list-mode",lists,listGlyphs,listMode,scale))changedText();
            if(listMode==0) {
                const char* markers[]={"随层级 • ◦ ▪","•","◦","▪","–","✓","自定义"};if(MenuControls::segments("##list-markers",markers,listMarker,scale))changedText();
                if(listMarker==6){MenuControls::input("##list-custom","项目符号（一个字符）",listCustom,sizeof(listCustom));if(ImGui::IsItemEdited())changedText();}
            } else if(listMode==1) {
                const char* styles[]={"1.","1)","(1)","a)","A)","i)","I)","1.1","1.a.i"};if(MenuControls::segments("##list-numbering",styles,listNumbering,scale))changedText();
                MenuControls::input("##list-start","起始编号 1—9999",listStart,sizeof(listStart));if(ImGui::IsItemEdited())changedText();
            }
            int indent=-1;const char* actions[]={"减少缩进","增加缩进"};const int indentGlyphs[]={99,100};
            if(MenuControls::iconSegments("##list-indent",actions,indentGlyphs,indent,scale)){indentCommand=indent==0?-1:1;restoreDraftFocus=true;}
            {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("缩进作用于选中行或当前行，最多6级；Tab增加、Shift+Tab减少。预览自动更新编号，空行保留；每级写入4个空格，写回PS后不自动续号。");}
        }
        if(textTab==5)drawTypography(scale);
        if(available && textDue(ImGui::GetTime())){textDirty=false;submit("text.preview",textParams());}
        if(textTab==1 && !planId.empty()) {
            {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("匹配 %d 处 · 强调色为匹配内容",resultCount);}
            MenuControls::textPreview("##text-matches",matchSource.c_str(),80*scale,textMatches);
        }
        const bool resultOnCanvas=mode=="typeset" && proofAvailable && thumbnail && textTab!=3;
        if(!resultOnCanvas || proofAttempt.empty() || proofCurrent || error.empty()){
            MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("%s",textStatus().c_str());
        }
        if(!resultOnCanvas && (!editingOnCanvas() || (textTab>=1 && textTab<=4)))
            MenuControls::textPreview("##text-result",planId.empty()?draft:resultPreview.c_str(),130*scale,{},textTab==4 && !planId.empty() && listMode!=2);
        const bool processed=textTab!=3 && !planId.empty() && resultKind=="replace";
        const bool typeset=mode=="typeset";
        ImGui::BeginDisabled(planId.empty() || pending);
        if(textTab!=3 && (!editingOnCanvas() || (textTab>=1 && textTab<=4)) && MenuControls::button("将结果继续编辑",scale)){
            checkpointDraft();
            if(textTab==4){const auto params=textParams();draftList=nullptr;if(listMode!=2){draftList=Json::object();for(const char* key:{"kind","marker","custom_marker","numbering","start"})draftList[key]=params.at(key);}draftListChanged=true;}
            setDraft(resultValue);textTab=0;
        }
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
    void draw(float scale,ImVec2 center,bool available,bool hostForeground) {
        const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
        const bool typeset=mode=="typeset";
        MenuControls::PanelStyle controls(scale,false,false,typeset);
        const float gap=menuControlPadding*scale,margin=typeset?0:menuFloatingInset*scale,h=menuControlHeight*scale;
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
            if(scopedStyles && available && hostForeground && !composingText() && (canvasTextActive || !ImGui::IsAnyItemActive()) &&
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
                    bool over=false,previewClick=false;ImVec2 point{};
                    if(MenuControls::resultRow("##result",icons[typeIndex],row.at("name").get_ref<const std::string&>().c_str(),row.at("document_name").get_ref<const std::string&>().c_str(),scale,over,point,&previewClick,canPreview))
                        submit("layers.focus",Json{{"search_id",results.at("search_id")},{"index",index}});
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
                const bool canIsolate=thumbnail && canvasTextFrame && textTab!=3 && textTab!=6;
                const float iconWidth=figmaDisplayIconWidth*scale;
                const bool joined=canIsolate && ImGui::GetContentRegionAvail().x>=7*iconWidth+2*gap;
                const char* zoomLabels[]={"缩小","放大","适合窗口"};const int zoomGlyphs[]={75,76,77};int zoomAction=-1;
                if(MenuControls::iconSegments("##canvas-zoom",zoomLabels,zoomGlyphs,zoomAction,scale,true)){
                    if(zoomAction==2){canvasZoom=1;canvasPan={};}
                    else canvasZoom=std::clamp(zoomAction==0?canvasZoom/1.2f:canvasZoom*1.2f,.25f,8.f);
                    changedView();
                }
                if(canIsolate){
                    if(joined)ImGui::SameLine();
                    const char* backgrounds[]={"文档背景","透明背景","纯色背景"};
                    const int backgroundGlyphs[]={78,79,13};
                    const bool swatchFits=ImGui::GetContentRegionAvail().x>=4*iconWidth+gap;
                    if(MenuControls::iconSegments("##canvas-background",backgrounds,backgroundGlyphs,backgroundMode,scale,true))changedView();
                    if(backgroundMode==2){
                        if(swatchFits)ImGui::SameLine();
                        if(MenuControls::colorSwatch("##canvas-bg-color",backgroundRgb,false))changedView();
                        if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))MenuControls::tip("背景颜色",scale);
                    }
                }
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
                if(thumbnail && textTab!=3 && textTab!=6){
                    MenuTextStyle status(MenuTextRole::Secondary);
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("本地草稿 · %.0f%%",canvasZoom*100);
                    ImGui::SameLine();ImGui::Dummy({0,menuControlHeight*scale});
                }
                const int shownBackground=canIsolate?backgroundMode:0;
                const auto extent=ImGui::GetContentRegionAvail(),p=ImGui::GetCursorScreenPos();
                if(!thumbnail && pending && pendingAction=="text.canvas" && textTab!=6 && !revision.empty())
                    drawDraft(std::max(h,extent.y-gap));
                if(thumbnail && extent.x>1 && extent.y>1){
                    const ImVec2 end={p.x+extent.x,p.y+extent.y};
                    const auto& io=ImGui::GetIO();
                    const bool hovered=ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows|ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
                        && ImGui::IsMouseHoveringRect(p,end);
                    if(hovered){
                        ImGui::SetKeyOwner(ImGuiKey_MouseWheelY,ImGui::GetCurrentWindow()->ID);
                        if(io.MouseWheel && !resizeHandle){
                            canvasZoom=std::clamp(canvasZoom*std::pow(1.2f,io.MouseWheel),.25f,8.f);
                            changedView();
                        }
                    }
                    if(!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle))canvasPanning=false;
                    if(canvasPanning && (ImGui::IsMouseDragging(0,2*scale) || ImGui::IsMouseDragging(2,2*scale))){
                        canvasPan={panBefore.x+io.MousePos.x-panGestureStart.x,panBefore.y+io.MousePos.y-panGestureStart.y};
                    }
                    const float fit=std::min(extent.x/thumbnailSize.x,extent.y/thumbnailSize.y)*canvasZoom;
                    const ImVec2 imageSize={thumbnailSize.x*fit,thumbnailSize.y*fit},min={p.x+(extent.x-imageSize.x)*.5f+canvasPan.x,p.y+(extent.y-imageSize.y)*.5f+canvasPan.y};
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
                            const float dx=(io.MousePos.x-resizeGestureStart.x)/pixelsPerUnit.x;
                            const float dy=(io.MousePos.y-resizeGestureStart.y)/pixelsPerUnit.y;
                            if(resizeHandle&1)draftBoxSize[0]=std::clamp(resizeStartSize[0]+dx,1.f,100000.f);
                            if(resizeHandle&4){
                                const float right=resizeStartOffset+resizeStartSize[0];
                                draftBoxSize[0]=std::clamp(resizeStartSize[0]-dx,std::max(1.f,right-100000.f),std::min(100000.f,right+100000.f));
                                draftBoxOffset=right-draftBoxSize[0];
                            }
                            if(resizeHandle&2)draftBoxSize[1]=std::clamp(resizeStartSize[1]+dy,1.f,100000.f);
                            if(draftBoxSize!=before || draftBoxOffset!=offsetBefore){updateHandles();changedText();canvasInteraction=ImGui::GetTime();}
                        }
                        int gripHover=0;
                        if(hovered && !canvasPanning){
                            const float hit=2*grip;
                            if(io.MousePos.y>=textAt.y-hit && io.MousePos.y<=textEnd.y+hit){
                                if(boxShiftSupported && std::abs(io.MousePos.x-textAt.x)<=hit)gripHover=4;
                                if(std::abs(io.MousePos.x-textEnd.x)<=hit && (!gripHover || io.MousePos.x>=(textAt.x+textEnd.x)*.5f))gripHover=1;
                            }
                            if(io.MousePos.x>=textAt.x-hit && io.MousePos.x<=textEnd.x+hit && std::abs(io.MousePos.y-textEnd.y)<=hit)gripHover|=2;
                        }
                        if(gripHover && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
                            resizeHandle=gripHover;trackedResize=false;
                            resizeGestureStart=io.MousePos;resizeStartSize=draftBoxSize;resizeStartOffset=draftBoxOffset;
                        }
                        const int cursor=resizeHandle?resizeHandle:gripHover;
                        if(cursor)ImGui::SetMouseCursor(cursor==3?ImGuiMouseCursor_ResizeNWSE:cursor==6?ImGuiMouseCursor_ResizeNESW:
                            cursor==2?ImGuiMouseCursor_ResizeNS:ImGuiMouseCursor_ResizeEW);
                    }
                    const bool onText=editingOnCanvas() && ImGui::IsMouseHoveringRect(textAt,textEnd);
                    if(hovered && !resizeHandle && (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
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
                    const ImVec2 clipMin={std::max(p.x,min.x),std::max(p.y,min.y)},clipMax={std::min(end.x,max.x),std::min(end.y,max.y)};
                    if(clipMin.x<clipMax.x && clipMin.y<clipMax.y){
                        if(shownBackground==2)draw->AddRectFilled(clipMin,clipMax,ImGui::ColorConvertFloat4ToU32({backgroundRgb[0],backgroundRgb[1],backgroundRgb[2],1.f}));
                        else ImGui::RenderColorRectWithAlphaCheckerboard(draw,clipMin,clipMax,0,2*menuPanelGap*scale,{0,0});
                    }
                    if(shownBackground==0)draw->AddImage((ImTextureID)(intptr_t)thumbnail,min,max);
                    if(editingOnCanvas())
                        draw->AddRect(textAt,textEnd,ImGui::GetColorU32(ImGuiCol_NavCursor),0,0,menuControlBorder*scale);
                    if(resizable)for(size_t i=0;i<(boxShiftSupported?handles.size():3);++i){
                        const auto pos=handles[i];
                        draw->AddRectFilled({pos.x-grip,pos.y-grip},{pos.x+grip,pos.y+grip},ImGui::GetColorU32(ImGuiCol_NavCursor));
                    }
                    draw->PopClipRect();
                    ImGui::Dummy(extent);
                    canvasTextActive=false;
                    if(editingOnCanvas() && textEnd.x>p.x && textEnd.y>p.y && textAt.x<end.x && textAt.y<end.y
                       && textEnd.x-textAt.x>=1 && textEnd.y-textAt.y>=1){
                        ImGui::PushClipRect(p,end,true);
                        ImGui::SetCursorScreenPos(textAt);
                        ImGui::PushID(int(menuEpoch));ImGui::PushID(draftBufferId);
                        if(restoreDraftFocus)ImGui::SetKeyboardFocusHere();
                        const float em=canvasEm>0?std::clamp(canvasEm*imageSize.y,4.f,192.f):menuFont(MenuTextRole::Body)->LegacySize;
                        const auto ink=canvasEm>0?canvasInk:ImGui::GetColorU32(ImGuiCol_Text);
                        const bool styled=renderLocalDraft(textEnd.x-textAt.x,textEnd.y-textAt.y,em);
                        const ImVec2 factor=styled?ImVec2{(textEnd.x-textAt.x)/localDraftWidth,(textEnd.y-textAt.y)/localDraftHeight}:ImVec2{1,1};
                        if(!io.MouseDown[0] || resizeHandle || !styled)canvasPicking=false;
                        if(styled && !resizeHandle && hovered && onText && ImGui::IsMouseClicked(0)){
                            canvasPicking=ImGui::GetMouseClickedCount(0)==1;
                            const int at=canvasByteAt({(io.MousePos.x-textAt.x)/factor.x,(io.MousePos.y-textAt.y)/factor.y});
                            canvasSelectionAnchor=io.KeyShift?(cursorPos==selectionStart?selectionEnd:selectionStart):at;
                        }
                        pickFromLayout=canvasPicking && !composingText();
                        if(pickFromLayout)canvasPickedByte=canvasByteAt({(io.MousePos.x-textAt.x)/factor.x,(io.MousePos.y-textAt.y)/factor.y});
                        ImGui::BeginDisabled(resizeHandle!=0);
                        if(MenuControls::canvasText("##draft",draft,sizeof(draft),{textEnd.x-textAt.x,textEnd.y-textAt.y},
                            em,ink,canvasWrap,!styled,draftCallback,this,scopedStyles)){
                            checkpointDraft(styleAnchor.c_str());changedText();
                        }
                        canvasTextActive=ImGui::IsItemActive();
                        pickFromLayout=false;
                        ImGui::EndDisabled();
                        if(styled){
                            draw->PushClipRect(textAt,textEnd,true);
                            drawCanvasSelection(*draw,textAt,factor,ink,canvasTextActive && localDraftGeneration==draftGeneration);
                            draw->PopClipRect();
                        }
                        ImGui::PopID();ImGui::PopID();
                        ImGui::PopClipRect();
                    }
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
