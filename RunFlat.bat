@echo off
setlocal
rem Flat launch from either an extracted release or a local build.
for %%V in (PSX_OPENXR PSX_VR_OPENXR PSX_VR_STEREO PSX_VR_MOVEMENT PSX_VR_WEAPON_AIM PSX_VR_WEAPON_POSE PSX_VR_PROBE PSX_VR_INTERP PSX_VR_PASS_PROBE PSX_VR_PASS_WATCHDOG PSX_VR_DESKTOP_FOV PSX_VR_OFFSET PSX_VR_HEAD_YAW PSX_VR_HEAD_POSITION) do set "%%V="
set "FLAT_EXE=%~dp0Medal_of_Honor__Recompiled.exe"
if not exist "%FLAT_EXE%" set "FLAT_EXE=%~dp0build-release\Medal_of_Honor__Recompiled.exe"
if not exist "%FLAT_EXE%" (
    echo Game executable missing. Extract the release completely, or build the game first.
    pause
    exit /b 1
)
pushd "%~dp0"
"%FLAT_EXE%" --game game.toml %*
set "FLAT_EXIT_CODE=%ERRORLEVEL%"
popd
if not "%FLAT_EXIT_CODE%"=="0" (
    echo Flat launch failed with code %FLAT_EXIT_CODE%.
    pause
)
exit /b %FLAT_EXIT_CODE%
