@echo off
setlocal
rem VR launch pinned to the SteamVR OpenXR runtime, without the wrist HUD replacement.
rem -WristHudReplace changes guest HUD code, and shards are keyed by region and content, so with it the
rem overlay loader registers nothing and every region runs interpreted. Dropping -WristHud and
rem -WristHudReplace keeps the shard route live, which is what brings the pair cost under the 90 Hz budget
rem and removes the reprojection warp on head motion. Use RunVR-SteamVR.bat when the wrist HUD matters more.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0vr\run_vr.ps1" -Runtime steamvr -WeaponPoseDiagnostic -BuildDirectory build-release %*
set "VR_EXIT_CODE=%ERRORLEVEL%"
if not "%VR_EXIT_CODE%"=="0" (
    echo.
    echo VR launch failed. See the error above.
    pause
)
exit /b %VR_EXIT_CODE%
