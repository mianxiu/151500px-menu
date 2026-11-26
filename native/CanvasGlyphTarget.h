#pragma once
#include <d2d1_1.h>
#include <wrl/client.h>

// Draw editable glyphs into the current DXGI frame, at the viewport's scale.
// Brushes belong to the device context and survive swap-chain resizing.
class CanvasGlyphTarget {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    ID3D11Device* device=nullptr;
    Ptr<ID2D1DeviceContext> painter;
    Ptr<ID3D11RenderTargetView> frame;
public:
    ID2D1DeviceContext* prepare(ID3D11Device* source){
        if(source==device && painter)return painter.Get();
        reset();
        if(!source)return nullptr;
        Ptr<IDXGIDevice> dxgi;
        Ptr<ID2D1Factory1> factory;
        Ptr<ID2D1Device> drawingDevice;
        if(FAILED(source->QueryInterface(IID_PPV_ARGS(dxgi.GetAddressOf()))) ||
           FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),nullptr,
               reinterpret_cast<void**>(factory.GetAddressOf()))) ||
           FAILED(factory->CreateDevice(dxgi.Get(),drawingDevice.GetAddressOf())) ||
           FAILED(drawingDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,painter.GetAddressOf())))return nullptr;
        device=source;
        painter->SetDpi(96,96);
        painter->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        return painter.Get();
    }
    bool bind(ID3D11RenderTargetView* view){
        if(view==frame.Get())return bool(frame);
        detach();
        if(!view || !painter)return false;
        Ptr<ID3D11Resource> resource;
        Ptr<IDXGISurface> surface;
        Ptr<ID2D1Bitmap1> bitmap;
        view->GetResource(resource.GetAddressOf());
        const auto props=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
        if(FAILED(resource.As(&surface)) ||
           FAILED(painter->CreateBitmapFromDxgiSurface(surface.Get(),props,bitmap.GetAddressOf())))return false;
        painter->SetTarget(bitmap.Get());
        frame=view;
        return true;
    }
    void detach(){if(painter)painter->SetTarget(nullptr);frame.Reset();}
    void reset(){detach();painter.Reset();device=nullptr;}
};
static CanvasGlyphTarget canvasGlyphTarget;
