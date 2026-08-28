#pragma once
#include <Arduino.h>

// --- L298N drive motor pins ---
// Pull-down resistors recommended on all 6: these GPIOs float HIGH on
// power-up before setup(), which the L298N can briefly read as a drive command.
#define PIN_MOTOR_L_IN1  D0
#define PIN_MOTOR_L_IN2  D1
#define PIN_MOTOR_L_EN   D2   // LEDC PWM, 5kHz 8-bit -> ENA
#define PIN_MOTOR_R_IN1  D3
#define PIN_MOTOR_R_IN2  D4
#define PIN_MOTOR_R_EN   D5   // LEDC PWM, 5kHz 8-bit -> ENB

// --- BLE ---
// App's pairing picker requires the name to START with "Droid ".
#define BLE_DEVICE_NAME     "Droid Grogu"
#define BLE_MTU_SIZE        128

// --- Secrets (ESP-NOW keys, event-board MAC, BLE UUIDs) ---
// Generated from .env by scripts/load_secrets.py.
#include "secrets.h"  // PMK_KEY, LMK_KEY, EVENT_BOARD_MAC, SERVICE_UUID, CHARACTERISTIC_UUID

// --- Timing ---
#define MOTOR_TIMEOUT_MS         500   // stop driving if no movement command received
#define EVENT_LINK_TIMEOUT_MS   5000   // no ESP-NOW data from the event board = disconnected
#define HEARTBEAT_INTERVAL_MS   1000   // sent to the event board so it can detect this link dropping

// Fixed WiFi channel for the ESP-NOW link -- must match motor_controller's
// copy exactly. Neither board joins an AP, so "current channel" isn't
// reliably the same across two different chip generations by default;
// pin both to the same explicit channel instead of relying on that.
#define ESPNOW_CHANNEL 1
