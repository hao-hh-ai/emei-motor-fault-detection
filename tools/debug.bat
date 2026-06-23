@echo off
echo Starting ESP32-C3 Debug...
if not exist "d:\ESP32-C3\equipmenttest_project\build\emei_fault_detection.elf" (
    echo ERROR: ELF not found. Build first.
    pause
    exit /b 1
)
if not exist "d:\ESP32-C3\equipmenttest_project\tools\openocd-esp32\openocd-esp32\bin\openocd.exe" (
    echo ERROR: OpenOCD not found.
    pause
    exit /b 1
)

echo Step 1: Start OpenOCD
start "OCD" /min "" "d:\ESP32-C3\equipmenttest_project\tools\openocd-esp32\openocd-esp32\bin\openocd.exe" -s "d:\ESP32-C3\equipmenttest_project\tools\openocd-esp32\openocd-esp32\share\openocd\scripts" -f board/esp32c3-builtin.cfg

echo Step 2: Wait for OpenOCD...
:wait
ping -n 2 127.0.0.1 >nul
netstat -an 2>nul | find ":3333" | find "LISTENING" >nul
if errorlevel 1 goto wait
echo OpenOCD ready.

echo Step 3: Start GDB
if exist "%USERPROFILE%\.platformio\packages\toolchain-riscv32-esp\bin\riscv32-esp-elf-gdb.exe" (
    "%USERPROFILE%\.platformio\packages\toolchain-riscv32-esp\bin\riscv32-esp-elf-gdb.exe" -q -ex "set confirm off" -ex "file d:\ESP32-C3\equipmenttest_project\build\emei_fault_detection.elf" -ex "target extended-remote :3333" -ex "monitor reset halt" -ex "thb app_main" -ex "c"
) else (
    echo WARN: GDB not found
    echo OpenOCD is running on port 3333, connect manually.
)

echo.
echo Cleaning up...
taskkill /f /im openocd.exe 2>nul
pause
