@echo off
REM ==================================================================
REM  build_msvc.bat - compile with Microsoft Visual C++ (cl.exe)
REM
REM  Open "x64 Native Tools Command Prompt for VS" first (Start menu),
REM  cd into the render-from-scratch folder, then:
REM     build_msvc                  build ALL chapters
REM     build_msvc ch14_sphere      build ONE chapter
REM ==================================================================
setlocal
if not exist bin mkdir bin
set FLAGS=/nologo /std:c++17 /O2 /EHsc /W3 /Iinclude

if "%~1"=="" goto all
cl %FLAGS% chapters\%~1.cpp /Fe:bin\%~1.exe /Fo:bin\
if errorlevel 1 goto error
goto end

:all
for %%f in (chapters\*.cpp) do (
    cl %FLAGS% %%f /Fe:bin\%%~nf.exe /Fo:bin\
    if errorlevel 1 goto error
)
goto end

:error
echo *** Build FAILED ***
exit /b 1
:end
endlocal
