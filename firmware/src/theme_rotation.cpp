#include "theme_rotation.h"

#include "config.h"

void ThemeRotationPolicy::configure(
    bool enabled, uint16_t interval_minutes, uint32_t now_ms) {
  if (interval_minutes < CODEXMETER_THEME_AUTO_MIN_MINUTES) {
    interval_minutes = CODEXMETER_THEME_AUTO_MIN_MINUTES;
  }
  if (interval_minutes > CODEXMETER_THEME_AUTO_MAX_MINUTES) {
    interval_minutes = CODEXMETER_THEME_AUTO_MAX_MINUTES;
  }
  enabled_ = enabled;
  interval_minutes_ = interval_minutes;
  reset(now_ms);
}

void ThemeRotationPolicy::set_eligible(bool eligible, uint32_t now_ms) {
  advance(now_ms);
  eligible_ = eligible;
  last_tick_ms_ = now_ms;
}

void ThemeRotationPolicy::advance(uint32_t now_ms) {
  if (!initialized_) {
    initialized_ = true;
    last_tick_ms_ = now_ms;
    return;
  }

  uint32_t elapsed = now_ms - last_tick_ms_;
  last_tick_ms_ = now_ms;
  if (!enabled_ || !eligible_) return;

  uint32_t interval_ms =
      static_cast<uint32_t>(interval_minutes_) * 60UL * 1000UL;
  if (UINT32_MAX - accumulated_ms_ < elapsed) {
    accumulated_ms_ = interval_ms;
  } else {
    accumulated_ms_ += elapsed;
  }
  if (accumulated_ms_ < interval_ms) return;
  accumulated_ms_ = 0;
  pending_due_ = true;
}

bool ThemeRotationPolicy::tick(uint32_t now_ms) {
  advance(now_ms);
  if (!enabled_ || !eligible_ || !pending_due_) return false;
  pending_due_ = false;
  return true;
}

void ThemeRotationPolicy::reset(uint32_t now_ms) {
  accumulated_ms_ = 0;
  pending_due_ = false;
  last_tick_ms_ = now_ms;
  initialized_ = true;
}
