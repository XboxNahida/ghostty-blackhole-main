#pragma once

#include <string>
#include <windows.h>

enum class MediaPlaybackState {
    Unavailable,
    None,
    Closed,
    Opened,
    Changing,
    Stopped,
    Playing,
    Paused
};

struct MediaSessionSnapshot {
    MediaPlaybackState state = MediaPlaybackState::Unavailable;
    std::string sourceAppId;
    HRESULT error = S_OK;
};

MediaSessionSnapshot MediaSession_QueryCurrent(unsigned timeoutMs = 500);

bool MediaSession_SourceMatchesProcess(const std::string &sourceAppId,
                                       const std::string &processName);
