#include "media_session.h"

#include <cassert>
#include <chrono>
#include <iostream>

int main()
{
    assert(MediaSession_SourceMatchesProcess("bilibiliuwp_abc", "bilibili.exe"));
    assert(MediaSession_SourceMatchesProcess("bilibiliuwp_abc", "哔哩哔哩.exe"));
    assert(MediaSession_SourceMatchesProcess("qqlive.package", "腾讯视频.exe"));
    assert(MediaSession_SourceMatchesProcess("iqiyi.video", "爱奇艺.exe"));
    assert(MediaSession_SourceMatchesProcess("chrome.exe", "chrome.exe"));
    assert(MediaSession_SourceMatchesProcess("Microsoft.ZuneVideo_8wekyb3d8bbwe", "zunevideo.exe"));
    assert(!MediaSession_SourceMatchesProcess("spotify.exe", "bilibili.exe"));
    assert(!MediaSession_SourceMatchesProcess("", "bilibili.exe"));
    assert(!MediaSession_SourceMatchesProcess("bilibili.exe", ""));

    const auto started = std::chrono::steady_clock::now();
    const MediaSessionSnapshot snapshot = MediaSession_QueryCurrent(500);
    const MediaSessionSnapshot repeatedSnapshot = MediaSession_QueryCurrent(500);
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    assert(elapsedMs < 1500);

    std::cout << "MEDIA_SESSION_TESTS_OK state=" << static_cast<int>(snapshot.state)
              << " repeatedState=" << static_cast<int>(repeatedSnapshot.state)
              << " source=" << snapshot.sourceAppId
              << " hr=0x" << std::hex << static_cast<unsigned long>(snapshot.error)
              << " elapsedMs=" << std::dec << elapsedMs << "\n";
    return 0;
}
