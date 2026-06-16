@echo off
setlocal

set "BASH_EXE="
where bash >nul 2>nul && set "BASH_EXE=bash" && goto :found
if exist "C:\Program Files\Git\bin\bash.exe" set "BASH_EXE=C:\Program Files\Git\bin\bash.exe" && goto :found
if exist "C:\Program Files (x86)\Git\bin\bash.exe" set "BASH_EXE=C:\Program Files (x86)\Git\bin\bash.exe" && goto :found

echo [ERROR] bash not found. Please install Git for Windows.
exit /b 1

:found
"%BASH_EXE%" "%~dp0run_gui.sh"
