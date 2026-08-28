#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "communication/MessageTypes.h"
#include "communication/BleController.h"
#include "communication/CommandParser.h"
#include "communication/EspNowController.h"
#include "motor/MotorController.h"

BleController    ble;
CommandParser    parser;
EspNowController espNow;
MotorController  motors;

static bool g_bleWasConnected = false;
static uint8_t g_lastButtonsMask = 0;  // bit i = NudgeButton(i) currently held, per the last STATUS_BUTTONS message
static bool g_l1Held = false;          // head-control modifier -- tracked separately, not part of g_lastButtonsMask

// Logs an outgoing action and the event board it's headed to.
static void logToEventBoard(const char* action) {
    Serial.printf("ESP-NOW: sending %s to %02X:%02X:%02X:%02X:%02X:%02X\n",
                  action, EVENT_BOARD_MAC[0], EVENT_BOARD_MAC[1], EVENT_BOARD_MAC[2],
                  EVENT_BOARD_MAC[3], EVENT_BOARD_MAC[4], EVENT_BOARD_MAC[5]);
}

// Sends a Nudge (pressed) or NudgeRelease (!pressed) for every set bit in
// `bits` -- one ESP-NOW message per button.
static void sendNudgeBits(uint8_t bits, bool pressed) {
    for (int i = 0; i < 8; i++) {
        if (bits & (1 << i)) {
            Serial.printf("ESP-NOW: sending Nudge(%d) %s\n", i, pressed ? "press" : "release");
            espNow.sendNudge((NudgeButton)i, pressed);
        }
    }
}

// Releases every currently-held nudge button (bits in g_lastButtonsMask),
// e.g. on BLE disconnect -- a safety stop that doesn't touch a real
// animation/idle, since motor_controller only acts on buttons it actually
// tracked as pressed.
static void releaseAllButtons() {
    sendNudgeBits(g_lastButtonsMask, false);
    g_lastButtonsMask = 0;
}

static void onBleMessage(const char* json, int len) {
    Serial.printf("BLE: recv %d bytes: %s\n", len, json);

    ParseResult r = parser.parseMessage(json, len);

    switch (r.type) {
    case MsgType::Movement:
        motors.setDrive(r.lx, r.ly);
        // Right stick -> motor_controller's head. Forwarded on every movement
        // update (~100ms while the stick is off-center) -- motor_controller
        // integrates these into position and has its own timeout if updates
        // stop arriving, same as the wheels' MOTOR_TIMEOUT_MS.
        espNow.sendHead(r.headRx, r.headRy, g_l1Held);
        break;

    case MsgType::Ping:
        ble.sendNotification(r.pingResponse);
        break;

    case MsgType::Trigger:
        if (r.triggerId >= 0 && r.triggerId <= 255) {
            char what[16];
            snprintf(what, sizeof(what), "Trigger(%d)", (int)r.triggerId);
            logToEventBoard(what);
            Serial.printf("ESP-NOW: send %s\n", espNow.sendEvent((uint8_t)r.triggerId) ? "OK" : "FAILED");
        } else {
            Serial.printf("ESP-NOW: trigger id %d out of range (0-255)\n", r.triggerId);
        }
        break;

    case MsgType::Stop:
        logToEventBoard("Stop");
        Serial.printf("ESP-NOW: send %s\n", espNow.sendStop() ? "OK" : "FAILED");
        break;

    case MsgType::Buttons: {
        // "p" is the full currently-held set every time -- diff against what
        // we last sent so each button's press/release reaches motor_controller
        // as its own event, independent of any other button that's also held
        // (lets both arms move at once).
        uint8_t pressedBits  = r.buttonsMask & (uint8_t)~g_lastButtonsMask;
        uint8_t releasedBits = (uint8_t)~r.buttonsMask & g_lastButtonsMask;
        sendNudgeBits(pressedBits, true);
        sendNudgeBits(releasedBits, false);
        g_lastButtonsMask = r.buttonsMask;

        // L1 is a head-control modifier, not a joint -- tracked and forwarded
        // separately from the NudgeButton bitmask above. Resent immediately on
        // its own edge (rates 0,0) so it takes effect even if the stick isn't
        // currently being moved, instead of waiting for the next incidental
        // movement update to carry it.
        if (r.l1Held != g_l1Held) {
            g_l1Held = r.l1Held;
            Serial.printf("ESP-NOW: sending Head L1=%d\n", (int)g_l1Held);
            espNow.sendHead(0, 0, g_l1Held);
        }
        break;
    }

    case MsgType::None:
        break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    // WiFi STA mode for both BLE and ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Printed first and on its own line: this board's MAC, needed for
    // motor_controller's RECEIVER_BOARD_MAC .env value.
    Serial.printf("This board's MAC (set as motor_controller's RECEIVER_BOARD_MAC): %s\n",
                  WiFi.macAddress().c_str());

    motors.setup();

    ble.setup();
    ble.setMessageCallback(onBleMessage);
    espNow.setup();

    Serial.println("Grogu ready, firmware " FIRMWARE_VERSION);
}

void loop() {
    ble.loop();
    espNow.loop();

    bool bleNow = ble.isConnected();
    if (bleNow != g_bleWasConnected) {
        g_bleWasConnected = bleNow;
        Serial.println(bleNow ? "BLE: connected" : "BLE: disconnected");
        if (!bleNow) {
            motors.stopAll();  // safety: stop driving if the app disconnects
            releaseAllButtons();  // safety: stop any held arm nudges too
            g_l1Held = false;
            espNow.sendHead(0, 0, false);  // safety: stop the head too (motor_controller also self-times-out on its own)
        }
    }

    motors.checkTimeout();
}
