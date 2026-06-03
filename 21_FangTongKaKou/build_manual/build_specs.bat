@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1

set "PROJECT_DIR=%~dp0.."
if not exist "%PROJECT_DIR%\build_manual" mkdir "%PROJECT_DIR%\build_manual"

cl /nologo /LD /EHsc /O2 /MD /DNDEBUG /DWIN32 /D_WINDOWS /D_USRDLL /D_CRT_SECURE_NO_WARNINGS /utf-8 ^
  "%PROJECT_DIR%\src\FangTongKaKouSpecs.cpp" ^
  /Fe"%PROJECT_DIR%\build_manual\FangTongKaKouSpecs.dll" ^
  /Fo"%PROJECT_DIR%\build_manual\FangTongKaKouSpecs.obj" ^
  /link shell32.lib user32.lib
