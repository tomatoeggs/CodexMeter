#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

#include "ble_service.h"
#include "config.h"
#include "device_log.h"
#include "display_rotation.h"
#include "imu.h"
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
static Arduino_CO5300* gfx = new Arduino_CO5300(
    bus, CODEXMETER_LCD_RST, 0, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H, 0, 0, 0, 0);

struct GpioButtonState {
  uint8_t pin;
  bool was_down;
  uint32_t last_press_ms;
};

static uint16_t* lv_buf_1 = nullptr;
static uint16_t* lv_buf_2 = nullptr;
static UsageModel usage;
static AlertModel alert;
static ActivityModel activity;
static ControlModel control;
static uint32_t last_usage_ms = 0;
static bool screen_on = true;
static int brightness_percent = CODEXMETER_BRIGHTNESS_DEFAULT;
static uint8_t displayed_rotation = 0;
static uint8_t rotation_ramp_step = 0;
static uint32_t rotation_ramp_last_ms = 0;

static uint32_t lv_tick() {
  return millis();
}

static void lv_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  if (!screen_on) {
    lv_display_flush_ready(disp);
    return;
  }

  int32_t w = area->x2 - area->x1 + 1;
  int32_t h = area->y2 - area->y1 + 1;
  display_rotation_draw(
      gfx, area->x1, area->y1, w, h, (uint16_t*)px_map, imu_rotation_quadrant());
  lv_display_flush_ready(disp);
}

static void lv_rounder(lv_event_t* event) {
  lv_area_t* area = (lv_area_t*)lv_event_get_param(event);
  area->x1 = area->x1 & ~1;
  area->y1 = area->y1 & ~1;
  area->x2 = area->x2 | 1;
  area->y2 = area->y2 | 1;
}

static int clamp_brightness(int pct) {
  if (pct < CODEXMETER_BRIGHTNESS_MIN) return CODEXMETER_BRIGHTNESS_MIN;
  if (pct > CODEXMETER_BRIGHTNESS_MAX) return CODEXMETER_BRIGHTNESS_MAX;
  return pct;
}

static uint8_t brightness_raw_from_percent(int pct) {
  pct = clamp_brightness(pct);
  uint8_t raw = (uint8_t)((pct * 255 + 50) / 100);
  if (raw == 0) raw = 1;
  return raw;
}

static void apply_brightness() {
  gfx->setBrightness(brightness_raw_from_percent(brightness_percent));
}

static void display_init() {
  if (!gfx->begin()) {
    device_logf("ERROR", "gfx begin failed");
  }
  screen_on = true;
  apply_brightness();
  gfx->fillScreen(0x0000);
  display_rotation_init();

  lv_init();
  lv_tick_set_cb(lv_tick);

  lv_buf_1 = (uint16_t*)heap_caps_malloc(
      CODEXMETER_SCREEN_W * BUF_LINES * sizeof(uint16_t), LV_BUF_CAPS);
  lv_buf_2 = (uint16_t*)heap_caps_malloc(
      CODEXMETER_SCREEN_W * BUF_LINES * sizeof(uint16_t), LV_BUF_CAPS);
  if (!lv_buf_1 || !lv_buf_2) {
    device_logf("ERROR", "LVGL buffer allocation failed");
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

static const char* non_empty_reason(const char* reason) {
  return reason && reason[0] ? reason : "control";
}

static void set_screen_on(bool on, const char* reason) {
  if (screen_on == on) return;

  screen_on = on;
  if (screen_on) {
    gfx->displayOn();
    apply_brightness();
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(nullptr);
    device_logf("INFO", "screen on %s", non_empty_reason(reason));
  } else {
    if (ui_alert_visible()) {
      ui_dismiss_alert();
    }
    gfx->displayOff();
    device_logf("INFO", "screen off %s", non_empty_reason(reason));
  }
}

static void toggle_screen(const char* reason) {
  set_screen_on(!screen_on, reason);
}

static void set_brightness_percent(int pct, const char* reason) {
  brightness_percent = clamp_brightness(pct);
  if (!screen_on) {
    set_screen_on(true, "brightness");
  }
  apply_brightness();
  ui_show_brightness(brightness_percent);
  device_logf(
      "INFO", "brightness %d reason=%s", brightness_percent,
      non_empty_reason(reason));
}

static void adjust_brightness(int delta, const char* reason) {
  set_brightness_percent(brightness_percent + delta, reason);
}

static bool parse_int_arg(const char* text, int* value) {
  if (!text || !*text || !value) return false;
  char* end = nullptr;
  long parsed = strtol(text, &end, 10);
  if (*end != '\0') return false;
  *value = (int)parsed;
  return true;
}

static void redraw_for_rotation() {
  if (!screen_on) return;
  gfx->setBrightness(0);
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(nullptr);
  rotation_ramp_step = 1;
  rotation_ramp_last_ms = 0;
}

static void handle_orientation() {
  imu_tick();

  uint8_t rotation = imu_rotation_quadrant();
  if (rotation != displayed_rotation) {
    displayed_rotation = rotation;
    device_logf("INFO", "display rotation=%u deg=%s", rotation,
                imu_rotation_label(rotation));
    redraw_for_rotation();
  }

  if (!screen_on || rotation_ramp_step == 0) return;

  uint32_t now = millis();
  if (now - rotation_ramp_last_ms < CODEXMETER_ROTATION_RAMP_STEP_MS) return;
  rotation_ramp_last_ms = now;

  static const uint8_t ramp_pct[] = {30, 60, 85, 100};
  uint8_t target = brightness_raw_from_percent(brightness_percent);
  uint8_t raw =
      (uint8_t)(((uint16_t)target * ramp_pct[rotation_ramp_step - 1]) / 100);
  if (raw == 0) raw = 1;
  gfx->setBrightness(raw);

  if (rotation_ramp_step >= sizeof(ramp_pct)) {
    rotation_ramp_step = 0;
    apply_brightness();
  } else {
    rotation_ramp_step++;
  }
}

static void handle_json(const char* json) {
  PayloadKind kind = parse_payload(json, &usage, &alert, &activity, &control);
  if (kind == PAYLOAD_USAGE) {
    last_usage_ms = millis();
    ui_update_usage(usage);
    device_logf(
        "INFO", "usage h5=%d d7=%d status=%s", usage.h5_remaining,
        usage.d7_remaining, usage.status);
    ble_service_ack();
  } else if (kind == PAYLOAD_ALERT) {
    if (alert.has_running_count) {
      activity.valid = true;
      activity.running_count = alert.running_count;
      activity.updated_at = alert.received_at;
      ui_update_activity(activity);
    }
    ui_show_alert(alert);
    if (alert.has_running_count) {
      device_logf("INFO", "alert id=%s run=%d", alert.id, alert.running_count);
    } else {
      device_logf("INFO", "alert id=%s run=n/a", alert.id);
    }
    ble_service_ack();
  } else if (kind == PAYLOAD_ACTIVITY) {
    ui_update_activity(activity);
    device_logf("INFO", "activity run=%d", activity.running_count);
    ble_service_ack();
  } else if (kind == PAYLOAD_CONTROL) {
    if (strcmp(control.command, "screen") == 0) {
      set_screen_on(control.screen_on, control.reason);
      device_logf(
          "INFO", "control screen=%d reason=%s", control.screen_on ? 1 : 0,
          non_empty_reason(control.reason));
      ble_service_ack();
    } else {
      device_logf("WARN", "invalid control command");
      ble_service_nack();
    }
  } else {
    device_logf("WARN", "invalid payload");
    ble_service_nack();
  }
}

static void send_screenshot() {
#ifndef BOARD_HAS_PSRAM
  device_logf("WARN", "screenshot unsupported");
  Serial.println("SCREENSHOT_UNSUPPORTED");
  return;
#else
  const uint32_t w = CODEXMETER_SCREEN_W;
  const uint32_t h = CODEXMETER_SCREEN_H;
  const uint32_t row_bytes = w * sizeof(uint16_t);
  const uint32_t buf_size = row_bytes * h;

  uint8_t* sbuf = (uint8_t*)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
  if (!sbuf) {
    device_logf("ERROR", "screenshot alloc failed");
    Serial.println("SCREENSHOT_ERR");
    return;
  }

  lv_timer_handler();
  lv_refr_now(nullptr);

  lv_draw_buf_t draw_buf;
  lv_result_t init_res =
      lv_draw_buf_init(&draw_buf, w, h, LV_COLOR_FORMAT_RGB565, row_bytes, sbuf, buf_size);
  if (init_res != LV_RESULT_OK) {
    heap_caps_free(sbuf);
    device_logf("ERROR", "screenshot draw buffer init failed");
    Serial.println("SCREENSHOT_ERR");
    return;
  }

  lv_result_t res =
      lv_snapshot_take_to_draw_buf(lv_screen_active(), LV_COLOR_FORMAT_RGB565, &draw_buf);
  if (res != LV_RESULT_OK) {
    heap_caps_free(sbuf);
    device_logf("ERROR", "screenshot snapshot failed");
    Serial.println("SCREENSHOT_ERR");
    return;
  }

  Serial.printf(
      "SCREENSHOT_START %lu %lu %lu\n", (unsigned long)w, (unsigned long)h,
      (unsigned long)buf_size);
  Serial.flush();
  const uint16_t* frame =
      display_rotation_frame((const uint16_t*)sbuf, imu_rotation_quadrant());
  Serial.write((const uint8_t*)frame, buf_size);
  Serial.flush();
  Serial.println();
  Serial.println("SCREENSHOT_END");
  heap_caps_free(sbuf);
  device_logf("INFO", "screenshot sent %lux%lu", (unsigned long)w, (unsigned long)h);
#endif
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
        device_logf("INFO", "serial demo_usage");
      } else if (strcmp(buf, "demo_alert") == 0) {
        alert_apply_demo(&alert);
        ui_show_alert(alert);
        device_logf("INFO", "serial demo_alert");
      } else if (strcmp(buf, "demo_activity") == 0) {
        activity_apply_demo(&activity, 3);
        ui_update_activity(activity);
        device_logf("INFO", "serial demo_activity run=3");
      } else if (strcmp(buf, "demo_idle") == 0) {
        activity_apply_demo(&activity, 0);
        ui_update_activity(activity);
        device_logf("INFO", "serial demo_idle");
      } else if (strcmp(buf, "screen_on") == 0) {
        set_screen_on(true, "serial");
      } else if (strcmp(buf, "screen_off") == 0) {
        set_screen_on(false, "serial");
      } else if (strcmp(buf, "screen_toggle") == 0) {
        toggle_screen("serial");
      } else if (strcmp(buf, "brightness_up") == 0) {
        adjust_brightness(CODEXMETER_BRIGHTNESS_STEP, "serial");
      } else if (strcmp(buf, "brightness_down") == 0) {
        adjust_brightness(-CODEXMETER_BRIGHTNESS_STEP, "serial");
      } else if (strncmp(buf, "brightness ", 11) == 0) {
        set_brightness_percent(atoi(buf + 11), "serial");
      } else if (strcmp(buf, "imu") == 0) {
        imu_print_status(Serial);
      } else if (strcmp(buf, "rotate auto") == 0) {
        imu_set_auto_rotation(true);
      } else if (strncmp(buf, "rotate ", 7) == 0) {
        int degrees = 0;
        int quadrant = -1;
        if (parse_int_arg(buf + 7, &degrees)) {
          if (degrees >= 0 && degrees <= 3) {
            quadrant = degrees;
          } else if (degrees == 90) {
            quadrant = 1;
          } else if (degrees == 180) {
            quadrant = 2;
          } else if (degrees == 270) {
            quadrant = 3;
          }
        }
        if (quadrant >= 0 && imu_set_rotation_quadrant((uint8_t)quadrant)) {
          Serial.printf("ROTATION %d\n", quadrant);
        } else {
          Serial.println("ROTATION_INVALID");
        }
      } else if (strcmp(buf, "screenshot") == 0) {
        send_screenshot();
      } else if (strncmp(buf, "logs", 4) == 0 && (buf[4] == '\0' || buf[4] == ' ')) {
        size_t limit = 0;
        if (buf[4] == ' ') {
          limit = (size_t)strtoul(buf + 5, nullptr, 10);
        }
        device_log_dump(Serial, limit);
      } else if (strcmp(buf, "log_clear") == 0) {
        device_log_clear();
        Serial.println("LOGS_CLEARED");
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
  static GpioButtonState left = {CODEXMETER_BUTTON_LEFT_PIN, false, 0};
  static GpioButtonState right = {CODEXMETER_BUTTON_RIGHT_PIN, false, 0};

  auto pressed = [](GpioButtonState* button) {
    bool down = digitalRead(button->pin) == LOW;
    bool edge = down && !button->was_down;
    uint32_t now = millis();
    bool accepted = edge && now - button->last_press_ms >= 120;
    if (accepted) {
      button->last_press_ms = now;
    }
    button->was_down = down;
    return accepted;
  };

  if (pressed(&left)) {
    adjust_brightness(-CODEXMETER_BRIGHTNESS_STEP, "left");
  }
  if (pressed(&right)) {
    adjust_brightness(CODEXMETER_BRIGHTNESS_STEP, "right");
  }
  if (power_button_pressed()) {
    toggle_screen("button");
  }
}

static void handle_ble_screen_policy() {
  static bool was_connected = false;
  static uint32_t disconnected_since_ms = 0;
  static bool disconnect_screen_off_sent = false;

  bool connected = ble_service_connected();
  uint32_t now = millis();
  if (connected) {
    disconnected_since_ms = 0;
    disconnect_screen_off_sent = false;
  } else if (was_connected || disconnected_since_ms == 0) {
    disconnected_since_ms = now;
    disconnect_screen_off_sent = false;
  } else if (!disconnect_screen_off_sent &&
             now - disconnected_since_ms >= CODEXMETER_AUTO_SCREEN_AFTER_MS) {
    set_screen_on(false, "ble_timeout");
    disconnect_screen_off_sent = true;
  }
  was_connected = connected;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("{\"ready\":true,\"device\":\"CodexMeter\"}");
  device_logf("INFO", "device ready");

  pinMode(CODEXMETER_BUTTON_LEFT_PIN, INPUT_PULLUP);
  pinMode(CODEXMETER_BUTTON_RIGHT_PIN, INPUT_PULLUP);
  power_init();
  imu_init();
  display_init();
  ui_init();
  ui_set_battery(power_battery_percent(), power_is_charging());
  ble_service_init();
  ble_service_request_refresh();
  device_logf("INFO", "setup complete");
}

void loop() {
  static uint32_t last_battery_ui_ms = 0;

  lv_timer_handler();
  ui_tick();
  power_tick();
  ble_service_tick();
  handle_ble_screen_policy();
  handle_button();
  handle_serial();
  handle_orientation();

  if (ble_service_has_data()) {
    handle_json(ble_service_take_data());
  }

  if (usage.valid && millis() - last_usage_ms > CODEXMETER_STALE_AFTER_MS) {
    strlcpy(usage.status, "stale", sizeof(usage.status));
    ui_update_usage(usage);
    last_usage_ms = millis();
    device_logf("WARN", "usage stale");
  }

  if (millis() - last_battery_ui_ms >= 2000) {
    last_battery_ui_ms = millis();
    ui_set_battery(power_battery_percent(), power_is_charging());
  }

  delay(5);
}
