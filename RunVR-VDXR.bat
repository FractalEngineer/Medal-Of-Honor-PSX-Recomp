@echo off
rem VR launch pinned to the Virtual Desktop (VDXR) OpenXR runtime.
call "%~dp0RunVR.bat" -Runtime vdxr %*
exit /b %ERRORLEVEL%
