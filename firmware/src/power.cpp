#include "power.h"

#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>

#include "config.h"

static XPowersPMU pmu;
static bool pmu_ready = false;
static int cached_pct = -1;
static bool cached_charging = false;
static bool pwr_pressed = false;
static uint32_t last_battery_ms = 0;
static uint32_t last_charging_ms = 0;
static uint32_t last_pwr_ms = 0;

void power_init() {
  pmu_ready = pmu.begin(Wire, CODEXMETER_AXP2101_ADDR, CODEXMETER_I2C_SDA,
                        CODEXMETER_I2C_SCL);
  if (!pmu_ready) {
    Serial.println("AXP2101 init failed");
    return;
  }

  pmu.enableBattDetection();
  pmu.enableBattVoltageMeasure();
  pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  pmu.clearIrqStatus();
  pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
  pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_8S);

  cached_pct = pmu.getBatteryPercent();
  cached_charging = pmu.isCharging();
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
    if (pmu.isPekeyShortPressIrq()) {
      pwr_pressed = true;
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

bool power_button_pressed() {
  if (!pwr_pressed) return false;
  pwr_pressed = false;
  return true;
}
