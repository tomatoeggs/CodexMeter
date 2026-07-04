#include "ble_service.h"

#include "config.h"

#include <NimBLEDevice.h>

#define SERVICE_UUID "4c41555a-4465-7669-6365-000000000001"
#define RX_CHAR_UUID "4c41555a-4465-7669-6365-000000000002"
#define TX_CHAR_UUID "4c41555a-4465-7669-6365-000000000003"
#define REQ_CHAR_UUID "4c41555a-4465-7669-6365-000000000004"
#define RX_BUF_SIZE 512

static NimBLEServer* server = nullptr;
static NimBLECharacteristic* tx_char = nullptr;
static NimBLECharacteristic* req_char = nullptr;
static char rx_buf[RX_BUF_SIZE];
static volatile bool data_ready = false;
static volatile bool should_advertise = false;
static bool connected = false;

static void start_advertising() {
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->reset();
  adv->addServiceUUID(SERVICE_UUID);
  adv->enableScanResponse(true);
  adv->setName(CODEXMETER_DEVICE_NAME);
  adv->start();
  Serial.println("BLE advertising");
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
    connected = true;
    Serial.printf("BLE connected: %s\n", info.getAddress().toString().c_str());
  }

  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
    connected = false;
    should_advertise = true;
    Serial.printf("BLE disconnected: %d\n", reason);
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* chr, NimBLEConnInfo&) override {
    std::string val = chr->getValue();
    size_t len = val.length();
    if (len >= RX_BUF_SIZE) len = RX_BUF_SIZE - 1;
    memcpy(rx_buf, val.c_str(), len);
    rx_buf[len] = '\0';
    data_ready = true;
  }
};

void ble_service_init() {
  NimBLEDevice::init(CODEXMETER_DEVICE_NAME);
  server = NimBLEDevice::createServer();
  static ServerCallbacks server_callbacks;
  server->setCallbacks(&server_callbacks);

  NimBLEService* svc = server->createService(SERVICE_UUID);
  NimBLECharacteristic* rx = svc->createCharacteristic(
      RX_CHAR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  static RxCallbacks rx_callbacks;
  rx->setCallbacks(&rx_callbacks);

  tx_char = svc->createCharacteristic(
      TX_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  req_char = svc->createCharacteristic(REQ_CHAR_UUID, NIMBLE_PROPERTY::NOTIFY);

  server->start();
  start_advertising();
}

void ble_service_tick() {
  if (should_advertise) {
    should_advertise = false;
    start_advertising();
  }
}

bool ble_service_has_data() {
  return data_ready;
}

const char* ble_service_take_data() {
  data_ready = false;
  return rx_buf;
}

void ble_service_ack() {
  if (!connected || !tx_char) return;
  tx_char->setValue("{\"ack\":true}");
  tx_char->notify();
}

void ble_service_nack() {
  if (!connected || !tx_char) return;
  tx_char->setValue("{\"err\":true}");
  tx_char->notify();
}

void ble_service_request_refresh() {
  if (!connected || !req_char) return;
  uint8_t value = 1;
  req_char->setValue(&value, 1);
  req_char->notify();
}
