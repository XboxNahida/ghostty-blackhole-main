// capture_dxgi.h  DXGI Desktop Duplication capture
// More stable than WGC: no DWM composition artifacts.
// Frame is the GPU backbuffer copy, not a compositor snapshot.
#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>

struct DXGICapture {
    ID3D11Device*           d3dDev    = nullptr;
    ID3D11DeviceContext*    d3dCtx    = nullptr;
    IDXGIOutputDuplication* dupl      = nullptr;
    ID3D11Texture2D*        stagingTex = nullptr;

    int width  = 0;
    int height = 0;
    bool active = false;
};

// hMon=nullptr → fallback to primary monitor
bool DXGI_Init(DXGICapture& dxgi, HMONITOR hMon);
ID3D11Texture2D* DXGI_GetFrame(DXGICapture& dxgi);
void DXGI_ReleaseFrame(DXGICapture& dxgi);
bool DXGI_CopyToStaging(DXGICapture& dxgi, ID3D11Texture2D* srcTex,
                        D3D11_MAPPED_SUBRESOURCE& mapped);
void DXGI_UnmapStaging(DXGICapture& dxgi);
void DXGI_Release(DXGICapture& dxgi);
