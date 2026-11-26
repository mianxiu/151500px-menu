#pragma once
#include <array>
#include <cmath>

struct CanvasViewTransform {
    ImVec2 center;
    float sine,cosine;
    CanvasViewTransform(ImVec2 center,float degrees):center(center),
        sine(std::sin(degrees*(IM_PI/180.f))),cosine(std::cos(degrees*(IM_PI/180.f))){}
    ImVec2 apply(ImVec2 local)const{
        const float x=local.x-center.x,y=local.y-center.y;
        return {center.x+x*cosine-y*sine,center.y+x*sine+y*cosine};
    }
    ImVec2 unapply(ImVec2 screen)const{
        const float x=screen.x-center.x,y=screen.y-center.y;
        return {center.x+x*cosine+y*sine,center.y-x*sine+y*cosine};
    }
    std::array<ImVec2,4> quad(ImVec2 min,ImVec2 max)const{
        return {apply(min),apply({max.x,min.y}),apply(max),apply({min.x,max.y})};
    }
    ImGuiMouseCursor resizeCursor(int handle)const{
        const float x=handle==2?0.f:handle&4?-1.f:1.f;
        const float y=handle&2?1.f:0.f;
        const auto direction=std::lround(std::atan2(x*sine+y*cosine,x*cosine-y*sine)/(IM_PI/4.f));
        constexpr ImGuiMouseCursor cursors[]={ImGuiMouseCursor_ResizeEW,ImGuiMouseCursor_ResizeNWSE,
            ImGuiMouseCursor_ResizeNS,ImGuiMouseCursor_ResizeNESW};
        return cursors[(direction+4)%4];
    }
};
