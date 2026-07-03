// capture_wgc.h  Windows Graphics Capture (no DXGI Duplication)
// WGC captures monitor desktop into D3D11 textures.
// Cross-GPU: works on NVIDIA / AMD / Intel.
#pragma once
#include <d3d11.h>
#include <cstdio>

struct WGCCapture {
    ID3D11Device*        d3dDev     = nullptr;
    ID3D11DeviceContext* d3dCtx     = nullptr;

    // WinRT capture objects (opaque IUnknown*)
    IUnknown*            framePool   = nullptr;
    IUnknown*            session     = nullptr;
    IUnknown*            captureItem = nullptr;

    // Staging texture for GPU-CPU readback
    ID3D11Texture2D*     stagingTex = nullptr;

    int width  = 0;
    int height = 0;
    bool active = false;
};

// Init: creates D3D11 device + WGC session for specified monitor
// hMon=nullptr → fallback to primary monitor
bool WGC_Init(WGCCapture& wgc, HMONITOR hMon);

// Get latest captured frame (caller must Release)
ID3D11Texture2D* WGC_GetFrame(WGCCapture& wgc);

// Copy frame to staging, then Map for CPU read
bool WGC_CopyToStaging(WGCCapture& wgc, ID3D11Texture2D* srcTex,
                       D3D11_MAPPED_SUBRESOURCE& mapped);

void WGC_UnmapStaging(WGCCapture& wgc);

void WGC_Release(WGCCapture& wgc);

// 将调试日志重定向到指定文件（默认 stderr）
void WGC_SetDebugLog(FILE* f);

// 探测当前系统是否支持 WGC 边框抑制（Win11 22H2+ 的 IsBorderRequired API）
// 调用方据此决定用 WGC 还是回退 DXGI
// 返回值: true=支持边框抑制（可用 WGC）, false=不支持（建议用 DXGI）
bool WGC_ProbeBorderSupport();
