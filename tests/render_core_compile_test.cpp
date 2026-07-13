// Compile-time verification that the shared render core headers and
// implementations are valid C++ on Linux.
//
// This test provides the minimal stubs needed by gl_funcs_win32.cpp
// so that the real render core compiles (not just its "no-op" stub under
// BLACKHOLE_USE_D3D11).  The resulting binary cannot run without a GL
// context, but that is intentional — this test only verifies that the
// extracted render code is syntactically and structurally sound before
// a proper GLFW/EGL context is available.

// Provide the external symbol that gl_funcs_win32.cpp needs.
// On Windows this is declared in win32_gl.h; here we provide a stub.
extern "C" void* Win32GL_GetProcAddress(const char* name) {
    (void)name;
    return nullptr;
}

// Include the full render core — no BLACKHOLE_USE_D3D11 guard.
#include "render/gl_funcs.h"
#include "render/shader_source.h"
#include "render/gl_program.h"
#include "render/blackhole_renderer.h"

// Instantiation smoke-test.
static void testRendererAPI() {
    BlackholeRenderer r;
    (void)r;
}

int main() {
    testRendererAPI();
    return 0;
}
