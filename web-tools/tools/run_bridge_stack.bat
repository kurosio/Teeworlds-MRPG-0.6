@echo off
setlocal enableextensions

set "ROOT_DIR=%~dp0"
set "SKINS_DIR=%ROOT_DIR%skins_api"
set "DISCORD_DIR=%ROOT_DIR%discord"
set "VENV_DIR=%ROOT_DIR%.venv"
set "PYTHON_CMD=py"

if not "%BRIDGE_HOST%"=="" (
  set "BRIDGE_HOST=%BRIDGE_HOST%"
) else (
  set "BRIDGE_HOST=127.0.0.1"
)
if not "%BRIDGE_PORT%"=="" (
  set "BRIDGE_PORT=%BRIDGE_PORT%"
) else (
  set "BRIDGE_PORT=8000"
)
if not "%SKIP_INSTALL%"=="" (
  set "SKIP_INSTALL=%SKIP_INSTALL%"
) else (
  set "SKIP_INSTALL=0"
)

if not exist "%VENV_DIR%\Scripts\python.exe" (
  echo [run_bridge_stack] Creating virtualenv...
  %PYTHON_CMD% -3 -m venv "%VENV_DIR%"
  if errorlevel 1 (
    echo [run_bridge_stack] Failed to create virtualenv.
    exit /b 1
  )
)
set "PYTHON_CMD=%VENV_DIR%\Scripts\python.exe"

if "%SKIP_INSTALL%"=="1" (
  echo [run_bridge_stack] SKIP_INSTALL=1 ^(dependency installation skipped^).
) else (
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
)

echo [run_bridge_stack] Starting skins_api on http://%BRIDGE_HOST%:%BRIDGE_PORT% ...
start "skins_api" /min cmd /c "cd /d "%SKINS_DIR%" && "%PYTHON_CMD%" -m uvicorn main:app --host %BRIDGE_HOST% --port %BRIDGE_PORT%"

REM Give API a few seconds to start
ping 127.0.0.1 -n 3 >nul

echo [run_bridge_stack] Starting discord bridge...
cd /d "%DISCORD_DIR%"
"%PYTHON_CMD%" main.py

endlocal
