@echo off
REM Build UntilThen Thai Translator GUI (Dear ImGui + DX11) with MSVC VS2022 BuildTools
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" ( echo [X] vcvars64.bat not found & exit /b 1 )
call "%VCVARS%" >nul
cd /d "%~dp0"
if not exist build mkdir build
set IMGUI=third_party\imgui
echo [..] Compiling icon resource...
rc /nologo /fo build\app.res app.rc
if errorlevel 1 ( echo [X] rc ^(icon resource^) failed & exit /b 1 )
echo [..] Compiling GUI (this takes ~30-60s the first time)...
cl /nologo /std:c++17 /EHsc /O2 /utf-8 /W3 /MD /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS ^
   /I src /I %IMGUI% /I %IMGUI%\backends /I third_party\json ^
   /Fe:build\UntilThenTranslator.exe /Fo:build\ ^
   src\app.cpp ^
   %IMGUI%\imgui.cpp %IMGUI%\imgui_draw.cpp %IMGUI%\imgui_tables.cpp %IMGUI%\imgui_widgets.cpp ^
   %IMGUI%\backends\imgui_impl_win32.cpp %IMGUI%\backends\imgui_impl_dx11.cpp ^
   build\app.res ^
   /link d3d11.lib dxgi.lib d3dcompiler.lib shell32.lib ole32.lib comdlg32.lib /SUBSYSTEM:CONSOLE
if errorlevel 1 ( echo [X] Build failed & exit /b 1 )
echo [OK] build\UntilThenTranslator.exe
