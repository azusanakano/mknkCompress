@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-windows.ps1" -Architecture All -Configuration Release
if errorlevel 1 pause & exit /b 1
pause
