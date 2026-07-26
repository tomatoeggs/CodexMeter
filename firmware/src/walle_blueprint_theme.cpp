#include "walle_blueprint_theme.h"

#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "device_log.h"

namespace {

constexpr char VALUE_FONT_PATH[] =
    "S:/fonts/Teko-SemiBold.ttf";
constexpr char LABEL_FONT_PATH[] =
    "S:/fonts/D-DINCondensed-Bold.ttf";
constexpr char BACKGROUND_PATH[] =
    "S:/themes/walle_blueprint_bg.rgb565";
constexpr char BAR_ACTIVE_PATH[] =
    "S:/themes/walle_blueprint_bar_active.rgb565";
constexpr char BAR_INACTIVE_PATH[] =
    "S:/themes/walle_blueprint_bar_inactive.rgb565";
constexpr char BAR_MASK_PATH[] =
    "S:/themes/walle_blueprint_bar_mask.bin";
constexpr char BATTERY_ACTIVE_PATH[] =
    "S:/themes/walle_blueprint_battery_active.rgb565";
constexpr char BATTERY_INACTIVE_PATH[] =
    "S:/themes/walle_blueprint_battery_inactive.rgb565";
constexpr char BATTERY_MASK_PATH[] =
    "S:/themes/walle_blueprint_battery_mask.bin";

constexpr size_t BACKGROUND_BYTES =
    CODEXMETER_SCREEN_W * CODEXMETER_SCREEN_H * sizeof(uint16_t);
constexpr int BAR_CELL_COUNT = 10;
constexpr int BAR_CROP_X = 14;
constexpr int BAR_CROP_Y = 391;
constexpr int BAR_CROP_W = 216;
constexpr int BAR_CROP_H = 25;
constexpr size_t BAR_CROP_PIXELS =
    BAR_CROP_W * BAR_CROP_H;
constexpr size_t BAR_CROP_BYTES =
    BAR_CROP_PIXELS * sizeof(uint16_t);
constexpr int BATTERY_CELL_COUNT = 7;
constexpr int BATTERY_CROP_X = 326;
constexpr int BATTERY_CROP_Y = 13;
constexpr int BATTERY_CROP_W = 69;
constexpr int BATTERY_CROP_H = 19;
constexpr size_t BATTERY_CROP_PIXELS =
    BATTERY_CROP_W * BATTERY_CROP_H;
constexpr size_t BATTERY_CROP_BYTES =
    BATTERY_CROP_PIXELS * sizeof(uint16_t);

constexpr int FOOTER_Y = 431;
constexpr int FOOTER_H = 39;
constexpr int TASK_CAPSULE_X = 7;
constexpr int TASK_CAPSULE_W = 218;
constexpr int BLE_CAPSULE_X = 233;
constexpr int BLE_CAPSULE_W = 104;
constexpr int SYNC_CAPSULE_X = 345;
constexpr int SYNC_CAPSULE_W = 116;
constexpr int FOOTER_LABEL_Y = 439;
constexpr int FOOTER_LABEL_SCALE_X = 250;
constexpr int FOOTER_LABEL_SCALE_Y = 255;
constexpr int FOOTER_LAMP_SIZE = 16;
constexpr int FOOTER_GROUP_GAP = 8;

const lv_color_t CREAM = lv_color_hex(0xF2CD83);
const lv_color_t OUTLINE = lv_color_hex(0x3B2E24);
const lv_color_t LAMP_GREEN = lv_color_hex(0x6F9D55);
const lv_color_t LAMP_AMBER = lv_color_hex(0xDEA23A);
const lv_color_t LAMP_RED = lv_color_hex(0xD44E3E);
const lv_color_t LAMP_OFF = lv_color_hex(0x405D7B);

struct WalleBlueprintThemeState {
  lv_obj_t* root = nullptr;
  lv_obj_t* background = nullptr;
  lv_obj_t* battery_value = nullptr;
  lv_obj_t* primary_heading = nullptr;
  lv_obj_t* primary_value = nullptr;
  lv_obj_t* primary_unit = nullptr;
  lv_obj_t* secondary_heading = nullptr;
  lv_obj_t* secondary_value = nullptr;
  lv_obj_t* secondary_unit = nullptr;
  lv_obj_t* quota_value = nullptr;
  lv_obj_t* quota_unit = nullptr;
  lv_obj_t* reset_day_value = nullptr;
  lv_obj_t* reset_day_unit = nullptr;
  lv_obj_t* reset_hour_value = nullptr;
  lv_obj_t* reset_hour_unit = nullptr;
  lv_obj_t* task_lamp = nullptr;
  lv_obj_t* task_text = nullptr;
  lv_obj_t* ble_lamp = nullptr;
  lv_obj_t* ble_text = nullptr;
  lv_obj_t* sync_lamp = nullptr;
  lv_obj_t* sync_text = nullptr;

  lv_font_t* value_font_120 = nullptr;
  lv_font_t* value_font_82 = nullptr;
  lv_font_t* value_font_60 = nullptr;
  lv_font_t* aux_font_38 = nullptr;
  lv_font_t* aux_font_30 = nullptr;
  lv_font_t* label_font_24 = nullptr;
  lv_font_t* label_font_20 = nullptr;

  uint8_t* background_pixels = nullptr;
  uint16_t* bar_active_pixels = nullptr;
  uint16_t* bar_inactive_pixels = nullptr;
  uint8_t* bar_mask = nullptr;
  uint16_t* battery_active_pixels = nullptr;
  uint16_t* battery_inactive_pixels = nullptr;
  uint8_t* battery_mask = nullptr;
  lv_image_dsc_t background_dsc{};
  ThemeResources resources{};

  int quota_percent = -2;
  int running_count = -1;
  int battery_percent = -2;
  bool battery_charging = false;
  bool battery_initialized = false;
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
  lv_obj_set_width(label, LV_SIZE_CONTENT);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_letter_space(label, 0, 0);
  lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
  lv_obj_set_style_transform_pivot_x(label, 0, 0);
  lv_obj_set_style_transform_pivot_y(label, 0, 0);
  return label;
}

void set_outline(lv_obj_t* label, uint8_t opacity = LV_OPA_40) {
  lv_obj_set_style_text_outline_stroke_color(label, OUTLINE, 0);
  lv_obj_set_style_text_outline_stroke_width(label, 1, 0);
  lv_obj_set_style_text_outline_stroke_opa(label, opacity, 0);
}

void set_scale(
    lv_obj_t* label, int scale_x, int scale_y = 256) {
  lv_obj_set_style_transform_scale_x(label, scale_x, 0);
  lv_obj_set_style_transform_scale_y(label, scale_y, 0);
}

bool set_label_text_if_changed(
    lv_obj_t* label, const char* text) {
  const char* current = lv_label_get_text(label);
  if (current && strcmp(current, text) == 0) return false;
  lv_label_set_text(label, text);
  return true;
}

bool set_unit_visibility(lv_obj_t* unit, const char* text) {
  bool changed = set_label_text_if_changed(unit, text);
  bool hidden = lv_obj_has_flag(unit, LV_OBJ_FLAG_HIDDEN);
  if (text && text[0]) {
    if (hidden) {
      lv_obj_clear_flag(unit, LV_OBJ_FLAG_HIDDEN);
      changed = true;
    }
  } else if (!hidden) {
    lv_obj_add_flag(unit, LV_OBJ_FLAG_HIDDEN);
    changed = true;
  }
  return changed;
}

int scaled_width(lv_obj_t* label, int scale_x) {
  lv_obj_update_layout(label);
  return (lv_obj_get_width(label) * scale_x + 128) / 256;
}

void split_suffix(
    const char* source, bool percent_suffix,
    char* value, size_t value_size,
    char* unit, size_t unit_size) {
  if (!source || !source[0]) {
    strlcpy(value, "--", value_size);
    unit[0] = '\0';
    return;
  }
  size_t length = strlen(source);
  char last = source[length - 1];
  bool has_suffix =
      (percent_suffix && last == '%') ||
      (!percent_suffix && last >= 'A' && last <= 'Z');
  size_t value_length = has_suffix ? length - 1 : length;
  if (value_length >= value_size) value_length = value_size - 1;
  memcpy(value, source, value_length);
  value[value_length] = '\0';
  if (has_suffix && unit_size >= 2) {
    unit[0] = last;
    unit[1] = '\0';
  } else if (unit_size > 0) {
    unit[0] = '\0';
  }
}

void position_from_left(
    lv_obj_t* value, int value_scale,
    lv_obj_t* unit, int unit_scale,
    int left_x, int gap) {
  lv_obj_set_x(value, left_x);
  if (lv_obj_has_flag(unit, LV_OBJ_FLAG_HIDDEN)) return;
  lv_obj_set_x(
      unit, left_x + scaled_width(value, value_scale) + gap);
}

void position_centered(
    lv_obj_t* value, int value_scale,
    lv_obj_t* unit, int unit_scale,
    int center_x, int gap) {
  int value_width = scaled_width(value, value_scale);
  bool has_unit = !lv_obj_has_flag(unit, LV_OBJ_FLAG_HIDDEN);
  int unit_width = has_unit ? scaled_width(unit, unit_scale) : 0;
  int group_width =
      value_width + (has_unit ? gap + unit_width : 0);
  int x = center_x - group_width / 2;
  lv_obj_set_x(value, x);
  if (has_unit) {
    lv_obj_set_x(unit, x + value_width + gap);
  }
}

void center_label(lv_obj_t* label, int center_x, int scale_x) {
  lv_obj_set_x(label, center_x - scaled_width(label, scale_x) / 2);
}

bool read_file_exact(
    const char* path, void* output, size_t expected,
    const char* label) {
  lv_fs_file_t file;
  lv_fs_res_t result = lv_fs_open(&file, path, LV_FS_MODE_RD);
  if (result != LV_FS_RES_OK) {
    device_logf(
        "ERROR", "walle_blueprint %s open failed path=%s res=%d",
        label, path, static_cast<int>(result));
    return false;
  }

  auto* bytes = static_cast<uint8_t*>(output);
  size_t total = 0;
  while (total < expected) {
    uint32_t bytes_read = 0;
    uint32_t request = static_cast<uint32_t>(
        expected - total > 16384 ? 16384 : expected - total);
    result = lv_fs_read(
        &file, bytes + total, request, &bytes_read);
    if (result != LV_FS_RES_OK || bytes_read == 0) break;
    total += bytes_read;
  }
  lv_fs_close(&file);
  if (result != LV_FS_RES_OK || total != expected) {
    device_logf(
        "ERROR",
        "walle_blueprint %s read failed bytes=%lu expected=%lu res=%d",
        label, static_cast<unsigned long>(total),
        static_cast<unsigned long>(expected),
        static_cast<int>(result));
    return false;
  }
  return true;
}

void release_assets(WalleBlueprintThemeState* state) {
  if (state->background_pixels) heap_caps_free(state->background_pixels);
  if (state->bar_active_pixels) heap_caps_free(state->bar_active_pixels);
  if (state->bar_inactive_pixels) heap_caps_free(state->bar_inactive_pixels);
  if (state->bar_mask) heap_caps_free(state->bar_mask);
  if (state->battery_active_pixels) {
    heap_caps_free(state->battery_active_pixels);
  }
  if (state->battery_inactive_pixels) {
    heap_caps_free(state->battery_inactive_pixels);
  }
  if (state->battery_mask) heap_caps_free(state->battery_mask);
  state->background_pixels = nullptr;
  state->bar_active_pixels = nullptr;
  state->bar_inactive_pixels = nullptr;
  state->bar_mask = nullptr;
  state->battery_active_pixels = nullptr;
  state->battery_inactive_pixels = nullptr;
  state->battery_mask = nullptr;
  state->background_dsc = lv_image_dsc_t{};
}

bool load_assets(WalleBlueprintThemeState* state) {
  state->background_pixels = static_cast<uint8_t*>(
      heap_caps_malloc(
          BACKGROUND_BYTES,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  state->bar_active_pixels = static_cast<uint16_t*>(
      heap_caps_malloc(
          BAR_CROP_BYTES,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  state->bar_inactive_pixels = static_cast<uint16_t*>(
      heap_caps_malloc(
          BAR_CROP_BYTES,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  state->bar_mask = static_cast<uint8_t*>(
      heap_caps_malloc(
          BAR_CROP_PIXELS,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  state->battery_active_pixels = static_cast<uint16_t*>(
      heap_caps_malloc(
          BATTERY_CROP_BYTES,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  state->battery_inactive_pixels = static_cast<uint16_t*>(
      heap_caps_malloc(
          BATTERY_CROP_BYTES,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  state->battery_mask = static_cast<uint8_t*>(
      heap_caps_malloc(
          BATTERY_CROP_PIXELS,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!state->background_pixels || !state->bar_active_pixels ||
      !state->bar_inactive_pixels || !state->bar_mask ||
      !state->battery_active_pixels ||
      !state->battery_inactive_pixels || !state->battery_mask) {
    device_logf("ERROR", "walle_blueprint asset allocation failed");
    release_assets(state);
    return false;
  }

  if (!read_file_exact(
          BACKGROUND_PATH, state->background_pixels,
          BACKGROUND_BYTES, "background") ||
      !read_file_exact(
          BAR_ACTIVE_PATH, state->bar_active_pixels,
          BAR_CROP_BYTES, "bar active") ||
      !read_file_exact(
          BAR_INACTIVE_PATH, state->bar_inactive_pixels,
          BAR_CROP_BYTES, "bar inactive") ||
      !read_file_exact(
          BAR_MASK_PATH, state->bar_mask,
          BAR_CROP_PIXELS, "bar mask") ||
      !read_file_exact(
          BATTERY_ACTIVE_PATH, state->battery_active_pixels,
          BATTERY_CROP_BYTES, "battery active") ||
      !read_file_exact(
          BATTERY_INACTIVE_PATH, state->battery_inactive_pixels,
          BATTERY_CROP_BYTES, "battery inactive") ||
      !read_file_exact(
          BATTERY_MASK_PATH, state->battery_mask,
          BATTERY_CROP_PIXELS, "battery mask")) {
    release_assets(state);
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

lv_obj_t* make_lamp(
    lv_obj_t* parent, int x, int y, lv_color_t color) {
  lv_obj_t* lamp = lv_obj_create(parent);
  strip_obj(lamp);
  lv_obj_set_pos(lamp, x, y);
  lv_obj_set_size(lamp, FOOTER_LAMP_SIZE, FOOTER_LAMP_SIZE);
  lv_obj_set_style_bg_color(lamp, color, 0);
  lv_obj_set_style_bg_opa(lamp, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(lamp, CREAM, 0);
  lv_obj_set_style_border_width(lamp, 1, 0);
  lv_obj_set_style_border_opa(lamp, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(lamp, FOOTER_LAMP_SIZE / 2, 0);
  return lamp;
}

void make_capsule(
    lv_obj_t* parent, int x, int width) {
  lv_obj_t* capsule = lv_obj_create(parent);
  strip_obj(capsule);
  lv_obj_set_pos(capsule, x, FOOTER_Y);
  lv_obj_set_size(capsule, width, FOOTER_H);
  lv_obj_set_style_bg_opa(capsule, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(capsule, CREAM, 0);
  lv_obj_set_style_border_width(capsule, 1, 0);
  lv_obj_set_style_border_opa(capsule, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(capsule, FOOTER_H / 2, 0);
}

void center_footer_group(
    lv_obj_t* lamp, lv_obj_t* label,
    int capsule_x, int capsule_width) {
  int label_width = scaled_width(label, FOOTER_LABEL_SCALE_X);
  int group_width =
      FOOTER_LAMP_SIZE + FOOTER_GROUP_GAP + label_width;
  int group_x = capsule_x + (capsule_width - group_width) / 2;
  lv_obj_set_x(lamp, group_x);
  lv_obj_set_x(
      label, group_x + FOOTER_LAMP_SIZE + FOOTER_GROUP_GAP);
}

void make_dynamic_layer(WalleBlueprintThemeState* state) {
  state->battery_value = make_label(
      state->root, "",
      font_or(state->aux_font_30, &lv_font_montserrat_24),
      CREAM, 409, 6);
  set_scale(state->battery_value, 225, 240);
  set_outline(state->battery_value);

  state->primary_heading = make_label(
      state->root, "TODAY TOKEN",
      font_or(state->label_font_20, &lv_font_montserrat_16),
      CREAM, 19, 68);
  set_scale(state->primary_heading, 280, 220);
  set_outline(state->primary_heading, LV_OPA_30);

  state->primary_value = make_label(
      state->root, "",
      font_or(state->value_font_120, state->resources.token_font),
      CREAM, 16, 62);
  state->primary_unit = make_label(
      state->root, "",
      font_or(state->value_font_82, &lv_font_montserrat_48),
      CREAM, 0, 93);
  set_outline(state->primary_value);
  set_outline(state->primary_unit);

  state->secondary_heading = make_label(
      state->root, "7 DAYS",
      font_or(state->label_font_20, &lv_font_montserrat_16),
      CREAM, 0, 84);
  set_scale(state->secondary_heading, 345, 280);
  set_outline(state->secondary_heading, LV_OPA_30);

  state->secondary_value = make_label(
      state->root, "",
      font_or(state->value_font_82, &lv_font_montserrat_48),
      CREAM, 0, 105);
  state->secondary_unit = make_label(
      state->root, "",
      font_or(state->value_font_60, &lv_font_montserrat_48),
      CREAM, 0, 117);
  set_outline(state->secondary_value);
  set_outline(state->secondary_unit);

  state->quota_value = make_label(
      state->root, "",
      font_or(state->value_font_82, &lv_font_montserrat_48),
      CREAM, 17, 291);
  state->quota_unit = make_label(
      state->root, "%",
      font_or(state->value_font_60, &lv_font_montserrat_48),
      CREAM, 0, 316);
  set_outline(state->quota_value);
  set_outline(state->quota_unit);

  state->reset_day_value = make_label(
      state->root, "",
      font_or(state->value_font_60, &lv_font_montserrat_48),
      CREAM, 0, 348);
  state->reset_day_unit = make_label(
      state->root, "D",
      font_or(state->aux_font_38, &lv_font_montserrat_32),
      CREAM, 0, 364);
  state->reset_hour_value = make_label(
      state->root, "",
      font_or(state->value_font_60, &lv_font_montserrat_48),
      CREAM, 0, 348);
  state->reset_hour_unit = make_label(
      state->root, "H",
      font_or(state->aux_font_38, &lv_font_montserrat_32),
      CREAM, 0, 364);
  set_outline(state->reset_day_value);
  set_outline(state->reset_day_unit);
  set_outline(state->reset_hour_value);
  set_outline(state->reset_hour_unit);

  make_capsule(state->root, TASK_CAPSULE_X, TASK_CAPSULE_W);
  make_capsule(state->root, BLE_CAPSULE_X, BLE_CAPSULE_W);
  make_capsule(state->root, SYNC_CAPSULE_X, SYNC_CAPSULE_W);

  state->task_lamp = make_lamp(
      state->root, 0, 442, LAMP_GREEN);
  state->task_text = make_label(
      state->root, "0 ACTIVE TASKS",
      font_or(state->label_font_24, &lv_font_montserrat_24),
      CREAM, 0, FOOTER_LABEL_Y);
  lv_obj_set_style_text_letter_space(state->task_text, 1, 0);
  set_scale(
      state->task_text,
      FOOTER_LABEL_SCALE_X, FOOTER_LABEL_SCALE_Y);
  set_outline(state->task_text, LV_OPA_30);

  state->ble_lamp = make_lamp(
      state->root, 0, 442, LAMP_GREEN);
  state->ble_text = make_label(
      state->root, "BLE",
      font_or(state->label_font_24, &lv_font_montserrat_24),
      CREAM, 0, FOOTER_LABEL_Y);
  lv_obj_set_style_text_letter_space(state->ble_text, 1, 0);
  set_scale(
      state->ble_text,
      FOOTER_LABEL_SCALE_X, FOOTER_LABEL_SCALE_Y);
  set_outline(state->ble_text, LV_OPA_30);

  state->sync_lamp = make_lamp(
      state->root, 0, 442, LAMP_GREEN);
  state->sync_text = make_label(
      state->root, "SYNC",
      font_or(state->label_font_24, &lv_font_montserrat_24),
      CREAM, 0, FOOTER_LABEL_Y);
  lv_obj_set_style_text_letter_space(state->sync_text, 1, 0);
  set_scale(
      state->sync_text,
      FOOTER_LABEL_SCALE_X, FOOTER_LABEL_SCALE_Y);
  set_outline(state->sync_text, LV_OPA_30);

  center_footer_group(
      state->task_lamp, state->task_text,
      TASK_CAPSULE_X, TASK_CAPSULE_W);
  center_footer_group(
      state->ble_lamp, state->ble_text,
      BLE_CAPSULE_X, BLE_CAPSULE_W);
  center_footer_group(
      state->sync_lamp, state->sync_text,
      SYNC_CAPSULE_X, SYNC_CAPSULE_W);
}

bool create_fonts(WalleBlueprintThemeState* state) {
#if LV_USE_TINY_TTF && LV_TINY_TTF_FILE_SUPPORT
  state->value_font_120 = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, 120, LV_FONT_KERNING_NORMAL, 16);
  state->value_font_82 = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, 82, LV_FONT_KERNING_NORMAL, 16);
  state->value_font_60 = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, 60, LV_FONT_KERNING_NORMAL, 20);
  state->aux_font_38 = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, 38, LV_FONT_KERNING_NORMAL, 24);
  state->aux_font_30 = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, 30, LV_FONT_KERNING_NORMAL, 24);
  state->label_font_24 = lv_tiny_ttf_create_file_ex(
      LABEL_FONT_PATH, 24, LV_FONT_KERNING_NORMAL, 32);
  state->label_font_20 = lv_tiny_ttf_create_file_ex(
      LABEL_FONT_PATH, 20, LV_FONT_KERNING_NORMAL, 32);

  lv_font_t* fonts[] = {
      state->value_font_120, state->value_font_82,
      state->value_font_60, state->aux_font_38,
      state->aux_font_30, state->label_font_24,
      state->label_font_20,
  };
  for (lv_font_t* font : fonts) {
    if (font) font->fallback = state->resources.token_font;
  }
#endif
  return true;
}

void update_primary(
    WalleBlueprintThemeState* state,
    const DashboardViewModel& model) {
  const char* source =
      model.token_usage_mode
          ? model.today_tokens_text
          : model.h5_percent_text;
  set_label_text_if_changed(
      state->primary_heading,
      model.token_usage_mode ? "TODAY TOKEN" : "5H REMAINS");

  char value[24];
  char unit[4];
  split_suffix(
      source, !model.token_usage_mode,
      value, sizeof(value), unit, sizeof(unit));
  bool value_changed =
      set_label_text_if_changed(state->primary_value, value);
  bool unit_changed =
      set_unit_visibility(state->primary_unit, unit);
  if (!value_changed && !unit_changed) return;

  int value_scale =
      strlen(value) >= 6 ? 128
                         : (strlen(value) >= 5 ? 147 : 170);
  int unit_scale = model.token_usage_mode ? 188 : 184;
  set_scale(state->primary_value, value_scale, 262);
  set_scale(state->primary_unit, unit_scale, 276);
  position_from_left(
      state->primary_value, value_scale,
      state->primary_unit, unit_scale, 16, 2);
}

void update_secondary(
    WalleBlueprintThemeState* state,
    const DashboardViewModel& model) {
  const char* source =
      model.token_usage_mode
          ? model.last_7d_tokens_text
          : model.d7_percent_text;
  set_label_text_if_changed(
      state->secondary_heading,
      model.token_usage_mode ? "7 DAYS" : "7D REMAINS");
  int heading_scale = model.token_usage_mode ? 345 : 250;
  set_scale(state->secondary_heading, heading_scale, 280);
  center_label(state->secondary_heading, 391, heading_scale);

  char value[24];
  char unit[4];
  split_suffix(
      source, !model.token_usage_mode,
      value, sizeof(value), unit, sizeof(unit));
  bool value_changed =
      set_label_text_if_changed(state->secondary_value, value);
  bool unit_changed =
      set_unit_visibility(state->secondary_unit, unit);
  if (!value_changed && !unit_changed) return;

  size_t value_length = strlen(value);
  int value_scale = 132;
  if (model.token_usage_mode) {
    value_scale =
        value_length >= 5 ? 108
                          : (value_length >= 4 ? 120 : 132);
  } else {
    value_scale = value_length >= 3 ? 125 : 142;
  }
  int unit_scale = model.token_usage_mode ? 150 : 145;
  set_scale(state->secondary_value, value_scale, 178);
  set_scale(state->secondary_unit, unit_scale, 190);
  position_centered(
      state->secondary_value, value_scale,
      state->secondary_unit, unit_scale, 391, 2);
}

void update_quota(
    WalleBlueprintThemeState* state,
    const DashboardViewModel& model) {
  char value[12];
  char unit[4];
  split_suffix(
      model.d7_percent_text, true,
      value, sizeof(value), unit, sizeof(unit));
  bool value_changed =
      set_label_text_if_changed(state->quota_value, value);
  bool unit_changed =
      set_unit_visibility(state->quota_unit, unit);
  if (value_changed || unit_changed) {
    int value_scale = strlen(value) >= 3 ? 195 : 218;
    constexpr int UNIT_SCALE = 215;
    set_scale(state->quota_value, value_scale, 282);
    set_scale(state->quota_unit, UNIT_SCALE, 275);
    position_centered(
        state->quota_value, value_scale,
        state->quota_unit, UNIT_SCALE, 66, 1);
  }

  int percent = model.d7_remaining;
  if (percent < 0) percent = -1;
  if (percent > 100) percent = 100;
  if (percent == state->quota_percent) return;
  state->quota_percent = percent;
  int active =
      percent < 0 ? 0 : (percent * BAR_CELL_COUNT) / 100;
  if (active > BAR_CELL_COUNT) active = BAR_CELL_COUNT;

  auto* background =
      reinterpret_cast<uint16_t*>(state->background_pixels);
  for (int y = 0; y < BAR_CROP_H; ++y) {
    for (int x = 0; x < BAR_CROP_W; ++x) {
      size_t crop_index =
          static_cast<size_t>(y) * BAR_CROP_W + x;
      uint8_t cell_index = state->bar_mask[crop_index];
      if (cell_index == 0) continue;
      size_t background_index =
          static_cast<size_t>(BAR_CROP_Y + y) *
              CODEXMETER_SCREEN_W + BAR_CROP_X + x;
      background[background_index] =
          cell_index <= active
              ? state->bar_active_pixels[crop_index]
              : state->bar_inactive_pixels[crop_index];
    }
  }
  lv_obj_invalidate(state->background);
}

void format_reset(
    char* days, size_t days_size,
    char* hours, size_t hours_size,
    int32_t seconds) {
  if (seconds < 0) {
    strlcpy(days, "--", days_size);
    strlcpy(hours, "--", hours_size);
    return;
  }
  long total_hours = (seconds + 3599L) / 3600L;
  snprintf(days, days_size, "%02ld", total_hours / 24L);
  snprintf(hours, hours_size, "%02ld", total_hours % 24L);
}

void update_reset(
    WalleBlueprintThemeState* state,
    const DashboardViewModel& model) {
  char days[12];
  char hours[12];
  format_reset(
      days, sizeof(days), hours, sizeof(hours),
      model.d7_reset_seconds);
  bool days_changed =
      set_label_text_if_changed(state->reset_day_value, days);
  bool hours_changed =
      set_label_text_if_changed(state->reset_hour_value, hours);
  if (!days_changed && !hours_changed) return;

  constexpr int DAY_VALUE_SCALE = 165;
  constexpr int DAY_UNIT_SCALE = 170;
  constexpr int HOUR_VALUE_SCALE = 168;
  constexpr int HOUR_UNIT_SCALE = 185;
  set_scale(state->reset_day_value, DAY_VALUE_SCALE, 200);
  set_scale(state->reset_day_unit, DAY_UNIT_SCALE, 205);
  set_scale(state->reset_hour_value, HOUR_VALUE_SCALE, 200);
  set_scale(state->reset_hour_unit, HOUR_UNIT_SCALE, 205);
  position_centered(
      state->reset_day_value, DAY_VALUE_SCALE,
      state->reset_day_unit, DAY_UNIT_SCALE, 377, 1);
  position_centered(
      state->reset_hour_value, HOUR_VALUE_SCALE,
      state->reset_hour_unit, HOUR_UNIT_SCALE, 433, 1);
}

void update_battery_cells(
    WalleBlueprintThemeState* state, int percent) {
  int active =
      percent < 0 ? 0 : (percent * BATTERY_CELL_COUNT) / 100;
  if (active > BATTERY_CELL_COUNT) active = BATTERY_CELL_COUNT;

  auto* background =
      reinterpret_cast<uint16_t*>(state->background_pixels);
  for (int y = 0; y < BATTERY_CROP_H; ++y) {
    for (int x = 0; x < BATTERY_CROP_W; ++x) {
      size_t crop_index =
          static_cast<size_t>(y) * BATTERY_CROP_W + x;
      uint8_t cell_index = state->battery_mask[crop_index];
      if (cell_index == 0) continue;
      size_t background_index =
          static_cast<size_t>(BATTERY_CROP_Y + y) *
              CODEXMETER_SCREEN_W + BATTERY_CROP_X + x;
      background[background_index] =
          cell_index <= active
              ? state->battery_active_pixels[crop_index]
              : state->battery_inactive_pixels[crop_index];
    }
  }
  lv_obj_invalidate(state->background);
}

void update_battery(
    WalleBlueprintThemeState* state, int percent, bool charging) {
  if (state->battery_initialized &&
      state->battery_percent == percent &&
      state->battery_charging == charging) {
    return;
  }
  state->battery_initialized = true;
  state->battery_percent = percent;
  state->battery_charging = charging;
  update_battery_cells(state, percent);

  char text[12];
  if (percent < 0) {
    strlcpy(text, charging ? "USB" : "--%", sizeof(text));
  } else {
    snprintf(text, sizeof(text), "%d%%", percent);
  }
  set_label_text_if_changed(state->battery_value, text);
  int scale = strlen(text) >= 4 ? 215 : 235;
  set_scale(state->battery_value, scale, 240);
  center_label(state->battery_value, 435, scale);
  lv_obj_set_style_text_color(
      state->battery_value,
      percent >= 0 && percent <= 10 ? LAMP_RED : CREAM, 0);
}

void update_activity(
    WalleBlueprintThemeState* state, int running_count) {
  if (running_count < 0) running_count = 0;
  if (running_count == state->running_count) return;
  state->running_count = running_count;

  char text[32];
  if (running_count > 99) {
    strlcpy(text, "99+ ACTIVE TASKS", sizeof(text));
  } else {
    snprintf(text, sizeof(text), "%d ACTIVE TASKS", running_count);
  }
  set_label_text_if_changed(state->task_text, text);
  set_scale(
      state->task_text,
      FOOTER_LABEL_SCALE_X, FOOTER_LABEL_SCALE_Y);
  center_footer_group(
      state->task_lamp, state->task_text,
      TASK_CAPSULE_X, TASK_CAPSULE_W);
}

void update_data_state(
    WalleBlueprintThemeState* state,
    DashboardDataState data_state) {
  if (state->data_state_initialized &&
      state->data_state == data_state) {
    return;
  }
  state->data_state_initialized = true;
  state->data_state = data_state;
  lv_color_t color = LAMP_OFF;
  switch (data_state) {
    case DashboardDataState::Ready:
      color = LAMP_GREEN;
      break;
    case DashboardDataState::Stale:
      color = LAMP_AMBER;
      break;
    case DashboardDataState::Error:
      color = LAMP_RED;
      break;
    case DashboardDataState::Waiting:
    default:
      color = LAMP_OFF;
      break;
  }
  lv_obj_set_style_bg_color(state->sync_lamp, color, 0);
}

bool walle_blueprint_mount(
    void* raw_state, lv_obj_t* parent,
    const ThemeResources& resources) {
  auto* state =
      static_cast<WalleBlueprintThemeState*>(raw_state);
  *state = WalleBlueprintThemeState{};
  state->resources = resources;
  if (!load_assets(state)) return false;
  create_fonts(state);

  state->root = lv_obj_create(parent);
  strip_obj(state->root);
  lv_obj_set_size(
      state->root, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(state->root, 0, 0);
  lv_obj_set_style_bg_color(state->root, lv_color_hex(0x002454), 0);
  lv_obj_set_style_bg_opa(state->root, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(state->root, 28, 0);
  lv_obj_set_style_clip_corner(state->root, true, 0);

  state->background = lv_image_create(state->root);
  lv_image_set_src(state->background, &state->background_dsc);
  lv_obj_set_pos(state->background, 0, 0);
  make_dynamic_layer(state);
  return true;
}

void walle_blueprint_update(
    void* raw_state, const DashboardViewModel& model) {
  auto* state =
      static_cast<WalleBlueprintThemeState*>(raw_state);
  update_primary(state, model);
  update_secondary(state, model);
  update_quota(state, model);
  update_reset(state, model);
  update_battery(
      state, model.battery_percent, model.charging);
  update_activity(state, model.running_count);
  update_data_state(state, model.data_state);
}

void walle_blueprint_tick(void* raw_state, uint32_t now_ms) {
  (void)raw_state;
  (void)now_ms;
}

void destroy_font(lv_font_t*& font) {
#if LV_USE_TINY_TTF
  if (font) lv_tiny_ttf_destroy(font);
#endif
  font = nullptr;
}

void walle_blueprint_unmount(void* raw_state) {
  auto* state =
      static_cast<WalleBlueprintThemeState*>(raw_state);
  if (state->root) lv_obj_delete(state->root);
  state->root = nullptr;

  destroy_font(state->value_font_120);
  destroy_font(state->value_font_82);
  destroy_font(state->value_font_60);
  destroy_font(state->aux_font_38);
  destroy_font(state->aux_font_30);
  destroy_font(state->label_font_24);
  destroy_font(state->label_font_20);
  release_assets(state);
}

const ThemeDashboardOps WALLE_BLUEPRINT_DASHBOARD = {
    sizeof(WalleBlueprintThemeState),
    walle_blueprint_mount,
    walle_blueprint_update,
    walle_blueprint_tick,
    walle_blueprint_unmount,
};

const ThemePack WALLE_BLUEPRINT_THEME = {
    "walle_blueprint",
    "WALL-E BLUEPRINT",
    1,
    CODEXMETER_BURN_IN_DRIFT_MAX_PX,
    WALLE_BLUEPRINT_DASHBOARD,
    nullptr,
    nullptr,
};

static_assert(
    sizeof(WalleBlueprintThemeState) <= CODEXMETER_THEME_STATE_BYTES,
    "WALL-E BLUEPRINT theme state exceeds ThemeRuntime storage");

}  // namespace

const ThemePack& walle_blueprint_theme_pack() {
  return WALLE_BLUEPRINT_THEME;
}
