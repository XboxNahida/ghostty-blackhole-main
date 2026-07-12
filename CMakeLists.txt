cmake_minimum_required(VERSION 3.20)
include(cmake/AppVersion.cmake)
project(blackhole-standalone VERSION ${BLACKHOLE_VERSION} LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSYS2 UCRT64 prefix for GLFW
list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")

find_package(glfw3 REQUIRED)

# OpenGL
find_package(OpenGL REQUIRED)

set(BLACKHOLE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${BLACKHOLE_GENERATED_DIR}")
configure_file(cmake/app_version.h.in "${BLACKHOLE_GENERATED_DIR}/app_version.h" @ONLY)
configure_file(cmake/renderer_version.rc.in "${BLACKHOLE_GENERATED_DIR}/renderer_version.rc" @ONLY)

add_executable(blackhole
    src/main.cpp
    src/foreground_window.cpp
    src/game_detection.cpp
    src/media_session.cpp
    src/movement_settings.cpp
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
    src/bloom_renderer.cpp
    src/win32_gl.cpp
    src/monitors.cpp
    src/win32_window.cpp
    src/d3d11_renderer.cpp
    resource.rc
    "${BLACKHOLE_GENERATED_DIR}/renderer_version.rc"
)

# 取消注释以编译 D3D11 路径
# target_compile_definitions(blackhole PRIVATE BLACKHOLE_USE_D3D11)  # 已回退到 OpenGL

target_include_directories(blackhole PRIVATE
    ${OPENGL_INCLUDE_DIRS}
    "${BLACKHOLE_GENERATED_DIR}"
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
    ole32
    advapi32
    wtsapi32
)

# Compile with MSYS2 GCC (ucrt64)
if(MINGW)
    target_compile_options(blackhole PRIVATE -fexec-charset=UTF-8)
    target_link_options(blackhole PRIVATE -mwindows)
    # Fix windres popen bug on Windows
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --use-temp-file --codepage=65001")
    # 自动拷贝 MSYS2 UCRT64 运行时 DLL 到输出目录
    # 缺这些会启动报错"找不到 glfw3.dll"等
    set(UCRT64_BIN "C:/msys64/ucrt64/bin")
    foreach(ucrt_dll
            glfw3.dll
            libgcc_s_seh-1.dll
            libstdc++-6.dll
            libwinpthread-1.dll)
        add_custom_command(TARGET blackhole POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${UCRT64_BIN}/${ucrt_dll}"
                "$<TARGET_FILE_DIR:blackhole>/${ucrt_dll}"
            COMMENT "Copying ${ucrt_dll} to output directory"
        )
    endforeach()
endif()

