@echo off
rem Builds the NATIVE side of the offline equivalence check (32-bit MSVC).
rem Same recipe as build.cmd (the LIFTED side): only /FIpf_harness_mem.h
rem differs from the real carrier build, and it is force-included here too,
rem so carrier/native/native_*.c compiles unchanged in both places.
rem
rem notes/extraction_plan.md S3: the generic wire-protocol driver
rem (native_check.c) and the shared memory-seam header (pf_harness_mem.h)
rem now live in the port_forge submodule, tools/win32_oracle/; this project
rem supplies only its own per-function dispatch
rem (icytower_harness_project_native.c). The command line below is the
rem SAME documented command (run from this directory) -- only the /I search
rem path and the source file list changed to point at the new locations.
setlocal
cd /d "%~dp0"
set PF_ORACLE=..\..\..\port_forge\tools\win32_oracle
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 ( echo FAILED: no MSVC x86 environment & exit /b 1 )
if not exist obj_native mkdir obj_native
cl /nologo /W3 /TC /D_CRT_SECURE_NO_WARNINGS /I..\..\gen /I. /I..\lifted /I..\..\native /I%PF_ORACLE% ^
   /FIpf_harness_mem.h ^
   %PF_ORACLE%\native_check.c icytower_harness_project_native.c ^
   ..\..\native\native_update_frame.c ..\..\native\native_is_solid.c ^
   /Fe:native_check.exe /Fo:obj_native\
if errorlevel 1 ( echo FAILED & exit /b 1 )
echo OK: harness\native_check.exe
endlocal
