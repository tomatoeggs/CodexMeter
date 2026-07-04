#include "ui.h"

#include "config.h"

#include <lvgl.h>
#include <time.h>

LV_FONT_DECLARE(codexmeter_font_30);
LV_FONT_DECLARE(codexmeter_alert_28);
LV_FONT_DECLARE(codexmeter_percent_64);

static lv_obj_t* screen;
static lv_obj_t* top_logo;
static lv_obj_t* top_title;
static lv_obj_t* top_battery;
static lv_obj_t* top_battery_fill;
static lv_obj_t* h5_pct;
static lv_obj_t* h5_reset;
static lv_obj_t* d7_pct;
static lv_obj_t* d7_reset;
static lv_obj_t* activity_dots[12];
static lv_obj_t* alert_layer;
static lv_obj_t* alert_title;
static lv_obj_t* alert_body;
static lv_obj_t* brightness_layer;
static lv_obj_t* brightness_bar;
static lv_obj_t* brightness_pct;

static bool alert_active = false;
static uint32_t alert_started_ms = 0;
static uint32_t flash_started_ms = 0;
static int flash_step = -1;
static bool brightness_active = false;
static uint32_t brightness_started_ms = 0;

static const lv_color_t BG = lv_color_hex(0x0f1115);
static const lv_color_t PANEL = lv_color_hex(0x1b1f2a);
static const lv_color_t TEXT = lv_color_hex(0xf6f7fb);
static const lv_color_t DIM = lv_color_hex(0x8d96a8);
static const lv_color_t CODEX_BLUE = lv_color_hex(0x18a8ff);
static const lv_color_t BATTERY_GREEN = lv_color_hex(0x2fda77);
static const lv_color_t BATTERY_YELLOW = lv_color_hex(0xffd43b);
static const lv_color_t BATTERY_RED = lv_color_hex(0xff4d4f);

static const int PANEL_W = CODEXMETER_SCREEN_W - 40;
static const int PANEL_H = 158;
static const int PANEL_CONTENT_W = PANEL_W - 64;
static const int PANEL_CONTENT_H = 108;
static const int ALERT_TEXT_W = CODEXMETER_SCREEN_W - 64;
static const int ALERT_TEXT_X = 32;
static const int ALERT_TITLE_Y = 126;
static const int ALERT_BODY_Y = 190;
static const int ACTIVITY_MAX_DOTS = 12;
static const int ACTIVITY_DOT_SIZE = 10;
static const int ACTIVITY_DOT_GAP = 9;
static const int ACTIVITY_DOT_Y = 452;
static const int BRIGHTNESS_LAYER_W = 300;
static const int BRIGHTNESS_LAYER_H = 104;

static const lv_font_t* ui_font() {
  return &codexmeter_font_30;
}

static const lv_font_t* percent_font() {
  return &codexmeter_percent_64;
}

static const lv_font_t* body_font() {
  return &codexmeter_alert_28;
}

static void base_style(lv_obj_t* obj) {
  lv_obj_set_style_bg_color(obj, BG, 0);
  lv_obj_set_style_text_color(obj, TEXT, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
}

static lv_obj_t* make_label(lv_obj_t* parent, const char* text, const lv_font_t* font,
                            lv_color_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_letter_space(label, 0, 0);
  return label;
}

static lv_obj_t* make_panel(int y) {
  lv_obj_t* panel = lv_obj_create(screen);
  lv_obj_set_size(panel, PANEL_W, PANEL_H);
  lv_obj_set_pos(panel, 20, y);
  lv_obj_set_style_radius(panel, 8, 0);
  lv_obj_set_style_bg_color(panel, PANEL, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0x303747), 0);
  return panel;
}

static void strip_obj(lv_obj_t* obj) {
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t* make_panel_content(lv_obj_t* panel) {
  lv_obj_t* content = lv_obj_create(panel);
  strip_obj(content);
  lv_obj_set_size(content, PANEL_CONTENT_W, PANEL_CONTENT_H);
  lv_obj_center(content);
  return content;
}

static void layout_alert_text() {
  lv_obj_set_width(alert_title, ALERT_TEXT_W);
  lv_obj_set_pos(alert_title, ALERT_TEXT_X, ALERT_TITLE_Y);

  lv_obj_set_width(alert_body, ALERT_TEXT_W);
  lv_obj_set_pos(alert_body, ALERT_TEXT_X, ALERT_BODY_Y);
}

static lv_obj_t* make_battery_icon(lv_obj_t* parent) {
  lv_obj_t* icon = lv_obj_create(parent);
  strip_obj(icon);
  lv_obj_set_size(icon, 31, 18);

  lv_obj_t* body = lv_obj_create(icon);
  strip_obj(body);
  lv_obj_set_size(body, 25, 16);
  lv_obj_set_pos(body, 0, 1);
  lv_obj_set_style_radius(body, 3, 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(body, 2, 0);
  lv_obj_set_style_border_color(body, DIM, 0);
  lv_obj_set_style_border_opa(body, LV_OPA_COVER, 0);

  top_battery_fill = lv_obj_create(body);
  strip_obj(top_battery_fill);
  lv_obj_set_size(top_battery_fill, 18, 8);
  lv_obj_set_pos(top_battery_fill, 3, 4);
  lv_obj_set_style_radius(top_battery_fill, 1, 0);
  lv_obj_set_style_bg_color(top_battery_fill, DIM, 0);
  lv_obj_set_style_bg_opa(top_battery_fill, LV_OPA_COVER, 0);

  lv_obj_t* cap = lv_obj_create(icon);
  strip_obj(cap);
  lv_obj_set_size(cap, 4, 8);
  lv_obj_set_pos(cap, 27, 5);
  lv_obj_set_style_radius(cap, 1, 0);
  lv_obj_set_style_bg_color(cap, DIM, 0);
  lv_obj_set_style_bg_opa(cap, LV_OPA_COVER, 0);
  return icon;
}

static lv_obj_t* make_activity_dot(lv_obj_t* parent) {
  lv_obj_t* dot = lv_obj_create(parent);
  strip_obj(dot);
  lv_obj_set_size(dot, ACTIVITY_DOT_SIZE, ACTIVITY_DOT_SIZE);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, CODEX_BLUE, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
  return dot;
}

static lv_color_t battery_fill_color(int pct) {
  if (pct <= 20) return BATTERY_RED;
  if (pct <= 50) return BATTERY_YELLOW;
  return BATTERY_GREEN;
}

static long usage_now_epoch(const UsageModel& usage) {
  if (usage.updated_at <= 0) return 0;
  return usage.updated_at + (long)((millis() - usage.received_ms) / 1000UL);
}

static long seconds_until_reset(long reset_epoch, const UsageModel& usage) {
  long now = usage_now_epoch(usage);
  if (reset_epoch <= 0 || now <= 0) return -1;
  long seconds = reset_epoch - now;
  return seconds > 0 ? seconds : 0;
}

static String h5_reset_text(const UsageModel& usage) {
  long seconds = seconds_until_reset(usage.h5_reset, usage);
  if (seconds < 0) return "-- 后重置";
  long minutes = (seconds + 59) / 60;
  char buf[24];
  snprintf(buf, sizeof(buf), "%02ld:%02ld 后重置", minutes / 60, minutes % 60);
  return String(buf);
}

static String d7_reset_text(const UsageModel& usage) {
  long seconds = seconds_until_reset(usage.d7_reset, usage);
  if (seconds < 0) return "-- 后重置";
  long days = (seconds + 24L * 60L * 60L - 1) / (24L * 60L * 60L);
  char buf[24];
  snprintf(buf, sizeof(buf), "%ldd 后重置", days);
  return String(buf);
}

static String pct_text(int value) {
  if (value < 0) return "--%";
  return String(value) + "%";
}

static void make_brightness_overlay() {
  brightness_layer = lv_obj_create(screen);
  lv_obj_remove_flag(brightness_layer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(brightness_layer, BRIGHTNESS_LAYER_W, BRIGHTNESS_LAYER_H);
  lv_obj_align(brightness_layer, LV_ALIGN_CENTER, 0, 24);
  lv_obj_set_style_radius(brightness_layer, 8, 0);
  lv_obj_set_style_pad_all(brightness_layer, 0, 0);
  lv_obj_set_style_bg_color(brightness_layer, lv_color_hex(0x151923), 0);
  lv_obj_set_style_bg_opa(brightness_layer, LV_OPA_90, 0);
  lv_obj_set_style_border_width(brightness_layer, 1, 0);
  lv_obj_set_style_border_color(brightness_layer, lv_color_hex(0x303747), 0);
  lv_obj_set_style_shadow_width(brightness_layer, 18, 0);
  lv_obj_set_style_shadow_color(brightness_layer, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_opa(brightness_layer, LV_OPA_40, 0);

  brightness_pct = make_label(brightness_layer, "80%", &lv_font_montserrat_32, TEXT);
  lv_obj_align(brightness_pct, LV_ALIGN_TOP_MID, 0, 12);

  brightness_bar = lv_bar_create(brightness_layer);
  lv_obj_set_size(brightness_bar, 238, 13);
  lv_obj_align(brightness_bar, LV_ALIGN_BOTTOM_MID, 0, -24);
  lv_bar_set_range(brightness_bar, 0, 100);
  lv_bar_set_value(brightness_bar, 80, LV_ANIM_OFF);
  lv_obj_set_style_radius(brightness_bar, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(brightness_bar, lv_color_hex(0x303747), 0);
  lv_obj_set_style_bg_opa(brightness_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(brightness_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightness_bar, CODEX_BLUE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(brightness_bar, LV_OPA_COVER, LV_PART_INDICATOR);

  lv_obj_add_flag(brightness_layer, LV_OBJ_FLAG_HIDDEN);
}

void ui_init() {
  screen = lv_screen_active();
  base_style(screen);

  top_logo = make_label(screen, "Codex", &lv_font_montserrat_32, CODEX_BLUE);
  lv_obj_align(top_logo, LV_ALIGN_TOP_LEFT, 22, 16);

  top_title = make_label(screen, "剩余用量", ui_font(), TEXT);
  lv_obj_align(top_title, LV_ALIGN_TOP_MID, 0, 17);

  lv_obj_t* battery_icon = make_battery_icon(screen);
  lv_obj_align(battery_icon, LV_ALIGN_TOP_RIGHT, -18, 25);
  top_battery = make_label(screen, "--%", &lv_font_montserrat_32, DIM);
  lv_obj_align(top_battery, LV_ALIGN_TOP_RIGHT, -58, 16);

  lv_obj_t* panel_h5 = make_panel(92);
  lv_obj_t* content_h5 = make_panel_content(panel_h5);
  make_label(content_h5, "5h 剩余:", ui_font(), DIM);
  lv_obj_t* label = lv_obj_get_child(content_h5, 0);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
  h5_pct = make_label(content_h5, "--%", percent_font(), TEXT);
  lv_obj_align(h5_pct, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  h5_reset = make_label(content_h5, "-- 后重置", ui_font(), DIM);
  lv_obj_align(h5_reset, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  lv_obj_t* panel_d7 = make_panel(270);
  lv_obj_t* content_d7 = make_panel_content(panel_d7);
  make_label(content_d7, "7d 剩余:", ui_font(), DIM);
  label = lv_obj_get_child(content_d7, 0);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
  d7_pct = make_label(content_d7, "--%", percent_font(), TEXT);
  lv_obj_align(d7_pct, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  d7_reset = make_label(content_d7, "-- 后重置", ui_font(), DIM);
  lv_obj_align(d7_reset, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  for (int i = 0; i < ACTIVITY_MAX_DOTS; i++) {
    activity_dots[i] = make_activity_dot(screen);
  }

  alert_layer = lv_obj_create(screen);
  lv_obj_set_size(alert_layer, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(alert_layer, 0, 0);
  lv_obj_set_style_bg_color(alert_layer, BG, 0);
  lv_obj_set_style_border_width(alert_layer, 0, 0);
  lv_obj_add_flag(alert_layer, LV_OBJ_FLAG_HIDDEN);

  alert_title = make_label(alert_layer, "任务完成！", ui_font(), TEXT);
  lv_obj_set_width(alert_title, ALERT_TEXT_W);
  lv_label_set_long_mode(alert_title, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(alert_title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(alert_title, ALERT_TEXT_X, ALERT_TITLE_Y);
  alert_body = make_label(alert_layer, "", body_font(), TEXT);
  lv_obj_set_width(alert_body, ALERT_TEXT_W);
  lv_label_set_long_mode(alert_body, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(alert_body, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(alert_body, ALERT_TEXT_X, ALERT_BODY_Y);

  make_brightness_overlay();
}

void ui_update_usage(const UsageModel& usage) {
  lv_label_set_text(h5_pct, pct_text(usage.h5_remaining).c_str());
  lv_label_set_text(h5_reset, h5_reset_text(usage).c_str());
  lv_label_set_text(d7_pct, pct_text(usage.d7_remaining).c_str());
  lv_label_set_text(d7_reset, d7_reset_text(usage).c_str());
}

void ui_update_activity(const ActivityModel& activity) {
  int count = activity.running_count;
  if (count < 0) count = 0;
  if (count > ACTIVITY_MAX_DOTS) count = ACTIVITY_MAX_DOTS;

  int total_w = count > 0
                    ? count * ACTIVITY_DOT_SIZE + (count - 1) * ACTIVITY_DOT_GAP
                    : 0;
  int start_x = (CODEXMETER_SCREEN_W - total_w) / 2;

  for (int i = 0; i < ACTIVITY_MAX_DOTS; i++) {
    if (i < count) {
      lv_obj_set_pos(
          activity_dots[i],
          start_x + i * (ACTIVITY_DOT_SIZE + ACTIVITY_DOT_GAP),
          ACTIVITY_DOT_Y);
      lv_obj_clear_flag(activity_dots[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(activity_dots[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void ui_show_alert(const AlertModel& alert) {
  alert_active = true;
  alert_started_ms = millis();
  flash_started_ms = millis();
  flash_step = 0;
  lv_obj_clear_flag(alert_layer, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(alert_title, alert.title);
  lv_label_set_text(alert_body, alert.body);
  layout_alert_text();
  lv_obj_add_flag(alert_title, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(alert_body, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(alert_layer, lv_color_hex(0xff2d2d), 0);
}

void ui_set_battery(int pct, bool charging) {
  if (pct < 0) {
    lv_label_set_text(top_battery, charging ? "USB" : "--%");
    lv_obj_set_width(top_battery_fill, charging ? 18 : 2);
    lv_obj_set_style_bg_color(top_battery_fill, charging ? BATTERY_GREEN : DIM, 0);
  } else {
    lv_label_set_text(top_battery, (String(pct) + "%").c_str());
    int width = 2 + (pct * 16 + 50) / 100;
    if (width < 2) width = 2;
    if (width > 18) width = 18;
    lv_obj_set_width(top_battery_fill, width);
    lv_obj_set_style_bg_color(top_battery_fill, battery_fill_color(pct), 0);
  }
}

void ui_show_brightness(int pct) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

  lv_label_set_text(brightness_pct, (String(pct) + "%").c_str());
  lv_bar_set_value(brightness_bar, pct, LV_ANIM_ON);
  lv_obj_clear_flag(brightness_layer, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(brightness_layer, -1);
  brightness_active = true;
  brightness_started_ms = millis();
}

void ui_tick() {
  uint32_t now = millis();
  if (brightness_active &&
      now - brightness_started_ms >= CODEXMETER_BRIGHTNESS_OVERLAY_MS) {
    brightness_active = false;
    lv_obj_add_flag(brightness_layer, LV_OBJ_FLAG_HIDDEN);
  }

  if (!alert_active) return;

  if (flash_step >= 0 && now - flash_started_ms >= CODEXMETER_FLASH_STEP_MS) {
    flash_step++;
    flash_started_ms = now;
    if (flash_step >= CODEXMETER_FLASH_STEPS) {
      flash_step = -1;
      alert_started_ms = now;
      lv_obj_set_style_bg_color(alert_layer, BG, 0);
      lv_obj_clear_flag(alert_title, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(alert_body, LV_OBJ_FLAG_HIDDEN);
      layout_alert_text();
    } else {
      switch (flash_step % 3) {
        case 0:
          lv_obj_set_style_bg_color(alert_layer, lv_color_hex(0xff2d2d), 0);
          break;
        case 1:
          lv_obj_set_style_bg_color(alert_layer, lv_color_hex(0xffd43b), 0);
          break;
        default:
          lv_obj_set_style_bg_color(alert_layer, lv_color_hex(0x2fda77), 0);
          break;
      }
    }
  }

  if (now - alert_started_ms >= CODEXMETER_ALERT_HOLD_MS) {
    ui_dismiss_alert();
  }
}

void ui_dismiss_alert() {
  alert_active = false;
  flash_step = -1;
  lv_obj_add_flag(alert_layer, LV_OBJ_FLAG_HIDDEN);
}

bool ui_alert_visible() {
  return alert_active;
}
