@echo off
setlocal
cd /d "%~dp0"
pushd "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
for /f "tokens=*" %%i in ('vswhere.exe -latest -version "[17.0,18.0)" -property installationPath') do set "OVERLAY_VS=%%i"
popd
if not defined OVERLAY_VS exit /b 1
call "%OVERLAY_VS%\VC\Auxiliary\Build\vcvars64.bat" > nul
if errorlevel 1 exit /b 1
if not exist tests\.deps mkdir tests\.deps
cl /nologo /EHsc /std:c++17 /utf-8 tests\overlay_seekbar_state_test.cpp /Fo:tests\.deps\overlay_seekbar_state_test.obj /Fe:tests\.deps\overlay_seekbar_state_test.exe
if errorlevel 1 exit /b 1
tests\.deps\overlay_seekbar_state_test.exe
exit /b %errorlevel%
