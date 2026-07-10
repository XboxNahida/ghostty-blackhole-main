// capture_dxgi.cpp  DXGI Desktop Duplication — stable, no DWM composition race
#include "capture_dxgi.h"
#include <cstdio>
#include <windows.h>

bool DXGI_Init(DXGICapture& dxgi, HMONITOR hMon) {
    dxgi.active = false;
    if (!hMon) hMon = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);

    // 用传入 hMon 的真实尺寸（避免 DPI 虚拟化）
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMon, &mi);
    dxgi.width  = mi.rcMonitor.right - mi.rcMonitor.left;
    dxgi.height = mi.rcMonitor.bottom - mi.rcMonitor.top;

    // Create D3D11 device
    D3D_FEATURE_LEVEL featLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION,
        &dxgi.d3dDev, &featLevel, &dxgi.d3dCtx);
    if (FAILED(hr)) {
        fprintf(stderr, "[DXGI] D3D11CreateDevice failed: 0x%08X\n", (unsigned)hr);
        return false;
    }

    // Get DXGI output for primary monitor
    IDXGIDevice* dxgiDev = nullptr;
    hr = dxgi.d3dDev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDev);
    if (FAILED(hr)) { DXGI_Release(dxgi); return false; }

    IDXGIAdapter* adapter = nullptr;
    hr = dxgiDev->GetAdapter(&adapter);
    dxgiDev->Release();
    if (FAILED(hr)) { DXGI_Release(dxgi); return false; }

    // 遍历 outputs 找匹配 hMon 的那个
    IDXGIOutput* output = nullptr;
    bool found = false;
    for (UINT i = 0; adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_OUTPUT_DESC desc;
        if (SUCCEEDED(output->GetDesc(&desc)) && desc.Monitor == hMon) {
            found = true;
            break;
        }
        output->Release();
        output = nullptr;
    }
    adapter->Release();
    if (!found || !output) { DXGI_Release(dxgi); return false; }

    IDXGIOutput1* output1 = nullptr;
    hr = output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);
    output->Release();
    if (FAILED(hr)) { DXGI_Release(dxgi); return false; }

    hr = output1->DuplicateOutput(dxgi.d3dDev, &dxgi.dupl);
    output1->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "[DXGI] DuplicateOutput failed: 0x%08X\n", (unsigned)hr);
        DXGI_Release(dxgi);
        return false;
    }

    // Staging texture will be created on first frame (to match format)
    // DXGI desktop backbuffer may use HDR or different format.


    dxgi.active = true;
    fprintf(stderr, "[DXGI] Capture ready: %dx%d\n", dxgi.width, dxgi.height);
    return true;
}

static bool DXGI_Reinit(DXGICapture& dxgi) {
    // Destroy old duplication
    if (dxgi.dupl) { dxgi.dupl->Release(); dxgi.dupl = nullptr; }
    if (dxgi.stagingTex) { dxgi.stagingTex->Release(); dxgi.stagingTex = nullptr; }

    // Re-enumerate output
    IDXGIDevice* dxgiDev = nullptr;
    HRESULT hr = dxgi.d3dDev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDev);
    if (FAILED(hr)) return false;
    IDXGIAdapter* adapter = nullptr;
    hr = dxgiDev->GetAdapter(&adapter);
    dxgiDev->Release();
    if (FAILED(hr)) return false;
    IDXGIOutput* output = nullptr;
    hr = adapter->EnumOutputs(0, &output);
    adapter->Release();
    if (FAILED(hr)) return false;
    IDXGIOutput1* output1 = nullptr;
    hr = output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);
    output->Release();
    if (FAILED(hr)) return false;
    hr = output1->DuplicateOutput(dxgi.d3dDev, &dxgi.dupl);
    output1->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "[DXGI] Reinit DuplicateOutput failed: 0x%08X\n", (unsigned)hr);
        return false;
    }

    return true;
}

ID3D11Texture2D* DXGI_GetFrame(DXGICapture& dxgi) {
    if (!dxgi.active || !dxgi.dupl) return nullptr;

    IDXGIResource* frameRes = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
    HRESULT hr = dxgi.dupl->AcquireNextFrame(100, &frameInfo, &frameRes);

    if (hr == DXGI_ERROR_ACCESS_LOST) {

        if (!DXGI_Reinit(dxgi)) {
            dxgi.active = false;
            return nullptr;
        }
        // Try again after reinit
        hr = dxgi.dupl->AcquireNextFrame(100, &frameInfo, &frameRes);
    }

    if (FAILED(hr)) {
        static int failCount = 0;
        if (++failCount <= 3)
            fprintf(stderr, "[DXGI] AcquireNextFrame failed: 0x%08X\n", (unsigned)hr);
        return nullptr;
    }

    ID3D11Texture2D* tex = nullptr;
    hr = frameRes->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex);
    frameRes->Release();
    // Always return the texture — caller must always call DXGI_ReleaseFrame
    // after processing, regardless of success/failure.
    return tex;
}

void DXGI_ReleaseFrame(DXGICapture& dxgi) {
    if (dxgi.dupl) dxgi.dupl->ReleaseFrame();
}

bool DXGI_CopyToStaging(DXGICapture& dxgi, ID3D11Texture2D* srcTex,
                        D3D11_MAPPED_SUBRESOURCE& mapped) {

    // Lazy-create staging texture matching source format
    if (!dxgi.stagingTex) {
        D3D11_TEXTURE2D_DESC srcDesc;
        srcTex->GetDesc(&srcDesc);

        D3D11_TEXTURE2D_DESC stagingDesc = {};
        stagingDesc.Width          = srcDesc.Width;
        stagingDesc.Height         = srcDesc.Height;
        stagingDesc.MipLevels      = 1;
        stagingDesc.ArraySize      = 1;
        stagingDesc.Format         = srcDesc.Format;
        stagingDesc.SampleDesc.Count = 1;
        stagingDesc.Usage          = D3D11_USAGE_STAGING;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.BindFlags      = 0;
        HRESULT hr = dxgi.d3dDev->CreateTexture2D(&stagingDesc, nullptr,
                                                   &dxgi.stagingTex);
        if (FAILED(hr)) {
            fprintf(stderr, "[DXGI] Create staging failed: 0x%08X\n", (unsigned)hr);
            return false;
        }
        dxgi.width  = (int)srcDesc.Width;
        dxgi.height = (int)srcDesc.Height;
        fprintf(stderr, "[DXGI] Staging created: %dx%d fmt=%d\n",
                dxgi.width, dxgi.height, (int)srcDesc.Format);
    }

    if (!dxgi.active || !srcTex || !dxgi.stagingTex) return false;
    dxgi.d3dCtx->CopyResource(dxgi.stagingTex, srcTex);
    HRESULT hr = dxgi.d3dCtx->Map(dxgi.stagingTex, 0, D3D11_MAP_READ, 0, &mapped);
    return SUCCEEDED(hr);
}

void DXGI_UnmapStaging(DXGICapture& dxgi) {
    if (dxgi.stagingTex) dxgi.d3dCtx->Unmap(dxgi.stagingTex, 0);
}

void DXGI_Release(DXGICapture& dxgi) {
    if (dxgi.stagingTex) { dxgi.stagingTex->Release(); dxgi.stagingTex = nullptr; }
    if (dxgi.dupl)      { dxgi.dupl->Release();      dxgi.dupl      = nullptr; }
    if (dxgi.d3dCtx)    { dxgi.d3dCtx->Release();    dxgi.d3dCtx    = nullptr; }
    if (dxgi.d3dDev)    { dxgi.d3dDev->Release();    dxgi.d3dDev    = nullptr; }
    dxgi.active = false;
}
