param(
    [ValidateSet('keyboard','gamepad')][string]$Player1Device = 'keyboard',
    [ValidateSet('keyboard','gamepad')][string]$Player2Device = 'keyboard',
    [string]$BuildDirectory = 'build-release',
    [string]$DiscPath = '',
    [ValidateRange(0,86400)][int]$Seconds = 0,
    [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
$captureRoot = Split-Path $PSScriptRoot -Parent
$captureExe = Join-Path $captureRoot 'Medal_of_Honor__Recompiled.exe'
if (-not (Test-Path -LiteralPath $captureExe)) {
    $captureExe = Join-Path (Join-Path $captureRoot $BuildDirectory) 'Medal_of_Honor__Recompiled.exe'
}
if (-not (Test-Path -LiteralPath $captureExe)) { throw "Game executable missing: $captureExe" }
$captureExe = (Resolve-Path -LiteralPath $captureExe).Path
$captureExeDirectory = Split-Path $captureExe -Parent

# Edit only one simple INI/TOML section; retain unrelated rows and sections.
function Set-CaptureSection([string]$Text, [string]$Section, [System.Collections.IDictionary]$Values) {
    $rows = New-Object 'System.Collections.Generic.List[string]'
    $pending = @{}
    foreach ($key in $Values.Keys) { $pending[$key] = $Values[$key] }
    $inside = $false
    $found = $false
    foreach ($row in ($Text -split '\r?\n')) {
        if ($row -match '^\s*\[([^\]]+)\]\s*(?:[#;].*)?$') {
            if ($inside) {
                foreach ($key in @($pending.Keys | Sort-Object)) { $rows.Add("$key = $($pending[$key])"); $pending.Remove($key) }
            }
            $inside = $Matches[1] -eq $Section
            if ($inside -and $found) { throw "Duplicate [$Section] section; fix the file before capture." }
            if ($inside) { $found = $true }
        } elseif ($inside -and $row -match '^\s*([A-Za-z0-9_]+)\s*=') {
            $key = $Matches[1]
            if ($Values.Contains($key)) {
                if ($pending.ContainsKey($key)) { $rows.Add("$key = $($pending[$key])"); $pending.Remove($key) }
                continue
            }
        }
        $rows.Add($row)
    }
    if (-not $found) { $rows.Add("[$Section]") }
    foreach ($key in @($pending.Keys | Sort-Object)) { $rows.Add("$key = $($pending[$key])") }
    return ($rows -join "`r`n") + "`r`n"
}

function Get-CapturePadStatus {
    $client = New-Object System.Net.Sockets.TcpClient
    try {
        if (-not $client.ConnectAsync('127.0.0.1',4370).Wait(500)) { throw 'Pad status connection timed out.' }
        $client.ReceiveTimeout = 1000
        $stream = $client.GetStream()
        $request = [Text.Encoding]::UTF8.GetBytes('{"cmd":"pad_status"}' + "`n")
        $stream.Write($request,0,$request.Length)
        $reader = New-Object System.IO.StreamReader($stream)
        return ($reader.ReadLine() | ConvertFrom-Json)
    } finally { $client.Dispose() }
}

$captureSettings = Join-Path $captureExeDirectory 'settings.toml'
$captureKeys = Join-Path $captureExeDirectory 'keybinds.ini'
$captureEdits = @{}
$settingsText = if (Test-Path -LiteralPath $captureSettings) { [IO.File]::ReadAllText($captureSettings) } else { '' }
$captureEdits[$captureSettings] = Set-CaptureSection $settingsText 'controller' @{
    p1_device = '"' + $Player1Device + '"'; p2_device = '"' + $Player2Device + '"'
    p1_mode = '"digital"'; p2_mode = '"digital"'; multitap = 'false'; multitap_analog = 'false'
}
$keyText = if (Test-Path -LiteralPath $captureKeys) { [IO.File]::ReadAllText($captureKeys) } else { '' }
$keyText = Set-CaptureSection $keyText 'player1' @{
    up='Up'; down='Down'; left='Left'; right='Right'; cross='X'; circle='S'; square='Z'; triangle='A'
    l1='Q'; r1='W'; l2='E'; r2='R'; l3='T'; r3='Y'; start='Return'; select='Right Shift'
    ls_up='Up'; ls_down='Down'; ls_left='Left'; ls_right='Right'
    rs_up='None'; rs_down='None'; rs_left='None'; rs_right='None'
}
$captureEdits[$captureKeys] = Set-CaptureSection $keyText 'player2' @{
    up='I'; down='K'; left='J'; right='L'; cross='P'; circle='O'; square='H'; triangle='U'
    l1='N'; r1='M'; l2='B'; r2='V'; l3='F'; r3='G'; start='Tab'; select='Left Shift'
    ls_up='I'; ls_down='K'; ls_left='J'; ls_right='L'
    rs_up='None'; rs_down='None'; rs_left='None'; rs_right='None'
}
Write-Host "Flat weapon capture: P1=$Player1Device; P2=$Player2Device; two digital ports, no multitap."
Write-Host 'P1: arrows; X/S/Z/A = Cross/Circle/Square/Triangle; Q/W/E/R = L1/R1/L2/R2; Enter = Start.'
Write-Host 'P2: I/K/J/L; P/O/H/U = Cross/Circle/Square/Triangle; N/M/B/V = L1/R1/L2/R2; Tab = Start.'
Write-Host 'Choose multiplayer in the game. Close the game to restore your settings and keyboard bindings.'
if ($CheckOnly) { Write-Host 'Configuration checked; no files changed and no game launched.'; return }

$captureSocket = New-Object System.Net.Sockets.TcpClient
try {
    $captureSocket.Connect('127.0.0.1',4370)
    throw 'A game/debug server already uses port 4370. Close it before capture.'
} catch [System.Net.Sockets.SocketException] {
    # Refused connection is expected before the owned process starts.
} finally { $captureSocket.Dispose() }
if (Get-Process -Name 'Medal_of_Honor__Recompiled' -ErrorAction SilentlyContinue) {
    throw 'Close the existing Medal of Honor game before temporarily changing controller settings.'
}
if ($DiscPath) {
    if (-not (Test-Path -LiteralPath $DiscPath -PathType Leaf)) { throw "Disc image missing: $DiscPath" }
    $DiscPath = (Resolve-Path -LiteralPath $DiscPath).Path
    if ($DiscPath.Contains('"')) { throw 'Disc path contains an unsupported quote.' }
}
$captureBackup = Join-Path $captureRoot ('analysis/weapon-capture/config-' + [Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $captureBackup
$captureOriginals = @{}
$captureOldEnvironment = @{}
$captureProcess = $null
try {
    foreach ($path in $captureEdits.Keys) {
        if (Test-Path -LiteralPath $path) {
            $captureOriginals[$path] = [IO.File]::ReadAllBytes($path)
            [IO.File]::WriteAllBytes((Join-Path $captureBackup (Split-Path $path -Leaf)), $captureOriginals[$path])
        } else { $captureOriginals[$path] = $null }
    }
    # Back up both originals before changing either file.
    foreach ($path in $captureEdits.Keys) { [IO.File]::WriteAllText($path,$captureEdits[$path],(New-Object Text.UTF8Encoding($false))) }
    Write-Host "Original config backup: $captureBackup"
    $captureEnvironment = @{ PSX_OPENXR='0'; PSX_DEV_INPUT='0'; PSX_NETPLAY='0'; PSX_HEADLESS='0' }
    foreach ($variable in Get-ChildItem Env:PSX_VR_*) { $captureEnvironment[$variable.Name] = '0' }
    foreach ($key in @('PSX_VR_STEREO','PSX_VR_OPENXR','PSX_VR_MOVEMENT','PSX_VR_WEAPON_POSE','PSX_VR_WEAPON_AIM')) { $captureEnvironment[$key] = '0' }
    foreach ($key in $captureEnvironment.Keys) {
        $captureOldEnvironment[$key] = [Environment]::GetEnvironmentVariable($key,'Process')
        [Environment]::SetEnvironmentVariable($key,$captureEnvironment[$key],'Process')
    }
    $captureArguments = @('--no-launcher','--game','game.toml','--debug-port','4370')
    if ($DiscPath) { $captureArguments += @('--disc',('"' + $DiscPath + '"')) }
    $captureProcess = Start-Process -FilePath $captureExe -ArgumentList $captureArguments -WorkingDirectory $captureRoot -WindowStyle Hidden -PassThru
    $captureDeadline = [DateTime]::UtcNow.AddSeconds(30)
    $captureReady = $false
    while ([DateTime]::UtcNow -lt $captureDeadline) {
        if ($captureProcess.HasExited) { throw "Game exited during capture startup (code $($captureProcess.ExitCode))." }
        try { $status = Get-CapturePadStatus } catch { $status = $null }
        if ($status.ok -and $status.slot0.connected -and $status.slot1.connected -and -not $status.slot0.analog -and -not $status.slot1.analog) {
            $captureReady = $true
            Write-Host 'TCP verified: player 1 and player 2 are connected digital pads.'
            break
        }
        Start-Sleep -Milliseconds 200
    }
    if (-not $captureReady) { throw 'Two digital pads were not verified. Use a build with TCP debug tools enabled.' }
    if ($Seconds -gt 0) { $null = $captureProcess.WaitForExit($Seconds * 1000) }
    else { $captureProcess.WaitForExit() }
    if ($captureProcess.HasExited -and $captureProcess.ExitCode -ne 0) { throw "Game exited with code $($captureProcess.ExitCode)." }
} finally {
    if ($captureProcess -and -not $captureProcess.HasExited) {
        Stop-Process -Id $captureProcess.Id -ErrorAction SilentlyContinue
        $captureProcess.WaitForExit()
    }
    foreach ($path in $captureOriginals.Keys) {
        if ($null -ne $captureOriginals[$path]) { [IO.File]::WriteAllBytes($path,$captureOriginals[$path]) }
        elseif (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    foreach ($key in $captureOldEnvironment.Keys) { [Environment]::SetEnvironmentVariable($key,$captureOldEnvironment[$key],'Process') }
}
