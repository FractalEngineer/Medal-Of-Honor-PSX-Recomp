param(
    [double]$WorldScale = 3,
    [double]$IPDmm = 67,
    [double]$UnitsPerMeter = (48.0 / 0.067),
    [switch]$Desktop,
    [switch]$HideHud,
    [switch]$Verify,
    [switch]$Build,
    [string]$BuildDirectory = 'build-release',
    [int]$Slot = -1,
    [string]$CaptureDirectory = '',
    [int]$Seconds = 0
)
$ErrorActionPreference = 'Stop'
$vrRoot = Split-Path $PSScriptRoot -Parent
$vrBuild = Join-Path $vrRoot $BuildDirectory
$vrExe = Join-Path $vrBuild 'Medal_of_Honor__Recompiled.exe'
$vrVariables = @{
    PSX_OPENXR = [string][int](-not $Desktop);
    PSX_VR_OPENXR = [string][int](-not $Desktop);
    PSX_VR_STEREO = '1'; PSX_VR_PROBE = '0'; PSX_VR_INTERP = '0';
    PSX_VR_PASS_PROBE = '0'; PSX_VR_PASS_WATCHDOG = '0'; PSX_VR_PASS_DRAW = '1';
    PSX_VR_OFFSET = '0'; PSX_VR_RECT = $null; PSX_VR_RECT_ALT = '0';
    PSX_VR_STEREO_FAULT = '0'; PSX_VR_STEREO_FAULT_HOLD = '0';
    PSX_VR_WORLD_SCALE = $WorldScale.ToString([Globalization.CultureInfo]::InvariantCulture);
    PSX_VR_IPD_MM = $IPDmm.ToString([Globalization.CultureInfo]::InvariantCulture);
    PSX_VR_UNITS_PER_METER = $UnitsPerMeter.ToString([Globalization.CultureInfo]::InvariantCulture);
    PSX_VR_EYE_OFFSET = $null; PSX_VR_HEAD_YAW = '0'; PSX_VR_HEAD_POSITION = '0,0,0';
    PSX_VR_DRAW_MASK = '15'; PSX_VR_DESKTOP_FOV = '0'; PSX_VR_AUTHORED_FOCAL = '1';
    PSX_VR_TEXT = [string][int](-not $HideHud); PSX_VR_HUD_ICON = [string][int](-not $HideHud);
    PSX_RENDER_PASS_VERIFY = [string][int]$Verify.IsPresent
}
$vrOldEnvironment = @{}
$vrProcess = $null
Push-Location $vrRoot
try {
    if ($Build) {
        cmake -S . -B $vrBuild -DPSX_OPENXR=ON "-DPSXRECOMP_ROOT=$vrRoot/psxrecomp"
        if ($LASTEXITCODE -ne 0) { throw 'VR configure failed' }
        cmake --build $vrBuild --target psx-runtime
        if ($LASTEXITCODE -ne 0) { throw 'VR build failed; launch refused' }
    }
    foreach ($vrKey in $vrVariables.Keys) {
        $vrOldEnvironment[$vrKey] = [Environment]::GetEnvironmentVariable($vrKey,'Process')
        [Environment]::SetEnvironmentVariable($vrKey,$vrVariables[$vrKey],'Process')
    }
    $vrProcess = Start-Process -FilePath $vrExe -ArgumentList '--no-launcher','--game','game.toml','--disc','Input/medal-of-honor/medal-of-honor.cue' -WorkingDirectory $vrRoot -WindowStyle Hidden -PassThru
    if ($CaptureDirectory) {
        $vrSaveSlot = if ($Slot -ge 0) {$Slot} else {3}
        python vr/capture_stereo.py $CaptureDirectory --slot $vrSaveSlot --pairs 2 --executable $vrExe
        if ($LASTEXITCODE -ne 0) { throw 'VR capture failed' }
    } elseif ($Slot -ge 0) {
        # Use the capture utility readiness/load protocol without committing assets.
        $vrRunDir = 'analysis/vr-proof/run-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
        python vr/capture_stereo.py $vrRunDir --slot $Slot --pairs 0 --executable $vrExe
        if ($LASTEXITCODE -ne 0) { throw 'VR slot load failed' }
    }
    if ($Seconds -gt 0) {
        $null = $vrProcess.WaitForExit($Seconds * 1000)
    } else {
        $vrProcess.WaitForExit()
    }
} finally {
    if ($vrProcess -and -not $vrProcess.HasExited) {
        Stop-Process -Id $vrProcess.Id -ErrorAction SilentlyContinue
        $vrProcess.WaitForExit()
    }
    foreach ($vrKey in $vrOldEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($vrKey,$vrOldEnvironment[$vrKey],'Process')
    }
    Pop-Location
}
