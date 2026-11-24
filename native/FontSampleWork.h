#pragma once
#include "FontSampleRaster.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>

// The font, Direct2D and WIC objects stay on this worker. Only CPU pixels
// cross to the render thread, which owns the Direct3D textures.
class FontSampleWork {
public:
    struct Sample {
        FontSampleRaster::Result status;
        std::vector<unsigned char> pixels;
        int frame=0;
    };
private:
    struct Job {
        uint64_t scope;
        int frame;
        std::string id;
        std::wstring face,sample;
        int width,height;
        float em;
    };
    std::mutex lock;
    std::condition_variable changed;
    std::deque<Job> queued;
    std::map<std::string,Sample> ready;
    uint64_t scope=0,runningScope=0;
    std::string runningFace;
    int frame=0,runningFrame=0;
    bool stop=false;
    std::thread worker;

    void run(){
        const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        {
            FontSampleRaster raster;
            for(;;){
                std::unique_lock guard(lock);
                changed.wait(guard,[&]{return stop || !queued.empty();});
                if(stop)break;
                auto job=std::move(queued.front());queued.pop_front();
                if(job.scope!=scope || job.frame+1<frame)continue;
                runningScope=job.scope;runningFace=job.id;runningFrame=job.frame;
                guard.unlock();
                Sample result{FontSampleRaster::Result::unavailable,{}};
                if(SUCCEEDED(com))result.status=raster.render(job.face,job.sample,job.width,job.height,job.em,result.pixels);
                guard.lock();
                result.frame=runningFrame;
                runningScope=0;runningFace.clear();
                if(job.scope==scope && result.frame+1>=frame && ready.size()<12)ready.emplace(std::move(job.id),std::move(result));
            }
        }
        if(SUCCEEDED(com))CoUninitialize();
    }
public:
    FontSampleWork():worker(&FontSampleWork::run,this){}
    ~FontSampleWork(){
        {std::lock_guard guard(lock);stop=true;}
        changed.notify_one();worker.join();
    }
    uint64_t begin(){
        std::lock_guard guard(lock);
        queued.clear();ready.clear();frame=0;
        return ++scope;
    }
    void cancel(uint64_t ticket){
        std::lock_guard guard(lock);
        if(ticket==scope){++scope;queued.clear();ready.clear();}
    }
    void nextFrame(uint64_t ticket,int currentFrame){
        std::lock_guard guard(lock);
        if(ticket!=scope)return;
        frame=currentFrame;
        for(auto it=ready.begin();it!=ready.end();){
            if(it->second.frame+12<frame)it=ready.erase(it);
            else ++it;
        }
    }
    std::optional<Sample> request(uint64_t ticket,int currentFrame,const std::string& id,
                                  std::wstring face,const std::wstring& sample,int width,int height,float em){
        std::lock_guard guard(lock);
        if(ticket!=scope)return {};
        frame=currentFrame;
        if(auto found=ready.find(id);found!=ready.end()){
            auto result=std::move(found->second);ready.erase(found);
            return result;
        }
        if(runningScope==scope && runningFace==id){runningFrame=frame;return {};}
        for(auto& job:queued)if(job.id==id){job.frame=frame;return {};}
        while(!queued.empty() && queued.front().frame+1<frame)queued.pop_front();
        if(queued.size()<12){
            queued.push_back({scope,frame,id,std::move(face),sample,width,height,em});
            changed.notify_one();
        }
        return {};
    }
};

inline FontSampleWork& fontSampleWork(){
    static FontSampleWork worker;
    return worker;
}
