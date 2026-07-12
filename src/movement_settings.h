#pragma once

enum class SpawnPosition : int {
    Random = 0,
    LeftTop = 1,
    RightTop = 2,
    LeftBottom = 3,
    RightBottom = 4
};

struct MovementSpawn {
    float x;
    float y;
    float phaseOffset;
    float presetOffset;
};

int NormalizeSpawnPosition(int value);
float ClampMovementSpeed(float value);
MovementSpawn ResolveMovementSpawn(int position, float randomX, float randomY,
                                  float randomPhase, float randomPresetOffset);
