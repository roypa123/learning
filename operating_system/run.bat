@echo off
REM ==================================================================
REM  run.bat  --  boot one of our operating systems in QEMU
REM
REM     run              boot Nimbus
REM     run nimbus       boot Nimbus
REM     run spark        boot Spark from the floppy image
REM     run nimbus debug boot Nimbus stopped, waiting for GDB on :1234
REM     run nimbus trace boot Nimbus logging every interrupt to bin\qemu.log
REM
REM  Explained in: docs\01-setup.md and docs\47-debugging.md
REM ==================================================================
setlocal

set QEMU=qemu-system-i386

where %QEMU% >nul 2>nul
if errorlevel 1 (
  echo qemu-system-i386 is not on your PATH.
  echo See docs\01-setup.md -- on Windows the installer does not add it,
  echo so you usually need:  set PATH=%%PATH%%;C:\Program Files\qemu
  exit /b 1
)

set TARGET=%~1
if "%TARGET%"=="" set TARGET=nimbus

if /I "%TARGET%"=="spark"  goto spark
if /I "%TARGET%"=="nimbus" goto nimbus
echo Unknown target "%TARGET%". Try: run spark, run nimbus
exit /b 1

REM ------------------------------------------------------------------
:spark
if not exist bin\spark.img (
  echo bin\spark.img does not exist. Run:  build spark
  exit /b 1
)

REM -fda     attach the image as the first floppy
REM -boot a  boot from it
REM
REM The image is exactly 1474560 bytes, which is what makes QEMU present it
REM with the 80/2/18 geometry that boot.asm's CHS arithmetic assumes.
%QEMU% -m 32M -no-reboot -fda bin\spark.img -boot a
goto :eof

REM ------------------------------------------------------------------
:nimbus
if not exist bin\nimbus.elf (
  echo bin\nimbus.elf does not exist. Run:  build nimbus
  exit /b 1
)

REM -kernel        QEMU's own multiboot loader reads the ELF and jumps to
REM                _start with EAX = 0x2BADB002. No bootloader involved.
REM -initrd        becomes multiboot module 0, which fs\initrd.c mounts.
REM -serial stdio  the kernel log lands in this console window. This is the
REM                single most useful flag in the file.
REM -no-reboot     a triple fault stops the machine instead of rebooting it
REM                forever, so the last thing on screen stays on screen.
set FLAGS=-m 128M -serial stdio -no-reboot -no-shutdown
set IMAGES=-kernel bin\nimbus.elf -initrd bin\initrd.tar

if exist bin\disk.img set IMAGES=%IMAGES% -drive file=bin\disk.img,format=raw

if /I "%~2"=="debug" (
  echo.
  echo QEMU is stopped before the first instruction, listening on :1234.
  echo In another terminal:
  echo     i686-elf-gdb bin\nimbus.elf -ex "target remote :1234" -ex "break kmain" -ex continue
  echo.
  %QEMU% %FLAGS% %IMAGES% -S -s
  goto :eof
)

if /I "%~2"=="trace" (
  echo Logging every interrupt to bin\qemu.log ...
  %QEMU% %FLAGS% %IMAGES% -d int,cpu_reset -D bin\qemu.log
  goto :eof
)

%QEMU% %FLAGS% %IMAGES%
