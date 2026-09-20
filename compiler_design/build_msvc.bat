@echo off
REM ==================================================================
REM  build_msvc.bat  -  compile chapter programs with Visual Studio
REM
REM  Open a "x64 Native Tools Command Prompt for VS" first, then:
REM     build_msvc                 build ALL chapters
REM     build_msvc ch07_lexer      build ONE chapter
REM ==================================================================
setlocal
if not exist bin mkdir bin
set FLAGS=/nologo /std:c++17 /O2 /EHsc /W3 /Iinclude /utf-8

if "%~1"=="" goto all

echo Building %~1 ...
cl %FLAGS% chapters\%~1.cpp /Fe:bin\%~1.exe /Fo:bin\ >nul
if errorlevel 1 goto error
echo OK: bin\%~1.exe
goto end

:all
for %%f in (chapters\*.cpp) do (
    echo Building %%~nf ...
    cl %FLAGS% %%f /Fe:bin\%%~nf.exe /Fo:bin\ >nul
    if errorlevel 1 goto error
)
echo.
echo All chapters built into bin\
goto end

:error
echo.
echo *** Build FAILED. ***
exit /b 1

:end
endlocal
