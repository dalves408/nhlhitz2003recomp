@echo off
setlocal
rem Always resolve paths from this checkout, including when double-clicked.
pushd "%~dp0" || exit /b 1
set "HITZ_BUILD_DIR=native\build"
set "HITZ_DEBUGKIT=OFF"
set "HITZ_GAME_EXE=nhl_hitz_2003.exe"
set "HITZ_BUILD_ONLY=0"
if /i "%~1"=="--debug" (
    set "HITZ_BUILD_DIR=native\build-debug"
    set "HITZ_DEBUGKIT=ON"
    set "HITZ_GAME_EXE=nhl_hitz_2003-db.exe"
    shift
)
if /i "%~1"=="--build-only" (
    set "HITZ_BUILD_ONLY=1"
    shift
)
if not "%~1"=="" (
    echo Usage: play.bat [--debug] [--build-only]
    goto failed
)

where cmake.exe >nul 2>&1
if errorlevel 1 (
    if exist "%ProgramFiles%\CMake\bin\cmake.exe" (
        set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
    ) else (
        echo ERROR: CMake is missing. Install CMake and Visual Studio 2022 C++ build tools.
        goto failed
    )
)
if not exist "native\src\recomp\gen\recomp_0029.c" (
    echo ERROR: Generated game sources are missing. Follow the README setup steps first.
    goto failed
)

rem Reuse the configured build; CMake automatically detects source/build-file changes.
if not exist "%HITZ_BUILD_DIR%\CMakeCache.txt" goto configure
findstr /l /x /c:"HITZ_DEBUGKIT:BOOL=%HITZ_DEBUGKIT%" "%HITZ_BUILD_DIR%\CMakeCache.txt" >nul
if not errorlevel 1 goto build
:configure
cmake -S native -B "%HITZ_BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 -DHITZ_DEBUGKIT=%HITZ_DEBUGKIT%
if errorlevel 1 goto failed
:build
echo Building latest Release game in %HITZ_BUILD_DIR%...
cmake --build "%HITZ_BUILD_DIR%" --config Release --target nhl_hitz_2003_recomp --parallel 6
if errorlevel 1 goto failed
if "%HITZ_BUILD_ONLY%"=="1" goto success
if not exist "native\game\default.xbe" (
    echo ERROR: native\game\default.xbe is missing. Follow the README game-data setup.
    goto failed
)
if "%HITZ_DEBUGKIT%"=="ON" (
    echo Starting xrdbg build. Press DEL to mark a glitch.
    echo Debug files: %LOCALAPPDATA%\NHLHitz2003\debug
)
rem WinMain loads game\default.xbe relative to native, not the project root.
pushd native || goto failed
start "" /wait "%~dp0%HITZ_BUILD_DIR%\Release\%HITZ_GAME_EXE%"
set "HITZ_EXIT=%ERRORLEVEL%"
popd
if not "%HITZ_EXIT%"=="0" (
    echo Game exited with code %HITZ_EXIT%.
    popd
    pause
    exit /b %HITZ_EXIT%
)
:success
popd
exit /b 0
:failed
echo Launch stopped. No older executable was started.
popd
pause
exit /b 1
