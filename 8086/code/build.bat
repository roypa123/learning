@echo off
rem  Usage:  build ch34\hello
if "%~1"=="" (
    echo usage: build ^<path\to\source-without-extension^>
    exit /b 1
)
nasm -f bin -I. "%~1.asm" -o "%~1.com" -l "%~1.lst"
if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)
for %%F in ("%~1.com") do echo Built %~1.com  (%%~zF bytes)
