# ENSC 351 Assignment 3

Zur Tomas (zta32@sfu.ca)
Jack Hinderager (jah28@sfu.ca)

## Layout

- `hal/`: low-level hardware access (the MCP3208 ADC over SPI, the rotary encoder through libgpiod, and the joystick).
- `app/`: application logic (audio mixing, drum beats, encoder and joystick/accelerometer input, the once-per-second status line, the UDP server, and timing).

## Building

The project cross-compiles with CMake, using `aarch64-linux-gnu-gcc` and the arm64 build of `libgpiod-dev`:

```
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=aarch64-toolchain.cmake && cmake --build build
```

The build copies the executable to `~/ensc351/public/myApps/beatbox`, the drum sounds to `~/ensc351/public/myApps/beatbox-wav-files/`, and the Node.js server to `~/ensc351/public/myApps/beatbox-server-copy/`
Adding `-DUSE_ASAN=ON` to the first command builds with AddressSanitizer.
It's off by default, since valgrind can't run an ASan binary.

## Running

On the target, run `npm install` once in `~/ensc351/public/myApps/beatbox-server-copy/`.
Then start the server there with `node server.js`.
Start the beat box from `~/ensc351/public/myApps/` with `./beatbox` (it looks for its sounds in `beatbox-wav-files/`).
The web interface is on port 8088 of the target.
To stop the beat box cleanly (all threads joined and memory freed), use the web page's "Terminate Program" button or send `stop` over UDP:

```
echo stop | nc -u -w1 127.0.0.1 12345
```

Sending `help` the same way lists the other UDP commands.
