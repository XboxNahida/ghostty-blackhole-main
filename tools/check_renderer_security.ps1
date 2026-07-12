$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $root "src\main.cpp"
$guiConfigPath = Join-Path $root "src\gui_config.cpp"
$win32GlPath = Join-Path $root "src\win32_gl.cpp"

function Fail-RendererSecurityCheck {
    param([Parameter(Mandatory = $true)][string]$Message)
    throw "RENDERER_SECURITY_CHECK_FAILED: $Message"
}

function Read-Source {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        Fail-RendererSecurityCheck "missing source file $Path"
    }
    return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path
}

function Require-Text {
    param(
        [Parameter(Mandatory = $true)][string]$Content,
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Content.Contains($Text)) {
        Fail-RendererSecurityCheck $Message
    }
}

function Reject-Pattern {
    param(
        [Parameter(Mandatory = $true)][string]$Content,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if ($Content -match $Pattern) {
        Fail-RendererSecurityCheck $Message
    }
}

function Require-Pattern {
    param(
        [Parameter(Mandatory = $true)][string]$Content,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if ($Content -notmatch $Pattern) {
        Fail-RendererSecurityCheck $Message
    }
}

$main = Read-Source $mainPath
$guiConfig = Read-Source $guiConfigPath
$win32Gl = Read-Source $win32GlPath
$rendererSources = $main + "`n" + $guiConfig + "`n" + $win32Gl

$forbiddenPatterns = @(
    @{ Pattern = '\bRegSetValueExA\s*\('; Message = "renderer still writes the Registry Run key" },
    @{ Pattern = '\bRegDeleteValueA\s*\('; Message = "renderer still deletes the Registry Run value" },
    @{ Pattern = '\bOpenProcess\s*\(\s*PROCESS_TERMINATE'; Message = "renderer still opens arbitrary processes for termination" },
    @{ Pattern = '\bTerminateProcess\s*\('; Message = "renderer still terminates processes directly" },
    @{ Pattern = '\bGetAsyncKeyState\s*\('; Message = "renderer still polls global keyboard state" },
    @{ Pattern = 'stricmp\s*\([^\r\n]*blackhole\.exe'; Message = "renderer still enumerates blackhole.exe by process name at startup" }
)

foreach ($item in $forbiddenPatterns) {
    Reject-Pattern -Content $rendererSources -Pattern $item.Pattern -Message $item.Message
}

Reject-Pattern -Content $guiConfig -Pattern 'ImGui::Checkbox\s*\(\s*"开机自启"' `
    -Message "renderer config still exposes the legacy autostart checkbox"
Require-Text -Content $guiConfig -Text '(int)cfg.autoStart' `
    -Message "renderer config no longer writes the compatible autoStart field"
Require-Text -Content $guiConfig -Text 'cfg.autoStart = (nf >= 6)' `
    -Message "renderer config no longer parses the compatible autoStart field"

Require-Text -Content $main -Text 'if (!isRenderer)' `
    -Message "renderer modes are not separated before creating the control mutex"
Require-Text -Content $main -Text 'CreateMutexA(' `
    -Message "non-render control modes do not create a named mutex"
Require-Text -Content $main -Text 'ERROR_ALREADY_EXISTS' `
    -Message "control mutex does not reject an existing control instance"
Require-Text -Content $main -Text 'JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE' `
    -Message "owned renderer children are not protected by a kill-on-close job"
Require-Text -Content $main -Text 'PROC_THREAD_ATTRIBUTE_JOB_LIST' `
    -Message "renderer children are not assigned to their job at process creation"
Require-Text -Content $main -Text 'CreateProcessInJob(cmd, g_hMonitorJob, g_pi)' `
    -Message "monitor-created renderer is not atomically assigned to its owned job"
Require-Text -Content $main -Text 'CreateProcessInJob(cmd, g_hChildJob, pi)' `
    -Message "multi-display renderer is not atomically assigned to its owned job"
Reject-Pattern -Content $main -Pattern '\bAssignProcessToJobObject\s*\(' `
    -Message "renderer still assigns jobs after process creation without a safe failure path"
Reject-Pattern -Content $main -Pattern '\bResumeThread\s*\(' `
    -Message "renderer still resumes a child after a separate job-assignment step"
Require-Text -Content $main -Text 'PostMessage(hwnd, WM_CLOSE, 0, 0)' `
    -Message "owned renderer shutdown does not request WM_CLOSE"
Require-Text -Content $main -Text 'CloseHandle(g_hMonitorJob)' `
    -Message "monitor child job is not closed during cleanup"
Require-Text -Content $main -Text 'CloseHandle(g_hChildJob)' `
    -Message "multi-display child job is not closed during cleanup"
Require-Text -Content $main -Text '\"%s\" --render --screen %d' `
    -Message "multi-display renderer spawn arguments are no longer supported"
Require-Text -Content $main -Text 'strcmp(argv[2], "--screen") == 0' `
    -Message "multi-display renderer no longer parses the --screen argument"
Require-Pattern -Content $main -Pattern '#endif\s+(?://[^\r\n]*\s+)*KillChildRenderers\(\);\s+return 0;' `
    -Message "multi-display child cleanup is not shared by both renderer backends"
Require-Text -Content $main -Text 'RegisterHotKey(' `
    -Message "primary renderer does not register Ctrl+Alt+R with the system hotkey API"
Require-Text -Content $rendererSources -Text 'WM_HOTKEY' `
    -Message "renderer does not handle WM_HOTKEY messages"
Require-Text -Content $main -Text 'UnregisterHotKey(' `
    -Message "renderer does not unregister its recording capture hotkey"

Write-Output "RENDERER_SECURITY_OK"
