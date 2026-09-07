@echo off
rem Builds the SOFTFLOAT-x87 variant of the LIFTED side (32-bit MSVC).
rem Identical to build.cmd except for -DPF_X87_SOFT, which swaps pf_x87_t from
rem `double` to the software 80-bit extended type in lifted\pf_x87_soft.h.
rem The generated .c files are byte-identical in both builds.
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 ( echo FAILED: no MSVC x86 environment & exit /b 1 )
if not exist obj_soft mkdir obj_soft
cl /nologo /W3 /TC /D_CRT_SECURE_NO_WARNINGS /DPF_X87_SOFT /I..\..\gen /I. /I..\lifted ^
   /FIpf_harness_mem.h ^
   lift_check.c ..\lifted\lifted_update_frame.c ..\lifted\lifted_is_solid.c ..\lifted\lifted_jump_player.c ..\lifted\lifted_line_intersect.c ^
   /Fe:lift_check_soft.exe /Fo:obj_soft\
if errorlevel 1 ( echo FAILED & exit /b 1 )
echo OK: harness\lift_check_soft.exe
endlocal
