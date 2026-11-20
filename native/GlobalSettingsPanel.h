#pragma once

struct GlobalSettingsPanel {
    Json latest,original,draft;
    bool visible=false,pending=false,saved=false;
    int tab=0;
    size_t binding=0;
    std::string error;

    void receive(const Json& settings){
        static constexpr const char* fields[]={"enabled","right_click_hide","remote_compatibility","render_mode","shortcuts","gradient_direct_overlay","gradient_layer_tint"};
        for(auto key:fields)if(!settings.contains(key))return;
        for(auto key:fields)latest[key]=settings.at(key);
    }
    void keys(){
        const char* actions[]={"toggle","hide","select","color","moveToTop","moveToBottom","selectAbove","selectBelow","extendAbove","extendBelow","copyStyle","pasteStyle","editStyle","convertSmartObject"};
        const char* names[]={"唤出菜单","收起菜单","选择组内图层","橙色图层标签","移到顶部","移到底部","单选上一个图层","单选下一个图层","向上扩展多选","向下扩展多选","复制图层样式","粘贴图层样式","打开图层样式","转换为智能对象"};
        ImGui::SetNextItemWidth(-1);
        MenuControls::choice("##shortcut-action",names,binding);
        auto& value=draft.at("shortcuts").at(actions[binding]);
        char text[64];std::snprintf(text,sizeof(text),"%s",value.get_ref<const std::string&>().c_str());
        ImGui::PushID(actions[binding]);
        MenuControls::input("##binding","例如 ctrl+f5",text,sizeof(text));
        ImGui::PopID();value=text;
    }
    void draw(float scale){
        ImGui::PushID(int(menuEpoch));
        ImGui::BeginDisabled(pieAnimating || holdFinished || pending || latest.is_null());
        if(MenuControls::iconButton("全局设置",47,scale,true)){
            original=draft=latest;error.clear();saved=false;tab=0;pinned=true;
            ImGui::OpenPopup("##global-settings");
        }
        ImGui::EndDisabled();
        MenuControls::PanelStyle style(scale);
        ImGui::SetNextWindowSize({menuPanelWidth*scale,0},ImGuiCond_Appearing);
        visible=ImGui::BeginPopup("##global-settings");
        if(visible){
            if(saved)ImGui::CloseCurrentPopup();
            ImGui::BeginDisabled(pending);
            auto check=[&](const char* key,const char* label){
                bool value=draft.at(key);
                if(MenuControls::check(label,value))draft[key]=value;
            };
            const char* tabs[]={"常用","快捷键","显示"};
            MenuControls::segments("##settings-tabs",tabs,tab,scale,true);
            if(tab==1)keys();
            else if(tab==2){
                ImGui::TextUnformatted("菜单渲染");
                const char* renderers[]={"自动","GPU","CPU"};
                const char* values[]={"auto","gpu","cpu"};
                const auto& current=draft.at("render_mode");int renderer=current=="gpu"?1:current=="cpu"?2:0;
                if(MenuControls::segments("##renderer",renderers,renderer,scale,true))draft["render_mode"]=values[renderer];
                check("remote_compatibility","远程操作兼容");
            }else{
                check("enabled","启用菜单与快捷键");
                check("right_click_hide","在菜单内右键收起");
                ImGui::TextUnformatted("渐变编辑模式");
                const char* modes[]={"经典模式","叠加模式"};
                int mode=draft.at("gradient_direct_overlay").get<bool>()?1:0;
                if(MenuControls::segments("##gradient-mode",modes,mode,scale,true))draft["gradient_direct_overlay"]=mode==1;
                std::string tint=draft.at("gradient_layer_tint");
                bool highlight=!tint.empty();
                if(MenuControls::check("高亮正在编辑的图层行",highlight)){
                    tint=highlight?"#58c778":"";
                    draft["gradient_layer_tint"]=tint;
                }
                if(highlight){
                    ImGui::SameLine();
                    unsigned rgb=std::stoul(tint.substr(1),nullptr,16);
                    float channels[]={float((rgb>>16)&255)/255,float((rgb>>8)&255)/255,float(rgb&255)/255};
                    if(ImGui::ColorEdit3("##图层高亮颜色",channels,ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoLabel)){
                        char hex[8];
                        std::snprintf(hex,sizeof(hex),"#%02x%02x%02x",IM_F32_TO_INT8_SAT(channels[0]),IM_F32_TO_INT8_SAT(channels[1]),IM_F32_TO_INT8_SAT(channels[2]));
                        draft["gradient_layer_tint"]=hex;
                    }
                }
            }
            if(!error.empty())ImGui::TextWrapped("%s",error.c_str());
            ImGui::BeginDisabled(draft==original);
            if(MenuControls::button("保存",scale)){
                Json changes=Json::object();
                for(const auto& [key,value]:draft.items())if(original.at(key)!=value)changes[key]=value;
                if(changes.contains("shortcuts")){
                    auto& bindings=changes["shortcuts"];bindings=Json::object();
                    for(const auto& [key,value]:draft.at("shortcuts").items())
                        if(original.at("shortcuts").at(key)!=value)bindings[key]=value;
                }
                pending=true;send("menu.settings.save",changes);
            }
            ImGui::EndDisabled();ImGui::SameLine();
            if(MenuControls::button("取消",scale))ImGui::CloseCurrentPopup();
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
};
