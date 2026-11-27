@echo off
setlocal DisableDelayedExpansion
pushd "%~dp0" || exit /b 1
"%~dp0gooseinfer-launcher.exe" --diagnostics
set "GOOSEINFER_EXIT=%ERRORLEVEL%"
popd
pause
exit /b %GOOSEINFER_EXIT%
