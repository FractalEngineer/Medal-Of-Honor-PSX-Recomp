@echo off
setlocal
rem Flat two-player setup for making weapon reference save states.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0vr\run_weapon_capture.ps1" %*
set "CAPTURE_EXIT_CODE=%ERRORLEVEL%"
if not "%CAPTURE_EXIT_CODE%"=="0" (
    echo Weapon capture launch failed. See the error above.
    pause
)
exit /b %CAPTURE_EXIT_CODE%
