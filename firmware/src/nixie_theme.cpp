#include "nixie_theme.h"

#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "device_log.h"

namespace {

constexpr char BACKGROUND_PATH[] = "S:/themes/nixie_bg.rgb565";
constexpr char TODAY_INTEGER_PANEL_PATH[] =
    "S:/themes/nixie_today_integer.rgb565";
constexpr char VALUE_FONT_PATH[] = "S:/fonts/D-DINCondensed-Bold.ttf";
constexpr size_t BACKGROUND_BYTES =
    CODEXMETER_SCREEN_W * CODEXMETER_SCREEN_H * sizeof(uint16_t);
constexpr int TODAY_INTEGER_PANEL_X = 47;
constexpr int TODAY_INTEGER_PANEL_Y = 337;
constexpr int TODAY_INTEGER_PANEL_W = 156;
constexpr int TODAY_INTEGER_PANEL_H = 55;
constexpr size_t TODAY_INTEGER_PANEL_BYTES =
    TODAY_INTEGER_PANEL_W * TODAY_INTEGER_PANEL_H * sizeof(uint16_t);

constexpr int DIGIT_SPRITE_W = 90;
constexpr int DIGIT_SPRITE_H = 108;
constexpr size_t DIGIT_SPRITE_BYTES =
    DIGIT_SPRITE_W * DIGIT_SPRITE_H * sizeof(uint32_t);
constexpr int DIGIT_X[] = {71, 185, 299};
constexpr int DIGIT_Y = 130;
constexpr int TUBE_GLOW_X[] = {84, 198, 312};
constexpr int TUBE_GLOW_Y = 103;
constexpr int TUBE_GLOW_W = 64;
constexpr int TUBE_GLOW_H = 154;
constexpr int BATTERY_BAR_X[] = {353, 362, 371};
constexpr int BATTERY_BAR_W = 7;
constexpr int BATTERY_BAR_COUNT = 3;
constexpr int BATTERY_BAR_Y = 432;
constexpr int BATTERY_BAR_H = 13;
constexpr int COUNTER_VALUE_Y = 344;
constexpr int COUNTER_UNIT_Y = 352;
constexpr int TODAY_VALUE_X[] = {51, 88, 114, 140};
constexpr int TODAY_VALUE_W[] = {32, 33, 33, 33};
constexpr int TODAY_INTEGER_VALUE_X[] = {51, 90, 129};
constexpr int TODAY_INTEGER_VALUE_W[] = {32, 32, 32};
constexpr int TODAY_ROLLER_CELL[] = {0, 1, 3};
constexpr int TODAY_ROLLER_COUNT = 3;
constexpr int WEEK_VALUE_X[] = {269, 308, 348};
constexpr int WEEK_VALUE_W[] = {42, 43, 42};
constexpr int TODAY_UNIT_X = 178;
constexpr int TODAY_INTEGER_UNIT_X = 172;
constexpr int TODAY_UNIT_W = 24;
constexpr int WEEK_UNIT_X = 385;
constexpr int WEEK_UNIT_W = 40;
constexpr int RESET_NUMBER_Y = 421;
constexpr int RESET_UNIT_Y = 434;
constexpr int RESET_DAY_X = 67;
constexpr int RESET_DAY_W = 34;
constexpr int RESET_DAY_UNIT_X = 107;
constexpr int RESET_HOUR_X = 128;
constexpr int RESET_HOUR_W = 39;
constexpr int RESET_HOUR_UNIT_X = 173;
constexpr int RESET_UNIT_W = 14;
constexpr int STATUS_VALUE_Y = 420;
constexpr int TASK_VALUE_X = 264;
constexpr int TASK_VALUE_W = 50;
constexpr int BATTERY_VALUE_X = 393;
constexpr int BATTERY_VALUE_W = 48;

const lv_color_t IVORY = lv_color_hex(0xD8C1A4);
const lv_color_t IVORY_DIM = lv_color_hex(0x8B7863);
const lv_color_t STATUS_GREEN = lv_color_hex(0x62F68D);
const lv_color_t STATUS_GREEN_DIM = lv_color_hex(0x173722);
const lv_color_t TUBE_GLOW_AMBER = lv_color_hex(0xFF6508);

struct NixieThemeState {
  lv_obj_t* root = nullptr;
  lv_obj_t* background = nullptr;
  lv_obj_t* today_integer_panel = nullptr;
  lv_obj_t* tube_glows[3]{};
  lv_obj_t* digit_images[3]{};
  lv_obj_t* today_cells[4]{};
  lv_obj_t* week_cells[3]{};
  lv_obj_t* today_unit = nullptr;
  lv_obj_t* week_unit = nullptr;
  lv_obj_t* reset_day = nullptr;
  lv_obj_t* reset_day_unit = nullptr;
  lv_obj_t* reset_hour = nullptr;
  lv_obj_t* reset_hour_unit = nullptr;
  lv_obj_t* activity_lamp = nullptr;
  lv_obj_t* task_count = nullptr;
  lv_obj_t* battery_bars[BATTERY_BAR_COUNT]{};
  lv_obj_t* battery_value = nullptr;

  lv_font_t* counter_48 = nullptr;
  lv_font_t* counter_unit_34 = nullptr;
  lv_font_t* status_39 = nullptr;
  lv_font_t* reset_38 = nullptr;
  lv_font_t* unit_22 = nullptr;

  uint8_t* background_pixels = nullptr;
  lv_image_dsc_t background_dsc{};
  uint8_t* today_integer_panel_pixels = nullptr;
  lv_image_dsc_t today_integer_panel_dsc{};
  uint8_t* digit_pixels[10]{};
  lv_image_dsc_t digit_dsc[10]{};
  ThemeResources resources{};

  int quota = -2;
  int running_count = -1;
  int battery_percent = -2;
  bool battery_charging = false;
  bool battery_initialized = false;
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

lv_font_t* create_font(
    int size, const lv_font_t* fallback, size_t cache_size = 24) {
#if LV_USE_TINY_TTF && LV_TINY_TTF_FILE_SUPPORT
  lv_font_t* font = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, size, LV_FONT_KERNING_NORMAL, cache_size);
  if (font) font->fallback = fallback;
  return font;
#else
  (void)size;
  (void)fallback;
  (void)cache_size;
  return nullptr;
#endif
}

lv_obj_t* make_label(
    lv_obj_t* parent, const char* text, const lv_font_t* font,
    lv_color_t color, int x, int y, int width,
    lv_text_align_t alignment = LV_TEXT_ALIGN_CENTER) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, width);
  lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, alignment, 0);
  lv_obj_set_style_text_letter_space(label, 1, 0);
  return label;
}

lv_obj_t* make_rect(
    lv_obj_t* parent, int x, int y, int width, int height,
    lv_color_t color, int radius = 0) {
  lv_obj_t* obj = lv_obj_create(parent);
  strip_obj(obj);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, width, height);
  lv_obj_set_style_bg_color(obj, color, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(obj, radius, 0);
  return obj;
}

bool set_text_if_changed(lv_obj_t* label, const char* text) {
  const char* current = lv_label_get_text(label);
  if (current && strcmp(current, text) == 0) return false;
  lv_label_set_text(label, text);
  return true;
}

bool read_background(NixieThemeState* state) {
  state->background_pixels = static_cast<uint8_t*>(
      heap_caps_malloc(
          BACKGROUND_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!state->background_pixels) {
    device_logf(
        "ERROR", "nixie background alloc failed bytes=%lu",
        static_cast<unsigned long>(BACKGROUND_BYTES));
    return false;
  }

  lv_fs_file_t file;
  lv_fs_res_t result =
      lv_fs_open(&file, BACKGROUND_PATH, LV_FS_MODE_RD);
  if (result != LV_FS_RES_OK) {
    device_logf(
        "ERROR", "nixie background open failed path=%s res=%d",
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
        "nixie background read failed bytes=%lu expected=%lu res=%d",
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

bool read_today_integer_panel(NixieThemeState* state) {
  state->today_integer_panel_pixels = static_cast<uint8_t*>(
      heap_caps_malloc(
          TODAY_INTEGER_PANEL_BYTES,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!state->today_integer_panel_pixels) {
    device_logf(
        "ERROR", "nixie Today integer panel alloc failed bytes=%lu",
        static_cast<unsigned long>(TODAY_INTEGER_PANEL_BYTES));
    return false;
  }

  lv_fs_file_t file;
  lv_fs_res_t result =
      lv_fs_open(&file, TODAY_INTEGER_PANEL_PATH, LV_FS_MODE_RD);
  if (result != LV_FS_RES_OK) {
    device_logf(
        "ERROR", "nixie Today integer panel open failed path=%s res=%d",
        TODAY_INTEGER_PANEL_PATH, static_cast<int>(result));
    heap_caps_free(state->today_integer_panel_pixels);
    state->today_integer_panel_pixels = nullptr;
    return false;
  }

  size_t total = 0;
  while (total < TODAY_INTEGER_PANEL_BYTES) {
    uint32_t bytes_read = 0;
    uint32_t request = static_cast<uint32_t>(
        TODAY_INTEGER_PANEL_BYTES - total > 8192
            ? 8192
            : TODAY_INTEGER_PANEL_BYTES - total);
    result = lv_fs_read(
        &file, state->today_integer_panel_pixels + total,
        request, &bytes_read);
    if (result != LV_FS_RES_OK || bytes_read == 0) break;
    total += bytes_read;
  }
  lv_fs_close(&file);
  if (result != LV_FS_RES_OK || total != TODAY_INTEGER_PANEL_BYTES) {
    device_logf(
        "ERROR",
        "nixie Today integer panel read failed bytes=%lu expected=%lu res=%d",
        static_cast<unsigned long>(total),
        static_cast<unsigned long>(TODAY_INTEGER_PANEL_BYTES),
        static_cast<int>(result));
    heap_caps_free(state->today_integer_panel_pixels);
    state->today_integer_panel_pixels = nullptr;
    return false;
  }

  state->today_integer_panel_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  state->today_integer_panel_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
  state->today_integer_panel_dsc.header.flags = 0;
  state->today_integer_panel_dsc.header.w = TODAY_INTEGER_PANEL_W;
  state->today_integer_panel_dsc.header.h = TODAY_INTEGER_PANEL_H;
  state->today_integer_panel_dsc.header.stride =
      TODAY_INTEGER_PANEL_W * sizeof(uint16_t);
  state->today_integer_panel_dsc.data_size = TODAY_INTEGER_PANEL_BYTES;
  state->today_integer_panel_dsc.data = state->today_integer_panel_pixels;
  state->today_integer_panel_dsc.reserved = nullptr;
  state->today_integer_panel_dsc.reserved_2 = nullptr;
  return true;
}

bool read_digit_assets(NixieThemeState* state) {
  for (int digit = 0; digit < 10; ++digit) {
    char path[48];
    snprintf(
        path, sizeof(path),
        "S:/themes/nixie_digit_%d.argb8888", digit);
    state->digit_pixels[digit] = static_cast<uint8_t*>(
        heap_caps_malloc(
            DIGIT_SPRITE_BYTES,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!state->digit_pixels[digit]) {
      device_logf(
          "ERROR", "nixie digit alloc failed digit=%d bytes=%lu",
          digit, static_cast<unsigned long>(DIGIT_SPRITE_BYTES));
      return false;
    }

    lv_fs_file_t file;
    lv_fs_res_t result = lv_fs_open(&file, path, LV_FS_MODE_RD);
    if (result != LV_FS_RES_OK) {
      device_logf(
          "ERROR", "nixie digit open failed path=%s res=%d",
          path, static_cast<int>(result));
      return false;
    }

    size_t total = 0;
    while (total < DIGIT_SPRITE_BYTES) {
      uint32_t bytes_read = 0;
      uint32_t request = static_cast<uint32_t>(
          DIGIT_SPRITE_BYTES - total > 8192
              ? 8192
              : DIGIT_SPRITE_BYTES - total);
      result = lv_fs_read(
          &file, state->digit_pixels[digit] + total,
          request, &bytes_read);
      if (result != LV_FS_RES_OK || bytes_read == 0) break;
      total += bytes_read;
    }
    lv_fs_close(&file);
    if (result != LV_FS_RES_OK || total != DIGIT_SPRITE_BYTES) {
      device_logf(
          "ERROR",
          "nixie digit read failed digit=%d bytes=%lu expected=%lu res=%d",
          digit, static_cast<unsigned long>(total),
          static_cast<unsigned long>(DIGIT_SPRITE_BYTES),
          static_cast<int>(result));
      return false;
    }

    lv_image_dsc_t& dsc = state->digit_dsc[digit];
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    dsc.header.flags = 0;
    dsc.header.w = DIGIT_SPRITE_W;
    dsc.header.h = DIGIT_SPRITE_H;
    dsc.header.stride = DIGIT_SPRITE_W * sizeof(uint32_t);
    dsc.data_size = DIGIT_SPRITE_BYTES;
    dsc.data = state->digit_pixels[digit];
    dsc.reserved = nullptr;
    dsc.reserved_2 = nullptr;
  }
  return true;
}

void create_fonts(NixieThemeState* state) {
  state->counter_48 = create_font(
      48, font_or(state->resources.token_font, &lv_font_montserrat_48), 28);
  state->counter_unit_34 = create_font(34, &lv_font_montserrat_32, 16);
  state->status_39 = create_font(39, &lv_font_montserrat_32, 24);
  state->reset_38 = create_font(38, &lv_font_montserrat_32, 24);
  state->unit_22 = create_font(22, &lv_font_montserrat_24, 24);
}

void create_digit_layer(NixieThemeState* state) {
  for (int position = 0; position < 3; ++position) {
    lv_obj_t* glow = lv_obj_create(state->root);
    strip_obj(glow);
    lv_obj_set_pos(glow, TUBE_GLOW_X[position], TUBE_GLOW_Y);
    lv_obj_set_size(glow, TUBE_GLOW_W, TUBE_GLOW_H);
    lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(glow, TUBE_GLOW_AMBER, 0);
    lv_obj_set_style_bg_opa(glow, 24, 0);
    lv_obj_set_style_shadow_color(glow, TUBE_GLOW_AMBER, 0);
    lv_obj_set_style_shadow_width(glow, 46, 0);
    lv_obj_set_style_shadow_spread(glow, 6, 0);
    lv_obj_set_style_shadow_opa(glow, 64, 0);
    lv_obj_set_style_blend_mode(glow, LV_BLEND_MODE_ADDITIVE, 0);
    state->tube_glows[position] = glow;
  }
  for (int position = 0; position < 3; ++position) {
    state->digit_images[position] = lv_image_create(state->root);
    lv_image_set_src(
        state->digit_images[position], &state->digit_dsc[0]);
    lv_obj_set_pos(
        state->digit_images[position], DIGIT_X[position], DIGIT_Y);
  }
}

void create_dynamic_layer(NixieThemeState* state) {
  create_digit_layer(state);

  const lv_font_t* counter =
      font_or(state->counter_48, &lv_font_montserrat_48);
  for (int i = 0; i < 4; ++i) {
    state->today_cells[i] = make_label(
        state->root, "", counter, IVORY,
        TODAY_VALUE_X[i], COUNTER_VALUE_Y, TODAY_VALUE_W[i]);
    lv_obj_set_style_text_letter_space(state->today_cells[i], 0, 0);
  }
  for (int i = 0; i < 3; ++i) {
    state->week_cells[i] = make_label(
        state->root, "", counter, IVORY,
        WEEK_VALUE_X[i], COUNTER_VALUE_Y, WEEK_VALUE_W[i]);
    lv_obj_set_style_text_letter_space(state->week_cells[i], 0, 0);
  }
  const lv_font_t* reset_font =
      font_or(state->reset_38, &lv_font_montserrat_32);
  const lv_font_t* counter_unit_font =
      font_or(state->counter_unit_34, &lv_font_montserrat_32);
  const lv_font_t* unit_font =
      font_or(state->unit_22, &lv_font_montserrat_24);
  state->today_unit = make_label(
      state->root, "", counter_unit_font, IVORY,
      TODAY_UNIT_X, COUNTER_UNIT_Y, TODAY_UNIT_W);
  state->week_unit = make_label(
      state->root, "", counter_unit_font, IVORY,
      WEEK_UNIT_X, COUNTER_UNIT_Y, WEEK_UNIT_W);
  lv_obj_set_style_text_letter_space(state->today_unit, 0, 0);
  lv_obj_set_style_text_letter_space(state->week_unit, 0, 0);
  state->reset_day = make_label(
      state->root, "--", reset_font, IVORY,
      RESET_DAY_X, RESET_NUMBER_Y, RESET_DAY_W,
      LV_TEXT_ALIGN_RIGHT);
  state->reset_day_unit = make_label(
      state->root, "D", unit_font, IVORY,
      RESET_DAY_UNIT_X, RESET_UNIT_Y, RESET_UNIT_W,
      LV_TEXT_ALIGN_LEFT);
  state->reset_hour = make_label(
      state->root, "--", reset_font, IVORY,
      RESET_HOUR_X, RESET_NUMBER_Y, RESET_HOUR_W,
      LV_TEXT_ALIGN_RIGHT);
  state->reset_hour_unit = make_label(
      state->root, "H", unit_font, IVORY,
      RESET_HOUR_UNIT_X, RESET_UNIT_Y, RESET_UNIT_W,
      LV_TEXT_ALIGN_LEFT);
  for (lv_obj_t* label : {
       state->reset_day, state->reset_day_unit,
       state->reset_hour, state->reset_hour_unit}) {
    lv_obj_set_style_text_letter_space(label, 0, 0);
  }
  lv_obj_set_style_text_letter_space(state->reset_hour, 7, 0);

  state->activity_lamp = make_rect(
      state->root, 232, 430, 16, 16, STATUS_GREEN_DIM, LV_RADIUS_CIRCLE);
  lv_obj_set_style_border_width(state->activity_lamp, 1, 0);
  lv_obj_set_style_border_color(
      state->activity_lamp, lv_color_hex(0x0D1B12), 0);
  state->task_count = make_label(
      state->root, "00",
      font_or(state->status_39, &lv_font_montserrat_32),
      IVORY, TASK_VALUE_X, STATUS_VALUE_Y, TASK_VALUE_W);
  lv_obj_set_style_text_letter_space(state->task_count, 5, 0);

  for (int i = 0; i < BATTERY_BAR_COUNT; ++i) {
    state->battery_bars[i] = make_rect(
        state->root, BATTERY_BAR_X[i], BATTERY_BAR_Y,
        BATTERY_BAR_W, BATTERY_BAR_H, IVORY, 1);
    lv_obj_set_style_bg_opa(state->battery_bars[i], LV_OPA_20, 0);
  }
  state->battery_value = make_label(
      state->root, "--",
      font_or(state->status_39, &lv_font_montserrat_32),
      IVORY, BATTERY_VALUE_X, STATUS_VALUE_Y, BATTERY_VALUE_W);
  lv_obj_set_style_text_letter_space(state->battery_value, 4, 0);
}

void set_digit(NixieThemeState* state, int position, int value) {
  bool visible = value >= 0 && value <= 9;
  lv_obj_t* image = state->digit_images[position];
  lv_obj_t* glow = state->tube_glows[position];
  if (!visible) {
    lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(glow, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_image_set_src(image, &state->digit_dsc[value]);
  lv_obj_clear_flag(image, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(glow, LV_OBJ_FLAG_HIDDEN);
}

void update_quota(NixieThemeState* state, int quota) {
  if (quota == state->quota) return;
  state->quota = quota;
  if (quota < 0) {
    for (int i = 0; i < 3; ++i) set_digit(state, i, -1);
    return;
  }
  if (quota > 100) quota = 100;
  set_digit(state, 0, (quota / 100) % 10);
  set_digit(state, 1, (quota / 10) % 10);
  set_digit(state, 2, quota % 10);
}

void update_counter_values(
    NixieThemeState* state, const DashboardViewModel& model) {
  const char* today =
      model.has_today_tokens ? model.today_tokens_text : "--";
  const char* week =
      model.has_last_7d_tokens ? model.last_7d_tokens_text : "--";

  auto update_cells = [](
      lv_obj_t* const* cells, int cell_count, const char* text) {
    size_t length = text ? strlen(text) : 0;
    if (length > static_cast<size_t>(cell_count)) {
      text += length - cell_count;
      length = cell_count;
    }
    int leading = cell_count - static_cast<int>(length);
    for (int i = 0; i < cell_count; ++i) {
      char glyph[2] = {'\0', '\0'};
      if (i >= leading) glyph[0] = text[i - leading];
      set_text_if_changed(cells[i], glyph);
    }
  };

  auto split_unit = [](
      const char* source, char* value, size_t value_size,
      char* unit, size_t unit_size) {
    strlcpy(value, source ? source : "--", value_size);
    unit[0] = '\0';
    size_t length = strlen(value);
    if (length == 0 || unit_size < 2) return;
    char suffix = value[length - 1];
    bool is_unit =
        (suffix >= 'A' && suffix <= 'Z') ||
        (suffix >= 'a' && suffix <= 'z');
    if (!is_unit) return;
    unit[0] = suffix;
    unit[1] = '\0';
    value[length - 1] = '\0';
  };

  auto update_roller_cells = [](
      lv_obj_t* const* cells, int cell_count,
      const int* roller_cells, int roller_count,
      const char* text) {
    for (int i = 0; i < cell_count; ++i) {
      set_text_if_changed(cells[i], "");
    }
    size_t length = text ? strlen(text) : 0;
    if (length > static_cast<size_t>(roller_count)) {
      text += length - roller_count;
      length = roller_count;
    }
    int leading = roller_count - static_cast<int>(length);
    for (int i = leading; i < roller_count; ++i) {
      char glyph[2] = {text[i - leading], '\0'};
      set_text_if_changed(cells[roller_cells[i]], glyph);
    }
  };

  // D-DIN's "7" has visibly more side bearing on its left at this size.
  // LVGL centers the advance box, so move that glyph one pixel left to center
  // its illuminated face inside the mechanical roller. Other digits measure
  // within half a pixel of the roller center and need no integer-pixel shift.
  auto apply_roller_optical_offsets = [](
      lv_obj_t* const* cells, int cell_count, const int* base_x) {
    for (int i = 0; i < cell_count; ++i) {
      const char* glyph = lv_label_get_text(cells[i]);
      int offset =
          glyph && glyph[0] == '7' && glyph[1] == '\0' ? -1 : 0;
      lv_obj_set_x(cells[i], base_x[i] + offset);
    }
  };

  char value[24];
  char unit[4];
  split_unit(today, value, sizeof(value), unit, sizeof(unit));
  bool today_integer_layout =
      model.has_today_tokens && model.today_tokens >= 100000000ULL &&
      unit[0] == 'M' && value[0] != '\0' && !strchr(value, '.');
  if (today_integer_layout) {
    lv_obj_clear_flag(state->today_integer_panel, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 3; ++i) {
      lv_obj_set_x(state->today_cells[i], TODAY_INTEGER_VALUE_X[i]);
      lv_obj_set_width(state->today_cells[i], TODAY_INTEGER_VALUE_W[i]);
    }
    set_text_if_changed(state->today_cells[3], "");
    lv_obj_set_x(state->today_unit, TODAY_INTEGER_UNIT_X);
    update_cells(state->today_cells, 3, value);
    apply_roller_optical_offsets(
        state->today_cells, 3, TODAY_INTEGER_VALUE_X);
  } else if (strchr(value, '.')) {
    lv_obj_add_flag(state->today_integer_panel, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 4; ++i) {
      lv_obj_set_x(state->today_cells[i], TODAY_VALUE_X[i]);
      lv_obj_set_width(state->today_cells[i], TODAY_VALUE_W[i]);
    }
    lv_obj_set_x(state->today_unit, TODAY_UNIT_X);
    update_cells(state->today_cells, 4, value);
    apply_roller_optical_offsets(
        state->today_cells, 4, TODAY_VALUE_X);
  } else {
    lv_obj_add_flag(state->today_integer_panel, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 4; ++i) {
      lv_obj_set_x(state->today_cells[i], TODAY_VALUE_X[i]);
      lv_obj_set_width(state->today_cells[i], TODAY_VALUE_W[i]);
    }
    lv_obj_set_x(state->today_unit, TODAY_UNIT_X);
    update_roller_cells(
        state->today_cells, 4,
        TODAY_ROLLER_CELL, TODAY_ROLLER_COUNT, value);
    apply_roller_optical_offsets(
        state->today_cells, 4, TODAY_VALUE_X);
  }
  set_text_if_changed(state->today_unit, unit);

  split_unit(week, value, sizeof(value), unit, sizeof(unit));
  update_cells(state->week_cells, 3, value);
  apply_roller_optical_offsets(
      state->week_cells, 3, WEEK_VALUE_X);
  set_text_if_changed(state->week_unit, unit);
}

void update_reset(
    NixieThemeState* state, const DashboardViewModel& model) {
  char day[8];
  char hour[8];
  if (model.d7_reset_seconds < 0) {
    strlcpy(day, "--", sizeof(day));
    strlcpy(hour, "--", sizeof(hour));
  } else {
    long total_hours =
        (static_cast<long>(model.d7_reset_seconds) + 3599L) / 3600L;
    long days = total_hours / 24L;
    long hours = total_hours % 24L;
    if (days > 99L) days = 99L;
    snprintf(day, sizeof(day), "%ld", days);
    snprintf(hour, sizeof(hour), "%02ld", hours);
  }
  set_text_if_changed(state->reset_day, day);
  set_text_if_changed(state->reset_hour, hour);
}

void update_activity(NixieThemeState* state, int running_count) {
  if (running_count < 0) running_count = 0;
  if (running_count == state->running_count) return;
  state->running_count = running_count;
  if (running_count > 99) running_count = 99;

  char text[8];
  snprintf(text, sizeof(text), "%02d", running_count);
  set_text_if_changed(state->task_count, text);

  bool active = running_count > 0;
  lv_obj_set_style_bg_color(
      state->activity_lamp,
      active ? STATUS_GREEN : STATUS_GREEN_DIM, 0);
  lv_obj_set_style_shadow_color(
      state->activity_lamp, STATUS_GREEN, 0);
  lv_obj_set_style_shadow_width(state->activity_lamp, active ? 10 : 0, 0);
  lv_obj_set_style_shadow_spread(state->activity_lamp, active ? 1 : 0, 0);
  lv_obj_set_style_shadow_opa(
      state->activity_lamp, active ? LV_OPA_60 : LV_OPA_TRANSP, 0);
}

void update_battery(
    NixieThemeState* state, int percent, bool charging) {
  if (state->battery_initialized &&
      state->battery_percent == percent &&
      state->battery_charging == charging) {
    return;
  }
  state->battery_initialized = true;
  state->battery_percent = percent;
  state->battery_charging = charging;

  char text[8];
  if (percent < 0) {
    strlcpy(text, "--", sizeof(text));
  } else {
    if (percent > 100) percent = 100;
    snprintf(text, sizeof(text), "%d", percent);
  }
  set_text_if_changed(state->battery_value, text);

  int lit = percent <= 0 ? 0 : (percent + 32) / 33;
  if (lit > BATTERY_BAR_COUNT) lit = BATTERY_BAR_COUNT;
  for (int i = 0; i < BATTERY_BAR_COUNT; ++i) {
    lv_obj_set_style_bg_color(
        state->battery_bars[i], IVORY, 0);
    lv_obj_set_style_bg_opa(
        state->battery_bars[i], i < lit ? LV_OPA_COVER : LV_OPA_20, 0);
  }
}

bool nixie_mount(
    void* raw_state, lv_obj_t* parent,
    const ThemeResources& resources) {
  auto* state = static_cast<NixieThemeState*>(raw_state);
  *state = NixieThemeState{};
  state->resources = resources;
  if (!read_background(state)) return false;
  if (!read_today_integer_panel(state)) return false;
  if (!read_digit_assets(state)) return false;
  create_fonts(state);

  state->root = lv_obj_create(parent);
  strip_obj(state->root);
  lv_obj_set_pos(state->root, 0, 0);
  lv_obj_set_size(
      state->root, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_style_bg_color(state->root, lv_color_hex(0x050403), 0);
  lv_obj_set_style_bg_opa(state->root, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(state->root, 28, 0);
  lv_obj_set_style_clip_corner(state->root, true, 0);

  state->background = lv_image_create(state->root);
  lv_image_set_src(state->background, &state->background_dsc);
  lv_obj_set_pos(state->background, 0, 0);
  state->today_integer_panel = lv_image_create(state->root);
  lv_image_set_src(
      state->today_integer_panel, &state->today_integer_panel_dsc);
  lv_obj_set_pos(
      state->today_integer_panel,
      TODAY_INTEGER_PANEL_X, TODAY_INTEGER_PANEL_Y);
  lv_obj_add_flag(state->today_integer_panel, LV_OBJ_FLAG_HIDDEN);
  create_dynamic_layer(state);
  return true;
}

void nixie_update(
    void* raw_state, const DashboardViewModel& model) {
  auto* state = static_cast<NixieThemeState*>(raw_state);
  update_quota(state, model.d7_remaining);
  update_counter_values(state, model);
  update_reset(state, model);
  update_activity(state, model.running_count);
  update_battery(state, model.battery_percent, model.charging);
}

void nixie_tick(void* raw_state, uint32_t now_ms) {
  (void)raw_state;
  (void)now_ms;
}

void destroy_font(lv_font_t*& font) {
#if LV_USE_TINY_TTF
  if (font) lv_tiny_ttf_destroy(font);
#endif
  font = nullptr;
}

void nixie_unmount(void* raw_state) {
  auto* state = static_cast<NixieThemeState*>(raw_state);
  if (state->root) lv_obj_delete(state->root);
  state->root = nullptr;

  destroy_font(state->counter_48);
  destroy_font(state->counter_unit_34);
  destroy_font(state->status_39);
  destroy_font(state->reset_38);
  destroy_font(state->unit_22);

  if (state->background_pixels) {
    heap_caps_free(state->background_pixels);
  }
  state->background_pixels = nullptr;
  state->background_dsc = lv_image_dsc_t{};
  if (state->today_integer_panel_pixels) {
    heap_caps_free(state->today_integer_panel_pixels);
  }
  state->today_integer_panel_pixels = nullptr;
  state->today_integer_panel_dsc = lv_image_dsc_t{};
  for (int digit = 0; digit < 10; ++digit) {
    if (state->digit_pixels[digit]) {
      heap_caps_free(state->digit_pixels[digit]);
    }
    state->digit_pixels[digit] = nullptr;
    state->digit_dsc[digit] = lv_image_dsc_t{};
  }
}

const ThemeDashboardOps NIXIE_DASHBOARD = {
    sizeof(NixieThemeState),
    nixie_mount,
    nixie_update,
    nixie_tick,
    nixie_unmount,
};

const ThemePack NIXIE_THEME = {
    "nixie",
    "NIXIE RACK",
    1,
    0,
    NIXIE_DASHBOARD,
    nullptr,
    nullptr,
};

static_assert(
    sizeof(NixieThemeState) <= CODEXMETER_THEME_STATE_BYTES,
    "Nixie theme state exceeds ThemeRuntime storage");

}  // namespace

const ThemePack& nixie_theme_pack() {
  return NIXIE_THEME;
}
