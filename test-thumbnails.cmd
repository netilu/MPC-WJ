@echo off
setlocal
cd /d "%~dp0"
pushd "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
for /f "tokens=*" %%i in ('vswhere.exe -latest -version "[17.0,18.0)" -property installationPath') do set "THUMB_VS=%%i"
popd
if not defined THUMB_VS exit /b 1
call "%THUMB_VS%\VC\Auxiliary\Build\vcvars64.bat" > nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 tests\thumbnail_layout_test.cpp /Fo:tests\thumbnail_layout_test.obj /Fe:tests\thumbnail_layout_test.exe
if errorlevel 1 exit /b 1
tests\thumbnail_layout_test.exe
exit /b %errorlevel%
