@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -no_logo
cd /d C:\ProjectWorldbuilder\TheSuperHackers
cmake --build build/win32 --target z_worldbuilder --config Release
echo EXITCODE:%ERRORLEVEL%
