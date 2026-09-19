@echo off
REM ==================================================================
REM  build.bat  -  compile chapter programs with g++ (MinGW / WinLibs)
REM
REM  Usage (run inside the render-from-scratch folder):
REM     build                    build ALL chapters
REM     build ch14_sphere        build ONE chapter
REM
REM  The programs are written to the bin\ folder.
REM  Images are written to the images\ folder when you run them.
REM ==================================================================
setlocal
if not exist bin mkdir bin
set FLAGS=-std=c++17 -O2 -Wall -Iinclude -pthread

if "%~1"=="" goto all

echo Building %~1 ...
g++ %FLAGS% chapters\%~1.cpp -o bin\%~1.exe
if errorlevel 1 goto error
echo OK: bin\%~1.exe
goto end

:all
for %%f in (chapters\*.cpp) do (
    echo Building %%~nf ...
    g++ %FLAGS% %%f -o bin\%%~nf.exe
    if errorlevel 1 goto error
)
echo.
echo All chapters built into bin\
goto end

:error
echo.
echo *** Build FAILED. Read the first error message above. ***
exit /b 1

:end
endlocal
