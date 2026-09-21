@echo off
REM Build Window Sweaters (Windows port) with MSVC. No CMake required.
setlocal
set VSDEV="C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat"
if exist %VSDEV% call %VSDEV% -arch=x64 >nul
where cl >nul 2>&1
if errorlevel 1 ( echo [x] cl.exe not found. Install VS Build Tools + Windows SDK. & exit /b 1 )
if not exist out mkdir out
cl /nologo /O2 /W3 /DUNICODE /D_UNICODE /I src\core /I src\win /Fo"out\\" ^
  src\core\table.c src\core\parse.c src\core\apps.c src\core\charts.c src\core\knit_core.c ^
  src\win\knit_gdi.c src\win\overlay.c src\win\tracker.c src\win\events.c ^
  src\win\autoyarn.c src\win\tray.c src\win\ipc.c src\win\prefs.c src\win\startup.c src\win\main_win.c ^
  /Fe:out\WindowSweaters.exe /link dwmapi.lib shell32.lib gdi32.lib user32.lib ole32.lib windowscodecs.lib advapi32.lib
if errorlevel 1 ( echo [x] build failed & exit /b 1 )
echo [ok] out\WindowSweaters.exe
REM render sanity test (no window needed): writes out\tile_test.bmp
cl /nologo /O2 /W3 /DUNICODE /D_UNICODE /I src\core /I src\win /Fo"out\\" ^
  src\core\table.c src\core\apps.c src\core\charts.c src\core\knit_core.c ^
  src\win\knit_gdi.c src\win\autoyarn.c tests\render_test.c ^
  /Fe:out\tile_test.exe /link gdi32.lib user32.lib ole32.lib shell32.lib windowscodecs.lib
if errorlevel 1 ( echo [x] test build failed & exit /b 1 )
out\tile_test.exe
