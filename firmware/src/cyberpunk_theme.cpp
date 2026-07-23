#include "cyberpunk_theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

namespace {

constexpr int ACTIVITY_MAX_DIAMONDS = 6;
constexpr int ACTIVITY_DIAMOND_SIZE = 10;
constexpr int TODAY_SHARE_WIDTH = 224;
constexpr int QUOTA_BAR_X = 212;
constexpr int QUOTA_BAR_WIDTH = 240;
constexpr int BATTERY_FILL_WIDTH = 32;
constexpr char CYBER_DISPLAY_FONT_PATH[] =
    "S:/fonts/BarlowCondensed-Black.ttf";
constexpr uint32_t ACTIVITY_FRAME_MS = 100;
constexpr uint32_t ACTIVITY_PULSE_MS = 3200;
constexpr float CYBER_TWO_PI = 6.28318530718f;

const lv_color_t BG = lv_color_hex(0x090A0D);
const lv_color_t SURFACE = lv_color_hex(0x15171D);
const lv_color_t SURFACE_DARK = lv_color_hex(0x0C0E12);
const lv_color_t SURFACE_LINE = lv_color_hex(0x3C414E);
const lv_color_t TRACK = lv_color_hex(0x282C35);
const lv_color_t TEXT = lv_color_hex(0xF5F6F2);
const lv_color_t DIM = lv_color_hex(0x767B88);
const lv_color_t ACID_YELLOW = lv_color_hex(0xFCDF0A);
const lv_color_t DATA_CYAN = lv_color_hex(0x00F0FF);
const lv_color_t SYSTEM_RED = lv_color_hex(0xFF003C);

struct CyberpunkThemeState {
  lv_obj_t* root = nullptr;
  lv_obj_t* header_status = nullptr;
  lv_obj_t* side_status = nullptr;
  lv_obj_t* live_chip = nullptr;
  lv_obj_t* live_label = nullptr;
  lv_obj_t* today_value = nullptr;
  lv_obj_t* today_unit = nullptr;
  lv_obj_t* today_share_fill = nullptr;
  lv_obj_t* today_share_text = nullptr;
  lv_obj_t* week_value = nullptr;
  lv_obj_t* rolling_status = nullptr;
  lv_obj_t* quota_value = nullptr;
  lv_obj_t* quota_unit = nullptr;
  lv_obj_t* quota_reset = nullptr;
  lv_obj_t* quota_fill = nullptr;
  lv_obj_t* quota_remainder = nullptr;
  lv_obj_t* quota_state = nullptr;
  lv_obj_t* activity_text = nullptr;
  lv_obj_t* activity_diamonds[ACTIVITY_MAX_DIAMONDS]{};
  lv_obj_t* battery_fill = nullptr;
  lv_obj_t* battery_value = nullptr;
  lv_font_t* display_font_80 = nullptr;
  lv_font_t* display_font_52 = nullptr;
  lv_font_t* display_font_36 = nullptr;
  ThemeResources resources{};
  int activity_visible_count = 0;
  uint32_t activity_started_ms = 0;
  uint32_t activity_last_frame_ms = 0;
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

lv_obj_t* make_panel(
    lv_obj_t* parent, int x, int y, int width, int height,
    lv_color_t color, lv_color_t border_color, int border_width = 1) {
  lv_obj_t* panel = make_rect(parent, x, y, width, height, color);
  lv_obj_set_style_border_width(panel, border_width, 0);
  lv_obj_set_style_border_color(panel, border_color, 0);
  lv_obj_set_style_border_opa(panel, LV_OPA_COVER, 0);
  return panel;
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

void make_display_text(
    lv_obj_t* label, lv_color_t color,
    int outline_width, int scale_x = 256, int scale_y = 256) {
  lv_obj_set_style_text_outline_stroke_color(label, color, 0);
  lv_obj_set_style_text_outline_stroke_width(label, outline_width, 0);
  lv_obj_set_style_text_outline_stroke_opa(label, LV_OPA_COVER, 0);
  lv_obj_set_style_transform_pivot_x(label, 0, 0);
  lv_obj_set_style_transform_pivot_y(label, 0, 0);
  lv_obj_set_style_transform_scale_x(label, scale_x, 0);
  lv_obj_set_style_transform_scale_y(label, scale_y, 0);
}

void make_glow(
    lv_obj_t* obj, lv_color_t color,
    int width, int spread, lv_opa_t opacity) {
  lv_obj_set_style_shadow_color(obj, color, 0);
  lv_obj_set_style_shadow_width(obj, width, 0);
  lv_obj_set_style_shadow_spread(obj, spread, 0);
  lv_obj_set_style_shadow_opa(obj, opacity, 0);
}

const lv_font_t* font_or(
    const lv_font_t* preferred, const lv_font_t* fallback) {
  return preferred ? preferred : fallback;
}

void make_cutout(
    lv_obj_t* parent, int x, int y, int size, lv_color_t color) {
  lv_obj_t* cutout = make_rect(parent, x, y, size, size, color);
  lv_obj_set_style_transform_pivot_x(cutout, size / 2, 0);
  lv_obj_set_style_transform_pivot_y(cutout, size / 2, 0);
  lv_obj_set_style_transform_rotation(cutout, 450, 0);
}

void make_header(CyberpunkThemeState* state) {
  make_rect(state->root, 0, 0, 480, 71, ACID_YELLOW);

  lv_obj_t* logo = make_label(
      state->root, "CODEX//METER",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      BG, 22, 11);
  lv_obj_set_style_text_letter_space(logo, 0, 0);
  make_display_text(
      logo, BG, state->display_font_36 ? 0 : 3,
      state->display_font_36 ? 195 : 230,
      state->display_font_36 ? 220 : 276);
  lv_obj_t* subtitle = make_label(
      state->root, "LOCAL SUBSCRIPTION MONITOR",
      &lv_font_montserrat_14, BG, 24, 45);
  lv_obj_set_style_text_letter_space(subtitle, 2, 0);
  make_display_text(subtitle, BG, 0, 175, 270);

  lv_obj_t* flow = make_label(
      state->root, "TOKEN FLOW",
      font_or(state->display_font_36, &lv_font_montserrat_24),
      BG, 224, 11);
  lv_obj_set_style_text_letter_space(flow, 0, 0);
  make_display_text(
      flow, BG, state->display_font_36 ? 0 : 3,
      state->display_font_36 ? 165 : 195,
      state->display_font_36 ? 220 : 276);
  state->header_status = make_label(
      state->root, "CYBERPUNK 2077", &lv_font_montserrat_14,
      BG, 224, 45);
  lv_obj_set_style_text_letter_space(state->header_status, 1, 0);
  make_display_text(state->header_status, BG, 0, 215, 270);

  make_rect(state->root, 408, 0, 72, 31, BG);
  for (int i = 0; i < 4; ++i) {
    lv_obj_t* stripe = make_rect(
        state->root, 420 + i * 16, -9, 4, 50, ACID_YELLOW);
    lv_obj_set_style_transform_pivot_x(stripe, 2, 0);
    lv_obj_set_style_transform_pivot_y(stripe, 25, 0);
    lv_obj_set_style_transform_rotation(stripe, -450, 0);
  }
  for (int row = 0; row < 19; ++row) {
    int left = 423 + row;
    make_rect(
        state->root, left, 52 + row, 480 - left, 1, BG);
  }
}

void make_side_rail(CyberpunkThemeState* state) {
  lv_obj_t* rail =
      make_rect(state->root, 0, 71, 17, 356, ACID_YELLOW);
  state->side_status = make_label(
      rail, "CODEXMETER // LOCAL DATA // WAITING",
      &lv_font_montserrat_14, BG, 4, 345);
  lv_obj_set_style_text_letter_space(state->side_status, 2, 0);
  lv_obj_set_style_transform_pivot_x(state->side_status, 0, 0);
  lv_obj_set_style_transform_pivot_y(state->side_status, 0, 0);
  lv_obj_set_style_transform_rotation(state->side_status, -900, 0);
  lv_obj_set_style_transform_scale_x(state->side_status, 185, 0);
  lv_obj_set_style_transform_scale_y(state->side_status, 270, 0);
}

void make_today_panel(CyberpunkThemeState* state) {
  lv_obj_t* panel = make_panel(
      state->root, 29, 89, 281, 143,
      SURFACE, ACID_YELLOW);
  make_cutout(panel, 268, -13, 26, BG);
  lv_obj_t* tab =
      make_rect(state->root, 29, 89, 149, 12, ACID_YELLOW);
  make_cutout(tab, 140, -5, 16, SURFACE);

  lv_obj_t* heading = make_label(
      state->root, "TODAY / TOKEN BURN",
      &lv_font_montserrat_14, ACID_YELLOW, 41, 103);
  lv_obj_set_style_text_letter_space(heading, 2, 0);
  make_display_text(heading, ACID_YELLOW, 0, 205, 295);

  state->live_chip = make_rect(
      state->root, 277, 99, 38, 12, SYSTEM_RED);
  state->live_label = make_label(
      state->root, "WAIT", &lv_font_montserrat_14,
      TEXT, 278, 96);
  make_display_text(state->live_label, TEXT, 0, 205, 250);

  state->today_value = make_label(
      state->root, "--",
      font_or(state->display_font_80, state->resources.token_font),
      TEXT, 40, 114);
  lv_obj_set_width(state->today_value, 232);
  lv_obj_set_style_text_letter_space(
      state->today_value, state->display_font_80 ? -1 : -3, 0);
  make_display_text(
      state->today_value, TEXT,
      state->display_font_80 ? 0 : 5,
      state->display_font_80 ? 320 : 400,
      state->display_font_80 ? 256 : 390);
  state->today_unit = make_label(
      state->root, "",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      DATA_CYAN, 270, 147);
  make_display_text(
      state->today_unit, DATA_CYAN,
      state->display_font_36 ? 0 : 1,
      state->display_font_36 ? 335 : 280,
      state->display_font_36 ? 360 : 380);

  make_rect(
      state->root, 41, 209, TODAY_SHARE_WIDTH, 4, TRACK);
  state->today_share_fill = make_rect(
      state->root, 41, 209, 1, 4, DATA_CYAN);
  make_glow(state->today_share_fill, DATA_CYAN, 9, 1, LV_OPA_50);
  state->today_share_text = make_label(
      state->root, "TODAY / 7D SHARE --",
      &lv_font_montserrat_14, DIM, 135, 203);
  lv_obj_set_width(state->today_share_text, 258);
  lv_obj_set_style_text_letter_space(state->today_share_text, 1, 0);
  lv_obj_set_style_transform_pivot_x(state->today_share_text, 0, 0);
  lv_obj_set_style_transform_pivot_y(state->today_share_text, 0, 0);
  lv_obj_set_style_transform_scale_x(state->today_share_text, 165, 0);
  lv_obj_set_style_transform_scale_y(state->today_share_text, 270, 0);

  make_rect(state->root, 34, 156, 51, 4, SYSTEM_RED);
  lv_obj_t* cyan_glitch =
      make_rect(state->root, 81, 161, 37, 2, DATA_CYAN);
  make_glow(cyan_glitch, DATA_CYAN, 7, 1, LV_OPA_40);
}

void make_week_panel(CyberpunkThemeState* state) {
  lv_obj_t* panel =
      make_rect(state->root, 324, 89, 141, 119, DATA_CYAN);
  make_cutout(panel, 129, 107, 24, BG);
  lv_obj_t* heading = make_label(
      state->root, "7D TOTAL", &lv_font_montserrat_14,
      BG, 338, 99);
  lv_obj_set_style_text_letter_space(heading, 2, 0);
  make_display_text(heading, BG, 0, 220, 295);
  state->week_value = make_label(
      state->root, "--",
      font_or(state->display_font_52, &lv_font_montserrat_48),
      BG, 338, 106);
  lv_obj_set_width(state->week_value, 128);
  lv_obj_set_style_text_letter_space(state->week_value, -1, 0);
  make_display_text(
      state->week_value, BG,
      state->display_font_52 ? 0 : 3,
      state->display_font_52 ? 285 : 245,
      state->display_font_52 ? 315 : 315);

  make_rect(state->root, 338, 175, 114, 2, BG);
  make_rect(state->root, 338, 181, 79, 2, BG);
  make_rect(state->root, 338, 187, 98, 2, BG);

  lv_obj_t* rolling_strip =
      make_rect(state->root, 324, 219, 141, 13, SYSTEM_RED);
  for (int row = 0; row < 13; ++row) {
    int inset = (20 * row) / 12;
    if (inset > 0) {
      make_rect(
          rolling_strip, 0, row, inset, 1, BG);
    }
  }
  state->rolling_status = make_label(
      state->root, "ROLLING WINDOW", &lv_font_montserrat_14,
      TEXT, 338, 216);
  make_display_text(state->rolling_status, TEXT, 0, 185, 270);
}

void make_quota_panel(CyberpunkThemeState* state) {
  lv_obj_t* panel = make_panel(
      state->root, 29, 251, 436, 155,
      lv_color_hex(0x111318), SURFACE_LINE);
  make_cutout(panel, 416, 135, 40, BG);
  lv_obj_t* tab =
      make_rect(state->root, 29, 251, 129, 12, ACID_YELLOW);
  make_cutout(tab, 120, -5, 16, lv_color_hex(0x111318));

  lv_obj_t* heading = make_label(
      state->root, "7D QUOTA REMAINING",
      &lv_font_montserrat_14, lv_color_hex(0xA7ABB5),
      38, 273);
  lv_obj_set_style_text_letter_space(heading, 2, 0);
  make_display_text(
      heading, lv_color_hex(0xA7ABB5), 0, 195, 295);

  state->quota_value = make_label(
      state->root, "--",
      font_or(state->display_font_80, state->resources.percent_font),
      TEXT, 36, 272);
  lv_obj_set_style_text_letter_space(
      state->quota_value, state->display_font_80 ? -1 : -4, 0);
  make_display_text(
      state->quota_value, TEXT,
      state->display_font_80 ? 0 : 5,
      state->display_font_80 ? 390 : 440,
      state->display_font_80 ? 340 : 430);
  state->quota_unit = make_label(
      state->root, "%",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      ACID_YELLOW, 146, 338);
  make_display_text(
      state->quota_unit, ACID_YELLOW,
      state->display_font_36 ? 0 : 1,
      state->display_font_36 ? 290 : 220,
      state->display_font_36 ? 290 : 290);

  make_rect(state->root, QUOTA_BAR_X, 288, QUOTA_BAR_WIDTH, 1, TRACK);
  lv_obj_t* reset_heading = make_label(
      state->root, "RESET WINDOW", &lv_font_montserrat_14,
      DIM, QUOTA_BAR_X, 319);
  make_display_text(reset_heading, DIM, 0, 220, 295);
  state->quota_reset = make_label(
      state->root, "--D : --H",
      font_or(state->display_font_36, &lv_font_montserrat_32),
      TEXT, QUOTA_BAR_X, 338);
  lv_obj_set_width(state->quota_reset, QUOTA_BAR_WIDTH);
  lv_obj_set_style_text_letter_space(state->quota_reset, -1, 0);
  make_display_text(
      state->quota_reset, TEXT,
      state->display_font_36 ? 0 : 3,
      state->display_font_36 ? 310 : 286,
      state->display_font_36 ? 280 : 340);

  make_rect(
      state->root, QUOTA_BAR_X, 309, QUOTA_BAR_WIDTH, 5, TRACK);
  state->quota_fill = make_rect(
      state->root, QUOTA_BAR_X, 309, 1, 5, DATA_CYAN);
  make_glow(state->quota_fill, DATA_CYAN, 7, 1, LV_OPA_40);
  state->quota_remainder = make_rect(
      state->root, QUOTA_BAR_X + 1, 309,
      QUOTA_BAR_WIDTH - 1, 5, SYSTEM_RED);

  lv_obj_t* quota_state_panel =
      make_rect(state->root, 407, 369, 58, 37, SYSTEM_RED);
  for (int row = 0; row < 20; ++row) {
    if (row > 0) {
      make_rect(
          quota_state_panel, 0, 17 + row, row, 1,
          lv_color_hex(0x111318));
    }
  }
  state->quota_state = make_label(
      state->root, "WAIT", &lv_font_montserrat_14,
      TEXT, 422, 379);
  make_display_text(state->quota_state, TEXT, 0, 195, 270);
}

void make_activity_rail(CyberpunkThemeState* state) {
  make_rect(state->root, 17, 427, 448, 37, SURFACE_DARK);
  lv_obj_t* heading = make_label(
      state->root, "ACTIVE THREADS", &lv_font_montserrat_14,
      DIM, 33, 437);
  lv_obj_set_style_text_letter_space(heading, 1, 0);
  lv_obj_set_style_transform_pivot_x(heading, 0, 0);
  lv_obj_set_style_transform_pivot_y(heading, 0, 0);
  lv_obj_set_style_transform_scale_x(heading, 185, 0);
  lv_obj_set_style_transform_scale_y(heading, 270, 0);

  for (int i = 0; i < ACTIVITY_MAX_DIAMONDS; ++i) {
    state->activity_diamonds[i] = make_rect(
      state->root,
        144 + i * 16, 440,
        ACTIVITY_DIAMOND_SIZE, ACTIVITY_DIAMOND_SIZE,
        i % 2 == 0 ? DATA_CYAN : ACID_YELLOW);
    lv_obj_set_style_transform_pivot_x(
        state->activity_diamonds[i], ACTIVITY_DIAMOND_SIZE / 2, 0);
    lv_obj_set_style_transform_pivot_y(
        state->activity_diamonds[i], ACTIVITY_DIAMOND_SIZE / 2, 0);
    lv_obj_set_style_transform_rotation(
        state->activity_diamonds[i], 450, 0);
    make_glow(
        state->activity_diamonds[i],
        i % 2 == 0 ? DATA_CYAN : ACID_YELLOW,
        10, 1, LV_OPA_50);
    lv_obj_add_flag(
        state->activity_diamonds[i], LV_OBJ_FLAG_HIDDEN);
  }

  state->activity_text = make_label(
      state->root, "00 / IDLE", &lv_font_montserrat_14,
      SYSTEM_RED, 246, 437);
  lv_obj_set_style_text_letter_space(state->activity_text, 1, 0);
  make_display_text(state->activity_text, SYSTEM_RED, 0, 205, 270);

  lv_obj_t* battery_bay = make_panel(
      state->root, 360, 424, 105, 40,
      SURFACE_DARK, SURFACE_LINE);
  make_cutout(battery_bay, 92, -8, 20, BG);
  lv_obj_t* battery_label = make_label(
      state->root, "BATTERY", &lv_font_montserrat_14,
      DIM, 369, 422);
  lv_obj_set_style_text_letter_space(battery_label, 1, 0);
  make_display_text(battery_label, DIM, 0, 240, 305);
  lv_obj_t* battery_body = make_panel(
      state->root, 369, 445, 36, 15,
      BG, ACID_YELLOW);
  state->battery_fill = make_rect(
      battery_body, 2, 2, 1, 11, ACID_YELLOW);
  make_rect(state->root, 406, 450, 4, 6, ACID_YELLOW);
  state->battery_value = make_label(
      state->root, "--%", &lv_font_montserrat_14,
      TEXT, 416, 442);
  make_display_text(state->battery_value, TEXT, 0, 235, 320);
}

void make_scanlines(CyberpunkThemeState* state) {
  for (int y = 76; y < 480; y += 8) {
    make_rect(
        state->root, 17, y, 463, 1,
        SURFACE_LINE, LV_OPA_20);
  }
  make_rect(state->root, 17, 239, 448, 2, TRACK, LV_OPA_70);
  make_rect(state->root, 17, 416, 448, 2, TRACK, LV_OPA_70);
}

const char* status_header(DashboardDataState) {
  return "CYBERPUNK 2077";
}

const char* status_side(DashboardDataState status) {
  switch (status) {
    case DashboardDataState::Ready:
      return "CODEXMETER // LOCAL DATA // SYNC OK";
    case DashboardDataState::Stale:
      return "CODEXMETER // LOCAL DATA // STALE";
    case DashboardDataState::Error:
      return "CODEXMETER // LOCAL DATA // ERROR";
    case DashboardDataState::Waiting:
    default:
      return "CODEXMETER // LOCAL DATA // WAITING";
  }
}

const char* status_short(DashboardDataState status) {
  switch (status) {
    case DashboardDataState::Ready:
      return "LIVE";
    case DashboardDataState::Stale:
      return "STAL";
    case DashboardDataState::Error:
      return "ERR";
    case DashboardDataState::Waiting:
    default:
      return "WAIT";
  }
}

lv_color_t status_color(DashboardDataState status) {
  switch (status) {
    case DashboardDataState::Ready:
      return SYSTEM_RED;
    case DashboardDataState::Waiting:
      return ACID_YELLOW;
    case DashboardDataState::Stale:
    case DashboardDataState::Error:
    default:
      return SYSTEM_RED;
  }
}

const char* status_result(DashboardDataState status) {
  switch (status) {
    case DashboardDataState::Ready:
      return "WXYZ";
    case DashboardDataState::Stale:
      return "STAL";
    case DashboardDataState::Error:
      return "ERR";
    case DashboardDataState::Waiting:
    default:
      return "WAIT";
  }
}

void format_today_value(
    char* output, size_t size, char* unit, size_t unit_size,
    const DashboardViewModel& model) {
  unit[0] = '\0';
  if (!model.has_today_tokens) {
    strlcpy(output, "--", size);
    return;
  }

  char compact[24];
  strlcpy(compact, model.today_tokens_text, sizeof(compact));
  size_t compact_length = strlen(compact);
  if (compact_length > 0) {
    char last = compact[compact_length - 1];
    if ((last >= 'A' && last <= 'Z') ||
        (last >= 'a' && last <= 'z')) {
      unit[0] = last;
      unit[1] = '\0';
      compact[compact_length - 1] = '\0';
    }
  }

  size_t input_length = strnlen(compact, sizeof(compact));
  const char* decimal = strchr(compact, '.');
  size_t integer_digits =
      decimal ? static_cast<size_t>(decimal - compact) : input_length;
  if (input_length > 0 && integer_digits > 0 &&
      integer_digits < 3 &&
      compact[0] >= '0' && compact[0] <= '9') {
    size_t zero_count = 3 - integer_digits;
    snprintf(
        output, size, zero_count == 2 ? "00%s" : "0%s", compact);
    return;
  }
  strlcpy(output, compact, size);
}

void format_share(
    char* output, size_t size, const DashboardViewModel& model) {
  if (!model.has_today_share) {
    strlcpy(output, "TODAY / 7D SHARE --", size);
    return;
  }
  unsigned int permille = model.today_share_permille;
  snprintf(
      output, size, "TODAY / 7D SHARE %u.%u%%",
      permille / 10U, permille % 10U);
}

void format_reset(
    char* output, size_t size, int32_t seconds) {
  if (seconds < 0) {
    strlcpy(output, "--D : --H", size);
    return;
  }
  long total_hours = (seconds + 3599L) / 3600L;
  snprintf(
      output, size, "%02ldD : %02ldH",
      total_hours / 24L, total_hours % 24L);
}

void update_data_state(
    CyberpunkThemeState* state, DashboardDataState data_state) {
  const char* short_status = status_short(data_state);
  lv_color_t color = status_color(data_state);
  set_label_text_if_changed(
      state->header_status, status_header(data_state));
  set_label_text_if_changed(
      state->side_status, status_side(data_state));
  set_label_text_if_changed(state->live_label, short_status);
  lv_obj_set_style_bg_color(state->live_chip, color, 0);
  set_label_text_if_changed(
      state->quota_state, status_result(data_state));
}

void update_today(
    CyberpunkThemeState* state, const DashboardViewModel& model) {
  char value[24];
  char unit[4];
  format_today_value(
      value, sizeof(value), unit, sizeof(unit), model);
  bool value_changed =
      set_label_text_if_changed(state->today_value, value);
  bool unit_changed =
      set_label_text_if_changed(state->today_unit, unit);
  size_t value_length = strlen(value);
  if (value_changed) {
    lv_obj_set_style_text_font(
        state->today_value,
        state->display_font_80
            ? state->display_font_80
            : (value_length <= 6 ? state->resources.token_font
                                 : &lv_font_montserrat_48),
        0);
  int value_scale_x =
      state->display_font_80
            ? (value_length <= 3
                   ? 360
                   : (value_length == 4
                          ? 330
                          : (value_length == 5
                                 ? 300
                                 : (value_length == 6 ? 280 : 220))))
            : (value_length <= 5 ? 400
                                 : (value_length == 6 ? 330 : 240));
    lv_obj_set_style_transform_scale_x(
        state->today_value, value_scale_x, 0);
  }
  if (value_changed || unit_changed) {
    int unit_x =
        value_length >= 6
            ? 270
            : (value_length == 5
                   ? 270
                   : (value_length == 4
                          ? (state->display_font_80 ? 250 : 260)
                          : (state->display_font_80 ? 200 : 220)));
    lv_obj_set_x(state->today_unit, unit_x);
  }

  char share[40];
  format_share(share, sizeof(share), model);
  set_label_text_if_changed(state->today_share_text, share);

  int share_width = 1;
  if (model.has_today_share) {
    share_width =
        (TODAY_SHARE_WIDTH * model.today_share_permille + 500) / 1000;
    if (share_width < 1) share_width = 1;
    if (share_width > TODAY_SHARE_WIDTH) {
      share_width = TODAY_SHARE_WIDTH;
    }
  }
  lv_obj_set_width(state->today_share_fill, share_width);
  lv_obj_set_style_bg_color(
      state->today_share_fill,
      model.has_today_share ? DATA_CYAN : DIM, 0);
}

void update_week(
    CyberpunkThemeState* state, const DashboardViewModel& model) {
  if (set_label_text_if_changed(
          state->week_value, model.last_7d_tokens_text)) {
    lv_obj_set_style_text_font(
        state->week_value,
        state->display_font_52
            ? (strlen(model.last_7d_tokens_text) <= 5
                   ? state->display_font_52
                   : state->display_font_36)
            : (strlen(model.last_7d_tokens_text) <= 5
                   ? &lv_font_montserrat_48
                   : &lv_font_montserrat_24),
        0);
  }

  char rolling[32];
  if (model.h5_remaining >= 0) {
    snprintf(rolling, sizeof(rolling), "5H WINDOW / %d%%", model.h5_remaining);
  } else {
    strlcpy(rolling, "ROLLING WINDOW", sizeof(rolling));
  }
  set_label_text_if_changed(state->rolling_status, rolling);
}

void update_quota(
    CyberpunkThemeState* state, const DashboardViewModel& model) {
  char quota[8];
  if (model.d7_remaining < 0) {
    strlcpy(quota, "--", sizeof(quota));
  } else {
    snprintf(quota, sizeof(quota), "%d", model.d7_remaining);
  }
  bool value_changed =
      set_label_text_if_changed(state->quota_value, quota);
  if (value_changed) {
    lv_obj_set_style_transform_scale_x(
        state->quota_value,
        state->display_font_80
            ? (model.d7_remaining >= 100 ? 310 : 390)
            : (model.d7_remaining >= 100 ? 350 : 440),
        0);
  }
  int unit_x =
      model.d7_remaining >= 100
          ? (state->display_font_80 ? 166 : 202)
          : (model.d7_remaining >= 10
                 ? (state->display_font_80 ? 154 : 174)
                 : (state->display_font_80 ? 113 : 112));
  if (value_changed) lv_obj_set_x(state->quota_unit, unit_x);
  char reset[24];
  format_reset(reset, sizeof(reset), model.d7_reset_seconds);
  set_label_text_if_changed(state->quota_reset, reset);

  if (model.d7_remaining < 0) {
    lv_obj_set_width(state->quota_fill, 1);
    lv_obj_set_x(state->quota_remainder, QUOTA_BAR_X + 1);
    lv_obj_set_width(state->quota_remainder, QUOTA_BAR_WIDTH - 1);
    lv_obj_set_style_bg_color(state->quota_fill, DIM, 0);
    lv_obj_set_style_bg_color(state->quota_remainder, TRACK, 0);
    return;
  }

  int fill =
      (QUOTA_BAR_WIDTH * model.d7_remaining + 50) / 100;
  if (fill < 0) fill = 0;
  if (fill > QUOTA_BAR_WIDTH) fill = QUOTA_BAR_WIDTH;

  if (fill == 0) {
    lv_obj_add_flag(state->quota_fill, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(state->quota_fill, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(state->quota_fill, fill);
    lv_obj_set_style_bg_color(state->quota_fill, DATA_CYAN, 0);
  }

  if (fill >= QUOTA_BAR_WIDTH) {
    lv_obj_add_flag(state->quota_remainder, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(state->quota_remainder, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(state->quota_remainder, QUOTA_BAR_X + fill);
    lv_obj_set_width(
        state->quota_remainder, QUOTA_BAR_WIDTH - fill);
    lv_obj_set_style_bg_color(
        state->quota_remainder, SYSTEM_RED, 0);
  }
}

void update_activity(
    CyberpunkThemeState* state, int running_count) {
  if (running_count < 0) running_count = 0;
  int visible = running_count;
  if (visible > ACTIVITY_MAX_DIAMONDS) {
    visible = ACTIVITY_MAX_DIAMONDS;
  }

  bool start_animation =
      state->activity_visible_count == 0 && visible > 0;
  for (int i = 0; i < ACTIVITY_MAX_DIAMONDS; ++i) {
    if (i < visible) {
      lv_obj_clear_flag(
          state->activity_diamonds[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(
          state->activity_diamonds[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  char text[24];
  if (running_count <= 0) {
    strlcpy(text, "00 / IDLE", sizeof(text));
  } else if (running_count > 99) {
    strlcpy(text, "99+ / RUNNING", sizeof(text));
  } else {
    snprintf(text, sizeof(text), "%02d / RUNNING", running_count);
  }
  set_label_text_if_changed(state->activity_text, text);
  lv_obj_set_style_text_color(
      state->activity_text,
      running_count > 0 ? SYSTEM_RED : DIM, 0);

  state->activity_visible_count = visible;
  if (start_animation) {
    state->activity_started_ms = millis();
    state->activity_last_frame_ms = 0;
  } else if (visible == 0) {
    state->activity_started_ms = 0;
    state->activity_last_frame_ms = 0;
  }
}

void update_battery(
    CyberpunkThemeState* state, int percent, bool charging) {
  char text[16];
  if (percent < 0) {
    strlcpy(text, charging ? "USB" : "--%", sizeof(text));
  } else {
    snprintf(text, sizeof(text), "%d%%%s", percent, charging ? "+" : "");
  }
  set_label_text_if_changed(state->battery_value, text);

  int width = percent < 0
                  ? (charging ? BATTERY_FILL_WIDTH : 1)
                  : (BATTERY_FILL_WIDTH * percent + 50) / 100;
  if (width < 1) width = 1;
  if (width > BATTERY_FILL_WIDTH) width = BATTERY_FILL_WIDTH;
  lv_obj_set_width(state->battery_fill, width);
  lv_obj_set_style_bg_color(
      state->battery_fill,
      percent >= 0 && percent <= 20 ? SYSTEM_RED
                                    : ACID_YELLOW,
      0);
}

void tick_activity(
    CyberpunkThemeState* state, uint32_t now_ms) {
  if (state->activity_visible_count <= 0) return;
  if (state->activity_last_frame_ms != 0 &&
      now_ms - state->activity_last_frame_ms < ACTIVITY_FRAME_MS) {
    return;
  }
  state->activity_last_frame_ms = now_ms;
  uint32_t elapsed = now_ms - state->activity_started_ms;

  for (int i = 0; i < state->activity_visible_count; ++i) {
    float phase =
        static_cast<float>(
            (elapsed + static_cast<uint32_t>(i * 180)) %
            ACTIVITY_PULSE_MS) /
        static_cast<float>(ACTIVITY_PULSE_MS);
    float wave = (sinf(CYBER_TWO_PI * phase) + 1.0f) * 0.5f;
    lv_opa_t opacity =
        static_cast<lv_opa_t>(140.0f + wave * 115.0f);
    lv_obj_set_style_bg_opa(
        state->activity_diamonds[i], opacity, 0);
    lv_obj_set_style_bg_color(
        state->activity_diamonds[i],
        i % 2 == 0 ? DATA_CYAN : ACID_YELLOW, 0);
  }
}

bool cyberpunk_mount(
    void* raw_state, lv_obj_t* parent,
    const ThemeResources& resources) {
  auto* state = static_cast<CyberpunkThemeState*>(raw_state);
  *state = CyberpunkThemeState{};
  state->resources = resources;

#if LV_USE_TINY_TTF && LV_TINY_TTF_FILE_SUPPORT
  state->display_font_80 = lv_tiny_ttf_create_file_ex(
      CYBER_DISPLAY_FONT_PATH, 80,
      LV_FONT_KERNING_NORMAL, 16);
  state->display_font_52 = lv_tiny_ttf_create_file_ex(
      CYBER_DISPLAY_FONT_PATH, 52,
      LV_FONT_KERNING_NORMAL, 20);
  state->display_font_36 = lv_tiny_ttf_create_file_ex(
      CYBER_DISPLAY_FONT_PATH, 36,
      LV_FONT_KERNING_NORMAL, 64);
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
  lv_obj_set_style_bg_color(state->root, BG, 0);
  lv_obj_set_style_bg_opa(state->root, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(state->root, 28, 0);
  lv_obj_set_style_clip_corner(state->root, true, 0);

  make_scanlines(state);
  make_header(state);
  make_side_rail(state);
  make_today_panel(state);
  make_week_panel(state);
  make_quota_panel(state);
  make_activity_rail(state);
  return true;
}

void cyberpunk_update(
    void* raw_state, const DashboardViewModel& model) {
  auto* state = static_cast<CyberpunkThemeState*>(raw_state);
  update_data_state(state, model.data_state);
  update_today(state, model);
  update_week(state, model);
  update_quota(state, model);
  update_activity(state, model.running_count);
  update_battery(
      state, model.battery_percent, model.charging);
}

void cyberpunk_tick(void* raw_state, uint32_t now_ms) {
  auto* state = static_cast<CyberpunkThemeState*>(raw_state);
  tick_activity(state, now_ms);
}

void cyberpunk_unmount(void* raw_state) {
  auto* state = static_cast<CyberpunkThemeState*>(raw_state);
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

const ThemeDashboardOps CYBERPUNK_DASHBOARD = {
    sizeof(CyberpunkThemeState),
    cyberpunk_mount,
    cyberpunk_update,
    cyberpunk_tick,
    cyberpunk_unmount,
};

const ThemePack CYBERPUNK_THEME = {
    "cyberpunk",
    "Cyberpunk",
    1,
    CODEXMETER_BURN_IN_DRIFT_MAX_PX,
    CYBERPUNK_DASHBOARD,
    nullptr,
    nullptr,
};

static_assert(
    sizeof(CyberpunkThemeState) <= CODEXMETER_THEME_STATE_BYTES,
    "Cyberpunk theme state exceeds ThemeRuntime storage");

}  // namespace

const ThemePack& cyberpunk_theme_pack() {
  return CYBERPUNK_THEME;
}
