@echo off
setlocal EnableDelayedExpansion
title Until Then - Thai Language Mod Installer
cd /d "%~dp0"

set "LOG=%~dp0debug.log"
> "%LOG%" echo === Until Then Thai Mod - install log ===
>>"%LOG%" echo when    : %date% %time%
>>"%LOG%" echo folder  : %~dp0
>>"%LOG%" echo os      : %OS%  %PROCESSOR_ARCHITECTURE%
>>"%LOG%" echo.

echo ================================================
echo    Until Then - Thai Language Mod  [Installer]
echo ================================================
echo.

REM ---- locate game folder (auto-detect across all Steam libraries) ----
call :step "Looking for the game..."
set "GAME="
for /f "delims=" %%G in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0detect_game.ps1" 2^>^>"%LOG%"') do set "GAME=%%G"
if not defined GAME set "GAME=C:\Program Files (x86)\Steam\steamapps\common\Until Then"
>>"%LOG%" echo game(detected): %GAME%
if not exist "%GAME%\UntilThen.pck" if not exist "%GAME%\UntilThen.pck.bak" (
  echo [X] Could not auto-detect "Until Then".
  echo     ^(Steam -^> right-click Until Then -^> Manage -^> Browse local files^)
  echo.
  set /p "GAME=Type the full path to the Until Then folder and press Enter: "
)
>>"%LOG%" echo game(final)   : !GAME!
if not exist "!GAME!\UntilThen.pck" if not exist "!GAME!\UntilThen.pck.bak" (
  call :fail "Cannot find UntilThen.pck in: !GAME!"
  goto :end
)
call :ok "Game folder: !GAME!"

REM ---- find Steam.exe (to close gracefully + reopen) ----
set "STEAMEXE="
for /f "tokens=2,*" %%A in ('reg query "HKCU\Software\Valve\Steam" /v SteamExe 2^>nul') do set "STEAMEXE=%%B"
if defined STEAMEXE set "STEAMEXE=!STEAMEXE:/=\!"
>>"%LOG%" echo steam.exe    : !STEAMEXE!
set "REOPEN="

REM ---- close Steam if running (it locks the pck) ----
tasklist /fi "imagename eq steam.exe" 2>nul | find /i "steam.exe" >nul
if not errorlevel 1 (
  set "REOPEN=1"
  if defined STEAMEXE (
    call :step "Closing Steam gracefully (will reopen when done)..."
    start "" "!STEAMEXE!" -shutdown
  ) else (
    echo [!] Please FULLY EXIT Steam ^(tray -^> Exit^), then press a key...
    pause >nul
  )
)
set /a _w=0
:waitsteam
tasklist /fi "imagename eq steam.exe" 2>nul | find /i "steam.exe" >nul
if not errorlevel 1 (
  set /a _w+=1
  if !_w! gtr 20 ( echo [!] Steam still running - exit it then press a key... & pause >nul & set /a _w=0 ) else ( ping -n 3 127.0.0.1 >nul )
  goto :waitsteam
)
>>"%LOG%" echo steam closed after !_w! checks

set "TOOL=%~dp0tools\GodotPCKExplorer.Console.exe"
set "PAYLOAD=%~dp0payload"
set "PCK=!GAME!\UntilThen.pck"
set "BAK=!GAME!\UntilThen.pck.bak"
set "TMP=!GAME!\UntilThen.thmod.tmp.pck"
if exist "!TOOL!" (>>"%LOG%" echo tool exists: YES) else (>>"%LOG%" echo tool exists: NO - antivirus may have removed it)
if exist "!PAYLOAD!" (>>"%LOG%" echo payload   : YES) else (>>"%LOG%" echo payload   : NO)
if not exist "!TOOL!"    ( call :fail "Missing pck tool (antivirus may have deleted it): !TOOL!" & goto :end )
if not exist "!PAYLOAD!" ( call :fail "Missing payload folder: !PAYLOAD!" & goto :end )

REM ---- backup original (only once) ----
if not exist "!BAK!" (
  call :step "Backing up original -> UntilThen.pck.bak"
  copy /Y "!PCK!" "!BAK!" >>"%LOG%" 2>&1
  if errorlevel 1 ( call :fail "Cannot write to the game folder (try Run as administrator)" & goto :end )
  call :ok "Backup created."
)

REM ---- build Thai pck from backup + payload ----
call :step "Building Thai version (~3 GB, ~10-30s)..."
if exist "!TMP!" del /f /q "!TMP!" >>"%LOG%" 2>&1
set "PLOG=%~dp0_pcktool.tmp"
"!TOOL!" -pc "!BAK!" "!PAYLOAD!" "!TMP!" "2.4.1.4" > "!PLOG!" 2>&1
>>"%LOG%" echo --- pck tool exit: !errorlevel! (last lines below) ---
powershell -NoProfile -Command "Get-Content -LiteralPath '!PLOG!' -Tail 12 -EA SilentlyContinue" >>"%LOG%" 2>&1
del /f /q "!PLOG!" 2>nul
if not exist "!TMP!" ( call :fail "Build failed - see debug.log (pck tool produced no output pck)" & goto :end )

REM ---- replace the live pck ----
call :step "Installing..."
copy /Y "!TMP!" "!PCK!" >>"%LOG%" 2>&1
if errorlevel 1 ( del /f /q "!TMP!" 2>nul & call :fail "Cannot replace the game file (close Steam / Run as administrator)" & goto :end )
del /f /q "!TMP!" >>"%LOG%" 2>&1

call :ok "DONE - installed!"
echo.
echo ================================================
echo    DONE!  Open the game then:
echo    Settings -^> Language:
echo       "Thai"          = polite (kid-safe)
echo       "Thai (rough)"  = slangy teen style
echo ================================================
echo To uninstall: run UNINSTALL.bat
if defined REOPEN if defined STEAMEXE ( echo. & call :step "Reopening Steam..." & start "" "!STEAMEXE!" )
goto :end

REM ================= helpers =================
:step
echo [..] %~1
>>"%LOG%" echo [..] %~1
goto :eof
:ok
echo [OK] %~1
>>"%LOG%" echo [OK] %~1
goto :eof
:fail
echo.
echo [X] %~1
echo     ^(a debug.log was saved next to this installer - send it to the author^)
>>"%LOG%" echo [X] FAIL: %~1
goto :eof

:end
>>"%LOG%" echo === finished: %date% %time% ===
echo.
echo  (debug log: "%LOG%")
echo.
pause
exit /b
