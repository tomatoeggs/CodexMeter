#include "famicom_theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

namespace {

constexpr int SHARE_CELL_COUNT = 18;
constexpr int QUOTA_CELL_COUNT = 12;
constexpr int BATTERY_CELL_COUNT = 4;
constexpr int BATTERY_VALUE_CENTER_X = 424;
constexpr int DATA_PANEL_TOP = 89;
constexpr int LOWER_PANEL_TOP = 279;
constexpr int SECTION_HEADING_TOP_INSET = 10;
constexpr int SECTION_HEADING_SCALE_X = 255;
constexpr int SECTION_HEADING_SCALE_Y = 390;
constexpr int RESET_PANEL_CENTER_X = 373;
constexpr int RESET_DAY_CENTER_X = 334;
constexpr int RESET_HOUR_CENTER_X = 411;
constexpr int RESET_TIME_SCALE_X = 210;
constexpr int BLE_PILL_CENTER_X = 268;
constexpr int SYNC_PILL_CENTER_X = 332;
constexpr int B_BUTTON_CENTER_X = 397;
constexpr int A_BUTTON_CENTER_X = 434;
constexpr char DISPLAY_FONT_PATH[] =
    "S:/fonts/BarlowCondensed-Black.ttf";
constexpr uint32_t ANIMATION_FRAME_MS = 80;
constexpr uint32_t POWER_PULSE_MS = 2400;
constexpr float FAMICOM_TWO_PI = 6.28318530718f;

const lv_color_t SHELL_LIGHT = lv_color_hex(0xF6EFD9);
const lv_color_t SHELL_DARK = lv_color_hex(0xD8CCB0);
const lv_color_t SHELL_PANEL = lv_color_hex(0xD4C8AF);
const lv_color_t SHELL_RAIL = lv_color_hex(0xC8BDA6);
const lv_color_t SHELL_BORDER = lv_color_hex(0x8D8171);
const lv_color_t IVORY = lv_color_hex(0xFFF7E5);
const lv_color_t WINE = lv_color_hex(0x86172A);
const lv_color_t DATA_RED = lv_color_hex(0xD1263E);
const lv_color_t POWER_RED = lv_color_hex(0xFF314C);
const lv_color_t GOLD = lv_color_hex(0xB89443);
const lv_color_t DATA_DISPLAY = lv_color_hex(0x25211F);
const lv_color_t DISPLAY_LINE = lv_color_hex(0x4A4440);
const lv_color_t DISPLAY_DIM = lv_color_hex(0x8E8781);
const lv_color_t DISPLAY_LABEL = lv_color_hex(0xD2C5AA);
const lv_color_t CONTROL_DARK = lv_color_hex(0x302C29);
const lv_color_t BUTTON_DARK = lv_color_hex(0x4D4742);
const lv_color_t CONTROL_TEXT = lv_color_hex(0x665E55);
const lv_color_t CONTROL_DIM = lv_color_hex(0x746B60);
const lv_color_t EMPTY_CELL = lv_color_hex(0xC9BFA9);

struct FamicomThemeState {
  lv_obj_t* root = nullptr;
  lv_obj_t* power_led = nullptr;
  lv_obj_t* battery_value = nullptr;
  lv_obj_t* battery_cells[BATTERY_CELL_COUNT]{};
  lv_obj_t* primary_heading = nullptr;
  lv_obj_t* primary_value = nullptr;
  lv_obj_t* primary_unit = nullptr;
  lv_obj_t* secondary_heading = nullptr;
  lv_obj_t* secondary_value = nullptr;
  lv_obj_t* secondary_unit = nullptr;
  lv_obj_t* share_text = nullptr;
  lv_obj_t* share_cells[SHARE_CELL_COUNT]{};
  lv_obj_t* quota_value = nullptr;
  lv_obj_t* quota_unit = nullptr;
  lv_obj_t* quota_cells[QUOTA_CELL_COUNT]{};
  lv_obj_t* reset_days = nullptr;
  lv_obj_t* reset_hours = nullptr;
  lv_obj_t* active_value = nullptr;
  lv_obj_t* active_state = nullptr;
  lv_obj_t* sync_pill = nullptr;
  lv_obj_t* sync_text = nullptr;
  lv_obj_t* b_button = nullptr;
  lv_obj_t* a_button = nullptr;
  lv_font_t* display_font_80 = nullptr;
  lv_font_t* display_font_52 = nullptr;
  lv_font_t* display_font_36 = nullptr;
  ThemeResources resources{};
  int share_active = -1;
  int quota_active = -1;
  int running_count = -1;
  DashboardDataState data_state = DashboardDataState::Waiting;
  bool data_state_initialized = false;
  bool battery_state_initialized = false;
  bool battery_alert = false;
  int battery_fill = -1;
  bool battery_charging = false;
  uint32_t animation_started_ms = 0;
  uint32_t animation_last_frame_ms = 0;
};

void strip_obj(lv_obj_t* obj) {
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

lv_obj_t* make_rect(
    lv_obj_t* parent, int x, int y, int width, int height,
    lv_color_t color, lv_opa_t opacity = LV_OPA_COVER) {
  lv_obj_t* rect = lv_obj_create(parent);
  strip_obj(rect);
  lv_obj_set_size(rect, width, height);
  lv_obj_set_pos(rect, x, y);
  lv_obj_set_style_bg_color(rect, color, 0);
  lv_obj_set_style_bg_opa(rect, opacity, 0);
  return rect;
}

lv_obj_t* make_round_rect(
    lv_obj_t* parent, int x, int y, int width, int height,
    int radius, lv_color_t color) {
  lv_obj_t* rect = make_rect(parent, x, y, width, height, color);
  lv_obj_set_style_radius(rect, radius, 0);
  return rect;
}

lv_obj_t* make_panel(
    lv_obj_t* parent, int x, int y, int width, int height,
    lv_color_t color, lv_color_t border, int border_width,
    int radius = 0) {
  lv_obj_t* panel =
      make_round_rect(parent, x, y, width, height, radius, color);
  lv_obj_set_style_border_width(panel, border_width, 0);
  lv_obj_set_style_border_color(panel, border, 0);
  lv_obj_set_style_border_opa(panel, LV_OPA_COVER, 0);
  return panel;
}

lv_obj_t* make_circle(
    lv_obj_t* parent, int center_x, int center_y, int diameter,
    lv_color_t color) {
  return make_round_rect(
      parent, center_x - diameter / 2, center_y - diameter / 2,
      diameter, diameter, LV_RADIUS_CIRCLE, color);
}

lv_obj_t* make_label(
    lv_obj_t* parent, const char* text, const lv_font_t* font,
    lv_color_t color, int x, int y) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_letter_space(label, 0, 0);
  lv_obj_set_pos(label, x, y);
  return label;
}

bool set_label_text_if_changed(lv_obj_t* label, const char* text) {
  const char* current = lv_label_get_text(label);
  if (current && strcmp(current, text) == 0) return false;
  lv_label_set_text(label, text);
  return true;
}

void scale_label(
    lv_obj_t* label, int scale_x, int scale_y,
    int outline_width = 0) {
  lv_obj_set_style_transform_pivot_x(label, 0, 0);
  lv_obj_set_style_transform_pivot_y(label, 0, 0);
  lv_obj_set_style_transform_scale_x(label, scale_x, 0);
  lv_obj_set_style_transform_scale_y(label, scale_y, 0);
  if (outline_width > 0) {
    lv_color_t color =
        lv_obj_get_style_text_color(label, LV_PART_MAIN);
    lv_obj_set_style_text_outline_stroke_color(label, color, 0);
    lv_obj_set_style_text_outline_stroke_width(
        label, outline_width, 0);
    lv_obj_set_style_text_outline_stroke_opa(
        label, LV_OPA_COVER, 0);
  }
}

void center_scaled_label_x(
    lv_obj_t* label, int center_x, int scale_x) {
  lv_obj_update_layout(label);
  int scaled_width =
      (lv_obj_get_width(label) * scale_x + 128) / 256;
  int x = center_x - scaled_width / 2;
  if (lv_obj_get_x(label) != x) {
    lv_obj_set_x(label, x);
  }
}

int scaled_label_width(lv_obj_t* label, int scale_x) {
  lv_obj_update_layout(label);
  return (lv_obj_get_width(label) * scale_x + 128) / 256;
}

const lv_font_t* font_or(
    const lv_font_t* preferred, const lv_font_t* fallback) {
  return preferred ? preferred : fallback;
}

void rotate_rect(lv_obj_t* obj, int rotation_tenths) {
  lv_obj_set_style_transform_pivot_x(
      obj, lv_obj_get_width(obj) / 2, 0);
  lv_obj_set_style_transform_pivot_y(
      obj, lv_obj_get_height(obj) / 2, 0);
  lv_obj_set_style_transform_rotation(obj, rotation_tenths, 0);
}

void make_shell(FamicomThemeState* state) {
  lv_obj_set_style_bg_color(state->root, SHELL_LIGHT, 0);
  lv_obj_set_style_bg_grad_color(state->root, SHELL_DARK, 0);
  lv_obj_set_style_bg_grad_dir(state->root, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(state->root, LV_OPA_COVER, 0);

  for (int y = 8; y < 472; y += 16) {
    make_rect(
        state->root, 18, y, 444, 1,
        lv_color_hex(0xFFFFFF), 7);
  }
  for (int x = 24; x < 458; x += 22) {
    make_rect(
        state->root, x, 0, 1, 480,
        lv_color_hex(0x6D6254), 7);
  }

  make_rect(state->root, 0, 0, 18, 480, WINE);
  make_rect(state->root, 462, 0, 18, 480, WINE);
}

void make_header(FamicomThemeState* state) {
  make_rect(state->root, 18, 0, 444, 72, WINE);

  make_rect(state->root, 18, 58, 388, 14, SHELL_LIGHT);
  lv_obj_t* notch =
      make_rect(state->root, 399, 55, 20, 20, SHELL_LIGHT);
  rotate_rect(notch, 450);

  make_rect(state->root, 306, 0, 156, 28, GOLD);
  for (int y = 1; y < 28; ++y) {
    int cover = (y * 26 + 27) / 28;
    make_rect(state->root, 306, y, cover, 1, WINE);
  }

  lv_obj_t* logo = make_label(
      state->root, "CODEX METER",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      IVORY, 34, 2);
  lv_obj_set_style_text_letter_space(logo, 1, 0);
  scale_label(
      logo, state->display_font_36 ? 213 : 200,
      state->display_font_36 ? 260 : 285,
      state->display_font_36 ? 0 : 2);

  lv_obj_t* subtitle = make_label(
      state->root, "FAMILY TOKEN SYSTEM / MODEL CM-01",
      &lv_font_montserrat_14, lv_color_hex(0xE7CFC6), 35, 36);
  lv_obj_set_style_text_letter_space(subtitle, 2, 0);
  scale_label(subtitle, 175, 300);

  make_panel(
      state->root, 330, 10, 130, 43,
      WINE, GOLD, 1, 3);

  state->power_led =
      make_circle(state->root, 344, 32, 10, POWER_RED);
  lv_obj_set_style_shadow_color(state->power_led, POWER_RED, 0);
  lv_obj_set_style_shadow_width(state->power_led, 8, 0);
  lv_obj_set_style_shadow_spread(state->power_led, 1, 0);
  lv_obj_set_style_shadow_opa(state->power_led, LV_OPA_50, 0);

  lv_obj_t* power_label = make_label(
      state->root, "PWR", &lv_font_montserrat_14,
      IVORY, 353, 23);
  lv_obj_set_style_text_letter_space(power_label, 1, 0);
  scale_label(power_label, 165, 300);
  make_rect(state->root, 386, 18, 1, 28, GOLD);

  state->battery_value = make_label(
      state->root, "--%", &lv_font_montserrat_14,
      IVORY, 399, 13);
  scale_label(state->battery_value, 270, 330, 1);
  center_scaled_label_x(
      state->battery_value, BATTERY_VALUE_CENTER_X, 270);

  make_panel(
      state->root, 394, 33, 55, 14,
      WINE, GOLD, 1, 1);
  make_rect(state->root, 449, 37, 5, 7, GOLD);
  for (int i = 0; i < BATTERY_CELL_COUNT; ++i) {
    state->battery_cells[i] = make_rect(
        state->root, 397 + i * 12, 37, 10, 7,
        CONTROL_DIM);
  }
}

void make_data_display(FamicomThemeState* state) {
  make_panel(
      state->root, 28, DATA_PANEL_TOP, 424, 174,
      DATA_DISPLAY, GOLD, 3, 5);
  make_rect(state->root, 38, 96, 404, 2, DISPLAY_LINE);
  make_rect(state->root, 281, 96, 2, 156, DISPLAY_LINE);

  state->primary_heading = make_label(
      state->root, "TODAY / TOKEN",
      &lv_font_montserrat_14, DISPLAY_LABEL, 46,
      DATA_PANEL_TOP + SECTION_HEADING_TOP_INSET);
  lv_obj_set_style_text_letter_space(state->primary_heading, 2, 0);
  scale_label(
      state->primary_heading,
      SECTION_HEADING_SCALE_X, SECTION_HEADING_SCALE_Y);

  state->primary_value = make_label(
      state->root, "--",
      font_or(state->display_font_80, state->resources.token_font),
      IVORY, 42, 115);
  lv_obj_set_style_text_letter_space(state->primary_value, -1, 0);
  scale_label(
      state->primary_value,
      state->display_font_80 ? 260 : 286,
      state->display_font_80 ? 219 : 258,
      state->display_font_80 ? 0 : 3);
  state->primary_unit = make_label(
      state->root, "",
      font_or(state->display_font_36, &lv_font_montserrat_24),
      DATA_RED, 237, 144);
  scale_label(
      state->primary_unit,
      state->display_font_36 ? 240 : 250,
      state->display_font_36 ? 290 : 320,
      state->display_font_36 ? 0 : 1);

  state->secondary_heading = make_label(
      state->root, "7 DAYS", &lv_font_montserrat_14,
      DISPLAY_LABEL, 298,
      DATA_PANEL_TOP + SECTION_HEADING_TOP_INSET);
  lv_obj_set_style_text_letter_space(state->secondary_heading, 2, 0);
  scale_label(
      state->secondary_heading,
      SECTION_HEADING_SCALE_X, SECTION_HEADING_SCALE_Y);

  state->secondary_value = make_label(
      state->root, "--",
      font_or(state->display_font_80, state->resources.token_font),
      IVORY, 295, 115);
  lv_obj_set_style_text_letter_space(state->secondary_value, -1, 0);
  scale_label(
      state->secondary_value,
      state->display_font_80 ? 260 : 286,
      state->display_font_80 ? 219 : 258,
      state->display_font_80 ? 0 : 3);
  state->secondary_unit = make_label(
      state->root, "",
      font_or(state->display_font_36, &lv_font_montserrat_24),
      GOLD, 394, 144);
  scale_label(
      state->secondary_unit,
      state->display_font_36 ? 240 : 250,
      state->display_font_36 ? 290 : 320,
      state->display_font_36 ? 0 : 1);

  state->share_text = make_label(
      state->root, "TODAY / 7D  --",
      &lv_font_montserrat_14, DISPLAY_DIM, 46, 204);
  lv_obj_set_style_text_letter_space(state->share_text, 1, 0);
  scale_label(state->share_text, 225, 340);

  for (int i = 0; i < SHARE_CELL_COUNT; ++i) {
    state->share_cells[i] = make_rect(
        state->root, 46 + i * 11, 236, 8, 8,
        DISPLAY_LINE);
  }
}

void make_quota_card(FamicomThemeState* state) {
  lv_obj_t* card = make_panel(
      state->root, 28, LOWER_PANEL_TOP, 248, 110,
      IVORY, WINE, 3);
  lv_obj_set_style_shadow_color(card, WINE, 0);
  lv_obj_set_style_shadow_width(card, 3, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);

  for (int i = 0; i < 14; ++i) {
    make_rect(
        state->root, 28, 375 + i, i + 1, 1,
        SHELL_DARK);
    make_rect(
        state->root, 28 + i, 375 + i, 3, 1,
        WINE);
  }

  lv_obj_t* heading = make_label(
      state->root, "7D LIMIT / REMAINS",
      &lv_font_montserrat_14, WINE, 45,
      LOWER_PANEL_TOP + SECTION_HEADING_TOP_INSET);
  lv_obj_set_style_text_letter_space(heading, 2, 0);
  scale_label(
      heading,
      SECTION_HEADING_SCALE_X, SECTION_HEADING_SCALE_Y);

  state->quota_value = make_label(
      state->root, "--",
      font_or(state->display_font_80, state->resources.percent_font),
      WINE, 42, 299);
  lv_obj_set_style_text_letter_space(state->quota_value, -2, 0);
  scale_label(
      state->quota_value,
      state->display_font_80 ? 300 : 330,
      state->display_font_80 ? 219 : 258,
      state->display_font_80 ? 0 : 3);
  state->quota_unit = make_label(
      state->root, "%", &lv_font_montserrat_24,
      GOLD, 134, 341);
  scale_label(state->quota_unit, 240, 310, 1);

  for (int i = 0; i < QUOTA_CELL_COUNT; ++i) {
    state->quota_cells[i] = make_rect(
        state->root, 174 + i * 8, 330, 6, 29,
        EMPTY_CELL);
  }
}

void make_reset_panel(FamicomThemeState* state) {
  make_panel(
      state->root, 293, LOWER_PANEL_TOP, 159, 110,
      SHELL_PANEL, SHELL_BORDER, 1, 5);

  lv_obj_t* heading = make_label(
      state->root, "RESET WINDOW",
      &lv_font_montserrat_14, CONTROL_TEXT, 310,
      LOWER_PANEL_TOP + SECTION_HEADING_TOP_INSET);
  lv_obj_set_style_text_letter_space(heading, 2, 0);
  scale_label(heading, 225, 345);
  center_scaled_label_x(
      heading, RESET_PANEL_CENTER_X, 225);

  make_round_rect(state->root, 299, 312, 70, 43, 22, WINE);
  make_round_rect(state->root, 376, 312, 70, 43, 22, WINE);
  state->reset_days = make_label(
      state->root, "--D",
      font_or(state->display_font_52, &lv_font_montserrat_32),
      IVORY, 306, 304);
  scale_label(
      state->reset_days,
      state->display_font_52 ? RESET_TIME_SCALE_X : 190,
      state->display_font_52 ? 225 : 205,
      state->display_font_52 ? 0 : 1);
  center_scaled_label_x(
      state->reset_days, RESET_DAY_CENTER_X,
      state->display_font_52 ? RESET_TIME_SCALE_X : 190);
  state->reset_hours = make_label(
      state->root, "--H",
      font_or(state->display_font_52, &lv_font_montserrat_32),
      IVORY, 383, 304);
  scale_label(
      state->reset_hours,
      state->display_font_52 ? RESET_TIME_SCALE_X : 190,
      state->display_font_52 ? 225 : 205,
      state->display_font_52 ? 0 : 1);
  center_scaled_label_x(
      state->reset_hours, RESET_HOUR_CENTER_X,
      state->display_font_52 ? RESET_TIME_SCALE_X : 190);

  lv_obj_t* select_label = make_label(
      state->root, "SELECT", &lv_font_montserrat_14,
      CONTROL_DIM, 319, 360);
  scale_label(select_label, 185, 310);
  center_scaled_label_x(
      select_label, RESET_DAY_CENTER_X, 185);
  lv_obj_t* start_label = make_label(
      state->root, "START", &lv_font_montserrat_14,
      CONTROL_DIM, 389, 360);
  scale_label(start_label, 185, 310);
  center_scaled_label_x(
      start_label, RESET_HOUR_CENTER_X, 185);
}

void make_dpad(lv_obj_t* parent) {
  make_round_rect(parent, 51, 417, 16, 38, 2, CONTROL_DARK);
  make_round_rect(parent, 40, 428, 38, 16, 2, CONTROL_DARK);
  make_rect(parent, 54, 431, 10, 10, BUTTON_DARK);
}

void make_controller_rail(FamicomThemeState* state) {
  make_panel(
      state->root, 28, 404, 424, 62,
      SHELL_RAIL, SHELL_BORDER, 1);
  make_dpad(state->root);

  lv_obj_t* heading = make_label(
      state->root, "ACTIVE TASKS", &lv_font_montserrat_14,
      CONTROL_TEXT, 97, 407);
  lv_obj_set_style_text_letter_space(heading, 1, 0);
  scale_label(heading, 215, 330);
  state->active_value = make_label(
      state->root, "00",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      WINE, 97, 423);
  scale_label(
      state->active_value,
      state->display_font_36 ? 230 : 205,
      state->display_font_36 ? 235 : 255,
      state->display_font_36 ? 0 : 2);
  state->active_state = make_label(
      state->root, "IDLE", &lv_font_montserrat_14,
      CONTROL_TEXT, 131, 433);
  scale_label(state->active_state, 210, 330);

  make_round_rect(state->root, 240, 428, 55, 16, 8, BUTTON_DARK);
  state->sync_pill =
      make_round_rect(state->root, 304, 428, 55, 16, 8, BUTTON_DARK);
  lv_obj_t* ble_label = make_label(
      state->root, "BLE", &lv_font_montserrat_14,
      SHELL_LIGHT, 254, 426);
  scale_label(ble_label, 180, 300);
  center_scaled_label_x(
      ble_label, BLE_PILL_CENTER_X, 180);
  state->sync_text = make_label(
      state->root, "SYNC", &lv_font_montserrat_14,
      SHELL_LIGHT, 313, 427);
  scale_label(state->sync_text, 170, 300);
  center_scaled_label_x(
      state->sync_text, SYNC_PILL_CENTER_X, 170);

  state->b_button =
      make_circle(state->root, 397, 435, 34, WINE);
  state->a_button =
      make_circle(state->root, 434, 435, 34, WINE);
  lv_obj_t* b_label = make_label(
      state->root, "B", &lv_font_montserrat_14,
      IVORY, 389, 425);
  lv_obj_t* a_label = make_label(
      state->root, "A", &lv_font_montserrat_14,
      IVORY, 426, 425);
  scale_label(b_label, 240, 340, 1);
  scale_label(a_label, 240, 340, 1);
  center_scaled_label_x(
      b_label, B_BUTTON_CENTER_X, 240);
  center_scaled_label_x(
      a_label, A_BUTTON_CENTER_X, 240);
}

void split_compact_value(
    const char* source, char* value, size_t value_size,
    char* unit, size_t unit_size, bool pad_primary) {
  if (!source || !source[0] || strcmp(source, "--") == 0) {
    strlcpy(value, "--", value_size);
    unit[0] = '\0';
    return;
  }

  size_t length = strlen(source);
  char last = source[length - 1];
  bool has_unit = (last >= 'A' && last <= 'Z');
  size_t value_length = has_unit ? length - 1 : length;
  if (value_length >= value_size) value_length = value_size - 1;
  memcpy(value, source, value_length);
  value[value_length] = '\0';
  if (has_unit) {
    unit[0] = last;
    unit[1] = '\0';
  } else {
    unit[0] = '\0';
  }

  if (pad_primary && value_length > 0 && value_length < 5) {
    size_t digits_before_decimal = strcspn(value, ".");
    if (digits_before_decimal < 3 && value_length + 1 < value_size) {
      memmove(value + 1, value, value_length + 1);
      value[0] = '0';
    }
  }
}

void format_percent_value(
    int percent, char* value, size_t value_size) {
  if (percent < 0) {
    strlcpy(value, "--", value_size);
  } else {
    snprintf(value, value_size, "%d", percent);
  }
}

void format_share(
    const DashboardViewModel& model,
    char* output, size_t output_size) {
  if (!model.token_usage_mode) {
    if (model.h5_remaining < 0) {
      strlcpy(output, "WINDOW BALANCE  --", output_size);
    } else {
      snprintf(
          output, output_size,
          "WINDOW BALANCE  %d%%", model.h5_remaining);
    }
    return;
  }
  if (!model.has_today_share) {
    strlcpy(output, "TODAY / 7D  --", output_size);
    return;
  }
  unsigned int permille = model.today_share_permille;
  snprintf(
      output, output_size, "TODAY / 7D  %u.%u%%",
      permille / 10U, permille % 10U);
}

void format_reset_parts(
    int32_t seconds,
    char* days, size_t days_size,
    char* hours, size_t hours_size) {
  if (seconds < 0) {
    strlcpy(days, "--D", days_size);
    strlcpy(hours, "--H", hours_size);
    return;
  }
  long total_hours = (seconds + 3599L) / 3600L;
  snprintf(days, days_size, "%02ldD", total_hours / 24L);
  snprintf(hours, hours_size, "%02ldH", total_hours % 24L);
}

void update_primary(
    FamicomThemeState* state,
    const DashboardViewModel& model) {
  char value[24];
  char unit[4];
  if (model.token_usage_mode) {
    split_compact_value(
        model.today_tokens_text,
        value, sizeof(value), unit, sizeof(unit), true);
    set_label_text_if_changed(
        state->primary_heading, "TODAY / TOKEN");
  } else {
    format_percent_value(
        model.h5_remaining, value, sizeof(value));
    strlcpy(unit, "%", sizeof(unit));
    set_label_text_if_changed(
        state->primary_heading, "5H / REMAINS");
  }

  bool value_changed =
      set_label_text_if_changed(state->primary_value, value);
  set_label_text_if_changed(state->primary_unit, unit);
  size_t length = strlen(value);
  int scale_x =
      state->display_font_80
          ? (length >= 6 ? 228 : (length == 5 ? 260 : 301))
          : (length >= 6 ? 248 : (length == 5 ? 286 : 330));
  if (value_changed) {
    lv_obj_set_style_transform_scale_x(
        state->primary_value, scale_x, 0);
  }
  int unit_x =
      length >= 6 ? 249 : (length == 5 ? 237 : 202);
  if (lv_obj_get_x(state->primary_unit) != unit_x) {
    lv_obj_set_x(state->primary_unit, unit_x);
  }
}

void update_secondary(
    FamicomThemeState* state,
    const DashboardViewModel& model) {
  char value[24];
  char unit[4];
  if (model.token_usage_mode) {
    split_compact_value(
        model.last_7d_tokens_text,
        value, sizeof(value), unit, sizeof(unit), false);
    set_label_text_if_changed(
        state->secondary_heading, "7 DAYS");
  } else {
    format_percent_value(
        model.d7_remaining, value, sizeof(value));
    strlcpy(unit, "%", sizeof(unit));
    set_label_text_if_changed(
        state->secondary_heading, "7D / REMAINS");
  }
  bool changed =
      set_label_text_if_changed(state->secondary_value, value);
  set_label_text_if_changed(state->secondary_unit, unit);
  size_t length = strlen(value);
  int value_scale_x =
      state->display_font_80
          ? (length >= 5 ? 205 : (length == 4 ? 235 : 260))
          : (length >= 5 ? 225 : (length == 4 ? 255 : 286));
  if (changed) {
    lv_obj_set_style_transform_scale_x(
        state->secondary_value, value_scale_x, 0);
  }
  int unit_x =
      lv_obj_get_x(state->secondary_value) +
      scaled_label_width(state->secondary_value, value_scale_x) + 8;
  if (unit_x > 432) unit_x = 432;
  if (lv_obj_get_x(state->secondary_unit) != unit_x) {
    lv_obj_set_x(state->secondary_unit, unit_x);
  }
}

void update_share(
    FamicomThemeState* state,
    const DashboardViewModel& model) {
  char text[40];
  format_share(model, text, sizeof(text));
  set_label_text_if_changed(state->share_text, text);

  int permille = 0;
  bool valid = false;
  if (model.token_usage_mode) {
    valid = model.has_today_share;
    permille = model.today_share_permille;
  } else if (model.h5_remaining >= 0) {
    valid = true;
    permille = model.h5_remaining * 10;
  }
  int active =
      valid
          ? (permille * SHARE_CELL_COUNT + 999) / 1000
          : 0;
  if (active < 0) active = 0;
  if (active > SHARE_CELL_COUNT) active = SHARE_CELL_COUNT;
  if (active == state->share_active) return;
  state->share_active = active;
  for (int i = 0; i < SHARE_CELL_COUNT; ++i) {
    lv_obj_set_style_bg_color(
        state->share_cells[i],
        i < active ? DATA_RED : DISPLAY_LINE, 0);
  }
}

void update_quota(
    FamicomThemeState* state,
    const DashboardViewModel& model) {
  char value[8];
  format_percent_value(
      model.d7_remaining, value, sizeof(value));
  bool changed =
      set_label_text_if_changed(state->quota_value, value);
  size_t length = strlen(value);
  int value_scale_x =
      state->display_font_80
          ? (length >= 3 ? 245 : 300)
          : (length >= 3 ? 270 : 330);
  if (changed) {
    lv_obj_set_style_transform_scale_x(
        state->quota_value, value_scale_x, 0);
  }
  int unit_x =
      lv_obj_get_x(state->quota_value) +
      scaled_label_width(state->quota_value, value_scale_x) + 6;
  if (unit_x > 152) unit_x = 152;
  if (lv_obj_get_x(state->quota_unit) != unit_x) {
    lv_obj_set_x(state->quota_unit, unit_x);
  }

  int active =
      model.d7_remaining < 0
          ? 0
          : (model.d7_remaining * QUOTA_CELL_COUNT + 99) / 100;
  if (active < 0) active = 0;
  if (active > QUOTA_CELL_COUNT) active = QUOTA_CELL_COUNT;
  if (active != state->quota_active) {
    state->quota_active = active;
    for (int i = 0; i < QUOTA_CELL_COUNT; ++i) {
      lv_obj_set_style_bg_color(
          state->quota_cells[i],
          i < active ? WINE : EMPTY_CELL, 0);
    }
  }

  char days[8];
  char hours[8];
  format_reset_parts(
      model.d7_reset_seconds,
      days, sizeof(days), hours, sizeof(hours));
  if (set_label_text_if_changed(state->reset_days, days)) {
    center_scaled_label_x(
        state->reset_days, RESET_DAY_CENTER_X,
        state->display_font_52 ? RESET_TIME_SCALE_X : 190);
  }
  if (set_label_text_if_changed(state->reset_hours, hours)) {
    center_scaled_label_x(
        state->reset_hours, RESET_HOUR_CENTER_X,
        state->display_font_52 ? RESET_TIME_SCALE_X : 190);
  }
}

void update_battery(
    FamicomThemeState* state, int percent, bool charging) {
  char text[16];
  if (percent < 0) {
    strlcpy(text, charging ? "USB" : "--%", sizeof(text));
  } else {
    snprintf(
        text, sizeof(text), "%d%%%s",
        percent, charging ? "+" : "");
  }
  bool text_changed =
      set_label_text_if_changed(state->battery_value, text);
  if (text_changed) {
    size_t length = strlen(text);
    int scale_x =
        length >= 5 ? 210 : (length == 4 ? 240 : 270);
    lv_obj_set_style_transform_scale_x(
        state->battery_value, scale_x, 0);
    center_scaled_label_x(
        state->battery_value,
        BATTERY_VALUE_CENTER_X, scale_x);
  }
  bool battery_alert = percent >= 0 && percent <= 20;
  int battery_fill =
      percent < 0 ? 0 : (percent == 0 ? 0 : (percent + 24) / 25);
  if (battery_fill > BATTERY_CELL_COUNT) {
    battery_fill = BATTERY_CELL_COUNT;
  }
  if (battery_fill != state->battery_fill ||
      charging != state->battery_charging ||
      battery_alert != state->battery_alert) {
    state->battery_fill = battery_fill;
    state->battery_charging = charging;
    lv_color_t active_color =
        charging ? GOLD : (battery_alert ? POWER_RED : IVORY);
    for (int i = 0; i < BATTERY_CELL_COUNT; ++i) {
      lv_obj_set_style_bg_color(
          state->battery_cells[i],
          i < battery_fill ? active_color : CONTROL_DIM, 0);
    }
    lv_obj_set_style_text_color(
        state->battery_value,
        charging ? GOLD : (battery_alert ? POWER_RED : IVORY), 0);
  }
  if (!state->battery_state_initialized ||
      battery_alert != state->battery_alert) {
    state->battery_state_initialized = true;
    state->battery_alert = battery_alert;
  }
}

void update_activity(
    FamicomThemeState* state, int running_count) {
  if (running_count < 0) running_count = 0;
  if (running_count == state->running_count) return;
  state->running_count = running_count;
  char count[8];
  if (running_count > 99) {
    strlcpy(count, "99+", sizeof(count));
  } else {
    snprintf(count, sizeof(count), "%02d", running_count);
  }
  set_label_text_if_changed(state->active_value, count);
  set_label_text_if_changed(
      state->active_state,
      running_count > 0 ? "RUNNING" : "IDLE");
  lv_obj_set_style_text_color(
      state->active_value,
      running_count > 0 ? WINE : CONTROL_DIM, 0);
  if (running_count > 0 && state->animation_started_ms == 0) {
    state->animation_started_ms = millis();
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
    FamicomThemeState* state,
    DashboardDataState data_state) {
  if (state->data_state_initialized &&
      data_state == state->data_state) {
    return;
  }
  state->data_state_initialized = true;
  state->data_state = data_state;
  if (set_label_text_if_changed(
          state->sync_text, sync_text_for(data_state))) {
    center_scaled_label_x(
        state->sync_text, SYNC_PILL_CENTER_X, 170);
  }
  lv_obj_set_style_bg_color(
      state->sync_pill,
      data_state == DashboardDataState::Error
          ? WINE
          : (data_state == DashboardDataState::Stale
                 ? GOLD
                 : BUTTON_DARK),
      0);
}

void tick_animation(
    FamicomThemeState* state, uint32_t now_ms) {
  if (state->animation_last_frame_ms != 0 &&
      now_ms - state->animation_last_frame_ms <
          ANIMATION_FRAME_MS) {
    return;
  }
  state->animation_last_frame_ms = now_ms;
  if (state->animation_started_ms == 0) {
    state->animation_started_ms = now_ms;
  }
  uint32_t elapsed = now_ms - state->animation_started_ms;
  float phase =
      static_cast<float>(elapsed % POWER_PULSE_MS) /
      static_cast<float>(POWER_PULSE_MS);
  float wave =
      (sinf(FAMICOM_TWO_PI * phase) + 1.0f) * 0.5f;
  lv_opa_t led_opacity =
      static_cast<lv_opa_t>(170.0f + wave * 85.0f);
  lv_obj_set_style_bg_opa(
      state->power_led, led_opacity, 0);
  lv_obj_set_style_shadow_opa(
      state->power_led,
      static_cast<lv_opa_t>(35.0f + wave * 80.0f), 0);

  if (state->running_count <= 0) {
    return;
  }
  float b_wave =
      (sinf(FAMICOM_TWO_PI * phase) + 1.0f) * 0.5f;
  float a_wave =
      (sinf(FAMICOM_TWO_PI * phase + 3.14159265f) + 1.0f) *
      0.5f;
  lv_obj_set_style_bg_opa(
      state->b_button,
      static_cast<lv_opa_t>(225.0f + b_wave * 30.0f), 0);
  lv_obj_set_style_bg_opa(
      state->a_button,
      static_cast<lv_opa_t>(225.0f + a_wave * 30.0f), 0);
}

bool famicom_mount(
    void* raw_state, lv_obj_t* parent,
    const ThemeResources& resources) {
  auto* state = static_cast<FamicomThemeState*>(raw_state);
  *state = FamicomThemeState{};
  state->resources = resources;

#if LV_USE_TINY_TTF && LV_TINY_TTF_FILE_SUPPORT
  state->display_font_80 = lv_tiny_ttf_create_file_ex(
      DISPLAY_FONT_PATH, 80,
      LV_FONT_KERNING_NORMAL, 20);
  state->display_font_52 = lv_tiny_ttf_create_file_ex(
      DISPLAY_FONT_PATH, 52,
      LV_FONT_KERNING_NORMAL, 24);
  state->display_font_36 = lv_tiny_ttf_create_file_ex(
      DISPLAY_FONT_PATH, 36,
      LV_FONT_KERNING_NORMAL, 48);
  if (state->display_font_80) {
    state->display_font_80->fallback = resources.token_font;
  }
  if (state->display_font_52) {
    state->display_font_52->fallback = &lv_font_montserrat_48;
  }
  if (state->display_font_36) {
    state->display_font_36->fallback = &lv_font_montserrat_32;
  }
#endif

  state->root = lv_obj_create(parent);
  strip_obj(state->root);
  lv_obj_set_size(
      state->root, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(state->root, 0, 0);
  lv_obj_set_style_radius(state->root, 28, 0);
  lv_obj_set_style_clip_corner(state->root, true, 0);

  make_shell(state);
  make_header(state);
  make_data_display(state);
  make_quota_card(state);
  make_reset_panel(state);
  make_controller_rail(state);
  return true;
}

void famicom_update(
    void* raw_state, const DashboardViewModel& model) {
  auto* state = static_cast<FamicomThemeState*>(raw_state);
  update_data_state(state, model.data_state);
  update_primary(state, model);
  update_secondary(state, model);
  update_share(state, model);
  update_quota(state, model);
  update_activity(state, model.running_count);
  update_battery(
      state, model.battery_percent, model.charging);
}

void famicom_tick(void* raw_state, uint32_t now_ms) {
  auto* state = static_cast<FamicomThemeState*>(raw_state);
  tick_animation(state, now_ms);
}

void famicom_unmount(void* raw_state) {
  auto* state = static_cast<FamicomThemeState*>(raw_state);
  if (state->root) {
    lv_obj_delete(state->root);
  }
  state->root = nullptr;
#if LV_USE_TINY_TTF
  if (state->display_font_80) {
    lv_tiny_ttf_destroy(state->display_font_80);
  }
  if (state->display_font_52) {
    lv_tiny_ttf_destroy(state->display_font_52);
  }
  if (state->display_font_36) {
    lv_tiny_ttf_destroy(state->display_font_36);
  }
#endif
  state->display_font_80 = nullptr;
  state->display_font_52 = nullptr;
  state->display_font_36 = nullptr;
}

const ThemeDashboardOps FAMICOM_DASHBOARD = {
    sizeof(FamicomThemeState),
    famicom_mount,
    famicom_update,
    famicom_tick,
    famicom_unmount,
};

const ThemePack FAMICOM_THEME = {
    "famicom",
    "Famicom",
    1,
    CODEXMETER_BURN_IN_DRIFT_MAX_PX,
    FAMICOM_DASHBOARD,
    nullptr,
    nullptr,
};

static_assert(
    sizeof(FamicomThemeState) <= CODEXMETER_THEME_STATE_BYTES,
    "Famicom theme state exceeds ThemeRuntime storage");

}  // namespace

const ThemePack& famicom_theme_pack() {
  return FAMICOM_THEME;
}
