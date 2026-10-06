@echo off
rem VR launch pinned to the SteamVR OpenXR runtime.
call "%~dp0RunVR.bat" -Runtime steamvr %*
exit /b %ERRORLEVEL%
