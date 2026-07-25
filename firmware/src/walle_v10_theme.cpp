#include "walle_v10_theme.h"

#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "device_log.h"

namespace {

constexpr char FONT_PATH[] =
    "S:/fonts/Jersey10-Regular.ttf";
constexpr char BACKGROUND_PATH[] =
    "S:/themes/walle_v10_bg.rgb565";
constexpr char LEAF_ACTIVE_PATH[] =
    "S:/themes/walle_v10_leaves_active.rgb565";
constexpr char LEAF_INACTIVE_PATH[] =
    "S:/themes/walle_v10_leaves_inactive.rgb565";
constexpr char LEAF_MASK_PATH[] =
    "S:/themes/walle_v10_leaves_mask.bin";

constexpr size_t BACKGROUND_BYTES =
    CODEXMETER_SCREEN_W * CODEXMETER_SCREEN_H * sizeof(uint16_t);
constexpr int LEAF_COUNT = 10;
constexpr int REFERENCE_ACTIVE_LEAVES = 8;
constexpr int LEAF_CROP_X = 21;
constexpr int LEAF_CROP_Y = 296;
constexpr int LEAF_CROP_W = 251;
constexpr int LEAF_CROP_H = 46;
constexpr size_t LEAF_CROP_PIXELS =
    LEAF_CROP_W * LEAF_CROP_H;
constexpr size_t LEAF_CROP_BYTES =
    LEAF_CROP_PIXELS * sizeof(uint16_t);

const lv_color_t CREAM = lv_color_hex(0xF3CE81);
const lv_color_t GREEN = lv_color_hex(0x98BA3C);
const lv_color_t OUTLINE = lv_color_hex(0x302319);
const lv_color_t LAMP_GREEN = lv_color_hex(0xB5F326);
const lv_color_t LAMP_AMBER = lv_color_hex(0xE9A429);
const lv_color_t LAMP_RED = lv_color_hex(0xD84B32);
const lv_color_t LAMP_OFF = lv_color_hex(0x4A4D42);

struct WalleV10ThemeState {
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
  lv_obj_t* task_count = nullptr;
  lv_obj_t* sync_lamp = nullptr;

  lv_font_t* font_160 = nullptr;
  lv_font_t* font_142 = nullptr;
  lv_font_t* font_112 = nullptr;
  lv_font_t* font_92 = nullptr;
  lv_font_t* font_76 = nullptr;
  lv_font_t* font_64 = nullptr;
  lv_font_t* font_48 = nullptr;
  lv_font_t* font_42 = nullptr;
  lv_font_t* font_34 = nullptr;

  uint8_t* background_pixels = nullptr;
  uint16_t* leaf_base_pixels = nullptr;
  uint16_t* leaf_active_pixels = nullptr;
  uint16_t* leaf_inactive_pixels = nullptr;
  uint8_t* leaf_mask = nullptr;
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

void set_outline(lv_obj_t* label, uint8_t opacity = LV_OPA_60) {
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
  int unit_width =
      has_unit ? scaled_width(unit, unit_scale) : 0;
  int group_width =
      value_width + (has_unit ? gap + unit_width : 0);
  int x = center_x - group_width / 2;
  lv_obj_set_x(value, x);
  if (has_unit) {
    lv_obj_set_x(unit, x + value_width + gap);
  }
}

bool read_file_exact(
    const char* path, void* output, size_t expected,
    const char* label) {
  lv_fs_file_t file;
  lv_fs_res_t result = lv_fs_open(&file, path, LV_FS_MODE_RD);
  if (result != LV_FS_RES_OK) {
    device_logf(
        "ERROR", "walle_v10 %s open failed path=%s res=%d",
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
        "walle_v10 %s read failed bytes=%lu expected=%lu res=%d",
        label, static_cast<unsigned long>(total),
        static_cast<unsigned long>(expected),
        static_cast<int>(result));
    return false;
  }
  return true;
}

void release_assets(WalleV10ThemeState* state) {
  if (state->background_pixels) {
    heap_caps_free(state->background_pixels);
  }
  if (state->leaf_base_pixels) {
    heap_caps_free(state->leaf_base_pixels);
  }
  if (state->leaf_active_pixels) {
    heap_caps_free(state->leaf_active_pixels);
  }
  if (state->leaf_inactive_pixels) {
    heap_caps_free(state->leaf_inactive_pixels);
  }
  if (state->leaf_mask) {
    heap_caps_free(state->leaf_mask);
  }
  state->background_pixels = nullptr;
  state->leaf_base_pixels = nullptr;
  state->leaf_active_pixels = nullptr;
  state->leaf_inactive_pixels = nullptr;
  state->leaf_mask = nullptr;
  state->background_dsc = lv_image_dsc_t{};
}

bool load_assets(WalleV10ThemeState* state) {
  state->background_pixels = static_cast<uint8_t*>(
      heap_caps_malloc(
          BACKGROUND_BYTES,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  state->leaf_base_pixels = static_cast<uint16_t*>(
      heap_caps_malloc(
          LEAF_CROP_BYTES,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  state->leaf_active_pixels = static_cast<uint16_t*>(
      heap_caps_malloc(
          LEAF_CROP_BYTES,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  state->leaf_inactive_pixels = static_cast<uint16_t*>(
      heap_caps_malloc(
          LEAF_CROP_BYTES,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  state->leaf_mask = static_cast<uint8_t*>(
      heap_caps_malloc(
          LEAF_CROP_PIXELS,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!state->background_pixels ||
      !state->leaf_base_pixels ||
      !state->leaf_active_pixels ||
      !state->leaf_inactive_pixels ||
      !state->leaf_mask) {
    device_logf("ERROR", "walle_v10 asset allocation failed");
    release_assets(state);
    return false;
  }

  if (!read_file_exact(
          BACKGROUND_PATH, state->background_pixels,
          BACKGROUND_BYTES, "background") ||
      !read_file_exact(
          LEAF_ACTIVE_PATH, state->leaf_active_pixels,
          LEAF_CROP_BYTES, "leaf active") ||
      !read_file_exact(
          LEAF_INACTIVE_PATH, state->leaf_inactive_pixels,
          LEAF_CROP_BYTES, "leaf inactive") ||
      !read_file_exact(
          LEAF_MASK_PATH, state->leaf_mask,
          LEAF_CROP_PIXELS, "leaf mask")) {
    release_assets(state);
    return false;
  }

  auto* background =
      reinterpret_cast<uint16_t*>(state->background_pixels);
  for (int y = 0; y < LEAF_CROP_H; ++y) {
    memcpy(
        state->leaf_base_pixels + y * LEAF_CROP_W,
        background +
            (LEAF_CROP_Y + y) * CODEXMETER_SCREEN_W +
            LEAF_CROP_X,
        LEAF_CROP_W * sizeof(uint16_t));
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
  lv_obj_set_size(lamp, 11, 11);
  lv_obj_set_style_bg_color(lamp, color, 0);
  lv_obj_set_style_bg_opa(lamp, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(lamp, 1, 0);
  return lamp;
}

void make_dynamic_layer(WalleV10ThemeState* state) {
  state->battery_value = make_label(
      state->root, "",
      font_or(state->font_42, &lv_font_montserrat_32),
      CREAM, 408, 12);
  set_outline(state->battery_value, LV_OPA_40);
  set_scale(state->battery_value, 250, 256);

  state->primary_heading = make_label(
      state->root, "TODAY TOKEN",
      font_or(state->font_34, &lv_font_montserrat_32),
      GREEN, 39, 122);
  set_outline(state->primary_heading, LV_OPA_40);
  set_scale(state->primary_heading, 266, 256);

  state->primary_value = make_label(
      state->root, "",
      font_or(state->font_160, state->resources.token_font),
      CREAM, 36, 115);
  state->primary_unit = make_label(
      state->root, "",
      font_or(state->font_112, &lv_font_montserrat_48),
      CREAM, 0, 184);
  set_outline(state->primary_value);
  set_outline(state->primary_unit);

  state->secondary_heading = make_label(
      state->root, "7 DAYS",
      font_or(state->font_34, &lv_font_montserrat_32),
      GREEN, 352, 130);
  set_outline(state->secondary_heading, LV_OPA_40);
  set_scale(state->secondary_heading, 290, 256);

  state->secondary_value = make_label(
      state->root, "",
      font_or(state->font_142, &lv_font_montserrat_48),
      CREAM, 341, 137);
  state->secondary_unit = make_label(
      state->root, "",
      font_or(state->font_92, &lv_font_montserrat_48),
      CREAM, 0, 185);
  set_outline(state->secondary_value);
  set_outline(state->secondary_unit);

  state->quota_value = make_label(
      state->root, "",
      font_or(state->font_112, &lv_font_montserrat_48),
      CREAM, 151, 318);
  lv_obj_set_style_text_letter_space(
      state->quota_value, 2, 0);
  state->quota_unit = make_label(
      state->root, "%",
      font_or(state->font_76, &lv_font_montserrat_48),
      CREAM, 0, 355);
  set_outline(state->quota_value);
  set_outline(state->quota_unit);

  state->reset_day_value = make_label(
      state->root, "",
      font_or(state->font_64, &lv_font_montserrat_48),
      CREAM, 0, 359);
  state->reset_day_unit = make_label(
      state->root, "D",
      font_or(state->font_48, &lv_font_montserrat_48),
      CREAM, 0, 373);
  state->reset_hour_value = make_label(
      state->root, "",
      font_or(state->font_64, &lv_font_montserrat_48),
      CREAM, 0, 359);
  state->reset_hour_unit = make_label(
      state->root, "H",
      font_or(state->font_48, &lv_font_montserrat_48),
      CREAM, 0, 373);
  set_outline(state->reset_day_value);
  set_outline(state->reset_day_unit);
  set_outline(state->reset_hour_value);
  set_outline(state->reset_hour_unit);

  state->task_count = make_label(
      state->root, "0",
      font_or(state->font_42, &lv_font_montserrat_32),
      CREAM, 56, 429);
  set_outline(state->task_count, LV_OPA_40);

  state->sync_lamp = make_lamp(
      state->root, 414, 441, LAMP_GREEN);
  lv_obj_set_style_bg_opa(
      state->sync_lamp, LV_OPA_TRANSP, 0);
}

bool create_fonts(WalleV10ThemeState* state) {
#if LV_USE_TINY_TTF && LV_TINY_TTF_FILE_SUPPORT
  state->font_160 = lv_tiny_ttf_create_file_ex(
      FONT_PATH, 160, LV_FONT_KERNING_NORMAL, 12);
  state->font_142 = lv_tiny_ttf_create_file_ex(
      FONT_PATH, 142, LV_FONT_KERNING_NORMAL, 12);
  state->font_112 = lv_tiny_ttf_create_file_ex(
      FONT_PATH, 112, LV_FONT_KERNING_NORMAL, 16);
  state->font_92 = lv_tiny_ttf_create_file_ex(
      FONT_PATH, 92, LV_FONT_KERNING_NORMAL, 16);
  state->font_76 = lv_tiny_ttf_create_file_ex(
      FONT_PATH, 76, LV_FONT_KERNING_NORMAL, 20);
  state->font_64 = lv_tiny_ttf_create_file_ex(
      FONT_PATH, 64, LV_FONT_KERNING_NORMAL, 20);
  state->font_48 = lv_tiny_ttf_create_file_ex(
      FONT_PATH, 48, LV_FONT_KERNING_NORMAL, 24);
  state->font_42 = lv_tiny_ttf_create_file_ex(
      FONT_PATH, 42, LV_FONT_KERNING_NORMAL, 24);
  state->font_34 = lv_tiny_ttf_create_file_ex(
      FONT_PATH, 34, LV_FONT_KERNING_NORMAL, 28);

  lv_font_t* fonts[] = {
      state->font_160, state->font_142, state->font_112,
      state->font_92, state->font_76, state->font_64,
      state->font_48, state->font_42, state->font_34,
  };
  for (lv_font_t* font : fonts) {
    if (font) font->fallback = state->resources.token_font;
  }
#endif
  return true;
}

void update_primary(
    WalleV10ThemeState* state,
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
      strlen(value) >= 6 ? 165
                         : (strlen(value) >= 5 ? 184 : 208);
  int unit_scale = model.token_usage_mode ? 148 : 142;
  set_scale(state->primary_value, value_scale, 274);
  set_scale(state->primary_unit, unit_scale, 196);
  position_from_left(
      state->primary_value, value_scale,
      state->primary_unit, unit_scale, 36, 0);
}

void update_secondary(
    WalleV10ThemeState* state,
    const DashboardViewModel& model) {
  const char* source =
      model.token_usage_mode
          ? model.last_7d_tokens_text
          : model.d7_percent_text;
  set_label_text_if_changed(
      state->secondary_heading,
      model.token_usage_mode ? "7 DAYS" : "7D REMAINS");
  lv_obj_set_x(
      state->secondary_heading,
      model.token_usage_mode ? 352 : 340);
  lv_obj_set_y(
      state->secondary_heading,
      model.token_usage_mode ? 129 : 130);
  set_scale(
      state->secondary_heading,
      model.token_usage_mode ? 293 : 156,
      model.token_usage_mode ? 284 : 256);

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

  int value_scale =
      strlen(value) >= 6
          ? 82
          : (strlen(value) >= 5
                 ? 92
                 : (strlen(value) >= 4 ? 104 : 112));
  int unit_scale = model.token_usage_mode ? 120 : 115;
  set_scale(state->secondary_value, value_scale, 224);
  set_scale(state->secondary_unit, unit_scale, 180);
  position_centered(
      state->secondary_value, value_scale,
      state->secondary_unit, unit_scale, 399, 4);
}

void update_quota(
    WalleV10ThemeState* state,
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
    int value_scale = strlen(value) >= 3 ? 182 : 213;
    constexpr int UNIT_SCALE = 151;
    set_scale(state->quota_value, value_scale, 248);
    set_scale(state->quota_unit, UNIT_SCALE, 205);
    position_centered(
        state->quota_value, value_scale,
        state->quota_unit, UNIT_SCALE, 213, 0);
  }

  int percent = model.d7_remaining;
  if (percent < 0) percent = -1;
  if (percent > 100) percent = 100;
  if (percent == state->quota_percent) return;
  state->quota_percent = percent;
  int active =
      percent < 0 ? 0 : (percent * LEAF_COUNT) / 100;
  if (active > LEAF_COUNT) active = LEAF_COUNT;

  auto* background =
      reinterpret_cast<uint16_t*>(state->background_pixels);
  for (int y = 0; y < LEAF_CROP_H; ++y) {
    for (int x = 0; x < LEAF_CROP_W; ++x) {
      size_t crop_index =
          static_cast<size_t>(y) * LEAF_CROP_W + x;
      uint8_t leaf_index = state->leaf_mask[crop_index];
      if (leaf_index == 0) continue;
      size_t background_index =
          static_cast<size_t>(LEAF_CROP_Y + y) *
              CODEXMETER_SCREEN_W +
          LEAF_CROP_X + x;
      if (active == REFERENCE_ACTIVE_LEAVES) {
        background[background_index] =
            state->leaf_base_pixels[crop_index];
      } else {
        background[background_index] =
            leaf_index <= active
                ? state->leaf_active_pixels[crop_index]
                : state->leaf_inactive_pixels[crop_index];
      }
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
    WalleV10ThemeState* state,
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

  constexpr int DAY_VALUE_SCALE = 223;
  constexpr int DAY_UNIT_SCALE = 190;
  constexpr int HOUR_VALUE_SCALE = 274;
  constexpr int HOUR_UNIT_SCALE = 205;
  set_scale(state->reset_day_value, DAY_VALUE_SCALE, 234);
  set_scale(state->reset_day_unit, DAY_UNIT_SCALE, 207);
  set_scale(state->reset_hour_value, HOUR_VALUE_SCALE, 234);
  set_scale(state->reset_hour_unit, HOUR_UNIT_SCALE, 207);
  position_centered(
      state->reset_day_value, DAY_VALUE_SCALE,
      state->reset_day_unit, DAY_UNIT_SCALE, 339, 4);
  position_centered(
      state->reset_hour_value, HOUR_VALUE_SCALE,
      state->reset_hour_unit, HOUR_UNIT_SCALE, 415, 1);
}

void update_battery(
    WalleV10ThemeState* state, int percent, bool charging) {
  if (state->battery_initialized &&
      state->battery_percent == percent &&
      state->battery_charging == charging) {
    return;
  }
  state->battery_initialized = true;
  state->battery_percent = percent;
  state->battery_charging = charging;

  char text[12];
  if (percent < 0) {
    strlcpy(text, charging ? "USB" : "--%", sizeof(text));
  } else {
    snprintf(text, sizeof(text), "%d%%", percent);
  }
  set_label_text_if_changed(state->battery_value, text);
  int scale =
      strlen(text) >= 4 ? 218
                        : (strlen(text) == 3 ? 250 : 256);
  lv_obj_set_x(
      state->battery_value, strlen(text) >= 4 ? 408 : 413);
  set_scale(state->battery_value, scale, 225);
  lv_obj_set_style_text_color(
      state->battery_value,
      percent >= 0 && percent <= 10 ? LAMP_RED : CREAM, 0);
}

void update_activity(
    WalleV10ThemeState* state, int running_count) {
  if (running_count < 0) running_count = 0;
  if (running_count == state->running_count) return;
  state->running_count = running_count;
  char text[8];
  if (running_count > 99) {
    strlcpy(text, "99+", sizeof(text));
  } else {
    snprintf(text, sizeof(text), "%d", running_count);
  }
  set_label_text_if_changed(state->task_count, text);
  int scale = strlen(text) >= 3 ? 150 : 176;
  set_scale(state->task_count, scale, 221);
}

void update_data_state(
    WalleV10ThemeState* state,
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
  lv_obj_set_style_bg_opa(
      state->sync_lamp,
      data_state == DashboardDataState::Ready
          ? LV_OPA_TRANSP
          : LV_OPA_COVER,
      0);
}

bool walle_v10_mount(
    void* raw_state, lv_obj_t* parent,
    const ThemeResources& resources) {
  auto* state =
      static_cast<WalleV10ThemeState*>(raw_state);
  *state = WalleV10ThemeState{};
  state->resources = resources;
  if (!load_assets(state)) return false;
  create_fonts(state);

  state->root = lv_obj_create(parent);
  strip_obj(state->root);
  lv_obj_set_size(
      state->root, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(state->root, 0, 0);
  lv_obj_set_style_bg_color(state->root, OUTLINE, 0);
  lv_obj_set_style_bg_opa(state->root, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(state->root, 28, 0);
  lv_obj_set_style_clip_corner(state->root, true, 0);

  state->background = lv_image_create(state->root);
  lv_image_set_src(
      state->background, &state->background_dsc);
  lv_obj_set_pos(state->background, 0, 0);
  make_dynamic_layer(state);
  return true;
}

void walle_v10_update(
    void* raw_state, const DashboardViewModel& model) {
  auto* state =
      static_cast<WalleV10ThemeState*>(raw_state);
  update_primary(state, model);
  update_secondary(state, model);
  update_quota(state, model);
  update_reset(state, model);
  update_battery(
      state, model.battery_percent, model.charging);
  update_activity(state, model.running_count);
  update_data_state(state, model.data_state);
}

void walle_v10_tick(void* raw_state, uint32_t now_ms) {
  (void)raw_state;
  (void)now_ms;
}

void destroy_font(lv_font_t*& font) {
#if LV_USE_TINY_TTF
  if (font) lv_tiny_ttf_destroy(font);
#endif
  font = nullptr;
}

void walle_v10_unmount(void* raw_state) {
  auto* state =
      static_cast<WalleV10ThemeState*>(raw_state);
  if (state->root) lv_obj_delete(state->root);
  state->root = nullptr;

  destroy_font(state->font_160);
  destroy_font(state->font_142);
  destroy_font(state->font_112);
  destroy_font(state->font_92);
  destroy_font(state->font_76);
  destroy_font(state->font_64);
  destroy_font(state->font_48);
  destroy_font(state->font_42);
  destroy_font(state->font_34);
  release_assets(state);
}

const ThemeDashboardOps WALLE_V10_DASHBOARD = {
    sizeof(WalleV10ThemeState),
    walle_v10_mount,
    walle_v10_update,
    walle_v10_tick,
    walle_v10_unmount,
};

const ThemePack WALLE_V10_THEME = {
    "walle_v10",
    "WALL-E EARTH",
    1,
    CODEXMETER_BURN_IN_DRIFT_MAX_PX,
    WALLE_V10_DASHBOARD,
    nullptr,
    nullptr,
};

static_assert(
    sizeof(WalleV10ThemeState) <= CODEXMETER_THEME_STATE_BYTES,
    "WALL-E EARTH theme state exceeds ThemeRuntime storage");

}  // namespace

const ThemePack& walle_v10_theme_pack() {
  return WALLE_V10_THEME;
}
