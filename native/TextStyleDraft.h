#pragma once

// Half-open byte intervals use draft.size()+1 for Photoshop's final paragraph marker.
struct TextStyleDraft {
    struct Run {
        int first,last;
        Json value;
    };
    using Runs=std::map<std::string,std::vector<Run>>;
    Runs source,edits;

    static const Json* valueAt(const std::vector<Run>& runs,int offset) {
        const auto it=std::upper_bound(runs.begin(),runs.end(),offset,[](int value,const Run& run){return value<run.first;});
        if(it==runs.begin())return nullptr;
        const auto& run=*std::prev(it);
        return offset<run.last?&run.value:nullptr;
    }

    void set(const char* key,std::array<int,2> range,Json value) {
        if(range[0]>=range[1])return;
        auto& current=edits[key];
        std::vector<Run> next;
        for(const auto& run:current) {
            if(run.last<=range[0] || run.first>=range[1])next.push_back(run);
            else{
                if(run.first<range[0])next.push_back({run.first,range[0],run.value});
                if(run.last>range[1])next.push_back({range[1],run.last,run.value});
            }
        }
        if(!value.is_null())next.push_back({range[0],range[1],std::move(value)});
        std::sort(next.begin(),next.end(),[](const Run& a,const Run& b){return a.first<b.first;});
        current.clear();
        for(auto& run:next) {
            if(!current.empty() && current.back().last==run.first && current.back().value==run.value)current.back().last=run.last;
            else current.push_back(std::move(run));
        }
        if(current.empty())edits.erase(key);
    }

    template<class Map>
    void remap(const Map& offset) {
        for(auto* entries:{&source,&edits})for(auto it=entries->begin();it!=entries->end();){
            auto& runs=it->second;
            for(auto& run:runs){
                run.first=offset(run.first,false);
                run.last=offset(run.last,true);
            }
            runs.erase(std::remove_if(runs.begin(),runs.end(),[](const Run& run){return run.last<=run.first;}),runs.end());
            if(runs.empty())it=entries->erase(it);else ++it;
        }
    }

    Json overrideFor(const char* key,std::array<int,2> range)const {
        const auto it=edits.find(key);
        if(it!=edits.end())for(const auto& run:it->second)
            if(run.first<=range[0] && run.last>=range[1])return run.value;
        return nullptr;
    }

    template<class Format>
    std::string current(const char* key,std::array<int,2> range,const Format& format,bool includeEdits=true)const {
        const auto base=source.find(key),desired=includeEdits?edits.find(key):edits.end();
        std::vector<int> boundaries{range[0]};
        if(range[0]<range[1] && base!=source.end())for(const auto& run:base->second)for(int point:{run.first,run.last})
            if(point>range[0] && point<range[1])boundaries.push_back(point);
        if(range[0]<range[1] && desired!=edits.end())for(const auto& run:desired->second)for(int point:{run.first,run.last})
            if(point>range[0] && point<range[1])boundaries.push_back(point);
        std::string result;
        for(int point:boundaries) {
            const auto staged=desired==edits.end()?nullptr:valueAt(desired->second,point);
            const auto original=base==source.end()?nullptr:valueAt(base->second,point);
            const auto value=staged?format(key,*staged):original?original->get<std::string>():"继承";
            if(result.empty())result=value;
            else if(result!=value)return "混合";
        }
        return result;
    }

    template<class Units>
    Json serialize(int bytes,const Units& units)const {
        std::map<std::array<int,2>,Json> patches;
        for(const auto& [key,runs]:edits)for(const auto& run:runs) {
            const auto at=[&](int offset){return units(std::min(offset,bytes))+int(offset>bytes);};
            const std::array<int,2> range={at(run.first),at(run.last)};
            if(range[1]>range[0])patches[range][key]=run.value;
        }
        auto result=Json::array();
        for(auto& [range,patch]:patches) {
            patch["range"]=range;
            result.push_back(std::move(patch));
        }
        return result;
    }
};
