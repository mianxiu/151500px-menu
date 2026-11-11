#pragma once

struct GradientPanel {
    static constexpr int end=4096;
    static constexpr float fieldWidth=88;
    Json draft;
    std::string title,phase,message;
    bool live=false,pending=false,dirty=false,terminal=false;
    int channel=0,cursor=end/2,anchorLane=-1,anchorIndex=-1;
    int primaryColor=-1;
    PerceptualPickerState picker;
    float start=0,span=end;
    std::array<uint32_t,2> selection{};
    enum Gesture { none, move, box, pan } gesture=none;
    ImVec2 press{};
    int pressLane=-1,appliedDelta=0;
    bool dragged=false;
    float panStart=0;
    Json original;
    std::array<uint32_t,2> boxBase{};
    ULONGLONG lastSent=0;
    bool refreshBar=true;
    float barStart=-1,barSpan=0;
    std::array<ImU32,129> barColors{};

    Json& paint(){return draft[channel==0?"fill":"stroke"];}
    Json& track(int lane){return paint()[lane==0?"opacity_stops":"stops"];}
    static const Json& track(const Json& paint,int lane){return paint.at(lane==0?"opacity_stops":"stops");}
    bool ready() const {return draft.is_object();}
    bool editable() const {return !terminal && (phase=="editing" || phase=="preview");}
    static uint32_t bit(int index){return uint32_t(1)<<index;}
    bool selected(int lane,int index) const {return (selection[lane]&bit(index))!=0;}
    void open(const Json& state){
        draft=state.at("draft");title=state.value("title","");live=state.value("live",false);
        channel=state.value("channel",std::string())=="stroke" || draft.at("fill").at("kind")!="gradient"?1:0;
        receive(state);
    }
    void receive(const Json& state){
        phase=state.at("phase").get<std::string>();message=state.value("message","");
        if(state.value("ack",false))pending=false;
        if(phase=="error")terminal=false;
    }
    void emit(const char* action){
        pending=true;dirty=false;lastSent=GetTickCount64();
        std::cout<<Json{{"type",action},{"draft",draft}}.dump()<<std::endl;
    }
    void finish(bool apply){
        if(!ready() || terminal || (apply && !editable()))return;
        terminal=true;gesture=none;
        emit(apply?"gradient.apply":"gradient.cancel");
    }
    void flush(){if(editable() && dirty && !pending && GetTickCount64()-lastSent>=80)emit("gradient.update");}

    void select(int lane,int index,bool ctrl,bool shift){
        if(shift && anchorLane==lane){
            selection={};
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
    static Json sample(const Json& stops,int lane,float at){
        const auto right=std::upper_bound(stops.begin(),stops.end(),at,[](float x,const Json& stop){return x<stop.at("location").get<int>();});
        Json value=right==stops.begin()?*right:*std::prev(right);
        if(right!=stops.begin() && right!=stops.end()){
            const auto& left=*std::prev(right);
            const float t=mixAmount(left,*right,at);
            if(lane==0)value["opacity"]=left.at("opacity").get<double>()+(right->at("opacity").get<double>()-left.at("opacity").get<double>())*t;
            else if(left.contains("rgb") && right->contains("rgb")){
                for(int i=0;i<3;++i){
                    const double a=left.at("rgb")[i],b=right->at("rgb")[i];value["rgb"][i]=std::round(a+(b-a)*t);
                }
            }else if(left.contains("native") && right->contains("native") && left["native"]["model"]==(*right)["native"]["model"]){
                for(size_t i=0;i<left["native"]["values"].size();++i){
                    const double a=left["native"]["values"][i],b=(*right)["native"]["values"][i];value["native"]["values"][i]=a+(b-a)*t;
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
        selection={};selection[lane]=bit(index);anchorLane=lane;anchorIndex=index;
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
        const float next=std::clamp(span/factor,64.f,float(end));
        start=std::clamp(start+std::clamp(anchor,0.f,1.f)*(span-next),0.f,end-next);span=next;
    }
    static ImU32 swatch(const Json& stop,ImU32 fallback,int alpha=255){
        if(!stop.contains("rgb"))return fallback;
        const auto& rgb=stop.at("rgb");
        return ImGui::ColorConvertFloat4ToU32({rgb[0].get<float>()/255,rgb[1].get<float>()/255,rgb[2].get<float>()/255,alpha/255.f});
    }
    void timeline(float scale){
        auto* draw=ImGui::GetWindowDrawList();const auto& io=ImGui::GetIO();
        const ImVec2 at=ImGui::GetCursorScreenPos();const float height=menuControlHeight*scale;
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
                if(gesture==box && !dragged && hovered){
                    if(pressLane>=0 && laneAt(mouse.y)==pressLane && !io.KeyCtrl && !io.KeyShift)add(pressLane,cursor);
                    else selection=boxBase;
                }
                gesture=none;
            }
        }
        const auto border=ImGui::GetColorU32(ImGuiCol_Border),active=ImGui::GetColorU32(ImGuiCol_CheckMark);
        const auto neutral=ImGui::GetColorU32(ImGuiCol_Button),dark=ImGui::GetColorU32(ImGuiCol_FrameBg);
        const auto text=ImGui::GetColorU32(ImGuiCol_Text);
        draw->PushClipRect(at,bottom,true);
        const int tick=span>2048?410:span>512?102:span>128?41:10;
        {
            MenuTextStyle secondary(MenuTextRole::Secondary);
            for(int pos=int(std::ceil(start/tick))*tick;pos<=start+span;pos+=tick){
                const float px=x(pos);char label[16];std::snprintf(label,sizeof(label),"%.1f%%",pos*100.f/end);
                draw->AddLine({px,at.y+height-4*scale},{px,at.y+height},border);
                const auto size=ImGui::CalcTextSize(label);
                draw->AddText({std::clamp(px-size.x*.5f,at.x,std::max(at.x,bottom.x-size.x)),at.y},text,label);
            }
        }
        const ImVec2 strip={left,at.y+height*2};
        draw->AddRectFilled(strip,{trackRight,strip.y+height},dark);
        const float tile=height*.5f;
        for(float px=left;px<trackRight;px+=tile){
            const float y=strip.y+(int((px-left)/tile)%2?tile:0);
            draw->AddRectFilled({px,y},{std::min(px+tile,trackRight),y+tile},neutral);
        }
        auto& colors=track(1);auto& alpha=track(0);
        const bool rgb=colors.front().contains("rgb");
        if(rgb){
            if(refreshBar || start!=barStart || span!=barSpan){
                for(int i=0;i<129;++i){
                    const float pos=start+i*span/128;
                    barColors[i]=swatch(sample(colors,1,pos),neutral,int(std::lround(sample(alpha,0,pos).at("opacity").get<float>()*2.55f)));
                }
                barStart=start;barSpan=span;refreshBar=false;
            }
            for(int i=0;i<128;++i){
                const float lo=left+i*trackWidth/128,hi=left+(i+1)*trackWidth/128;
                draw->AddRectFilledMultiColor({lo,strip.y},{hi,strip.y+height},barColors[i],barColors[i+1],barColors[i+1],barColors[i]);
            }
        }
        draw->AddRect(strip,{trackRight,strip.y+height},border);
        for(int lane=0;lane<2;++lane){
            draw->AddLine({left,laneCenter[lane]},{trackRight,laneCenter[lane]},border);
            for(int i=0;i<int(track(lane).size());++i){
                const auto& stop=track(lane)[i];const float px=x(stop.at("location").get<int>());
                if(px<left-.1f || px>trackRight+.1f)continue;
                const float radius=marker*.5f,y=laneCenter[lane];
                const ImVec2 a={px-radius,y-radius},b={px+radius,y+radius};
                const float gray=lane==0?stop.at("opacity").get<float>()/100:0;
                draw->AddRectFilled(a,b,lane==0?ImGui::ColorConvertFloat4ToU32({gray,gray,gray,1}):swatch(stop,neutral),ImGui::GetStyle().FrameRounding);
                draw->AddRect(a,b,selected(lane,i)?active:border,ImGui::GetStyle().FrameRounding,0,selected(lane,i)?2*scale:scale);
            }
        }
        if(gesture==box && dragged){
            const ImVec2 a={std::min(press.x,mouse.x),std::min(press.y,mouse.y)},b={std::max(press.x,mouse.x),std::max(press.y,mouse.y)};
            draw->AddRectFilled(a,b,ImGui::GetColorU32(ImGuiCol_CheckMark,.16f));draw->AddRect(a,b,active);
        }
        draw->PopClipRect();
    }

    template<typename Read,typename Write>
    void selectionValue(const char* label,int onlyLane,double low,double high,float step,const char* format,Read read,Write write){
        bool found=false,mixed=false;double value=0;
        for(int lane=0;lane<2;++lane)if(onlyLane<0 || lane==onlyLane)
            for(int i=0;i<int(track(lane).size());++i)if(selected(lane,i)){
                const double next=read(track(lane)[i]);if(found)mixed|=value!=next;else{value=next;found=true;}
            }
        if(!found)return;
        ImGui::SetNextItemWidth(fieldWidth*scale);
        if(ImGui::DragScalar(label,ImGuiDataType_Double,&value,step,&low,&high,mixed?"混合":format,ImGuiSliderFlags_AlwaysClamp)){
            for(int lane=0;lane<2;++lane)if(onlyLane<0 || lane==onlyLane)
                for(int i=0;i<int(track(lane).size());++i)if(selected(lane,i))write(track(lane)[i],value);
            dirty=refreshBar=true;
        }
    }
    void optionValue(const char* label,const char* key,double low,double high,float step,const char* format){
        auto& option=paint()["options"][key];double value=option;
        ImGui::SetNextItemWidth(fieldWidth*scale);
        if(ImGui::DragScalar(label,ImGuiDataType_Double,&value,step,&low,&high,format,ImGuiSliderFlags_AlwaysClamp)){
            option=value;dirty=refreshBar=true;
        }
    }
    template<size_t N> void choice(const char* label,Json& value,const char* const (&keys)[N],const char* const (&names)[N]){
        size_t index=0;while(index+1<N && value!=keys[index])++index;
        ImGui::SetNextItemWidth(fieldWidth*scale);
        if(MenuControls::choice(label,names,index)){value=keys[index];dirty=refreshBar=true;}
    }
    void colorPicker(float width){
        auto& stops=track(1);
        int index=-1;
        if(primaryColor>=0 && primaryColor<int(stops.size()) && selected(1,primaryColor))index=primaryColor;
        else for(int i=0;i<int(stops.size());++i)if(selected(1,i)){index=i;break;}
        Json rgb=stops[index<0?0:index].at("rgb");
        const auto identity=std::to_string(channel)+":"+std::to_string(index);
        ImGui::BeginDisabled(index<0);
        if(drawPerceptualPicker(picker,rgb,width,identity) && index>=0){
            for(int i=0;i<int(stops.size());++i)if(selected(1,i)){
                stops[i]["rgb"]=rgb;stops[i]["source"]="userStop";
            }
            dirty=refreshBar=true;
        }
        if(picker.hoverPart>=0 && ImGui::BeginItemTooltip()){
            {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextUnformatted(perceptualPickerTips[picker.hoverPart]);}
            ImGui::EndTooltip();
        }
        ImGui::EndDisabled();
        if(index<0){MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextDisabled("选择色标后调色");}
        else ImGui::Dummy({0,0});
    }
    static void nextField(const char* label){
        const auto x=ImGui::GetCursorScreenPos().x;
        const auto gap=ImGui::GetStyle().ItemSpacing.x;
        if(ImGui::GetItemRectMax().x+gap+fieldWidth*scale+ImGui::CalcTextSize(label).x+gap<=x+ImGui::GetContentRegionAvail().x)
            ImGui::SameLine();
    }
    void draw(){
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
        ImGui::Begin("##gradient",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
        if(!ready()){ImGui::End();return;}
        {
            MenuControls::PanelStyle style(scale);
            {MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextUnformatted(title.c_str());}
            if(ImGui::IsKeyPressed(ImGuiKey_Escape) && !ImGui::IsAnyItemActive())finish(false);
            ImGui::BeginDisabled(terminal);
            if(MenuControls::button("关闭并保留草稿",scale))finish(false);
            ImGui::SameLine();ImGui::BeginDisabled(!editable());
            if(MenuControls::button("应用",scale))finish(true);
            ImGui::EndDisabled();ImGui::EndDisabled();
            ImGui::SameLine();{MenuTextStyle text(MenuTextRole::Secondary);ImGui::TextUnformatted(live?"实时更新 PS":"确认后更新 PS");}
            ImGui::BeginDisabled(!editable());
            if(MenuControls::button("缩小",scale))zoom(1.f/1.5f,.5f);
            ImGui::SameLine();char fit[24];std::snprintf(fit,sizeof(fit),"%.0f%%##全览",end*100.f/span);
            if(MenuControls::button(fit,scale)){start=0;span=end;}
            ImGui::SameLine();if(MenuControls::button("放大",scale))zoom(1.5f,.5f);
            if(draft["fill"]["kind"]=="gradient" && draft["stroke"]["kind"]=="gradient"){
                ImGui::SameLine();const char* names[]={"填充","描边"};
                if(MenuControls::segments("##gradient-channel",names,channel,scale)){selection={};anchorLane=primaryColor=-1;gesture=none;refreshBar=true;}
            }
            timeline(scale);
            if(span<end){ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##渐变平移",&start,0,end-span,"",ImGuiSliderFlags_NoInput);}
            ImGui::BeginDisabled(track(1).size()>=32);if(MenuControls::button("添加色标",scale))add(1,cursor);ImGui::EndDisabled();
            ImGui::SameLine();ImGui::BeginDisabled(track(0).size()>=32);if(MenuControls::button("添加不透明度节点",scale))add(0,cursor);ImGui::EndDisabled();
            ImGui::SameLine();ImGui::BeginDisabled(!canRemove());if(MenuControls::button("删除选中",scale))remove();ImGui::EndDisabled();
            if(editable() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive()){
                const auto& io=ImGui::GetIO();
                if(ImGui::IsKeyPressed(ImGuiKey_Delete))remove();
                if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A))for(int lane=0;lane<2;++lane){selection[lane]=0;for(int i=0;i<int(track(lane).size());++i)selection[lane]|=bit(i);}
                for(int sign:{-1,1})if((selection[0] || selection[1]) && ImGui::IsKeyPressed(sign<0?ImGuiKey_LeftArrow:ImGuiKey_RightArrow)){
                    const Json base=paint();const int delta=shift(base,sign*(io.KeyShift?41:4));if(delta)moveBy(base,delta);
                }
            }
            const bool rgb=track(1).front().contains("rgb");
            const float pickerWidth=menuPanelWidth*scale;
            const bool sideBySide=rgb && ImGui::GetContentRegionAvail().x>=(menuPanelWidth+280)*scale &&
                ImGui::BeginTable("##gradient-parameters",2,ImGuiTableFlags_NoSavedSettings|ImGuiTableFlags_SizingStretchSame);
            if(sideBySide){
                ImGui::TableSetupColumn("##settings",ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##picker",ImGuiTableColumnFlags_WidthFixed,pickerWidth);
                ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);
            }
            if(selection[0] || selection[1]){
                for(int lane=0;lane<2;++lane)if(selection[lane]){
                    for(int i=0;i<int(track(lane).size());++i)if(selected(lane,i)){
                        const int location=track(lane)[i].at("location");double value=location*100./end;
                        const double lo=0,hi=100;ImGui::SetNextItemWidth(fieldWidth*scale);
                        if(ImGui::DragScalar("位置",ImGuiDataType_Double,&value,.1f,&lo,&hi,"%.2f%%",ImGuiSliderFlags_AlwaysClamp)){
                            const Json base=paint();const int delta=shift(base,int(std::lround(value*end/100))-location);if(delta)moveBy(base,delta);
                        }
                        break;
                    }
                    break;
                }
                nextField("中点");selectionValue("中点",-1,0,100,.2f,"%.0f%%",[](const Json& stop){return stop.at("midpoint").get<double>();},[](Json& stop,double value){stop["midpoint"]=int(std::lround(value));});
                if(selection[0]){nextField("不透明度");selectionValue("不透明度",0,0,100,.2f,"%.2f%%",[](const Json& stop){return stop.at("opacity").get<double>();},[](Json& stop,double value){stop["opacity"]=value;});}
                if(selection[1]){
                    const auto& first=track(1).front();const bool native=first.contains("native");
                    const std::string model=native?first.at("native").at("model").get<std::string>():"rgb";
                    const char* const rgbNames[]={"R","G","B"},*const cmyk[]={"C","M","Y","K"},*const lab[]={"L*","a*","b*"},*const gray[]={"灰度"};
                    const auto names=model=="cmyk"?cmyk:model=="lab"?lab:model=="gray"?gray:rgbNames;
                    const int count=model=="cmyk"?4:model=="gray"?1:3;
                    for(int i=0;i<count;++i){
                        if(i)nextField(names[i]);
                        selectionValue(names[i],1,model=="lab" && i>0?-128:0,!native?255:model=="lab" && i>0?127:100,.25f,native?"%.2f":"%.0f",
                            [=](const Json& stop){return (native?stop.at("native").at("values"):stop.at("rgb"))[i].get<double>();},
                            [=](Json& stop,double value){if(native)stop["native"]["values"][i]=value;else stop["rgb"][i]=int(std::lround(value));stop["source"]="userStop";});
                    }
                }
            }
            const char* styles[]={"linear","radial","angle","reflected","diamond"},*styleNames[]={"线性","径向","角度","对称","菱形"};
            const char* methods[]={"classic","linear","perceptual"},*methodNames[]={"经典","线性","感知"};
            choice("样式",paint()["options"]["style"],styles,styleNames);
            nextField("插值");choice("插值",paint()["method"],methods,methodNames);
            optionValue("角度", "angle",-360,360,.25f,"%.2f°");nextField("缩放");optionValue("缩放","scale",1,1000,.25f,"%.2f%%");
            auto& smoothness=paint()["options"]["smoothness"];double smooth=smoothness.get<double>()*100/end;
            const double lo=0,hi=100;nextField("平滑度");ImGui::SetNextItemWidth(fieldWidth*scale);
            if(ImGui::DragScalar("平滑度",ImGuiDataType_Double,&smooth,.25f,&lo,&hi,"%.1f%%",ImGuiSliderFlags_AlwaysClamp)){
                smoothness=int(std::lround(smooth*end/100));dirty=refreshBar=true;
            }
            auto flag=[&](const char* label,const char* key){auto& option=paint()["options"][key];bool value=option;if(ImGui::Checkbox(label,&value)){option=value;dirty=refreshBar=true;}};
            flag("反向","reverse");ImGui::SameLine();flag("仿色","dither");ImGui::SameLine();flag("与图层对齐","align");
            if(sideBySide){
                ImGui::TableSetColumnIndex(1);colorPicker(pickerWidth);ImGui::EndTable();
            }else if(rgb)colorPicker(std::min(pickerWidth,ImGui::GetContentRegionAvail().x));
            ImGui::EndDisabled();
            {MenuTextStyle text(MenuTextRole::Secondary);
                ImGui::TextWrapped("%s",phase=="applying"?"正在确认 Photoshop 修改…":phase=="closing"?"正在恢复原渐变…":
                    phase=="error"?message.c_str():pending || dirty?"正在保存草稿…":message.c_str());}
        }
        flush();ImGui::End();
    }
};
