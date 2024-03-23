# HomeCtrl - Modular Smart Home Controller for Raspberry Pi Pico

## Overview

**HomeCtrl** is a modular smart home controller project for Raspberry Pi Pico (RP2040), designed to manage various subsystems in your home and garden.  
It supports multiple hardware configurations, all from a single codebase, using flexible macros and configuration files.

### Supported Features

- **Contactron (door/window sensors) monitoring**
- **Heating control (multiple zones)**
- **Temperature & humidity sensors (DHT)**
- **Alarm system**
- **Garden watering (multiple zones, rain sensor, soil moisture sensor, safety fuse)**
- **Garden light control**
- **Watchdog for reliability**
- **TCP server for integration with Fibaro Home Center 3**
- **UART logging for debugging**
- **Safety fuse: only one watering zone active at a time, auto-disable after 30 minutes**
- **Power management: disables watering power if all zones inactive for 1 minute or rain detected**

### Configurations

You can build firmware for three main setups:
- **Down (Parter):** Contactrons, heating, DHT sensors, alarm
- **Up (First Floor):** Heating, DHT sensors
- **Garden:** Watering zones, garden light, rain sensor, soil moisture sensor

All features and pin assignments are controlled by macros and config files (`config.h`).

**Note:** These are example configurations—you can freely adapt or extend them to match your own smart home needs.

---

## Getting Started

### 1. Clone the repository

```bash
git clone https://github.com/yourusername/HomeCtrl.git
cd HomeCtrl
```

### 2. Initialize submodules

This project uses the Raspberry Pi Pico SDK and FreeRTOS as submodules.  
**Run:**
```bash
git submodule update --init --recursive
```

### 3. Configure your hardware

- Edit the appropriate config file (e.g. `config/garden/config.h`, `config/down/config.h`, `config/up/config.h`) to match your hardware setup.
- Set enabled features and GPIO pin assignments using macros.

### 4. Build the firmware

Use the provided build script:
```bash
./build.sh <config_name>
```
This will configure and build the project using CMake and the Pico SDK.

### 5. Flash the firmware

- Connect your Pico in bootloader mode (hold BOOTSEL while plugging in USB).
- Copy the generated `.uf2` file from the `build` directory to the Pico's USB drive (usually `/media/$USER/RPI-RP2/`).

---

## UART Logging

To view logs for debugging:

1. **Connect a USB-UART converter:**
   - Pico TX (default: GPIO 28, configurable in `CMakeLists.txt`) → UART RX
   - Pico GND → UART GND

2. **Open a serial terminal:**
   - Baud rate: `115200`
   - Data bits: `8`
   - Parity: `None`
   - Stop bits: `1`

---

## Integration with Fibaro

- The controller communicates with Fibaro QuickApp via TCP.
- Status messages are sent in JSON format every 15–30 seconds, e.g.:
  ```json
  {"watering_power":1,"light":0,"zones":[0,0,0],"rain_status":0}
  ```
- QuickApp parses these messages and updates device states accordingly.

---

## Safety & Reliability

- **Watchdog:** Automatically restarts the board if communication is lost.
- **Zone fuse:** Only one watering zone can be active at a time; each zone is auto-disabled after 30 minutes.
- **Rain detection:** Watering is disabled during rain.
- **Power management:** Watering power is disabled if all zones are inactive for 1 minute.

---

## Dependencies

- **Raspberry Pi Pico SDK**
- **FreeRTOS**
- **CMake**

All dependencies are included as submodules.

---

## License

MIT License

---

**For questions, issues, or feature requests, open an issue on GitHub!**
