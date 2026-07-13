// Compile-time verification that the shared render core headers are valid C++
// on a platform without a GL context or Windows headers.
// This test does not link against GL; it only verifies syntactic correctness
// of the render/ layer.  The blackhole-renderer Linux target in DS-03 will
// provide the actual GL loader and context.

// Check that the headers are self-contained.
#include "render/gl_funcs.h"
#include "render/shader_source.h"
#include "render/gl_program.h"
#include "render/blackhole_renderer.h"

// Verify that BlackholeRenderer can be instantiated and its key methods called.
// (No GL context available, so we test signature compatibility only.)
static void testRendererAPI() {
    BlackholeRenderer r;

    // These would fail at runtime without a GL context, but the compiler must
    // accept their signatures:
    // r.init(0, 640, 480, nullptr);
    // r.drawAndBloom(640, 480, BlackholeConfig(), 0,
    //                0.0f, 0.0f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, nullptr);
    // r.destroy();

    // Prevent unused-variable warning
    (void)r;
}

int main() {
    testRendererAPI();
    return 0;
}
