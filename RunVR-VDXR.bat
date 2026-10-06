@echo off
setlocal
rem VR launch pinned to the Virtual Desktop (VDXR) OpenXR runtime.
rem The runtime is chosen by which launcher you run; the system's active runtime
rem is unchanged. Uses the accepted VR settings and tracked-weapon controls.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0vr\run_vr.ps1" -Runtime vdxr -WeaponPoseDiagnostic %*
set "VR_EXIT_CODE=%ERRORLEVEL%"
if not "%VR_EXIT_CODE%"=="0" (
    echo.
    echo VR launch failed. See the error above.
    pause
)
exit /b %VR_EXIT_CODE%
