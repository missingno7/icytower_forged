@echo off
rem build_src_gcc.cmd -- Windows-cmd wrapper around build_src_gcc.sh (see that
rem file for the full explanation). Runs the 32-bit MinGW GCC now installed
rem at C:\msys64\mingw32\bin\gcc.exe through the MSYS2 MINGW32 environment,
rem same recipe build_src.cmd uses for MSVC, ported to GCC.
rem
rem Usage: build_src_gcc.cmd OUT.exe [extra gcc flags...]
rem   build_src_gcc.cmd gcc_check_x87_O2.exe -mfpmath=387 -O2
setlocal
set MSYSTEM=MINGW32
if "%~1"=="" (
    echo usage: build_src_gcc.cmd OUT.exe [gcc flags...]
    exit /b 2
)
set OUT=%~1
shift
set FLAGS=
:collect
if "%~1"=="" goto run
set FLAGS=%FLAGS% %1
shift
goto collect
:run
C:\msys64\usr\bin\bash.exe -lc "cd \"$(cygpath -u '%~dp0')\" && ./build_src_gcc.sh '%OUT%' %FLAGS%"
if errorlevel 1 ( echo FAILED & exit /b 1 )
endlocal
