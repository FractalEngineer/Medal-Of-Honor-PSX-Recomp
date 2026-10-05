param(
    [ValidateSet('all','rifle','pistol','bar','thompson','mp40','shotgun','scoped-rifle','bazooka','frag-grenade','stick-grenade')]
    [string]$Weapon = 'all',
    [ValidateSet('tracked','native','compare')][string]$Mode = 'tracked',
    [ValidateRange(1,300)][int]$Seconds = 30,
    [ValidateSet('native','720p','1080p','1440p','4k','5k','8k','display')]
    [string]$InternalResolution = '1080p',
    [string]$BuildDirectory = 'build-release',
    [string]$DiscPath = '',
    [switch]$Desktop,
    [switch]$Verify,
    [switch]$List
)
$ErrorActionPreference = 'Stop'
$batchRoot = Split-Path $PSScriptRoot -Parent
$batchArguments = @((Join-Path $PSScriptRoot 'weapon_batch.py'), '--weapons', $Weapon)
$batchArguments += @('--mode',$Mode,'--seconds',[string]$Seconds,
    '--internal-resolution',$InternalResolution,'--build-directory',$BuildDirectory)
if ($DiscPath) { $batchArguments += @('--disc',$DiscPath) }
if ($Desktop) { $batchArguments += '--desktop' }
if ($Verify) { $batchArguments += '--verify' }
if ($List) { $batchArguments += '--list' }
Push-Location $batchRoot
try {
    python @batchArguments
    if ($LASTEXITCODE -ne 0) { throw "Weapon batch failed (code $LASTEXITCODE). See its local receipt." }
} finally { Pop-Location }
