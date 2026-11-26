#pragma once
#include <array>
#include <cmath>
#include <d3d11.h>
#include <wrl/client.h>
#include "backends/imgui_impl_dx11.h"

// The Photoshop light checkerboard is a document backdrop and remains
// constant across interface themes, DPI settings and document zoom levels.
class CanvasTransparency {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> image;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
    static constexpr ImU32 light=IM_COL32(255,255,255,255),dark=IM_COL32(205,205,205,255);
    static constexpr float tile=16.f; // Two eight-screen-pixel squares.
public:
    template<class Load>
    void draw(ImDrawList& draw,const std::array<ImVec2,4>& corners,ID3D11Device* device,const Load& load){
        if(!image){
            constexpr ImU32 pixels[]={light,dark,dark,light};
            load(pixels,image.GetAddressOf());
        }
        if(!sampler && device){
            D3D11_SAMPLER_DESC desc{};
            desc.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;
            desc.AddressU=desc.AddressV=desc.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
            desc.ComparisonFunc=D3D11_COMPARISON_NEVER;
            desc.MaxLOD=D3D11_FLOAT32_MAX;
            device->CreateSamplerState(&desc,sampler.GetAddressOf());
        }
        if(!image || !sampler){draw.AddQuadFilled(corners[0],corners[1],corners[2],corners[3],light);return;}
        const auto uv=[&](ImVec2 point){
            return ImVec2{(point.x-std::floor(corners[0].x))/tile,(point.y-std::floor(corners[0].y))/tile};
        };
        draw.AddCallback([](const ImDrawList*,const ImDrawCmd* command){
            const auto state=static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
            auto* wrapping=static_cast<ID3D11SamplerState*>(command->UserCallbackData);
            state->DeviceContext->PSSetSamplers(0,1,&wrapping);
        },sampler.Get());
        draw.AddImageQuad((ImTextureID)(intptr_t)image.Get(),corners[0],corners[1],corners[2],corners[3],
            uv(corners[0]),uv(corners[1]),uv(corners[2]),uv(corners[3]));
        draw.AddCallback([](const ImDrawList*,const ImDrawCmd*){
            const auto state=static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
            state->DeviceContext->PSSetSamplers(0,1,&state->SamplerLinear);
        },nullptr);
    }
};
