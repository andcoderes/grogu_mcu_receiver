#pragma once
#include <Arduino.h>

// ========== App (BLE) status codes ==========
// Shared DroidController app protocol. Grogu acts on movement, ping, and
// the audio slot (repurposed for motor_controller triggers, see below).
#define STATUS_BUTTONS   0
#define STATUS_MOVEMENT  1
#define STATUS_SETTINGS  2
#define STATUS_AUDIO     3
#define STATUS_PING      4

// ========== Grogu receiver -> motor_controller (ESP-NOW) ==========
// Must stay byte-for-byte identical to motor_controller's copy.
#define EVENTNOW_MAGIC 0xE7

enum class EventMsgType : uint8_t {
    Heartbeat    = 0x10,   // keepalive, sent periodically by both boards
    Trigger      = 0x20,   // arg1 = animation index to play on motor_controller
    Stop         = 0x30,   // no arg -- stop whatever animation is currently playing
    Nudge        = 0x40,   // arg1 = NudgeButton -- start moving that joint
    NudgeRelease = 0x41,   // arg1 = NudgeButton -- stop moving that joint
    Head         = 0x50,   // arg1 = horizontal rate, arg2 = tilt rate, both -100..100, flags bit0 = L1 held (see motor_controller's HeadControl.h)
};

// Head packet flag bits (EventPacket.flags).
#define HEAD_FLAG_L1_HELD 0x01

// Gamepad buttons (app's "p" array under STATUS_BUTTONS, s:0) that each
// drive a single joint continuously while held, instead of playing an
// animation. main.cpp diffs the app's "currently pressed" set against the
// previous one and sends a Nudge/NudgeRelease per button that changed --
// multiple can be held/active at once, so both arms can move together.
enum class NudgeButton : int16_t {
    Y         = 0,  // right shoulder, up
    A         = 1,  // right shoulder, down
    X         = 2,  // right elbow, in
    B         = 3,  // right elbow, out
    DpadUp    = 4,  // left shoulder, up
    DpadDown  = 5,  // left shoulder, down
    DpadLeft  = 6,  // left elbow, in
    DpadRight = 7,  // left elbow, out
};

struct __attribute__((packed)) EventPacket {
    uint8_t      magic   = EVENTNOW_MAGIC;
    EventMsgType msgType = EventMsgType::Heartbeat;
    uint32_t     seq     = 0;   // monotonic per-sender counter, replay guard
    int16_t      arg1    = 0;   // Trigger: animation index. Head: horizontal rate. Heartbeat: unused.
    int16_t      arg2    = 0;   // Head: tilt rate. Unused by every other message type.
    uint8_t      flags   = 0;   // Head: HEAD_FLAG_* bits. Unused by every other message type.
};

// Animation index key (see motor_controller's GeneratedCodeAnimations.cpp,
// re-check after every Studio re-export): 0=idle annimation, 1=No, 2=yes,
// 3=the force, 4=grab me, 5=eating, 6="6 7" (unnamed placeholder).

// Reserved id for the app's STOP button (audio_grogu.json), matching the
// ecosystem-wide "999 = Stop" convention (see rogerroger's macro json).
#define STOP_MACRO_ID 999
