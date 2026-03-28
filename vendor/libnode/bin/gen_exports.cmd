@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d D:\fivem\vendor\libnode\bin
dumpbin /exports libnode22.dll > libnode22.exports.txt
dumpbin /exports libuv.dll > libuv.exports.txt

