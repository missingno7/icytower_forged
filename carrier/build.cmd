@echo off
rem build.cmd - one command builds carrier.exe (x86, debug: /Zi /Od).
rem Regenerate gen\*.inc / gen\import_stubs.cpp first if imports.json changed:
rem   python gen\gen_imports.py ..\imports.json gen
setlocal
cd /d "%~dp0"

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 (
  echo FAILED: could not initialize MSVC x86 build environment
  exit /b 1
)

if not exist obj mkdir obj

cl /nologo /Zi /Od /EHsc /W3 /D_CRT_SECURE_NO_WARNINGS ^
  src\main.cpp src\pe_image.cpp src\imports.cpp src\trace.cpp src\wrappers.cpp src\symbols.cpp src\det.cpp gen\import_stubs.cpp ^
  /Fe:carrier.exe /Fo:obj\ ^
  /link /DYNAMICBASE:NO /FIXED /BASE:0x10000000 /LARGEADDRESSAWARE:NO /SUBSYSTEM:CONSOLE /DEBUG kernel32.lib user32.lib psapi.lib

if errorlevel 1 (
  echo FAILED: build errors above
  exit /b 1
)
echo OK: carrier\carrier.exe built
endlocal
