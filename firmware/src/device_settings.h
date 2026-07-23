#pragma once

#include <Arduino.h>

struct DeviceSettings {
  char theme_id[24] = "classic";
  bool auto_theme_enabled = false;
  uint16_t auto_theme_interval_minutes = 10;
  uint8_t brightness_percent = 60;
  uint8_t volume_percent = 50;
};

void device_settings_init();
const DeviceSettings& device_settings_get();
void device_settings_set_theme(const char* theme_id);
void device_settings_set_auto_theme(bool enabled);
void device_settings_set_auto_theme_interval(uint16_t minutes);
void device_settings_set_brightness(uint8_t percent);
void device_settings_set_volume(uint8_t percent);
void device_settings_tick();
void device_settings_flush();
