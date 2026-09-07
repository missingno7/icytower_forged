@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0lifted"
for %%F in (lifted_update_frame.c lifted_is_solid.c lifted_jump_player.c) do (
  echo === %%F
  cl /nologo /c /W3 /TC /I..\..\gen /Fo:%%~nF.obj %%F
)
