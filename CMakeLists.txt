cmake_minimum_required(VERSION 3.20)
project(blackhole-standalone LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSYS2 UCRT64 prefix for GLFW
list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")

find_package(glfw3 REQUIRED)

# OpenGL
find_package(OpenGL REQUIRED)

add_executable(blackhole
    src/main.cpp
    src/capture_wgc.cpp
    src/capture_dxgi.cpp
    src/gui_config.cpp
    src/imgui/imgui.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_widgets.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/imgui_impl_glfw.cpp
    src/imgui/imgui_impl_opengl3.cpp
    src/gl_texture.cpp
    src/win32_gl.cpp
    src/win32_window.cpp
    src/d3d11_renderer.cpp
    resource.rc
)

# 取消注释以编译 D3D11 路径
# target_compile_definitions(blackhole PRIVATE BLACKHOLE_USE_D3D11)  # 已回退到 OpenGL

target_include_directories(blackhole PRIVATE
    ${OPENGL_INCLUDE_DIRS}
)

target_link_libraries(blackhole PRIVATE
    glfw
    ${OPENGL_LIBRARIES}
    d3d11
    dxgi
    d3dcompiler
    gdi32
    runtimeobject
    dwmapi
    comctl32
)

# Compile with MSYS2 GCC (ucrt64)
if(MINGW)
    target_compile_options(blackhole PRIVATE -fexec-charset=UTF-8)
    target_link_options(blackhole PRIVATE -mwindows)
    # Fix windres popen bug on Windows
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --use-temp-file")
    # 自动拷贝 MSYS2 UCRT64 运行时 DLL 到输出目录
    set(UCRT64_BIN "C:/msys64/ucrt64/bin")
    add_custom_command(TARGET blackhole POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${UCRT64_BIN}/libwinpthread-1.dll"
            "$<TARGET_FILE_DIR:blackhole>/libwinpthread-1.dll"
        COMMENT "Copying libwinpthread-1.dll to output directory"
    )
endif()

