# ENSC 351 Assignment 3

Zur Tomas (zta32@sfu.ca)

## Layout

- `hal/`: low-level hardware access (the MCP3208 ADC over SPI, the rotary encoder through libgpiod, and the joystick).
- `app/`: application logic (audio mixing, light sampling, dip detection, terminal output, the UDP server, and timing).

## Building

The project cross-compiles with CMake, using `aarch64-linux-gnu-gcc` and the arm64 build of `libgpiod-dev`:

```
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=aarch64-toolchain.cmake && cmake --build build
```

The build copies the executable to `~/ensc351/public/myApps/assignment_3`.
Adding `-DUSE_ASAN=ON` to the first command builds with AddressSanitizer.
It's off by default, since valgrind can't run an ASan binary.

## Running



The PWM output has to be exported once after each boot, before the program runs:

```
sudo beagle-pwm-export --pin hat-32
```

On the target, run the program from the NFS shared folder with `sudo ./assignment_3` (SPI, GPIO, and PWM access need root).
