# ENSC 351 Assignment 2

Jack Hinderager (jah28@sfu.ca)

Zur Tomas (zta32@sfu.ca)

## Layout

- `hal/`: low-level hardware access (the MCP3208 ADC over SPI, and the rotary encoder through libgpiod).
- `app/`: application logic (light sampling, dip detection, terminal output, the UDP server, and timing).

## Building

The project cross-compiles with CMake, using `aarch64-linux-gnu-gcc` and the arm64 build of `libgpiod-dev`:

```
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=aarch64-toolchain.cmake && cmake --build build
```

The build copies the executable to `~/ensc351/public/myApps/light_sampler`.
Adding `-DUSE_ASAN=ON` to the first command builds with AddressSanitizer.
It's off by default, since valgrind can't run an ASan binary.

## Running

The light sensor connects to channel 2 of the MCP3208, and the rotary encoder connects to GPIO5 and GPIO6 (header pins 29 and 31).
The LED emitter is driven by the PWM on GPIO12 (header pin 32).

The PWM output has to be exported once after each boot, before the program runs:

```
sudo beagle-pwm-export --pin hat-32
```

On the target, run the program from the NFS shared folder with `sudo ./light_sampler` (SPI, GPIO, and PWM access need root).
