#include "BleController.h"
#include "../config.h"

void BleController::setup() {
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEDevice::setMTU(BLE_MTU_SIZE);

    pServer_ = BLEDevice::createServer();
    pServer_->setCallbacks(this);

    BLEService* pService = pServer_->createService(SERVICE_UUID);
    pCharacteristic_ = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ   |
        BLECharacteristic::PROPERTY_WRITE  |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE
    );
    pCharacteristic_->setCallbacks(this);
    pService->start();

    // Security: Secure Connections + MITM + Bonding, no I/O capability.
    BLESecurity::setAuthenticationMode(true, true, true);
    BLESecurity::setCapability(ESP_IO_CAP_NONE);

    BLEAdvertising* pAdv = BLEDevice::getAdvertising();
    pAdv->addServiceUUID(SERVICE_UUID);
    pAdv->setScanResponse(true);
    pAdv->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BLE: advertising as \"" BLE_DEVICE_NAME "\"");
}

// ---- BLE callbacks (run in the NimBLE host task) ----

void BleController::onWrite(BLECharacteristic* pCharacteristic) {
    auto val = pCharacteristic->getValue();
    storeMessage(val.c_str(), val.length());
}

void BleController::onConnect(BLEServer* pServer) {
    connected_ = true;
    BLEDevice::stopAdvertising();
}

void BleController::onDisconnect(BLEServer* pServer) {
    connected_ = false;
    BLEDevice::startAdvertising();
}

// ---- Main-loop helpers ----

void BleController::loop() {
    // Drains every queued message this call, not just one, so a burst
    // (e.g. a movement update and a button press arriving close together)
    // gets fully processed in order rather than only the latest surviving.
    for (;;) {
        char local[MAX_MSG_LEN];
        int len = -1;
        portENTER_CRITICAL(&msgMux_);
        if (queueCount_ > 0) {
            QueuedMsg& slot = queue_[queueHead_];
            len = slot.len;
            memcpy(local, slot.data, len + 1);  // include null terminator
            queueHead_ = (queueHead_ + 1) % QUEUE_SIZE;
            queueCount_--;
        }
        portEXIT_CRITICAL(&msgMux_);
        if (len < 0) break;
        if (callback_) {
            callback_(local, len);
        }
    }
}

void BleController::sendNotification(const char* data) {
    if (connected_ && pCharacteristic_) {
        pCharacteristic_->setValue((uint8_t*)data, strlen(data));
        pCharacteristic_->notify();
    }
}
