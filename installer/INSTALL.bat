@echo off
setlocal EnableDelayedExpansion
title Until Then - Thai Language Mod Installer
cd /d "%~dp0"

echo ================================================
echo    Until Then - Thai Language Mod  [Installer]
echo ================================================
echo.

REM ---- locate game folder (auto-detect across all Steam libraries) ----
echo [..] Looking for the game...
set "GAME="
for /f "delims=" %%G in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0detect_game.ps1" 2^>nul') do set "GAME=%%G"
if not defined GAME set "GAME=C:\Program Files (x86)\Steam\steamapps\common\Until Then"
if not exist "%GAME%\UntilThen.pck" if not exist "%GAME%\UntilThen.pck.bak" (
  echo [X] Could not auto-detect "Until Then".
  echo     ^(Steam -^> right-click Until Then -^> Manage -^> Browse local files^)
  echo.
  set /p GAME=Type the full path to the "Until Then" folder and press Enter:
)
if not exist "%GAME%\UntilThen.pck" if not exist "%GAME%\UntilThen.pck.bak" (
  echo [X] Still cannot find UntilThen.pck. Aborting.
  pause & exit /b 1
)
echo [OK] Game folder: %GAME%
echo.

REM ---- find Steam.exe so we can close it gracefully + reopen it afterwards ----
set "STEAMEXE="
for /f "tokens=2,*" %%A in ('reg query "HKCU\Software\Valve\Steam" /v SteamExe 2^>nul') do set "STEAMEXE=%%B"
if defined STEAMEXE set "STEAMEXE=%STEAMEXE:/=\%"
set "REOPEN="

REM ---- close Steam if it is running (it locks the pck) ----
tasklist /fi "imagename eq steam.exe" 2>nul | find /i "steam.exe" >nul
if %errorlevel%==0 (
  set "REOPEN=1"
  if defined STEAMEXE (
    echo [..] Closing Steam gracefully ^(it will be reopened when done^)...
    start "" "%STEAMEXE%" -shutdown
  ) else (
    echo [!] Please FULLY EXIT Steam ^(tray icon -^> Exit^), then press any key...
    pause >nul
  )
)

REM ---- wait until Steam has fully closed (max ~40s) ----
set /a _w=0
:waitsteam
tasklist /fi "imagename eq steam.exe" 2>nul | find /i "steam.exe" >nul
if %errorlevel%==0 (
  set /a _w+=1
  if !_w! gtr 20 (
    echo [!] Steam is still running. Please exit it manually, then press any key...
    pause >nul
    set /a _w=0
  ) else (
    ping -n 3 127.0.0.1 >nul
  )
  goto waitsteam
)

set "TOOL=%~dp0tools\GodotPCKExplorer.Console.exe"
set "PAYLOAD=%~dp0payload"
set "PCK=%GAME%\UntilThen.pck"
set "BAK=%GAME%\UntilThen.pck.bak"
set "TMP=%GAME%\UntilThen.thmod.tmp.pck"
if not exist "%TOOL%" ( echo [X] Missing %TOOL% & pause & exit /b 1 )
if not exist "%PAYLOAD%" ( echo [X] Missing payload folder & pause & exit /b 1 )

REM ---- backup original (only once) ----
if not exist "%BAK%" (
  echo [..] Backing up original UntilThen.pck -^> UntilThen.pck.bak
  copy /Y "%PCK%" "%BAK%" >nul
  if errorlevel 1 (
    echo [X] Could not write to the game folder.
    echo     Right-click this installer -^> "Run as administrator" and try again.
    pause & exit /b 1
  )
  echo [OK] Backup created.
)

REM ---- build Thai pck from the ORIGINAL backup + payload ----
echo [..] Building Thai version ^(about 10-20 seconds, ~3 GB^)...
if exist "%TMP%" del /f /q "%TMP%"
"%TOOL%" -pc "%BAK%" "%PAYLOAD%" "%TMP%" "2.4.1.4"
if not exist "%TMP%" ( echo [X] Build failed. & pause & exit /b 1 )

REM ---- replace the live pck ----
echo [..] Installing...
copy /Y "%TMP%" "%PCK%" >nul
if errorlevel 1 (
  echo [X] Could not replace the game file.
  echo     Close Steam, or right-click this installer -^> "Run as administrator".
  del /f /q "%TMP%" 2>nul
  pause & exit /b 1
)
del /f /q "%TMP%"

echo.
echo ================================================
echo    DONE!  Launch Until Then
echo    Settings -^> Game -^> Language:
echo       "Thai"          = polite (kid-safe)
echo       "Thai (rough)"  = slangy teen style
echo ================================================
echo.
echo To uninstall: run UNINSTALL.bat
echo.

REM ---- reopen Steam if we closed it ----
if defined REOPEN if defined STEAMEXE (
  echo [..] Reopening Steam...
  start "" "%STEAMEXE%"
)

pause
