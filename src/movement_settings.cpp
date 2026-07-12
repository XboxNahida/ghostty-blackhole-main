#include "movement_settings.h"

int NormalizeSpawnPosition(int value) {
    if (value < static_cast<int>(SpawnPosition::Random) ||
        value > static_cast<int>(SpawnPosition::RightBottom)) {
        return static_cast<int>(SpawnPosition::Random);
    }
    return value;
}

float ClampMovementSpeed(float value) {
    if (value < 0.1f) {
        return 0.1f;
    }
    if (value > 3.0f) {
        return 3.0f;
    }
    return value;
}

MovementSpawn ResolveMovementSpawn(int position, float randomX, float randomY,
                                  float randomPhase, float randomPresetOffset) {
    switch (static_cast<SpawnPosition>(NormalizeSpawnPosition(position))) {
    case SpawnPosition::LeftTop:
        return {0.18f, 0.18f, 0.0f, 0.0f};
    case SpawnPosition::RightTop:
        return {0.82f, 0.18f, 0.0f, 0.0f};
    case SpawnPosition::LeftBottom:
        return {0.18f, 0.82f, 0.0f, 0.0f};
    case SpawnPosition::RightBottom:
        return {0.82f, 0.82f, 0.0f, 0.0f};
    case SpawnPosition::Random:
        return {randomX, randomY, randomPhase, randomPresetOffset};
    }

    return {randomX, randomY, randomPhase, randomPresetOffset};
}
