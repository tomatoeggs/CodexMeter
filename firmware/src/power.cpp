#include "power.h"

#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>

#include "config.h"
#include "device_log.h"

static XPowersPMU pmu;
static bool pmu_ready = false;
static bool display_active = true;
static int cached_pct = -1;
static bool cached_charging = false;
static PowerKeyEvent pending_key_event = PowerKeyEvent::None;
static uint32_t last_battery_ms = 0;
static uint32_t last_charging_ms = 0;
static uint32_t last_pwr_ms = 0;
static uint32_t suppress_short_until_ms = 0;
static bool suppress_short_press = false;
static bool state_changed = false;

static uint32_t power_key_poll_interval_ms() {
  return display_active ? CODEXMETER_POWER_KEY_POLL_ACTIVE_MS
                        : CODEXMETER_POWER_KEY_POLL_INACTIVE_MS;
}

static uint32_t charging_poll_interval_ms() {
  return display_active ? CODEXMETER_CHARGING_POLL_ACTIVE_MS
                        : CODEXMETER_CHARGING_POLL_INACTIVE_MS;
}

static uint32_t battery_poll_interval_ms() {
  return display_active ? CODEXMETER_BATTERY_POLL_ACTIVE_MS
                        : CODEXMETER_BATTERY_POLL_INACTIVE_MS;
}

static void refresh_cached_battery(uint32_t now) {
  const int pct = pmu.getBatteryPercent();
  if (pct != cached_pct) {
    cached_pct = pct;
    state_changed = true;
  }
  last_battery_ms = now;
}

static void refresh_cached_charging(uint32_t now) {
  const bool charging = pmu.isCharging();
  if (charging != cached_charging) {
    cached_charging = charging;
    state_changed = true;
  }
  last_charging_ms = now;
}

static void refresh_cached_state(uint32_t now) {
  refresh_cached_battery(now);
  refresh_cached_charging(now);
}

void power_init() {
  pmu_ready = pmu.begin(Wire, CODEXMETER_AXP2101_ADDR, CODEXMETER_I2C_SDA,
                        CODEXMETER_I2C_SCL);
  if (!pmu_ready) {
    device_logf("ERROR", "AXP2101 init failed");
    return;
  }

  pmu.enableBattDetection();
  pmu.enableBattVoltageMeasure();
  pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  pmu.clearIrqStatus();
  pmu.enableIRQ(
      XPOWERS_AXP2101_PKEY_SHORT_IRQ |
      XPOWERS_AXP2101_PKEY_LONG_IRQ |
      XPOWERS_AXP2101_VBUS_REMOVE_IRQ |
      XPOWERS_AXP2101_VBUS_INSERT_IRQ |
      XPOWERS_AXP2101_BAT_CHG_START_IRQ |
      XPOWERS_AXP2101_BAT_CHG_DONE_IRQ);
  pmu.setPowerKeyPressOnTime(XPOWERS_POWERON_2S);
  pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_8S);

  refresh_cached_state(millis());
  device_logf("INFO", "AXP2101 ready battery=%d charging=%d", cached_pct, cached_charging);
}

void power_tick() {
  if (!pmu_ready) return;

  uint32_t now = millis();
  const uint32_t charging_interval = charging_poll_interval_ms();
  if (charging_interval > 0 &&
      now - last_charging_ms >= charging_interval) {
    refresh_cached_charging(now);
  }
  const uint32_t battery_interval = battery_poll_interval_ms();
  if (battery_interval > 0 &&
      now - last_battery_ms >= battery_interval) {
    refresh_cached_battery(now);
  }
  if (now - last_pwr_ms >= power_key_poll_interval_ms()) {
    last_pwr_ms = now;
    pmu.getIrqStatus();
    bool long_press = pmu.isPekeyLongPressIrq();
    bool short_press = pmu.isPekeyShortPressIrq();
    bool power_state_event =
        pmu.isVbusInsertIrq() || pmu.isVbusRemoveIrq() ||
        pmu.isBatChargeStartIrq() || pmu.isBatChargeDoneIrq();
    if (long_press) {
      pending_key_event = PowerKeyEvent::LongPress;
      suppress_short_until_ms = now + 1000;
      suppress_short_press = true;
      device_logf("INFO", "power key long press");
    } else {
      if (suppress_short_press &&
          static_cast<int32_t>(now - suppress_short_until_ms) >= 0) {
        suppress_short_press = false;
      }
    }
    if (short_press && !long_press && !suppress_short_press) {
      pending_key_event = PowerKeyEvent::ShortPress;
      device_logf("INFO", "power key short press");
    }
    if (power_state_event) {
      refresh_cached_state(now);
      device_logf("INFO", "PMU state event battery=%d charging=%d", cached_pct,
                  cached_charging ? 1 : 0);
    }
    pmu.clearIrqStatus();
  }
}

void power_set_display_active(bool active) {
  if (display_active == active) return;

  display_active = active;
  if (pmu_ready && display_active) {
    refresh_cached_state(millis());
  }
  device_logf(
      "INFO",
      "PMU display_active=%d key_poll_ms=%lu battery_poll_ms=%lu charging_poll_ms=%lu",
      display_active ? 1 : 0,
      static_cast<unsigned long>(power_key_poll_interval_ms()),
      static_cast<unsigned long>(battery_poll_interval_ms()),
      static_cast<unsigned long>(charging_poll_interval_ms()));
}

bool power_take_state_changed() {
  const bool changed = state_changed;
  state_changed = false;
  return changed;
}

int power_battery_percent() {
  return cached_pct;
}

bool power_is_charging() {
  return cached_charging;
}

PowerKeyEvent power_take_key_event() {
  PowerKeyEvent event = pending_key_event;
  pending_key_event = PowerKeyEvent::None;
  return event;
}
