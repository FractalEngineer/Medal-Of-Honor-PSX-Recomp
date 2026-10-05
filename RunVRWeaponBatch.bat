@echo off
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0vr\run_weapon_batch.ps1" %*
exit /b %errorlevel%
