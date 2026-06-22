@echo off
setlocal EnableDelayedExpansion
title Until Then - Thai Mod Uninstaller

net session >nul 2>&1
if %errorlevel% neq 0 (
  powershell -NoProfile -Command "Start-Process -Verb RunAs -FilePath '%~f0'"
  exit /b
)

set "GAME=C:\Program Files (x86)\Steam\steamapps\common\Until Then"
if not exist "%GAME%\UntilThen.pck.bak" (
  set /p GAME=Type the full path to the "Until Then" folder:
)

set "PCK=%GAME%\UntilThen.pck"
set "BAK=%GAME%\UntilThen.pck.bak"

if not exist "%BAK%" (
  echo [X] No backup found. Use Steam -^> Verify integrity instead.
  pause & exit /b 1
)

set "STEAMEXE="
for /f "tokens=2,*" %%A in ('reg query "HKCU\Software\Valve\Steam" /v SteamExe 2^>nul') do set "STEAMEXE=%%B"
if defined STEAMEXE set "STEAMEXE=%STEAMEXE:/=\%"
set "REOPEN="

tasklist /fi "imagename eq steam.exe" 2>nul | find /i "steam.exe" >nul
if %errorlevel%==0 (
  set "REOPEN=1"
  if defined STEAMEXE (
    echo [..] Closing Steam gracefully ^(will reopen when done^)...
    start "" "%STEAMEXE%" -shutdown
  ) else (
    echo [!] Please FULLY EXIT Steam, then press any key...
    pause >nul
  )
)
set /a _w=0
:waitsteam
tasklist /fi "imagename eq steam.exe" 2>nul | find /i "steam.exe" >nul
if %errorlevel%==0 (
  set /a _w+=1
  if !_w! gtr 20 ( echo [!] Steam still running, exit it then press a key... & pause >nul & set /a _w=0 ) else ( ping -n 3 127.0.0.1 >nul )
  goto waitsteam
)

echo [..] Restoring original...
copy /Y "%BAK%" "%PCK%" >nul
echo [OK] Restored to English. (backup kept as UntilThen.pck.bak)
echo.
if defined REOPEN if defined STEAMEXE ( echo [..] Reopening Steam... & start "" "%STEAMEXE%" )
pause
