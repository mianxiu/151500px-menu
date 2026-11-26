#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include "TextThumbnailGesture.h"

namespace TextThumbnailHit {

using TextThumbnailGesture::Point;

struct Rect { int left=0,top=0,right=0,bottom=0; };

struct Pixels {
    const std::uint32_t* data=nullptr;
    int width=0,height=0;

    int neutral(int x,int y) const {
        if(x<0 || y<0 || x>=width || y>=height)return -1;
        auto color=data[y*width+x];
        const int b=color&255,g=(color>>8)&255,r=(color>>16)&255;
        return std::max({r,g,b})-std::min({r,g,b})<=5?g:-1;
    }
};

inline bool closeColor(int a,int b,int tolerance) {
    return a>=0 && b>=0 && std::abs(a-b)<=tolerance;
}

inline bool smartBadge(const Pixels& pixels,const Rect& bounds,double scale) {
    // The native embedded-object badge has a folded top edge and two offset
    // page outlines. Sample their distinct strokes and interiors, independently
    // of the thumbnail artwork under the badge.
    constexpr std::array<Point,12> stroke={{{6,6},{6,11},{8,8},{10,6},{12,9},{15,7},
        {13,12},{9,13},{11,15},{12,4},{8,12},{15,5}}};
    constexpr std::array<Point,10> fill={{{4,8},{5,13},{7,15},{11,6},{12,7},{14,6},
        {8,11},{10,12},{11,13},{15,11}}};
    const auto px=[&](double dip){return int(std::lround(dip*scale));};
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
        const auto sample=[&](Point at){return pixels.neutral(bounds.right-px(at.x)+dx,bounds.bottom-px(at.y-.5)+dy);};
        const int ink=sample(stroke[0]),paper=sample(fill[0]);
        if(ink<0 || paper<0 || std::abs(ink-paper)<42)continue;
        int outlines=0,background=0;
        for(auto point:stroke)if(closeColor(sample(point),ink,24))++outlines;
        for(auto point:fill)if(closeColor(sample(point),paper,20))++background;
        if(outlines>=11 && background>=9)return true;
    }
    return false;
}

inline std::optional<Rect> selectedSmartThumbnail(const Pixels& pixels,Point cursor,double scale) {
    if(!pixels.data || scale<1 || scale>3 || cursor.x<1 || cursor.y<1 || cursor.x>=pixels.width || cursor.y>=pixels.height)
        return {};
    const auto px=[&](double dip){return int(std::lround(dip*scale));};
    const int minArm=px(9),maxArm=px(14),minSize=px(16),maxSize=px(78),padding=px(2);
    const auto matches=[&](int x,int y,int ink){return closeColor(pixels.neutral(x,y),ink,6);};
    const auto run=[&](int x,int y,int ink){
        int count=0;while(count<=maxArm && matches(x+count,y,ink))++count;
        return count;
    };
    std::optional<Rect> found;
    for(int y=std::max(padding,cursor.y-maxSize);y<=cursor.y-padding;++y) {
        for(int x=std::max(padding,cursor.x-maxSize);x<=cursor.x-padding;++x) {
            const int ink=pixels.neutral(x,y);
            if(ink<0 || (ink>30 && ink<225) || matches(x-1,y,ink) || matches(x,y-1,ink))continue;
            const int firstArm=run(x,y,ink);
            if(firstArm<minArm || firstArm>maxArm || !matches(x,y+px(3),ink))continue;
            for(int rightStart=std::max(x+firstArm+padding,cursor.x-maxArm);rightStart<=std::min(pixels.width-1,x+maxSize);++rightStart) {
                if(matches(rightStart-1,y,ink))continue;
                const int secondArm=run(rightStart,y,ink),right=rightStart+secondArm;
                if(secondArm<minArm || secondArm>maxArm || right<cursor.x+padding || right-x>maxSize || right>=pixels.width ||
                   !matches(right-1,y+px(3),ink))continue;
                for(int bottomRow=std::max(y+minSize-1,cursor.y+padding);bottomRow<std::min(pixels.height-padding,y+maxSize);++bottomRow) {
                    if(!matches(x,bottomRow,ink) || !matches(right-1,bottomRow,ink) || matches(x,bottomRow+1,ink))continue;
                    if(run(x,bottomRow,ink)!=firstArm || run(rightStart,bottomRow,ink)!=secondArm ||
                       !matches(x,bottomRow-px(3),ink) || !matches(right-1,bottomRow-px(3),ink))continue;
                    const int middle=(bottomRow+y)/2,backdrop=pixels.neutral(x-1,middle);
                    if(!closeColor(backdrop,pixels.neutral(right,middle),8) || std::abs(backdrop-ink)<34 ||
                       !closeColor(backdrop,pixels.neutral(x,middle),8) || !closeColor(backdrop,pixels.neutral(right-1,middle),8))continue;
                    const Rect outline{x,y,right,bottomRow+1};
                    if(!smartBadge(pixels,outline,scale))continue;
                    const Rect hit{x+padding,y+padding,right-padding,bottomRow+1-padding};
                    if(cursor.x<hit.left || cursor.x>=hit.right || cursor.y<hit.top || cursor.y>=hit.bottom)continue;
                    if(found)return {};
                    found=hit;
                }
            }
        }
    }
    return found;
}

}
