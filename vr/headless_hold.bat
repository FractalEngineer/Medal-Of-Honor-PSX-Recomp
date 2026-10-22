@echo off
rem Headless session that holds, for probing and dumps. cmd /c <this bat> works where a direct
rem powershell -File background launch does not, mirroring how the RunVR bats are started.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_vr.ps1" -Headless -Seconds 180 %*
