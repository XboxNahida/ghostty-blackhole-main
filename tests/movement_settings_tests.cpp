#include "movement_settings.h"

#include <cmath>
#include <cstdlib>

static void RequireNear(float actual, float expected) {
    if (std::fabs(actual - expected) > 0.0001f) {
        std::abort();
    }
}

int main() {
    if (NormalizeSpawnPosition(0) != static_cast<int>(SpawnPosition::Random)) return 1;
    if (NormalizeSpawnPosition(1) != static_cast<int>(SpawnPosition::LeftTop)) return 2;
    if (NormalizeSpawnPosition(2) != static_cast<int>(SpawnPosition::RightTop)) return 3;
    if (NormalizeSpawnPosition(3) != static_cast<int>(SpawnPosition::LeftBottom)) return 4;
    if (NormalizeSpawnPosition(4) != static_cast<int>(SpawnPosition::RightBottom)) return 5;
    if (NormalizeSpawnPosition(-1) != 0) return 1;
    if (NormalizeSpawnPosition(5) != 0) return 2;
    RequireNear(ClampMovementSpeed(0.0f), 0.1f);
    RequireNear(ClampMovementSpeed(1.0f), 1.0f);
    RequireNear(ClampMovementSpeed(4.0f), 3.0f);

    MovementSpawn leftTop = ResolveMovementSpawn(1, 0.4f, 0.6f, 1.2f, 9.0f);
    RequireNear(leftTop.x, 0.18f);
    RequireNear(leftTop.y, 0.18f);
    MovementSpawn rightTop = ResolveMovementSpawn(2, 0.4f, 0.6f, 1.2f, 9.0f);
    RequireNear(rightTop.x, 0.82f);
    RequireNear(rightTop.y, 0.18f);
    MovementSpawn leftBottom = ResolveMovementSpawn(3, 0.4f, 0.6f, 1.2f, 9.0f);
    RequireNear(leftBottom.x, 0.18f);
    RequireNear(leftBottom.y, 0.82f);
    MovementSpawn rightBottom = ResolveMovementSpawn(4, 0.4f, 0.6f, 1.2f, 9.0f);
    RequireNear(rightBottom.x, 0.82f);
    RequireNear(rightBottom.y, 0.82f);
    MovementSpawn random = ResolveMovementSpawn(0, 0.4f, 0.6f, 1.2f, 9.0f);
    RequireNear(random.x, 0.4f);
    RequireNear(random.y, 0.6f);
    RequireNear(random.phaseOffset, 1.2f);
    RequireNear(random.presetOffset, 9.0f);
    return 0;
}
