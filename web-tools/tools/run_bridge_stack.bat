@echo off
setlocal enableextensions

set "ROOT_DIR=%~dp0"
set "SKINS_DIR=%ROOT_DIR%skins_api"
set "DISCORD_DIR=%ROOT_DIR%discord"
set "PYTHON_CMD=py"

echo [run_bridge_stack] Checking and installing missing dependencies...
%PYTHON_CMD% -m pip install --disable-pip-version-check -q -r "%SKINS_DIR%\requires.txt"
if errorlevel 1 (
  echo [run_bridge_stack] Failed to install skins_api dependencies.
  exit /b 1
)
%PYTHON_CMD% -m pip install --disable-pip-version-check -q -r "%DISCORD_DIR%\requires.txt"
if errorlevel 1 (
  echo [run_bridge_stack] Failed to install discord bridge dependencies.
  exit /b 1
)

echo [run_bridge_stack] Starting skins_api on http://127.0.0.1:8000 ...
start "skins_api" /min cmd /c "cd /d "%SKINS_DIR%" && %PYTHON_CMD% -m uvicorn main:app --host 127.0.0.1 --port 8000"

REM Give API a few seconds to start
ping 127.0.0.1 -n 3 >nul

echo [run_bridge_stack] Starting discord bridge...
cd /d "%DISCORD_DIR%"
%PYTHON_CMD% main.py

endlocal
