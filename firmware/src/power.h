#pragma once

#include <stdint.h>

enum class PowerKeyEvent : uint8_t {
  None = 0,
  ShortPress,
  LongPress,
};

void power_init();
void power_tick();
int power_battery_percent();
bool power_is_charging();
PowerKeyEvent power_take_key_event();
