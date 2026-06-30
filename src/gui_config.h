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
    int   mode       = 1;
    int   idleSec    = 300;
    float holeRadius = -1.0f;
    float diskGain   = -1.0f;
    float diskTemp   = -1.0f;
    float exposure   = -1.0f;
    float spd        = -1.0f;
    float starGain   = -1.0f;
    float diskIncl   = -1.0f;

    int   presetCount = 0;
    DiskPreset presets[64];
    int   playMode        = 1;    // 0=顺序 1=循环 2=随机
    float slotSec        = 5.25f; // 每个预设播放秒数
    bool  videoAsIdle     = false; // 播放视频时视为空闲
    bool  autoStart       = false; // 开机自启
    bool  confirmed  = false;
};

void InitDefaultPresets(BlackholeConfig& cfg);
bool GUI_ShowConfigPanel(BlackholeConfig& cfg);
void SavePresetsToFile(const BlackholeConfig& cfg, const char names[16][64]);
bool LoadPresetsFromFile(BlackholeConfig& cfg, char names[16][64]);