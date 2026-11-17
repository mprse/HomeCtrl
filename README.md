# HomeCtrl

HomeCtrl is a Raspberry Pi Pico W firmware project for smart-home and garden automation. It combines the Pico SDK, FreeRTOS, and a small TCP command interface to control relays, sensors, safety logic, and local automation tasks from a controller board.

The project is built around a common firmware and multiple hardware profiles located under `config/`.

## Features

The firmware supports these areas, depending on the selected configuration:

- DHT22 temperature and humidity sensors
- Heating zone control
- Contactron door/window status monitoring
- Gate control
- Beeper / alarm outputs
- Garden watering with multiple zones
- Rain and soil-related safety logic
- Awning position control
- Watchdog support and reboot handling
- TCP server for command and status exchange
- Pico W Wi-Fi connectivity via the CyW43 driver

## Supported build profiles

The repo includes three configuration profiles:

- `down` — downstairs configuration with contactrons, heating, DHT, and alarm logic
- `up` — upper-level configuration with heating and DHT
- `garden` — garden configuration with watering, awning, and related outdoor automation

These are defined in:

- `config/down/`
- `config/up/`
- `config/garden/`

Each profile contains a `CMakeLists.txt` and a `config.h` that enable the relevant modules and GPIO mappings.

## Project layout

- `smart_home.c` — main firmware entry point and command processing
- `tcp_server.c` / `tcp_server.h` — TCP listener and communication layer
- `watchdog.c` / `watchdog.h` — watchdog management
- `dht.c`, `dht22.c`, `heat.c`, `watering.c`, `gate.c`, `beeper.c`, `awning.c`, `contactron.c` — subsystem implementations
- `config/` — configuration-specific hardware definitions
- `build.sh` — convenience script for selecting a config and building the firmware
- `build/` — generated build output

## Requirements

- Raspberry Pi Pico W (or Pico-compatible board with Wi-Fi support)
- CMake
- Pico SDK
- FreeRTOS kernel sources included in this repo
- GNU patch tool if you plan to apply the SMP patch helper used by `build.sh`

## Initial setup

Clone the repository and initialize its submodules:

```bash
git clone https://github.com/mprse/HomeCtrl.git
cd HomeCtrl
git submodule update --init --recursive
```

## Building

The project uses a build script that copies the selected config into the project root before configuring CMake:

```bash
./build.sh down
./build.sh up
./build.sh garden
```

This script:

1. validates the selected configuration name,
2. copies `config/<name>/CMakeLists.txt` and `config/<name>/config.h` into the root,
3. creates or reuses `build/`,
4. runs CMake and `make` for the Pico W target.

## Flashing

After a successful build, the generated UF2 file is placed in the `build/` directory. To flash it:

1. hold the BOOTSEL button on the Pico,
2. connect the board over USB,
3. copy the generated `.uf2` file to the Pico mass storage device.

The helper script also supports `--flash`, but that requires the environment variables `UF2_FILE` and `PICO_MOUNT` to be set correctly for your system.

## Hardware configuration

The actual I/O mapping is not hardcoded in the firmware; it is defined in the selected `config.h`. This makes the same firmware usable across different installations by adjusting GPIO numbers and feature flags.

Common feature flags in the build definitions are:

- `DHT_ENABLED`
- `CONT_ENABLED`
- `HEAT_ENABLED`
- `GATE_ENABLED`
- `BEEPER_ENABLED`
- `WATERING_ENABLED`
- `AWNING_ENABLED`
- `WATCHDOG_ENABLED`

## TCP / control interface

The firmware starts a TCP server and reacts to commands such as:

- `ping` → responds with `pong`
- `flash` → reboots into USB bootloader
- `reboot` → software reboot
- `heat <zone> <state>`
- `water <zone> <state>`
- `water_garden_light <state>`
- `awning up|stop|down`
- `brama`
- `alarm_off`, `alarm_arm`, `alarm_disarm`, `alarm_detect`, `alarm_run`

The project is intended to be controlled from a home automation controller or a custom client application.

## Serial logging

The firmware uses stdio over UART and prints status information to the serial console at 115200 baud. This is useful for debugging sensor activity, watchdog events, and TCP command handling.

## Notes

- The project is built for a Pico W and expects Wi-Fi credentials to be configured in the selected build definition (`WIFI_SSID` and `WIFI_PASSWORD`).
- The code uses feature guards heavily, so the same codebase can be compiled for different physical installations.
- The README and configuration files are meant to be adjusted for your exact wiring and home setup.

## License

This project is distributed under the same license terms as the repository itself unless otherwise noted in a specific file.
