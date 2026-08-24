@echo off
REM TheSuperHackers @bugfix Nemellud 24/08/2026 Build with MSVC 14.44 (VS 2022 BuildTools), not the
REM 14.50 toolset that ships with Visual Studio 18. A full rebuild with 14.50 produces a WorldBuilder
REM that crashes on startup: DX8Wrapper::Get_Current_Caps() is still null when a vertex/index buffer
REM is created, and the WWASSERT that would have caught it is compiled out in Release, so
REM DX8Caps::Support_TnL dereferences a null pointer. The same sources built with 14.44 start and run
REM normally. It went unnoticed for months because only incremental builds were done - the object
REM files from the last full build (April, 14.44) kept being reused.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86 -no_logo
cd /d C:\ProjectWorldbuilder\TheSuperHackers
cmake --build build/win32-1444 --target z_worldbuilder --config Release
echo EXITCODE:%ERRORLEVEL%
