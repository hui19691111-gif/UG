@echo off
setlocal
set PROJECT_DIR=%~dp0..
set PROJECT=%PROJECT_DIR%\XuanZeGuanJian.vcxproj

if "%UGII_BASE_DIR%"=="" (
  set UGII_BASE_DIR=D:\Program Files\Siemens\NX2412
)

where msbuild.exe >nul 2>nul
if errorlevel 1 (
  call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
)

msbuild "%PROJECT%" /p:Configuration=Release /p:Platform=x64 /m
exit /b %errorlevel%
