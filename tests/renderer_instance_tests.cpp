#include "renderer_instance.h"

#include <cstdlib>
#include <string>

namespace {

void Require(bool condition)
{
    if (!condition) std::exit(1);
}

}

int main()
{
    Require(RendererMutexName(-1) == "Local\\BlakholeRendererPrimary");
    Require(RendererMutexName(1) == RendererMutexName(1));
    Require(RendererMutexName(1) != RendererMutexName(2));
    Require(RendererMutexName(0) == "Local\\BlakholeRendererScreen0");
    return 0;
}
