#pragma once

#include <stdint.h>

enum class PowerKeyEvent : uint8_t {
  None = 0,
  ShortPress,
  LongPress,
};

void power_init();
void power_tick();
void power_set_display_active(bool active);
bool power_take_state_changed();
int power_battery_percent();
bool power_is_charging();
PowerKeyEvent power_take_key_event();
