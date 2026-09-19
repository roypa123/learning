@echo off
REM ==================================================================
REM  run.bat  -  build ONE chapter and run it immediately.
REM
REM  Usage:   run ch14_sphere
REM           run ch38_final_shot final      (extra words are passed on)
REM ==================================================================
if "%~1"=="" (
    echo Usage: run chapter_name   e.g.  run ch14_sphere
    exit /b 1
)
call build.bat %1
if errorlevel 1 exit /b 1
echo.
echo Running %1 ...
bin\%1.exe %2 %3 %4
