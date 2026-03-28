@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d D:\fivem\vendor\libnode\bin
lib /def:libnode22.def /machine:x64 /out:libnode22.lib
if errorlevel 1 exit /b 1
lib /def:libuv.def /machine:x64 /out:libuv.lib
if errorlevel 1 exit /b 1

