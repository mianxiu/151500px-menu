#pragma once
#include <cstdint>

// Experimental pairing core; no production caller until the PS timing gate passes.
struct ThreadFTicket {
    std::uintptr_t root=0,focus=0,nonce=0;
    std::uint64_t epoch=0;
    bool operator==(const ThreadFTicket& other) const {
        return root==other.root && focus==other.focus && nonce==other.nonce && epoch==other.epoch;
    }
};
struct ThreadFKey {
    ThreadFTicket granted{};
    bool armed=false,owned=false;
    bool arm(const ThreadFTicket& ticket,bool keyDown) {
        if(armed || owned || keyDown || !ticket.root || !ticket.focus || !ticket.nonce || !ticket.epoch)return false;
        granted=ticket;armed=true;return true;
    }
    bool stop(){armed=false;return !owned;}
    // Only after the target actually loses focus: no down was delivered to it.
    void focusLeft(){armed=false;owned=false;}
    template<class Notify> bool key(int code,unsigned vk,std::uint32_t bits,
                                   const ThreadFTicket& current,bool nonInput,bool plain,Notify notify) {
        if(code!=0 || vk!='F')return false;
        const bool up=(bits&0x80000000U)!=0,repeat=(bits&0x40000000U)!=0;
        if(owned){if(up)owned=false;return true;}
        if(up || repeat || !armed || !plain || !nonInput || !(current==granted))return false;
        if(!notify()){armed=false;return false;}
        owned=true;return true;
    }
};
