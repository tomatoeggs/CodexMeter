#include "ui.h"

#include "config.h"
#include "device_log.h"

#include <esp_heap_caps.h>
#include <lvgl.h>
#include <math.h>
#include <string.h>
#include <time.h>

LV_FONT_DECLARE(codexmeter_font_30);
LV_FONT_DECLARE(codexmeter_percent_64);

static lv_obj_t* screen;
static lv_obj_t* main_layer;
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
static lv_font_t* alert_title_ttf;
static lv_font_t* alert_body_ttf;

static bool alert_active = false;
static bool alert_reveal_pending = false;
static uint32_t alert_started_ms = 0;
static uint32_t flash_started_ms = 0;
static uint32_t alert_reveal_started_ms = 0;
static int flash_step = -1;
static bool brightness_active = false;
static uint32_t brightness_started_ms = 0;
static uint32_t drift_last_ms = 0;
static uint8_t drift_index = 0;
static int activity_visible_count = 0;
static uint32_t activity_anim_started_ms = 0;
static uint32_t activity_anim_last_ms = 0;
static uint32_t activity_dot_started_ms[12];

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
// TinyTTF centers by advance width; this font's visible CJK bounds sit slightly right.
static const int ALERT_TITLE_X =
    ALERT_TEXT_X + CODEXMETER_ALERT_TITLE_VISUAL_OFFSET_X;
static const int ALERT_BODY_X =
    ALERT_TEXT_X + CODEXMETER_ALERT_BODY_VISUAL_OFFSET_X;
static const int ALERT_TITLE_Y = 126;
static const int ALERT_BODY_Y = 190;
static const int ALERT_BODY_LINES = 4;
static const int ACTIVITY_MAX_DOTS = 12;
static const int ACTIVITY_DOT_SIZE = 10;
static const int ACTIVITY_DOT_GAP = 9;
static const int ACTIVITY_DOT_Y = 452;
static const uint32_t ACTIVITY_ANIM_INTERVAL_MS = 33;
static const uint32_t ACTIVITY_COLOR_CYCLE_MS = 7000;
static const uint32_t ACTIVITY_COLOR_DOT_PHASE_MS = 420;
static const uint32_t ACTIVITY_COLOR_SPREAD_IN_MS = 2200;
static const uint32_t ACTIVITY_BREATH_CYCLE_MS = 4000;
static const uint32_t ACTIVITY_DOT_PHASE_MS = 180;
static const uint16_t ACTIVITY_COLOR_START_HUE = 203;
static const uint8_t ACTIVITY_COLOR_SATURATION = 91;
static const uint8_t ACTIVITY_COLOR_VALUE = 100;
static const float ACTIVITY_TWO_PI = 6.28318530718f;
static const uint8_t ACTIVITY_OPA_MIN = 135;
static const uint8_t ACTIVITY_OPA_MAX = 255;
static const int BRIGHTNESS_LAYER_W = 300;
static const int BRIGHTNESS_LAYER_H = 104;

struct DriftOffset {
  int8_t x;
  int8_t y;
};

static const DriftOffset DRIFT_OFFSETS[] = {
    {0, 0},  {1, 0},   {-1, 1}, {2, -1}, {-2, 0}, {1, 2},
    {0, -2}, {-1, -1}, {2, 1},  {-2, 2}, {1, -2}, {-2, -1},
};

static const lv_font_t* ui_font() {
  return &codexmeter_font_30;
}

static const lv_font_t* percent_font() {
  return &codexmeter_percent_64;
}

static const lv_font_t* body_font() {
  if (alert_body_ttf) return alert_body_ttf;
  return ui_font();
}

static const lv_font_t* alert_title_font() {
  if (alert_title_ttf) return alert_title_ttf;
  return ui_font();
}

static size_t text_len(const char* text) {
  return text ? strlen(text) : 0;
}

static void log_lvgl_mem(const char* phase) {
  lv_mem_monitor_t mon;
  lv_mem_monitor(&mon);
  device_logf(
      "INFO", "lv_mem %s t=%lu f=%lu b=%lu u=%u g=%u",
      phase && phase[0] ? phase : "-", (unsigned long)mon.total_size,
      (unsigned long)mon.free_size, (unsigned long)mon.free_biggest_size,
      mon.used_pct, mon.frag_pct);
}

static void log_alert_diagnostics(const char* phase) {
#if CODEXMETER_ALERT_DIAGNOSTICS
  device_log_heap(phase);
  log_lvgl_mem(phase);
#else
  (void)phase;
#endif
}

static void init_dynamic_fonts() {
#if LV_USE_TINY_TTF
  alert_title_ttf = lv_tiny_ttf_create_file_ex(
      CODEXMETER_TTF_FONT_PATH, CODEXMETER_TTF_TITLE_SIZE, LV_FONT_KERNING_NONE,
      CODEXMETER_TTF_CACHE_GLYPHS);
  alert_body_ttf = lv_tiny_ttf_create_file_ex(
      CODEXMETER_TTF_FONT_PATH, CODEXMETER_TTF_ALERT_SIZE, LV_FONT_KERNING_NONE,
      CODEXMETER_TTF_CACHE_GLYPHS);
  if (alert_title_ttf) alert_title_ttf->fallback = ui_font();
  if (alert_body_ttf) alert_body_ttf->fallback = ui_font();
  if (alert_title_ttf && alert_body_ttf) {
    device_logf("INFO", "TinyTTF font ready %s", CODEXMETER_TTF_FONT_PATH);
    device_log_heap("ttf_ready");
    log_lvgl_mem("ttf_ready");
  } else {
    device_logf("WARN", "TinyTTF font unavailable %s", CODEXMETER_TTF_FONT_PATH);
    device_log_heap("ttf_unavailable");
    log_lvgl_mem("ttf_unavailable");
  }
#else
  device_logf("WARN", "TinyTTF disabled");
#endif
}

static void base_style(lv_obj_t* obj) {
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_color(obj, BG, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
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
  lv_obj_t* panel = lv_obj_create(main_layer);
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
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

static int clamp_drift_offset(int value) {
  if (value < -CODEXMETER_BURN_IN_DRIFT_MAX_PX) {
    return -CODEXMETER_BURN_IN_DRIFT_MAX_PX;
  }
  if (value > CODEXMETER_BURN_IN_DRIFT_MAX_PX) {
    return CODEXMETER_BURN_IN_DRIFT_MAX_PX;
  }
  return value;
}

static void apply_burn_in_drift(const DriftOffset& offset) {
  if (!main_layer) return;
  int x = clamp_drift_offset(offset.x);
  int y = clamp_drift_offset(offset.y);
  lv_obj_set_pos(main_layer, x, y);
  lv_obj_invalidate(screen);
  lv_refr_now(nullptr);
  device_logf("INFO", "burn_in_drift x=%d y=%d", x, y);
}

static void tick_burn_in_drift(uint32_t now) {
#if CODEXMETER_BURN_IN_DRIFT_INTERVAL_MS > 0 && CODEXMETER_BURN_IN_DRIFT_MAX_PX > 0
  if (!main_layer || alert_active) return;

  if (drift_last_ms == 0) {
    drift_last_ms = now;
    return;
  }
  if (now - drift_last_ms < CODEXMETER_BURN_IN_DRIFT_INTERVAL_MS) return;

  drift_last_ms = now;
  drift_index = (drift_index + 1) % (sizeof(DRIFT_OFFSETS) / sizeof(DRIFT_OFFSETS[0]));
  apply_burn_in_drift(DRIFT_OFFSETS[drift_index]);
#else
  (void)now;
#endif
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
  lv_obj_set_pos(alert_title, ALERT_TITLE_X, ALERT_TITLE_Y);

  lv_obj_set_size(
      alert_body, ALERT_TEXT_W, body_font()->line_height * ALERT_BODY_LINES);
  lv_obj_set_pos(alert_body, ALERT_BODY_X, ALERT_BODY_Y);
}

static void reveal_alert_text(uint32_t now) {
  alert_reveal_pending = false;
  alert_started_ms = now;
  lv_obj_clear_flag(alert_title, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(alert_body, LV_OBJ_FLAG_HIDDEN);
  layout_alert_text();
  device_logf("INFO", "alert reveal");
  log_alert_diagnostics("alert_reveal");
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

static float eased_cycle_phase(uint32_t cycle_ms, uint32_t cycle_duration_ms) {
  if (cycle_duration_ms == 0) return 0.0f;
  float phase = (float)cycle_ms / (float)cycle_duration_ms;
  return phase - sinf(ACTIVITY_TWO_PI * phase) / ACTIVITY_TWO_PI;
}

static float smoothstep01(float phase) {
  if (phase <= 0.0f) return 0.0f;
  if (phase >= 1.0f) return 1.0f;
  return phase * phase * (3.0f - 2.0f * phase);
}

static uint32_t activity_dot_color_offset_ms(uint32_t dot_age_ms, int index) {
  uint32_t target_ms = index * ACTIVITY_COLOR_DOT_PHASE_MS;
  if (target_ms == 0) return 0;
  float spread =
      smoothstep01((float)dot_age_ms / (float)ACTIVITY_COLOR_SPREAD_IN_MS);
  return (uint32_t)(target_ms * spread + 0.5f);
}

static lv_color_t activity_color_at(uint32_t elapsed_ms) {
  uint32_t cycle_ms = elapsed_ms % ACTIVITY_COLOR_CYCLE_MS;
  float phase = eased_cycle_phase(cycle_ms, ACTIVITY_COLOR_CYCLE_MS);
  uint16_t hue =
      (ACTIVITY_COLOR_START_HUE + (uint16_t)(phase * 360.0f + 0.5f)) % 360;
  return lv_color_hsv_to_rgb(
      hue, ACTIVITY_COLOR_SATURATION, ACTIVITY_COLOR_VALUE);
}

static lv_opa_t activity_breath_opa_at(uint32_t elapsed_ms) {
  uint32_t half_cycle = ACTIVITY_BREATH_CYCLE_MS / 2;
  uint32_t phase =
      (elapsed_ms + half_cycle) % ACTIVITY_BREATH_CYCLE_MS;
  uint32_t wave =
      phase < half_cycle ? phase : ACTIVITY_BREATH_CYCLE_MS - phase;
  uint32_t amount = (wave * 255U) / half_cycle;
  return (lv_opa_t)(ACTIVITY_OPA_MIN +
                    ((ACTIVITY_OPA_MAX - ACTIVITY_OPA_MIN) * amount) / 255U);
}

static void tick_activity_dots(uint32_t now) {
  if (activity_visible_count <= 0) return;
  if (activity_anim_last_ms != 0 &&
      now - activity_anim_last_ms < ACTIVITY_ANIM_INTERVAL_MS) {
    return;
  }

  activity_anim_last_ms = now;
  uint32_t elapsed_ms = now - activity_anim_started_ms;
  for (int i = 0; i < activity_visible_count; i++) {
    uint32_t dot_started_ms =
        activity_dot_started_ms[i] ? activity_dot_started_ms[i]
                                   : activity_anim_started_ms;
    uint32_t dot_age_ms = now - dot_started_ms;
    uint32_t color_elapsed_ms =
        elapsed_ms + activity_dot_color_offset_ms(dot_age_ms, i);
    uint32_t breath_elapsed_ms = elapsed_ms + i * ACTIVITY_DOT_PHASE_MS;
    lv_obj_set_style_bg_color(
        activity_dots[i], activity_color_at(color_elapsed_ms), 0);
    lv_obj_set_style_bg_opa(
        activity_dots[i], activity_breath_opa_at(breath_elapsed_ms), 0);
  }
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

  brightness_pct = make_label(brightness_layer, "", &lv_font_montserrat_32, TEXT);
  lv_label_set_text(
      brightness_pct, (String(CODEXMETER_BRIGHTNESS_DEFAULT) + "%").c_str());
  lv_obj_align(brightness_pct, LV_ALIGN_TOP_MID, 0, 12);

  brightness_bar = lv_bar_create(brightness_layer);
  lv_obj_set_size(brightness_bar, 238, 13);
  lv_obj_align(brightness_bar, LV_ALIGN_BOTTOM_MID, 0, -24);
  lv_bar_set_range(brightness_bar, 0, 100);
  lv_bar_set_value(brightness_bar, CODEXMETER_BRIGHTNESS_DEFAULT, LV_ANIM_OFF);
  lv_obj_set_style_radius(brightness_bar, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(brightness_bar, lv_color_hex(0x303747), 0);
  lv_obj_set_style_bg_opa(brightness_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(brightness_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightness_bar, CODEX_BLUE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(brightness_bar, LV_OPA_COVER, LV_PART_INDICATOR);

  lv_obj_add_flag(brightness_layer, LV_OBJ_FLAG_HIDDEN);
}

void ui_init() {
  init_dynamic_fonts();

  screen = lv_screen_active();
  base_style(screen);

  main_layer = lv_obj_create(screen);
  strip_obj(main_layer);
  lv_obj_set_size(main_layer, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(main_layer, 0, 0);
  lv_obj_set_style_bg_color(main_layer, BG, 0);
  lv_obj_set_style_bg_opa(main_layer, LV_OPA_COVER, 0);

  top_logo = make_label(main_layer, "Codex", &lv_font_montserrat_32, CODEX_BLUE);
  lv_obj_align(top_logo, LV_ALIGN_TOP_LEFT, 22, 16);

  top_title = make_label(main_layer, "剩余用量", ui_font(), TEXT);
  lv_obj_align(top_title, LV_ALIGN_TOP_MID, 0, 17);

  lv_obj_t* battery_icon = make_battery_icon(main_layer);
  lv_obj_align(battery_icon, LV_ALIGN_TOP_RIGHT, -18, 25);
  top_battery = make_label(main_layer, "--%", &lv_font_montserrat_32, DIM);
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
    activity_dots[i] = make_activity_dot(main_layer);
  }

  alert_layer = lv_obj_create(screen);
  lv_obj_set_size(alert_layer, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(alert_layer, 0, 0);
  lv_obj_set_style_bg_color(alert_layer, BG, 0);
  lv_obj_set_style_border_width(alert_layer, 0, 0);
  lv_obj_add_flag(alert_layer, LV_OBJ_FLAG_HIDDEN);

  alert_title = make_label(alert_layer, "任务完成！", alert_title_font(), TEXT);
  lv_obj_set_width(alert_title, ALERT_TEXT_W);
  lv_label_set_long_mode(alert_title, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(alert_title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(alert_title, ALERT_TITLE_X, ALERT_TITLE_Y);
  alert_body = make_label(alert_layer, "", body_font(), TEXT);
  lv_obj_set_size(
      alert_body, ALERT_TEXT_W, body_font()->line_height * ALERT_BODY_LINES);
  lv_label_set_long_mode(alert_body, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(alert_body, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(alert_body, ALERT_BODY_X, ALERT_BODY_Y);

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
  bool restart_anim = activity_visible_count == 0 && count > 0;
  uint32_t now = millis();
  if (restart_anim) {
    activity_anim_started_ms = now;
    activity_anim_last_ms = 0;
  }

  int total_w = count > 0
                    ? count * ACTIVITY_DOT_SIZE + (count - 1) * ACTIVITY_DOT_GAP
                    : 0;
  int start_x = (CODEXMETER_SCREEN_W - total_w) / 2;

  for (int i = 0; i < ACTIVITY_MAX_DOTS; i++) {
    if (i < count) {
      bool dot_newly_visible =
          restart_anim || i >= activity_visible_count ||
          activity_dot_started_ms[i] == 0;
      lv_obj_set_pos(
          activity_dots[i],
          start_x + i * (ACTIVITY_DOT_SIZE + ACTIVITY_DOT_GAP),
          ACTIVITY_DOT_Y);
      if (dot_newly_visible) {
        activity_dot_started_ms[i] = now;
        uint32_t elapsed_ms = now - activity_anim_started_ms;
        lv_obj_set_style_bg_color(
            activity_dots[i], activity_color_at(elapsed_ms), 0);
        lv_obj_set_style_bg_opa(activity_dots[i], LV_OPA_COVER, 0);
      }
      lv_obj_clear_flag(activity_dots[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(activity_dots[i], LV_OBJ_FLAG_HIDDEN);
      activity_dot_started_ms[i] = 0;
    }
  }

  activity_visible_count = count;
  if (count == 0) {
    activity_anim_started_ms = 0;
    activity_anim_last_ms = 0;
  }
}

void ui_show_alert(const AlertModel& alert) {
  device_logf(
      "INFO", "alert show title_b=%u body_b=%u",
      (unsigned int)text_len(alert.title), (unsigned int)text_len(alert.body));
  log_alert_diagnostics("alert_before");
  alert_active = true;
  alert_reveal_pending = false;
  alert_started_ms = millis();
  flash_started_ms = millis();
  alert_reveal_started_ms = 0;
  flash_step = 0;
  lv_obj_clear_flag(alert_layer, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(alert_title, alert.title);
  lv_label_set_text(alert_body, alert.body);
  log_alert_diagnostics("alert_text");
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
  tick_burn_in_drift(now);
  tick_activity_dots(now);

  if (brightness_active &&
      now - brightness_started_ms >= CODEXMETER_BRIGHTNESS_OVERLAY_MS) {
    brightness_active = false;
    lv_obj_add_flag(brightness_layer, LV_OBJ_FLAG_HIDDEN);
  }

  if (!alert_active) return;

  if (alert_reveal_pending &&
      now - alert_reveal_started_ms >= CODEXMETER_ALERT_REVEAL_DELAY_MS) {
    reveal_alert_text(now);
  }

  if (flash_step >= 0 && now - flash_started_ms >= CODEXMETER_FLASH_STEP_MS) {
    flash_step++;
    flash_started_ms = now;
    if (flash_step >= CODEXMETER_FLASH_STEPS) {
      flash_step = -1;
      alert_reveal_pending = true;
      alert_reveal_started_ms = now;
      lv_obj_set_style_bg_color(alert_layer, BG, 0);
      lv_obj_invalidate(alert_layer);
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

  if (!alert_reveal_pending && now - alert_started_ms >= CODEXMETER_ALERT_HOLD_MS) {
    ui_dismiss_alert();
  }
}

void ui_dismiss_alert() {
  device_logf("INFO", "alert dismiss");
  log_alert_diagnostics("alert_dismiss");
  alert_active = false;
  alert_reveal_pending = false;
  flash_step = -1;
  lv_obj_add_flag(alert_layer, LV_OBJ_FLAG_HIDDEN);
}

bool ui_alert_visible() {
  return alert_active;
}
