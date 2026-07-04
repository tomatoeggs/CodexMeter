#pragma once

#include <Arduino.h>

#define CODEXMETER_DEVICE_NAME "CodexMeter"

#define CODEXMETER_SCREEN_W 480
#define CODEXMETER_SCREEN_H 480

// Waveshare ESP32-S3-Touch-AMOLED-2.16 QSPI AMOLED defaults. Keep all board
// pins here so hardware revisions can be adjusted without touching UI logic.
#define CODEXMETER_LCD_CS 12
#define CODEXMETER_LCD_SCLK 38
#define CODEXMETER_LCD_D0 4
#define CODEXMETER_LCD_D1 5
#define CODEXMETER_LCD_D2 6
#define CODEXMETER_LCD_D3 7
#define CODEXMETER_LCD_RST 2

#define CODEXMETER_I2C_SDA 15
#define CODEXMETER_I2C_SCL 14
#define CODEXMETER_AXP2101_ADDR 0x34

// BOOT/left side button is enough for dismissing alerts in v1. Touch support
// can be added behind the same ui_dismiss_alert() call once the touch driver is
// validated on the physical board.
#define CODEXMETER_BUTTON_PIN 0

#define CODEXMETER_STALE_AFTER_MS (180UL * 1000UL)
#define CODEXMETER_ALERT_HOLD_MS 8000UL
#define CODEXMETER_FLASH_STEP_MS 180UL
#define CODEXMETER_FLASH_STEPS 6
