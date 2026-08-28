#include "CommandParser.h"
#include "MessageTypes.h"
#include <string.h>

ParseResult CommandParser::parseMessage(const char* json, int len) {
    ParseResult result = {};

    doc_.clear();
    if (deserializeJson(doc_, json, len) != DeserializationError::Ok) {
        return result;
    }

    JsonVariant statusField = doc_["s"];
    if (statusField.isNull() || !statusField.is<int>()) {
        return result;  // missing/malformed status — reject rather than guess
    }
    int status = statusField.as<int>();

    switch (status) {

    case STATUS_MOVEMENT:
        // Left stick -> drive; right stick (same message) -> motor_controller's
        // head over ESP-NOW, not anything on this board.
        result.type = MsgType::Movement;
        result.lx = constrain((int)(doc_["l"][0] | 0), -100, 100);
        result.ly = constrain((int)(doc_["l"][1] | 0), -100, 100);
        result.headRx = constrain((int)(doc_["r"][0] | 0), -100, 100);
        result.headRy = constrain((int)(doc_["r"][1] | 0), -100, 100);
        break;

    case STATUS_PING:
        result.type = MsgType::Ping;
        snprintf(result.pingResponse, sizeof(result.pingResponse), "{\"s\":4,\"ok\":1}");
        break;

    case STATUS_AUDIO: {
        // grogu has no audio hardware, so the app's audio-button slot
        // (audio_grogu.json, sent here as "m": [id]) is repurposed to hold
        // motor_controller's animation triggers instead.
        JsonArray mArr = doc_["m"].as<JsonArray>();
        if (mArr.size() > 0) {
            int16_t id = mArr[0].as<int16_t>();
            if (id == STOP_MACRO_ID) {
                result.type = MsgType::Stop;
            } else {
                result.type = MsgType::Trigger;
                result.triggerId = id;
            }
        }
        break;
    }

    case STATUS_BUTTONS: {
        // Physical gamepad face/dpad buttons arrive here as named strings
        // in "p" (e.g. "y"), not the numeric macro ids the audio slot
        // uses. "p" is always the FULL set of currently-held buttons (the
        // app resends it on every press/release), so main.cpp diffs this
        // mask against the previous one to find what actually changed --
        // an empty "p" just means nothing is held right now.
        result.type = MsgType::Buttons;
        static const struct { const char* name; NudgeButton btn; } kButtons[] = {
            {"y",  NudgeButton::Y},         {"a",  NudgeButton::A},
            {"x",  NudgeButton::X},         {"b",  NudgeButton::B},
            {"du", NudgeButton::DpadUp},    {"dd", NudgeButton::DpadDown},
            {"dl", NudgeButton::DpadLeft},  {"dr", NudgeButton::DpadRight},
        };
        JsonArray pArr = doc_["p"].as<JsonArray>();
        for (JsonVariant v : pArr) {
            const char* s = v.as<const char*>();
            if (!s) continue;
            if (strcmp(s, "l1") == 0) { result.l1Held = true; continue; }
            for (const auto& e : kButtons) {
                if (strcmp(s, e.name) == 0) {
                    result.buttonsMask |= (1 << (int)e.btn);
                    break;
                }
            }
        }
        break;
    }

    default:
        // STATUS_SETTINGS: no-op for now.
        break;
    }

    return result;
}
