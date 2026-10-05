@echo off
setlocal
rem Uses the accepted VR settings and tracked-weapon controls. Close the game to exit.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0vr\run_vr.ps1" -WeaponPoseDiagnostic %*
set "VR_EXIT_CODE=%ERRORLEVEL%"
if not "%VR_EXIT_CODE%"=="0" (
    echo.
    echo VR launch failed. See the error above.
    pause
)
exit /b %VR_EXIT_CODE%
