@echo off
REM ============================================================
REM   IRON COMMAND  -  double-click to build ^& play (Windows)
REM   Factory-RTS: C&C x Satisfactory. LAN free-for-all.
REM ============================================================
cd /d "%~dp0"
echo ======================================
echo         IRON  COMMAND
echo ======================================

where cmake >nul 2>nul
if errorlevel 1 (
  echo.
  echo Missing CMake. Install CMake and a C++ compiler ^(Visual Studio Build Tools^),
  echo plus GLFW/GLM, then run this again.
  echo.
  pause
  exit /b 1
)

REM Install bundled games the first time
if not exist "%USERPROFILE%\.voxelengine" (
  if exist "content\voxelengine-saves" (
    echo Installing games to %USERPROFILE%\.voxelengine ...
    xcopy /E /I /Q "content\voxelengine-saves" "%USERPROFILE%\.voxelengine" >nul
  )
)

REM Build if needed
if not exist "build\VoxelEngine.exe" (
  echo Building the engine ^(first time only^)...
  cmake -S . -B build || ( echo Build config failed & pause & exit /b 1 )
  cmake --build build --config Release || ( echo Build failed & pause & exit /b 1 )
)

echo Launching Iron Command...
if exist "build\Release\VoxelEngine.exe" (
  cd build\Release && VoxelEngine.exe ironcommand
) else (
  cd build && VoxelEngine.exe ironcommand
)
