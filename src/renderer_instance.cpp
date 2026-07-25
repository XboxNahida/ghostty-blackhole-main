#include "renderer_instance.h"

std::string RendererMutexName(int screenIdx)
{
    if (screenIdx < 0) {
        return "Local\\BlakholeRendererPrimary";
    }
    return "Local\\BlakholeRendererScreen" + std::to_string(screenIdx);
}
