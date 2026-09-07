@echo off
rem Builds the SRC side of the offline equivalence check (32-bit MSVC).
rem Same recipe as build.cmd/build_native.cmd: src/icytower/update_frame.c
rem and .../is_solid.c compile UNCHANGED from how they compile into the real
rem carrier, except that /FIpf_harness_mem.h + /FIpf_bindings_harness.h
rem (the --mem-macro PF_MEM twin of carrier/gen/pf_bindings_src.h) redirect
rem every game global through PF_MEM() into an in-process copy of the image
rem instead of the real 0x400000 range. src_check.c closes the two
rem remaining gaps (pointer VALUES read out of guest memory -- see its
rem header comment) by hand; src/ itself needs no change and no seam.
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 ( echo FAILED: no MSVC x86 environment & exit /b 1 )
if not exist obj_src mkdir obj_src
cl /nologo /W3 /TC /D_CRT_SECURE_NO_WARNINGS /I..\..\gen /I. /I..\..\..\src\icytower ^
   /FIpf_harness_mem.h /FIpf_bindings_harness.h /FIpf_harness_rand.h ^
   src_check.c harness_rand.c ..\..\..\src\icytower\update_frame.c ..\..\..\src\icytower\is_solid.c ^
   ..\..\..\src\icytower\jump_player.c ..\..\..\src\icytower\map.c ^
   ..\..\..\src\icytower\add_combo.c ..\..\..\src\icytower\add_jump_sequence.c ^
   ..\..\..\src\icytower\line_intersect.c ^
   ..\..\..\src\icytower\control.c ..\..\..\src\icytower\particle.c ^
   ..\..\..\src\icytower\scroller.c ..\..\..\src\icytower\timer.c ^
   ..\..\..\src\icytower\main_state.c ^
   ..\..\..\src\icytower\new_rand.c ..\..\..\src\icytower\ok_to_play.c ^
   ..\..\..\src\icytower\reset_player.c ..\..\..\src\icytower\update_player.c ^
   /Fe:src_check.exe /Fo:obj_src\
if errorlevel 1 ( echo FAILED & exit /b 1 )
echo OK: harness\src_check.exe
endlocal
