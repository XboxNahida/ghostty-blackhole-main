cmake_minimum_required(VERSION 3.16)

include(../cmake/AppVersion.cmake)
project(BlakholeUI VERSION ${BLACKHOLE_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Widgets OpenGL Network)

include(CTest)

qt_standard_project_setup(REQUIRES 6.8)

set(BLACKHOLE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${BLACKHOLE_GENERATED_DIR}")
configure_file(../cmake/app_version.h.in "${BLACKHOLE_GENERATED_DIR}/app_version.h" @ONLY)
configure_file(app_version.rc.in "${BLACKHOLE_GENERATED_DIR}/app_version.rc" @ONLY)

# Fix windres popen issue on Windows (MinGW)
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --use-temp-file --codepage=65001")

qt_add_executable(appBlakholeUI
    main.cpp
    ../src/autostart_registry.cpp
    ../src/foreground_window.cpp
    ../src/game_detection.cpp
    ../src/media_session.cpp
    ../src/movement_settings.cpp
    core/application_catalog.cpp
    core/avatar_storage.cpp
    core/update_release.cpp
    core/update_checker.cpp
    core/systemtray.h
    core/systemtray.cpp
    core/blackholecore.h
    core/blackholecore.cpp
    core/blackholepreviewfbo.h
    core/blackholepreviewfbo.cpp
    app.rc
    "${BLACKHOLE_GENERATED_DIR}/app_version.rc"
)

target_include_directories(appBlakholeUI PRIVATE ../src "${BLACKHOLE_GENERATED_DIR}")

file(GLOB QML_COMPONENTS CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "components/*.qml")
file(GLOB QML_PAGES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "pages/*.qml")

qt_add_qml_module(appBlakholeUI
    URI BlakholeUI
    VERSION 1.0
    QML_FILES
        Main.qml
        ${QML_COMPONENTS}
        ${QML_PAGES}
)

qt_add_resources(APP_RESOURCES
    src.qrc
)

target_sources(appBlakholeUI PRIVATE ${APP_RESOURCES})

set_target_properties(appBlakholeUI PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appBlakholeUI
    PRIVATE Qt6::Quick Qt6::Widgets Qt6::OpenGL Qt6::Network advapi32 dwmapi ole32 runtimeobject version
)

if(BUILD_TESTING)
    add_executable(movement_settings_tests EXCLUDE_FROM_ALL
        ../tests/movement_settings_tests.cpp
        ../src/movement_settings.cpp
    )
    target_include_directories(movement_settings_tests PRIVATE ../src)
    add_test(NAME movement_settings_tests COMMAND movement_settings_tests)

    add_executable(application_catalog_tests EXCLUDE_FROM_ALL
        ../tests/application_catalog_tests.cpp
        core/application_catalog.cpp
    )
    target_include_directories(application_catalog_tests PRIVATE core)
    target_link_libraries(application_catalog_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets version)
    add_test(NAME application_catalog_tests COMMAND application_catalog_tests)

    add_executable(avatar_storage_tests EXCLUDE_FROM_ALL
        ../tests/avatar_storage_tests.cpp
        core/avatar_storage.cpp
    )
    target_include_directories(avatar_storage_tests PRIVATE core)
    target_link_libraries(avatar_storage_tests PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME avatar_storage_tests COMMAND avatar_storage_tests)

    add_executable(update_release_tests EXCLUDE_FROM_ALL
        ../tests/update_release_tests.cpp
        core/update_release.cpp
    )
    target_include_directories(update_release_tests PRIVATE core)
    target_link_libraries(update_release_tests PRIVATE Qt6::Core)
    add_test(NAME update_release_tests COMMAND update_release_tests)

    add_executable(update_checker_state_tests EXCLUDE_FROM_ALL
        ../tests/update_checker_state_tests.cpp
        core/update_checker.cpp
        core/update_release.cpp
    )
    target_include_directories(update_checker_state_tests PRIVATE core)
    target_link_libraries(update_checker_state_tests PRIVATE Qt6::Core Qt6::Network Qt6::Gui)
    add_test(NAME update_checker_state_tests COMMAND update_checker_state_tests)
endif()

# MSYS2 UCRT64 运行时 + Qt 依赖打包 (与根 CMakeLists.txt 同款)
# 缺这些会启动报错 "找不到 libgcc_s_seh-1.dll" / "找不到 libfreetype-6.dll" / "找不到 Qt6Core.dll"
if(MINGW)
    set(UCRT64_BIN "C:/msys64/ucrt64/bin")
    get_filename_component(QT_RUNTIME_BIN "${Qt6_DIR}/../../../bin" ABSOLUTE)
    find_program(WINDEPLOYQT6_EXECUTABLE
        NAMES windeployqt6 windeployqt6.exe
        HINTS "${QT_RUNTIME_BIN}" "${UCRT64_BIN}"
    )

    # 1) GCC 运行时 + Qt6 第三方依赖 DLL — 必须手动拷贝
    #    windeployqt6 只复制 Qt6*.dll 和插件,不会递归拷贝 Qt6Core/Qt6Gui 的 3rd-party deps
    #    (ICU, PCRE2, freetype, harfbuzz, md4c, png, zlib, b2, double-conversion, brotli, zstd 等)
    #    递归分析来源: objdump -p Qt6Core.dll / Qt6Gui.dll / Qt6Quick.dll ...
    foreach(ucrt_dll
            # GCC 运行时
            libgcc_s_seh-1.dll
            libstdc++-6.dll
            libwinpthread-1.dll
            # Qt6Core 3rd-party deps
            libb2-1.dll
            libdouble-conversion.dll
            libicuin75.dll
            libicuuc75.dll
            libicudt75.dll
            libpcre2-16-0.dll
            zlib1.dll
            libzstd.dll
            # Qt6Gui 3rd-party deps (字体/图像)
            libfreetype-6.dll
            libharfbuzz-0.dll
            libmd4c.dll
            libpng16-16.dll
            # Qt6Qml/Qt6Network 3rd-party deps (压缩/网络)
            libbrotlidec.dll
            libbrotlicommon.dll
            libbz2-1.dll
            # harfbuzz -> graphite2 -> glib 等传递依赖
            libgraphite2.dll
            libglib-2.0-0.dll
            libintl-8.dll
            libiconv-2.dll
            libpcre2-8-0.dll)
        if(EXISTS "${UCRT64_BIN}/${ucrt_dll}")
            add_custom_command(TARGET appBlakholeUI POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${UCRT64_BIN}/${ucrt_dll}"
                    "$<TARGET_FILE_DIR:appBlakholeUI>/${ucrt_dll}"
                COMMENT "Copying ${ucrt_dll} to output directory"
            )
        endif()
    endforeach()

    # 2) Qt 运行时 + 插件 + QML 模块 — 用 windeployqt6 自动分析依赖
    #    --qmldir 指向源目录以发现 QML import; --no-translations 避免冗余文件
    #    MSYS2 把 qmlimportscanner.exe 放在 share/qt6/bin/ (windeployqt6 需要),必须加到 PATH
    if(WINDEPLOYQT6_EXECUTABLE)
        add_custom_command(TARGET appBlakholeUI POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E env
                    "PATH=${QT_RUNTIME_BIN}\;C:/msys64/ucrt64/share/qt6/bin\;C:/msys64/ucrt64/bin\;$ENV{PATH}"
                    "${WINDEPLOYQT6_EXECUTABLE}"
                    --release --no-translations
                    --qmldir "${CMAKE_CURRENT_SOURCE_DIR}"
                    "$<TARGET_FILE:appBlakholeUI>"
            COMMENT "Running windeployqt6 to bundle Qt runtime"
        )
    endif()

    # 3) 黑洞着色器 — FBO 预览在运行时加载,必须拷到 exe 同级目录
    #    blackholepreviewfbo.cpp resolveShaderPath() 搜索 <exe_dir>/shaders/vert.glsl,
    #    <exe_dir>/shaders/frag_preview_header.glsl, <exe_dir>/blackhole_preview.glsl.
    #    源文件位于 Blakhole_UI/../shaders/ 和 Blakhole_UI/../blackhole.glsl
    #    若 initShaders() 失败,预览回退到 glClearColor(1,0,0,1) 红屏错误指示.
    set(BH_SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shaders")
    set(BH_SHADER_DST_DIR "$<TARGET_FILE_DIR:appBlakholeUI>/shaders")
    add_custom_command(TARGET appBlakholeUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BH_SHADER_DST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BH_SHADER_SRC_DIR}/vert.glsl"
                "${BH_SHADER_DST_DIR}/vert.glsl"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BH_SHADER_SRC_DIR}/frag_preview_header.glsl"
                "${BH_SHADER_DST_DIR}/frag_preview_header.glsl"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BH_SHADER_SRC_DIR}/blackhole_preview.glsl"
                "$<TARGET_FILE_DIR:appBlakholeUI>/blackhole_preview.glsl"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/../blackhole.glsl"
                "$<TARGET_FILE_DIR:appBlakholeUI>/blackhole.glsl"
        COMMENT "Copying black hole shaders to output directory"
    )
endif()

include(GNUInstallDirs)
install(TARGETS appBlakholeUI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
