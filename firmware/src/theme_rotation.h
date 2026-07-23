#pragma once

#include <Arduino.h>

class ThemeRotationPolicy {
 public:
  void configure(bool enabled, uint16_t interval_minutes, uint32_t now_ms);
  void set_eligible(bool eligible, uint32_t now_ms);
  bool tick(uint32_t now_ms);
  void reset(uint32_t now_ms);

 bool enabled() const { return enabled_; }
  uint16_t interval_minutes() const { return interval_minutes_; }

 private:
  void advance(uint32_t now_ms);

  bool enabled_ = false;
  bool eligible_ = false;
  bool initialized_ = false;
  bool pending_due_ = false;
  uint16_t interval_minutes_ = 10;
  uint32_t last_tick_ms_ = 0;
  uint32_t accumulated_ms_ = 0;
};
