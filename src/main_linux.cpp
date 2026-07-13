#include <cstdio>
#include <cstdlib>
#include <GLFW/glfw3.h>

int main()
{
    if (!glfwInit()) {
        fprintf(stderr, "FAILED: glfwInit\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    fprintf(stdout, "blackhole-renderer placeholder\n");

    glfwTerminate();
    return EXIT_SUCCESS;
}
