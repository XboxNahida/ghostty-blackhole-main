#include "renderer_instance.h"

#include <string>

int main() {
    if (RendererInstanceMutexName(-1) != "Local\\BlakholeRendererDefault") return 1;
    if (RendererInstanceMutexName(1) != "Local\\BlakholeRendererScreen1") return 2;
    if (RendererInstanceMutexName(2) != "Local\\BlakholeRendererScreen2") return 3;
    if (IsValidRendererChildScreenIndex(0, 2)) return 4;
    if (!IsValidRendererChildScreenIndex(1, 2)) return 5;
    if (IsValidRendererChildScreenIndex(2, 2)) return 6;
    if (IsValidRendererChildScreenIndex(1, 1)) return 7;

    const int testScreenIndex =
        1000000 + static_cast<int>(GetCurrentProcessId() & 0x0FFFFFFF);
    const int otherTestScreenIndex = testScreenIndex + 1;

    RendererInstanceLock testRenderer(testScreenIndex);
    if (!testRenderer.acquired()) return 8;

    RendererInstanceLock duplicateTestRenderer(testScreenIndex);
    if (duplicateTestRenderer.acquired()) return 9;

    RendererInstanceLock otherTestRenderer(otherTestScreenIndex);
    if (!otherTestRenderer.acquired()) return 10;

    return 0;
}
