#include "power.h"

#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>

#include "config.h"
#include "device_log.h"

static XPowersPMU pmu;
static bool pmu_ready = false;
static int cached_pct = -1;
static bool cached_charging = false;
static PowerKeyEvent pending_key_event = PowerKeyEvent::None;
static uint32_t last_battery_ms = 0;
static uint32_t last_charging_ms = 0;
static uint32_t last_pwr_ms = 0;
static uint32_t suppress_short_until_ms = 0;
static bool suppress_short_press = false;

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
      XPOWERS_AXP2101_PKEY_LONG_IRQ);
  pmu.setPowerKeyPressOnTime(XPOWERS_POWERON_2S);
  pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_8S);

  cached_pct = pmu.getBatteryPercent();
  cached_charging = pmu.isCharging();
  device_logf("INFO", "AXP2101 ready battery=%d charging=%d", cached_pct, cached_charging);
}

void power_tick() {
  if (!pmu_ready) return;

  uint32_t now = millis();
  if (now - last_charging_ms >= 500) {
    last_charging_ms = now;
    cached_charging = pmu.isCharging();
  }
  if (now - last_battery_ms >= 2000) {
    last_battery_ms = now;
    cached_pct = pmu.getBatteryPercent();
  }
  if (now - last_pwr_ms >= 50) {
    last_pwr_ms = now;
    pmu.getIrqStatus();
    bool long_press = pmu.isPekeyLongPressIrq();
    bool short_press = pmu.isPekeyShortPressIrq();
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
    pmu.clearIrqStatus();
  }
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
