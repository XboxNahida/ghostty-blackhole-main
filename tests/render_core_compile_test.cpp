// Compile-time syntax check for the shared render core.
// The CMakeLists.txt render_core_syntax_check target compiles each source
// file with -fsyntax-only, so no GL/Bloom/Win32 symbols are linked.

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
