@echo off
rem Builds the NATIVE side of the offline equivalence check (32-bit MSVC).
rem Same recipe as build.cmd (the LIFTED side): only /FIpf_harness_mem.h
rem differs from the real carrier build, and it is force-included here too,
rem so carrier/native/native_*.c compiles unchanged in both places.
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 ( echo FAILED: no MSVC x86 environment & exit /b 1 )
if not exist obj_native mkdir obj_native
cl /nologo /W3 /TC /D_CRT_SECURE_NO_WARNINGS /I..\..\gen /I. /I..\lifted /I..\..\native ^
   /FIpf_harness_mem.h ^
   native_check.c ..\..\native\native_update_frame.c ..\..\native\native_is_solid.c ^
   /Fe:native_check.exe /Fo:obj_native\
if errorlevel 1 ( echo FAILED & exit /b 1 )
echo OK: harness\native_check.exe
endlocal
