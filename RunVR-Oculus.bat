@echo off
rem VR launch pinned to the Oculus (Quest Link / Air Link) OpenXR runtime.
call "%~dp0RunVR.bat" -Runtime oculus %*
exit /b %ERRORLEVEL%
