# Host unit tests

These tests compile the hardware-free components (`access_ctrl`, `face_db`,
`access_log`) with your PC's C compiler, so they run without the board.
They are separate from the ESP-IDF build: `idf.py build` ignores this folder.

## Requirements

A host C compiler plus CMake. ESP-IDF's install only ships the ESP32
cross-compiler, so on Windows install one of:

- **MSYS2 / MinGW-w64** (`pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake`)
- **WSL** (Ubuntu: `sudo apt install build-essential cmake`)
- **Visual Studio Build Tools** with "Desktop development with C++"

## Run

From the repo root:

```
cmake -S host_test -B host_test/build
cmake --build host_test/build
host_test/build/host_tests        # Windows MSVC: host_test\build\Debug\host_tests.exe
```

Expected last line: `98 checks, 0 failed`. The program exits non-zero if any
check fails, so it also works in CI.

## What is covered

- **access_ctrl**: threshold edge cases (including NaN), unlock timeout and
  extension, relock on denial, lockout after 3 failures in 60 s, failure
  window reset, fail-secure camera fault, lockout surviving a camera fault,
  and 32-bit millisecond timer wraparound.
- **face_db**: enroll/match with noisy synthetic embeddings, scale invariance,
  multi-sample averaging, error codes, full database, delete and id reuse
  rules, name truncation, and save/load round trip with corruption checks.
- **access_log**: CSV format, name sanitizing, and small-buffer handling.
