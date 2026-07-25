// d3d11_renderer.h  D3D11 渲染器实现 (IRenderer)
// 使用共享 D3D11 Device, 通过 DXGI SwapChain 输出
// GPU-only 管线: WGC 纹理 → CopyResource → ShaderResource → HLSL → Present
#pragma once
#include "renderer_interface.h"
#include <d3d11.h>
#include <dxgi.h>
#include <string>

class D3D11Renderer : public IRenderer {
public:
    D3D11Renderer() = default;
    ~D3D11Renderer() override { Shutdown(); }

    bool Init(HWND hwnd, int width, int height,
              ID3D11Device* sharedDev = nullptr,
              ID3D11DeviceContext* sharedCtx = nullptr) override;

    void Render(const TextureFrame& frame,
                const BlackHoleUniforms& uniforms) override;

    void Resize(int width, int height) override;
    void Shutdown() override;
    bool IsActive() const override { return active_; }

private:
    bool CreateSwapChain(HWND hwnd, int w, int h);
    bool CompileShaders();
    bool CreateFullscreenQuad();
    bool CreateDesktopTexture();
    bool CreateSamplerState();
    void CleanupResources();

    // 共享设备
    ID3D11Device*        device_  = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    bool                 ownDevice_ = false;  // 是否自有 device (非共享时需释放)

    // SwapChain
    IDXGISwapChain*         swapChain_ = nullptr;
    ID3D11RenderTargetView* rtv_       = nullptr;

    // Shader
    ID3D11VertexShader* vs_           = nullptr;
    ID3D11PixelShader*  ps_           = nullptr;
    ID3D11InputLayout*  inputLayout_  = nullptr;

    // 全屏四边形
    ID3D11Buffer* quadVB_ = nullptr;

    // 桌面纹理 (接收 WGC 帧副本)
    ID3D11Texture2D*          desktopTex_ = nullptr;
    ID3D11ShaderResourceView* desktopSRV_ = nullptr;
    int texWidth_  = 0;
    int texHeight_ = 0;

    // Sampler
    ID3D11SamplerState* samplerLinear_ = nullptr;

    // Constant Buffer
    ID3D11Buffer* constBuf_ = nullptr;
    ID3D11Query* copyFence_ = nullptr;

    bool active_ = false;
    int  width_  = 0;
    int  height_ = 0;

};
