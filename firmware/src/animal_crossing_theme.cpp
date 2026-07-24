#include "animal_crossing_theme.h"

#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "device_log.h"

namespace {

constexpr char BACKGROUND_PATH[] =
    "S:/themes/animal_crossing_bg.rgb565";
constexpr char DISPLAY_FONT_PATH[] =
    "S:/fonts/Fredoka-SemiBold-Latin.ttf";
constexpr size_t BACKGROUND_BYTES =
    CODEXMETER_SCREEN_W * CODEXMETER_SCREEN_H * sizeof(uint16_t);
constexpr int QUOTA_CELL_COUNT = 10;
constexpr int PRIMARY_HEADING_X = 80;
constexpr int PRIMARY_HEADING_WIDTH = 170;
constexpr int PRIMARY_VALUE_X = 43;
constexpr int PRIMARY_VALUE_WIDTH = 234;
constexpr int PRIMARY_VALUE_CENTER_X = 160;
constexpr int SECONDARY_VALUE_CENTER_X = 378;
constexpr int PRIMARY_UNIT_SCALE_X = 240;
constexpr int SECONDARY_UNIT_SCALE_X = 275;
constexpr int RESET_VALUE_CENTER_X = 265;
constexpr int RESET_VALUE_SCALE_X = 265;
constexpr int RESET_UNIT_SCALE_X = 195;

const lv_color_t ISLAND_GREEN = lv_color_hex(0x255444);
const lv_color_t ISLAND_GREEN_DIM = lv_color_hex(0x7D955D);
const lv_color_t LEAF_EMPTY = lv_color_hex(0xD1DA96);
const lv_color_t CORAL = lv_color_hex(0xC97C70);
const lv_color_t WOOD_BROWN = lv_color_hex(0x574232);
const lv_color_t STATUS_GOLD = lv_color_hex(0x8D7049);

struct AnimalCrossingThemeState {
  lv_obj_t* root = nullptr;
  lv_obj_t* background = nullptr;
  lv_obj_t* battery_value = nullptr;
  lv_obj_t* primary_heading = nullptr;
  lv_obj_t* primary_value = nullptr;
  lv_obj_t* primary_unit = nullptr;
  lv_obj_t* secondary_heading = nullptr;
  lv_obj_t* secondary_value = nullptr;
  lv_obj_t* secondary_unit = nullptr;
  lv_obj_t* quota_heading = nullptr;
  lv_obj_t* quota_value = nullptr;
  lv_obj_t* quota_percent = nullptr;
  lv_obj_t* quota_cells[QUOTA_CELL_COUNT]{};
  lv_obj_t* reset_heading = nullptr;
  lv_obj_t* reset_day_value = nullptr;
  lv_obj_t* reset_day_unit = nullptr;
  lv_obj_t* reset_hour_value = nullptr;
  lv_obj_t* reset_hour_unit = nullptr;
  lv_obj_t* task_value = nullptr;
  lv_obj_t* sync_value = nullptr;
  lv_font_t* display_font_64 = nullptr;
  lv_font_t* display_font_52 = nullptr;
  lv_font_t* display_font_36 = nullptr;
  lv_font_t* display_font_26 = nullptr;
  uint8_t* background_pixels = nullptr;
  lv_image_dsc_t background_dsc{};
  ThemeResources resources{};
  int quota_active = -1;
  int running_count = -1;
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

lv_obj_t* make_centered_label(
    lv_obj_t* parent, const char* text, const lv_font_t* font,
    lv_color_t color, int x, int y, int width) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_width(label, width);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_letter_space(label, 0, 0);
  lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  return label;
}

lv_obj_t* make_content_label(
    lv_obj_t* parent, const char* text, const lv_font_t* font,
    lv_color_t color, int x, int y) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_text_letter_space(label, 0, 0);
  lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  return label;
}

bool set_label_text_if_changed(lv_obj_t* label, const char* text) {
  const char* current = lv_label_get_text(label);
  if (current && strcmp(current, text) == 0) return false;
  lv_label_set_text(label, text);
  return true;
}

const lv_font_t* font_or(
    const lv_font_t* preferred, const lv_font_t* fallback) {
  return preferred ? preferred : fallback;
}

bool load_background(AnimalCrossingThemeState* state) {
  state->background_pixels = static_cast<uint8_t*>(
      heap_caps_malloc(
          BACKGROUND_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!state->background_pixels) {
    device_logf(
        "ERROR", "animal_crossing background alloc failed bytes=%lu",
        static_cast<unsigned long>(BACKGROUND_BYTES));
    return false;
  }

  lv_fs_file_t file;
  lv_fs_res_t result =
      lv_fs_open(&file, BACKGROUND_PATH, LV_FS_MODE_RD);
  if (result != LV_FS_RES_OK) {
    device_logf(
        "ERROR", "animal_crossing background open failed path=%s res=%d",
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
    if (result != LV_FS_RES_OK || bytes_read == 0) {
      break;
    }
    total += bytes_read;
  }
  lv_fs_close(&file);

  if (result != LV_FS_RES_OK || total != BACKGROUND_BYTES) {
    device_logf(
        "ERROR",
        "animal_crossing background read failed bytes=%lu expected=%lu res=%d",
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

void make_background(AnimalCrossingThemeState* state) {
  state->background = lv_image_create(state->root);
  lv_image_set_src(
      state->background, &state->background_dsc);
  lv_obj_set_pos(state->background, 0, 0);
}

void make_battery(AnimalCrossingThemeState* state) {
  state->battery_value = make_centered_label(
      state->root, "--%",
      font_or(state->display_font_26, &lv_font_montserrat_24),
      ISLAND_GREEN, 400, 44, 70);
  lv_obj_set_style_transform_scale_x(
      state->battery_value,
      state->display_font_26 ? 240 : 250, 0);
  lv_obj_set_style_transform_scale_y(
      state->battery_value,
      state->display_font_26 ? 250 : 255, 0);
}

void make_primary_panel(AnimalCrossingThemeState* state) {
  state->primary_heading = make_centered_label(
      state->root, "TODAY TOKEN",
      font_or(state->display_font_26, &lv_font_montserrat_24),
      ISLAND_GREEN, PRIMARY_HEADING_X, 158,
      PRIMARY_HEADING_WIDTH);
  lv_obj_set_style_text_letter_space(
      state->primary_heading, 0, 0);
  lv_obj_set_style_transform_pivot_x(
      state->primary_heading, PRIMARY_HEADING_WIDTH / 2, 0);
  lv_obj_set_style_transform_scale_x(
      state->primary_heading, 150, 0);
  lv_obj_set_style_transform_scale_y(
      state->primary_heading, 230, 0);

  state->primary_value = make_centered_label(
      state->root, "--",
      font_or(state->display_font_64, state->resources.token_font),
      ISLAND_GREEN, PRIMARY_VALUE_X, 178,
      PRIMARY_VALUE_WIDTH);
  lv_obj_set_style_text_letter_space(
      state->primary_value, 0, 0);
  lv_obj_set_style_transform_pivot_x(
      state->primary_value, PRIMARY_VALUE_WIDTH / 2, 0);
  lv_obj_set_style_transform_scale_y(
      state->primary_value, 275, 0);

  state->primary_unit = make_content_label(
      state->root, "",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      ISLAND_GREEN, 0, 199);
  lv_obj_set_style_transform_pivot_x(
      state->primary_unit, 0, 0);
  lv_obj_set_style_transform_scale_x(
      state->primary_unit, PRIMARY_UNIT_SCALE_X, 0);
  lv_obj_set_style_transform_scale_y(
      state->primary_unit, 340, 0);
}

void make_secondary_panel(AnimalCrossingThemeState* state) {
  state->secondary_heading = make_centered_label(
      state->root, "7 DAYS",
      font_or(state->display_font_26, &lv_font_montserrat_24),
      ISLAND_GREEN, 322, 133, 150);
  lv_obj_set_style_text_letter_space(
      state->secondary_heading, 0, 0);
  lv_obj_set_style_transform_scale_x(
      state->secondary_heading, 180, 0);
  lv_obj_set_style_transform_scale_y(
      state->secondary_heading, 225, 0);

  state->secondary_value = make_centered_label(
      state->root, "--",
      font_or(state->display_font_52, &lv_font_montserrat_48),
      ISLAND_GREEN, 313, 157, 160);
  lv_obj_set_style_text_letter_space(
      state->secondary_value, 0, 0);
  lv_obj_set_style_transform_scale_y(
      state->secondary_value,
      state->display_font_52 ? 280 : 255, 0);

  state->secondary_unit = make_content_label(
      state->root, "",
      font_or(state->display_font_26, &lv_font_montserrat_24),
      ISLAND_GREEN, 0, 177);
  lv_obj_set_style_transform_pivot_x(
      state->secondary_unit, 0, 0);
  lv_obj_set_style_transform_scale_x(
      state->secondary_unit, SECONDARY_UNIT_SCALE_X, 0);
  lv_obj_set_style_transform_scale_y(
      state->secondary_unit,
      state->display_font_26 ? 360 : 335, 0);
}

void make_quota_panel(AnimalCrossingThemeState* state) {
  state->quota_heading = make_centered_label(
      state->root, "7D REMAINS",
      font_or(state->display_font_26, &lv_font_montserrat_24),
      ISLAND_GREEN, 71, 287, 170);
  lv_obj_set_style_transform_scale_x(
      state->quota_heading, 130, 0);
  lv_obj_set_style_transform_scale_y(
      state->quota_heading, 165, 0);

  state->quota_value = make_centered_label(
      state->root, "--",
      font_or(state->display_font_52, &lv_font_montserrat_48),
      ISLAND_GREEN, 68, 299, 86);
  lv_obj_set_style_text_letter_space(
      state->quota_value, 0, 0);
  lv_obj_set_style_transform_scale_y(
      state->quota_value,
      state->display_font_52 ? 235 : 245, 0);

  state->quota_percent = make_centered_label(
      state->root, "%",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      ISLAND_GREEN, 138, 315, 28);
  lv_obj_set_style_transform_scale_x(
      state->quota_percent, 165, 0);
  lv_obj_set_style_transform_scale_y(
      state->quota_percent, 220, 0);

  for (int i = 0; i < QUOTA_CELL_COUNT; ++i) {
    int x = 42 + i * 12;
    state->quota_cells[i] =
        make_rect(
            state->root, x, 355, 8, 12,
            LEAF_EMPTY, 5);
    lv_obj_set_style_border_width(
        state->quota_cells[i], 1, 0);
    lv_obj_set_style_border_color(
        state->quota_cells[i], ISLAND_GREEN_DIM, 0);
    lv_obj_set_style_transform_rotation(
        state->quota_cells[i], -280, 0);
  }
}

void make_reset_panel(AnimalCrossingThemeState* state) {
  state->reset_heading = make_centered_label(
      state->root, "RESET",
      font_or(state->display_font_26, &lv_font_montserrat_24),
      WOOD_BROWN, 246, 287, 70);
  lv_obj_set_style_transform_scale_x(
      state->reset_heading, 150, 0);
  lv_obj_set_style_transform_scale_y(
      state->reset_heading, 165, 0);

  state->reset_day_value = make_content_label(
      state->root, "--",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      WOOD_BROWN, 0, 297);
  state->reset_hour_value = make_content_label(
      state->root, "--",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      WOOD_BROWN, 0, 297);
  for (lv_obj_t* value :
       {state->reset_day_value, state->reset_hour_value}) {
    lv_obj_set_style_transform_pivot_x(value, 0, 0);
    lv_obj_set_style_transform_scale_x(
        value, RESET_VALUE_SCALE_X, 0);
    lv_obj_set_style_transform_scale_y(value, 270, 0);
  }

  state->reset_day_unit = make_content_label(
      state->root, "D",
      font_or(state->display_font_26, &lv_font_montserrat_24),
      WOOD_BROWN, 0, 310);
  state->reset_hour_unit = make_content_label(
      state->root, "H",
      font_or(state->display_font_26, &lv_font_montserrat_24),
      WOOD_BROWN, 0, 310);
  for (lv_obj_t* unit :
       {state->reset_day_unit, state->reset_hour_unit}) {
    lv_obj_set_style_transform_pivot_x(unit, 0, 0);
    lv_obj_set_style_transform_scale_x(
        unit, RESET_UNIT_SCALE_X, 0);
    lv_obj_set_style_transform_scale_y(unit, 240, 0);
  }
}

void make_status_strip(AnimalCrossingThemeState* state) {
  state->task_value = make_centered_label(
      state->root, "IDLE",
      font_or(state->display_font_26, &lv_font_montserrat_24),
      ISLAND_GREEN, 98, 425, 102);
  lv_obj_set_style_transform_scale_x(
      state->task_value, 230, 0);
  lv_obj_set_style_transform_scale_y(
      state->task_value, 255, 0);

  state->sync_value = make_centered_label(
      state->root, "WAIT",
      font_or(state->display_font_26, &lv_font_montserrat_24),
      ISLAND_GREEN, 352, 428, 82);
  lv_obj_set_style_transform_scale_x(
      state->sync_value, 225, 0);
  lv_obj_set_style_transform_scale_y(
      state->sync_value, 235, 0);

}

void split_compact_value(
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
  } else if (unit_size > 0) {
    unit[0] = '\0';
  }
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
  int group_x = center_x - group_width / 2;
  lv_obj_set_x(value, group_x);
  if (has_unit) {
    lv_obj_set_x(unit, group_x + value_width + gap);
  }
}

void position_reset_group(AnimalCrossingThemeState* state) {
  int day_value_width = scaled_label_width(
      state->reset_day_value, RESET_VALUE_SCALE_X);
  int day_unit_width = scaled_label_width(
      state->reset_day_unit, RESET_UNIT_SCALE_X);
  int hour_value_width = scaled_label_width(
      state->reset_hour_value, RESET_VALUE_SCALE_X);
  int hour_unit_width = scaled_label_width(
      state->reset_hour_unit, RESET_UNIT_SCALE_X);
  constexpr int UNIT_GAP = 1;
  constexpr int PART_GAP = 7;
  int group_width =
      day_value_width + UNIT_GAP + day_unit_width +
      PART_GAP +
      hour_value_width + UNIT_GAP + hour_unit_width;
  int x = RESET_VALUE_CENTER_X - group_width / 2;
  lv_obj_set_x(state->reset_day_value, x);
  x += day_value_width + UNIT_GAP;
  lv_obj_set_x(state->reset_day_unit, x);
  x += day_unit_width + PART_GAP;
  lv_obj_set_x(state->reset_hour_value, x);
  x += hour_value_width + UNIT_GAP;
  lv_obj_set_x(state->reset_hour_unit, x);
}

void update_primary(
    AnimalCrossingThemeState* state,
    const DashboardViewModel& model) {
  if (model.token_usage_mode) {
    char value[24];
    char unit[4];
    split_compact_value(
        model.today_tokens_text,
        value, sizeof(value), unit, sizeof(unit));
    set_label_text_if_changed(
        state->primary_heading, "TODAY TOKEN");
    lv_obj_set_x(
        state->primary_heading, PRIMARY_HEADING_X);
    lv_obj_set_width(
        state->primary_heading, PRIMARY_HEADING_WIDTH);
    lv_obj_set_style_transform_scale_x(
        state->primary_heading, 150, 0);
    set_label_text_if_changed(state->primary_value, value);
    set_label_text_if_changed(state->primary_unit, unit);
    lv_obj_set_width(state->primary_value, LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(
        state->primary_value, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_transform_pivot_x(
        state->primary_value, 0, 0);
    size_t length = strlen(model.today_tokens_text);
    int value_scale_x =
        length >= 7 ? 185 : (length >= 5 ? 210 : 248);
    lv_obj_set_style_transform_scale_x(
        state->primary_value, value_scale_x, 0);
    if (unit[0]) {
      lv_obj_clear_flag(
          state->primary_unit, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(
          state->primary_unit, LV_OBJ_FLAG_HIDDEN);
    }
    position_value_group(
        state->primary_value, value_scale_x,
        state->primary_unit, PRIMARY_UNIT_SCALE_X,
        PRIMARY_VALUE_CENTER_X, 2);
  } else {
    set_label_text_if_changed(
        state->primary_heading, "5H REMAINS");
    lv_obj_set_x(
        state->primary_heading, PRIMARY_HEADING_X);
    lv_obj_set_width(
        state->primary_heading, PRIMARY_HEADING_WIDTH);
    lv_obj_set_style_transform_scale_x(
        state->primary_heading, 200, 0);
    set_label_text_if_changed(
        state->primary_value, model.h5_percent_text);
    lv_obj_add_flag(
        state->primary_unit, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(state->primary_value, PRIMARY_VALUE_X);
    lv_obj_set_width(
        state->primary_value, PRIMARY_VALUE_WIDTH);
    lv_obj_set_style_text_align(
        state->primary_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_transform_pivot_x(
        state->primary_value, PRIMARY_VALUE_WIDTH / 2, 0);
    size_t length = strlen(model.h5_percent_text);
    int value_scale_x =
        length >= 7 ? 185 : (length >= 5 ? 210 : 248);
    lv_obj_set_style_transform_scale_x(
        state->primary_value, value_scale_x, 0);
  }
}

void update_secondary(
    AnimalCrossingThemeState* state,
    const DashboardViewModel& model) {
  if (model.token_usage_mode) {
    char value[24];
    char unit[4];
    split_compact_value(
        model.last_7d_tokens_text,
        value, sizeof(value), unit, sizeof(unit));
    set_label_text_if_changed(
        state->secondary_heading, "7 DAYS");
    lv_obj_set_x(state->secondary_heading, 322);
    lv_obj_set_style_transform_scale_x(
        state->secondary_heading, 180, 0);
    set_label_text_if_changed(
        state->secondary_value, value);
    set_label_text_if_changed(
        state->secondary_unit, unit);
    lv_obj_set_width(
        state->secondary_value, LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(
        state->secondary_value, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_transform_pivot_x(
        state->secondary_value, 0, 0);
    size_t length = strlen(model.last_7d_tokens_text);
    int value_scale_x =
        length >= 7 ? 178 : (length >= 5 ? 190 : 205);
    lv_obj_set_style_transform_scale_x(
        state->secondary_value, value_scale_x, 0);
    if (unit[0]) {
      lv_obj_clear_flag(
          state->secondary_unit, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(
          state->secondary_unit, LV_OBJ_FLAG_HIDDEN);
    }
    position_value_group(
        state->secondary_value, value_scale_x,
        state->secondary_unit, SECONDARY_UNIT_SCALE_X,
        SECONDARY_VALUE_CENTER_X, 2);
  } else {
    set_label_text_if_changed(
        state->secondary_heading, "7D REMAINS");
    lv_obj_set_x(state->secondary_heading, 316);
    lv_obj_set_style_transform_scale_x(
        state->secondary_heading, 150, 0);
    set_label_text_if_changed(
        state->secondary_value, model.d7_percent_text);
    lv_obj_add_flag(
        state->secondary_unit, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(state->secondary_value, 313);
    lv_obj_set_width(state->secondary_value, 160);
    lv_obj_set_style_text_align(
        state->secondary_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_transform_pivot_x(
        state->secondary_value, 0, 0);
    size_t length = strlen(model.d7_percent_text);
    int value_scale_x =
        length >= 7 ? 178 : (length >= 5 ? 190 : 205);
    lv_obj_set_style_transform_scale_x(
        state->secondary_value, value_scale_x, 0);
  }
}

void update_quota(
    AnimalCrossingThemeState* state,
    const DashboardViewModel& model) {
  char value[12];
  strlcpy(value, model.d7_percent_text, sizeof(value));
  size_t length = strlen(value);
  if (length > 0 && value[length - 1] == '%') {
    value[--length] = '\0';
  }
  if (set_label_text_if_changed(state->quota_value, value)) {
    bool three_digits = length >= 3;
    lv_obj_set_x(
        state->quota_value, three_digits ? 64 : 75);
    lv_obj_set_width(
        state->quota_value, three_digits ? 104 : 86);
    int scale =
        three_digits ? 190 : (length == 2 ? 215 : 240);
    lv_obj_set_style_transform_scale_x(
        state->quota_value, scale, 0);
    lv_obj_set_x(
        state->quota_percent, 138);
  }

  int active =
      model.d7_remaining < 0
          ? 0
          : (model.d7_remaining * QUOTA_CELL_COUNT) / 100;
  if (active < 0) active = 0;
  if (active > QUOTA_CELL_COUNT) {
    active = QUOTA_CELL_COUNT;
  }
  if (active == state->quota_active) return;
  state->quota_active = active;
  for (int i = 0; i < QUOTA_CELL_COUNT; ++i) {
    lv_obj_set_style_bg_color(
        state->quota_cells[i],
        i < active ? ISLAND_GREEN : LEAF_EMPTY, 0);
    lv_obj_set_style_border_color(
        state->quota_cells[i],
        i < active ? ISLAND_GREEN : ISLAND_GREEN_DIM, 0);
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

void update_reset(
    AnimalCrossingThemeState* state,
    const DashboardViewModel& model) {
  char days[8];
  char hours[8];
  format_reset_parts(
      model.d7_reset_seconds,
      days, sizeof(days), hours, sizeof(hours));
  set_label_text_if_changed(state->reset_day_value, days);
  set_label_text_if_changed(state->reset_hour_value, hours);
  position_reset_group(state);
}

void update_battery(
    AnimalCrossingThemeState* state,
    int percent, bool charging) {
  char text[12];
  if (percent < 0) {
    strlcpy(text, charging ? "USB" : "--%", sizeof(text));
  } else {
    snprintf(
        text, sizeof(text), "%d%%%s",
        percent, charging ? "+" : "");
  }
  if (set_label_text_if_changed(state->battery_value, text)) {
    size_t length = strlen(text);
    int scale =
        length >= 5 ? 160 : (length == 4 ? 185 : 215);
    lv_obj_set_style_transform_scale_x(
        state->battery_value, scale, 0);
  }
  lv_obj_set_style_text_color(
      state->battery_value,
      charging
          ? STATUS_GOLD
          : (percent >= 0 && percent <= 20
                 ? CORAL
                 : ISLAND_GREEN),
      0);
}

void update_activity(
    AnimalCrossingThemeState* state, int running_count) {
  if (running_count < 0) running_count = 0;
  if (running_count == state->running_count) return;
  state->running_count = running_count;

  char text[20];
  if (running_count <= 0) {
    strlcpy(text, "IDLE", sizeof(text));
  } else if (running_count == 1) {
    strlcpy(text, "1 TASK", sizeof(text));
  } else if (running_count > 99) {
    strlcpy(text, "99+ TASKS", sizeof(text));
  } else {
    snprintf(text, sizeof(text), "%d TASKS", running_count);
  }
  int x =
      running_count <= 0 ? 80
                         : (running_count == 1 ? 94 : 98);
  lv_obj_set_x(state->task_value, x);
  lv_obj_set_y(
      state->task_value, 427);
  set_label_text_if_changed(state->task_value, text);
  int scale =
      strlen(text) >= 9 ? 185
                        : (strlen(text) >= 7 ? 215 : 230);
  lv_obj_set_style_transform_scale_x(
      state->task_value, scale, 0);
  lv_obj_set_style_text_color(
      state->task_value, ISLAND_GREEN, 0);
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
    AnimalCrossingThemeState* state,
    DashboardDataState data_state) {
  if (state->data_state_initialized &&
      data_state == state->data_state) {
    return;
  }
  state->data_state_initialized = true;
  state->data_state = data_state;
  set_label_text_if_changed(
      state->sync_value, sync_text_for(data_state));
  lv_obj_set_style_text_color(
      state->sync_value,
      data_state == DashboardDataState::Error
          ? CORAL
          : (data_state == DashboardDataState::Stale
                 ? STATUS_GOLD
                 : ISLAND_GREEN),
      0);
}

bool animal_crossing_mount(
    void* raw_state, lv_obj_t* parent,
    const ThemeResources& resources) {
  auto* state =
      static_cast<AnimalCrossingThemeState*>(raw_state);
  *state = AnimalCrossingThemeState{};
  state->resources = resources;

  if (!load_background(state)) return false;

#if LV_USE_TINY_TTF && LV_TINY_TTF_FILE_SUPPORT
  state->display_font_64 = lv_tiny_ttf_create_file_ex(
      DISPLAY_FONT_PATH, 64,
      LV_FONT_KERNING_NORMAL, 20);
  state->display_font_52 = lv_tiny_ttf_create_file_ex(
      DISPLAY_FONT_PATH, 52,
      LV_FONT_KERNING_NORMAL, 24);
  state->display_font_36 = lv_tiny_ttf_create_file_ex(
      DISPLAY_FONT_PATH, 36,
      LV_FONT_KERNING_NORMAL, 32);
  state->display_font_26 = lv_tiny_ttf_create_file_ex(
      DISPLAY_FONT_PATH, 26,
      LV_FONT_KERNING_NORMAL, 32);
  if (state->display_font_64) {
    state->display_font_64->fallback = resources.token_font;
  }
  if (state->display_font_52) {
    state->display_font_52->fallback =
        &lv_font_montserrat_48;
  }
  if (state->display_font_36) {
    state->display_font_36->fallback =
        &lv_font_montserrat_32;
  }
  if (state->display_font_26) {
    state->display_font_26->fallback =
        &lv_font_montserrat_24;
  }
#endif

  state->root = lv_obj_create(parent);
  strip_obj(state->root);
  lv_obj_set_size(
      state->root, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(state->root, 0, 0);
  lv_obj_set_style_bg_color(
      state->root, lv_color_hex(0xBDE9E2), 0);
  lv_obj_set_style_bg_opa(
      state->root, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(state->root, 28, 0);
  lv_obj_set_style_clip_corner(state->root, true, 0);

  make_background(state);
  make_battery(state);
  make_primary_panel(state);
  make_secondary_panel(state);
  make_quota_panel(state);
  make_reset_panel(state);
  make_status_strip(state);
  return true;
}

void animal_crossing_update(
    void* raw_state, const DashboardViewModel& model) {
  auto* state =
      static_cast<AnimalCrossingThemeState*>(raw_state);
  update_primary(state, model);
  update_secondary(state, model);
  update_quota(state, model);
  update_reset(state, model);
  update_battery(
      state, model.battery_percent, model.charging);
  update_activity(state, model.running_count);
  update_data_state(state, model.data_state);
}

void animal_crossing_tick(
    void* raw_state, uint32_t now_ms) {
  (void)raw_state;
  (void)now_ms;
}

void animal_crossing_unmount(void* raw_state) {
  auto* state =
      static_cast<AnimalCrossingThemeState*>(raw_state);
  if (state->root) {
    lv_obj_delete(state->root);
  }
  state->root = nullptr;

#if LV_USE_TINY_TTF
  if (state->display_font_64) {
    lv_tiny_ttf_destroy(state->display_font_64);
  }
  if (state->display_font_52) {
    lv_tiny_ttf_destroy(state->display_font_52);
  }
  if (state->display_font_36) {
    lv_tiny_ttf_destroy(state->display_font_36);
  }
  if (state->display_font_26) {
    lv_tiny_ttf_destroy(state->display_font_26);
  }
#endif
  state->display_font_64 = nullptr;
  state->display_font_52 = nullptr;
  state->display_font_36 = nullptr;
  state->display_font_26 = nullptr;

  if (state->background_pixels) {
    heap_caps_free(state->background_pixels);
  }
  state->background_pixels = nullptr;
  state->background_dsc = lv_image_dsc_t{};
}

const ThemeDashboardOps ANIMAL_CROSSING_DASHBOARD = {
    sizeof(AnimalCrossingThemeState),
    animal_crossing_mount,
    animal_crossing_update,
    animal_crossing_tick,
    animal_crossing_unmount,
};

const ThemePack ANIMAL_CROSSING_THEME = {
    "animal_crossing",
    "Animal Crossing",
    1,
    CODEXMETER_BURN_IN_DRIFT_MAX_PX,
    ANIMAL_CROSSING_DASHBOARD,
    nullptr,
    nullptr,
};

static_assert(
    sizeof(AnimalCrossingThemeState) <=
        CODEXMETER_THEME_STATE_BYTES,
    "Animal Crossing theme state exceeds ThemeRuntime storage");

}  // namespace

const ThemePack& animal_crossing_theme_pack() {
  return ANIMAL_CROSSING_THEME;
}
