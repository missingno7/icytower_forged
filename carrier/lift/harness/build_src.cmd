@echo off
rem Builds the SRC side of the offline equivalence check (32-bit MSVC).
rem Same recipe as build.cmd/build_native.cmd: src/icytower/update_frame.c
rem and .../is_solid.c compile UNCHANGED from how they compile into the real
rem carrier, except that /FIpf_harness_mem.h + /FIpf_bindings_harness.h
rem (the --mem-macro PF_MEM twin of carrier/gen/pf_bindings_src.h) redirect
rem every game global through PF_MEM() into an in-process copy of the image
rem instead of the real 0x400000 range. icytower_harness_project.c closes
rem the two remaining gaps (pointer VALUES read out of guest memory -- see
rem its header comment) by hand; src/ itself needs no change and no seam.
rem
rem notes/extraction_plan.md S3: the generic wire-protocol driver
rem (src_check.c), the rand()-pinning shim (harness_rand.c/
rem pf_harness_rand.h) and the shared memory-seam header (pf_harness_mem.h)
rem now live in the port_forge submodule, tools/win32_oracle/; this project
rem supplies only its own per-function dispatch (icytower_harness_project.c)
rem plus the Icy-specific call-trace stubs (call_trace_stubs.c,
rem pf_harness_calltrace.h) and the generated PF_MEM() bindings table
rem (pf_bindings_harness.h), all still local. The command line below is the
rem SAME documented command (run from this directory) -- only the /I search
rem path and the source file list changed to point at the new locations.
setlocal
cd /d "%~dp0"
set PF_ORACLE=..\..\..\port_forge\tools\win32_oracle
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 ( echo FAILED: no MSVC x86 environment & exit /b 1 )
if not exist obj_src mkdir obj_src
cl /nologo /W3 /TC /D_CRT_SECURE_NO_WARNINGS /I..\..\gen /I. /I..\..\..\src\icytower /I%PF_ORACLE% ^
   /FIpf_harness_mem.h /FIpf_bindings_harness.h /FIpf_harness_rand.h /FIpf_harness_calltrace.h ^
   %PF_ORACLE%\src_check.c %PF_ORACLE%\harness_rand.c icytower_harness_project.c call_trace_stubs.c ^
   ..\..\..\src\icytower\update_frame.c ..\..\..\src\icytower\is_solid.c ^
   ..\..\..\src\icytower\jump_player.c ..\..\..\src\icytower\map.c ^
   ..\..\..\src\icytower\add_combo.c ..\..\..\src\icytower\add_jump_sequence.c ^
   ..\..\..\src\icytower\line_intersect.c ^
   ..\..\..\src\icytower\control.c ..\..\..\src\icytower\particle.c ^
   ..\..\..\src\icytower\scroller.c ..\..\..\src\icytower\timer.c ^
   ..\..\..\src\icytower\main_state.c ^
   ..\..\..\src\icytower\new_rand.c ..\..\..\src\icytower\ok_to_play.c ^
   ..\..\..\src\icytower\reset_player.c ..\..\..\src\icytower\update_player.c ^
   ..\..\..\src\icytower\play_jump_sound.c ..\..\..\src\icytower\handle_player_collision_original.c ^
   ..\..\..\src\icytower\start_reward.c ^
   /Fe:src_check.exe /Fo:obj_src\
if errorlevel 1 ( echo FAILED & exit /b 1 )
echo OK: harness\src_check.exe
endlocal
