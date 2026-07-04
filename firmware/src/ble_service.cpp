#include "ble_service.h"

#include "config.h"
#include "device_log.h"

#include <NimBLEDevice.h>

#define SERVICE_UUID "4c41555a-4465-7669-6365-000000000001"
#define RX_CHAR_UUID "4c41555a-4465-7669-6365-000000000002"
#define TX_CHAR_UUID "4c41555a-4465-7669-6365-000000000003"
#define REQ_CHAR_UUID "4c41555a-4465-7669-6365-000000000004"
#define RX_BUF_SIZE 512
#define RX_QUEUE_SIZE 6

static NimBLEServer* server = nullptr;
static NimBLECharacteristic* tx_char = nullptr;
static NimBLECharacteristic* req_char = nullptr;
static char rx_queue[RX_QUEUE_SIZE][RX_BUF_SIZE];
static char rx_current[RX_BUF_SIZE];
static uint8_t rx_head = 0;
static uint8_t rx_tail = 0;
static uint8_t rx_count = 0;
static portMUX_TYPE rx_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool should_advertise = false;
static bool connected = false;

static void start_advertising() {
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->reset();
  adv->addServiceUUID(SERVICE_UUID);
  adv->enableScanResponse(true);
  adv->setName(CODEXMETER_DEVICE_NAME);
  adv->start();
  device_logf("INFO", "BLE advertising");
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
    connected = true;
    device_logf("INFO", "BLE connected %s", info.getAddress().toString().c_str());
  }

  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
    connected = false;
    should_advertise = true;
    device_logf("WARN", "BLE disconnected reason=%d", reason);
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* chr, NimBLEConnInfo&) override {
    std::string val = chr->getValue();
    size_t len = val.length();
    if (len >= RX_BUF_SIZE) len = RX_BUF_SIZE - 1;

    bool dropped = false;
    portENTER_CRITICAL(&rx_mux);
    memcpy(rx_queue[rx_head], val.c_str(), len);
    rx_queue[rx_head][len] = '\0';
    rx_head = (rx_head + 1) % RX_QUEUE_SIZE;
    if (rx_count == RX_QUEUE_SIZE) {
      rx_tail = (rx_tail + 1) % RX_QUEUE_SIZE;
      dropped = true;
    } else {
      rx_count++;
    }
    portEXIT_CRITICAL(&rx_mux);
    if (dropped) {
      device_logf("WARN", "BLE RX queue full; dropped oldest");
    }
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
  portENTER_CRITICAL(&rx_mux);
  bool has_data = rx_count > 0;
  portEXIT_CRITICAL(&rx_mux);
  return has_data;
}

const char* ble_service_take_data() {
  portENTER_CRITICAL(&rx_mux);
  if (rx_count == 0) {
    rx_current[0] = '\0';
  } else {
    strlcpy(rx_current, rx_queue[rx_tail], sizeof(rx_current));
    rx_tail = (rx_tail + 1) % RX_QUEUE_SIZE;
    rx_count--;
  }
  portEXIT_CRITICAL(&rx_mux);
  return rx_current;
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
