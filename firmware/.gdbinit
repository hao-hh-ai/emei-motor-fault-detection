set confirm off
target extended-remote :3333
monitor reset halt
file build/emei_fault_detection.elf
thb app_main
c
echo === GDB ready. Ctrl+C to break, c to continue ===\n
