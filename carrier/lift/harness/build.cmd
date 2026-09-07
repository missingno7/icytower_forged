@echo off
rem Builds the LIFTED side of the offline equivalence check (32-bit MSVC).
rem The lifted .c files are compiled UNCHANGED; only /FIpf_harness_mem.h
rem differs from the carrier configuration (it redefines PF_MEM).
rem
rem notes/extraction_plan.md S3: pf_harness_mem.h now lives in the
rem port_forge submodule, tools/win32_oracle/ -- only the /I search path
rem changed (this project's own lift_check.c/lifted_*.c are unmoved).
setlocal
cd /d "%~dp0"
set PF_ORACLE=..\..\..\port_forge\tools\win32_oracle
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 ( echo FAILED: no MSVC x86 environment & exit /b 1 )
if not exist obj mkdir obj
cl /nologo /W3 /TC /D_CRT_SECURE_NO_WARNINGS /I..\..\gen /I. /I..\lifted /I%PF_ORACLE% ^
   /FIpf_harness_mem.h ^
   lift_check.c ..\lifted\lifted_update_frame.c ..\lifted\lifted_is_solid.c ..\lifted\lifted_jump_player.c ..\lifted\lifted_line_intersect.c ^
   /Fe:lift_check.exe /Fo:obj\
if errorlevel 1 ( echo FAILED & exit /b 1 )
echo OK: harness\lift_check.exe
endlocal
