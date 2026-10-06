param(
    [double]$WorldScale = 3,
    [double]$IPDmm = 67,
    [double]$UnitsPerMeter = (48.0 / 0.067),
    [switch]$Desktop,
    [switch]$DesktopVSyncDiagnostic,
    [switch]$MovementDiagnostic,
    [switch]$NoMovement,
    [switch]$NoHeadFrustumDiagnostic,
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
    # Fresh checkouts/release packages have no settings.toml, so without this the
    # runtime renders at the default (low) internal resolution. PSX_INTERNAL_RESOLUTION
    # overrides every config layer for one run (native/720p/1080p/1440p/4k/5k/8k/display).
    [ValidateSet('native', '720p', '1080p', '1440p', '4k', '5k', '8k', 'display')]
    [string]$InternalResolution = '1080p',
    # Force one installed OpenXR runtime for this launch. The pinned loader reads
    # a process-scoped XR_RUNTIME_JSON override, so the active system runtime and
    # the registry are left untouched. The runtime must expose OpenGL for this
    # build (XR_KHR_opengl_enable); 'current' uses whatever is already active.
    [ValidateSet('current', 'vdxr', 'oculus', 'steamvr')]
    [string]$Runtime = 'current',
    [string]$RuntimeJson = '',
    [switch]$Build,
    [string]$BuildDirectory = 'build-release',
    [string]$DiscPath = '',
    [int]$Slot = -1,
    [string]$CaptureDirectory = '',
    [int]$Seconds = 0
)
$ErrorActionPreference = 'Stop'
$vrRoot = Split-Path $PSScriptRoot -Parent
$vrBuild = Join-Path $vrRoot $BuildDirectory
$vrExe = Join-Path $vrBuild 'Medal_of_Honor__Recompiled.exe'
if (-not $Build -and (Test-Path -LiteralPath (Join-Path $vrRoot 'Medal_of_Honor__Recompiled.exe'))) {
    $vrExe = Join-Path $vrRoot 'Medal_of_Honor__Recompiled.exe'
}

function Resolve-VRRuntimeJson {
    param([string]$Name, [string]$ExplicitPath)
    if ($ExplicitPath) {
        if (-not (Test-Path -LiteralPath $ExplicitPath)) {
            throw "OpenXR runtime manifest not found: $ExplicitPath"
        }
        return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }
    if (-not $Name -or $Name -eq 'current') { return $null }
    $vrPf = $env:ProgramFiles
    $vrPf86 = ${env:ProgramFiles(x86)}
    $vrLocal = $env:LOCALAPPDATA
    $vrCandidates = @()
    if ($Name -eq 'vdxr') {
        if ($vrPf) { $vrCandidates += (Join-Path $vrPf 'Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr.json') }
        if ($vrPf86) { $vrCandidates += (Join-Path $vrPf86 'Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr.json') }
        if ($vrLocal) { $vrCandidates += (Join-Path $vrLocal 'Programs\VirtualDesktopStreamer\OpenXR\virtualdesktop-openxr.json') }
    } elseif ($Name -eq 'oculus') {
        # Quest Link ships under "Meta Horizon" now; older installs used "Oculus".
        if ($vrPf) { $vrCandidates += (Join-Path $vrPf 'Meta Horizon\Support\oculus-runtime\oculus_openxr_64.json') }
        if ($vrPf) { $vrCandidates += (Join-Path $vrPf 'Oculus\Support\oculus-runtime\oculus_openxr_64.json') }
    } elseif ($Name -eq 'steamvr') {
        if ($vrPf86) { $vrCandidates += (Join-Path $vrPf86 'Steam\steamapps\common\SteamVR\steamxr_win64.json') }
        if ($vrPf) { $vrCandidates += (Join-Path $vrPf 'Steam\steamapps\common\SteamVR\steamxr_win64.json') }
    }
    foreach ($vrCandidate in $vrCandidates) {
        if (Test-Path -LiteralPath $vrCandidate) { return (Resolve-Path -LiteralPath $vrCandidate).Path }
    }
    throw ("No $Name OpenXR runtime manifest found. Install it or pass -RuntimeJson <path>. Tried: " + ($vrCandidates -join '; '))
}
$vrRuntimeJson = Resolve-VRRuntimeJson -Name $Runtime -ExplicitPath $RuntimeJson
if ($vrRuntimeJson) { Write-Host "OpenXR runtime: $Runtime -> $vrRuntimeJson" }

function Get-VRStartupStatus {
    $client = New-Object System.Net.Sockets.TcpClient
    try {
        if (-not $client.ConnectAsync('127.0.0.1', 4370).Wait(500)) {
            throw 'VR status connection timed out.'
        }
        $client.ReceiveTimeout = 1000
        $client.SendTimeout = 1000
        $stream = $client.GetStream()
        $request = [Text.Encoding]::UTF8.GetBytes('{"cmd":"openxr_stats"}' + "`n")
        $stream.Write($request, 0, $request.Length)
        $reader = New-Object System.IO.StreamReader($stream)
        return ($reader.ReadLine() | ConvertFrom-Json)
    } finally { $client.Dispose() }
}
$vrVariables = @{
    PSX_OPENXR = [string][int](-not $Desktop);
    # XR waits for its compositor; avoid an additional desktop VSync wait.
    # The runtime retains its deadline-based guest speed cap.
    PSX_VSYNC = $(if ($Desktop) { [Environment]::GetEnvironmentVariable('PSX_VSYNC','Process') } else { [string][int]$DesktopVSyncDiagnostic.IsPresent });
    PSX_VR_OPENXR = [string][int](-not $Desktop);
    PSX_VR_STEREO = '1'; PSX_VR_PROBE = '0'; PSX_VR_INTERP = '0';
    PSX_INTERNAL_RESOLUTION = $InternalResolution;
    # Process-scoped runtime override; restored with the rest in the finally block.
    # Native-only (no XR_RUNTIME_JSON) leaves any inherited value untouched.
    XR_RUNTIME_JSON = $(if ($vrRuntimeJson) { $vrRuntimeJson } else { [Environment]::GetEnvironmentVariable('XR_RUNTIME_JSON','Process') });
    # Accepted eye-view visibility for normal VR; keep desktop diagnostics native.
    PSX_VR_HEAD_FRUSTUM = [string][int]((-not $Desktop) -and (-not $NoHeadFrustumDiagnostic));
    PSX_VR_MOVEMENT = [string][int]((-not $NoMovement) -and ((-not $Desktop) -or $MovementDiagnostic));
    PSX_VR_WEAPON_AIM = [string][int]($WeaponAimDiagnostic -or $WeaponPoseDiagnostic);
    PSX_VR_WEAPON_POSE = [string][int]$WeaponPoseDiagnostic.IsPresent;
    # Barrel laser: raycast the level tree so the beam ends on the first surface.
    PSX_VR_LASER_RAY = '1'; PSX_VR_LASER_DOT = '2';
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
    if (-not (Test-Path -LiteralPath $vrExe)) { throw "Game executable missing: $vrExe" }
    # Explicit selection wins; otherwise reuse the runtime's remembered disc,
    # then the existing local development layout. The release needs no ROM copy.
    if (-not $DiscPath) {
        $vrDiscCache = Join-Path (Split-Path $vrExe -Parent) 'disc.cfg'
        if (Test-Path -LiteralPath $vrDiscCache) {
            $vrCachedDisc = (Get-Content -LiteralPath $vrDiscCache -Raw).Trim()
            if ($vrCachedDisc -and (Test-Path -LiteralPath $vrCachedDisc)) { $DiscPath = $vrCachedDisc }
        }
    }
    if (-not $DiscPath -and (Test-Path -LiteralPath 'Input/medal-of-honor/medal-of-honor.cue')) {
        $DiscPath = (Resolve-Path -LiteralPath 'Input/medal-of-honor/medal-of-honor.cue').Path
    }
    if (-not $DiscPath) {
        Add-Type -AssemblyName System.Windows.Forms
        $vrPicker = New-Object System.Windows.Forms.OpenFileDialog
        try {
            $vrPicker.Title = 'Select your Medal of Honor SLUS-00974 disc image'
            $vrPicker.Filter = 'PlayStation CUE image (*.cue)|*.cue'
            if ($vrPicker.ShowDialog() -ne [Windows.Forms.DialogResult]::OK) { throw 'Disc selection cancelled.' }
            $DiscPath = $vrPicker.FileName
        } finally { $vrPicker.Dispose() }
    }
    $DiscPath = (Resolve-Path -LiteralPath $DiscPath).Path
    if ([IO.Path]::GetExtension($DiscPath) -ine '.cue') { throw 'Select the supported SLUS-00974 CUE image.' }
    $vrDiscCache = Join-Path (Split-Path $vrExe -Parent) 'disc.cfg'
    [IO.File]::WriteAllText($vrDiscCache, $DiscPath, (New-Object Text.UTF8Encoding($false)))
    foreach ($vrKey in $vrVariables.Keys) {
        $vrOldEnvironment[$vrKey] = [Environment]::GetEnvironmentVariable($vrKey,'Process')
        [Environment]::SetEnvironmentVariable($vrKey,$vrVariables[$vrKey],'Process')
    }
    $vrProcess = Start-Process -FilePath $vrExe -ArgumentList '--no-launcher','--game','game.toml','--disc',('"' + $DiscPath + '"') -WorkingDirectory $vrRoot -WindowStyle Hidden -PassThru
    if ($CaptureDirectory) {
        $vrSaveSlot = if ($Slot -ge 0) {$Slot} else {3}
        python vr/capture_stereo.py $CaptureDirectory --slot $vrSaveSlot --pairs 2 --executable $vrExe
        if ($LASTEXITCODE -ne 0) { throw 'VR capture failed' }
    } elseif ($Slot -ge 0) {
        # Use the capture utility readiness/load protocol without committing assets.
        $vrRunDir = 'analysis/vr-proof/run-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
        python vr/capture_stereo.py $vrRunDir --slot $Slot --pairs 0 --executable $vrExe
        if ($LASTEXITCODE -ne 0) { throw 'VR slot load failed' }
    } elseif (-not $Desktop) {
        $vrDeadline = [DateTime]::UtcNow.AddSeconds(30)
        $vrReady = $false
        $vrLastStatus = $null
        while ([DateTime]::UtcNow -lt $vrDeadline) {
            if ($vrProcess.HasExited) { throw "Game exited during VR startup (code $($vrProcess.ExitCode)). Check your active OpenXR runtime and disc." }
            try { $vrLastStatus = Get-VRStartupStatus } catch { $vrLastStatus = $null }
            if ($vrLastStatus.running -and $vrLastStatus.submitted -gt 0) {
                Write-Host "VR active: $($vrLastStatus.runtime); submissions: $($vrLastStatus.submitted)"
                $vrReady = $true
                break
            }
            if ($vrLastStatus.failures -gt 0) { break }
            Start-Sleep -Milliseconds 200
        }
        if (-not $vrReady) { throw ('VR startup failed; no headset frames submitted. This build needs an OpenGL-capable OpenXR runtime (check the active runtime, or pass -Runtime). Status: ' + ($vrLastStatus | ConvertTo-Json -Compress)) }
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
