#pragma once
#include <ArduinoJson.h>
#include "MessageTypes.h"

// Which kind of message parseMessage() decoded. Exactly one applies per
// message -- the payload fields below are only meaningful for their own type.
enum class MsgType : uint8_t {
    None,       // unrecognized / ignored (e.g. STATUS_SETTINGS, malformed)
    Movement,   // left stick -> drive (lx/ly), right stick -> head (headRx/headRy)
    Ping,       // -> pingResponse
    Trigger,    // audio-slot id -> motor_controller animation (triggerId)
    Stop,       // STOP_MACRO_ID -> stop wheels + motor_controller animation
    Buttons,    // "p" array -> buttonsMask + l1Held
};

struct ParseResult {
    MsgType type   = MsgType::None;
    int8_t  lx     = 0;      // drive X: -100 to 100
    int8_t  ly     = 0;      // drive Y: -100 to 100
    int8_t  headRx = 0;      // right stick X: -100 to 100 -- horizontal_movement rate
    int8_t  headRy = 0;      // right stick Y: -100 to 100 -- head tilt rate (both pivots together)
    char    pingResponse[32] = {};
    int16_t triggerId = 0;   // animation index (see motor_controller's GeneratedCodeAnimations.h)
    uint8_t buttonsMask = 0; // bit i set = NudgeButton(i) currently held (see MessageTypes.h)
    bool    l1Held = false;  // "l1" present in "p" -- head-control modifier, not part of buttonsMask/NudgeButton
};

class CommandParser {
public:
    CommandParser() = default;

    // Parse a JSON message from BLE and extract what grogu acts on.
    // Movement -> drive command (left stick, "l") + head rate (right
    // stick, "r", both sent in the same STATUS_MOVEMENT message); ping ->
    // a response; audio-slot id ("m") -> ESP-NOW trigger/stop for
    // motor_controller (grogu repurposes the audio button space since it
    // has no speaker -- see MessageTypes.h); gamepad face/dpad buttons
    // ("p") -> bitmask of currently-held nudge buttons, diffed against the
    // previous call by main.cpp to send per-button ESP-NOW Nudge/
    // NudgeRelease events. Settings are ignored.
    ParseResult parseMessage(const char* json, int len);

private:
    // Kept as a member so callers don't carry a JsonDocument on the stack;
    // cleared and re-filled on every parseMessage() call.
    JsonDocument doc_;
};
