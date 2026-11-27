@echo off
setlocal DisableDelayedExpansion
pushd "%~dp0" || exit /b 1
"%~dp0gooseinfer-launcher.exe" --settings
set "GOOSEINFER_EXIT=%ERRORLEVEL%"
popd
if not "%GOOSEINFER_EXIT%"=="0" pause
exit /b %GOOSEINFER_EXIT%
