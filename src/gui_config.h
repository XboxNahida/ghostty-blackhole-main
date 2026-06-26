// gui_config.h  ImGui config panel with preset editing
#pragma once

struct DiskPreset {
    float temp  = 5500.0f;
    float incl  = 1.50f;
    float roll  = 0.35f;
    float inner = 1.8f;
    float outer = 8.0f;
    float opac  = 0.90f;
    float dopp  = 0.60f;
    float beam  = 2.5f;
    float gain  = 2.2f;
    float contr = 1.6f;
    float wind  = 7.0f;
    float speed = 5.0f;
    float expo  = 1.40f;
    float star  = 0.0f;
};

struct BlackholeConfig {
    int   mode       = 0;
    int   idleSec    = 300;
    float holeRadius = -1.0f;
    float diskGain   = -1.0f;
    float diskTemp   = -1.0f;
    float exposure   = -1.0f;
    float spd        = -1.0f;
    float starGain   = -1.0f;
    float diskIncl   = -1.0f;

    int   presetCount = 0;
    DiskPreset presets[16];
    bool  useCustomPresets = false;
    bool  confirmed  = false;
};

void InitDefaultPresets(BlackholeConfig& cfg);
bool GUI_ShowConfigPanel(BlackholeConfig& cfg);