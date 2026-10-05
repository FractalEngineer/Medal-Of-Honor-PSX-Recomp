@echo off
setlocal
rem Local weapon candidate: movement enabled, 30 seconds, slot 6 by default.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0vr\run_weapon_check.ps1" %*
exit /b %ERRORLEVEL%
