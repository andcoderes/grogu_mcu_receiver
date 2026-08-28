#include "EspNowController.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

volatile bool EspNowController::peerEverRecv_ = false;
unsigned long EspNowController::lastPeerRecv_ = 0;
uint32_t      EspNowController::txSeq_        = 0;

// ---- ESP-NOW callbacks (run in the WiFi task) ----

void EspNowController::onDataRecv(const esp_now_recv_info_t* info,
                                   const uint8_t* data, int len) {
    if (!info || !info->src_addr) return;
    if (len != sizeof(EventPacket)) {
        Serial.printf("ESP-NOW: recv %d bytes from unexpected source, ignored\n", len);
        return;
    }
    if (memcmp(info->src_addr, EVENT_BOARD_MAC, 6) != 0) {
        Serial.println("ESP-NOW: recv from unknown MAC, ignored");
        return;
    }

    peerEverRecv_ = true;
    lastPeerRecv_ = millis();

    EventPacket packet;
    memcpy(&packet, data, sizeof(packet));
    Serial.printf("ESP-NOW: recv msgType=0x%02X seq=%lu\n", (uint8_t)packet.msgType, (unsigned long)packet.seq);
}

void EspNowController::onDataSent(const esp_now_send_info_t* tx_info,
                                   esp_now_send_status_t status) {
    Serial.printf("ESP-NOW: send callback status=%s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// ---- Public API ----

void EspNowController::setup() {
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW: init FAILED");
        return;
    }

    esp_now_set_pmk(PMK_KEY);

    esp_now_peer_info_t peer = {};
    memcpy(peer.lmk, LMK_KEY, 16);
    memcpy(peer.peer_addr, EVENT_BOARD_MAC, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.encrypt = true;
    peer.ifidx   = WIFI_IF_STA;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("ESP-NOW: failed to add event-board peer");
    }

    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);

    txSeq_ = esp_random();

    Serial.printf("ESP-NOW: ready  MAC=%s  channel=%d  peer=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  WiFi.macAddress().c_str(), ESPNOW_CHANNEL,
                  EVENT_BOARD_MAC[0], EVENT_BOARD_MAC[1], EVENT_BOARD_MAC[2],
                  EVENT_BOARD_MAC[3], EVENT_BOARD_MAC[4], EVENT_BOARD_MAC[5]);
}

void EspNowController::loop() {
    unsigned long now = millis();
    if (now - lastHeartbeatSent_ >= HEARTBEAT_INTERVAL_MS) {
        sendHeartbeat();
        lastHeartbeatSent_ = now;
    }
}

// Stamps the sequence number and ships one packet to the event board. The
// public send* methods differ only in msgType and which arg fields they use.
bool EspNowController::send(EventMsgType type, int16_t arg1, int16_t arg2, uint8_t flags) {
    EventPacket packet;
    packet.msgType = type;
    packet.seq     = ++txSeq_;
    packet.arg1    = arg1;
    packet.arg2    = arg2;
    packet.flags   = flags;
    return esp_now_send(EVENT_BOARD_MAC, (const uint8_t*)&packet, sizeof(packet)) == ESP_OK;
}

void EspNowController::sendHeartbeat() {
    send(EventMsgType::Heartbeat);
}

bool EspNowController::sendEvent(uint8_t eventId) {
    return send(EventMsgType::Trigger, eventId);
}

bool EspNowController::sendStop() {
    return send(EventMsgType::Stop);
}

bool EspNowController::sendNudge(NudgeButton button, bool pressed) {
    return send(pressed ? EventMsgType::Nudge : EventMsgType::NudgeRelease, (int16_t)button);
}

bool EspNowController::sendHead(int8_t rx, int8_t ry, bool l1Held) {
    return send(EventMsgType::Head, rx, ry, l1Held ? HEAD_FLAG_L1_HELD : 0);
}

bool EspNowController::isPeerConnected() const {
    return peerEverRecv_ && (millis() - lastPeerRecv_ < EVENT_LINK_TIMEOUT_MS);
}
