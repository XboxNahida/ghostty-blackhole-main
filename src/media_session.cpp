#include "media_session.h"

#include <asyncinfo.h>
#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>

#include <algorithm>
#include <cstddef>
#include <cctype>

namespace {

struct MediaSessionManager;
struct MediaSession;
struct MediaPlaybackInfo;
struct MediaSessionManagerAsyncOperation;
struct MediaSessionManagerStatics;

constexpr wchar_t kManagerClassName[] =
    L"Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager";

const GUID kManagerStaticsIid = {
    0x2050c4ee, 0x11a0, 0x57de, {0xae, 0xd7, 0xc9, 0x7c, 0x70, 0x33, 0x82, 0x45}
};
const GUID kAsyncInfoIid = {
    0x00000036, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
};

template<typename Function>
Function AbiFunction(void *object, size_t slot)
{
    return reinterpret_cast<Function>((*reinterpret_cast<void ***>(object))[slot]);
}

HRESULT AbiQueryInterface(void *object, REFIID iid, void **result)
{
    using Function = HRESULT (STDMETHODCALLTYPE *)(void *, REFIID, void **);
    return AbiFunction<Function>(object, 0)(object, iid, result);
}

void AbiRelease(void *object)
{
    if (!object) return;
    using Function = ULONG (STDMETHODCALLTYPE *)(void *);
    AbiFunction<Function>(object, 2)(object);
}

void AbiAddRef(void *object)
{
    using Function = ULONG (STDMETHODCALLTYPE *)(void *);
    AbiFunction<Function>(object, 1)(object);
}

HRESULT AbiRequestManager(MediaSessionManagerStatics *statics,
                          MediaSessionManagerAsyncOperation **operation)
{
    using Function = HRESULT (STDMETHODCALLTYPE *)(void *, MediaSessionManagerAsyncOperation **);
    return AbiFunction<Function>(statics, 6)(statics, operation);
}

HRESULT AbiGetManager(MediaSessionManagerAsyncOperation *operation,
                      MediaSessionManager **manager)
{
    using Function = HRESULT (STDMETHODCALLTYPE *)(void *, MediaSessionManager **);
    return AbiFunction<Function>(operation, 8)(operation, manager);
}

HRESULT AbiGetCurrentSession(MediaSessionManager *manager, MediaSession **session)
{
    using Function = HRESULT (STDMETHODCALLTYPE *)(void *, MediaSession **);
    return AbiFunction<Function>(manager, 6)(manager, session);
}

HRESULT AbiGetSourceAppId(MediaSession *session, HSTRING *value)
{
    using Function = HRESULT (STDMETHODCALLTYPE *)(void *, HSTRING *);
    return AbiFunction<Function>(session, 6)(session, value);
}

HRESULT AbiGetPlaybackInfo(MediaSession *session, MediaPlaybackInfo **playbackInfo)
{
    using Function = HRESULT (STDMETHODCALLTYPE *)(void *, MediaPlaybackInfo **);
    return AbiFunction<Function>(session, 9)(session, playbackInfo);
}

HRESULT AbiGetPlaybackStatus(MediaPlaybackInfo *playbackInfo, int *status)
{
    using Function = HRESULT (STDMETHODCALLTYPE *)(void *, int *);
    return AbiFunction<Function>(playbackInfo, 7)(playbackInfo, status);
}

struct ManagerCache {
    bool attempted = false;
    bool ownsInitialization = false;
    HRESULT error = S_OK;
    MediaSessionManager *manager = nullptr;

    ~ManagerCache()
    {
        AbiRelease(manager);
        if (ownsInitialization) RoUninitialize();
    }
};

ManagerCache &Cache()
{
    static ManagerCache cache;
    return cache;
}

HRESULT AcquireManager(unsigned timeoutMs, MediaSessionManager **manager)
{
    if (!manager) {
        return E_POINTER;
    }
    *manager = nullptr;

    ManagerCache &cache = Cache();
    if (cache.manager) {
        AbiAddRef(cache.manager);
        *manager = cache.manager;
        return S_OK;
    }
    if (cache.attempted) return cache.error;
    cache.attempted = true;

    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        cache.error = hr;
        return hr;
    }
    cache.ownsInitialization = SUCCEEDED(hr);

    HSTRING className = nullptr;
    hr = WindowsCreateString(kManagerClassName,
                             static_cast<UINT32>(std::wcslen(kManagerClassName)),
                             &className);
    if (FAILED(hr)) {
        cache.error = hr;
        return hr;
    }

    MediaSessionManagerStatics *statics = nullptr;
    hr = RoGetActivationFactory(className, kManagerStaticsIid,
                                reinterpret_cast<void **>(&statics));
    WindowsDeleteString(className);
    if (FAILED(hr)) {
        cache.error = hr;
        return hr;
    }

    MediaSessionManagerAsyncOperation *operation = nullptr;
    hr = AbiRequestManager(statics, &operation);
    AbiRelease(statics);
    if (FAILED(hr) || !operation) {
        cache.error = FAILED(hr) ? hr : E_FAIL;
        return cache.error;
    }

    IAsyncInfo *asyncInfo = nullptr;
    hr = AbiQueryInterface(operation, kAsyncInfoIid, reinterpret_cast<void **>(&asyncInfo));
    if (FAILED(hr) || !asyncInfo) {
        AbiRelease(operation);
        cache.error = FAILED(hr) ? hr : E_NOINTERFACE;
        return cache.error;
    }

    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    AsyncStatus status = Started;
    while (GetTickCount64() <= deadline) {
        hr = asyncInfo->get_Status(&status);
        if (FAILED(hr) || status != Started) {
            break;
        }
        Sleep(5);
    }

    if (SUCCEEDED(hr) && status == Completed) {
        hr = AbiGetManager(operation, &cache.manager);
    } else if (SUCCEEDED(hr) && status == Error) {
        hr = asyncInfo->get_ErrorCode(&hr) == S_OK ? hr : E_FAIL;
    } else if (SUCCEEDED(hr)) {
        asyncInfo->Cancel();
        hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    }

    asyncInfo->Release();
    AbiRelease(operation);
    if (FAILED(hr) || !cache.manager) {
        cache.error = FAILED(hr) ? hr : E_FAIL;
        return cache.error;
    }
    AbiAddRef(cache.manager);
    *manager = cache.manager;
    cache.error = S_OK;
    return S_OK;
}

std::string HStringToUtf8(HSTRING value)
{
    UINT32 length = 0;
    const wchar_t *wide = WindowsGetStringRawBuffer(value, &length);
    if (!wide || length == 0) {
        return {};
    }

    const int byteCount = WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(length),
                                               nullptr, 0, nullptr, nullptr);
    if (byteCount <= 0) {
        return {};
    }
    std::string utf8(static_cast<size_t>(byteCount), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(length), utf8.data(), byteCount,
                        nullptr, nullptr);
    return utf8;
}

std::string Normalized(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return c < 0x80 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
    });
    const size_t slash = value.find_last_of("/\\");
    if (slash != std::string::npos) {
        value.erase(0, slash + 1);
    }
    if (value.size() > 4 && value.compare(value.size() - 4, 4, ".exe") == 0) {
        value.resize(value.size() - 4);
    }
    return value;
}

std::string CanonicalMediaToken(const std::string &value)
{
    if (value.find("bilibili") != std::string::npos || value.find("哔哩") != std::string::npos) {
        return "bilibili";
    }
    if (value.find("iqiyi") != std::string::npos || value.find("爱奇艺") != std::string::npos) {
        return "iqiyi";
    }
    if (value.find("youku") != std::string::npos || value.find("优酷") != std::string::npos) {
        return "youku";
    }
    if (value.find("qqlive") != std::string::npos || value.find("腾讯视频") != std::string::npos) {
        return "qqlive";
    }
    if (value.find("mgtv") != std::string::npos || value.find("芒果") != std::string::npos) {
        return "mgtv";
    }
    if (value.find("douyin") != std::string::npos || value.find("抖音") != std::string::npos) {
        return "douyin";
    }
    if (value.find("kuaishou") != std::string::npos || value.find("快手") != std::string::npos) {
        return "kuaishou";
    }
    return {};
}

MediaPlaybackState ConvertPlaybackState(int value)
{
    switch (value) {
    case 0: return MediaPlaybackState::Closed;
    case 1: return MediaPlaybackState::Opened;
    case 2: return MediaPlaybackState::Changing;
    case 3: return MediaPlaybackState::Stopped;
    case 4: return MediaPlaybackState::Playing;
    case 5: return MediaPlaybackState::Paused;
    default: return MediaPlaybackState::Unavailable;
    }
}

} // namespace

MediaSessionSnapshot MediaSession_QueryCurrent(unsigned timeoutMs)
{
    MediaSessionSnapshot snapshot;
    MediaSessionManager *manager = nullptr;
    HRESULT hr = AcquireManager(timeoutMs, &manager);
    if (FAILED(hr) || !manager) {
        snapshot.error = FAILED(hr) ? hr : E_FAIL;
        return snapshot;
    }

    MediaSession *session = nullptr;
    hr = AbiGetCurrentSession(manager, &session);
    AbiRelease(manager);
    if (FAILED(hr)) {
        snapshot.error = hr;
        return snapshot;
    }
    if (!session) {
        snapshot.state = MediaPlaybackState::None;
        return snapshot;
    }

    HSTRING source = nullptr;
    hr = AbiGetSourceAppId(session, &source);
    if (SUCCEEDED(hr) && source) {
        snapshot.sourceAppId = HStringToUtf8(source);
        WindowsDeleteString(source);
    }

    MediaPlaybackInfo *playbackInfo = nullptr;
    const HRESULT playbackHr = AbiGetPlaybackInfo(session, &playbackInfo);
    AbiRelease(session);
    if (FAILED(playbackHr) || !playbackInfo) {
        snapshot.error = FAILED(playbackHr) ? playbackHr : E_FAIL;
        return snapshot;
    }

    int playbackStatus = -1;
    hr = AbiGetPlaybackStatus(playbackInfo, &playbackStatus);
    AbiRelease(playbackInfo);
    if (FAILED(hr)) {
        snapshot.error = hr;
        return snapshot;
    }

    snapshot.state = ConvertPlaybackState(playbackStatus);
    snapshot.error = S_OK;
    return snapshot;
}

bool MediaSession_SourceMatchesProcess(const std::string &sourceAppId,
                                       const std::string &processName)
{
    const std::string source = Normalized(sourceAppId);
    const std::string process = Normalized(processName);
    if (source.empty() || process.size() < 3) {
        return false;
    }
    const std::string sourceToken = CanonicalMediaToken(source);
    const std::string processToken = CanonicalMediaToken(process);
    if (!sourceToken.empty() && sourceToken == processToken) {
        return true;
    }
    return source == process || source.find(process) != std::string::npos ||
           process.find(source) != std::string::npos;
}
