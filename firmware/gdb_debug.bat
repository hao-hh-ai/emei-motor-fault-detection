@echo off
setlocal
echo ========================================
echo  ESP32-C3 GDB Debug (USB JTAG)
echo ========================================
echo.

set "OPENOCD=d:\ESP32-C3\equipmenttest_project\tools\openocd-esp32\openocd-esp32\bin\openocd.exe"
set "SCRIPTS=d:\ESP32-C3\equipmenttest_project\tools\openocd-esp32\openocd-esp32\share\openocd\scripts"
set "ELF=d:\ESP32-C3\equipmenttest_project\build\emei_fault_detection.elf"
set "GDB=%USERPROFILE%\.platformio\packages\toolchain-riscv32-esp\bin\riscv32-esp-elf-gdb.exe"

if not exist "%ELF%" (
    echo [ERR] ELF not found. Build first in ESP-IDF.
    pause & exit /b 1
)

echo [1/3] Starting OpenOCD...
start "OpenOCD" /min "" "%OPENOCD%" -s "%SCRIPTS%" -f board/esp32c3-builtin.cfg

echo [2/3] Waiting for OpenOCD (port 3333)...
:wait
ping -n 2 127.0.0.1 >nul
netstat -an 2>nul | find ":3333" | find "LISTENING" >nul
if errorlevel 1 goto wait
echo OpenOCD ready.

echo [3/3] Starting GDB...
if exist "%GDB%" (
    "%GDB%" -q -ex "set confirm off" -ex "file %ELF%" -ex "target extended-remote :3333" -ex "monitor reset halt" -ex "thb app_main" -ex "c"
) else (
    echo [WARN] GDB not found: %GDB%
    echo Connect manually: riscv32-esp-elf-gdb build/emei_fault_detection.elf
)

echo.
echo Debug session ended.
taskkill /f /im openocd.exe 2>nul
pause
