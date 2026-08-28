#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLESecurity.h>

// BLE peripheral bridging the DroidController app to this board. Adapted
// from Chopper's Droid-Receiver BleController (same wire protocol/security
// setup, copied rather than shared so grogu can evolve independently).
class BleController : public BLECharacteristicCallbacks, public BLEServerCallbacks {
public:
    using MessageCallback = void (*)(const char* json, int len);

    void setup();
    void loop();
    bool isConnected() const { return connected_; }
    void sendNotification(const char* data);
    void setMessageCallback(MessageCallback cb) { callback_ = cb; }

private:
    void onWrite(BLECharacteristic* pCharacteristic) override;
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;

    // Pushes data into the next free queue slot under lock. A single-slot
    // mailbox here would silently overwrite an unprocessed message if two
    // writes arrive before loop() drains the first (e.g. a movement update
    // and a button press landing close together) -- this queue means
    // nothing is lost as long as fewer than QUEUE_SIZE messages arrive
    // between two loop() calls, which run continuously and fast.
    void storeMessage(const char* data, int len) {
        if (len <= 0 || len >= MAX_MSG_LEN) return;
        portENTER_CRITICAL(&msgMux_);
        if (queueCount_ < QUEUE_SIZE) {
            QueuedMsg& slot = queue_[queueTail_];
            memcpy(slot.data, data, len);
            slot.data[len] = '\0';
            slot.len = len;
            queueTail_ = (queueTail_ + 1) % QUEUE_SIZE;
            queueCount_++;
        } else {
            Serial.println("BLE: message queue full, dropping");
        }
        portEXIT_CRITICAL(&msgMux_);
    }

    BLEServer* pServer_ = nullptr;
    BLECharacteristic* pCharacteristic_ = nullptr;
    MessageCallback callback_ = nullptr;
    bool connected_ = false;

    static const int MAX_MSG_LEN = 256;
    static const int QUEUE_SIZE = 8;
    struct QueuedMsg {
        char data[MAX_MSG_LEN];
        int len;
    };
    QueuedMsg queue_[QUEUE_SIZE];
    volatile int queueHead_ = 0;
    volatile int queueTail_ = 0;
    volatile int queueCount_ = 0;
    // Guards the queue between onWrite() (NimBLE host task) and loop()
    // (Arduino task) — they can run on different cores.
    portMUX_TYPE msgMux_ = portMUX_INITIALIZER_UNLOCKED;
};
