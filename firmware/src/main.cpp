#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

#include "ble_service.h"
#include "config.h"
#include "model.h"
#include "power.h"
#include "ui.h"

#ifdef BOARD_HAS_PSRAM
#define LV_BUF_CAPS MALLOC_CAP_SPIRAM
#define BUF_LINES 40
#else
#define LV_BUF_CAPS (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define BUF_LINES 20
#endif

static Arduino_DataBus* bus = new Arduino_ESP32QSPI(
    CODEXMETER_LCD_CS, CODEXMETER_LCD_SCLK, CODEXMETER_LCD_D0, CODEXMETER_LCD_D1,
    CODEXMETER_LCD_D2, CODEXMETER_LCD_D3);
static Arduino_GFX* gfx = new Arduino_CO5300(
    bus, CODEXMETER_LCD_RST, 0, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H, 0, 0, 0, 0);

static uint16_t* lv_buf_1 = nullptr;
static uint16_t* lv_buf_2 = nullptr;
static UsageModel usage;
static AlertModel alert;
static uint32_t last_usage_ms = 0;

static uint32_t lv_tick() {
  return millis();
}

static void lv_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  int32_t w = area->x2 - area->x1 + 1;
  int32_t h = area->y2 - area->y1 + 1;
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)px_map, w, h);
  lv_display_flush_ready(disp);
}

static void lv_rounder(lv_event_t* event) {
  lv_area_t* area = (lv_area_t*)lv_event_get_param(event);
  area->x1 = area->x1 & ~1;
  area->y1 = area->y1 & ~1;
  area->x2 = area->x2 | 1;
  area->y2 = area->y2 | 1;
}

static void display_init() {
  if (!gfx->begin()) {
    Serial.println("gfx begin failed");
  }
  gfx->fillScreen(0x0000);

  lv_init();
  lv_tick_set_cb(lv_tick);

  lv_buf_1 = (uint16_t*)heap_caps_malloc(
      CODEXMETER_SCREEN_W * BUF_LINES * sizeof(uint16_t), LV_BUF_CAPS);
  lv_buf_2 = (uint16_t*)heap_caps_malloc(
      CODEXMETER_SCREEN_W * BUF_LINES * sizeof(uint16_t), LV_BUF_CAPS);
  if (!lv_buf_1 || !lv_buf_2) {
    Serial.println("LVGL buffer allocation failed");
    while (true) delay(1000);
  }

  lv_display_t* disp = lv_display_create(CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, lv_flush);
  lv_display_add_event_cb(disp, lv_rounder, LV_EVENT_INVALIDATE_AREA, nullptr);
  lv_display_set_buffers(
      disp, lv_buf_1, lv_buf_2, CODEXMETER_SCREEN_W * BUF_LINES * sizeof(uint16_t),
      LV_DISPLAY_RENDER_MODE_PARTIAL);
}

static void handle_json(const char* json) {
  PayloadKind kind = parse_payload(json, &usage, &alert);
  if (kind == PAYLOAD_USAGE) {
    last_usage_ms = millis();
    ui_update_usage(usage);
    ble_service_ack();
  } else if (kind == PAYLOAD_ALERT) {
    ui_show_alert(alert);
    ble_service_ack();
  } else {
    ble_service_nack();
  }
}

static void handle_serial() {
  static char buf[520];
  static size_t pos = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      buf[pos] = '\0';
      if (strcmp(buf, "demo_usage") == 0) {
        usage_apply_demo(&usage);
        last_usage_ms = millis();
        ui_update_usage(usage);
      } else if (strcmp(buf, "demo_alert") == 0) {
        alert_apply_demo(&alert);
        ui_show_alert(alert);
      } else if (buf[0] == '{') {
        handle_json(buf);
      }
      pos = 0;
    } else if (pos < sizeof(buf) - 1) {
      buf[pos++] = c;
    }
  }
}

static void handle_button() {
  static bool was_down = false;
  bool down = digitalRead(CODEXMETER_BUTTON_PIN) == LOW;
  if (((down && !was_down) || power_button_pressed()) && ui_alert_visible()) {
    ui_dismiss_alert();
  }
  was_down = down;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("{\"ready\":true,\"device\":\"CodexMeter\"}");

  pinMode(CODEXMETER_BUTTON_PIN, INPUT_PULLUP);
  power_init();
  display_init();
  ui_init();
  ui_set_battery(power_battery_percent(), power_is_charging());
  ble_service_init();
  ble_service_request_refresh();
}

void loop() {
  static uint32_t last_battery_ui_ms = 0;

  lv_timer_handler();
  ui_tick();
  power_tick();
  ble_service_tick();
  handle_button();
  handle_serial();

  if (ble_service_has_data()) {
    handle_json(ble_service_take_data());
  }

  if (usage.valid && millis() - last_usage_ms > CODEXMETER_STALE_AFTER_MS) {
    strlcpy(usage.status, "stale", sizeof(usage.status));
    ui_update_usage(usage);
    last_usage_ms = millis();
  }

  if (millis() - last_battery_ui_ms >= 2000) {
    last_battery_ui_ms = millis();
    ui_set_battery(power_battery_percent(), power_is_charging());
  }

  delay(5);
}
