# ESP32-S3 AI Door Lock

ESP-IDF firmware and supporting materials for a facial-recognition door lock built around the DFRobot FireBeetle 2 ESP32-S3 AI board with GC2145 camera.

## Firmware

The ESP-IDF project lives at the repository root. Its default target is `esp32s3` in `sdkconfig.defaults`, and the first firmware entry point is `main/main.c`.

Open this repository in VS Code with the Espressif ESP-IDF extension and select the installed ESP-IDF v5.5.6 setup. In the ESP-IDF terminal, build with:

```powershell
idf.py build
```

When the board is connected, select its serial port, then use `idf.py flash` and `idf.py monitor` (or the extension's Build, Flash and Monitor action).

## Repository layout

- `main/` — ESP-IDF application source
- `docs/` — hardware notes, pin mapping, and bring-up steps
- `prototype/` — host-side experiments and model preparation
- `sim/` — simulation assets

Hardware drivers and model dependencies will be added after confirming the board and display interfaces and pin mapping. See `docs/firmware-setup.md`.

## Hardware-free door-lock demo

While the board is in transit, run the access-control flow on your computer with Python's standard library:

```powershell
cd prototype
python doorlock_demo.py authorized
python doorlock_demo.py unknown
python doorlock_demo.py low-confidence
python doorlock_demo.py camera-error
python -m unittest -v
```

The demo uses scripted recognition outcomes, not real face recognition; its lock output is printed as simulated text. It demonstrates the policy boundary (enrolled identity plus confidence threshold), fail-secure camera errors, status messages, and timed relocking. These rules can be moved into ESP-IDF firmware and connected to the real camera/display/actuator after the hardware arrives. `prototype/requirements.txt` lists future experimentation packages; the demo does not need them.
