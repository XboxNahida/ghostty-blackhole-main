// capture_wgc.cpp  Windows Graphics Capture using WinRT C++ ABI
// No C++/WinRT dependency. Cross-GPU (NVIDIA / AMD / Intel).
#include "capture_wgc.h"

// Enable IID_* macros in WIDL-generated headers
#define WIDL_using_Windows_Graphics_DirectX_Direct3D11
#define WIDL_using_Windows_Graphics_Capture
#define WIDL_using_Windows_Graphics

#include <cstdio>
#include <windows.h>
#include <winstring.h>
#include <roapi.h>

#include <initguid.h>

// WinRT ABI headers (MinGW WIDL-generated)
#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.h>


// IDirect3DDxgiInterfaceAccess -- not in MinGW WIDL headers.
// {A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1}
static const GUID IID_IDirect3DDxgiInterfaceAccess = {
    0xA9B3D012, 0x3DF2, 0x4EE3, {0xB8,0xD1,0x86,0x95,0xF4,0x57,0xD3,0xC1}};
struct IDirect3DDxgiInterfaceAccess : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetInterface(
        REFIID riid, void **p) = 0;
};

// ---- Convenience namespace aliases ----
namespace WGC = ABI::Windows::Graphics::Capture;
namespace WGD = ABI::Windows::Graphics::DirectX;
namespace WGD3D = ABI::Windows::Graphics::DirectX::Direct3D11;

using WGC::IDirect3D11CaptureFramePoolStatics;
using WGC::IDirect3D11CaptureFramePool;
using WGC::IGraphicsCaptureSession;
using WGC::IGraphicsCaptureSession2;
using WGC::IGraphicsCaptureSession3;
using WGC::IDirect3D11CaptureFrame;
using WGC::IGraphicsCaptureItem;
using WGD3D::IDirect3DDevice;
using WGD3D::IDirect3DSurface;
using ABI::Windows::Graphics::SizeInt32;

// GUID for IDirect3DDevice (not exported as IID_ in MinGW WIDL)
// {A37624AB-8D5F-4650-9D3E-9EAE3D9BC670}
// IGraphicsCaptureSession2: {2C39AE40-7D2E-5044-804E-8B6799D4CF9E}
static const GUID IID_IGraphicsCaptureSession2_WGC = {
    0x2c39ae40, 0x7d2e, 0x5044, {0x80,0x4e,0x8b,0x67,0x99,0xd4,0xcf,0x9e}};

static const GUID IID_IDirect3DDevice_WGC = {
    0xa37624ab, 0x8d5f, 0x4650, {0x9d,0x3e,0x9e,0xae,0x3d,0x9b,0xc6,0x70}};

// ---- Helpers ----

static HRESULT WGC_GetActivationFactory(const wchar_t* className, REFIID iid,
                                         void** factory) {
    HSTRING hstr = nullptr;
    HRESULT hr = WindowsCreateString(className, (UINT32)wcslen(className), &hstr);
    if (FAILED(hr)) return hr;
    hr = RoGetActivationFactory(hstr, iid, factory);
    WindowsDeleteString(hstr);
    return hr;
}

// Convert native ID3D11Device to WinRT IDirect3DDevice.
// CreateDirect3D11DeviceFromDXGIDevice is exported by d3d11.dll
// but not declared in MinGW headers.
typedef HRESULT (WINAPI *PFN_CreateDirect3D11DeviceFromDXGIDevice)(
    IDXGIDevice* dxgiDevice, IInspectable** outDevice);

static HRESULT WGC_WrapD3DDevice(ID3D11Device* d3dDev, IDirect3DDevice** outDev) {
    static PFN_CreateDirect3D11DeviceFromDXGIDevice pFn = nullptr;
    if (!pFn) {
        HMODULE d3d11 = LoadLibraryA("d3d11.dll");
        if (!d3d11) return E_FAIL;
        pFn = (PFN_CreateDirect3D11DeviceFromDXGIDevice)
              GetProcAddress(d3d11, "CreateDirect3D11DeviceFromDXGIDevice");
        if (!pFn) return E_FAIL;
    }

    IDXGIDevice* dxgiDev = nullptr;
    HRESULT hr = d3dDev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDev);
    if (FAILED(hr)) return hr;

    IInspectable* insp = nullptr;
    hr = pFn(dxgiDev, &insp);
    dxgiDev->Release();
    if (FAILED(hr)) return hr;

    hr = insp->QueryInterface(IID_IDirect3DDevice_WGC, (void**)outDev);
    insp->Release();
    return hr;
}

// ---- Public API ----

bool WGC_Init(WGCCapture& wgc) {
    wgc.active = false;

    // 1. Create D3D11 device
    D3D_FEATURE_LEVEL featLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION,
        &wgc.d3dDev, &featLevel, &wgc.d3dCtx);
    if (FAILED(hr)) {
        fprintf(stderr, "[WGC] D3D11CreateDevice failed: 0x%08X\n", (unsigned)hr);
        return false;
    }

    // 2. Initialize WinRT (MTA required for WGC)
    hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        fprintf(stderr, "[WGC] RoInitialize failed: 0x%08X\n", (unsigned)hr);
        WGC_Release(wgc);
        return false;
    }

    // 3. Get IGraphicsCaptureItemInterop factory
    IGraphicsCaptureItemInterop* interop = nullptr;
    hr = WGC_GetActivationFactory(
        L"Windows.Graphics.Capture.GraphicsCaptureItem",
        IID_IGraphicsCaptureItemInterop,
        (void**)&interop);
    if (FAILED(hr)) {
        fprintf(stderr, "[WGC] GraphicsCaptureItem factory failed: 0x%08X\n",
                (unsigned)hr);
        WGC_Release(wgc);
        return false;
    }

    // 4. Create capture item for primary monitor
    IGraphicsCaptureItem* item = nullptr;
    hr = interop->CreateForMonitor(
        MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY),
        __uuidof(IGraphicsCaptureItem),
        (void**)&item);
    interop->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "[WGC] CreateForMonitor failed: 0x%08X\n", (unsigned)hr);
        WGC_Release(wgc);
        return false;
    }
    wgc.captureItem = (IUnknown*)item;

    // 5. Wrap D3D11 device as WinRT IDirect3DDevice
    IDirect3DDevice* rtDevice = nullptr;
    hr = WGC_WrapD3DDevice(wgc.d3dDev, &rtDevice);
    if (FAILED(hr)) {
        fprintf(stderr, "[WGC] Wrap D3D device failed: 0x%08X\n", (unsigned)hr);
        WGC_Release(wgc);
        return false;
    }

    // 6. Get IDirect3D11CaptureFramePoolStatics factory
    IDirect3D11CaptureFramePoolStatics* poolStatics = nullptr;
    hr = WGC_GetActivationFactory(
        L"Windows.Graphics.Capture.Direct3D11CaptureFramePool",
        __uuidof(IDirect3D11CaptureFramePoolStatics),
        (void**)&poolStatics);
    if (FAILED(hr)) {
        fprintf(stderr, "[WGC] FramePool factory failed: 0x%08X\n", (unsigned)hr);
        rtDevice->Release();
        WGC_Release(wgc);
        return false;
    }

    // 7. Query monitor size (使用真实分辨率，避免DPI虚拟化)
    SetProcessDPIAware();
    HMONITOR hMon = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMon, &mi);
    wgc.width  = mi.rcMonitor.right - mi.rcMonitor.left;
    wgc.height = mi.rcMonitor.bottom - mi.rcMonitor.top;

    // 8. Create frame pool (2 buffers for pipelining)
    IDirect3D11CaptureFramePool* pool = nullptr;
    WGD::DirectXPixelFormat pixelFmt = WGD::DirectXPixelFormat_B8G8R8A8UIntNormalized;
    SizeInt32 size = { wgc.width, wgc.height };

    hr = poolStatics->Create(rtDevice, pixelFmt, 3, size, &pool);
    rtDevice->Release();
    poolStatics->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "[WGC] Create frame pool failed: 0x%08X\n", (unsigned)hr);
        WGC_Release(wgc);
        return false;
    }
    wgc.framePool = (IUnknown*)pool;

    // 9. Create capture session
    IGraphicsCaptureSession* sess = nullptr;
    hr = pool->CreateCaptureSession(item, &sess);
    if (FAILED(hr)) {
        fprintf(stderr, "[WGC] CreateCaptureSession failed: 0x%08X\n",
                (unsigned)hr);
        WGC_Release(wgc);
        return false;
    }
    wgc.session = (IUnknown*)sess;

    // 10. Start capture
    hr = sess->StartCapture();

    if (FAILED(hr)) {
        fprintf(stderr, "[WGC] StartCapture failed: 0x%08X\n", (unsigned)hr);
        WGC_Release(wgc);
        return false;
    }

    // 10b. Try to disable capture border (Win11 yellow border)
    {
        IGraphicsCaptureSession3* sess3 = nullptr;
        static const GUID IID_IGCS3 = { 0xf2cdd966, 0x22ae, 0x5ea1, {0x95,0x96, 0x3a,0x28,0x93,0x44,0xc3,0xbe}};
        HRESULT hr3 = sess->QueryInterface(IID_IGCS3, (void**)&sess3);
        if (SUCCEEDED(hr3) && sess3) {
            boolean borderOff = false;
            HRESULT hrBorder = sess3->put_IsBorderRequired(borderOff);
            if (SUCCEEDED(hrBorder))
                fprintf(stderr, "[WGC] IsBorderRequired set to false\n");
            else
                fprintf(stderr, "[WGC] IsBorderRequired(false) failed: 0x%08X\n", (unsigned)hrBorder);
            sess3->Release();
        } else {
            fprintf(stderr, "[WGC] IGraphicsCaptureSession3 not available (pre-Win11?)\n");
        }
    }

    // 10c. Disable cursor capture so WGC texture has no cursor (eliminates
    // double-cursor without hiding the system cursor globally)
    {
        IGraphicsCaptureSession2* sess2 = nullptr;
        HRESULT hr2 = sess->QueryInterface(IID_IGraphicsCaptureSession2_WGC, (void**)&sess2);
        if (SUCCEEDED(hr2) && sess2) {
            boolean cursorOff = false;
            HRESULT hrCursor = sess2->put_IsCursorCaptureEnabled(cursorOff);
            if (SUCCEEDED(hrCursor))
                fprintf(stderr, "[WGC] IsCursorCaptureEnabled set to false\n");
            else
                fprintf(stderr, "[WGC] IsCursorCaptureEnabled(false) failed: 0x%08X\n", (unsigned)hrCursor);
            sess2->Release();
        } else {
            fprintf(stderr, "[WGC] IGraphicsCaptureSession2 not available (cursor capture cannot be disabled)\n");
        }
    }

    // 11. Create staging texture for GPU->CPU readback
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width          = wgc.width;
    stagingDesc.Height         = wgc.height;
    stagingDesc.MipLevels      = 1;
    stagingDesc.ArraySize      = 1;
    stagingDesc.Format         = DXGI_FORMAT_B8G8R8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage          = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.BindFlags      = 0;

    hr = wgc.d3dDev->CreateTexture2D(&stagingDesc, nullptr, &wgc.stagingTex);
    if (FAILED(hr)) {
        fprintf(stderr, "[WGC] Create staging tex failed: 0x%08X\n", (unsigned)hr);
        WGC_Release(wgc);
        return false;
    }

    wgc.active = true;
    fprintf(stderr, "[WGC] Capture ready: %dx%d\n", wgc.width, wgc.height);
    return true;
}

ID3D11Texture2D* WGC_GetFrame(WGCCapture& wgc) {
    if (!wgc.active) return nullptr;

    IDirect3D11CaptureFramePool* pool = (IDirect3D11CaptureFramePool*)wgc.framePool;
    IDirect3D11CaptureFrame* frame = nullptr;

    // TryGetNextFrame: non-blocking
    HRESULT hr = pool->TryGetNextFrame(&frame);
    if (FAILED(hr) || !frame) return nullptr;

    // Get the Direct3D surface from the frame
    IDirect3DSurface* surface = nullptr;
    hr = frame->get_Surface(&surface);
    frame->Release();
    if (FAILED(hr) || !surface) return nullptr;

    // Use IDirect3DDxgiInterfaceAccess to get the underlying D3D11 texture.
    // Direct QI for ID3D11Texture2D on a WinRT surface returns E_NOINTERFACE.
    IDirect3DDxgiInterfaceAccess* dxgiAccess = nullptr;
    hr = surface->QueryInterface(IID_IDirect3DDxgiInterfaceAccess,
                                  (void**)&dxgiAccess);
    if (FAILED(hr) || !dxgiAccess) {
        fprintf(stderr, "[WGC] DxgiInterfaceAccess QI failed: 0x%08X\n",
                (unsigned)hr);
        surface->Release();
        return nullptr;
    }

    ID3D11Texture2D* tex = nullptr;
    hr = dxgiAccess->GetInterface(__uuidof(ID3D11Texture2D), (void**)&tex);
    dxgiAccess->Release();
    surface->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "[WGC] GetInterface(ID3D11Texture2D) failed: 0x%08X\n",
                (unsigned)hr);
        return nullptr;
    }
    return tex;
}

bool WGC_CopyToStaging(WGCCapture& wgc, ID3D11Texture2D* srcTex,
                       D3D11_MAPPED_SUBRESOURCE& mapped) {
    if (!wgc.active || !srcTex || !wgc.stagingTex) return false;

    // Copy frame to staging texture (GPU-only)
    wgc.d3dCtx->CopyResource(wgc.stagingTex, srcTex);

    // D3D11 fence: ensure CopyResource completes before CPU Map.
    // Without this, high-fps GPU pipelining can return partially-written data.
    ID3D11Query* fence = nullptr;
    D3D11_QUERY_DESC qdesc = { D3D11_QUERY_EVENT, 0 };
    if (SUCCEEDED(wgc.d3dDev->CreateQuery(&qdesc, &fence))) {
        wgc.d3dCtx->End(fence);
        while (wgc.d3dCtx->GetData(fence, nullptr, 0, 0) == S_FALSE)
            ;  // spin-wait for GPU
        fence->Release();
    }

    // Map staging for CPU read
    HRESULT hr = wgc.d3dCtx->Map(wgc.stagingTex, 0, D3D11_MAP_READ, 0, &mapped);
    return SUCCEEDED(hr);
}

void WGC_UnmapStaging(WGCCapture& wgc) {
    if (wgc.stagingTex)
        wgc.d3dCtx->Unmap(wgc.stagingTex, 0);
}

void WGC_Release(WGCCapture& wgc) {
    if (wgc.stagingTex) { wgc.stagingTex->Release(); wgc.stagingTex = nullptr; }

    if (wgc.session) {
        ((IGraphicsCaptureSession*)wgc.session)->Release();
        wgc.session = nullptr;
    }
    if (wgc.framePool) {
        ((IDirect3D11CaptureFramePool*)wgc.framePool)->Release();
        wgc.framePool = nullptr;
    }
    if (wgc.captureItem) {
        wgc.captureItem->Release();
        wgc.captureItem = nullptr;
    }
    if (wgc.d3dCtx) { wgc.d3dCtx->Release(); wgc.d3dCtx = nullptr; }
    if (wgc.d3dDev) { wgc.d3dDev->Release(); wgc.d3dDev = nullptr; }
    wgc.active = false;
}
