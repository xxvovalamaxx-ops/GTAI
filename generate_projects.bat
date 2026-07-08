@echo off
cd /d "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles"
call Build.bat -projectfiles -project="D:\Projects\GitHub Projects\GTAI\GTA7_UE5\GTA7.uproject" -game -rocket -progress
echo Exit code: %ERRORLEVEL%
