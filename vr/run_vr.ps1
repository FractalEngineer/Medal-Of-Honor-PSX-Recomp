param(
    [double]$WorldScale = 3,
    [double]$IPDmm = 67,
    [double]$UnitsPerMeter = (48.0 / 0.067),
    [switch]$Desktop,
    [switch]$MovementDiagnostic,
    [switch]$NoMovement,
    [switch]$WeaponAimDiagnostic,
    [switch]$WeaponPoseDiagnostic,
    [ValidateRange(1,65536)][double]$WeaponModelUnitsPerMeter = 850,
    [ValidateRange(1,32)][double]$WeaponProjectionScale = 16,
    [ValidateCount(3,3)][ValidateRange(-32766,32766)][double[]]$WeaponPivot = @(80,150,100),
    [switch]$DesktopFov,
    [ValidateRange(.25,20)][double]$MenuDistance = 2,
    [ValidateRange(.25,10)][double]$MenuWidth = 2,
    [switch]$NoMenuSurfaceDiagnostic,
    [ValidateRange(0,3)][int]$StereoFaultDiagnostic = 0,
    [double]$MoveDeadzone = 0.2,
    [double]$TurnGain = 0.65,
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
    PSX_VR_MOVEMENT = [string][int]((-not $NoMovement) -and ((-not $Desktop) -or $MovementDiagnostic));
    PSX_VR_WEAPON_AIM = [string][int]($WeaponAimDiagnostic -or $WeaponPoseDiagnostic);
    PSX_VR_WEAPON_POSE = [string][int]$WeaponPoseDiagnostic.IsPresent;
    PSX_VR_WEAPON_MODEL_UNITS_PER_METER = $WeaponModelUnitsPerMeter.ToString([Globalization.CultureInfo]::InvariantCulture);
    PSX_VR_WEAPON_PROJECTION_SCALE = $WeaponProjectionScale.ToString([Globalization.CultureInfo]::InvariantCulture);
    PSX_VR_MENU_SURFACE = [string][int](-not $NoMenuSurfaceDiagnostic);
    PSX_VR_MENU_DISTANCE = $MenuDistance.ToString([Globalization.CultureInfo]::InvariantCulture);
    PSX_VR_MENU_WIDTH = $MenuWidth.ToString([Globalization.CultureInfo]::InvariantCulture);
    PSX_VR_WEAPON_PIVOT = ($WeaponPivot | ForEach-Object { $_.ToString([Globalization.CultureInfo]::InvariantCulture) }) -join ',';
    PSX_VR_MOVE_DEADZONE = $MoveDeadzone.ToString([Globalization.CultureInfo]::InvariantCulture);
    PSX_VR_TURN_GAIN = $TurnGain.ToString([Globalization.CultureInfo]::InvariantCulture);
    PSX_VR_PASS_PROBE = '0'; PSX_VR_PASS_WATCHDOG = '0'; PSX_VR_PASS_DRAW = '1';
    PSX_VR_OFFSET = '0'; PSX_VR_RECT = $null; PSX_VR_RECT_ALT = '0';
    PSX_VR_STEREO_FAULT = [string]$StereoFaultDiagnostic; PSX_VR_STEREO_FAULT_HOLD = '0';
    PSX_VR_WORLD_SCALE = $WorldScale.ToString([Globalization.CultureInfo]::InvariantCulture);
    PSX_VR_IPD_MM = $IPDmm.ToString([Globalization.CultureInfo]::InvariantCulture);
    PSX_VR_UNITS_PER_METER = $UnitsPerMeter.ToString([Globalization.CultureInfo]::InvariantCulture);
    PSX_VR_EYE_OFFSET = $null; PSX_VR_HEAD_YAW = '0'; PSX_VR_HEAD_POSITION = '0,0,0';
    PSX_VR_DRAW_MASK = '15'; PSX_VR_DESKTOP_FOV = [string][int]$DesktopFov.IsPresent; PSX_VR_AUTHORED_FOCAL = '1';
    PSX_VR_TEXT = [string][int](-not $HideHud); PSX_VR_HUD_ICON = [string][int](-not $HideHud);
    PSX_RENDER_PASS_VERIFY = [string][int]$Verify.IsPresent
}
$vrOldEnvironment = @{}
$vrProcess = $null
Push-Location $vrRoot
try {
    # A capture must not accidentally inspect an older game on the same TCP port.
    $vrSocket = New-Object System.Net.Sockets.TcpClient
    try {
        $vrSocket.Connect('127.0.0.1',4370)
        throw 'A game/debug server already uses port 4370. Close it before launching this VR test.'
    } catch [System.Net.Sockets.SocketException] {
        # Refused connection is expected before this owned process starts.
    } finally { $vrSocket.Dispose() }
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
