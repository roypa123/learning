@echo off
REM ==================================================================
REM  run.bat  -  build ONE chapter and run it immediately.
REM
REM  Usage:   run ch07_lexer
REM           run ch07_lexer examples\hello.peb     (arguments are passed on)
REM ==================================================================
if "%~1"=="" (
    echo Usage: run chapter_name   e.g.  run ch07_lexer
    exit /b 1
)
call build.bat %1
if errorlevel 1 exit /b 1
echo.
echo Running %1 ...
bin\%1.exe %2 %3 %4 %5 %6
