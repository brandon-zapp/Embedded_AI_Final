# Firmware setup and first bring-up

## Project baseline

The repository root is the ESP-IDF project root. `sdkconfig.defaults` selects `esp32s3`; `main/main.c` currently confirms startup over the ESP-IDF log. Keep board-independent application logic in `main/` and add hardware drivers as separate ESP-IDF components when they are ready.

## First hardware steps

1. Build the baseline firmware before connecting peripherals.
2. Connect the FireBeetle board over USB, identify its serial port, and flash the firmware.
3. Confirm the boot log in the serial monitor.
4. Bring up the GC2145 camera and LCD independently before combining them.
5. Add facial detection/recognition and lock-actuator control only after the camera and display paths are stable.

The board includes 16 MB flash, 8 MB PSRAM, a GC2145 camera connector, and a GDI display interface. The LCD model has a microSD slot. Before wiring or writing peripheral drivers, verify the actual connector pinout and display controller from the board documentation and display revision. Do not infer GPIO assignments from an unrelated ESP32 camera example.

The upstream `esp32-camera` driver lists ESP32-S3 and GC2145 support and documents ESP-IDF component-manager installation. Camera setup will be added as a managed component after the board is on hand; camera pin definitions and PSRAM settings must match this board.
