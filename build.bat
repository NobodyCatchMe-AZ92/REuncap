@echo off
rem Builds build\reuncap.asi (32-bit). Needs Visual Studio 2019/2022 with "Desktop development with C++".
cd /d %~dp0
where cl >nul 2>nul
if not errorlevel 1 goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto novs
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto novs
call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat" >nul
:build
if not exist build mkdir build
cl /nologo /O2 /MT /W3 /LD /EHsc src\reuncap.cpp /Fobuild\ /Febuild\reuncap.asi /link /DLL user32.lib winmm.lib kernel32.lib
set "RC=%errorlevel%"
rem intermediate files record the local build path - keep only the .asi
del /q build\reuncap.obj build\reuncap.exp build\reuncap.lib 2>nul
exit /b %RC%
:novs
echo Visual Studio with the C++ tools was not found. Install "Desktop development with C++" or run from a VS developer prompt.
exit /b 1
