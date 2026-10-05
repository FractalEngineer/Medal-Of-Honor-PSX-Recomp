param(
    [ValidateRange(0,11)][int]$Slot = 6,
    [ValidateRange(1,300)][int]$Seconds = 30,
    [string]$BuildDirectory = 'build-vr-weapons',
    [string]$DiscPath = '',
    [switch]$Desktop,
    [switch]$Verify
)
$ErrorActionPreference = 'Stop'
$weaponParameters = @{
    Slot = $Slot
    Seconds = $Seconds
    BuildDirectory = $BuildDirectory
    DiscPath = $DiscPath
    WeaponPoseDiagnostic = $true
    Desktop = $Desktop.IsPresent
    DesktopFov = $Desktop.IsPresent
    Verify = $Verify.IsPresent
}
& (Join-Path $PSScriptRoot 'run_vr.ps1') @weaponParameters
