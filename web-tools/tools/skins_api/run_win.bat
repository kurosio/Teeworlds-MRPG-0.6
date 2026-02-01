@echo off
setlocal

pushd "%~dp0" >nul
py -m uvicorn main:app --reload --host 127.0.0.1 --port 8000

popd >nul
echo.
echo server stopped. press any key to close...
pause >nul
