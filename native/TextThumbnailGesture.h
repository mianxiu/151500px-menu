#pragma once

#include <cstdint>

namespace TextThumbnailGesture {

struct Point {
    int x=0, y=0;
};

struct Target {
    std::uint64_t identity=0;
    std::uintptr_t panel=0;
    int left=0, top=0, right=0, bottom=0;

    bool contains(Point point) const {
        return identity && panel && left<=point.x && point.x<right && top<=point.y && point.y<bottom;
    }

    bool same(const Target& other) const {
        return identity && identity==other.identity && panel==other.panel && left==other.left &&
            top==other.top && right==other.right && bottom==other.bottom;
    }
};

// The first click stays in Photoshop. A claimed second down always owns
// its matching up, even if focus or the target changes before release.
class Clicks {
    enum class Phase { idle, firstDown, firstUp, claimed };
    Phase phase=Phase::idle;
    Target original{};
    std::uintptr_t panel=0;
    Point first{};
    std::uint64_t pressedAt=0;
    bool cancelled=false;

    static bool withinClickDistance(Point a,Point b,int halfWidth,int halfHeight) {
        const auto x=static_cast<std::int64_t>(a.x)-b.x;
        const auto y=static_cast<std::int64_t>(a.y)-b.y;
        return -halfWidth<=x && x<=halfWidth && -halfHeight<=y && y<=halfHeight;
    }

public:
    bool tracking() const {return phase!=Phase::idle;}
    bool ownsSecondClick() const {return phase==Phase::claimed;}

    void cancel() {
        if(phase==Phase::claimed)cancelled=true;
        else phase=Phase::idle;
    }

    void move(Point point,int halfWidth,int halfHeight) {
        if(phase!=Phase::idle && !withinClickDistance(first,point,halfWidth,halfHeight))cancel();
    }

    bool down(std::uintptr_t pointedPanel,const Target& target,Point point,std::uint64_t now,std::uint64_t timeout,
              int halfWidth,int halfHeight,bool canClaim) {
        if(phase==Phase::claimed)return true;
        const bool valid=pointedPanel && pointedPanel==target.panel && target.contains(point);
        if(valid && phase==Phase::firstUp && canClaim && pointedPanel==panel && target.contains(first) &&
           (!original.identity || target.same(original)) &&
           now>=pressedAt && now-pressedAt<=timeout && withinClickDistance(first,point,halfWidth,halfHeight)) {
            phase=Phase::claimed;
            original=target;
            cancelled=false;
            return true;
        }
        phase=pointedPanel?Phase::firstDown:Phase::idle;
        panel=pointedPanel;
        original=valid?target:Target{};
        first=point;
        pressedAt=now;
        return false;
    }

    bool up(std::uintptr_t pointedPanel,const Target& target,Point point,std::uint64_t now,std::uint64_t timeout,
            std::uint64_t& activated) {
        activated=0;
        const bool samePanel=pointedPanel && pointedPanel==panel;
        const bool timely=now>=pressedAt && now-pressedAt<=timeout;
        if(phase==Phase::claimed) {
            phase=Phase::idle;
            if(samePanel && timely && target.same(original) && target.contains(point) && !cancelled)activated=original.identity;
            return true;
        }
        phase=phase==Phase::firstDown && samePanel && timely?Phase::firstUp:Phase::idle;
        return false;
    }
};

}
