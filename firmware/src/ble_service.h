#pragma once

#include <Arduino.h>

void ble_service_init();
void ble_service_tick();
bool ble_service_has_data();
const char* ble_service_take_data();
void ble_service_ack();
void ble_service_nack();
void ble_service_request_refresh();
bool ble_service_connected();
