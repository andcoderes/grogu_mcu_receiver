#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include "MessageTypes.h"
#include "../config.h"

// ESP-NOW link to grogu's motor_controller board (dev/grogu/motor_controller).
class EspNowController {
public:
    void setup();
    void loop();

    // eventId = animation index to play (see GeneratedCodeAnimations.h).
    bool sendEvent(uint8_t eventId);
    bool sendStop();
    bool sendNudge(NudgeButton button, bool pressed);

    // rx/ry: -100 to 100, right stick -- horizontal/tilt rate for the head.
    // l1Held: while true, motor_controller blocks horizontal and remaps rx
    // to drive the pivots instead of ry (see HeadControl.h).
    bool sendHead(int8_t rx, int8_t ry, bool l1Held);

    bool isPeerConnected() const;

private:
    static void onDataRecv(const esp_now_recv_info_t* info,
                           const uint8_t* data, int len);
    static void onDataSent(const esp_now_send_info_t* tx_info, esp_now_send_status_t status);

    void sendHeartbeat();
    bool send(EventMsgType type, int16_t arg1 = 0, int16_t arg2 = 0, uint8_t flags = 0);

    static volatile bool peerEverRecv_;
    static unsigned long lastPeerRecv_;
    static uint32_t      txSeq_;

    unsigned long lastHeartbeatSent_ = 0;
};
