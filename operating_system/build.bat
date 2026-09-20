@echo off
REM ==================================================================
REM  build.bat  --  build Spark and Nimbus on Windows, without make
REM
REM  Usage (run inside the operating_system folder):
REM     build              build everything
REM     build spark        just the basic OS
REM     build nimbus       just the full OS + userland + initrd
REM     build check        report which tools are missing
REM
REM  Needs on PATH: nasm, i686-elf-gcc, i686-elf-ld, i686-elf-objcopy.
REM  docs\01-setup.md explains how to get all four on Windows.
REM ==================================================================
setlocal EnableDelayedExpansion

set CC=i686-elf-gcc
set LD=i686-elf-ld
set OBJCOPY=i686-elf-objcopy
set READELF=i686-elf-readelf
set NASM=nasm

set CFLAGS=-std=gnu11 -m32 -ffreestanding -nostdlib -fno-builtin ^
 -fno-stack-protector -fno-pic -fno-omit-frame-pointer ^
 -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mno-red-zone ^
 -Wall -Wextra -Wno-unused-parameter -O2 -g -Inimbus\include

set ASFLAGS=-f elf32 -g -F dwarf

if /I "%~1"=="check"  goto check
if /I "%~1"=="spark"  goto spark
if /I "%~1"=="nimbus" goto nimbus
if "%~1"==""          goto all
echo Unknown target "%~1". Try: build, build spark, build nimbus, build check
exit /b 1

REM ------------------------------------------------------------------
:check
echo Checking the toolchain...
echo.
call :probe %NASM% "NASM assembler"
call :probe %CC% "i686-elf cross compiler"
call :probe %LD% "i686-elf linker"
call :probe %OBJCOPY% "i686-elf objcopy"
call :probe qemu-system-i386 "QEMU PC emulator"
echo.
echo Anything marked MISSING is explained in docs\01-setup.md
goto :eof

:probe
where %~1 >nul 2>nul
if errorlevel 1 (echo   MISSING   %~2 ^(%~1^)) else (echo   ok        %~2)
goto :eof

REM ------------------------------------------------------------------
:all
call :build_spark
if errorlevel 1 exit /b 1
call :build_nimbus
if errorlevel 1 exit /b 1
echo.
echo Everything built. Try:  run nimbus
goto :eof

:spark
call :build_spark
goto :eof

:nimbus
call :build_nimbus
goto :eof

REM ==================================================================
REM  Spark
REM ==================================================================
:build_spark
echo.
echo === Spark ========================================================
if not exist bin\spark mkdir bin\spark

echo   assembling boot sector
%NASM% -f bin spark\boot\boot.asm -o bin\spark\boot.bin       || goto fail
echo   assembling stage 2
%NASM% -f bin spark\boot\stage2.asm -o bin\spark\stage2.bin   || goto fail

echo   assembling kernel entry
%NASM% %ASFLAGS% spark\kernel\entry.asm -o bin\spark\entry.o  || goto fail
echo   compiling kernel
%CC% %CFLAGS% -c spark\kernel\kernel.c -o bin\spark\kernel.o  || goto fail

echo   linking
%LD% -m elf_i386 -T spark\link.ld -o bin\spark\kernel.elf ^
     bin\spark\entry.o bin\spark\kernel.o                     || goto fail
%OBJCOPY% -O binary bin\spark\kernel.elf bin\spark\kernel.bin || goto fail

echo   building the floppy image
powershell -NoProfile -ExecutionPolicy Bypass -File tools\mkimage.ps1 ^
  -Out bin\spark.img -Size 1474560 ^
  -Parts bin\spark\boot.bin:0,bin\spark\stage2.bin:1,bin\spark\kernel.bin:5 || goto fail

echo   -^> bin\spark.img
goto :eof

REM ==================================================================
REM  Nimbus
REM ==================================================================
:build_nimbus
echo.
echo === Nimbus =======================================================
if not exist bin\nimbus mkdir bin\nimbus
if not exist bin\user   mkdir bin\user

set KOBJ=

echo   assembling
for %%f in (boot isr cpu) do (
  %NASM% %ASFLAGS% nimbus\boot\%%f.asm -o bin\nimbus\%%f.o || goto fail
  set KOBJ=!KOBJ! bin\nimbus\%%f.o
)

echo   compiling kernel
for %%f in (main printk gdt idt isr irq timer console task sched sync syscall elf) do (
  %CC% %CFLAGS% -DNIMBUS_KERNEL -c nimbus\kernel\%%f.c -o bin\nimbus\k_%%f.o || goto fail
  set KOBJ=!KOBJ! bin\nimbus\k_%%f.o
)

echo   compiling memory manager
for %%f in (pmm paging heap) do (
  %CC% %CFLAGS% -DNIMBUS_KERNEL -c nimbus\mm\%%f.c -o bin\nimbus\m_%%f.o || goto fail
  set KOBJ=!KOBJ! bin\nimbus\m_%%f.o
)

echo   compiling drivers
for %%f in (vga serial keyboard ata) do (
  %CC% %CFLAGS% -DNIMBUS_KERNEL -c nimbus\drivers\%%f.c -o bin\nimbus\d_%%f.o || goto fail
  set KOBJ=!KOBJ! bin\nimbus\d_%%f.o
)

echo   compiling filesystems
for %%f in (vfs fd initrd fat16 pipe) do (
  %CC% %CFLAGS% -DNIMBUS_KERNEL -c nimbus\fs\%%f.c -o bin\nimbus\f_%%f.o || goto fail
  set KOBJ=!KOBJ! bin\nimbus\f_%%f.o
)

echo   compiling shared library
for %%f in (string printf) do (
  %CC% %CFLAGS% -DNIMBUS_KERNEL -c nimbus\lib\%%f.c -o bin\nimbus\l_%%f.o || goto fail
  set KOBJ=!KOBJ! bin\nimbus\l_%%f.o
)

echo   linking kernel
%LD% -m elf_i386 -T nimbus\link.ld -o bin\nimbus.elf !KOBJ! || goto fail
echo   -^> bin\nimbus.elf

REM ---- userland ----------------------------------------------------
echo.
echo   building userland
%NASM% %ASFLAGS% nimbus\user\crt0.asm -o bin\user\crt0.o     || goto fail
%CC% %CFLAGS% -c nimbus\user\libc.c   -o bin\user\libc.o     || goto fail
%CC% %CFLAGS% -c nimbus\lib\string.c  -o bin\user\string.o   || goto fail
%CC% %CFLAGS% -c nimbus\lib\printf.c  -o bin\user\printf.o   || goto fail

set ULIB=bin\user\crt0.o bin\user\libc.o bin\user\string.o bin\user\printf.o

echo     sh
%CC% %CFLAGS% -c nimbus\user\sh.c -o bin\user\sh.o           || goto fail
%LD% -m elf_i386 -T nimbus\user\user.ld -o bin\user\sh %ULIB% bin\user\sh.o || goto fail

for %%p in (ls cat echo hexdump sleep true false forktest) do (
  echo     %%p
  call :upper %%p
  %CC% %CFLAGS% -DBUILD_!UP! -c nimbus\user\coreutils.c -o bin\user\%%p.o || goto fail
  %LD% -m elf_i386 -T nimbus\user\user.ld -o bin\user\%%p %ULIB% bin\user\%%p.o || goto fail
)

echo   building the initrd
powershell -NoProfile -ExecutionPolicy Bypass -File tools\mkinitrd.ps1 ^
  -Out bin\initrd.tar ^
  -Files bin\user\sh,bin\user\ls,bin\user\cat,bin\user\echo,bin\user\hexdump,bin\user\sleep,bin\user\true,bin\user\false,bin\user\forktest || goto fail

echo   -^> bin\initrd.tar
goto :eof

:upper
set UP=%1
for %%a in (a b c d e f g h i j k l m n o p q r s t u v w x y z) do call set UP=%%UP:%%a=%%a%%
REM Batch has no uppercase function; the BUILD_ macros are spelled in caps in
REM coreutils.c, so map the handful of names we actually use.
if "%1"=="ls"       set UP=LS
if "%1"=="cat"      set UP=CAT
if "%1"=="echo"     set UP=ECHO
if "%1"=="hexdump"  set UP=HEXDUMP
if "%1"=="sleep"    set UP=SLEEP
if "%1"=="true"     set UP=TRUE
if "%1"=="false"    set UP=FALSE
if "%1"=="forktest" set UP=FORKTEST
goto :eof

REM ------------------------------------------------------------------
:fail
echo.
echo *** BUILD FAILED. Read the FIRST error above, not the last. ***
echo *** If it says a command is not recognised, run: build check ***
exit /b 1
