#include "three_body_theme.h"

#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "device_log.h"

namespace {

constexpr char BACKGROUND_PATH[] = "S:/themes/three_body_bg.rgb565";
constexpr char DISPLAY_FONT_PATH[] =
    "S:/fonts/BarlowCondensed-ExtraBold.ttf";
constexpr char STATUS_FONT_PATH[] =
    "S:/fonts/BarlowCondensed-Bold.ttf";
constexpr size_t BACKGROUND_BYTES =
    CODEXMETER_SCREEN_W * CODEXMETER_SCREEN_H * sizeof(uint16_t);

constexpr int VALUE_Y = 359;
constexpr int UNIT_Y = 374;
constexpr int CARD_CENTERS[] = {66, 181, 298, 416};
constexpr int VALUE_SCALE_Y = 180;
constexpr int UNIT_SCALE_X = 165;
constexpr int UNIT_SCALE_Y = 190;
constexpr int RESET_NUMBER_SCALE_X = 120;
constexpr int RESET_UNIT_SCALE_X = 115;
constexpr int RESET_UNIT_SCALE_Y = 155;
constexpr int FOOTER_TEXT_Y = 433;
constexpr int FOOTER_TEXT_SCALE_Y = 190;
constexpr int FOOTER_LABEL_SCALE_X = 135;

const lv_color_t VALUE_YELLOW = lv_color_hex(0xF6D60A);
const lv_color_t VALUE_WHITE = lv_color_hex(0xF3F1E8);
const lv_color_t STATE_CYAN = lv_color_hex(0x18C8F4);
const lv_color_t STATE_YELLOW = lv_color_hex(0xF6D60A);
const lv_color_t STATE_RED = lv_color_hex(0xF0644A);

const lv_point_precise_t BLE_SPINE_POINTS[] = {{4, 0}, {4, 18}};
const lv_point_precise_t BLE_ZIGZAG_POINTS[] = {
    {4, 0}, {10, 5}, {4, 9}, {10, 14}, {4, 18}};
const lv_point_precise_t BLE_CROSS_POINTS[] = {
    {0, 5}, {4, 9}, {0, 13}};

struct ThreeBodyThemeState {
  lv_obj_t* root = nullptr;
  lv_obj_t* background = nullptr;

  lv_obj_t* today_value = nullptr;
  lv_obj_t* today_unit = nullptr;
  lv_obj_t* week_value = nullptr;
  lv_obj_t* week_unit = nullptr;
  lv_obj_t* quota_value = nullptr;
  lv_obj_t* quota_unit = nullptr;
  lv_obj_t* reset_heading = nullptr;
  lv_obj_t* reset_day_value = nullptr;
  lv_obj_t* reset_day_unit = nullptr;
  lv_obj_t* reset_hour_value = nullptr;
  lv_obj_t* reset_hour_unit = nullptr;
  lv_obj_t* battery_icon = nullptr;
  lv_obj_t* battery_icon_cap = nullptr;
  lv_obj_t* battery_icon_fill = nullptr;
  lv_obj_t* battery_label = nullptr;
  lv_obj_t* battery_value = nullptr;
  lv_obj_t* battery_unit = nullptr;
  lv_obj_t* task_icon = nullptr;
  lv_obj_t* task_icon_tab = nullptr;
  lv_obj_t* task_icon_line_top = nullptr;
  lv_obj_t* task_icon_line_bottom = nullptr;
  lv_obj_t* task_count = nullptr;
  lv_obj_t* task_label = nullptr;
  lv_obj_t* sync_value = nullptr;

  lv_font_t* value_font_64 = nullptr;
  lv_font_t* unit_font_40 = nullptr;
  lv_font_t* status_font_30 = nullptr;

  uint8_t* background_pixels = nullptr;
  lv_image_dsc_t background_dsc{};
  ThemeResources resources{};

  int today_scale = -1;
  int week_scale = -1;
  int quota_scale = -1;
  int battery_scale = -1;
  int running_count = -1;
  DashboardDataState data_state = DashboardDataState::Waiting;
  bool data_state_initialized = false;
};

void strip_obj(lv_obj_t* obj) {
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

const lv_font_t* font_or(
    const lv_font_t* preferred, const lv_font_t* fallback) {
  return preferred ? preferred : fallback;
}

lv_obj_t* make_label(
    lv_obj_t* parent, const char* text, const lv_font_t* font,
    lv_color_t color, int x, int y) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_letter_space(label, 0, 0);
  lv_obj_set_style_text_outline_stroke_color(
      label, lv_color_hex(0x071323), 0);
  lv_obj_set_style_text_outline_stroke_width(label, 1, 0);
  lv_obj_set_style_text_outline_stroke_opa(label, LV_OPA_40, 0);
  lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  return label;
}

void set_scale(lv_obj_t* label, int scale_x, int scale_y);

lv_obj_t* make_footer_rect(
    lv_obj_t* parent, int x, int y, int width, int height,
    lv_color_t color, lv_opa_t opacity, int radius = 0) {
  lv_obj_t* rect = lv_obj_create(parent);
  strip_obj(rect);
  lv_obj_set_pos(rect, x, y);
  lv_obj_set_size(rect, width, height);
  lv_obj_set_style_bg_color(rect, color, 0);
  lv_obj_set_style_bg_opa(rect, opacity, 0);
  lv_obj_set_style_radius(rect, radius, 0);
  return rect;
}

void make_footer_chrome(
    ThreeBodyThemeState* state, lv_obj_t* parent,
    const lv_font_t* status_font) {
  for (int x : {10, 167, 324}) {
    lv_obj_t* capsule = make_footer_rect(
        parent, x, 428, 146, 37,
        lv_color_hex(0x020B16), LV_OPA_50, 18);
    lv_obj_set_style_border_color(
        capsule, lv_color_hex(0x087FBC), 0);
    lv_obj_set_style_border_width(capsule, 1, 0);
    lv_obj_set_style_shadow_color(
        capsule, lv_color_hex(0x006DA8), 0);
    lv_obj_set_style_shadow_width(capsule, 5, 0);
    lv_obj_set_style_shadow_opa(capsule, LV_OPA_20, 0);
  }

  make_footer_rect(
      parent, 84, 435, 1, 23,
      lv_color_hex(0x36566B), LV_OPA_80);
  make_footer_rect(
      parent, 398, 435, 1, 23,
      lv_color_hex(0x36566B), LV_OPA_80);

  lv_obj_t* ble_spine = lv_line_create(parent);
  lv_line_set_points(
      ble_spine, BLE_SPINE_POINTS,
      sizeof(BLE_SPINE_POINTS) / sizeof(BLE_SPINE_POINTS[0]));
  lv_obj_set_pos(ble_spine, 30, 437);
  lv_obj_set_style_line_width(ble_spine, 2, 0);
  lv_obj_set_style_line_color(ble_spine, STATE_CYAN, 0);

  lv_obj_t* ble_zigzag = lv_line_create(parent);
  lv_line_set_points(
      ble_zigzag, BLE_ZIGZAG_POINTS,
      sizeof(BLE_ZIGZAG_POINTS) / sizeof(BLE_ZIGZAG_POINTS[0]));
  lv_obj_set_pos(ble_zigzag, 30, 437);
  lv_obj_set_style_line_width(ble_zigzag, 2, 0);
  lv_obj_set_style_line_color(ble_zigzag, STATE_CYAN, 0);

  lv_obj_t* ble_cross = lv_line_create(parent);
  lv_line_set_points(
      ble_cross, BLE_CROSS_POINTS,
      sizeof(BLE_CROSS_POINTS) / sizeof(BLE_CROSS_POINTS[0]));
  lv_obj_set_pos(ble_cross, 30, 437);
  lv_obj_set_style_line_width(ble_cross, 2, 0);
  lv_obj_set_style_line_color(ble_cross, STATE_CYAN, 0);

  lv_obj_t* ble_label = make_label(
      parent, "BLE", status_font, VALUE_WHITE, 45, FOOTER_TEXT_Y);
  set_scale(
      ble_label, FOOTER_LABEL_SCALE_X, FOOTER_TEXT_SCALE_Y);

  state->task_icon = make_footer_rect(
      parent, 205, 439, 16, 18,
      lv_color_hex(0x020B16), LV_OPA_TRANSP, 2);
  lv_obj_set_style_border_color(state->task_icon, STATE_CYAN, 0);
  lv_obj_set_style_border_width(state->task_icon, 2, 0);
  state->task_icon_tab = make_footer_rect(
      parent, 209, 436, 8, 5,
      lv_color_hex(0x020B16), LV_OPA_COVER, 1);
  lv_obj_set_style_border_color(state->task_icon_tab, STATE_CYAN, 0);
  lv_obj_set_style_border_width(state->task_icon_tab, 2, 0);
  state->task_icon_line_top = make_footer_rect(
      parent, 209, 445, 8, 1, STATE_CYAN, LV_OPA_COVER);
  state->task_icon_line_bottom = make_footer_rect(
      parent, 209, 450, 8, 1, STATE_CYAN, LV_OPA_COVER);
}

void set_task_icon_x(ThreeBodyThemeState* state, int x) {
  lv_obj_set_x(state->task_icon, x);
  lv_obj_set_x(state->task_icon_tab, x + 4);
  lv_obj_set_x(state->task_icon_line_top, x + 4);
  lv_obj_set_x(state->task_icon_line_bottom, x + 4);
}

bool set_label_text_if_changed(lv_obj_t* label, const char* text) {
  const char* current = lv_label_get_text(label);
  if (current && strcmp(current, text) == 0) return false;
  lv_label_set_text(label, text);
  return true;
}

void set_scale(lv_obj_t* label, int scale_x, int scale_y) {
  lv_obj_set_style_transform_pivot_x(label, 0, 0);
  lv_obj_set_style_transform_pivot_y(label, 0, 0);
  lv_obj_set_style_transform_scale_x(label, scale_x, 0);
  lv_obj_set_style_transform_scale_y(label, scale_y, 0);
}

int scaled_label_width(lv_obj_t* label, int scale_x) {
  lv_obj_update_layout(label);
  return (lv_obj_get_width(label) * scale_x + 128) / 256;
}

int scaled_baseline_offset(lv_obj_t* label, int scale_y) {
  const lv_font_t* font =
      lv_obj_get_style_text_font(label, LV_PART_MAIN);
  int offset = font->line_height - font->base_line;
  return (offset * scale_y + 128) / 256;
}

void align_baseline(
    lv_obj_t* reference, int reference_scale_y,
    lv_obj_t* target, int target_scale_y) {
  lv_obj_update_layout(reference);
  int baseline =
      lv_obj_get_y(reference) +
      scaled_baseline_offset(reference, reference_scale_y);
  lv_obj_set_y(
      target,
      baseline - scaled_baseline_offset(target, target_scale_y));
}

void center_group(
    lv_obj_t* value, int value_scale_x,
    lv_obj_t* unit, int unit_scale_x,
    int center_x, int gap) {
  lv_obj_update_layout(value);
  lv_obj_update_layout(unit);
  int value_width = scaled_label_width(value, value_scale_x);
  bool has_unit = !lv_obj_has_flag(unit, LV_OBJ_FLAG_HIDDEN);
  int unit_width = has_unit ? scaled_label_width(unit, unit_scale_x) : 0;
  int total = value_width + (has_unit ? gap + unit_width : 0);
  int left = center_x - total / 2;
  lv_obj_set_x(value, left);
  if (has_unit) lv_obj_set_x(unit, left + value_width + gap);
}

void split_suffix(
    const char* source, char* value, size_t value_size,
    char* unit, size_t unit_size) {
  if (!source || !source[0] || strcmp(source, "--") == 0) {
    strlcpy(value, "--", value_size);
    unit[0] = '\0';
    return;
  }
  size_t length = strlen(source);
  char last = source[length - 1];
  bool has_unit = last >= 'A' && last <= 'Z';
  size_t value_length = has_unit ? length - 1 : length;
  if (value_length >= value_size) value_length = value_size - 1;
  memcpy(value, source, value_length);
  value[value_length] = '\0';
  if (has_unit && unit_size >= 2) {
    unit[0] = last;
    unit[1] = '\0';
  } else {
    unit[0] = '\0';
  }
}

void set_unit_text(lv_obj_t* label, const char* text) {
  set_label_text_if_changed(label, text);
  if (text && text[0]) {
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
  }
}

lv_font_t* create_font(
    const char* path, int size,
    const lv_font_t* fallback, size_t cache_size) {
#if LV_USE_TINY_TTF && LV_TINY_TTF_FILE_SUPPORT
  lv_font_t* font = lv_tiny_ttf_create_file_ex(
      path, size, LV_FONT_KERNING_NORMAL, cache_size);
  if (font) font->fallback = fallback;
  return font;
#else
  (void)path;
  (void)size;
  (void)fallback;
  (void)cache_size;
  return nullptr;
#endif
}

void create_fonts(ThreeBodyThemeState* state) {
  state->value_font_64 = create_font(
      DISPLAY_FONT_PATH, 64,
      font_or(state->resources.token_font, &lv_font_montserrat_48), 20);
  state->unit_font_40 = create_font(
      DISPLAY_FONT_PATH, 40, &lv_font_montserrat_32, 16);
  state->status_font_30 = create_font(
      STATUS_FONT_PATH, 30, &lv_font_montserrat_24, 28);
}

bool load_background(ThreeBodyThemeState* state) {
  state->background_pixels = static_cast<uint8_t*>(
      heap_caps_malloc(
          BACKGROUND_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!state->background_pixels) {
    device_logf(
        "ERROR", "three_body background alloc failed bytes=%lu",
        static_cast<unsigned long>(BACKGROUND_BYTES));
    return false;
  }

  lv_fs_file_t file;
  lv_fs_res_t result =
      lv_fs_open(&file, BACKGROUND_PATH, LV_FS_MODE_RD);
  if (result != LV_FS_RES_OK) {
    device_logf(
        "ERROR", "three_body background open failed path=%s res=%d",
        BACKGROUND_PATH, static_cast<int>(result));
    heap_caps_free(state->background_pixels);
    state->background_pixels = nullptr;
    return false;
  }

  size_t total = 0;
  while (total < BACKGROUND_BYTES) {
    uint32_t bytes_read = 0;
    uint32_t request = static_cast<uint32_t>(
        BACKGROUND_BYTES - total > 16384
            ? 16384
            : BACKGROUND_BYTES - total);
    result = lv_fs_read(
        &file, state->background_pixels + total,
        request, &bytes_read);
    if (result != LV_FS_RES_OK || bytes_read == 0) break;
    total += bytes_read;
  }
  lv_fs_close(&file);
  if (result != LV_FS_RES_OK || total != BACKGROUND_BYTES) {
    device_logf(
        "ERROR",
        "three_body background read failed bytes=%lu expected=%lu res=%d",
        static_cast<unsigned long>(total),
        static_cast<unsigned long>(BACKGROUND_BYTES),
        static_cast<int>(result));
    heap_caps_free(state->background_pixels);
    state->background_pixels = nullptr;
    return false;
  }

  state->background_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  state->background_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
  state->background_dsc.header.flags = 0;
  state->background_dsc.header.w = CODEXMETER_SCREEN_W;
  state->background_dsc.header.h = CODEXMETER_SCREEN_H;
  state->background_dsc.header.stride =
      CODEXMETER_SCREEN_W * sizeof(uint16_t);
  state->background_dsc.data_size = BACKGROUND_BYTES;
  state->background_dsc.data = state->background_pixels;
  state->background_dsc.reserved = nullptr;
  state->background_dsc.reserved_2 = nullptr;
  return true;
}

void make_dynamic_layer(ThreeBodyThemeState* state) {
  const lv_font_t* value_font =
      font_or(state->value_font_64, &lv_font_montserrat_48);
  const lv_font_t* unit_font =
      font_or(state->unit_font_40, &lv_font_montserrat_32);
  const lv_font_t* status_font =
      font_or(state->status_font_30, &lv_font_montserrat_24);

  make_footer_chrome(state, state->root, status_font);

  state->today_value = make_label(
      state->root, "--", value_font, VALUE_YELLOW, 0, VALUE_Y);
  state->today_unit = make_label(
      state->root, "", unit_font, VALUE_WHITE, 0, UNIT_Y);
  state->week_value = make_label(
      state->root, "--", value_font, VALUE_YELLOW, 0, VALUE_Y);
  state->week_unit = make_label(
      state->root, "", unit_font, VALUE_WHITE, 0, UNIT_Y);
  state->quota_value = make_label(
      state->root, "--", value_font, VALUE_YELLOW, 0, VALUE_Y);
  state->quota_unit = make_label(
      state->root, "%", unit_font, VALUE_WHITE, 0, UNIT_Y);

  state->reset_heading = make_label(
      state->root, "RESET", status_font, VALUE_WHITE, 0, 347);
  set_scale(state->reset_heading, 110, 150);
  int reset_heading_width =
      scaled_label_width(state->reset_heading, 110);
  lv_obj_set_x(
      state->reset_heading,
      CARD_CENTERS[3] - reset_heading_width / 2);
  state->reset_day_value = make_label(
      state->root, "--", value_font,
      VALUE_YELLOW, 0, 359);
  state->reset_day_unit = make_label(
      state->root, "D", unit_font,
      VALUE_WHITE, 0, UNIT_Y);
  state->reset_hour_value = make_label(
      state->root, "--", value_font,
      VALUE_YELLOW, 0, 359);
  state->reset_hour_unit = make_label(
      state->root, "H", unit_font,
      VALUE_WHITE, 0, UNIT_Y);
  for (lv_obj_t* label : {
       state->reset_day_value, state->reset_hour_value}) {
    set_scale(label, RESET_NUMBER_SCALE_X, VALUE_SCALE_Y);
  }
  for (lv_obj_t* label : {
       state->reset_day_unit, state->reset_hour_unit}) {
    set_scale(label, RESET_UNIT_SCALE_X, RESET_UNIT_SCALE_Y);
  }

  state->battery_icon = lv_obj_create(state->root);
  strip_obj(state->battery_icon);
  lv_obj_set_size(state->battery_icon, 19, 13);
  lv_obj_set_pos(state->battery_icon, 338, 440);
  lv_obj_set_style_bg_opa(state->battery_icon, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(state->battery_icon, STATE_CYAN, 0);
  lv_obj_set_style_border_width(state->battery_icon, 2, 0);
  lv_obj_set_style_radius(state->battery_icon, 2, 0);

  state->battery_icon_fill = lv_obj_create(state->battery_icon);
  strip_obj(state->battery_icon_fill);
  lv_obj_set_size(state->battery_icon_fill, 13, 7);
  lv_obj_set_pos(state->battery_icon_fill, 3, 3);
  lv_obj_set_style_bg_color(state->battery_icon_fill, STATE_CYAN, 0);
  lv_obj_set_style_bg_opa(state->battery_icon_fill, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(state->battery_icon_fill, 1, 0);

  state->battery_icon_cap = lv_obj_create(state->root);
  strip_obj(state->battery_icon_cap);
  lv_obj_set_size(state->battery_icon_cap, 3, 7);
  lv_obj_set_pos(state->battery_icon_cap, 358, 443);
  lv_obj_set_style_bg_color(state->battery_icon_cap, STATE_CYAN, 0);
  lv_obj_set_style_bg_opa(state->battery_icon_cap, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(state->battery_icon_cap, 1, 0);

  state->battery_label = make_label(
      state->root, "BAT", status_font,
      VALUE_WHITE, 364, FOOTER_TEXT_Y);
  set_scale(
      state->battery_label,
      FOOTER_LABEL_SCALE_X, FOOTER_TEXT_SCALE_Y);
  state->battery_value = make_label(
      state->root, "--", status_font,
      VALUE_YELLOW, 0, FOOTER_TEXT_Y);
  state->battery_unit = make_label(
      state->root, "%", status_font,
      VALUE_WHITE, 0, FOOTER_TEXT_Y);

  for (lv_obj_t* label : {
       state->today_value, state->week_value,
       state->quota_value}) {
    set_scale(label, 210, VALUE_SCALE_Y);
  }
  for (lv_obj_t* label : {
       state->today_unit, state->week_unit,
       state->quota_unit}) {
    set_scale(label, UNIT_SCALE_X, UNIT_SCALE_Y);
  }

  set_scale(
      state->battery_value, 160, FOOTER_TEXT_SCALE_Y);
  set_scale(
      state->battery_unit,
      FOOTER_LABEL_SCALE_X, FOOTER_TEXT_SCALE_Y);

  state->task_count = make_label(
      state->root, "", status_font,
      VALUE_YELLOW, 0, FOOTER_TEXT_Y);
  set_scale(state->task_count, 165, FOOTER_TEXT_SCALE_Y);
  state->task_label = make_label(
      state->root, "IDLE", status_font,
      VALUE_WHITE, 0, FOOTER_TEXT_Y);
  set_scale(state->task_label, 145, FOOTER_TEXT_SCALE_Y);

  state->sync_value = make_label(
      state->root, "WAIT", status_font,
      VALUE_WHITE, 0, FOOTER_TEXT_Y);
  set_scale(
      state->sync_value,
      FOOTER_LABEL_SCALE_X, FOOTER_TEXT_SCALE_Y);
}

int value_scale_for(const char* text, bool compact_card) {
  size_t length = text ? strlen(text) : 0;
  if (length >= 6) return compact_card ? 142 : 146;
  if (length == 5) return compact_card ? 158 : 166;
  if (length == 4) return compact_card ? 190 : 205;
  if (length == 3) return compact_card ? 210 : 190;
  return compact_card ? 224 : 214;
}

void update_token_group(
    lv_obj_t* value_label, lv_obj_t* unit_label,
    int& cached_scale, const char* source,
    int center_x, bool compact_card) {
  char value[24];
  char unit[4];
  split_suffix(source, value, sizeof(value), unit, sizeof(unit));
  set_label_text_if_changed(value_label, value);
  set_unit_text(unit_label, unit);
  int scale = value_scale_for(value, compact_card);
  if (scale != cached_scale) {
    cached_scale = scale;
    set_scale(value_label, scale, VALUE_SCALE_Y);
  }
  align_baseline(
      value_label, VALUE_SCALE_Y,
      unit_label, UNIT_SCALE_Y);
  center_group(
      value_label, scale, unit_label,
      UNIT_SCALE_X, center_x, 4);
}

void update_percent_group(
    lv_obj_t* value_label, lv_obj_t* unit_label,
    int& cached_scale, int percent, int center_x) {
  char value[8];
  if (percent < 0) {
    strlcpy(value, "--", sizeof(value));
  } else {
    snprintf(value, sizeof(value), "%d", percent > 100 ? 100 : percent);
  }
  set_label_text_if_changed(value_label, value);
  set_unit_text(unit_label, "%");
  int scale = 184;
  if (scale != cached_scale) {
    cached_scale = scale;
    set_scale(value_label, scale, VALUE_SCALE_Y);
  }
  align_baseline(
      value_label, VALUE_SCALE_Y,
      unit_label, UNIT_SCALE_Y);
  center_group(
      value_label, scale, unit_label,
      UNIT_SCALE_X, center_x, 2);
}

void update_reset(
    ThreeBodyThemeState* state, int32_t seconds) {
  char days_text[8];
  char hours_text[8];
  if (seconds < 0) {
    strlcpy(days_text, "--", sizeof(days_text));
    strlcpy(hours_text, "--", sizeof(hours_text));
  } else {
    long total_hours = (static_cast<long>(seconds) + 3599L) / 3600L;
    long days = total_hours / 24L;
    long hours = total_hours % 24L;
    if (days > 99L) days = 99L;
    snprintf(days_text, sizeof(days_text), "%02ld", days);
    snprintf(hours_text, sizeof(hours_text), "%02ld", hours);
  }
  set_label_text_if_changed(state->reset_day_value, days_text);
  set_label_text_if_changed(state->reset_hour_value, hours_text);

  align_baseline(
      state->reset_day_value, VALUE_SCALE_Y,
      state->reset_day_unit, RESET_UNIT_SCALE_Y);
  align_baseline(
      state->reset_day_value, VALUE_SCALE_Y,
      state->reset_hour_unit, RESET_UNIT_SCALE_Y);

  int day_width = scaled_label_width(
      state->reset_day_value, RESET_NUMBER_SCALE_X);
  int day_unit_width = scaled_label_width(
      state->reset_day_unit, RESET_UNIT_SCALE_X);
  int hour_width = scaled_label_width(
      state->reset_hour_value, RESET_NUMBER_SCALE_X);
  int hour_unit_width = scaled_label_width(
      state->reset_hour_unit, RESET_UNIT_SCALE_X);
  constexpr int UNIT_GAP = 1;
  constexpr int GROUP_GAP = 4;
  int total_width =
      day_width + UNIT_GAP + day_unit_width + GROUP_GAP +
      hour_width + UNIT_GAP + hour_unit_width;
  int x = CARD_CENTERS[3] - total_width / 2;
  lv_obj_set_x(state->reset_day_value, x);
  x += day_width + UNIT_GAP;
  lv_obj_set_x(state->reset_day_unit, x);
  x += day_unit_width + GROUP_GAP;
  lv_obj_set_x(state->reset_hour_value, x);
  x += hour_width + UNIT_GAP;
  lv_obj_set_x(state->reset_hour_unit, x);
}

void update_battery(
    ThreeBodyThemeState* state, int percent) {
  char value[8];
  if (percent < 0) {
    strlcpy(value, "--", sizeof(value));
  } else {
    if (percent > 100) percent = 100;
    snprintf(value, sizeof(value), "%d", percent);
  }
  set_label_text_if_changed(state->battery_value, value);
  set_unit_text(state->battery_unit, "%");
  int scale = strlen(value) >= 3 ? 160 : 175;
  if (scale != state->battery_scale) {
    state->battery_scale = scale;
    set_scale(
        state->battery_value, scale, FOOTER_TEXT_SCALE_Y);
  }
  align_baseline(
      state->battery_value, FOOTER_TEXT_SCALE_Y,
      state->battery_unit, FOOTER_TEXT_SCALE_Y);
  center_group(
      state->battery_value, scale,
      state->battery_unit, FOOTER_LABEL_SCALE_X, 435, 2);

  if (percent < 0 || percent == 0) {
    lv_obj_add_flag(state->battery_icon_fill, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(state->battery_icon_fill, LV_OBJ_FLAG_HIDDEN);
    int fill_width = (13 * percent + 99) / 100;
    lv_obj_set_width(state->battery_icon_fill, fill_width);
  }
}

void update_activity(ThreeBodyThemeState* state, int running_count) {
  if (running_count < 0) running_count = 0;
  if (running_count == state->running_count) return;
  state->running_count = running_count;
  char count_text[8];
  const char* label_text = "TASKS";
  if (running_count == 0) {
    count_text[0] = '\0';
    label_text = "IDLE";
  } else if (running_count == 1) {
    strlcpy(count_text, "1", sizeof(count_text));
    label_text = "TASK";
  } else {
    if (running_count > 99) running_count = 99;
    snprintf(count_text, sizeof(count_text), "%d", running_count);
  }
  set_label_text_if_changed(state->task_count, count_text);
  set_label_text_if_changed(state->task_label, label_text);
  if (count_text[0]) {
    lv_obj_clear_flag(state->task_count, LV_OBJ_FLAG_HIDDEN);
    int count_scale = strlen(count_text) >= 2 ? 145 : 165;
    set_scale(
        state->task_count, count_scale, FOOTER_TEXT_SCALE_Y);
    set_scale(
        state->task_label, 145, FOOTER_TEXT_SCALE_Y);
    align_baseline(
        state->task_count, FOOTER_TEXT_SCALE_Y,
        state->task_label, FOOTER_TEXT_SCALE_Y);
    int count_width =
        scaled_label_width(state->task_count, count_scale);
    int label_width =
        scaled_label_width(state->task_label, 145);
    constexpr int ICON_WIDTH = 16;
    constexpr int ICON_GAP = 6;
    int text_width = count_width + 3 + label_width;
    int left = 240 - (ICON_WIDTH + ICON_GAP + text_width) / 2;
    set_task_icon_x(state, left);
    int text_left = left + ICON_WIDTH + ICON_GAP;
    lv_obj_set_x(state->task_count, text_left);
    lv_obj_set_x(state->task_label, text_left + count_width + 3);
  } else {
    lv_obj_add_flag(state->task_count, LV_OBJ_FLAG_HIDDEN);
    set_scale(
        state->task_label, 145, FOOTER_TEXT_SCALE_Y);
    int width = scaled_label_width(state->task_label, 145);
    constexpr int ICON_WIDTH = 16;
    constexpr int ICON_GAP = 6;
    int left = 240 - (ICON_WIDTH + ICON_GAP + width) / 2;
    set_task_icon_x(state, left);
    lv_obj_set_x(
        state->task_label, left + ICON_WIDTH + ICON_GAP);
  }
}

void update_sync(
    ThreeBodyThemeState* state, DashboardDataState data_state) {
  if (state->data_state_initialized && state->data_state == data_state) {
    return;
  }
  state->data_state_initialized = true;
  state->data_state = data_state;
  const char* text = "WAIT";
  lv_color_t color = STATE_YELLOW;
  switch (data_state) {
    case DashboardDataState::Ready:
      text = "SYNC";
      color = VALUE_WHITE;
      break;
    case DashboardDataState::Stale:
      text = "STALE";
      color = STATE_YELLOW;
      break;
    case DashboardDataState::Error:
      text = "ERR";
      color = STATE_RED;
      break;
    case DashboardDataState::Waiting:
    default:
      break;
  }
  set_label_text_if_changed(state->sync_value, text);
  lv_obj_set_style_text_color(state->sync_value, color, 0);
  int scale = strlen(text) >= 5 ? 115 : FOOTER_LABEL_SCALE_X;
  set_scale(state->sync_value, scale, FOOTER_TEXT_SCALE_Y);
  int width = scaled_label_width(state->sync_value, scale);
  lv_obj_set_x(state->sync_value, 121 - width / 2);
}

bool three_body_mount(
    void* raw_state, lv_obj_t* parent,
    const ThemeResources& resources) {
  auto* state = static_cast<ThreeBodyThemeState*>(raw_state);
  *state = ThreeBodyThemeState{};
  state->resources = resources;
  if (!load_background(state)) return false;
  create_fonts(state);

  state->root = lv_obj_create(parent);
  strip_obj(state->root);
  lv_obj_set_size(
      state->root, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(state->root, 0, 0);
  lv_obj_set_style_bg_color(state->root, lv_color_hex(0x020915), 0);
  lv_obj_set_style_bg_opa(state->root, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(state->root, 28, 0);
  lv_obj_set_style_clip_corner(state->root, true, 0);

  state->background = lv_image_create(state->root);
  lv_image_set_src(state->background, &state->background_dsc);
  lv_obj_set_pos(state->background, 0, 0);
  make_dynamic_layer(state);
  return true;
}

void three_body_update(
    void* raw_state, const DashboardViewModel& model) {
  auto* state = static_cast<ThreeBodyThemeState*>(raw_state);
  update_token_group(
      state->today_value, state->today_unit,
      state->today_scale, model.today_tokens_text,
      CARD_CENTERS[0], false);
  update_token_group(
      state->week_value, state->week_unit,
      state->week_scale, model.last_7d_tokens_text,
      CARD_CENTERS[1], true);
  update_percent_group(
      state->quota_value, state->quota_unit,
      state->quota_scale, model.d7_remaining,
      CARD_CENTERS[2]);
  update_reset(state, model.d7_reset_seconds);
  update_battery(state, model.battery_percent);
  update_activity(state, model.running_count);
  update_sync(state, model.data_state);
}

void three_body_tick(void* raw_state, uint32_t now_ms) {
  (void)raw_state;
  (void)now_ms;
}

void destroy_font(lv_font_t*& font) {
#if LV_USE_TINY_TTF
  if (font) lv_tiny_ttf_destroy(font);
#endif
  font = nullptr;
}

void three_body_unmount(void* raw_state) {
  auto* state = static_cast<ThreeBodyThemeState*>(raw_state);
  if (state->root) lv_obj_delete(state->root);
  state->root = nullptr;

  destroy_font(state->value_font_64);
  destroy_font(state->unit_font_40);
  destroy_font(state->status_font_30);

  if (state->background_pixels) {
    heap_caps_free(state->background_pixels);
  }
  state->background_pixels = nullptr;
  state->background_dsc = lv_image_dsc_t{};
}

const ThemeDashboardOps THREE_BODY_DASHBOARD = {
    sizeof(ThreeBodyThemeState),
    three_body_mount,
    three_body_update,
    three_body_tick,
    three_body_unmount,
};

const ThemePack THREE_BODY_THEME = {
    "three_body",
    "THREE BODY",
    1,
    0,
    THREE_BODY_DASHBOARD,
    nullptr,
    nullptr,
};

static_assert(
    sizeof(ThreeBodyThemeState) <= CODEXMETER_THEME_STATE_BYTES,
    "Three-Body theme state exceeds ThemeRuntime storage");

}  // namespace

const ThemePack& three_body_theme_pack() {
  return THREE_BODY_THEME;
}
