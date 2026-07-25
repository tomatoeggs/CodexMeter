#include "walle_theme.h"

#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "device_log.h"

namespace {

constexpr char VALUE_FONT_PATH[] =
    "S:/fonts/Teko-SemiBold.ttf";
constexpr char AUX_FONT_PATH[] =
    "S:/fonts/Teko-SemiBold.ttf";
constexpr char LABEL_FONT_PATH[] =
    "S:/fonts/D-DINCondensed-Bold.ttf";
constexpr char BACKGROUND_PATH[] =
    "S:/themes/walle_bg.rgb565";
constexpr char LEAF_ACTIVE_PATH[] =
    "S:/themes/walle_leaves_active.rgb565";
constexpr char LEAF_MASK_PATH[] =
    "S:/themes/walle_leaves_mask.bin";
constexpr size_t BACKGROUND_BYTES =
    CODEXMETER_SCREEN_W * CODEXMETER_SCREEN_H * sizeof(uint16_t);
constexpr int QUOTA_LEAF_COUNT = 10;
constexpr int RUNTIME_LEAF_COUNT = 9;
constexpr int LEAF_CROP_X = 23;
constexpr int LEAF_CROP_Y = 259;
constexpr int LEAF_CROP_W = 219;
constexpr int LEAF_CROP_H = 93;
constexpr size_t LEAF_CROP_PIXELS =
    LEAF_CROP_W * LEAF_CROP_H;
constexpr size_t LEAF_CROP_BYTES =
    LEAF_CROP_PIXELS * sizeof(uint16_t);

const lv_color_t YELLOW = lv_color_hex(0xF6B91E);
const lv_color_t YELLOW_LIGHT = lv_color_hex(0xFFC72D);
const lv_color_t DARK = lv_color_hex(0x1B1C1E);
const lv_color_t BLOCK_DARK = lv_color_hex(0x101113);
const lv_color_t TREAD_DARK = lv_color_hex(0x292A2D);
const lv_color_t TREAD_LINE = lv_color_hex(0x494A4D);
const lv_color_t CREAM = lv_color_hex(0xF1E9D8);
const lv_color_t RED = lv_color_hex(0xC43B2F);
const lv_color_t RED_LOW = lv_color_hex(0xA92325);
const lv_color_t GREEN = lv_color_hex(0x62B33F);
const lv_color_t GREEN_BRIGHT = lv_color_hex(0x79E66D);
const lv_color_t LEAF_EMPTY = lv_color_hex(0x9B9B96);
const lv_color_t BLUE = lv_color_hex(0x4778BC);
const lv_color_t MUTED = lv_color_hex(0x74777B);

struct WalleThemeState {
  lv_obj_t* root = nullptr;
  lv_obj_t* background = nullptr;
  lv_obj_t* battery_shell = nullptr;
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
  lv_obj_t* quota_leaves[QUOTA_LEAF_COUNT]{};
  lv_obj_t* reset_heading = nullptr;
  lv_obj_t* reset_day_value = nullptr;
  lv_obj_t* reset_day_unit = nullptr;
  lv_obj_t* reset_hour_value = nullptr;
  lv_obj_t* reset_hour_unit = nullptr;

  lv_obj_t* task_count = nullptr;
  lv_obj_t* task_heading = nullptr;
  lv_obj_t* ble_value = nullptr;
  lv_obj_t* ble_dot = nullptr;
  lv_obj_t* sync_value = nullptr;
  lv_obj_t* sync_dot = nullptr;

  lv_font_t* font_120 = nullptr;
  lv_font_t* font_82 = nullptr;
  lv_font_t* font_60 = nullptr;
  lv_font_t* font_38 = nullptr;
  lv_font_t* font_30 = nullptr;
  lv_font_t* font_24 = nullptr;
  lv_font_t* font_20 = nullptr;

  uint8_t* background_pixels = nullptr;
  uint16_t* leaf_base_pixels = nullptr;
  uint16_t* leaf_active_pixels = nullptr;
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
  lv_opa_t sync_dot_opacity = LV_OPA_COVER;
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
  lv_obj_t* label = make_label(
      parent, text, font, color, x, y,
      LV_SIZE_CONTENT, LV_TEXT_ALIGN_LEFT);
  lv_obj_set_style_transform_pivot_x(label, 0, 0);
  return label;
}

const lv_font_t* font_or(
    const lv_font_t* preferred, const lv_font_t* fallback) {
  return preferred ? preferred : fallback;
}

bool set_label_text_if_changed(
    lv_obj_t* label, const char* text) {
  const char* current = lv_label_get_text(label);
  if (current && strcmp(current, text) == 0) return false;
  lv_label_set_text(label, text);
  return true;
}

bool read_file_exact(
    const char* path, void* output, size_t expected,
    const char* label) {
  lv_fs_file_t file;
  lv_fs_res_t result =
      lv_fs_open(&file, path, LV_FS_MODE_RD);
  if (result != LV_FS_RES_OK) {
    device_logf(
        "ERROR", "walle %s open failed path=%s res=%d",
        label, path, static_cast<int>(result));
    return false;
  }

  auto* bytes = static_cast<uint8_t*>(output);
  size_t total = 0;
  while (total < expected) {
    uint32_t bytes_read = 0;
    uint32_t request =
        static_cast<uint32_t>(
            expected - total > 16384
                ? 16384
                : expected - total);
    result =
        lv_fs_read(
            &file, bytes + total, request, &bytes_read);
    if (result != LV_FS_RES_OK || bytes_read == 0) break;
    total += bytes_read;
  }
  lv_fs_close(&file);

  if (result != LV_FS_RES_OK || total != expected) {
    device_logf(
        "ERROR",
        "walle %s read failed bytes=%lu expected=%lu res=%d",
        label, static_cast<unsigned long>(total),
        static_cast<unsigned long>(expected),
        static_cast<int>(result));
    return false;
  }
  return true;
}

void release_asset_buffers(WalleThemeState* state) {
  if (state->background_pixels) {
    heap_caps_free(state->background_pixels);
  }
  if (state->leaf_base_pixels) {
    heap_caps_free(state->leaf_base_pixels);
  }
  if (state->leaf_active_pixels) {
    heap_caps_free(state->leaf_active_pixels);
  }
  if (state->leaf_mask) {
    heap_caps_free(state->leaf_mask);
  }
  state->background_pixels = nullptr;
  state->leaf_base_pixels = nullptr;
  state->leaf_active_pixels = nullptr;
  state->leaf_mask = nullptr;
  state->background_dsc = lv_image_dsc_t{};
}

bool load_assets(WalleThemeState* state) {
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
  state->leaf_mask = static_cast<uint8_t*>(
      heap_caps_malloc(
          LEAF_CROP_PIXELS,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!state->background_pixels ||
      !state->leaf_base_pixels ||
      !state->leaf_active_pixels ||
      !state->leaf_mask) {
    device_logf(
        "ERROR", "walle asset allocation failed");
    release_asset_buffers(state);
    return false;
  }

  if (!read_file_exact(
          BACKGROUND_PATH, state->background_pixels,
          BACKGROUND_BYTES, "background") ||
      !read_file_exact(
          LEAF_ACTIVE_PATH, state->leaf_active_pixels,
          LEAF_CROP_BYTES, "leaf active") ||
      !read_file_exact(
          LEAF_MASK_PATH, state->leaf_mask,
          LEAF_CROP_PIXELS, "leaf mask")) {
    release_asset_buffers(state);
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

  state->background_dsc.header.magic =
      LV_IMAGE_HEADER_MAGIC;
  state->background_dsc.header.cf =
      LV_COLOR_FORMAT_RGB565;
  state->background_dsc.header.flags = 0;
  state->background_dsc.header.w =
      CODEXMETER_SCREEN_W;
  state->background_dsc.header.h =
      CODEXMETER_SCREEN_H;
  state->background_dsc.header.stride =
      CODEXMETER_SCREEN_W * sizeof(uint16_t);
  state->background_dsc.data_size = BACKGROUND_BYTES;
  state->background_dsc.data =
      state->background_pixels;
  state->background_dsc.reserved = nullptr;
  state->background_dsc.reserved_2 = nullptr;
  return true;
}

void make_background(WalleThemeState* state) {
  state->background = lv_image_create(state->root);
  lv_image_set_src(
      state->background, &state->background_dsc);
  lv_obj_set_pos(state->background, 0, 0);
}

void set_center_scale(lv_obj_t* obj, int scale_x, int scale_y = 256) {
  lv_obj_set_style_transform_pivot_x(
      obj, lv_obj_get_width(obj) / 2, 0);
  lv_obj_set_style_transform_scale_x(obj, scale_x, 0);
  lv_obj_set_style_transform_scale_y(obj, scale_y, 0);
}

void set_top_left_scale(
    lv_obj_t* obj, int scale_x, int scale_y = 256) {
  lv_obj_set_style_transform_pivot_x(obj, 0, 0);
  lv_obj_set_style_transform_pivot_y(obj, 0, 0);
  lv_obj_set_style_transform_scale_x(obj, scale_x, 0);
  lv_obj_set_style_transform_scale_y(obj, scale_y, 0);
}

int scaled_label_width(lv_obj_t* label, int scale_x) {
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

void position_value_group_from_left(
    lv_obj_t* value, int value_scale_x,
    lv_obj_t* unit, int unit_scale_x,
    int left_x, int gap) {
  lv_obj_set_x(value, left_x);
  if (lv_obj_has_flag(unit, LV_OBJ_FLAG_HIDDEN)) return;
  lv_obj_set_x(
      unit,
      left_x + scaled_label_width(value, value_scale_x) + gap);
}

void position_value_group_centered(
    lv_obj_t* value, int value_scale_x,
    lv_obj_t* unit, int unit_scale_x,
    int center_x, int gap) {
  int value_width = scaled_label_width(value, value_scale_x);
  bool has_unit = !lv_obj_has_flag(unit, LV_OBJ_FLAG_HIDDEN);
  int unit_width =
      has_unit ? scaled_label_width(unit, unit_scale_x) : 0;
  int group_width =
      value_width + (has_unit ? gap + unit_width : 0);
  int left_x = center_x - group_width / 2;
  lv_obj_set_x(value, left_x);
  if (has_unit) {
    lv_obj_set_x(unit, left_x + value_width + gap);
  }
}

bool set_unit_visibility(lv_obj_t* unit, const char* text) {
  bool changed = set_label_text_if_changed(unit, text);
  bool hidden = lv_obj_has_flag(unit, LV_OBJ_FLAG_HIDDEN);
  if (text && text[0]) {
    if (hidden) {
      lv_obj_clear_flag(unit, LV_OBJ_FLAG_HIDDEN);
      changed = true;
    }
  } else {
    if (!hidden) {
      lv_obj_add_flag(unit, LV_OBJ_FLAG_HIDDEN);
      changed = true;
    }
  }
  return changed;
}

void make_corner_cut(
    lv_obj_t* parent, int center_x, int center_y,
    lv_color_t color) {
  lv_obj_t* cut = make_rect(
      parent, center_x - 7, center_y - 7,
      14, 14, color, 0);
  lv_obj_set_style_transform_pivot_x(cut, 7, 0);
  lv_obj_set_style_transform_pivot_y(cut, 7, 0);
  lv_obj_set_style_transform_rotation(cut, 450, 0);
}

void make_eye(lv_obj_t* parent, int x, bool right_eye) {
  make_rect(parent, x, 21, 61, 45, DARK, 0);
  make_corner_cut(parent, x, 21, YELLOW);
  make_corner_cut(parent, x + 60, 21, YELLOW);
  make_corner_cut(parent, x, 65, YELLOW);
  make_corner_cut(parent, x + 60, 65, YELLOW);
  lv_obj_t* ring = make_rect(
      parent, x + (right_eye ? 14 : 16), 27, 34, 34,
      YELLOW_LIGHT, LV_RADIUS_CIRCLE);
  lv_obj_t* lens = make_rect(
      ring, 7, 7, 20, 20, DARK, LV_RADIUS_CIRCLE);
  make_rect(
      lens, right_eye ? 5 : 10, 4, 6, 6,
      YELLOW_LIGHT, LV_RADIUS_CIRCLE);
}

void make_header(WalleThemeState* state) {
  make_rect(state->root, 0, 0, 480, 100, YELLOW, 0);
  make_eye(state->root, 18, false);
  make_eye(state->root, 84, true);

  lv_obj_t* title = make_content_label(
      state->root, "WALL·E // CODEX",
      font_or(state->font_30, &lv_font_montserrat_24),
      DARK, 0, 25);
  lv_obj_set_style_text_letter_space(title, 1, 0);
  constexpr int TITLE_SCALE = 185;
  lv_obj_set_style_transform_scale_x(title, TITLE_SCALE, 0);
  lv_obj_set_style_transform_scale_y(title, 242, 0);
  lv_obj_set_x(
      title, 251 - scaled_label_width(title, TITLE_SCALE) / 2);

  state->battery_shell = make_rect(
      state->root, 370, 29, 87, 35, DARK, 7);
  make_rect(
      state->battery_shell, 5, 5, 75, 25,
      YELLOW, 5);
  make_rect(state->root, 457, 38, 7, 17, DARK, 2);
  state->battery_value = make_label(
      state->root, "",
      font_or(state->font_30, &lv_font_montserrat_24),
      DARK, 375, 30, 75, LV_TEXT_ALIGN_CENTER);
  set_center_scale(state->battery_value, 225, 240);

  lv_obj_t* stripe_clip = lv_obj_create(state->root);
  strip_obj(stripe_clip);
  lv_obj_set_pos(stripe_clip, 0, 81);
  lv_obj_set_size(stripe_clip, 343, 19);
  lv_obj_set_style_bg_color(stripe_clip, YELLOW, 0);
  lv_obj_set_style_bg_opa(stripe_clip, LV_OPA_COVER, 0);
  lv_obj_set_style_clip_corner(stripe_clip, true, 0);
  for (int i = 0; i < 8; ++i) {
    lv_obj_t* stripe = make_rect(
        stripe_clip, -17 + i * 49, -5, 37, 24,
        DARK, 0);
    lv_obj_set_style_transform_pivot_x(stripe, 18, 0);
    lv_obj_set_style_transform_pivot_y(stripe, 12, 0);
    lv_obj_set_style_transform_rotation(stripe, 450, 0);
  }
}

void make_secondary_panel(WalleThemeState* state) {
  lv_obj_t* panel =
      make_rect(state->root, 343, 100, 137, 164, YELLOW_LIGHT, 0);
  lv_obj_set_style_bg_grad_color(panel, YELLOW, 0);
  lv_obj_set_style_bg_grad_dir(panel, LV_GRAD_DIR_VER, 0);
  lv_obj_t* chamfer = make_rect(
      state->root, 333, 241, 31, 31, DARK, 0);
  lv_obj_set_style_transform_pivot_x(chamfer, 15, 0);
  lv_obj_set_style_transform_pivot_y(chamfer, 15, 0);
  lv_obj_set_style_transform_rotation(chamfer, 450, 0);
}

void make_quota_panel(WalleThemeState* state) {
  make_rect(state->root, 8, 249, 246, 167, CREAM, 121);
  make_rect(state->root, 8, 329, 246, 87, CREAM, 0);

  constexpr int LEAF_X[QUOTA_LEAF_COUNT] = {
      25, 46, 70, 97, 126, 155, 183, 207, 219, 210};
  constexpr int LEAF_Y[QUOTA_LEAF_COUNT] = {
      302, 283, 270, 264, 261, 265, 274, 289, 314, 338};
  constexpr int LEAF_ROT[QUOTA_LEAF_COUNT] = {
      -580, -430, -280, -130, 30, 180, 340, 500, 620, 690};
  for (int i = 0; i < QUOTA_LEAF_COUNT; ++i) {
    state->quota_leaves[i] = make_rect(
        state->root, LEAF_X[i], LEAF_Y[i],
        13, 25, LEAF_EMPTY, LV_RADIUS_CIRCLE);
    lv_obj_set_style_transform_pivot_x(
        state->quota_leaves[i], 6, 0);
    lv_obj_set_style_transform_pivot_y(
        state->quota_leaves[i], 12, 0);
    lv_obj_set_style_transform_rotation(
        state->quota_leaves[i], LEAF_ROT[i], 0);
  }
}

void make_reset_panel(WalleThemeState* state) {
  lv_obj_t* panel =
      make_rect(state->root, 264, 275, 206, 140, RED, 4);
  lv_obj_set_style_bg_grad_color(
      panel, lv_color_hex(0xD13B2F), 0);
  lv_obj_set_style_bg_grad_dir(panel, LV_GRAD_DIR_VER, 0);
  make_rect(state->root, 259, 303, 11, 17, DARK, 5);
  make_rect(state->root, 464, 303, 11, 17, DARK, 5);

  lv_obj_t* tape = make_rect(
      state->root, 291, 368, 152, 32, BLOCK_DARK, 16);
  make_rect(tape, 7, 5, 21, 21, CREAM, LV_RADIUS_CIRCLE);
  make_rect(tape, 12, 10, 11, 11, TREAD_DARK, LV_RADIUS_CIRCLE);
  make_rect(tape, 16, 14, 3, 3, CREAM, LV_RADIUS_CIRCLE);
  make_rect(tape, 15, 10, 5, 2, CREAM, 1);
  make_rect(tape, 15, 19, 5, 2, CREAM, 1);
  make_rect(tape, 124, 5, 21, 21, CREAM, LV_RADIUS_CIRCLE);
  make_rect(tape, 129, 10, 11, 11, TREAD_DARK, LV_RADIUS_CIRCLE);
  make_rect(tape, 133, 14, 3, 3, CREAM, LV_RADIUS_CIRCLE);
  make_rect(tape, 132, 10, 5, 2, CREAM, 1);
  make_rect(tape, 132, 19, 5, 2, CREAM, 1);
  make_rect(tape, 51, 7, 51, 17, BLUE, 1);
}

void make_tread(lv_obj_t* parent, int x) {
  lv_obj_t* tread = make_rect(
      parent, x, 428, 31, 46, TREAD_DARK, 4);
  for (int i = 0; i < 4; ++i) {
    make_rect(
        tread, 3, 5 + i * 10, 25, 5,
        TREAD_LINE, 1);
  }
}

void make_sprout(WalleThemeState* state) {
  make_rect(state->root, 238, 434, 4, 24, GREEN, 2);
  lv_obj_t* left_leaf = make_rect(
      state->root, 220, 427, 20, 12,
      GREEN, LV_RADIUS_CIRCLE);
  lv_obj_set_style_transform_pivot_x(left_leaf, 10, 0);
  lv_obj_set_style_transform_pivot_y(left_leaf, 6, 0);
  lv_obj_set_style_transform_rotation(left_leaf, 320, 0);
  lv_obj_t* right_leaf = make_rect(
      state->root, 241, 427, 21, 12,
      GREEN, LV_RADIUS_CIRCLE);
  lv_obj_set_style_transform_pivot_x(right_leaf, 10, 0);
  lv_obj_set_style_transform_pivot_y(right_leaf, 6, 0);
  lv_obj_set_style_transform_rotation(right_leaf, -320, 0);
  make_rect(state->root, 229, 453, 21, 17, YELLOW, 0);
}

void make_status_panel(WalleThemeState* state) {
  state->task_count = make_label(
      state->root, "0",
      font_or(state->font_38, &lv_font_montserrat_32),
      YELLOW, 52, 423, 44, LV_TEXT_ALIGN_CENTER);
  set_center_scale(state->task_count, 280, 280);
  state->ble_dot = make_rect(
      state->root, 324, 440, 18, 18,
      GREEN_BRIGHT, LV_RADIUS_CIRCLE);
  lv_obj_set_style_shadow_width(state->ble_dot, 8, 0);
  lv_obj_set_style_shadow_color(state->ble_dot, GREEN_BRIGHT, 0);
  lv_obj_set_style_shadow_opa(state->ble_dot, LV_OPA_50, 0);

  state->sync_value = make_label(
      state->root, "WAIT",
      font_or(state->font_20, &lv_font_montserrat_16),
      BLUE, 354, 441, 47, LV_TEXT_ALIGN_CENTER);
  set_center_scale(state->sync_value, 235, 250);
  state->sync_dot = make_rect(
      state->root, 403, 440, 18, 18,
      MUTED, LV_RADIUS_CIRCLE);
}

void make_dynamic_labels(WalleThemeState* state) {
  state->battery_shell =
      lv_obj_create(state->root);
  strip_obj(state->battery_shell);
  lv_obj_set_size(state->battery_shell, 87, 35);
  lv_obj_set_pos(state->battery_shell, 370, 29);
  lv_obj_set_style_bg_opa(
      state->battery_shell, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(
      state->battery_shell, 3, 0);
  lv_obj_set_style_border_opa(
      state->battery_shell, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(state->battery_shell, 7, 0);
  state->battery_value = make_label(
      state->root, "",
      font_or(state->font_30, &lv_font_montserrat_24),
      DARK, 375, 30, 75, LV_TEXT_ALIGN_CENTER);
  set_center_scale(state->battery_value, 225, 240);

  state->primary_heading = make_label(
      state->root, "TODAY TOKEN",
      font_or(state->font_24, &lv_font_montserrat_24),
      CREAM, 25, 109, 250);
  set_top_left_scale(state->primary_heading, 225, 260);
  state->primary_value = make_content_label(
      state->root, "",
      font_or(state->font_120, state->resources.token_font),
      CREAM, 18, 101);
  state->primary_unit = make_content_label(
      state->root, "",
      font_or(state->font_82, &lv_font_montserrat_48),
      CREAM, 0, 142);

  state->secondary_heading = make_label(
      state->root, "7 DAYS",
      font_or(state->font_24, &lv_font_montserrat_24),
      DARK, 359, 128, 108);
  set_top_left_scale(state->secondary_heading, 261, 270);
  state->secondary_value = make_content_label(
      state->root, "",
      font_or(state->font_82, &lv_font_montserrat_48),
      DARK, 356, 133);
  state->secondary_unit = make_content_label(
      state->root, "",
      font_or(state->font_60, &lv_font_montserrat_48),
      DARK, 0, 158);

  state->quota_value = make_content_label(
      state->root, "",
      font_or(state->font_82, &lv_font_montserrat_48),
      DARK, 58, 313);
  state->quota_unit = make_content_label(
      state->root, "%",
      font_or(state->font_60, &lv_font_montserrat_48),
      DARK, 0, 339);

  state->reset_day_value = make_content_label(
      state->root, "",
      font_or(state->font_60, &lv_font_montserrat_48),
      CREAM, 0, 292);
  state->reset_day_unit = make_content_label(
      state->root, "D",
      font_or(state->font_38, &lv_font_montserrat_32),
      CREAM, 0, 313);
  state->reset_hour_value = make_content_label(
      state->root, "",
      font_or(state->font_60, &lv_font_montserrat_48),
      CREAM, 0, 292);
  state->reset_hour_unit = make_content_label(
      state->root, "H",
      font_or(state->font_38, &lv_font_montserrat_32),
      CREAM, 0, 313);
}

void update_primary(
    WalleThemeState* state,
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
  bool value_changed =
      set_label_text_if_changed(state->primary_value, value);
  bool unit_changed =
      set_unit_visibility(state->primary_unit, unit);
  if (!value_changed && !unit_changed) return;
  int value_scale =
      strlen(value) >= 6 ? 220
                         : (strlen(value) >= 5 ? 270 : 308);
  constexpr int UNIT_SCALE = 294;
  lv_obj_set_style_transform_scale_x(
      state->primary_value, value_scale, 0);
  lv_obj_set_style_transform_scale_y(
      state->primary_value, 300, 0);
  lv_obj_set_style_transform_scale_x(
      state->primary_unit, UNIT_SCALE, 0);
  lv_obj_set_style_transform_scale_y(
      state->primary_unit, 300, 0);
  position_value_group_from_left(
      state->primary_value, value_scale,
      state->primary_unit, UNIT_SCALE, 20, 0);
}

void update_secondary(
    WalleThemeState* state,
    const DashboardViewModel& model) {
  const char* source =
      model.token_usage_mode
          ? model.last_7d_tokens_text
          : model.d7_percent_text;
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
  bool value_changed =
      set_label_text_if_changed(state->secondary_value, value);
  bool unit_changed =
      set_unit_visibility(state->secondary_unit, unit);
  if (!value_changed && !unit_changed) return;
  int value_scale =
      strlen(value) >= 5 ? 164 : 187;
  constexpr int UNIT_SCALE = 204;
  lv_obj_set_style_transform_scale_x(
      state->secondary_value, value_scale, 0);
  lv_obj_set_style_transform_scale_y(
      state->secondary_value, 290, 0);
  lv_obj_set_style_transform_scale_x(
      state->secondary_unit, UNIT_SCALE, 0);
  lv_obj_set_style_transform_scale_y(
      state->secondary_unit, 290, 0);
  position_value_group_from_left(
      state->secondary_value, value_scale,
      state->secondary_unit, UNIT_SCALE, 356, 3);
}

void update_quota(
    WalleThemeState* state,
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
    int value_scale = strlen(value) >= 3 ? 243 : 278;
    constexpr int UNIT_SCALE = 251;
    lv_obj_set_style_transform_scale_x(
        state->quota_value, value_scale, 0);
    lv_obj_set_style_transform_scale_y(
        state->quota_value, 282, 0);
    lv_obj_set_style_transform_scale_x(
        state->quota_unit, UNIT_SCALE, 0);
    lv_obj_set_style_transform_scale_y(
        state->quota_unit, 275, 0);
    position_value_group_centered(
        state->quota_value, value_scale,
        state->quota_unit, UNIT_SCALE, 131, 2);
  }

  int percent = model.d7_remaining;
  if (percent < 0) percent = -1;
  if (percent > 100) percent = 100;
  if (percent == state->quota_percent) return;
  state->quota_percent = percent;
  int active =
      percent < 0
          ? 0
          : (percent * RUNTIME_LEAF_COUNT + 50) / 100;
  if (active > RUNTIME_LEAF_COUNT) {
    active = RUNTIME_LEAF_COUNT;
  }

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
      background[background_index] =
          leaf_index <= active
              ? state->leaf_active_pixels[crop_index]
              : state->leaf_base_pixels[crop_index];
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
    WalleThemeState* state,
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
  constexpr int VALUE_SCALE = 270;
  constexpr int UNIT_SCALE = 290;
  constexpr int UNIT_GAP = 1;
  constexpr int GROUP_GAP = 8;
  lv_obj_set_style_transform_scale_x(
      state->reset_day_value, VALUE_SCALE, 0);
  lv_obj_set_style_transform_scale_y(
      state->reset_day_value, 300, 0);
  lv_obj_set_style_transform_scale_x(
      state->reset_day_unit, UNIT_SCALE, 0);
  lv_obj_set_style_transform_scale_y(
      state->reset_day_unit, 330, 0);
  lv_obj_set_style_transform_scale_x(
      state->reset_hour_value, VALUE_SCALE, 0);
  lv_obj_set_style_transform_scale_y(
      state->reset_hour_value, 300, 0);
  lv_obj_set_style_transform_scale_x(
      state->reset_hour_unit, UNIT_SCALE, 0);
  lv_obj_set_style_transform_scale_y(
      state->reset_hour_unit, 330, 0);

  int day_value_width =
      scaled_label_width(state->reset_day_value, VALUE_SCALE);
  int day_unit_width =
      scaled_label_width(state->reset_day_unit, UNIT_SCALE);
  int hour_value_width =
      scaled_label_width(state->reset_hour_value, VALUE_SCALE);
  int hour_unit_width =
      scaled_label_width(state->reset_hour_unit, UNIT_SCALE);
  int group_width =
      day_value_width + UNIT_GAP + day_unit_width +
      GROUP_GAP + hour_value_width + UNIT_GAP + hour_unit_width;
  int x = 364 - group_width / 2;
  lv_obj_set_x(state->reset_day_value, x);
  x += day_value_width + UNIT_GAP;
  lv_obj_set_x(state->reset_day_unit, x);
  x += day_unit_width + GROUP_GAP;
  lv_obj_set_x(state->reset_hour_value, x);
  x += hour_value_width + UNIT_GAP;
  lv_obj_set_x(state->reset_hour_unit, x);
}

void update_battery(
    WalleThemeState* state, int percent, bool charging) {
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
      strlen(text) >= 4 ? 205
                        : (strlen(text) == 3 ? 225 : 240);
  set_center_scale(state->battery_value, scale, 240);
  bool emphasized =
      charging || (percent >= 0 && percent <= 10);
  lv_obj_set_style_border_color(
      state->battery_shell,
      charging ? GREEN : RED_LOW, 0);
  lv_obj_set_style_border_opa(
      state->battery_shell,
      emphasized ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

void update_activity(
    WalleThemeState* state, int running_count) {
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
  set_center_scale(
      state->task_count,
      strlen(text) >= 3 ? 220 : 280, 280);
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

lv_color_t sync_color_for(DashboardDataState data_state) {
  switch (data_state) {
    case DashboardDataState::Ready:
      return GREEN_BRIGHT;
    case DashboardDataState::Stale:
      return YELLOW;
    case DashboardDataState::Error:
      return RED;
    case DashboardDataState::Waiting:
    default:
      return MUTED;
  }
}

void update_data_state(
    WalleThemeState* state,
    DashboardDataState data_state) {
  if (state->data_state_initialized &&
      state->data_state == data_state) {
    return;
  }
  state->data_state_initialized = true;
  state->data_state = data_state;
  set_label_text_if_changed(
      state->sync_value, sync_text_for(data_state));
  lv_color_t color = sync_color_for(data_state);
  lv_obj_set_style_bg_color(state->sync_dot, color, 0);
  lv_obj_set_style_shadow_width(
      state->sync_dot,
      data_state == DashboardDataState::Ready ? 8 : 0, 0);
  lv_obj_set_style_shadow_color(state->sync_dot, color, 0);
  lv_obj_set_style_shadow_opa(
      state->sync_dot,
      data_state == DashboardDataState::Ready
          ? LV_OPA_50
          : LV_OPA_TRANSP,
      0);
}

bool create_fonts(WalleThemeState* state) {
#if LV_USE_TINY_TTF && LV_TINY_TTF_FILE_SUPPORT
  state->font_120 = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, 120,
      LV_FONT_KERNING_NORMAL, 16);
  state->font_82 = lv_tiny_ttf_create_file_ex(
      VALUE_FONT_PATH, 82,
      LV_FONT_KERNING_NORMAL, 16);
  state->font_60 = lv_tiny_ttf_create_file_ex(
      AUX_FONT_PATH, 60,
      LV_FONT_KERNING_NORMAL, 20);
  state->font_38 = lv_tiny_ttf_create_file_ex(
      AUX_FONT_PATH, 38,
      LV_FONT_KERNING_NORMAL, 24);
  state->font_30 = lv_tiny_ttf_create_file_ex(
      AUX_FONT_PATH, 30,
      LV_FONT_KERNING_NORMAL, 24);
  state->font_24 = lv_tiny_ttf_create_file_ex(
      LABEL_FONT_PATH, 24,
      LV_FONT_KERNING_NORMAL, 32);
  state->font_20 = lv_tiny_ttf_create_file_ex(
      LABEL_FONT_PATH, 20,
      LV_FONT_KERNING_NORMAL, 32);
  if (state->font_120) {
    state->font_120->fallback = state->resources.token_font;
  }
  if (state->font_82) {
    state->font_82->fallback = &lv_font_montserrat_48;
  }
  if (state->font_60) {
    state->font_60->fallback = &lv_font_montserrat_48;
  }
  if (state->font_38) {
    state->font_38->fallback = &lv_font_montserrat_32;
  }
  if (state->font_30) {
    state->font_30->fallback = &lv_font_montserrat_24;
  }
  if (state->font_24) {
    state->font_24->fallback = &lv_font_montserrat_24;
  }
  if (state->font_20) {
    state->font_20->fallback = &lv_font_montserrat_16;
  }
#endif
  return true;
}

bool walle_mount(
    void* raw_state, lv_obj_t* parent,
    const ThemeResources& resources) {
  auto* state =
      static_cast<WalleThemeState*>(raw_state);
  *state = WalleThemeState{};
  state->resources = resources;
  if (!load_assets(state)) return false;
  create_fonts(state);

  state->root = lv_obj_create(parent);
  strip_obj(state->root);
  lv_obj_set_size(
      state->root, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(state->root, 0, 0);
  lv_obj_set_style_bg_color(state->root, DARK, 0);
  lv_obj_set_style_bg_opa(state->root, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(state->root, 28, 0);
  lv_obj_set_style_clip_corner(state->root, true, 0);

  make_background(state);
  make_dynamic_labels(state);
  make_status_panel(state);
  return true;
}

void walle_update(
    void* raw_state, const DashboardViewModel& model) {
  auto* state =
      static_cast<WalleThemeState*>(raw_state);
  update_primary(state, model);
  update_secondary(state, model);
  update_quota(state, model);
  update_reset(state, model);
  update_battery(
      state, model.battery_percent, model.charging);
  update_activity(state, model.running_count);
  update_data_state(state, model.data_state);
}

void walle_tick(void* raw_state, uint32_t now_ms) {
  auto* state =
      static_cast<WalleThemeState*>(raw_state);
  if (state->data_state != DashboardDataState::Ready) return;
  constexpr uint32_t PERIOD_MS = 3400;
  constexpr uint32_t HALF_MS = PERIOD_MS / 2;
  uint32_t phase = now_ms % PERIOD_MS;
  uint32_t ramp =
      phase <= HALF_MS ? phase : PERIOD_MS - phase;
  lv_opa_t opacity =
      static_cast<lv_opa_t>(
          205 + (50UL * ramp) / HALF_MS);
  if (opacity == state->sync_dot_opacity) return;
  state->sync_dot_opacity = opacity;
  lv_obj_set_style_bg_opa(state->sync_dot, opacity, 0);
}

void destroy_font(lv_font_t*& font) {
#if LV_USE_TINY_TTF
  if (font) lv_tiny_ttf_destroy(font);
#endif
  font = nullptr;
}

void walle_unmount(void* raw_state) {
  auto* state =
      static_cast<WalleThemeState*>(raw_state);
  if (state->root) lv_obj_delete(state->root);
  state->root = nullptr;

  destroy_font(state->font_120);
  destroy_font(state->font_82);
  destroy_font(state->font_60);
  destroy_font(state->font_38);
  destroy_font(state->font_30);
  destroy_font(state->font_24);
  destroy_font(state->font_20);
  release_asset_buffers(state);
}

const ThemeDashboardOps WALLE_DASHBOARD = {
    sizeof(WalleThemeState),
    walle_mount,
    walle_update,
    walle_tick,
    walle_unmount,
};

const ThemePack WALLE_THEME = {
    "walle",
    "WALL-E",
    1,
    CODEXMETER_BURN_IN_DRIFT_MAX_PX,
    WALLE_DASHBOARD,
    nullptr,
    nullptr,
};

static_assert(
    sizeof(WalleThemeState) <= CODEXMETER_THEME_STATE_BYTES,
    "WALL-E theme state exceeds ThemeRuntime storage");

}  // namespace

const ThemePack& walle_theme_pack() {
  return WALLE_THEME;
}
