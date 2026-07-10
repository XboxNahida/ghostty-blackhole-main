$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:PATH"
$ws = "d:\ghostty-blackhole-main-main\ghostty-blackhole-main\ghostty-blackhole-main"

# 编译资源文件（包含图标）- 使用 coff 格式
Write-Host "Compiling resource file..."
& windres --output-format=coff --input-format=rc -o "$ws\build\resource.o" "$ws\resource.rc" 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "windres failed, continuing without icon..."
}

# 编译主程序
Write-Host "Compiling main program..."
$resourceFile = if (Test-Path "$ws\build\resource.o") { "$ws\build\resource.o" } else { "" }

& g++ -std=c++17 -O2 -o "$ws\build\blackhole.exe" `
    "$ws\src\main.cpp" `
    "$ws\src\capture_wgc.cpp" `
    "$ws\src\capture_dxgi.cpp" `
    "$ws\src\gl_texture.cpp" `
    "$ws\src\gui_config.cpp" `
    "$ws\src\win32_gl.cpp" `
    "$ws\src\monitors.cpp" `
    "$ws\src\imgui\imgui.cpp" `
    "$ws\src\imgui\imgui_draw.cpp" `
    "$ws\src\imgui\imgui_widgets.cpp" `
    "$ws\src\imgui\imgui_tables.cpp" `
    "$ws\src\imgui\imgui_impl_glfw.cpp" `
    "$ws\src\imgui\imgui_impl_opengl3.cpp" `
    $resourceFile `
    -I"$ws\src\imgui" `
    -I"C:\msys64\ucrt64\include" `
    -L"C:\msys64\ucrt64\lib" `
    -lglfw3 -lopengl32 -lgdi32 -ld3d11 -ldxgi -lruntimeobject -ldwmapi -lcomctl32 -lole32 -ladvapi32 -lwtsapi32 `
    -mwindows 2>&1

if ($LASTEXITCODE -eq 0) {
    Write-Host "BUILD SUCCESS - blackhole.exe"
    Get-Item "$ws\build\blackhole.exe" | Select-Object Length, LastWriteTime

    # 复制着色器到 build 目录
    if (-not (Test-Path "$ws\build\shaders")) { New-Item -ItemType Directory -Path "$ws\build\shaders" | Out-Null }
    Copy-Item "$ws\shaders\*" "$ws\build\shaders\" -Force
    Copy-Item "$ws\blackhole.glsl" "$ws\build\blackhole.glsl" -Force

    # 复制 MSYS2 UCRT64 运行时 DLL（缺这些会启动报错"找不到 glfw3.dll"等）
    $ucrtBin = "C:\msys64\ucrt64\bin"
    foreach ($dll in @("glfw3.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
        $src = "$ucrtBin\$dll"
        if (Test-Path $src) {
            Copy-Item $src "$ws\build\$dll" -Force
            Write-Host "Copied $dll"
        } else {
            Write-Host "WARN: $dll not found in $ucrtBin"
        }
    }
} else {
    Write-Host "BUILD FAILED (exit $LASTEXITCODE)"
}

# 清理临时文件
Remove-Item "$ws\build\resource.o" -Force -ErrorAction SilentlyContinue
