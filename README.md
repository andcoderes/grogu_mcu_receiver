# Grogu receiver

Firmware for grogu's BLE/wheels board, built on a **Seeed XIAO ESP32-C6**.
Advertises to the [**Droid_Phone_Controller**](https://github.com/andcoderes/Droid_Phone_Controller)
phone app over BLE and drives an **L298N** dual H-bridge (differential/tank
drive) directly.

It also brings up an **ESP-NOW link to
[`grogu_servo_controller`](https://github.com/andcoderes/grogu_servo_controller)**,
a Bottango Impulse board running grogu's custom firmware for
servos/animatronics. Grogu has no audio hardware, so the app's audio
button slot (`s:3`) is repurposed to hold animation triggers/stop instead
of real audio — see the `grogu_servo_controller` repo for that side of the
link.

> **New here?** [`SETUP.md`](SETUP.md) has the board details, wiring, and a
> quick-start. The three repos that make up the Grogu droid —
> [Droid_Phone_Controller](https://github.com/andcoderes/Droid_Phone_Controller),
> [grogu_mcu_receiver](https://github.com/andcoderes/grogu_mcu_receiver) (this repo),
> and [grogu_servo_controller](https://github.com/andcoderes/grogu_servo_controller)
> — are listed there.

This project's structure (BLE peripheral, JSON command parsing, secrets
generated from `.env`) was adapted from Chopper's `Droid-Receiver` firmware
in the `Chopper_v2` repo, copied rather than shared so the two droids can
evolve independently.

## Features

- **BLE peripheral** — advertises as "Droid Grogu" (the app's pairing picker requires the `"Droid "` prefix), receives JSON commands from the `Droid_Phone_Controller` app
- **L298N differential drive** — movement commands (`s:1`) drive two DC motors via tank-steering mixing
- **ESP-NOW trigger link** — audio-slot button presses (`s:3`) forward the id to `grogu_servo_controller` as an animation trigger, or as a stop (id `999`)
- **Dead-man's switch** — motors stop if no movement command arrives within `MOTOR_TIMEOUT_MS`, and on BLE disconnect

## Project Structure

```
src/
  main.cpp                        Entry point, BLE -> motor dispatch
  config.h                        Pins, BLE device name, timing constants (UUIDs/keys come from secrets.h)
  communication/
    BleController.h/.cpp          BLE peripheral, JSON message buffering
    CommandParser.h/.cpp          App JSON -> drive command / ping response / grogu_servo_controller trigger id
    EspNowController.h/.cpp       ESP-NOW link to grogu_servo_controller — sends button triggers, tracks link-alive via heartbeats
    MessageTypes.h                App status codes + ESP-NOW EventPacket format (shared with grogu_servo_controller)
  motor/
    MotorController.h/.cpp        L298N differential drive (IN1/IN2/EN per side)
scripts/
  load_secrets.py                 Pre-build script: .env -> include/secrets.h
.env.example                      Template for ESP-NOW keys + BLE UUIDs — copy to .env
platformio.ini
```

## Pin Mapping (Seeed XIAO ESP32-C6)

| Signal            | Pin | L298N terminal |
|--------------------|-----|-----------------|
| `PIN_MOTOR_L_IN1`  | D0  | IN1 (left)      |
| `PIN_MOTOR_L_IN2`  | D1  | IN2 (left)      |
| `PIN_MOTOR_L_EN`   | D2  | ENA (left PWM)  |
| `PIN_MOTOR_R_IN1`  | D3  | IN3 (right)     |
| `PIN_MOTOR_R_IN2`  | D4  | IN4 (right)     |
| `PIN_MOTOR_R_EN`   | D5  | ENB (right PWM) |

D6-D10 are left free for future use (status LED, buttons, sensors, etc.).
External pull-down resistors on all six motor pins are recommended, same as
Chopper's body board — these GPIOs can float/default HIGH during power-on
before `setup()` runs, which the L298N can briefly read as a drive command.

## Prerequisites

- [Visual Studio Code](https://code.visualstudio.com/) with the [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) extension
- Python 3 (used by the `load_secrets.py` build script)

## Setup

1. Wire the L298N per the pin table above, and power the motors from the
   L298N's own motor supply (not the XIAO's 3.3V/5V rail).

2. Set up `.env` — see [Generating ESP-NOW keys](#generating-esp-now-keys--ble-uuids) below.

3. Build the firmware:
   ```bash
   pio run -e seeed_xiao_esp32c6
   ```

4. Upload to the board:
   ```bash
   pio run -e seeed_xiao_esp32c6 -t upload
   ```

5. Open the serial monitor:
   ```bash
   pio device monitor -b 115200
   ```

6. In the `Droid_Phone_Controller` app, add/select Grogu and connect — it
   should drive from the movement stick immediately. Settings (`s:2`) and
   regular buttons (`s:0`) are accepted but ignored. Audio-slot buttons
   (`s:3`) forward to `grogu_servo_controller` over ESP-NOW — see the
   [`grogu_servo_controller`](https://github.com/andcoderes/grogu_servo_controller)
   repo to set that board up and pair the two boards' MAC addresses.

## Generating ESP-NOW keys + BLE UUIDs

Copy the template and fill in real values:

```bash
cp .env.example .env
```

**`PMK_KEY` / `LMK_KEY`** — two 16-byte (128-bit) keys for the ESP-NOW
event link to `grogu_servo_controller`, each as 32 hex characters:

```bash
openssl rand -hex 16   # run twice — once for PMK_KEY, once for LMK_KEY
```

These must end up byte-for-byte identical to `grogu_servo_controller`'s
`.env` copies.

**`EVENT_BOARD_MAC`** — `grogu_servo_controller`'s WiFi MAC address. Leave
as zeros until that board has been flashed and run — see its README for how
the two boards exchange MAC addresses.

**`SERVICE_UUID` / `CHARACTERISTIC_UUID`** — `.env.example` pre-fills
`SERVICE_UUID` with the value shared across every droid in the app
(confirmed identical in both Chopper's and roger_roger's own secrets) and
`CHARACTERISTIC_UUID` with a freshly generated UUID unique to Grogu.
Regenerate the characteristic UUID if it ever collides with another droid
registered in the app:

```bash
python3 -c "import uuid; print(uuid.uuid4())"
```

Both must match whatever the `Droid_Phone_Controller` app has configured
for Grogu's entry.

`scripts/load_secrets.py` runs automatically before every build (see
`platformio.ini`) and turns `.env` into `include/secrets.h`, which
`config.h` includes. Neither `.env` nor the generated `secrets.h` are
committed to git.

## Future Work

- Add a native unit-test environment (mirroring Chopper's `Droid-Receiver`) once the protocol/logic stabilizes.

## License

All rights reserved.
