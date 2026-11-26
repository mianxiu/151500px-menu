#pragma once

#include <windows.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "TextThumbnailHit.h"

namespace TextThumbnailObserver {

struct Request {
    HWND owner=nullptr,panel=nullptr;
    std::uint64_t identity=0,expires=0;

    bool same(const Request& other) const {
        return owner==other.owner && panel==other.panel && identity==other.identity && expires==other.expires;
    }
};

class Observer {
    using Target=TextThumbnailGesture::Target;
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    Request request{};
    Target target{};
    std::uint64_t sampledAt=0;
    bool closing=false;

    static Target read(const Request& request) {
        if(!request.identity || !request.panel || !request.owner || GetTickCount64()>request.expires ||
           GetForegroundWindow()!=request.owner || !IsWindowVisible(request.panel))return {};
        POINT cursor{};
        RECT panel{};
        if(!GetCursorPos(&cursor) || !GetWindowRect(request.panel,&panel) || !PtInRect(&panel,cursor) ||
           WindowFromPoint(cursor)!=request.panel)return {};
        using WindowDpi=UINT(WINAPI*)(HWND);
        static const auto windowDpi=reinterpret_cast<WindowDpi>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow"));
        const auto dpi=windowDpi?windowDpi(request.panel):0;
        if(dpi<96 || dpi>288)return {};
        const int radius=MulDiv(82,dpi,96);
        if(cursor.x>=panel.left+MulDiv(180,dpi,96))return {};
        const int left=std::max(static_cast<int>(panel.left),static_cast<int>(cursor.x)-radius);
        const int top=std::max(static_cast<int>(panel.top),static_cast<int>(cursor.y)-radius);
        const int right=std::min(static_cast<int>(panel.right),static_cast<int>(cursor.x)+radius);
        const int bottom=std::min(static_cast<int>(panel.bottom),static_cast<int>(cursor.y)+radius);
        const int width=right-left,height=bottom-top;
        if(width<8 || height<8)return {};

        HDC screen=GetDC(nullptr);
        if(!screen)return {};
        HDC scratch=CreateCompatibleDC(screen);
        BITMAPINFO bitmap{};
        bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        bitmap.bmiHeader.biWidth=width;
        bitmap.bmiHeader.biHeight=-height;
        bitmap.bmiHeader.biPlanes=1;
        bitmap.bmiHeader.biBitCount=32;
        void* bytes=nullptr;
        HBITMAP dib=scratch?CreateDIBSection(scratch,&bitmap,DIB_RGB_COLORS,&bytes,nullptr,0):nullptr;
        HGDIOBJ previous=dib?SelectObject(scratch,dib):nullptr;
        Target result{};
        if(previous && previous!=HGDI_ERROR && bytes && BitBlt(scratch,0,0,width,height,screen,left,top,SRCCOPY)) {
            GdiFlush();
            const auto hit=TextThumbnailHit::selectedSmartThumbnail({static_cast<const std::uint32_t*>(bytes),width,height},
                {static_cast<int>(cursor.x)-left,static_cast<int>(cursor.y)-top},dpi/96.0);
            if(hit && GetForegroundWindow()==request.owner && WindowFromPoint(cursor)==request.panel)
                result=Target{request.identity,reinterpret_cast<std::uintptr_t>(request.panel),left+hit->left,top+hit->top,left+hit->right,top+hit->bottom};
        }
        if(previous && previous!=HGDI_ERROR)SelectObject(scratch,previous);
        if(dib)DeleteObject(dib);
        if(scratch)DeleteDC(scratch);
        ReleaseDC(nullptr,screen);
        return result;
    }

    void run() {
        SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
        std::unique_lock<std::mutex> lock(mutex);
        while(!closing) {
            const Request current=request;
            if(!current.identity || GetTickCount64()>current.expires) {
                wake.wait(lock,[&]{return closing || !request.same(current);});
                continue;
            }
            lock.unlock();
            const auto captured=read(current);
            const auto completed=GetTickCount64();
            lock.lock();
            if(request.same(current)) {
                target=captured;
                sampledAt=completed;
            }
            wake.wait_for(lock,std::chrono::milliseconds(32),[&]{return closing || !request.same(current);});
        }
    }

public:
    void start() {worker=std::thread([this]{run();});}
    void update(const Request& next) {
        std::lock_guard<std::mutex> lock(mutex);
        if(request.same(next))return;
        if(request.identity!=next.identity || request.panel!=next.panel || request.owner!=next.owner)target={};
        request=next;
        wake.notify_one();
    }
    Target current(const Request& expected,std::uint64_t now) {
        std::unique_lock<std::mutex> lock(mutex,std::try_to_lock);
        if(!lock || !request.same(expected) || now<sampledAt || now-sampledAt>95 || now>request.expires)return {};
        return target;
    }
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            closing=true;
            wake.notify_one();
        }
        if(worker.joinable())worker.join();
    }
};

}
