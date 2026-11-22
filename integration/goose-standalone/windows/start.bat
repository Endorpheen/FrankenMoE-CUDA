@echo off
setlocal DisableDelayedExpansion
pushd "%~dp0" || exit /b 1
"%~dp0frankenmoe-launcher.exe"
set "FRANKENMOE_EXIT=%ERRORLEVEL%"
popd
if not "%FRANKENMOE_EXIT%"=="0" pause
exit /b %FRANKENMOE_EXIT%
