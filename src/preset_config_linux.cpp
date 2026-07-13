#include "gui_config.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static const DiskPreset DEFAULT_PRESETS[16] = {
    {5500, 1.50f, 0.35f, 1.8f, 8.0f, 0.90f, 0.60f, 2.5f, 2.2f, 1.6f, 7.0f, 5.0f, 1.40f, 0.0f},
    {4500, 1.52f, 0.10f, 2.2f, 7.0f, 0.85f, 0.35f, 2.0f, 1.4f, 0.5f, 7.0f, 5.0f, 1.20f, 0.0f},
    {3800, 0.55f,-0.30f, 2.2f, 6.0f, 0.45f, 0.90f, 3.5f, 1.6f, 0.4f, 3.0f, 2.5f, 1.10f, 0.0f},
    {6500, 0.30f, 0.00f, 3.0f,10.0f, 0.50f, 0.80f, 2.5f, 1.0f, 1.1f, 7.0f, 5.0f, 1.00f, 0.0f},
    {15000,1.30f, 0.35f, 3.0f,14.0f, 0.35f, 1.00f, 4.0f, 1.2f, 1.3f, 8.0f, 5.0f, 0.80f, 0.0f},
    {18000,1.05f, 0.55f, 3.0f,16.0f, 0.30f, 1.00f, 5.0f, 1.0f, 1.5f, 9.0f, 6.0f, 0.75f, 0.0f},
    {5500, 1.50f, 0.35f, 1.8f, 8.0f, 0.00f, 1.00f, 2.5f, 0.0f, 1.6f, 7.0f, 5.0f, 1.00f, 0.6f},
    {5500, 1.50f, 0.35f, 1.8f, 8.0f, 0.90f, 0.60f, 2.5f, 2.2f, 1.6f, 7.0f, 5.0f, 1.40f, 0.0f},
    {3200, 1.45f, 0.60f, 2.0f, 9.0f, 0.95f, 0.20f, 1.5f, 3.0f, 2.0f, 5.0f, 4.0f, 1.60f, 0.0f},
    {8000, 1.20f,-0.50f, 2.5f, 7.0f, 0.70f, 0.75f, 2.8f, 1.8f, 1.4f, 8.0f, 5.5f, 1.30f, 0.0f},
    {2500, 1.55f, 0.20f, 1.6f, 6.0f, 0.60f, 0.10f, 1.2f, 2.6f, 1.8f, 4.0f, 3.0f, 1.50f, 0.0f},
    {12000,0.80f, 0.45f, 2.8f,12.0f, 0.40f, 0.95f, 3.5f, 1.5f, 1.2f, 8.5f, 5.0f, 0.90f, 0.0f},
    {5000, 0.10f, 0.00f, 2.0f, 9.0f, 0.85f, 0.50f, 2.0f, 1.3f, 1.5f, 6.0f, 4.5f, 1.10f, 0.0f},
    {22000,1.40f, 0.70f, 3.5f,18.0f, 0.25f, 1.00f, 4.5f, 0.9f, 1.7f,10.0f, 6.5f, 0.70f, 0.0f},
    {4200, 1.48f, 0.15f, 1.9f, 7.5f, 0.80f, 0.45f, 2.2f, 2.0f, 0.8f, 6.5f, 4.8f, 1.25f, 0.0f},
    {9000, 0.45f,-0.15f, 2.6f,11.0f, 0.55f, 0.85f, 3.0f, 1.1f, 1.3f, 7.5f, 5.2f, 1.05f, 0.0f},
};

void InitDefaultPresets(BlackholeConfig& cfg) {
    cfg.presetCount = 16;
    for (int i = 0; i < cfg.presetCount; ++i) cfg.presets[i] = DEFAULT_PRESETS[i];
}

bool LoadPresetsFromFile(BlackholeConfig& cfg, char names[64][64], const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return false;
    char line[256];
    if (!fgets(line, sizeof(line), f) || !strstr(line, "v4") ||
        !fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }
    int videoAsIdle = 0, autoStart = 0, fixedSize = 0;
    int fields = sscanf(line, "%d %d %f %d %d %d %d %f %d %d",
        &cfg.mode, &cfg.idleSec, &cfg.slotSec, &cfg.playMode,
        &videoAsIdle, &autoStart, &fixedSize, &cfg.fixedLevel,
        &cfg.captureMode, &cfg.displayMode);
    if (fields < 4 || !fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }
    cfg.videoAsIdle = videoAsIdle != 0;
    cfg.autoStart = autoStart != 0;
    cfg.fixedSize = fixedSize != 0;
    const int count = atoi(line);
    if (count < 1 || count > 64) {
        fclose(f);
        return false;
    }
    for (int i = 0; i < count; ++i) {
        if (!fgets(line, sizeof(line), f)) { fclose(f); return false; }
        line[strcspn(line, "\r\n")] = 0;
        strncpy(names[i], line, 63);
        names[i][63] = 0;
        if (!fgets(line, sizeof(line), f)) { fclose(f); return false; }
        DiskPreset& p = cfg.presets[i];
        if (sscanf(line, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f",
            &p.temp, &p.incl, &p.roll, &p.inner, &p.outer, &p.opac, &p.dopp,
            &p.beam, &p.gain, &p.contr, &p.wind, &p.speed, &p.expo, &p.star) != 14) {
            fclose(f);
            return false;
        }
    }
    cfg.presetCount = count;
    fclose(f);
    return true;
}
