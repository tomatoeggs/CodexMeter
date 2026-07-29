#include "gargantua_theme.h"

#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "device_log.h"

namespace {

constexpr char BACKGROUND_PATH[] =
    "S:/themes/gargantua_bg.rgb565";
constexpr char DISPLAY_FONT_PATH[] =
    "S:/fonts/Teko-SemiBold.ttf";
constexpr char LABEL_FONT_PATH[] =
    "S:/fonts/D-DINCondensed-Bold.ttf";
constexpr size_t BACKGROUND_BYTES =
    CODEXMETER_SCREEN_W * CODEXMETER_SCREEN_H * sizeof(uint16_t);

const lv_color_t TEXT = lv_color_hex(0xEFC4B1);
const lv_color_t BATTERY = lv_color_hex(0xD9AA95);

struct GargantuaThemeState {
  lv_obj_t* root = nullptr;
  lv_obj_t* background = nullptr;

  lv_obj_t* today_heading = nullptr;
  lv_obj_t* today_value = nullptr;
  lv_obj_t* today_unit = nullptr;
  lv_obj_t* week_heading = nullptr;
  lv_obj_t* week_value = nullptr;
  lv_obj_t* week_unit = nullptr;

  lv_obj_t* battery_shell = nullptr;
  lv_obj_t* battery_tip = nullptr;
  lv_obj_t* battery_fill = nullptr;
  lv_obj_t* battery_value = nullptr;

  lv_obj_t* quota_heading = nullptr;
  lv_obj_t* quota_value = nullptr;
  lv_obj_t* quota_unit = nullptr;
  lv_obj_t* reset_heading = nullptr;
  lv_obj_t* reset_value = nullptr;
  lv_obj_t* task_count = nullptr;
  lv_obj_t* task_label = nullptr;

  lv_font_t* display_118 = nullptr;
  lv_font_t* display_68 = nullptr;
  lv_font_t* display_50 = nullptr;
  lv_font_t* display_38 = nullptr;
  lv_font_t* display_34 = nullptr;
  lv_font_t* display_22 = nullptr;
  lv_font_t* label_41 = nullptr;
  lv_font_t* label_34 = nullptr;
  lv_font_t* label_31 = nullptr;
  lv_font_t* label_24 = nullptr;
  lv_font_t* label_22 = nullptr;

  uint8_t* background_pixels = nullptr;
  lv_image_dsc_t background_dsc{};
  ThemeResources resources{};

  int battery_percent = -2;
  bool battery_charging = false;
  bool battery_initialized = false;
  int running_count = -1;
  int quota_scale = -1;
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
    int x, int y, int width = LV_SIZE_CONTENT,
    lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, width);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, TEXT, 0);
  lv_obj_set_style_text_align(label, align, 0);
  lv_obj_set_style_text_letter_space(label, 0, 0);
  lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
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

void set_scale(
    lv_obj_t* label, int scale_x, int scale_y = 256) {
  lv_obj_set_style_transform_pivot_x(label, 0, 0);
  lv_obj_set_style_transform_pivot_y(label, 0, 0);
  lv_obj_set_style_transform_scale_x(label, scale_x, 0);
  lv_obj_set_style_transform_scale_y(label, scale_y, 0);
}

int scaled_width(lv_obj_t* label, int scale_x) {
  lv_obj_update_layout(label);
  return (lv_obj_get_width(label) * scale_x + 128) / 256;
}

void split_suffix(
    const char* source, char* value, size_t value_size,
    char* unit, size_t unit_size) {
  if (!source || !source[0]) {
    strlcpy(value, "--", value_size);
    unit[0] = '\0';
    return;
  }
  size_t length = strlen(source);
  char last = source[length - 1];
  bool has_suffix = last >= 'A' && last <= 'Z';
  size_t value_length = has_suffix ? length - 1 : length;
  if (value_length >= value_size) value_length = value_size - 1;
  memcpy(value, source, value_length);
  value[value_length] = '\0';
  if (has_suffix && unit_size >= 2) {
    unit[0] = last;
    unit[1] = '\0';
  } else {
    unit[0] = '\0';
  }
}

void position_unit(
    lv_obj_t* value, int value_scale,
    lv_obj_t* unit, int left, int gap) {
  lv_obj_set_x(value, left);
  if (lv_obj_has_flag(unit, LV_OBJ_FLAG_HIDDEN)) return;
  lv_obj_set_x(
      unit, left + scaled_width(value, value_scale) + gap);
}

bool read_background(GargantuaThemeState* state) {
  state->background_pixels = static_cast<uint8_t*>(
      heap_caps_malloc(
          BACKGROUND_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!state->background_pixels) {
    device_logf(
        "ERROR", "gargantua background alloc failed bytes=%lu",
        static_cast<unsigned long>(BACKGROUND_BYTES));
    return false;
  }

  lv_fs_file_t file;
  lv_fs_res_t result =
      lv_fs_open(&file, BACKGROUND_PATH, LV_FS_MODE_RD);
  if (result != LV_FS_RES_OK) {
    device_logf(
        "ERROR", "gargantua background open failed path=%s res=%d",
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
        "gargantua background read failed bytes=%lu expected=%lu res=%d",
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

lv_font_t* create_font(
    const char* path, int size, const lv_font_t* fallback,
    size_t cache_size = 24) {
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

void create_fonts(GargantuaThemeState* state) {
  const lv_font_t* display_fallback =
      font_or(state->resources.token_font, &lv_font_montserrat_48);
  state->display_118 = create_font(
      DISPLAY_FONT_PATH, 118, display_fallback, 12);
  state->display_68 = create_font(
      DISPLAY_FONT_PATH, 68, display_fallback, 16);
  state->display_50 = create_font(
      DISPLAY_FONT_PATH, 50, display_fallback, 16);
  state->display_38 = create_font(
      DISPLAY_FONT_PATH, 38, &lv_font_montserrat_32, 18);
  state->display_34 = create_font(
      DISPLAY_FONT_PATH, 34, &lv_font_montserrat_32, 16);
  state->display_22 = create_font(
      DISPLAY_FONT_PATH, 22, &lv_font_montserrat_24, 16);
  state->label_41 = create_font(
      LABEL_FONT_PATH, 41, &lv_font_montserrat_32, 12);
  state->label_34 = create_font(
      LABEL_FONT_PATH, 34, &lv_font_montserrat_32, 12);
  state->label_31 = create_font(
      LABEL_FONT_PATH, 31, &lv_font_montserrat_32, 12);
  state->label_24 = create_font(
      LABEL_FONT_PATH, 24, &lv_font_montserrat_24, 20);
  state->label_22 = create_font(
      LABEL_FONT_PATH, 22, &lv_font_montserrat_24, 32);
}

void make_dynamic_layer(GargantuaThemeState* state) {
  const lv_font_t* label22 =
      font_or(state->label_22, &lv_font_montserrat_24);

  state->today_heading = make_label(
      state->root, "TODAY TOKEN", label22, 21, 18);
  state->week_heading = make_label(
      state->root, "7 DAYS", label22, 20, 134);
  state->quota_heading = make_label(
      state->root, "7D REMAINS", label22, 20, 349);
  state->reset_heading = make_label(
      state->root, "RESET", label22, 360, 349, 100,
      LV_TEXT_ALIGN_RIGHT);
  for (lv_obj_t* label : {
       state->today_heading, state->week_heading,
       state->quota_heading, state->reset_heading}) {
    lv_obj_set_style_text_letter_space(label, 1, 0);
  }
  set_scale(state->today_heading, 399, 272);
  set_scale(state->week_heading, 389, 272);
  set_scale(state->quota_heading, 383, 272);
  set_scale(state->reset_heading, 429, 272);
  lv_obj_set_style_transform_pivot_x(
      state->reset_heading, 100, 0);
  lv_obj_set_x(state->reset_heading, 361);

  state->today_value = make_label(
      state->root, "--",
      font_or(state->display_68, state->resources.token_font),
      18, 25);
  set_scale(state->today_value, 324, 292);
  state->today_unit = make_label(
      state->root, "",
      font_or(state->label_34, &lv_font_montserrat_32),
      158, 69);
  set_scale(state->today_unit, 361, 270);

  state->week_value = make_label(
      state->root, "--",
      font_or(state->display_50, &lv_font_montserrat_48),
      19, 148);
  set_scale(state->week_value, 333, 288);
  state->week_unit = make_label(
      state->root, "",
      font_or(state->label_31, &lv_font_montserrat_32),
      108, 174);
  set_scale(state->week_unit, 397, 270);

  state->battery_shell = make_rect(
      state->root, 379, 25, 33, 19, BATTERY, 4);
  lv_obj_set_style_bg_opa(
      state->battery_shell, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(state->battery_shell, 3, 0);
  lv_obj_set_style_border_color(state->battery_shell, BATTERY, 0);
  state->battery_fill = make_rect(
      state->root, 384, 30, 22, 9, BATTERY, 1);
  state->battery_tip = make_rect(
      state->root, 412, 31, 4, 8, BATTERY, 1);
  state->battery_value = make_label(
      state->root, "--%",
      font_or(state->display_22, &lv_font_montserrat_24),
      423, 23);
  set_scale(state->battery_value, 318);

  state->quota_value = make_label(
      state->root, "--",
      font_or(state->display_118, state->resources.token_font),
      15, 335);
  set_scale(state->quota_value, 330, 296);
  state->quota_unit = make_label(
      state->root, "%",
      font_or(state->label_41, &lv_font_montserrat_32),
      144, 419);
  set_scale(state->quota_unit, 310, 271);

  state->reset_value = make_label(
      state->root, "--D --H",
      font_or(state->display_38, &lv_font_montserrat_32),
      260, 366, 200, LV_TEXT_ALIGN_RIGHT);
  set_scale(state->reset_value, 366, 294);
  lv_obj_set_style_transform_pivot_x(
      state->reset_value, 200, 0);

  state->task_label = make_label(
      state->root, "TASKS",
      font_or(state->label_24, &lv_font_montserrat_24),
      381, 441, 80, LV_TEXT_ALIGN_RIGHT);
  lv_obj_set_style_text_letter_space(state->task_label, 1, 0);
  set_scale(state->task_label, 395, 283);
  lv_obj_set_style_transform_pivot_x(
      state->task_label, 80, 0);
  state->task_count = make_label(
      state->root, "00",
      font_or(state->display_34, &lv_font_montserrat_32),
      324, 427, 44, LV_TEXT_ALIGN_LEFT);
  set_scale(state->task_count, 327, 293);
}

void update_tokens(GargantuaThemeState* state,
                   const DashboardViewModel& model) {
  char value[24];
  char unit[4];
  split_suffix(
      model.today_tokens_text,
      value, sizeof(value), unit, sizeof(unit));
  bool today_changed =
      set_label_text_if_changed(state->today_value, value);
  bool today_unit_changed =
      set_unit_visibility(state->today_unit, unit);
  if (today_changed || today_unit_changed) {
    position_unit(
        state->today_value, 324,
        state->today_unit, 18, 9);
  }

  split_suffix(
      model.last_7d_tokens_text,
      value, sizeof(value), unit, sizeof(unit));
  bool week_changed =
      set_label_text_if_changed(state->week_value, value);
  bool week_unit_changed =
      set_unit_visibility(state->week_unit, unit);
  if (week_changed || week_unit_changed) {
    position_unit(
        state->week_value, 333,
        state->week_unit, 19, 6);
  }
}

void update_quota(GargantuaThemeState* state,
                  const DashboardViewModel& model) {
  char value[8];
  if (model.d7_remaining < 0) {
    strlcpy(value, "--", sizeof(value));
  } else {
    snprintf(value, sizeof(value), "%d", model.d7_remaining);
  }
  bool value_changed =
      set_label_text_if_changed(state->quota_value, value);
  int value_scale = strlen(value) >= 3 ? 205 : 330;
  if (value_changed || state->quota_scale != value_scale) {
    state->quota_scale = value_scale;
    set_scale(state->quota_value, value_scale, 296);
    position_unit(
        state->quota_value, value_scale,
        state->quota_unit, 15, -1);
  }
}

void update_reset(GargantuaThemeState* state,
                  const DashboardViewModel& model) {
  char text[24];
  if (model.d7_reset_seconds < 0) {
    strlcpy(text, "--D --H", sizeof(text));
  } else {
    long total_hours =
        (static_cast<long>(model.d7_reset_seconds) + 3599L) / 3600L;
    long days = total_hours / 24L;
    long hours = total_hours % 24L;
    if (days > 99L) days = 99L;
    snprintf(text, sizeof(text), "%02ldD %02ldH", days, hours);
  }
  set_label_text_if_changed(state->reset_value, text);
}

void update_battery(GargantuaThemeState* state,
                    int percent, bool charging) {
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
}

void update_activity(GargantuaThemeState* state,
                     int running_count) {
  if (running_count < 0) running_count = 0;
  if (running_count == state->running_count) return;
  state->running_count = running_count;

  char text[8];
  if (running_count > 99) running_count = 99;
  snprintf(text, sizeof(text), "%02d", running_count);
  set_label_text_if_changed(state->task_count, text);
}

bool gargantua_mount(
    void* raw_state, lv_obj_t* parent,
    const ThemeResources& resources) {
  auto* state = static_cast<GargantuaThemeState*>(raw_state);
  *state = GargantuaThemeState{};
  state->resources = resources;
  if (!read_background(state)) return false;
  create_fonts(state);

  state->root = lv_obj_create(parent);
  strip_obj(state->root);
  lv_obj_set_size(
      state->root, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(state->root, 0, 0);
  lv_obj_set_style_bg_color(state->root, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(state->root, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(state->root, 28, 0);
  lv_obj_set_style_clip_corner(state->root, true, 0);

  state->background = lv_image_create(state->root);
  lv_image_set_src(state->background, &state->background_dsc);
  lv_obj_set_pos(state->background, 0, 0);
  make_dynamic_layer(state);
  return true;
}

void gargantua_update(
    void* raw_state, const DashboardViewModel& model) {
  auto* state = static_cast<GargantuaThemeState*>(raw_state);
  update_tokens(state, model);
  update_quota(state, model);
  update_reset(state, model);
  update_battery(state, model.battery_percent, model.charging);
  update_activity(state, model.running_count);
}

void gargantua_tick(void* raw_state, uint32_t now_ms) {
  (void)raw_state;
  (void)now_ms;
}

void destroy_font(lv_font_t*& font) {
#if LV_USE_TINY_TTF
  if (font) lv_tiny_ttf_destroy(font);
#endif
  font = nullptr;
}

void gargantua_unmount(void* raw_state) {
  auto* state = static_cast<GargantuaThemeState*>(raw_state);
  if (state->root) lv_obj_delete(state->root);
  state->root = nullptr;

  destroy_font(state->display_118);
  destroy_font(state->display_68);
  destroy_font(state->display_50);
  destroy_font(state->display_38);
  destroy_font(state->display_34);
  destroy_font(state->display_22);
  destroy_font(state->label_41);
  destroy_font(state->label_34);
  destroy_font(state->label_31);
  destroy_font(state->label_24);
  destroy_font(state->label_22);

  if (state->background_pixels) {
    heap_caps_free(state->background_pixels);
  }
  state->background_pixels = nullptr;
  state->background_dsc = lv_image_dsc_t{};
}

const ThemeDashboardOps GARGANTUA_DASHBOARD = {
    sizeof(GargantuaThemeState),
    gargantua_mount,
    gargantua_update,
    gargantua_tick,
    gargantua_unmount,
};

const ThemePack GARGANTUA_THEME = {
    "gargantua",
    "GARGANTUA",
    1,
    0,
    GARGANTUA_DASHBOARD,
    nullptr,
    nullptr,
};

static_assert(
    sizeof(GargantuaThemeState) <= CODEXMETER_THEME_STATE_BYTES,
    "Gargantua theme state exceeds ThemeRuntime storage");

}  // namespace

const ThemePack& gargantua_theme_pack() {
  return GARGANTUA_THEME;
}
