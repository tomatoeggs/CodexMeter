#include "identity.h"

#include <Arduino.h>

#include "config.h"

static bool initialized = false;
static char device_id[32];
static char short_id[8];
static char ble_name[32];
static char json[128];

void identity_init() {
  if (initialized) return;

  uint64_t chip = ESP.getEfuseMac();
  uint32_t short_value = (uint32_t)((chip >> 24) & 0xFFFFFFULL);
  snprintf(device_id, sizeof(device_id), "codexmeter-%012llx",
           (unsigned long long)chip);
  snprintf(short_id, sizeof(short_id), "%06lX", (unsigned long)short_value);
  snprintf(ble_name, sizeof(ble_name), "%s-%s", CODEXMETER_DEVICE_NAME, short_id);
  snprintf(json, sizeof(json),
           "{\"device_id\":\"%s\",\"short_id\":\"%s\",\"name\":\"%s\"}",
           device_id, short_id, ble_name);
  initialized = true;
}

const char* identity_device_id() {
  identity_init();
  return device_id;
}

const char* identity_short_id() {
  identity_init();
  return short_id;
}

const char* identity_ble_name() {
  identity_init();
  return ble_name;
}

const char* identity_json() {
  identity_init();
  return json;
}
