#include "ui.h"

#include <esp_heap_caps.h>
#include <lvgl.h>
#include <string.h>

#include "codexmeter_montserrat_medium_ttf.inc"
#include "config.h"
#include "dashboard_view_model.h"
#include "device_log.h"
#include "device_settings.h"
#include "theme_registry.h"
#include "theme_rotation.h"
#include "theme_runtime.h"

LV_FONT_DECLARE(codexmeter_font_30);
LV_FONT_DECLARE(codexmeter_percent_64);

namespace {

constexpr int ALERT_TEXT_W = CODEXMETER_SCREEN_W - 64;
constexpr int ALERT_TEXT_X = 32;
constexpr int ALERT_TITLE_X =
    ALERT_TEXT_X + CODEXMETER_ALERT_TITLE_VISUAL_OFFSET_X;
constexpr int ALERT_BODY_X =
    ALERT_TEXT_X + CODEXMETER_ALERT_BODY_VISUAL_OFFSET_X;
constexpr int ALERT_TITLE_Y = 126;
constexpr int ALERT_BODY_Y = 190;
constexpr int ALERT_BODY_LINES = 4;
constexpr int BRIGHTNESS_LAYER_W = 300;
constexpr int BRIGHTNESS_LAYER_H = 104;
constexpr int SETTINGS_ITEM_COUNT = 6;

const lv_color_t BG = lv_color_hex(0x0f1115);
const lv_color_t PANEL = lv_color_hex(0x1b1f2a);
const lv_color_t TEXT = lv_color_hex(0xf6f7fb);
const lv_color_t DIM = lv_color_hex(0x8d96a8);
const lv_color_t CODEX_BLUE = lv_color_hex(0x18a8ff);
const lv_color_t EDIT_YELLOW = lv_color_hex(0xffd43b);

enum class SettingItem : uint8_t {
  Theme = 0,
  Brightness = 1,
  Volume = 2,
  AutoTheme = 3,
  AutoInterval = 4,
  Exit = 5,
};

struct DriftOffset {
  int8_t x;
  int8_t y;
};

const DriftOffset DRIFT_OFFSETS[] = {
    {0, 0},  {1, 0},   {-1, 1}, {2, -1}, {-2, 0}, {1, 2},
    {0, -2}, {-1, -1}, {2, 1},  {-2, 2}, {1, -2}, {-2, -1},
};

const char* const SETTING_LABELS[SETTINGS_ITEM_COUNT] = {
    "主题",
    "屏幕亮度",
    "音量",
    "自动换肤",
    "切换间隔",
    "退出设置",
};

lv_obj_t* screen = nullptr;
lv_obj_t* theme_host = nullptr;
lv_obj_t* alert_layer = nullptr;
lv_obj_t* alert_title = nullptr;
lv_obj_t* alert_body = nullptr;
lv_obj_t* brightness_layer = nullptr;
lv_obj_t* brightness_bar = nullptr;
lv_obj_t* brightness_pct = nullptr;
lv_obj_t* theme_toast = nullptr;
lv_obj_t* theme_toast_label = nullptr;
lv_obj_t* settings_layer = nullptr;
lv_obj_t* settings_mode_label = nullptr;
lv_obj_t* settings_rows[SETTINGS_ITEM_COUNT]{};
lv_obj_t* settings_values[SETTINGS_ITEM_COUNT]{};
lv_font_t* alert_title_ttf = nullptr;
lv_font_t* alert_body_ttf = nullptr;
lv_font_t* token_value_ttf = nullptr;

UiSystemHooks system_hooks{};
ThemeRuntime theme_runtime;
ThemeRotationPolicy rotation_policy;
UsageModel app_usage;
ActivityModel app_activity;
DashboardViewModel dashboard_model;
int app_battery_percent = -1;
bool app_charging = false;

bool display_active = true;
bool alert_active = false;
bool alert_reveal_pending = false;
bool brightness_active = false;
bool toast_active = false;
bool settings_active = false;
bool settings_editing = false;
bool rotation_eligible = false;
bool pending_theme_change = false;
bool pending_theme_persist = false;
char pending_theme_id[24]{};

uint32_t alert_started_ms = 0;
uint32_t flash_started_ms = 0;
uint32_t alert_reveal_started_ms = 0;
uint32_t brightness_started_ms = 0;
uint32_t toast_started_ms = 0;
uint32_t settings_last_input_ms = 0;
uint32_t drift_last_ms = 0;
uint32_t dashboard_last_refresh_ms = 0;
uint8_t drift_index = 0;
int flash_step = -1;
int settings_selected = 0;

char edit_original_theme[24]{};
int edit_original_brightness = CODEXMETER_BRIGHTNESS_DEFAULT;
int edit_original_volume = CODEXMETER_VOLUME_DEFAULT;
uint16_t edit_original_interval =
    CODEXMETER_THEME_AUTO_DEFAULT_MINUTES;

const lv_font_t* ui_font() {
  return &codexmeter_font_30;
}

const lv_font_t* percent_font() {
  return &codexmeter_percent_64;
}

const lv_font_t* body_font() {
  return alert_body_ttf ? alert_body_ttf : ui_font();
}

const lv_font_t* title_font() {
  return alert_title_ttf ? alert_title_ttf : ui_font();
}

const lv_font_t* token_font() {
  return token_value_ttf ? token_value_ttf : &lv_font_montserrat_48;
}

size_t text_len(const char* text) {
  return text ? strlen(text) : 0;
}

void strip_obj(lv_obj_t* obj) {
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

void base_style(lv_obj_t* obj) {
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_color(obj, BG, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(obj, TEXT, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
}

lv_obj_t* make_label(
    lv_obj_t* parent, const char* text, const lv_font_t* font,
    lv_color_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_letter_space(label, 0, 0);
  return label;
}

void log_lvgl_mem(const char* phase) {
  lv_mem_monitor_t mon;
  lv_mem_monitor(&mon);
  device_logf(
      "INFO", "lv_mem %s t=%lu f=%lu b=%lu u=%u g=%u",
      phase && phase[0] ? phase : "-", (unsigned long)mon.total_size,
      (unsigned long)mon.free_size, (unsigned long)mon.free_biggest_size,
      mon.used_pct, mon.frag_pct);
}

void log_alert_diagnostics(const char* phase) {
#if CODEXMETER_ALERT_DIAGNOSTICS
  device_log_heap(phase);
  log_lvgl_mem(phase);
#else
  (void)phase;
#endif
}

void init_dynamic_fonts() {
#if LV_USE_TINY_TTF
  alert_title_ttf = lv_tiny_ttf_create_file_ex(
      CODEXMETER_TTF_FONT_PATH, CODEXMETER_TTF_TITLE_SIZE,
      LV_FONT_KERNING_NONE, CODEXMETER_TTF_CACHE_GLYPHS);
  alert_body_ttf = lv_tiny_ttf_create_file_ex(
      CODEXMETER_TTF_FONT_PATH, CODEXMETER_TTF_ALERT_SIZE,
      LV_FONT_KERNING_NONE, CODEXMETER_TTF_CACHE_GLYPHS);
  token_value_ttf = lv_tiny_ttf_create_data_ex(
      codexmeter_montserrat_medium_ttf,
      codexmeter_montserrat_medium_ttf_len,
      CODEXMETER_TTF_TOKEN_SIZE, LV_FONT_KERNING_NONE,
      CODEXMETER_TTF_TOKEN_CACHE_GLYPHS);
  if (alert_title_ttf) alert_title_ttf->fallback = ui_font();
  if (alert_body_ttf) alert_body_ttf->fallback = ui_font();
  if (token_value_ttf) {
    token_value_ttf->fallback = &lv_font_montserrat_48;
  }
  if (alert_title_ttf && alert_body_ttf && token_value_ttf) {
    device_logf("INFO", "TinyTTF font ready %s", CODEXMETER_TTF_FONT_PATH);
    device_log_heap("ttf_ready");
    log_lvgl_mem("ttf_ready");
  } else {
    device_logf("WARN", "TinyTTF font unavailable %s", CODEXMETER_TTF_FONT_PATH);
  }
#else
  device_logf("WARN", "TinyTTF disabled");
#endif
}

void refresh_dashboard_model(uint32_t now_ms) {
  dashboard_model = build_dashboard_view_model(
      app_usage, app_activity, app_battery_percent,
      app_charging, now_ms);
  dashboard_last_refresh_ms = now_ms;
  theme_runtime.update(dashboard_model);
}

void layout_alert_text() {
  lv_obj_set_width(alert_title, ALERT_TEXT_W);
  lv_obj_set_pos(alert_title, ALERT_TITLE_X, ALERT_TITLE_Y);
  lv_obj_set_size(
      alert_body, ALERT_TEXT_W,
      body_font()->line_height * ALERT_BODY_LINES);
  lv_obj_set_pos(alert_body, ALERT_BODY_X, ALERT_BODY_Y);
}

void reveal_alert_text(uint32_t now) {
  alert_reveal_pending = false;
  alert_started_ms = now;
  lv_obj_clear_flag(alert_title, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(alert_body, LV_OBJ_FLAG_HIDDEN);
  layout_alert_text();
  device_logf("INFO", "alert reveal");
  log_alert_diagnostics("alert_reveal");
}

void make_alert_layer() {
  alert_layer = lv_obj_create(screen);
  lv_obj_set_size(
      alert_layer, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(alert_layer, 0, 0);
  lv_obj_set_style_bg_color(alert_layer, BG, 0);
  lv_obj_set_style_border_width(alert_layer, 0, 0);
  lv_obj_add_flag(alert_layer, LV_OBJ_FLAG_HIDDEN);

  alert_title = make_label(
      alert_layer, "任务完成！", title_font(), TEXT);
  lv_obj_set_width(alert_title, ALERT_TEXT_W);
  lv_label_set_long_mode(alert_title, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(alert_title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(alert_title, ALERT_TITLE_X, ALERT_TITLE_Y);

  alert_body = make_label(alert_layer, "", body_font(), TEXT);
  lv_obj_set_size(
      alert_body, ALERT_TEXT_W,
      body_font()->line_height * ALERT_BODY_LINES);
  lv_label_set_long_mode(alert_body, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(alert_body, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(alert_body, ALERT_BODY_X, ALERT_BODY_Y);
}

void make_brightness_overlay() {
  brightness_layer = lv_obj_create(screen);
  lv_obj_remove_flag(brightness_layer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(
      brightness_layer, BRIGHTNESS_LAYER_W, BRIGHTNESS_LAYER_H);
  lv_obj_align(brightness_layer, LV_ALIGN_CENTER, 0, 24);
  lv_obj_set_style_radius(brightness_layer, 8, 0);
  lv_obj_set_style_pad_all(brightness_layer, 0, 0);
  lv_obj_set_style_bg_color(
      brightness_layer, lv_color_hex(0x151923), 0);
  lv_obj_set_style_bg_opa(brightness_layer, LV_OPA_90, 0);
  lv_obj_set_style_border_width(brightness_layer, 1, 0);
  lv_obj_set_style_border_color(
      brightness_layer, lv_color_hex(0x303747), 0);
  lv_obj_set_style_shadow_width(brightness_layer, 18, 0);
  lv_obj_set_style_shadow_color(
      brightness_layer, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_opa(brightness_layer, LV_OPA_40, 0);

  brightness_pct = make_label(
      brightness_layer, "", &lv_font_montserrat_32, TEXT);
  lv_label_set_text(
      brightness_pct,
      (String(device_settings_get().brightness_percent) + "%").c_str());
  lv_obj_align(brightness_pct, LV_ALIGN_TOP_MID, 0, 12);

  brightness_bar = lv_bar_create(brightness_layer);
  lv_obj_set_size(brightness_bar, 238, 13);
  lv_obj_align(brightness_bar, LV_ALIGN_BOTTOM_MID, 0, -24);
  lv_bar_set_range(brightness_bar, 0, 100);
  lv_bar_set_value(
      brightness_bar, device_settings_get().brightness_percent,
      LV_ANIM_OFF);
  lv_obj_set_style_radius(
      brightness_bar, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(
      brightness_bar, lv_color_hex(0x303747), 0);
  lv_obj_set_style_bg_opa(brightness_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(
      brightness_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(
      brightness_bar, CODEX_BLUE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(
      brightness_bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_add_flag(brightness_layer, LV_OBJ_FLAG_HIDDEN);
}

void make_theme_toast() {
  theme_toast = lv_obj_create(screen);
  lv_obj_remove_flag(theme_toast, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(theme_toast, 260, 62);
  lv_obj_align(theme_toast, LV_ALIGN_BOTTOM_MID, 0, -24);
  lv_obj_set_style_radius(theme_toast, 8, 0);
  lv_obj_set_style_bg_color(theme_toast, lv_color_hex(0x151923), 0);
  lv_obj_set_style_bg_opa(theme_toast, LV_OPA_90, 0);
  lv_obj_set_style_border_width(theme_toast, 1, 0);
  lv_obj_set_style_border_color(theme_toast, CODEX_BLUE, 0);
  theme_toast_label = make_label(
      theme_toast, "", &lv_font_montserrat_24, TEXT);
  lv_obj_center(theme_toast_label);
  lv_obj_add_flag(theme_toast, LV_OBJ_FLAG_HIDDEN);
}

void make_settings_layer() {
  settings_layer = lv_obj_create(screen);
  lv_obj_set_size(
      settings_layer, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(settings_layer, 0, 0);
  lv_obj_set_style_bg_color(settings_layer, BG, 0);
  lv_obj_set_style_bg_opa(settings_layer, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(settings_layer, 0, 0);
  lv_obj_set_style_pad_all(settings_layer, 0, 0);
  lv_obj_remove_flag(settings_layer, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = make_label(
      settings_layer, "设置", title_font(), TEXT);
  lv_obj_set_pos(title, 24, 18);
  settings_mode_label = make_label(
      settings_layer, "BROWSE", &lv_font_montserrat_14, CODEX_BLUE);
  lv_obj_align(settings_mode_label, LV_ALIGN_TOP_RIGHT, -24, 29);

  for (int i = 0; i < SETTINGS_ITEM_COUNT; ++i) {
    lv_obj_t* row = lv_obj_create(settings_layer);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, 440, 50);
    lv_obj_set_pos(row, 20, 66 + i * 58);
    lv_obj_set_style_radius(row, 7, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_bg_color(row, PANEL, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(0x303747), 0);
    settings_rows[i] = row;

    lv_obj_t* label = make_label(
        row, SETTING_LABELS[i], title_font(), TEXT);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 16, 0);
    settings_values[i] = make_label(
        row, "", &lv_font_montserrat_24, DIM);
    lv_obj_align(settings_values[i], LV_ALIGN_RIGHT_MID, -16, 0);
  }

  lv_obj_t* hint = make_label(
      settings_layer, "L/R SELECT  •  MID OK  •  HOLD POWER",
      &lv_font_montserrat_14, DIM);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_flag(settings_layer, LV_OBJ_FLAG_HIDDEN);
}

int clamp_percent(int value, int minimum = 0) {
  if (value < minimum) return minimum;
  if (value > 100) return 100;
  return value;
}

int clamp_drift_offset(int value) {
  int limit = CODEXMETER_BURN_IN_DRIFT_MAX_PX;
  if (theme_runtime.drift_margin_px() < limit) {
    limit = theme_runtime.drift_margin_px();
  }
  if (value < -limit) {
    return -limit;
  }
  if (value > limit) {
    return limit;
  }
  return value;
}

void apply_burn_in_drift(const DriftOffset& offset) {
  if (!theme_host) return;
  int x = clamp_drift_offset(offset.x);
  int y = clamp_drift_offset(offset.y);
  lv_obj_set_pos(theme_host, x, y);
  lv_obj_invalidate(screen);
  device_logf("INFO", "burn_in_drift x=%d y=%d", x, y);
}

void tick_burn_in_drift(uint32_t now) {
#if CODEXMETER_BURN_IN_DRIFT_INTERVAL_MS > 0 && CODEXMETER_BURN_IN_DRIFT_MAX_PX > 0
  if (!theme_host || alert_active || settings_active || !display_active) {
    return;
  }
  if (drift_last_ms == 0) {
    drift_last_ms = now;
    return;
  }
  if (now - drift_last_ms < CODEXMETER_BURN_IN_DRIFT_INTERVAL_MS) {
    return;
  }
  drift_last_ms = now;
  drift_index =
      (drift_index + 1) %
      (sizeof(DRIFT_OFFSETS) / sizeof(DRIFT_OFFSETS[0]));
  apply_burn_in_drift(DRIFT_OFFSETS[drift_index]);
#else
  (void)now;
#endif
}

void show_theme_toast() {
  if (settings_active || !display_active) return;
  lv_label_set_text(theme_toast_label, theme_runtime.current_name());
  lv_obj_clear_flag(theme_toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(theme_toast, -1);
  toast_active = true;
  toast_started_ms = millis();
}

bool perform_theme_switch(
    const char* id, bool persist, bool show_toast) {
  if (!id || !id[0]) return false;
  const char* current = theme_runtime.current_id();
  if (current && strcmp(current, id) == 0) {
    if (persist) device_settings_set_theme(id);
    rotation_policy.reset(millis());
    return true;
  }

  lv_obj_add_flag(theme_host, LV_OBJ_FLAG_HIDDEN);
  bool exact_switch = theme_runtime.switch_to(id);
  lv_obj_clear_flag(theme_host, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(theme_host, 0, 0);
  drift_index = 0;
  drift_last_ms = millis();
  if (!theme_runtime.ready()) return false;

  if (persist && exact_switch) {
    device_settings_set_theme(theme_runtime.current_id());
  }
  rotation_policy.reset(millis());
  lv_obj_invalidate(screen);
  if (show_toast && exact_switch) show_theme_toast();
  return exact_switch;
}

void apply_pending_theme_change() {
  if (!pending_theme_change || alert_active || settings_active) return;
  char id[sizeof(pending_theme_id)];
  strlcpy(id, pending_theme_id, sizeof(id));
  bool persist = pending_theme_persist;
  pending_theme_change = false;
  pending_theme_persist = false;
  pending_theme_id[0] = '\0';
  perform_theme_switch(id, persist, true);
}

void refresh_settings_rows() {
  const DeviceSettings& settings = device_settings_get();
  lv_label_set_text(
      settings_values[static_cast<int>(SettingItem::Theme)],
      theme_runtime.current_name());
  lv_label_set_text(
      settings_values[static_cast<int>(SettingItem::Brightness)],
      (String(settings.brightness_percent) + "%").c_str());
  lv_label_set_text(
      settings_values[static_cast<int>(SettingItem::Volume)],
      (String(settings.volume_percent) + "%").c_str());
  lv_label_set_text(
      settings_values[static_cast<int>(SettingItem::AutoTheme)],
      settings.auto_theme_enabled ? "ON" : "OFF");
  lv_label_set_text(
      settings_values[static_cast<int>(SettingItem::AutoInterval)],
      (String(settings.auto_theme_interval_minutes) + " min").c_str());
  lv_label_set_text(
      settings_values[static_cast<int>(SettingItem::Exit)], "OK");

  for (int i = 0; i < SETTINGS_ITEM_COUNT; ++i) {
    bool selected = i == settings_selected;
    lv_obj_set_style_bg_color(
        settings_rows[i],
        selected ? lv_color_hex(0x202838) : PANEL, 0);
    lv_obj_set_style_border_width(settings_rows[i], selected ? 2 : 1, 0);
    lv_obj_set_style_border_color(
        settings_rows[i],
        selected ? (settings_editing ? EDIT_YELLOW : CODEX_BLUE)
                 : lv_color_hex(0x303747),
        0);
    lv_obj_set_style_text_color(
        settings_values[i],
        selected ? TEXT : DIM, 0);
  }
  lv_label_set_text(
      settings_mode_label, settings_editing ? "EDIT" : "BROWSE");
  lv_obj_set_style_text_color(
      settings_mode_label,
      settings_editing ? EDIT_YELLOW : CODEX_BLUE, 0);
}

void note_settings_input() {
  settings_last_input_ms = millis();
}

void begin_edit() {
  const DeviceSettings& settings = device_settings_get();
  strlcpy(
      edit_original_theme, theme_runtime.current_id(),
      sizeof(edit_original_theme));
  edit_original_brightness = settings.brightness_percent;
  edit_original_volume = settings.volume_percent;
  edit_original_interval = settings.auto_theme_interval_minutes;
  settings_editing = true;
  refresh_settings_rows();
}

void confirm_edit() {
  if (!settings_editing) return;
  if (static_cast<SettingItem>(settings_selected) == SettingItem::Theme) {
    device_settings_set_theme(theme_runtime.current_id());
  }
  settings_editing = false;
  rotation_policy.reset(millis());
  refresh_settings_rows();
}

void cancel_edit() {
  if (!settings_editing) return;
  SettingItem item = static_cast<SettingItem>(settings_selected);
  switch (item) {
    case SettingItem::Theme:
      perform_theme_switch(edit_original_theme, false, false);
      break;
    case SettingItem::Brightness:
      device_settings_set_brightness(edit_original_brightness);
      if (system_hooks.apply_brightness) {
        system_hooks.apply_brightness(edit_original_brightness);
      }
      break;
    case SettingItem::Volume:
      device_settings_set_volume(edit_original_volume);
      break;
    case SettingItem::AutoInterval:
      device_settings_set_auto_theme_interval(edit_original_interval);
      rotation_policy.configure(
          device_settings_get().auto_theme_enabled,
          edit_original_interval, millis());
      break;
    default:
      break;
  }
  settings_editing = false;
}

void close_settings_internal(bool cancel_unconfirmed) {
  if (!settings_active) return;
  if (cancel_unconfirmed) cancel_edit();
  settings_editing = false;
  settings_active = false;
  lv_obj_add_flag(settings_layer, LV_OBJ_FLAG_HIDDEN);
  rotation_eligible = false;
  device_logf("INFO", "settings close");
  apply_pending_theme_change();
}

void open_settings() {
  if (!display_active) return;
  if (alert_active) ui_dismiss_alert();
  brightness_active = false;
  toast_active = false;
  lv_obj_add_flag(brightness_layer, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(theme_toast, LV_OBJ_FLAG_HIDDEN);
  settings_active = true;
  settings_editing = false;
  settings_selected = 0;
  note_settings_input();
  refresh_settings_rows();
  lv_obj_clear_flag(settings_layer, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(settings_layer, -1);
  device_logf("INFO", "settings open");
}

uint16_t adjusted_interval(uint16_t current, int direction) {
  if (direction < 0) {
    if (current <= 1) return 1;
    if (current <= 5) return 1;
    return current - 5;
  }
  if (current < 5) return 5;
  uint32_t next = static_cast<uint32_t>(current) + 5;
  if (next > CODEXMETER_THEME_AUTO_MAX_MINUTES) {
    next = CODEXMETER_THEME_AUTO_MAX_MINUTES;
  }
  return static_cast<uint16_t>(next);
}

void adjust_current_setting(int direction) {
  const DeviceSettings& settings = device_settings_get();
  SettingItem item = static_cast<SettingItem>(settings_selected);
  switch (item) {
    case SettingItem::Theme:
      ui_next_theme(direction, false);
      break;
    case SettingItem::Brightness: {
      int value = clamp_percent(
          static_cast<int>(settings.brightness_percent) +
              (direction < 0 ? -CODEXMETER_BRIGHTNESS_STEP
                             : CODEXMETER_BRIGHTNESS_STEP),
          CODEXMETER_BRIGHTNESS_MIN);
      device_settings_set_brightness(value);
      if (system_hooks.apply_brightness) {
        system_hooks.apply_brightness(value);
      }
      break;
    }
    case SettingItem::Volume: {
      int value = clamp_percent(
          static_cast<int>(settings.volume_percent) +
              (direction < 0 ? -10 : 10));
      device_settings_set_volume(value);
      break;
    }
    case SettingItem::AutoInterval: {
      uint16_t value = adjusted_interval(
          settings.auto_theme_interval_minutes, direction);
      device_settings_set_auto_theme_interval(value);
      rotation_policy.configure(
          settings.auto_theme_enabled, value, millis());
      break;
    }
    default:
      break;
  }
  refresh_settings_rows();
}

void select_current_setting() {
  SettingItem item = static_cast<SettingItem>(settings_selected);
  if (settings_editing) {
    confirm_edit();
    return;
  }

  switch (item) {
    case SettingItem::Theme:
    case SettingItem::Brightness:
    case SettingItem::Volume:
    case SettingItem::AutoInterval:
      begin_edit();
      break;
    case SettingItem::AutoTheme: {
      bool enabled = !device_settings_get().auto_theme_enabled;
      device_settings_set_auto_theme(enabled);
      rotation_policy.configure(
          enabled,
          device_settings_get().auto_theme_interval_minutes,
          millis());
      refresh_settings_rows();
      break;
    }
    case SettingItem::Exit:
      close_settings_internal(false);
      break;
  }
}

}  // namespace

void ui_init(const UiSystemHooks& hooks) {
  system_hooks = hooks;
  init_dynamic_fonts();
  if (!theme_registry_validate()) {
    device_logf("ERROR", "theme registry invalid");
  }

  screen = lv_screen_active();
  base_style(screen);

  theme_host = lv_obj_create(screen);
  strip_obj(theme_host);
  lv_obj_set_size(
      theme_host, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(theme_host, 0, 0);

  ThemeResources resources{
      ui_font(),
      percent_font(),
      title_font(),
      body_font(),
      token_font(),
  };
  theme_runtime.begin(theme_host, resources);
  refresh_dashboard_model(millis());
  theme_runtime.switch_to(device_settings_get().theme_id);

  make_alert_layer();
  make_brightness_overlay();
  make_theme_toast();
  make_settings_layer();

  rotation_policy.configure(
      device_settings_get().auto_theme_enabled,
      device_settings_get().auto_theme_interval_minutes,
      millis());
  device_logf(
      "INFO", "ui ready theme=%s auto=%d every=%umin",
      theme_runtime.current_id(),
      device_settings_get().auto_theme_enabled ? 1 : 0,
      device_settings_get().auto_theme_interval_minutes);
}

void ui_update_usage(const UsageModel& usage) {
  app_usage = usage;
  refresh_dashboard_model(millis());
}

void ui_update_activity(const ActivityModel& activity) {
  app_activity = activity;
  refresh_dashboard_model(millis());
}

void ui_show_alert(const AlertModel& alert) {
  close_settings_internal(true);
  brightness_active = false;
  toast_active = false;
  lv_obj_add_flag(brightness_layer, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(theme_toast, LV_OBJ_FLAG_HIDDEN);

  device_logf(
      "INFO", "alert show title_b=%u body_b=%u",
      (unsigned int)text_len(alert.title),
      (unsigned int)text_len(alert.body));
  log_alert_diagnostics("alert_before");
  alert_active = true;
  alert_reveal_pending = false;
  alert_started_ms = millis();
  flash_started_ms = millis();
  alert_reveal_started_ms = 0;
  flash_step = 0;
  lv_obj_clear_flag(alert_layer, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(alert_layer, -1);
  lv_label_set_text(alert_title, alert.title);
  lv_label_set_text(alert_body, alert.body);
  layout_alert_text();
  lv_obj_add_flag(alert_title, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(alert_body, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(
      alert_layer, lv_color_hex(0xff2d2d), 0);
}

void ui_set_battery(int pct, bool charging) {
  app_battery_percent = pct;
  app_charging = charging;
  refresh_dashboard_model(millis());
}

void ui_show_brightness(int pct) {
  if (settings_active) return;
  pct = clamp_percent(pct);
  lv_label_set_text(
      brightness_pct, (String(pct) + "%").c_str());
  lv_bar_set_value(brightness_bar, pct, LV_ANIM_ON);
  lv_obj_clear_flag(brightness_layer, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(brightness_layer, -1);
  brightness_active = true;
  brightness_started_ms = millis();
}

void ui_tick() {
  uint32_t now = millis();
  if (now - dashboard_last_refresh_ms >= 30000UL) {
    refresh_dashboard_model(now);
  }
  theme_runtime.tick(now);
  tick_burn_in_drift(now);

  if (brightness_active &&
      now - brightness_started_ms >=
          CODEXMETER_BRIGHTNESS_OVERLAY_MS) {
    brightness_active = false;
    lv_obj_add_flag(brightness_layer, LV_OBJ_FLAG_HIDDEN);
  }

  if (toast_active &&
      now - toast_started_ms >= CODEXMETER_THEME_TOAST_MS) {
    toast_active = false;
    lv_obj_add_flag(theme_toast, LV_OBJ_FLAG_HIDDEN);
  }

  if (settings_active &&
      now - settings_last_input_ms >=
          CODEXMETER_SETTINGS_IDLE_TIMEOUT_MS) {
    close_settings_internal(true);
  }

  bool eligible =
      display_active && !alert_active && !settings_active &&
      !brightness_active && !toast_active;
  if (eligible != rotation_eligible) {
    rotation_eligible = eligible;
    rotation_policy.set_eligible(eligible, now);
  }
  if (rotation_policy.tick(now)) {
    ui_next_theme(1, false);
  }

  if (!alert_active) {
    apply_pending_theme_change();
    return;
  }

  if (alert_reveal_pending &&
      now - alert_reveal_started_ms >=
          CODEXMETER_ALERT_REVEAL_DELAY_MS) {
    reveal_alert_text(now);
  }

  if (flash_step >= 0 &&
      now - flash_started_ms >= CODEXMETER_FLASH_STEP_MS) {
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
          lv_obj_set_style_bg_color(
              alert_layer, lv_color_hex(0xff2d2d), 0);
          break;
        case 1:
          lv_obj_set_style_bg_color(
              alert_layer, lv_color_hex(0xffd43b), 0);
          break;
        default:
          lv_obj_set_style_bg_color(
              alert_layer, lv_color_hex(0x2fda77), 0);
          break;
      }
    }
  }

  if (!alert_reveal_pending &&
      now - alert_started_ms >= CODEXMETER_ALERT_HOLD_MS) {
    ui_dismiss_alert();
  }
}

void ui_set_display_active(bool active) {
  display_active = active;
  rotation_eligible = false;
  rotation_policy.set_eligible(false, millis());
  if (!active) {
    close_settings_internal(true);
    if (alert_active) ui_dismiss_alert();
    brightness_active = false;
    toast_active = false;
    lv_obj_add_flag(brightness_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(theme_toast, LV_OBJ_FLAG_HIDDEN);
  }
}

void ui_middle_short_press() {
  if (!display_active) return;
  if (settings_active) {
    note_settings_input();
    select_current_setting();
  } else {
    open_settings();
  }
}

void ui_settings_move(int direction) {
  if (!settings_active || direction == 0) return;
  note_settings_input();
  direction = direction < 0 ? -1 : 1;
  if (settings_editing) {
    adjust_current_setting(direction);
    return;
  }
  settings_selected =
      (settings_selected + direction + SETTINGS_ITEM_COUNT) %
      SETTINGS_ITEM_COUNT;
  refresh_settings_rows();
}

bool ui_settings_visible() {
  return settings_active;
}

void ui_close_settings() {
  close_settings_internal(true);
}

void ui_dismiss_alert() {
  if (!alert_active) return;
  device_logf("INFO", "alert dismiss");
  log_alert_diagnostics("alert_dismiss");
  alert_active = false;
  alert_reveal_pending = false;
  flash_step = -1;
  lv_obj_add_flag(alert_layer, LV_OBJ_FLAG_HIDDEN);
  apply_pending_theme_change();
}

bool ui_alert_visible() {
  return alert_active;
}

bool ui_set_theme(const char* id, bool persist) {
  if (!id || !id[0]) return false;
  if (alert_active && !settings_active) {
    strlcpy(pending_theme_id, id, sizeof(pending_theme_id));
    pending_theme_change = true;
    pending_theme_persist = persist;
    return true;
  }
  return perform_theme_switch(id, persist, !settings_active);
}

bool ui_next_theme(int direction, bool persist) {
  const ThemePack* next =
      theme_registry_next(theme_runtime.current_id(), direction);
  if (!next) return false;
  if (strcmp(next->id, theme_runtime.current_id()) == 0) {
    rotation_policy.reset(millis());
    return true;
  }
  return ui_set_theme(next->id, persist);
}

const char* ui_theme_id() {
  return theme_runtime.current_id();
}

const char* ui_theme_name() {
  return theme_runtime.current_name();
}

void ui_set_auto_theme(bool enabled) {
  device_settings_set_auto_theme(enabled);
  rotation_policy.configure(
      enabled,
      device_settings_get().auto_theme_interval_minutes,
      millis());
  if (settings_active) refresh_settings_rows();
}

bool ui_auto_theme_enabled() {
  return device_settings_get().auto_theme_enabled;
}

void ui_set_auto_theme_interval(uint16_t minutes) {
  device_settings_set_auto_theme_interval(minutes);
  rotation_policy.configure(
      device_settings_get().auto_theme_enabled,
      device_settings_get().auto_theme_interval_minutes,
      millis());
  if (settings_active) refresh_settings_rows();
}

uint16_t ui_auto_theme_interval() {
  return device_settings_get().auto_theme_interval_minutes;
}

void ui_set_volume(int percent) {
  device_settings_set_volume(clamp_percent(percent));
  if (settings_active) refresh_settings_rows();
}

int ui_volume() {
  return device_settings_get().volume_percent;
}
