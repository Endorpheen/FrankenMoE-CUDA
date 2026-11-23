@echo off
setlocal DisableDelayedExpansion
pushd "%~dp0" || exit /b 1
"%~dp0frankenmoe-launcher.exe" --diagnostics
set "FRANKENMOE_EXIT=%ERRORLEVEL%"
popd
pause
exit /b %FRANKENMOE_EXIT%
