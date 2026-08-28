# Grogu Receiver — Setup & Overview

The receiver is the board that lives inside the Grogu droid. It takes
commands over Bluetooth from the phone app, drives the wheels, and relays
animation triggers to the servo board over ESP-NOW.

## MCU board

**Seeed Studio XIAO ESP32-C6**

| | |
|---|---|
| SoC | ESP32-C6 (single-core RIS-V, 160 MHz) |
| Radios | BLE 5.0 + Wi-Fi 6 (2.4 GHz) — BLE for the app link, Wi-Fi MAC layer for ESP-NOW |
| Flash | 4 MB (single ~3 MB app partition via `huge_app.csv` — no OTA) |
| Framework | Arduino via PlatformIO (`board = seeed_xiao_esp32c6`) |
| Serial | 115200 baud |

Only D0–D5 are used, all for the L298N motor driver. D6–D10 are free.

| XIAO pin | Function | L298N |
|---|---|---|
| D0 / D1 | Left motor direction | IN1 / IN2 |
| D2 | Left motor PWM (LEDC 5 kHz, 8-bit) | ENA |
| D3 / D4 | Right motor direction | IN3 / IN4 |
| D5 | Right motor PWM (LEDC 5 kHz, 8-bit) | ENB |

Pull-down resistors are recommended on all six pins — they can float HIGH
during power-up before `setup()` runs, which the L298N may read as a drive
command.

## Basic setup

**Prerequisites:** VS Code + the PlatformIO IDE extension, and Python 3
(used by the pre-build secrets script).

1. **Wire the L298N** per the pin table above. Power the motors from the
   L298N's own motor supply, not the XIAO's 3.3 V / 5 V rail.

2. **Create `.env`** from the template and fill in real values:
   ```bash
   cp .env.example .env
   ```
   - `PMK_KEY` / `LMK_KEY` — two 16-byte keys as 32 hex chars each
     (`openssl rand -hex 16`), identical to the servo controller's copies.
   - `EVENT_BOARD_MAC` — the servo controller's Wi-Fi MAC; leave as zeros
     until that board is flashed.
   - `SERVICE_UUID` — shared value used by every droid in the phone app
     (pre-filled).
   - `CHARACTERISTIC_UUID` — unique to Grogu (pre-filled); must match the
     app's entry for Grogu.

   `scripts/load_secrets.py` runs automatically before each build and turns
   `.env` into `include/secrets.h`. Neither `.env` nor `secrets.h` is
   committed.

3. **Build, upload, monitor:**
   ```bash
   pio run -e seeed_xiao_esp32c6
   pio run -e seeed_xiao_esp32c6 -t upload
   pio device monitor -b 115200
   ```

4. **Connect from the phone app** — add/select Grogu and connect. The board
   advertises as `Droid Grogu` (the app's picker requires the `Droid `
   prefix). It drives from the movement stick immediately; settings and
   plain buttons are accepted but ignored; audio-slot buttons forward to
   the servo controller as animation triggers.

## Related projects

Grogu is made of three separate repos that talk to each other:

- **[Droid_Phone_Controller](https://github.com/andcoderes/Droid_Phone_Controller)**
  — the phone app. Sends JSON commands to this board over BLE (movement
  stick, buttons, settings, ping). Shared across every droid; Grogu is just
  another entry in its pairing list.

- **[grogu_servo_controller](https://github.com/andcoderes/grogu_servo_controller)**
  — the servo / animatronics board (a Bottango
  Impulse board running Grogu's custom firmware). This receiver forwards
  audio-slot button presses to it over ESP-NOW as animation triggers
  (or a stop, id `999`), since Grogu has no audio hardware. The `PMK_KEY`,
  `LMK_KEY`, and `ESPNOW_CHANNEL` must match on both sides, and each board
  needs the other's Wi-Fi MAC address.
  > Referenced in older comments and `README.md` as `motor_controller` /
  > `../motor_controller`.

Command protocol and the ESP-NOW `EventPacket` format are defined in
`src/communication/MessageTypes.h` and must stay in sync with the servo
controller.
