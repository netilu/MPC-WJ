@echo off
setlocal
cd /d "%~dp0"
set "CL=/utf-8"
call build.bat Build x64 Release VS2022 NoWait > thumbnail-build.log 2>&1
if errorlevel 1 (
  type thumbnail-build.log
  exit /b 1
)
call test-thumbnails.cmd
if errorlevel 1 exit /b 1
echo Built: %CD%\_bin\mpc-be_x64\mpc-be64.exe
echo Log: %CD%\thumbnail-build.log
