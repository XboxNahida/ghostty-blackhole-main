// d3d11_renderer.cpp  D3D11 渲染器实现
// GPU-only 管线: WGC frame → CopyResource → HLSL Pixel Shader → SwapChain Present
#include "d3d11_renderer.h"
#include <cstdio>
#include <cstring>
#include <d3dcompiler.h>

// ── 链接 d3dcompiler ──
#pragma comment(lib, "d3dcompiler.lib")

// ── 全屏四边形顶点结构 ──
struct QuadVertex {
    float x, y;
    float u, v;
};

// 全屏四边形: 两个三角形覆盖整个屏幕 [-1,1]×[-1,1], UV [0,1]×[0,1]
static const QuadVertex kQuadVertices[4] = {
    {-1.0f,  1.0f, 0.0f, 0.0f},  // top-left:     uv(0,0)
    { 1.0f,  1.0f, 1.0f, 0.0f},  // top-right:    uv(1,0)
    {-1.0f, -1.0f, 0.0f, 1.0f},  // bottom-left:  uv(0,1)
    { 1.0f, -1.0f, 1.0f, 1.0f},  // bottom-right: uv(1,1)
};

// ── 内嵌 HLSL 源码 (编译期嵌入, 也可从文件加载) ──
static const char* kVertexShaderSrc = R"(
struct VSInput {
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
};
struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};
VSOutput main(VSInput input) {
    VSOutput output;
    output.pos = float4(input.pos, 0.0, 1.0);
    output.uv  = input.uv;
    return output;
}
)";

// 像素着色器从外部文件加载 (build_shader.ps1)
static const char* kPixelShaderPath = "shaders/blackhole.hlsl";

// ── 默认像素着色器 (后备: 简单采样桌面纹理) ──
static const char* kFallbackPS = R"(
Texture2D    iChannel0 : register(t0);
SamplerState iChannel0Sampler : register(s0);
struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};
float4 main(PSInput input) : SV_TARGET {
    return iChannel0.Sample(iChannel0Sampler, input.uv);
}
)";

// ═══════════════════════════════════════════════════════════════════════
// 公开 API
// ═══════════════════════════════════════════════════════════════════════

bool D3D11Renderer::Init(HWND hwnd, int width, int height,
                         ID3D11Device* sharedDev,
                         ID3D11DeviceContext* sharedCtx) {
    if (active_) Shutdown();

    width_  = width;
    height_ = height;

    // 1. 确定设备来源
    if (sharedDev && sharedCtx) {
        device_  = sharedDev;
        context_ = sharedCtx;
        ownDevice_ = false;
        device_->AddRef();
        context_->AddRef();
    } else {
        // 创建独立设备
        D3D_FEATURE_LEVEL featLevel;
        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0, D3D11_SDK_VERSION,
            &device_, &featLevel, &context_);
        if (FAILED(hr)) {
            fprintf(stderr, "[D3D11R] CreateDevice failed: 0x%08X\n", (unsigned)hr);
            return false;
        }
        ownDevice_ = true;
    }

    // 2. SwapChain
    if (!CreateSwapChain(hwnd, width, height)) return false;

    // 3. Shader
    if (!CompileShaders()) return false;

    // 4. 全屏四边形
    if (!CreateFullscreenQuad()) return false;

    // 5. 桌面纹理 (稍后 Resize 时更新尺寸)
    if (!CreateDesktopTexture()) return false;

    // 6. Sampler
    if (!CreateSamplerState()) return false;

    active_ = true;
    fprintf(stderr, "[D3D11R] Initialized: %dx%d\n", width, height);
    return true;
}

void D3D11Renderer::Render(const TextureFrame& frame,
                           const BlackHoleUniforms& uniforms) {
    if (!active_) return;

    // 1. 如果需要, 调整桌面纹理大小
    if (frame.valid && frame.d3dTex) {
        D3D11_TEXTURE2D_DESC desc;
        frame.d3dTex->GetDesc(&desc);
        if ((int)desc.Width != texWidth_ || (int)desc.Height != texHeight_) {
            // 重新创建桌面纹理
            if (desktopSRV_) { desktopSRV_->Release(); desktopSRV_ = nullptr; }
            if (desktopTex_) { desktopTex_->Release(); desktopTex_ = nullptr; }

            D3D11_TEXTURE2D_DESC td = {};
            td.Width          = desc.Width;
            td.Height         = desc.Height;
            td.MipLevels      = 1;
            td.ArraySize      = 1;
            td.Format         = desc.Format;
            td.SampleDesc.Count = 1;
            td.Usage          = D3D11_USAGE_DEFAULT;
            td.BindFlags      = D3D11_BIND_SHADER_RESOURCE;

            HRESULT hr = device_->CreateTexture2D(&td, nullptr, &desktopTex_);
            if (FAILED(hr)) return;

            D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
            srvd.Format                    = td.Format;
            srvd.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvd.Texture2D.MipLevels       = 1;
            srvd.Texture2D.MostDetailedMip = 0;

            hr = device_->CreateShaderResourceView(desktopTex_, &srvd, &desktopSRV_);
            if (FAILED(hr)) return;

            texWidth_  = desc.Width;
            texHeight_ = desc.Height;
        }

        // GPU-only: CopyResource 从 WGC 帧到桌面纹理
        context_->CopyResource(desktopTex_, frame.d3dTex);
    }

    // 2. 更新 Constant Buffer
    if (constBuf_) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(context_->Map(constBuf_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, &uniforms, sizeof(BlackHoleUniforms));
            context_->Unmap(constBuf_, 0);
        }
    }

    // 3. 设置渲染目标
    context_->OMSetRenderTargets(1, &rtv_, nullptr);

    // 4. 视口
    D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)width_, (float)height_, 0.0f, 1.0f };
    context_->RSSetViewports(1, &vp);

    // 5. 清除背景
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    if (rtv_) context_->ClearRenderTargetView(rtv_, clearColor);

    // 6. 绑定 Shader
    context_->VSSetShader(vs_, nullptr, 0);
    context_->PSSetShader(ps_, nullptr, 0);

    // 7. 绑定纹理
    if (desktopSRV_)
        context_->PSSetShaderResources(0, 1, &desktopSRV_);
    if (samplerLinear_)
        context_->PSSetSamplers(0, 1, &samplerLinear_);

    // 8. 绑定 Constant Buffer
    if (constBuf_)
        context_->PSSetConstantBuffers(0, 1, &constBuf_);

    // 9. 绑定顶点缓冲
    UINT stride = sizeof(QuadVertex);
    UINT offset = 0;
    context_->IASetVertexBuffers(0, 1, &quadVB_, &stride, &offset);
    context_->IASetInputLayout(inputLayout_);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // 10. 绘制
    context_->Draw(4, 0);

    // 11. 呈现
    if (swapChain_) swapChain_->Present(1, 0);
}

void D3D11Renderer::Resize(int width, int height) {
    if (!active_ || !swapChain_) return;

    width_  = width;
    height_ = height;

    // 释放旧的 RTV
    if (rtv_) { rtv_->Release(); rtv_ = nullptr; }

    // 调整 SwapChain 缓冲区大小
    HRESULT hr = swapChain_->ResizeBuffers(0, width, height,
                                           DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D11R] ResizeBuffers failed: 0x%08X\n", (unsigned)hr);
        return;
    }

    // 重新获取 RTV
    ID3D11Texture2D* backBuffer = nullptr;
    hr = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D),
                               (void**)&backBuffer);
    if (SUCCEEDED(hr)) {
        device_->CreateRenderTargetView(backBuffer, nullptr, &rtv_);
        backBuffer->Release();
    }
}

void D3D11Renderer::Shutdown() {
    CleanupResources();

    if (context_) { context_->Release(); context_ = nullptr; }
    if (device_)  { device_->Release();  device_  = nullptr; }
    ownDevice_ = false;
    active_    = false;
    fprintf(stderr, "[D3D11R] Shutdown complete\n");
}

// ═══════════════════════════════════════════════════════════════════════
// 内部实现
// ═══════════════════════════════════════════════════════════════════════

bool D3D11Renderer::CreateSwapChain(HWND hwnd, int w, int h) {
    // 获取 DXGI 工厂
    IDXGIDevice* dxgiDev = nullptr;
    HRESULT hr = device_->QueryInterface(__uuidof(IDXGIDevice),
                                         (void**)&dxgiDev);
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D11R] QueryInterface IDXGIDevice failed\n");
        return false;
    }

    IDXGIAdapter* adapter = nullptr;
    dxgiDev->GetAdapter(&adapter);
    dxgiDev->Release();

    IDXGIFactory* factory = nullptr;
    if (adapter) {
        adapter->GetParent(__uuidof(IDXGIFactory), (void**)&factory);
        adapter->Release();
    }
    if (!factory) {
        fprintf(stderr, "[D3D11R] Get DXGI factory failed\n");
        return false;
    }

    // SwapChain 描述
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferDesc.Width     = w;
    sd.BufferDesc.Height    = h;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format    = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count     = 1;
    sd.BufferUsage          = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount          = 2;
    sd.OutputWindow         = hwnd;
    sd.Windowed             = TRUE;
    sd.SwapEffect           = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    hr = factory->CreateSwapChain(device_, &sd, &swapChain_);
    factory->Release();

    if (FAILED(hr)) {
        fprintf(stderr, "[D3D11R] CreateSwapChain failed: 0x%08X\n", (unsigned)hr);
        return false;
    }

    // 获取 BackBuffer RTV
    ID3D11Texture2D* backBuffer = nullptr;
    hr = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D),
                               (void**)&backBuffer);
    if (SUCCEEDED(hr)) {
        device_->CreateRenderTargetView(backBuffer, nullptr, &rtv_);
        backBuffer->Release();
    }

    return true;
}

bool D3D11Renderer::CompileShaders() {
    HRESULT hr;

    // ── 顶点着色器 (内嵌) ──
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* vsErr  = nullptr;
    hr = D3DCompile(kVertexShaderSrc, strlen(kVertexShaderSrc),
                    "fullscreen_vs", nullptr, nullptr,
                    "main", "vs_4_0", 0, 0, &vsBlob, &vsErr);
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D11R] VS compile error:\n%s\n",
                vsErr ? (char*)vsErr->GetBufferPointer() : "unknown");
        if (vsErr) vsErr->Release();
        return false;
    }

    hr = device_->CreateVertexShader(vsBlob->GetBufferPointer(),
                                     vsBlob->GetBufferSize(),
                                     nullptr, &vs_);
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D11R] CreateVertexShader failed\n");
        vsBlob->Release();
        return false;
    }

    // ── 输入布局 ──
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 8,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = device_->CreateInputLayout(layout, 2,
                                    vsBlob->GetBufferPointer(),
                                    vsBlob->GetBufferSize(),
                                    &inputLayout_);
    vsBlob->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D11R] CreateInputLayout failed\n");
        return false;
    }

    // ── 像素着色器: 尝试从文件加载, 失败则用后备 ──
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* psErr  = nullptr;

    // 尝试加载外部编译的 .hlsl 文件 (预编译版本)
    HANDLE hFile = CreateFileA(kPixelShaderPath, GENERIC_READ,
                               FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD size = GetFileSize(hFile, nullptr);
        if (size > 0) {
            char* src = new char[size + 1];
            DWORD read;
            ReadFile(hFile, src, size, &read, nullptr);
            src[size] = 0;
            CloseHandle(hFile);

            hr = D3DCompile(src, size, "blackhole_ps", nullptr, nullptr,
                           "main", "ps_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3,
                           0, &psBlob, &psErr);
            delete[] src;

            if (FAILED(hr)) {
                fprintf(stderr, "[D3D11R] PS compile error:\n%s\n",
                        psErr ? (char*)psErr->GetBufferPointer() : "unknown");
                if (psErr) psErr->Release();

                // 回退到简单采样
                fprintf(stderr, "[D3D11R] Falling back to simple PS\n");
                hr = D3DCompile(kFallbackPS, strlen(kFallbackPS),
                               "fallback_ps", nullptr, nullptr,
                               "main", "ps_4_0", 0, 0, &psBlob, nullptr);
            }
        } else {
            CloseHandle(hFile);
        }
    } else {
        // 文件不存在, 使用后备着色器
        fprintf(stderr, "[D3D11R] PS file not found, using fallback\n");
        hr = D3DCompile(kFallbackPS, strlen(kFallbackPS),
                       "fallback_ps", nullptr, nullptr,
                       "main", "ps_4_0", 0, 0, &psBlob, nullptr);
    }

    if (SUCCEEDED(hr) && psBlob) {
        hr = device_->CreatePixelShader(psBlob->GetBufferPointer(),
                                        psBlob->GetBufferSize(),
                                        nullptr, &ps_);
        psBlob->Release();
    }

    if (!ps_) {
        fprintf(stderr, "[D3D11R] No pixel shader available\n");
        return false;
    }

    // ── Constant Buffer ──
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth      = sizeof(BlackHoleUniforms);
    cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device_->CreateBuffer(&cbDesc, nullptr, &constBuf_);
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D11R] Create constant buffer failed\n");
        return false;
    }

    fprintf(stderr, "[D3D11R] Shaders compiled\n");
    return true;
}

bool D3D11Renderer::CreateFullscreenQuad() {
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth      = sizeof(kQuadVertices);
    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = kQuadVertices;

    HRESULT hr = device_->CreateBuffer(&bd, &initData, &quadVB_);
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D11R] Create vertex buffer failed\n");
        return false;
    }
    return true;
}

bool D3D11Renderer::CreateDesktopTexture() {
    // 创建一个临时 1x1 纹理, 后续每帧根据实际 WGC 帧尺寸调整
    D3D11_TEXTURE2D_DESC td = {};
    td.Width          = 1;
    td.Height         = 1;
    td.MipLevels      = 1;
    td.ArraySize      = 1;
    td.Format         = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage          = D3D11_USAGE_DEFAULT;
    td.BindFlags      = D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device_->CreateTexture2D(&td, nullptr, &desktopTex_);
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format                    = td.Format;
    srvd.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels       = 1;
    srvd.Texture2D.MostDetailedMip = 0;

    hr = device_->CreateShaderResourceView(desktopTex_, &srvd, &desktopSRV_);
    if (FAILED(hr)) return false;

    texWidth_  = 1;
    texHeight_ = 1;
    return true;
}

bool D3D11Renderer::CreateSamplerState() {
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

    HRESULT hr = device_->CreateSamplerState(&sd, &samplerLinear_);
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D11R] Create sampler failed\n");
        return false;
    }
    return true;
}

void D3D11Renderer::CleanupResources() {
    if (samplerLinear_) { samplerLinear_->Release(); samplerLinear_ = nullptr; }
    if (constBuf_)      { constBuf_->Release();      constBuf_      = nullptr; }
    if (desktopSRV_)    { desktopSRV_->Release();     desktopSRV_    = nullptr; }
    if (desktopTex_)    { desktopTex_->Release();     desktopTex_    = nullptr; }
    if (inputLayout_)   { inputLayout_->Release();    inputLayout_   = nullptr; }
    if (ps_)            { ps_->Release();             ps_            = nullptr; }
    if (vs_)            { vs_->Release();             vs_            = nullptr; }
    if (quadVB_)        { quadVB_->Release();         quadVB_        = nullptr; }
    if (rtv_)           { rtv_->Release();            rtv_           = nullptr; }
    if (swapChain_)     { swapChain_->Release();      swapChain_     = nullptr; }
}
