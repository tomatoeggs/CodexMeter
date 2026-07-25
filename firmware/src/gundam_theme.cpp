#include "gundam_theme.h"

#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "device_log.h"

namespace {

constexpr char BACKGROUND_PATH[] = "S:/themes/gundam_bg.rgb565";
constexpr char INSTRUMENT_FONT_PATH[] =
    "S:/fonts/D-DINCondensed-Bold.ttf";
constexpr char VALUE_FONT_PATH[] =
    "S:/fonts/Teko-SemiBold.ttf";
constexpr size_t BACKGROUND_BYTES =
    CODEXMETER_SCREEN_W * CODEXMETER_SCREEN_H * sizeof(uint16_t);
constexpr int QUOTA_CELL_COUNT = 10;
constexpr int TASK_LAMP_COUNT = 7;

constexpr int PRIMARY_LEFT_X = 30;
constexpr int SECONDARY_LEFT_X = 30;
constexpr int QUOTA_LEFT_X = 30;
constexpr int RESET_CENTER_X = 395;
constexpr int STATUS_Y_OFFSET = 2;

const lv_color_t IVORY = lv_color_hex(0xE2CEAD);
const lv_color_t STEEL = lv_color_hex(0x778391);
const lv_color_t PANEL_DARK = lv_color_hex(0x101A23);
const lv_color_t SENSOR_GREEN = lv_color_hex(0x69C77C);
const lv_color_t SENSOR_GREEN_BRIGHT = lv_color_hex(0x82E788);
const lv_color_t SIGNAL_YELLOW = lv_color_hex(0xEAB22A);
const lv_color_t SIGNAL_RED = lv_color_hex(0xCC4837);
const lv_color_t CELL_EMPTY = lv_color_hex(0x252B2C);
const lv_color_t CELL_BORDER = lv_color_hex(0x48515A);

struct GundamThemeState {
  lv_obj_t* root = nullptr;
  lv_obj_t* background = nullptr;
  lv_obj_t* sensor_connector = nullptr;
  lv_obj_t* sensor_reticle = nullptr;
  lv_point_precise_t sensor_connector_points[4]{};

  lv_obj_t* battery_heading = nullptr;
  lv_obj_t* battery_value = nullptr;

  lv_obj_t* primary_heading = nullptr;
  lv_obj_t* primary_value = nullptr;
  lv_obj_t* primary_unit = nullptr;

  lv_obj_t* secondary_heading = nullptr;
  lv_obj_t* secondary_value = nullptr;
  lv_obj_t* secondary_unit = nullptr;

  lv_obj_t* quota_heading = nullptr;
  lv_obj_t* quota_value = nullptr;
  lv_obj_t* quota_unit = nullptr;
  lv_obj_t* quota_cells[QUOTA_CELL_COUNT]{};
  lv_obj_t* quota_fills[QUOTA_CELL_COUNT]{};

  lv_obj_t* reset_heading = nullptr;
  lv_obj_t* reset_day_value = nullptr;
  lv_obj_t* reset_day_unit = nullptr;
  lv_obj_t* reset_hour_value = nullptr;
  lv_obj_t* reset_hour_unit = nullptr;

  lv_obj_t* task_value = nullptr;
  lv_obj_t* task_lamps[TASK_LAMP_COUNT]{};
  lv_obj_t* task_lamp_cores[TASK_LAMP_COUNT]{};
  lv_obj_t* ble_value = nullptr;
  lv_obj_t* sync_value = nullptr;

  lv_font_t* display_font_100 = nullptr;
  lv_font_t* display_font_64 = nullptr;
  lv_font_t* display_font_56 = nullptr;
  lv_font_t* display_font_52 = nullptr;
  lv_font_t* display_font_36 = nullptr;
  lv_font_t* display_font_32 = nullptr;
  lv_font_t* display_font_20 = nullptr;

  uint8_t* background_pixels = nullptr;
  lv_image_dsc_t background_dsc{};
  ThemeResources resources{};

  int quota_percent = -2;
  int running_count = -1;
  lv_opa_t task_lamp_opacity = LV_OPA_TRANSP;
  DashboardDataState data_state = DashboardDataState::Waiting;
  bool data_state_initialized = false;
};

void strip_obj(lv_obj_t* obj) {
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

lv_obj_t* make_rect(
    lv_obj_t* parent, int x, int y, int width, int height,
    lv_color_t color, int radius = 0) {
  lv_obj_t* rect = lv_obj_create(parent);
  strip_obj(rect);
  lv_obj_set_size(rect, width, height);
  lv_obj_set_pos(rect, x, y);
  lv_obj_set_style_bg_color(rect, color, 0);
  lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(rect, radius, 0);
  return rect;
}

lv_obj_t* make_label(
    lv_obj_t* parent, const char* text, const lv_font_t* font,
    lv_color_t color, int x, int y, int width,
    lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_width(label, width);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, align, 0);
  lv_obj_set_style_text_letter_space(label, 0, 0);
  lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  return label;
}

lv_obj_t* make_content_label(
    lv_obj_t* parent, const char* text, const lv_font_t* font,
    lv_color_t color, int x, int y) {
  lv_obj_t* label =
      make_label(
          parent, text, font, color, x, y,
          LV_SIZE_CONTENT, LV_TEXT_ALIGN_LEFT);
  lv_obj_set_style_transform_pivot_x(label, 0, 0);
  return label;
}

void set_tracking(lv_obj_t* label, int pixels) {
  lv_obj_set_style_text_letter_space(label, pixels, 0);
}

void set_left_transform_origin(lv_obj_t* label) {
  lv_obj_set_style_transform_pivot_x(label, 0, 0);
}

void set_instrument_outline(lv_obj_t* label) {
  lv_obj_set_style_text_outline_stroke_color(
      label, lv_color_hex(0x07101A), 0);
  lv_obj_set_style_text_outline_stroke_width(label, 1, 0);
  lv_obj_set_style_text_outline_stroke_opa(
      label, LV_OPA_50, 0);
}

bool set_label_text_if_changed(
    lv_obj_t* label, const char* text) {
  const char* current = lv_label_get_text(label);
  if (current && strcmp(current, text) == 0) return false;
  lv_label_set_text(label, text);
  return true;
}

const lv_font_t* font_or(
    const lv_font_t* preferred, const lv_font_t* fallback) {
  return preferred ? preferred : fallback;
}

int scaled_label_width(lv_obj_t* label, int scale_x) {
  lv_obj_update_layout(label);
  return (lv_obj_get_width(label) * scale_x + 128) / 256;
}

void position_value_group(
    lv_obj_t* value, int value_scale_x,
    lv_obj_t* unit, int unit_scale_x,
    int center_x, int gap) {
  int value_width = scaled_label_width(value, value_scale_x);
  bool has_unit = !lv_obj_has_flag(unit, LV_OBJ_FLAG_HIDDEN);
  int unit_width =
      has_unit ? scaled_label_width(unit, unit_scale_x) : 0;
  int group_width =
      value_width + (has_unit ? gap + unit_width : 0);
  int x = center_x - group_width / 2;
  lv_obj_set_x(value, x);
  if (has_unit) {
    lv_obj_set_x(unit, x + value_width + gap);
  }
}

void position_value_group_from_left(
    lv_obj_t* value, int value_scale_x,
    lv_obj_t* unit,
    int left_x, int gap) {
  int value_width = scaled_label_width(value, value_scale_x);
  bool has_unit = !lv_obj_has_flag(unit, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_x(value, left_x);
  if (has_unit) {
    lv_obj_set_x(unit, left_x + value_width + gap);
  }
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

void format_gundam_week_tokens(
    char* output, size_t size,
    const DashboardViewModel& model) {
  constexpr uint64_t ONE_BILLION = 1000000000ULL;
  constexpr uint64_t ONE_TRILLION = 1000000000000ULL;
  constexpr uint64_t TOKENS_PER_HUNDREDTH_BILLION = 10000000ULL;

  if (!model.has_last_7d_tokens ||
      model.last_7d_tokens < ONE_BILLION ||
      model.last_7d_tokens >= ONE_TRILLION) {
    strlcpy(output, model.last_7d_tokens_text, size);
    return;
  }

  uint64_t hundredths =
      (model.last_7d_tokens +
       TOKENS_PER_HUNDREDTH_BILLION / 2ULL) /
      TOKENS_PER_HUNDREDTH_BILLION;
  snprintf(
      output, size, "%llu.%02lluB",
      static_cast<unsigned long long>(hundredths / 100ULL),
      static_cast<unsigned long long>(hundredths % 100ULL));
}

bool load_background(GundamThemeState* state) {
  state->background_pixels = static_cast<uint8_t*>(
      heap_caps_malloc(
          BACKGROUND_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!state->background_pixels) {
    device_logf(
        "ERROR", "gundam background alloc failed bytes=%lu",
        static_cast<unsigned long>(BACKGROUND_BYTES));
    return false;
  }

  lv_fs_file_t file;
  lv_fs_res_t result =
      lv_fs_open(&file, BACKGROUND_PATH, LV_FS_MODE_RD);
  if (result != LV_FS_RES_OK) {
    device_logf(
        "ERROR", "gundam background open failed path=%s res=%d",
        BACKGROUND_PATH, static_cast<int>(result));
    return false;
  }

  size_t total = 0;
  while (total < BACKGROUND_BYTES) {
    uint32_t bytes_read = 0;
    uint32_t request =
        static_cast<uint32_t>(
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
        "gundam background read failed bytes=%lu expected=%lu res=%d",
        static_cast<unsigned long>(total),
        static_cast<unsigned long>(BACKGROUND_BYTES),
        static_cast<int>(result));
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

void make_background(GundamThemeState* state) {
  state->background = lv_image_create(state->root);
  lv_image_set_src(state->background, &state->background_dsc);
  lv_obj_set_pos(state->background, 0, 0);
}

void make_sensor_connector(GundamThemeState* state) {
  state->sensor_connector_points[0] = {418, 69};
  state->sensor_connector_points[1] = {418, 94};
  state->sensor_connector_points[2] = {381, 136};
  state->sensor_connector_points[3] = {337, 147};

  state->sensor_connector = lv_line_create(state->root);
  lv_line_set_points(
      state->sensor_connector,
      state->sensor_connector_points,
      sizeof(state->sensor_connector_points) /
          sizeof(state->sensor_connector_points[0]));
  lv_obj_set_pos(state->sensor_connector, 0, 0);
  lv_obj_set_style_line_width(
      state->sensor_connector, 1, 0);
  lv_obj_set_style_line_color(
      state->sensor_connector, SENSOR_GREEN, 0);
  lv_obj_set_style_line_opa(
      state->sensor_connector, LV_OPA_60, 0);

  state->sensor_reticle = make_rect(
      state->root, 333, 143, 9, 9,
      PANEL_DARK, 1);
  lv_obj_set_style_bg_opa(
      state->sensor_reticle, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(
      state->sensor_reticle, 1, 0);
  lv_obj_set_style_border_color(
      state->sensor_reticle, SENSOR_GREEN, 0);
  lv_obj_set_style_border_opa(
      state->sensor_reticle, LV_OPA_70, 0);
}

void make_battery(GundamThemeState* state) {
  const lv_font_t* small =
      font_or(state->display_font_20, &lv_font_montserrat_16);
  state->battery_heading = make_label(
      state->root, "BATTERY", small, SENSOR_GREEN,
      398, 26, 68, LV_TEXT_ALIGN_CENTER);
  lv_obj_set_style_transform_pivot_x(
      state->battery_heading, 34, 0);
  lv_obj_set_style_transform_scale_x(
      state->battery_heading, 150, 0);
  lv_obj_set_style_transform_scale_y(
      state->battery_heading, 170, 0);
  set_tracking(state->battery_heading, 1);

  state->battery_value = make_label(
      state->root, "--%",
      font_or(state->display_font_32, &lv_font_montserrat_32),
      SENSOR_GREEN, 399, 34, 70, LV_TEXT_ALIGN_CENTER);
  lv_obj_set_style_transform_pivot_x(
      state->battery_value, 35, 0);
  set_instrument_outline(state->battery_value);
}

void make_primary(GundamThemeState* state) {
  state->primary_heading = make_label(
      state->root, "TODAY TOKEN",
      font_or(state->display_font_20, &lv_font_montserrat_16),
      IVORY, 30, 96, 154);
  set_left_transform_origin(state->primary_heading);
  lv_obj_set_style_transform_scale_x(
      state->primary_heading, 300, 0);
  lv_obj_set_style_transform_scale_y(
      state->primary_heading, 235, 0);
  set_tracking(state->primary_heading, 1);

  state->primary_value = make_content_label(
      state->root, "--",
      font_or(state->display_font_100, state->resources.token_font),
      IVORY, 28, 97);
  lv_obj_set_style_transform_scale_y(
      state->primary_value, 297, 0);
  state->primary_unit = make_content_label(
      state->root, "",
      font_or(state->display_font_64, &lv_font_montserrat_48),
      IVORY, 0, 145);
  set_instrument_outline(state->primary_value);
  set_instrument_outline(state->primary_unit);
}

void make_secondary(GundamThemeState* state) {
  state->secondary_heading = make_label(
      state->root, "7 DAYS",
      font_or(state->display_font_20, &lv_font_montserrat_16),
      IVORY, 30, 236, 142);
  set_left_transform_origin(state->secondary_heading);
  lv_obj_set_style_transform_scale_x(
      state->secondary_heading, 285, 0);
  lv_obj_set_style_transform_scale_y(
      state->secondary_heading, 235, 0);
  set_tracking(state->secondary_heading, 1);

  state->secondary_value = make_content_label(
      state->root, "--",
      font_or(state->display_font_56, &lv_font_montserrat_48),
      IVORY, SECONDARY_LEFT_X, 243);
  lv_obj_set_style_transform_scale_y(
      state->secondary_value, 274, 0);
  state->secondary_unit = make_content_label(
      state->root, "",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      IVORY, 0, 266);
  lv_obj_set_style_transform_scale_y(
      state->secondary_unit, 280, 0);
  set_instrument_outline(state->secondary_value);
  set_instrument_outline(state->secondary_unit);
}

void make_quota(GundamThemeState* state) {
  state->quota_heading = make_label(
      state->root, "7D REMAINS",
      font_or(state->display_font_20, &lv_font_montserrat_16),
      IVORY, 30, 337, 104);
  set_left_transform_origin(state->quota_heading);
  lv_obj_set_style_transform_scale_x(
      state->quota_heading, 275, 0);
  lv_obj_set_style_transform_scale_y(
      state->quota_heading, 235, 0);
  set_tracking(state->quota_heading, 1);

  state->quota_value = make_content_label(
      state->root, "--",
      font_or(state->display_font_56, &lv_font_montserrat_48),
      IVORY, 26, 339);
  lv_obj_set_style_transform_scale_y(
      state->quota_value, 300, 0);
  state->quota_unit = make_content_label(
      state->root, "%",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      IVORY, 0, 366);
  set_instrument_outline(state->quota_value);
  set_instrument_outline(state->quota_unit);

  for (int i = 0; i < QUOTA_CELL_COUNT; ++i) {
    int x = 113 + i * 19;
    state->quota_cells[i] = make_rect(
        state->root, x, 357, 16, 42, CELL_EMPTY, 0);
    lv_obj_set_style_border_width(
        state->quota_cells[i], 1, 0);
    lv_obj_set_style_border_color(
        state->quota_cells[i], CELL_BORDER, 0);
    state->quota_fills[i] = make_rect(
        state->quota_cells[i], 1, 1, 1, 40,
        SIGNAL_YELLOW, 0);
    lv_obj_add_flag(
        state->quota_fills[i], LV_OBJ_FLAG_HIDDEN);
  }
}

void make_reset(GundamThemeState* state) {
  state->reset_heading = make_label(
      state->root, "RESET",
      font_or(state->display_font_20, &lv_font_montserrat_16),
      SIGNAL_RED, 333, 338, 124);
  set_left_transform_origin(state->reset_heading);
  lv_obj_set_style_transform_scale_x(
      state->reset_heading, 245, 0);
  lv_obj_set_style_transform_scale_y(
      state->reset_heading, 235, 0);
  set_tracking(state->reset_heading, 1);

  state->reset_day_value = make_content_label(
      state->root, "--",
      font_or(state->display_font_52, &lv_font_montserrat_48),
      SIGNAL_RED, 0, 340);
  lv_obj_set_style_transform_scale_y(
      state->reset_day_value, 300, 0);
  state->reset_day_unit = make_content_label(
      state->root, "D",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      SIGNAL_RED, 0, 365);
  state->reset_hour_value = make_content_label(
      state->root, "--",
      font_or(state->display_font_52, &lv_font_montserrat_48),
      SIGNAL_RED, 0, 340);
  lv_obj_set_style_transform_scale_y(
      state->reset_hour_value, 300, 0);
  state->reset_hour_unit = make_content_label(
      state->root, "H",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      SIGNAL_RED, 0, 365);
  set_instrument_outline(state->reset_day_value);
  set_instrument_outline(state->reset_day_unit);
  set_instrument_outline(state->reset_hour_value);
  set_instrument_outline(state->reset_hour_unit);
}

void make_status(GundamThemeState* state) {
  state->task_value = make_content_label(
      state->root, "IDLE",
      font_or(state->display_font_20, &lv_font_montserrat_16),
      IVORY, 30, 429 + STATUS_Y_OFFSET);
  set_left_transform_origin(state->task_value);
  lv_obj_set_style_transform_scale_y(
      state->task_value, 270, 0);
  set_tracking(state->task_value, 1);

  for (int i = 0; i < TASK_LAMP_COUNT; ++i) {
    int center_x = 125 + i * 28;
    state->task_lamps[i] = make_rect(
        state->root, center_x - 9,
        428 + STATUS_Y_OFFSET, 19, 19,
        PANEL_DARK, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(
        state->task_lamps[i], 1, 0);
    lv_obj_set_style_border_color(
        state->task_lamps[i], CELL_BORDER, 0);
    state->task_lamp_cores[i] = make_rect(
        state->root, center_x - 6,
        431 + STATUS_Y_OFFSET, 13, 13,
        SENSOR_GREEN_BRIGHT, LV_RADIUS_CIRCLE);
    lv_obj_add_flag(
        state->task_lamp_cores[i], LV_OBJ_FLAG_HIDDEN);
  }

  state->ble_value = make_label(
      state->root, "BLE",
      font_or(state->display_font_20, &lv_font_montserrat_16),
      IVORY, 322, 429 + STATUS_Y_OFFSET,
      61, LV_TEXT_ALIGN_CENTER);
  lv_obj_set_style_transform_pivot_x(
      state->ble_value, 30, 0);
  lv_obj_set_style_transform_scale_x(
      state->ble_value, 280, 0);
  lv_obj_set_style_transform_scale_y(
      state->ble_value, 270, 0);
  set_tracking(state->ble_value, 1);

  state->sync_value = make_label(
      state->root, "WAIT",
      font_or(state->display_font_20, &lv_font_montserrat_16),
      STEEL, 392, 429 + STATUS_Y_OFFSET,
      66, LV_TEXT_ALIGN_CENTER);
  lv_obj_set_style_transform_pivot_x(
      state->sync_value, 33, 0);
  lv_obj_set_style_transform_scale_x(
      state->sync_value, 290, 0);
  lv_obj_set_style_transform_scale_y(
      state->sync_value, 270, 0);
  set_tracking(state->sync_value, 1);
}

int scale_for_value(size_t length, bool primary) {
  if (primary) {
    if (length >= 7) return 187;
    if (length >= 6) return 205;
    if (length >= 5) return 230;
    return 233;
  }
  if (length >= 7) return 180;
  if (length >= 6) return 204;
  if (length >= 5) return 238;
  return 275;
}

void update_primary(
    GundamThemeState* state,
    const DashboardViewModel& model) {
  const char* source =
      model.token_usage_mode
          ? model.today_tokens_text
          : model.h5_percent_text;
  set_label_text_if_changed(
      state->primary_heading,
      model.token_usage_mode
          ? "TODAY TOKEN"
          : "5H REMAINS");

  char value[24];
  char unit[4];
  split_suffix(
      source, !model.token_usage_mode,
      value, sizeof(value), unit, sizeof(unit));
  set_label_text_if_changed(state->primary_value, value);
  set_label_text_if_changed(state->primary_unit, unit);
  int value_scale = scale_for_value(strlen(source), true);
  int unit_scale = 210;
  lv_obj_set_style_transform_scale_x(
      state->primary_value, value_scale, 0);
  lv_obj_set_style_transform_scale_x(
      state->primary_unit, unit_scale, 0);
  if (unit[0]) {
    lv_obj_clear_flag(
        state->primary_unit, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(
        state->primary_unit, LV_OBJ_FLAG_HIDDEN);
  }
  position_value_group_from_left(
      state->primary_value, value_scale,
      state->primary_unit,
      PRIMARY_LEFT_X, 5);
}

void update_secondary(
    GundamThemeState* state,
    const DashboardViewModel& model) {
  char token_source[24];
  if (model.token_usage_mode) {
    format_gundam_week_tokens(
        token_source, sizeof(token_source), model);
  } else {
    strlcpy(
        token_source, model.d7_percent_text,
        sizeof(token_source));
  }
  const char* source = token_source;
  set_label_text_if_changed(
      state->secondary_heading,
      model.token_usage_mode
          ? "7 DAYS"
          : "7D REMAINS");

  char value[24];
  char unit[4];
  split_suffix(
      source, !model.token_usage_mode,
      value, sizeof(value), unit, sizeof(unit));
  set_label_text_if_changed(state->secondary_value, value);
  set_label_text_if_changed(state->secondary_unit, unit);
  int value_scale = scale_for_value(strlen(source), false);
  int unit_scale = 205;
  lv_obj_set_style_transform_scale_x(
      state->secondary_value, value_scale, 0);
  lv_obj_set_style_transform_scale_x(
      state->secondary_unit, unit_scale, 0);
  if (unit[0]) {
    lv_obj_clear_flag(
        state->secondary_unit, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(
        state->secondary_unit, LV_OBJ_FLAG_HIDDEN);
  }
  position_value_group_from_left(
      state->secondary_value, value_scale,
      state->secondary_unit,
      SECONDARY_LEFT_X, 0);
}

void update_quota(
    GundamThemeState* state,
    const DashboardViewModel& model) {
  char value[12];
  char unit[4];
  split_suffix(
      model.d7_percent_text, true,
      value, sizeof(value), unit, sizeof(unit));
  set_label_text_if_changed(state->quota_value, value);
  set_label_text_if_changed(state->quota_unit, "%");
  size_t length = strlen(value);
  int value_scale =
      length >= 3 ? 220 : 272;
  int unit_scale = 220;
  lv_obj_set_style_transform_scale_x(
      state->quota_value, value_scale, 0);
  lv_obj_set_style_transform_scale_x(
      state->quota_unit, unit_scale, 0);
  position_value_group_from_left(
      state->quota_value, value_scale,
      state->quota_unit,
      QUOTA_LEFT_X, 2);

  int percent = model.d7_remaining;
  if (percent < 0) percent = -1;
  if (percent > 100) percent = 100;
  if (percent == state->quota_percent) return;
  state->quota_percent = percent;

  for (int i = 0; i < QUOTA_CELL_COUNT; ++i) {
    int fill_percent =
        percent < 0
            ? 0
            : (percent - i * 10) * 10;
    if (fill_percent < 0) fill_percent = 0;
    if (fill_percent > 100) fill_percent = 100;
    int width = (14 * fill_percent + 99) / 100;
    if (width <= 0) {
      lv_obj_add_flag(
          state->quota_fills[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_set_width(state->quota_fills[i], width);
      lv_obj_clear_flag(
          state->quota_fills[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void format_reset_parts(
    int32_t seconds,
    char* days, size_t days_size,
    char* hours, size_t hours_size) {
  if (seconds < 0) {
    strlcpy(days, "--", days_size);
    strlcpy(hours, "--", hours_size);
    return;
  }
  long total_hours = (seconds + 3599L) / 3600L;
  snprintf(days, days_size, "%02ld", total_hours / 24L);
  snprintf(hours, hours_size, "%02ld", total_hours % 24L);
}

void position_reset_group(GundamThemeState* state) {
  constexpr int VALUE_SCALE = 258;
  constexpr int UNIT_SCALE = 220;
  constexpr int UNIT_GAP = 1;
  constexpr int PART_GAP = 6;

  int day_width =
      scaled_label_width(
          state->reset_day_value, VALUE_SCALE);
  int day_unit_width =
      scaled_label_width(
          state->reset_day_unit, UNIT_SCALE);
  int hour_width =
      scaled_label_width(
          state->reset_hour_value, VALUE_SCALE);
  int hour_unit_width =
      scaled_label_width(
          state->reset_hour_unit, UNIT_SCALE);
  int group_width =
      day_width + UNIT_GAP + day_unit_width +
      PART_GAP + hour_width + UNIT_GAP + hour_unit_width;
  int x = RESET_CENTER_X - group_width / 2;
  lv_obj_set_x(state->reset_day_value, x);
  x += day_width + UNIT_GAP;
  lv_obj_set_x(state->reset_day_unit, x);
  x += day_unit_width + PART_GAP;
  lv_obj_set_x(state->reset_hour_value, x);
  x += hour_width + UNIT_GAP;
  lv_obj_set_x(state->reset_hour_unit, x);
}

void update_reset(
    GundamThemeState* state,
    const DashboardViewModel& model) {
  char days[8];
  char hours[8];
  format_reset_parts(
      model.d7_reset_seconds,
      days, sizeof(days), hours, sizeof(hours));
  set_label_text_if_changed(state->reset_day_value, days);
  set_label_text_if_changed(state->reset_hour_value, hours);
  constexpr int VALUE_SCALE = 258;
  constexpr int UNIT_SCALE = 220;
  for (lv_obj_t* value :
       {state->reset_day_value, state->reset_hour_value}) {
    lv_obj_set_style_transform_scale_x(
        value, VALUE_SCALE, 0);
  }
  for (lv_obj_t* unit :
       {state->reset_day_unit, state->reset_hour_unit}) {
    lv_obj_set_style_transform_scale_x(
        unit, UNIT_SCALE, 0);
  }
  position_reset_group(state);
}

void update_battery(
    GundamThemeState* state, int percent, bool charging) {
  char text[12];
  if (percent < 0) {
    strlcpy(text, charging ? "USB" : "--%", sizeof(text));
  } else {
    snprintf(text, sizeof(text), "%d%%", percent);
  }
  set_label_text_if_changed(
      state->battery_heading,
      charging ? "BATTERY +" : "BATTERY");
  set_label_text_if_changed(state->battery_value, text);

  int scale =
      strlen(text) >= 4 ? 190
                        : (strlen(text) == 3 ? 220 : 240);
  lv_obj_set_style_transform_scale_x(
      state->battery_value, scale, 0);
  lv_color_t color =
      percent >= 0 && percent <= 10
          ? SIGNAL_RED
          : (percent >= 0 && percent <= 20
                 ? SIGNAL_YELLOW
                 : (percent < 0 ? STEEL : SENSOR_GREEN));
  if (charging) color = SIGNAL_YELLOW;
  lv_obj_set_style_text_color(
      state->battery_heading, color, 0);
  lv_obj_set_style_text_color(
      state->battery_value, color, 0);
}

void update_activity(
    GundamThemeState* state, int running_count) {
  if (running_count < 0) running_count = 0;
  if (running_count == state->running_count) return;
  state->running_count = running_count;
  state->task_lamp_opacity = LV_OPA_TRANSP;

  char text[24];
  if (running_count <= 0) {
    strlcpy(text, "IDLE", sizeof(text));
  } else if (running_count == 1) {
    strlcpy(text, "1 TASK", sizeof(text));
  } else if (running_count > 99) {
    strlcpy(text, "99+ TASKS", sizeof(text));
  } else {
    snprintf(
        text, sizeof(text),
        "%d TASKS", running_count);
  }
  set_label_text_if_changed(state->task_value, text);
  size_t length = strlen(text);
  int scale =
      length >= 9 ? 245
                  : (length >= 8 ? 270 : 300);
  lv_obj_set_style_transform_scale_x(
      state->task_value, scale, 0);

  int active =
      running_count > TASK_LAMP_COUNT
          ? TASK_LAMP_COUNT
          : running_count;
  for (int i = 0; i < TASK_LAMP_COUNT; ++i) {
    bool on = i < active;
    lv_obj_set_style_bg_color(
        state->task_lamps[i],
        PANEL_DARK, 0);
    lv_obj_set_style_border_color(
        state->task_lamps[i],
        on ? SENSOR_GREEN : CELL_BORDER, 0);
    if (on) {
      lv_obj_clear_flag(
          state->task_lamp_cores[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(
          state->task_lamp_cores[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_shadow_width(
        state->task_lamp_cores[i], on ? 3 : 0, 0);
    lv_obj_set_style_shadow_color(
        state->task_lamp_cores[i], SENSOR_GREEN, 0);
    lv_obj_set_style_shadow_opa(
        state->task_lamp_cores[i],
        on ? LV_OPA_50 : LV_OPA_TRANSP, 0);
  }
}

const char* sync_text_for(DashboardDataState data_state) {
  switch (data_state) {
    case DashboardDataState::Ready:
      return "SYNC";
    case DashboardDataState::Stale:
      return "STALE";
    case DashboardDataState::Error:
      return "ERROR";
    case DashboardDataState::Waiting:
    default:
      return "WAIT";
  }
}

void update_data_state(
    GundamThemeState* state,
    DashboardDataState data_state) {
  if (state->data_state_initialized &&
      data_state == state->data_state) {
    return;
  }
  state->data_state_initialized = true;
  state->data_state = data_state;
  set_label_text_if_changed(
      state->sync_value, sync_text_for(data_state));
  lv_color_t color =
      data_state == DashboardDataState::Error
          ? SIGNAL_RED
          : (data_state == DashboardDataState::Stale
                 ? SIGNAL_YELLOW
                 : (data_state == DashboardDataState::Ready
                        ? SENSOR_GREEN
                        : STEEL));
  lv_obj_set_style_text_color(
      state->sync_value, color, 0);
}

bool gundam_mount(
    void* raw_state, lv_obj_t* parent,
    const ThemeResources& resources) {
  auto* state =
      static_cast<GundamThemeState*>(raw_state);
  *state = GundamThemeState{};
  state->resources = resources;

  if (!load_background(state)) return false;

#if LV_USE_TINY_TTF && LV_TINY_TTF_FILE_SUPPORT
  state->display_font_100 = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, 100,
      LV_FONT_KERNING_NORMAL, 24);
  state->display_font_64 = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, 64,
      LV_FONT_KERNING_NORMAL, 24);
  state->display_font_56 = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, 56,
      LV_FONT_KERNING_NORMAL, 24);
  state->display_font_52 = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, 52,
      LV_FONT_KERNING_NORMAL, 24);
  state->display_font_36 = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, 36,
      LV_FONT_KERNING_NORMAL, 32);
  state->display_font_32 = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, 32,
      LV_FONT_KERNING_NORMAL, 32);
  state->display_font_20 = lv_tiny_ttf_create_file_ex(
      INSTRUMENT_FONT_PATH, 20,
      LV_FONT_KERNING_NORMAL, 32);
  if (state->display_font_100) {
    state->display_font_100->fallback = resources.token_font;
  }
  if (state->display_font_64) {
    state->display_font_64->fallback =
        &lv_font_montserrat_48;
  }
  if (state->display_font_56) {
    state->display_font_56->fallback =
        &lv_font_montserrat_48;
  }
  if (state->display_font_52) {
    state->display_font_52->fallback =
        &lv_font_montserrat_48;
  }
  if (state->display_font_36) {
    state->display_font_36->fallback =
        &lv_font_montserrat_32;
  }
  if (state->display_font_32) {
    state->display_font_32->fallback =
        &lv_font_montserrat_32;
  }
  if (state->display_font_20) {
    state->display_font_20->fallback =
        &lv_font_montserrat_16;
  }
#endif

  state->root = lv_obj_create(parent);
  strip_obj(state->root);
  lv_obj_set_size(
      state->root, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(state->root, 0, 0);
  lv_obj_set_style_bg_color(
      state->root, lv_color_hex(0x071426), 0);
  lv_obj_set_style_bg_opa(
      state->root, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(state->root, 28, 0);
  lv_obj_set_style_clip_corner(state->root, true, 0);

  make_background(state);
  make_sensor_connector(state);
  make_battery(state);
  make_primary(state);
  make_secondary(state);
  make_quota(state);
  make_reset(state);
  make_status(state);
  return true;
}

void gundam_update(
    void* raw_state, const DashboardViewModel& model) {
  auto* state =
      static_cast<GundamThemeState*>(raw_state);
  update_primary(state, model);
  update_secondary(state, model);
  update_quota(state, model);
  update_reset(state, model);
  update_battery(
      state, model.battery_percent, model.charging);
  update_activity(state, model.running_count);
  update_data_state(state, model.data_state);
}

void gundam_tick(void* raw_state, uint32_t now_ms) {
  auto* state =
      static_cast<GundamThemeState*>(raw_state);
  int active =
      state->running_count > TASK_LAMP_COUNT
          ? TASK_LAMP_COUNT
          : state->running_count;
  if (active <= 0) return;

  constexpr uint32_t PERIOD_MS = 2800;
  constexpr uint32_t HALF_MS = PERIOD_MS / 2;
  uint32_t phase = now_ms % PERIOD_MS;
  uint32_t ramp =
      phase <= HALF_MS ? phase : PERIOD_MS - phase;
  uint32_t level = (4UL * ramp) / HALF_MS;
  lv_opa_t opacity =
      static_cast<lv_opa_t>(
          240 + (15UL * level) / 4UL);
  if (opacity == state->task_lamp_opacity) return;
  state->task_lamp_opacity = opacity;
  for (int i = 0; i < active; ++i) {
    lv_obj_set_style_bg_opa(
        state->task_lamp_cores[i], opacity, 0);
  }
}

void gundam_unmount(void* raw_state) {
  auto* state =
      static_cast<GundamThemeState*>(raw_state);
  if (state->root) lv_obj_delete(state->root);
  state->root = nullptr;

#if LV_USE_TINY_TTF
  if (state->display_font_100) {
    lv_tiny_ttf_destroy(state->display_font_100);
  }
  if (state->display_font_64) {
    lv_tiny_ttf_destroy(state->display_font_64);
  }
  if (state->display_font_56) {
    lv_tiny_ttf_destroy(state->display_font_56);
  }
  if (state->display_font_52) {
    lv_tiny_ttf_destroy(state->display_font_52);
  }
  if (state->display_font_36) {
    lv_tiny_ttf_destroy(state->display_font_36);
  }
  if (state->display_font_32) {
    lv_tiny_ttf_destroy(state->display_font_32);
  }
  if (state->display_font_20) {
    lv_tiny_ttf_destroy(state->display_font_20);
  }
#endif
  state->display_font_100 = nullptr;
  state->display_font_64 = nullptr;
  state->display_font_56 = nullptr;
  state->display_font_52 = nullptr;
  state->display_font_36 = nullptr;
  state->display_font_32 = nullptr;
  state->display_font_20 = nullptr;

  if (state->background_pixels) {
    heap_caps_free(state->background_pixels);
  }
  state->background_pixels = nullptr;
  state->background_dsc = lv_image_dsc_t{};
}

const ThemeDashboardOps GUNDAM_DASHBOARD = {
    sizeof(GundamThemeState),
    gundam_mount,
    gundam_update,
    gundam_tick,
    gundam_unmount,
};

const ThemePack GUNDAM_THEME = {
    "gundam",
    "Gundam",
    1,
    CODEXMETER_BURN_IN_DRIFT_MAX_PX,
    GUNDAM_DASHBOARD,
    nullptr,
    nullptr,
};

static_assert(
    sizeof(GundamThemeState) <= CODEXMETER_THEME_STATE_BYTES,
    "Gundam theme state exceeds ThemeRuntime storage");

}  // namespace

const ThemePack& gundam_theme_pack() {
  return GUNDAM_THEME;
}
