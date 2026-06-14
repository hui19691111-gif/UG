@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1

set "PROJECT_DIR=%~dp0.."
if "%UGII_BASE_DIR%"=="" set "UGII_BASE_DIR=D:\Program Files\Siemens\NX2412"

if not exist "%PROJECT_DIR%\build_manual" mkdir "%PROJECT_DIR%\build_manual"

cl /nologo /LD /EHsc /std:c++17 /O2 /MD /DNDEBUG /DWIN32 /D_WINDOWS /D_USRDLL /D_CRT_SECURE_NO_WARNINGS /utf-8 ^
  /I"%UGII_BASE_DIR%\UGOPEN" /I"%UGII_BASE_DIR%\UGOPEN\NXOpen" /I"%PROJECT_DIR%" ^
  "%PROJECT_DIR%\src\FanTonGMK.cpp" ^
  /Fe"%PROJECT_DIR%\build_manual\FanTonGMK.dll" ^
  /Fo"%PROJECT_DIR%\build_manual\FanTonGMK.obj" ^
  /link /LIBPATH:"%UGII_BASE_DIR%\UGOPEN" libufun.lib libnxopencpp.lib libnxopencpp_features.lib libnxopencpp_tooling.lib libnxopenuicpp.lib shell32.lib user32.lib

copy /Y "%PROJECT_DIR%\FanTonGMK.dlx" "%PROJECT_DIR%\build_manual\FanTonGMK.dlx" >nul
copy /Y "%PROJECT_DIR%\FanTon1.bmp" "%PROJECT_DIR%\build_manual\FanTon1.bmp" >nul
copy /Y "%PROJECT_DIR%\FanTon2.bmp" "%PROJECT_DIR%\build_manual\FanTon2.bmp" >nul
copy /Y "%PROJECT_DIR%\FanTon1Icon.bmp" "%PROJECT_DIR%\build_manual\FanTon1Icon.bmp" >nul
copy /Y "%PROJECT_DIR%\FanTon2Icon.bmp" "%PROJECT_DIR%\build_manual\FanTon2Icon.bmp" >nul
