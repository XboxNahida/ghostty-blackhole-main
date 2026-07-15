cmake_minimum_required(VERSION 3.20)
include(cmake/AppVersion.cmake)

# RC language only on Windows (no RC compiler on Linux)
if(WIN32)
    project(blackhole-standalone VERSION ${BLACKHOLE_VERSION} LANGUAGES CXX RC)
else()
    project(blackhole-standalone VERSION ${BLACKHOLE_VERSION} LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Options ----
option(BLACKHOLE_BUILD_UI "Build Qt/QML configuration UI" ON)
option(BLACKHOLE_BUILD_RENDERER "Build the blackhole-renderer (glfw+OpenGL)" ON)
option(BLACKHOLE_ENABLE_PORTAL_CAPTURE "Enable XDG Desktop Portal screen capture (Linux)" ON)
include(CTest)
include(GNUInstallDirs)
set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

# ---- Generated version header (platform neutral) ----
set(BLACKHOLE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${BLACKHOLE_GENERATED_DIR}")
configure_file(cmake/app_version.h.in "${BLACKHOLE_GENERATED_DIR}/app_version.h" @ONLY)

# ══════════════════════════════════════════════════════════════
# Windows renderer (blackhole)
# ══════════════════════════════════════════════════════════════
if(WIN32 AND BLACKHOLE_BUILD_RENDERER)
    # MSYS2 UCRT64 prefix for GLFW
    list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")

    find_package(glfw3 REQUIRED)
    find_package(OpenGL REQUIRED)

    configure_file(cmake/renderer_version.rc.in "${BLACKHOLE_GENERATED_DIR}/renderer_version.rc" @ONLY)

    add_executable(blackhole
        src/main.cpp
        src/render/gl_funcs_win32.cpp
        src/render/shader_source.cpp
        src/render/gl_program.cpp
        src/render/blackhole_renderer.cpp
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
        src
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
endif()

# ══════════════════════════════════════════════════════════════
# Linux renderer (blackhole-renderer)
# ══════════════════════════════════════════════════════════════
if(NOT WIN32 AND BLACKHOLE_BUILD_RENDERER)
    find_package(glfw3 REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(Qt6 REQUIRED COMPONENTS Core DBus)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

    # Generate xdg-foreign-unstable-v2 client code
    set(XDG_FOREIGN_XML "/usr/share/wayland-protocols/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml")
    set(XDG_FOREIGN_GEN "${CMAKE_CURRENT_BINARY_DIR}/xdg-foreign-gen")
    file(MAKE_DIRECTORY "${XDG_FOREIGN_GEN}")
    add_custom_command(
        OUTPUT "${XDG_FOREIGN_GEN}/xdg-foreign-unstable-v2.h"
               "${XDG_FOREIGN_GEN}/xdg-foreign-unstable-v2.c"
        COMMAND wayland-scanner client-header "${XDG_FOREIGN_XML}"
                "${XDG_FOREIGN_GEN}/xdg-foreign-unstable-v2.h"
        COMMAND wayland-scanner private-code "${XDG_FOREIGN_XML}"
                "${XDG_FOREIGN_GEN}/xdg-foreign-unstable-v2.c"
        DEPENDS "${XDG_FOREIGN_XML}"
        COMMENT "Generating xdg-foreign-unstable-v2 code"
    )

    add_executable(blackhole-renderer
        src/main_linux.cpp
        src/render/gl_funcs_linux.cpp
        src/render/shader_source.cpp
        src/render/gl_program.cpp
        src/preset_config_linux.cpp
        src/portal_capture.cpp
        src/portal_capture.h
        src/wayland_surface_export.cpp
        src/wayland_surface_export.h
        "${XDG_FOREIGN_GEN}/xdg-foreign-unstable-v2.h"
        "${XDG_FOREIGN_GEN}/xdg-foreign-unstable-v2.c"
    )

    target_include_directories(blackhole-renderer PRIVATE
        src
        "${BLACKHOLE_GENERATED_DIR}"
        "${XDG_FOREIGN_GEN}"
    )

    target_link_libraries(blackhole-renderer PRIVATE
        glfw
        OpenGL::GL
        Qt6::Core
        Qt6::DBus
        PkgConfig::PIPEWIRE
        PkgConfig::WAYLAND_CLIENT
    )
    set_target_properties(blackhole-renderer PROPERTIES AUTOMOC ON)

    # Copy shader files alongside the binary for runtime loading
    add_custom_command(TARGET blackhole-renderer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:blackhole-renderer>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/shaders/vert.glsl"
            "$<TARGET_FILE_DIR:blackhole-renderer>/shaders/vert.glsl"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/shaders/frag_desktop_header.glsl"
            "$<TARGET_FILE_DIR:blackhole-renderer>/shaders/frag_desktop_header.glsl"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/blackhole.glsl"
            "$<TARGET_FILE_DIR:blackhole-renderer>/blackhole.glsl"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/blackhole_presets.txt"
            "$<TARGET_FILE_DIR:blackhole-renderer>/blackhole_presets.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/blackhole_advanced.txt"
            "$<TARGET_FILE_DIR:blackhole-renderer>/blackhole_advanced.txt"
        COMMENT "Copying runtime resources for blackhole-renderer"
    )

    # Pass the install data dir as a compile definition so the UI can find
    # installed resources at runtime (dev tree falls back to projectRoot).
    target_compile_definitions(blackhole-renderer PRIVATE
        BLACKHOLE_INSTALL_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/blackhole"
    )
endif()

# Linux packages install only the active GNOME compositor product path.  The
# standalone renderer remains buildable for historical tests, but is not a
# runtime component of the Ubuntu-local variant.
if(NOT WIN32)
    install(FILES
        "${CMAKE_SOURCE_DIR}/shaders/vert.glsl"
        "${CMAKE_SOURCE_DIR}/shaders/frag_preview_header.glsl"
        "${CMAKE_SOURCE_DIR}/shaders/blackhole_preview.glsl"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/blackhole/shaders
    )

    # GNOME extensions load local schemas from a compiled database inside the
    # extension directory. Stage the extension in the build tree so generated
    # gschemas.compiled is packaged without polluting the source tree.
    find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas REQUIRED)
    set(BLACKHOLE_EXTENSION_SOURCE
        "${CMAKE_SOURCE_DIR}/gnome-extension/blackhole@xboxnahida.github.com")
    set(BLACKHOLE_EXTENSION_BUILD
        "${CMAKE_CURRENT_BINARY_DIR}/gnome-extension/blackhole@xboxnahida.github.com")
    file(GLOB_RECURSE BLACKHOLE_EXTENSION_FILES CONFIGURE_DEPENDS
        "${BLACKHOLE_EXTENSION_SOURCE}/*")
    add_custom_command(
        OUTPUT "${BLACKHOLE_EXTENSION_BUILD}/schemas/gschemas.compiled"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${BLACKHOLE_EXTENSION_BUILD}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${BLACKHOLE_EXTENSION_SOURCE}" "${BLACKHOLE_EXTENSION_BUILD}"
        COMMAND "${GLIB_COMPILE_SCHEMAS}" "${BLACKHOLE_EXTENSION_BUILD}/schemas"
        DEPENDS ${BLACKHOLE_EXTENSION_FILES}
        COMMENT "Staging GNOME extension and compiling its GSettings schema"
        VERBATIM
    )
    add_custom_target(blackhole-gnome-extension ALL
        DEPENDS "${BLACKHOLE_EXTENSION_BUILD}/schemas/gschemas.compiled")
    install(DIRECTORY
        "${BLACKHOLE_EXTENSION_BUILD}/"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/gnome-shell/extensions/blackhole@xboxnahida.github.com
    )
endif()

# ══════════════════════════════════════════════════════════════
# Qt/QML UI (cross-platform)
# ══════════════════════════════════════════════════════════════
if(BLACKHOLE_BUILD_UI)
    add_subdirectory(Blakhole_UI)
endif()

# ══════════════════════════════════════════════════════════════
# Tests
# ══════════════════════════════════════════════════════════════
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_test(NAME gnome_uniform_usage_test
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/gnome_uniform_usage_test.py")

    add_executable(shader_source_tests EXCLUDE_FROM_ALL
        tests/shader_source_tests.cpp
        src/render/shader_source.cpp
    )
    target_include_directories(shader_source_tests PRIVATE src)
    add_test(NAME shader_source_tests COMMAND shader_source_tests)
    set_tests_properties(shader_source_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    # Compile-guard: verify all render core source files compile on Linux.
    # Uses -fsyntax-only so no GL/Bloom/Win32 symbols need to be linked.
    set(_RENDER_FLAGS -std=c++17 -fsyntax-only -I"${CMAKE_SOURCE_DIR}/src")
    add_custom_target(render_core_syntax_check
        COMMAND ${CMAKE_CXX_COMPILER} ${_RENDER_FLAGS}
            "${CMAKE_SOURCE_DIR}/tests/render_core_compile_test.cpp"
        COMMAND ${CMAKE_CXX_COMPILER} ${_RENDER_FLAGS}
            "${CMAKE_SOURCE_DIR}/src/render/gl_funcs_win32.cpp"
        COMMAND ${CMAKE_CXX_COMPILER} ${_RENDER_FLAGS}
            "${CMAKE_SOURCE_DIR}/src/render/shader_source.cpp"
        COMMAND ${CMAKE_CXX_COMPILER} ${_RENDER_FLAGS}
            "${CMAKE_SOURCE_DIR}/src/render/gl_program.cpp"
        COMMAND ${CMAKE_CXX_COMPILER} ${_RENDER_FLAGS}
            "${CMAKE_SOURCE_DIR}/src/render/blackhole_renderer.cpp"
        COMMENT "Syntax-checking render core on Linux"
    )
    add_test(NAME render_core_syntax_check
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}"
            --target render_core_syntax_check
    )

    if(NOT WIN32 AND BLACKHOLE_BUILD_RENDERER)
        find_package(Qt6 REQUIRED COMPONENTS Core DBus Test)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
        add_executable(portal_capture_tests EXCLUDE_FROM_ALL
            tests/portal_capture_tests.cpp
            src/portal_capture.cpp src/portal_capture.h
            src/wayland_surface_export.cpp src/wayland_surface_export.h
            "${XDG_FOREIGN_GEN}/xdg-foreign-unstable-v2.c")
        set_target_properties(portal_capture_tests PROPERTIES AUTOMOC ON)
        target_include_directories(portal_capture_tests PRIVATE src "${XDG_FOREIGN_GEN}")
        target_link_libraries(portal_capture_tests PRIVATE Qt6::Core Qt6::DBus Qt6::Test PkgConfig::PIPEWIRE PkgConfig::WAYLAND_CLIENT)
        add_test(NAME portal_capture_tests COMMAND portal_capture_tests)
    endif()
endif()

# ══════════════════════════════════════════════════════════════
# Packaging (CPack DEB)
# ══════════════════════════════════════════════════════════════
include(GNUInstallDirs)
set(CPACK_PACKAGE_NAME "blackhole")
set(CPACK_PACKAGE_VERSION "${BLACKHOLE_DEBIAN_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Ubuntu-local black hole compositor effect for GNOME Wayland")
set(CPACK_PACKAGE_VENDOR "XboxNahida")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/XboxNahida/ghostty-blackhole-main")
set(CPACK_PACKAGE_CONTACT "XboxNahida")
set(CPACK_PACKAGE_FILE_NAME "blackhole-${BLACKHOLE_DISPLAY_VERSION}-linux-amd64")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

set(CPACK_DEBIAN_PACKAGE_NAME "blackhole")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "gnome-shell (>= 50)")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/XboxNahida/ghostty-blackhole-main")
set(CPACK_GENERATOR "DEB")
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};blackhole-standalone;ALL;/")

include(CPack)
