#include "MenuTextGroups.h"

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
    ImGuiTextFilter fontFilter;
    bool fontsAvailable=false,fontsRequested=false;
    std::string fontCandidate,fontPreview;
    double fontHoverSince=0;
    bool scopeSupported=false,paragraphIndentsSupported=false,paragraphAlignmentSupported=false,extendedTypography=false,managedList=false;
    bool canvasAvailable=false,proofAvailable=false,objectProofAvailable=false,proofCurrent=false;
    bool isolatedProofAvailable=false;
    bool canvasEditAvailable=false,canvasTextFrame=false,canvasTextActive=false,canvasWrap=true,canvasPanning=false;
    std::array<float,4> textBox{};
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
    ImVec2 canvasPan{};
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
        if(!fontPreview.empty())changes["fontPostScriptName"]=fontPreview;
        const int scope=!fontPreview.empty() && !scopeExplicit && selectionStart!=selectionEnd?1:styleScope;
        if(scope && !changes.empty()){
            const int length=int(std::strlen(draft));
            const bool selected=selectionStart!=selectionEnd;
            int start=std::clamp(selected?std::min(selectionStart,selectionEnd):lastSelectionStart,0,length);
            int end=std::clamp(selected?std::max(selectionStart,selectionEnd):lastSelectionEnd,0,length);
            if(scope==2){
                start=end=std::clamp(cursorPos,0,length);
                while(start>0 && draft[start-1]!='\n')--start;
                while(end<length && draft[end]!='\n')++end;
            }
            const auto units=[this](int bytes){return bytes?MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,draft,bytes,nullptr,0):0;};
            changes["range"]={units(start),units(end)+(scope==2?1:0)};
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
    void changedText(){remapKerning();proofCurrent=false;proofAttempt.clear();sharpAttempt=sharpProofScale=1;++draftGeneration;planId.clear();resultValue.clear();resultPreview.clear();resultKind.clear();textMatches.clear();matchSource.clear();error.clear();textDirty=true;textChanged=ImGui::GetTime();}
    void setDraft(const std::string& value){std::snprintf(draft,sizeof(draft),"%s",value.c_str());kerningEdits.clear();kerningFieldAt=-1;kerningFieldInvalid=false;++draftBufferId;indentCommand=selectionStart=selectionEnd=lastSelectionStart=lastSelectionEnd=cursorPos=0;restoreDraftFocus=false;changedText();}
    int draftUnits(int bytes)const{return bytes>0?MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,draft,bytes,nullptr,0):0;}
    bool hasStyles()const{return !styleChanges().empty() || !kerningEdits.empty();}
    static int draftCallback(ImGuiInputTextCallbackData* data) {
        auto& panel=*static_cast<MenuToolsPanel*>(data->UserData);
        if(data->EventFlag==ImGuiInputTextFlags_CallbackCharFilter) {
            const auto& io=ImGui::GetIO();
            if(panel.textTab==4 && data->EventChar=='\t' && ImGui::IsKeyDown(ImGuiKey_Tab) && !io.KeyCtrl && !io.KeyAlt && !io.KeySuper){panel.indentCommand=1;return 1;}
            return 0;
        }
        if(panel.restoreDraftFocus){data->SelectionStart=panel.selectionStart;data->SelectionEnd=panel.selectionEnd;data->CursorPos=panel.cursorPos;panel.restoreDraftFocus=false;}
        const auto& io=ImGui::GetIO();
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
        if((panel.styleScope==1 && rangeChanged) ||
           (panel.styleScope==2 && panel.cursorPos!=data->CursorPos))panel.changedText();
        panel.selectionStart=data->SelectionStart;panel.selectionEnd=data->SelectionEnd;panel.cursorPos=data->CursorPos;
        return 0;
    }
    bool textDue(double now)const{return textDirty && active && ready && !pending && !revision.empty() && !kerningFieldInvalid && (kerningEdits.empty() || textTab==0) && (textTab!=1 || find[0]) && (textTab!=3 || splitMode==0 || separator[0]) && now-textChanged>=.12;}
    bool canWriteText()const{return !pending && !revision.empty() && !kerningFieldInvalid && draft[0];}
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
        if(!error.empty())return textTab==3?error+"；仍可写回当前文字":error;
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
        const auto styles=styleChanges();
        if(!styles.empty() && textTab!=3)args["typography"]=styles;
        if(!fontPreview.empty())args["font_preview"]=true;
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
            if(mode=="split" || mode=="typeset")submit("text.read",Json::object());
            else changeSearch(ImGui::GetTime()-.18);
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
            if(!isolatedProofAvailable)backgroundMode=0;
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
            fontFilter.Clear();fontsAvailable=result.value("fonts_available",false);fontsRequested=false;
            fontCandidate.clear();fontPreview.clear();
            loadFonts(result.value("fonts",Json::array()));
            scopeSupported=result.value("style_scope_supported",false);styleScope=0;scopeExplicit=false;
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
            setDraft(sourceDisplay);
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
                const auto frame=result.value("text_frame",Json(nullptr));
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
        if(MenuControls::button("添加行",scale)){cells.emplace_back(cols);changedText();}
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(cells.size()==1);
        if(MenuControls::button("移除末行",scale)){cells.pop_back();changedText();}
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(cols>=8);
        if(MenuControls::button("添加列",scale)){for(auto& row:cells)row.emplace_back();changedText();}
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(cols==1);
        if(MenuControls::button("移除末列",scale)){for(auto& row:cells)row.pop_back();changedText();}
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
        canvasTextFrame=false;
        submit("text.canvas",Json{{"revision",revision},{"background",canvasEditAvailable}});
    }
    bool editingOnCanvas()const{
        return mode=="typeset" && canvasTextFrame && thumbnail && textTab!=6;
    }
    void drawDraft(float height) {
        ImGui::PushID(int(menuEpoch));
        ImGui::PushID(draftBufferId);
        if(restoreDraftFocus)ImGui::SetKeyboardFocusHere();
        if(MenuControls::multiline("##draft",draft,sizeof(draft),height,draftCallback,this))changedText();
        ImGui::PopID();ImGui::PopID();
    }
    void maybeProof(bool available,const std::string& key,Json args){
        const auto now=ImGui::GetTime();
        if(!available || pending || now-std::max(textChanged,canvasInteraction)<(fontPreview.empty()?.35:.12) || composingText())return;
        if(proofAttempt!=key){
            proofAttempt=key;pendingProofScale=1;
        }else if(proofCurrent && wantedProofScale>sharpAttempt && now-proofZoomChanged>=.4){
            pendingProofScale=sharpAttempt=wantedProofScale;args["scale"]=pendingProofScale;
        }else return;
        if(backgroundMode)args["isolated"]=true;
        previewGeneration=draftGeneration;submit("text.proof",std::move(args));
    }
    std::string sourceFont()const{
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
    void setFont(const std::string& id){
        if(scopeSupported && !scopeExplicit && selectionStart!=selectionEnd)styleScope=1;
        std::snprintf(styleValues[0],sizeof(styleValues[0]),"%s",id.c_str());changedText();
    }
    void updateFontPreview(std::string candidate){
        const auto now=ImGui::GetTime();
        if(candidate!=fontCandidate){fontCandidate=std::move(candidate);fontHoverSince=now;}
        if(fontCandidate!=fontPreview && (fontCandidate.empty() || now-fontHoverSince>=.20)){
            fontPreview=fontCandidate;changedText();
        }
    }
    void drawManualKerning(float scale){
        MenuTextStyle role(MenuTextRole::Control);
        const int at=cursorPos;
        const auto [first,last]=pairSpan(draft,at);
        if(first<0 || selectionStart!=selectionEnd){
            kerningFieldAt=-1;kerningFieldInvalid=false;
            MenuTextStyle hint(MenuTextRole::Secondary);
            ImGui::TextWrapped("把光标放到两个字符之间");
            return;
        }
        const std::string left(draft+first,draft+at),right(draft+at,draft+last);
        ImGui::Text("%s | %s",left.c_str(),right.c_str());
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
            kerningAnchor=draft;kerningEdits[at]=std::move(change);changedText();
        };
        ImGui::PushID(at);
        MenuControls::fieldIcon("手动字偶微调 (1/1000 em)",62,scale);
        MenuControls::input("##manual-kerning","自动",kerningField,sizeof(kerningField));
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
        if(!value.is_null() && MenuControls::button("恢复自动字偶",scale)){
            kerningField[0]='\0';kerningFieldInvalid=false;stage(nullptr);
        }
        ImGui::PopID();
    }
    void drawTypography(float scale) {
        if(!sourceStyles.is_object() || revision.empty() || tableObject){updateFontPreview({});return;}
        std::string hoveredFont;
        ImGui::BeginDisabled(textTab==3 || textTab==6);
        const char* scopes[]={"整层","文字选区","当前段落"};
        if(scopeSupported && MenuControls::segments("##style-scope",scopes,styleScope,scale)){scopeExplicit=true;changedText();}
        const auto label=[&](const char* title){ImGui::TextWrapped("%s",title);};
        const auto original=[&](const char* key){return sourceStyles.value(key,std::string("沿用原值"));};
        const auto field=[&](int index){
            ImGui::PushID(styleKeys[index]);
            const int glyphs[]={57,59,60,61,63,64,65,66,67,68,69,70};
            MenuControls::fieldIcon(styleLabels[index],glyphs[index],scale);
            const auto hint=original(styleKeys[index]);
            requireText(hint);
            if(index==leadingField && extendedTypography){
                const char* modes[]={"沿用原行距","固定行距","自动行距"};
                size_t selected=leadingMode;
                ImGui::SetNextItemWidth(-1);
                if(MenuControls::choice("##leading-mode",modes,selected)){
                    leadingMode=int(selected);styleValues[index][0]='\0';changedText();
                }
                if(leadingMode==1)MenuControls::fieldIcon("固定行距 (pt)",60,scale);
            }
            if(index!=leadingField || !extendedTypography || leadingMode==1){
                MenuControls::input("##style",hint.c_str(),styleValues[index],sizeof(styleValues[index]));
                if(ImGui::IsItemEdited())changedText();
            }
            ImGui::PopID();
        };
        const auto select=[&](const char* title,const char* key,const auto& choices,int& index,int glyph=74){
            ImGui::PushID(key);
            label(title);
            const auto hint=original(key);
            if(index==0){MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("%s",hint.c_str());}
            MenuControls::fieldIcon(title,glyph,scale);
            size_t choice=index;
            ImGui::SetNextItemWidth(-1);
            if(MenuControls::choice("##option",choices,choice)){index=int(choice);changedText();}
            ImGui::PopID();
        };
        const auto pair=[&](const char* id,const auto& left,const auto& right){
            if(ImGui::BeginTable(id,2,ImGuiTableFlags_SizingStretchSame)){
                ImGui::TableNextColumn();left();
                ImGui::TableNextColumn();right();
                ImGui::EndTable();
            }
        };
        const char* switches[]={"沿用","关闭","开启"};
        if(MenuControls::section("字符")){
            if(mode=="typeset" && (fontsAvailable || !fonts.empty())){
                const auto id=styleValues[0][0]?std::string(styleValues[0]):sourceFont();
                const auto face=std::find_if(fonts.begin(),fonts.end(),[&](const auto& row){return row.id==id;});
                MenuControls::fieldIcon("字体",57,scale);
                const char* family=face==fonts.end()?(id=="沿用原值"?"浏览 Photoshop 字体":id.c_str()):face->family.c_str();
                requireText(family);
                bool opened=false;
                int familyHover=-1;
                const int chosen=MenuControls::listChoice("##font-family",family,families,&fontFilter,&opened,&familyHover);
                if(opened && fonts.empty() && !fontsRequested && !pending){
                    fontsRequested=true;submit("text.fonts",Json{{"revision",revision}});
                }
                const auto familyFace=[&](int index)->const std::string&{
                    const auto& picked=families[index];
                    auto preferred=std::find_if(fonts.begin(),fonts.end(),[&](const auto& f){return f.family==picked.first &&
                        f.style==(face==fonts.end()?"Regular":face->style);});
                    return preferred==fonts.end()?picked.second:preferred->id;
                };
                if(chosen>=0)setFont(familyFace(chosen));
                else if(familyHover>=0 && familyFace(familyHover)!=id)hoveredFont=familyFace(familyHover);
                if(face!=fonts.end()){
                    MenuControls::fieldIcon("样式／字重",58,scale);
                    const auto shown=face->style.empty()?face->id:face->style;
                    requireText(shown);
                    std::vector<std::pair<std::string,std::string>> faces;
                    for(const auto& row:fonts)if(row.family==face->family)faces.emplace_back(row.style.empty()?row.id:row.style,row.id);
                    int faceHover=-1;
                    const int picked=MenuControls::listChoice("##font-face",shown.c_str(),faces,nullptr,nullptr,&faceHover);
                    if(picked>=0){
                        setFont(faces[picked].second);
                    }else if(faceHover>=0 && faces[faceHover].second!=id)hoveredFont=faces[faceHover].second;
                }
                if(styleValues[0][0] && MenuControls::button("沿用原字体",scale)){styleValues[0][0]='\0';changedText();}
            }else field(0);
            pair("##size-leading",[&]{field(1);},[&]{field(2);});
            pair("##kerning-tracking",[&]{
                if(advancedTypography){
                    const char* kern[]={"沿用","度量","视觉"};
                    select("字偶间距",enumKeys[0],kern,styleEnums[0],62);
                }
            },[&]{field(3);});
            if(kerningSupported && textTab==0 && MenuControls::section("手动字偶",false))drawManualKerning(scale);
            if(advancedTypography && textRgb){
                MenuControls::fieldIcon("文字颜色",71,scale);
                if(MenuControls::colorSwatch("##文字颜色",textRgbDraft)){textColorChanged=true;changedText();}
                const auto current=original("color");
                if(!textColorChanged){ImGui::SameLine();MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextUnformatted(current.c_str());}
                else{
                    ImGui::SameLine();
                    if(MenuControls::button("沿用",scale)){
                        std::copy_n(textRgbOriginal,3,textRgbDraft);textColorChanged=false;changedText();
                    }
                }
            }
            if(extendedTypography && MenuControls::section("更多字符设置",false)){
                field(0);
                pair("##scale-pair",[&]{field(5);},[&]{field(6);});field(4);
                if(advancedTypography){
                    const char* caps[]={"沿用","正常","大写","小型大写"};
                    const char* baseline[]={"沿用","正常","上标","下标"};
                    pair("##caps-baseline",[&]{select("大小写",enumKeys[1],caps,styleEnums[1]);},
                         [&]{select("上下标",enumKeys[2],baseline,styleEnums[2]);});
                    pair("##decorations",[&]{select("下划线",enumKeys[3],switches,styleEnums[3]);},
                         [&]{select("删除线",enumKeys[4],switches,styleEnums[4]);});
                    pair("##synthetic",[&]{select("仿粗体",switchKeys[0],switches,styleSwitches[0]);},
                         [&]{select("仿斜体",switchKeys[1],switches,styleSwitches[1]);});
                    pair("##line-options",[&]{select("标准连字",switchKeys[2],switches,styleSwitches[2]);},
                         [&]{select("禁止换行",switchKeys[3],switches,styleSwitches[3]);});
                }
            }
            if(openTypeSupported && MenuControls::section("OpenType",false)){
                const char* figures[]={"沿用","默认","比例等高","等宽等高","比例旧式","等宽旧式"};
                select("数字样式",enumKeys[5],figures,styleEnums[5],72);
                const char* labels[]={"上下文连字","样式替代","花体","标题替代","装饰字形","分数","序数"};
                for(int i=0;i<IM_ARRAYSIZE(labels);++i)select(labels[i],switchKeys[5+i],switches,styleSwitches[5+i],72);
            }
        }
        if(MenuControls::section("段落")){
            if(extendedTypography && paragraphAlignmentSupported && !managedList && textTab!=4){
                const auto align=original("align");
                label("对齐");
                MenuControls::detailLine(align.c_str(),scale);
                int selected=paragraphAlign-1;
                const char* basic[]={"左对齐","居中","右对齐"};
                const int basicIcons[]={50,51,52};
                const char* full[]={"左对齐","居中","右对齐","两端对齐，末行左","两端对齐，末行居中","两端对齐，末行右","全部两端对齐"};
                const int fullIcons[]={50,51,52,53,54,55,56};
                const bool changed=advancedTypography && paragraphIndentsSupported
                    ?MenuControls::iconSegments("##align",full,fullIcons,selected,scale)
                    :MenuControls::iconSegments("##align",basic,basicIcons,selected,scale);
                if(changed){paragraphAlign=selected+1;changedText();}
                if(paragraphAlign && MenuControls::button("沿用原对齐",scale)){paragraphAlign=0;changedText();}
            }
            if(paragraphIndentsSupported){
                if(!managedList && textTab!=4){
                    pair("##indents",[&]{field(9);},[&]{field(10);});field(11);
                }else field(10);
            }
            pair("##paragraph-spacing",[&]{field(7);},[&]{field(8);});
            if(advancedTypography && paragraphIndentsSupported && !managedList && textTab!=4)
                select("自动断字",switchKeys[4],switches,styleSwitches[4],73);
        }
        ImGui::EndDisabled();
        updateFontPreview(mode=="typeset" && proofAvailable && textTab==0?std::move(hoveredFont):std::string());
    }
    void drawText(float scale,bool available) {
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
                int tab=textTab==6?5:textTab;
                if(MenuControls::segments("##typeset-tabs",choices,tab,scale)){
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
            const bool canTable=available && (tableObject || (draft==sourceDisplay && !hasStyles() && !draftListChanged));
            const bool needsProof=mode=="typeset" && objectProofAvailable
                && (!tableObject || cells!=savedCells || wantedProofScale>1 || backgroundMode);
            if(needsProof)maybeProof(canTable,"table",Json{{"revision",revision},{"cells",cells}});
            ImGui::BeginDisabled(pending || !canTable || (needsProof && !proofCurrent && error.empty()));
            if(MenuControls::button(tableObject?"更新表格":"创建独立表格",scale))submit("text.table",Json{{"revision",revision},{"cells",cells}});
            ImGui::EndDisabled();
            if(!error.empty()){MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("%s",error.c_str());}
            drawTable(scale);return;
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
            {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("留空自动按行分组，最多64组，自动折行不计。写回当前文字保留原图层，拆成多个图层才替换原层；保留位置要求原有换行边界。");}
        } else if(textTab==4) {
            const char* lists[]={"项目列表","编号列表","移除列表"};if(MenuControls::segments("##list-mode",lists,listMode,scale))changedText();
            if(listMode==0) {
                const char* markers[]={"随层级 • ◦ ▪","•","◦","▪","–","✓","自定义"};if(MenuControls::segments("##list-markers",markers,listMarker,scale))changedText();
                if(listMarker==6){MenuControls::input("##list-custom","项目符号（一个字符）",listCustom,sizeof(listCustom));if(ImGui::IsItemEdited())changedText();}
            } else if(listMode==1) {
                const char* styles[]={"1.","1)","(1)","a)","A)","i)","I)","1.1","1.a.i"};if(MenuControls::segments("##list-numbering",styles,listNumbering,scale))changedText();
                MenuControls::input("##list-start","起始编号 1—9999",listStart,sizeof(listStart));if(ImGui::IsItemEdited())changedText();
            }
            int indent=-1;const char* actions[]={"减少缩进","增加缩进"};
            if(MenuControls::segments("##list-indent",actions,indent,scale)){indentCommand=indent==0?-1:1;restoreDraftFocus=true;}
            {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("缩进作用于选中行或当前行，最多6级；Tab增加、Shift+Tab减少。预览自动更新编号，空行保留；每级写入4个空格，写回PS后不自动续号。");}
        }
        if(textTab==5)drawTypography(scale);
        if(available && textDue(ImGui::GetTime())){textDirty=false;submit("text.preview",textParams());}
        if(textTab==1 && !planId.empty()) {
            {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("匹配 %d 处 · 强调色为匹配内容",resultCount);}
            MenuControls::textPreview("##text-matches",matchSource.c_str(),80*scale,textMatches);
        }
        const bool resultOnCanvas=mode=="typeset" && proofAvailable && thumbnail && textTab!=3;
        {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextWrapped("%s",textStatus().c_str());}
        if(!resultOnCanvas && (!editingOnCanvas() || (textTab>=1 && textTab<=4)))
            MenuControls::textPreview("##text-result",planId.empty()?draft:resultPreview.c_str(),130*scale,{},textTab==4 && !planId.empty() && listMode!=2);
        const bool processed=textTab!=3 && !planId.empty() && resultKind=="replace";
        const bool needsProof=mode=="typeset" && proofAvailable && processed
            && (resultValue!=sourceDisplay || hasStyles() || textTab==4 || draftListChanged
                || (vectorObject && wantedProofScale>1) || backgroundMode);
        if(needsProof)maybeProof(available,planId,Json{{"revision",revision},{"plan_id",planId}});
        if(needsProof && !proofCurrent && !error.empty() && MenuControls::button("重新预览",scale))changedText();
        const bool requiresExact=!kerningEdits.empty() || styleEnums[5]!=0 ||
            std::any_of(styleSwitches+5,std::end(styleSwitches),[](int setting){return setting!=0;});
        ImGui::BeginDisabled(!fontCandidate.empty() || !fontPreview.empty() || !canWriteText() || (hasStyles() && !processed) || (!kerningEdits.empty() && textTab!=0)
            || (needsProof && !proofCurrent && (requiresExact || !vectorObject || error.empty())));
        const auto makeVector=[&]{
            if(MenuControls::button("生成矢量智能对象",scale)){
                if(processed)submit("text.vector",Json{{"revision",revision},{"plan_id",planId}});
                else {Json args={{"revision",revision},{"draft",draft}};if(draftListChanged)args["list_options"]=draftList;submit("text.vector",args);}
            }
        };
        if(mode=="typeset" && !vectorObject && textTab!=3)makeVector();
        if(MenuControls::button(vectorObject?"更新矢量文字对象":processed?"写回预览结果":"写回当前文字",scale)) {
            if(processed)submit("text.apply",Json{{"revision",revision},{"plan_id",planId}});
            else {Json args={{"revision",revision},{"draft",draft}};if(draftListChanged)args["list_options"]=draftList;submit("text.write",args);}
        }
        if(mode!="typeset" && !vectorObject && textTab!=3)makeVector();
        ImGui::EndDisabled();
        if(textTab==3) {
            ImGui::BeginDisabled(vectorObject || hasStyles() || planId.empty() || resultKind!="split" || pending);
            if(MenuControls::button("拆成多个图层",scale))submit("text.apply",Json{{"revision",revision},{"plan_id",planId}});
            ImGui::EndDisabled();
        }
        ImGui::BeginDisabled(planId.empty() || pending);
        if((!editingOnCanvas() || (textTab>=1 && textTab<=4)) && MenuControls::button("将结果继续编辑",scale)){
            if(textTab==4){const auto params=textParams();draftList=nullptr;if(listMode!=2){draftList=Json::object();for(const char* key:{"kind","marker","custom_marker","numbering","start"})draftList[key]=params.at(key);}draftListChanged=true;}
            setDraft(resultValue);textTab=0;
        }
        ImGui::EndDisabled();
    }
    void draw(float scale,ImVec2 center,bool available) {
        const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
        MenuControls::PanelStyle controls(scale);
        const float gap=menuControlPadding*scale,margin=menuFloatingInset*scale,h=menuControlHeight*scale;
        const bool typeset=mode=="typeset";
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
        ImGui::BeginChild("##caps-tools",{width,height},ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PopStyleColor();
        if(typeset){
            const char* resize=maximized?"还原":"最大化";
            const float dragWidth=std::max(1.f,ImGui::GetContentRegionAvail().x-2*(h+gap)-.5f);
            if(MenuControls::windowDragBar("画布排版",h,dragWidth) && !maximized){
                dragging=true;GetCursorPos(&dragPointer);
            }
            ImGui::SameLine();
            if(MenuControls::iconButton(resize,maximized?49:48,scale))toggleMaximized();
            ImGui::SameLine();
            if(MenuControls::iconButton("关闭",41,scale))send("hide");
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
                const float sidebar=std::floor(std::min(244*scale,body.x*.25f));
                ImGui::BeginChild("##layout-typography",{sidebar,bodyHeight},ImGuiChildFlags_None);
                drawTypography(scale);
                ImGui::EndChild();ImGui::SameLine();
                ImGui::BeginChild("##layout-canvas",{std::max(1.f,body.x-2*(sidebar+gap)),bodyHeight},
                    ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
                if(MenuControls::button("缩小",scale))canvasZoom=std::max(.25f,canvasZoom/1.2f);
                ImGui::SameLine();
                if(MenuControls::button("放大",scale))canvasZoom=std::min(8.f,canvasZoom*1.2f);
                ImGui::SameLine();
                if(MenuControls::button("适合窗口",scale)){canvasZoom=1;canvasPan={};}
                const bool canIsolate=isolatedProofAvailable && thumbnail && textTab!=3
                    && (textTab==6?objectProofAvailable:proofAvailable);
                if(canIsolate){
                    const bool wasIsolated=backgroundMode!=0;
                    const char* backgrounds[]={"文档背景","透明背景","纯色背景"};
                    if(MenuControls::segments("##canvas-background",backgrounds,backgroundMode,scale)
                       && wasIsolated!=(backgroundMode!=0))changedText();
                    if(backgroundMode==2)MenuControls::colorSwatch("背景颜色",backgroundRgb);
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
                        if(io.MouseWheel)canvasZoom=std::clamp(canvasZoom*std::pow(1.2f,io.MouseWheel),.25f,8.f);
                    }
                    if(!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle))canvasPanning=false;
                    if(canvasPanning){
                        canvasPan.x+=io.MouseDelta.x;canvasPan.y+=io.MouseDelta.y;
                    }
                    const float fit=std::min(extent.x/thumbnailSize.x,extent.y/thumbnailSize.y)*canvasZoom;
                    const ImVec2 imageSize={thumbnailSize.x*fit,thumbnailSize.y*fit},min={p.x+(extent.x-imageSize.x)*.5f+canvasPan.x,p.y+(extent.y-imageSize.y)*.5f+canvasPan.y};
                    if(proofNativeSize.x>0){
                        const float demand=std::max(imageSize.x/proofNativeSize.x,imageSize.y/proofNativeSize.y);
                        const int scale=std::min(maxProofScale,demand>4.f?8:demand>2.f?4:demand>1.15f?2:1);
                        if(scale!=wantedProofScale){wantedProofScale=scale;proofZoomChanged=ImGui::GetTime();}
                    }
                    const ImVec2 max={min.x+imageSize.x,min.y+imageSize.y};
                    const ImVec2 textAt={min.x+textBox[0]*imageSize.x,min.y+textBox[1]*imageSize.y};
                    const ImVec2 textEnd={min.x+textBox[2]*imageSize.x,min.y+textBox[3]*imageSize.y};
                    const bool onText=editingOnCanvas() && ImGui::IsMouseHoveringRect(textAt,textEnd);
                    if(hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                       (!onText && ImGui::IsMouseClicked(ImGuiMouseButton_Left))))canvasPanning=true;
                    if((hovered && onText && ImGui::IsMouseDown(ImGuiMouseButton_Left)) || restoreDraftFocus ||
                       (canvasTextActive && (io.InputQueueCharacters.Size || ImGui::IsKeyPressed(ImGuiKey_Backspace) ||
                          ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Enter) ||
                          (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Z) || ImGui::IsKeyPressed(ImGuiKey_Y) ||
                                         ImGui::IsKeyPressed(ImGuiKey_V) || ImGui::IsKeyPressed(ImGuiKey_X))))))
                        canvasInteraction=ImGui::GetTime();
                    const bool exact=proofImage && proofCurrent && (shownBackground!=0)==(backgroundMode!=0) && ImGui::GetTime()-canvasInteraction>=.35
                        && (!canvasTextActive || !composingText());
                    auto* draw=ImGui::GetWindowDrawList();
                    draw->PushClipRect(p,end,true);
                    const ImVec2 clipMin={std::max(p.x,min.x),std::max(p.y,min.y)},clipMax={std::min(end.x,max.x),std::min(end.y,max.y)};
                    if(clipMin.x<clipMax.x && clipMin.y<clipMax.y){
                        if(shownBackground==2)draw->AddRectFilled(clipMin,clipMax,ImGui::ColorConvertFloat4ToU32({backgroundRgb[0],backgroundRgb[1],backgroundRgb[2],1.f}));
                        else ImGui::RenderColorRectWithAlphaCheckerboard(draw,clipMin,clipMax,0,2*menuPanelGap*scale,{0,0});
                    }
                    auto* composed=sharpProofImage && sharpProofScale>1 && wantedProofScale>1?sharpProofImage:proofImage;
                    if(exact || shownBackground==0)draw->AddImage((ImTextureID)(intptr_t)(exact?composed:thumbnail),min,max);
                    if(editingOnCanvas())
                        draw->AddRect(textAt,textEnd,ImGui::GetColorU32(ImGuiCol_NavCursor),0,0,menuControlBorder*scale);
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
                        if(MenuControls::canvasText("##draft",draft,sizeof(draft),{textEnd.x-textAt.x,textEnd.y-textAt.y},
                            em,ink,canvasWrap,!exact,draftCallback,this))changedText();
                        canvasTextActive=ImGui::IsItemActive();
                        ImGui::PopID();ImGui::PopID();
                        ImGui::PopClipRect();
                    }
                }
                ImGui::EndChild();ImGui::SameLine();
            }
            ImGui::BeginChild("##text-body",{0,bodyHeight},ImGuiChildFlags_None);
            drawText(scale,available);ImGui::EndChild();
        }
        ImGui::EndDisabled();
        if(!typeset && MenuControls::button("关闭",scale))send("hide");
        if(!typeset && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive()) {
            dragging=true;GetCursorPos(&dragPointer);
        }
        if((mode!="typeset" && (!available || pending)) || !ImGui::IsMouseDown(ImGuiMouseButton_Left))dragging=false;
        if(dragging && !pending) {
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
