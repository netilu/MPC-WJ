@echo off
setlocal
cd /d "%~dp0.."
call "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" > nul
if errorlevel 1 exit /b 1
if not exist "tests\player" mkdir "tests\player"
cl /nologo /EHsc /std:c++17 /utf-8 tests\inspect_crash_dump.cpp /Fo:tests\player\inspect_crash_dump.obj /Fe:tests\player\inspect_crash_dump.exe
if errorlevel 1 exit /b 1
tests\player\inspect_crash_dump.exe "%~1"
exit /b %errorlevel%
