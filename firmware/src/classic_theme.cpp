#include "classic_theme.h"

#include <math.h>
#include <string.h>

#include "config.h"

namespace {

constexpr int PANEL_W = CODEXMETER_SCREEN_W - 40;
constexpr int PANEL_H = 158;
constexpr int PANEL_CONTENT_W = PANEL_W - 64;
constexpr int PANEL_CONTENT_H = 108;
constexpr int TOKEN_RIGHT_COLUMN_X = 216;
constexpr int TOKEN_VALUE_Y_OFFSET = 7;
constexpr int TOP_TITLE_DEFAULT_Y = 17;
constexpr int TOP_TITLE_TOKEN_Y = 14;
constexpr int ACTIVITY_MAX_DOTS = 12;
constexpr int ACTIVITY_DOT_SIZE = 10;
constexpr int ACTIVITY_DOT_GAP = 9;
constexpr int ACTIVITY_DOT_Y = 452;
constexpr uint32_t ACTIVITY_ANIM_INTERVAL_MS = 33;
constexpr uint32_t ACTIVITY_COLOR_CYCLE_MS = 7000;
constexpr uint32_t ACTIVITY_COLOR_DOT_PHASE_MS = 420;
constexpr uint32_t ACTIVITY_COLOR_SPREAD_IN_MS = 2200;
constexpr uint32_t ACTIVITY_BREATH_CYCLE_MS = 4000;
constexpr uint32_t ACTIVITY_DOT_PHASE_MS = 180;
constexpr uint16_t ACTIVITY_COLOR_START_HUE = 203;
constexpr uint8_t ACTIVITY_COLOR_SATURATION = 91;
constexpr uint8_t ACTIVITY_COLOR_VALUE = 100;
constexpr float ACTIVITY_TWO_PI = 6.28318530718f;
constexpr uint8_t ACTIVITY_OPA_MIN = 135;
constexpr uint8_t ACTIVITY_OPA_MAX = 255;

const lv_color_t BG = lv_color_hex(0x0f1115);
const lv_color_t PANEL = lv_color_hex(0x1b1f2a);
const lv_color_t TEXT = lv_color_hex(0xf6f7fb);
const lv_color_t DIM = lv_color_hex(0x8d96a8);
const lv_color_t CODEX_BLUE = lv_color_hex(0x18a8ff);
const lv_color_t BATTERY_GREEN = lv_color_hex(0x2fda77);
const lv_color_t BATTERY_YELLOW = lv_color_hex(0xffd43b);
const lv_color_t BATTERY_RED = lv_color_hex(0xff4d4f);

struct ClassicThemeState {
  lv_obj_t* root = nullptr;
  lv_obj_t* top_title = nullptr;
  lv_obj_t* top_battery = nullptr;
  lv_obj_t* top_battery_fill = nullptr;
  lv_obj_t* h5_content = nullptr;
  lv_obj_t* h5_pct = nullptr;
  lv_obj_t* h5_reset = nullptr;
  lv_obj_t* token_content = nullptr;
  lv_obj_t* token_today_value = nullptr;
  lv_obj_t* token_7d_value = nullptr;
  lv_obj_t* d7_heading = nullptr;
  lv_obj_t* d7_pct = nullptr;
  lv_obj_t* d7_reset = nullptr;
  lv_obj_t* activity_dots[ACTIVITY_MAX_DOTS]{};
  ThemeResources resources{};
  bool token_usage_mode = false;
  int activity_visible_count = 0;
  uint32_t activity_anim_started_ms = 0;
  uint32_t activity_anim_last_ms = 0;
  uint32_t activity_dot_started_ms[ACTIVITY_MAX_DOTS]{};
};

void strip_obj(lv_obj_t* obj) {
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

lv_obj_t* make_label(
    lv_obj_t* parent, const char* text, const lv_font_t* font,
    lv_color_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_letter_space(label, 0, 0);
  return label;
}

lv_obj_t* make_panel(ClassicThemeState* state, int y) {
  lv_obj_t* panel = lv_obj_create(state->root);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_size(panel, PANEL_W, PANEL_H);
  lv_obj_set_pos(panel, 20, y);
  lv_obj_set_style_radius(panel, 8, 0);
  lv_obj_set_style_bg_color(panel, PANEL, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0x303747), 0);
  return panel;
}

lv_obj_t* make_panel_content(lv_obj_t* panel) {
  lv_obj_t* content = lv_obj_create(panel);
  strip_obj(content);
  lv_obj_set_size(content, PANEL_CONTENT_W, PANEL_CONTENT_H);
  lv_obj_center(content);
  return content;
}

lv_obj_t* make_battery_icon(ClassicThemeState* state) {
  lv_obj_t* icon = lv_obj_create(state->root);
  strip_obj(icon);
  lv_obj_set_size(icon, 31, 18);

  lv_obj_t* body = lv_obj_create(icon);
  strip_obj(body);
  lv_obj_set_size(body, 25, 16);
  lv_obj_set_pos(body, 0, 1);
  lv_obj_set_style_radius(body, 3, 0);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(body, 2, 0);
  lv_obj_set_style_border_color(body, DIM, 0);
  lv_obj_set_style_border_opa(body, LV_OPA_COVER, 0);

  state->top_battery_fill = lv_obj_create(body);
  strip_obj(state->top_battery_fill);
  lv_obj_set_size(state->top_battery_fill, 18, 8);
  lv_obj_set_pos(state->top_battery_fill, 3, 4);
  lv_obj_set_style_radius(state->top_battery_fill, 1, 0);
  lv_obj_set_style_bg_color(state->top_battery_fill, DIM, 0);
  lv_obj_set_style_bg_opa(state->top_battery_fill, LV_OPA_COVER, 0);

  lv_obj_t* cap = lv_obj_create(icon);
  strip_obj(cap);
  lv_obj_set_size(cap, 4, 8);
  lv_obj_set_pos(cap, 27, 5);
  lv_obj_set_style_radius(cap, 1, 0);
  lv_obj_set_style_bg_color(cap, DIM, 0);
  lv_obj_set_style_bg_opa(cap, LV_OPA_COVER, 0);
  return icon;
}

lv_obj_t* make_activity_dot(lv_obj_t* parent) {
  lv_obj_t* dot = lv_obj_create(parent);
  strip_obj(dot);
  lv_obj_set_size(dot, ACTIVITY_DOT_SIZE, ACTIVITY_DOT_SIZE);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, CODEX_BLUE, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
  return dot;
}

float eased_cycle_phase(uint32_t cycle_ms, uint32_t cycle_duration_ms) {
  if (cycle_duration_ms == 0) return 0.0f;
  float phase = static_cast<float>(cycle_ms) /
                static_cast<float>(cycle_duration_ms);
  return phase - sinf(ACTIVITY_TWO_PI * phase) / ACTIVITY_TWO_PI;
}

float smoothstep01(float phase) {
  if (phase <= 0.0f) return 0.0f;
  if (phase >= 1.0f) return 1.0f;
  return phase * phase * (3.0f - 2.0f * phase);
}

uint32_t activity_dot_color_offset_ms(uint32_t dot_age_ms, int index) {
  uint32_t target_ms = index * ACTIVITY_COLOR_DOT_PHASE_MS;
  if (target_ms == 0) return 0;
  float spread =
      smoothstep01(static_cast<float>(dot_age_ms) /
                   static_cast<float>(ACTIVITY_COLOR_SPREAD_IN_MS));
  return static_cast<uint32_t>(target_ms * spread + 0.5f);
}

lv_color_t activity_color_at(uint32_t elapsed_ms) {
  uint32_t cycle_ms = elapsed_ms % ACTIVITY_COLOR_CYCLE_MS;
  float phase = eased_cycle_phase(cycle_ms, ACTIVITY_COLOR_CYCLE_MS);
  uint16_t hue =
      (ACTIVITY_COLOR_START_HUE +
       static_cast<uint16_t>(phase * 360.0f + 0.5f)) %
      360;
  return lv_color_hsv_to_rgb(
      hue, ACTIVITY_COLOR_SATURATION, ACTIVITY_COLOR_VALUE);
}

lv_opa_t activity_breath_opa_at(uint32_t elapsed_ms) {
  uint32_t half_cycle = ACTIVITY_BREATH_CYCLE_MS / 2;
  uint32_t phase =
      (elapsed_ms + half_cycle) % ACTIVITY_BREATH_CYCLE_MS;
  uint32_t wave =
      phase < half_cycle ? phase : ACTIVITY_BREATH_CYCLE_MS - phase;
  uint32_t amount = (wave * 255U) / half_cycle;
  return static_cast<lv_opa_t>(
      ACTIVITY_OPA_MIN +
      ((ACTIVITY_OPA_MAX - ACTIVITY_OPA_MIN) * amount) / 255U);
}

lv_color_t battery_fill_color(int pct) {
  if (pct <= 20) return BATTERY_RED;
  if (pct <= 50) return BATTERY_YELLOW;
  return BATTERY_GREEN;
}

void set_token_usage_mode(ClassicThemeState* state, bool enabled) {
  if (state->token_usage_mode == enabled) return;
  state->token_usage_mode = enabled;

  if (enabled) {
    lv_obj_add_flag(state->h5_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(state->token_content, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(state->top_title, "Token用量");
    lv_obj_set_style_text_font(
        state->top_title, state->resources.title_font, 0);
    lv_label_set_text(state->d7_heading, "7d 额度剩余");
    lv_obj_set_style_text_font(
        state->d7_heading, state->resources.title_font, 0);
  } else {
    lv_obj_clear_flag(state->h5_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(state->token_content, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(state->top_title, "剩余用量");
    lv_obj_set_style_text_font(
        state->top_title, state->resources.ui_font, 0);
    lv_label_set_text(state->d7_heading, "7d 剩余:");
    lv_obj_set_style_text_font(
        state->d7_heading, state->resources.ui_font, 0);
  }

  lv_obj_align(
      state->top_title, LV_ALIGN_TOP_MID, 0,
      enabled ? TOP_TITLE_TOKEN_Y : TOP_TITLE_DEFAULT_Y);
  lv_obj_align(state->d7_heading, LV_ALIGN_TOP_LEFT, 0, 0);
}

void update_activity(
    ClassicThemeState* state, int running_count) {
  int count = running_count;
  if (count < 0) count = 0;
  if (count > ACTIVITY_MAX_DOTS) count = ACTIVITY_MAX_DOTS;
  bool restart_anim = state->activity_visible_count == 0 && count > 0;
  uint32_t now = millis();
  if (restart_anim) {
    state->activity_anim_started_ms = now;
    state->activity_anim_last_ms = 0;
  }

  int total_w = count > 0
                    ? count * ACTIVITY_DOT_SIZE +
                          (count - 1) * ACTIVITY_DOT_GAP
                    : 0;
  int start_x = (CODEXMETER_SCREEN_W - total_w) / 2;

  for (int i = 0; i < ACTIVITY_MAX_DOTS; ++i) {
    if (i < count) {
      bool newly_visible =
          restart_anim || i >= state->activity_visible_count ||
          state->activity_dot_started_ms[i] == 0;
      lv_obj_set_pos(
          state->activity_dots[i],
          start_x + i * (ACTIVITY_DOT_SIZE + ACTIVITY_DOT_GAP),
          ACTIVITY_DOT_Y);
      if (newly_visible) {
        state->activity_dot_started_ms[i] = now;
        uint32_t elapsed = now - state->activity_anim_started_ms;
        lv_obj_set_style_bg_color(
            state->activity_dots[i], activity_color_at(elapsed), 0);
        lv_obj_set_style_bg_opa(
            state->activity_dots[i], LV_OPA_COVER, 0);
      }
      lv_obj_clear_flag(state->activity_dots[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(state->activity_dots[i], LV_OBJ_FLAG_HIDDEN);
      state->activity_dot_started_ms[i] = 0;
    }
  }
  state->activity_visible_count = count;
  if (count == 0) {
    state->activity_anim_started_ms = 0;
    state->activity_anim_last_ms = 0;
  }
}

void update_usage(
    ClassicThemeState* state, const DashboardViewModel& model) {
  set_token_usage_mode(state, model.token_usage_mode);
  lv_label_set_text(
      state->h5_pct, model.h5_percent_text);
  lv_label_set_text(state->h5_reset, model.h5_reset_text);
  lv_label_set_text(
      state->token_today_value,
      model.today_tokens_text);
  lv_label_set_text(
      state->token_7d_value,
      model.last_7d_tokens_text);
  lv_label_set_text(
      state->d7_pct, model.d7_percent_text);
  lv_label_set_text(state->d7_reset, model.d7_reset_text);
}

void update_battery(ClassicThemeState* state, int pct, bool charging) {
  if (pct < 0) {
    lv_label_set_text(state->top_battery, charging ? "USB" : "--%");
    lv_obj_set_width(state->top_battery_fill, charging ? 18 : 2);
    lv_obj_set_style_bg_color(
        state->top_battery_fill, charging ? BATTERY_GREEN : DIM, 0);
    return;
  }

  lv_label_set_text(
      state->top_battery, (String(pct) + "%").c_str());
  int width = 2 + (pct * 16 + 50) / 100;
  if (width < 2) width = 2;
  if (width > 18) width = 18;
  lv_obj_set_width(state->top_battery_fill, width);
  lv_obj_set_style_bg_color(
      state->top_battery_fill, battery_fill_color(pct), 0);
}

void tick_activity(ClassicThemeState* state, uint32_t now) {
  if (state->activity_visible_count <= 0) return;
  if (state->activity_anim_last_ms != 0 &&
      now - state->activity_anim_last_ms <
          ACTIVITY_ANIM_INTERVAL_MS) {
    return;
  }

  state->activity_anim_last_ms = now;
  uint32_t elapsed = now - state->activity_anim_started_ms;
  for (int i = 0; i < state->activity_visible_count; ++i) {
    uint32_t started = state->activity_dot_started_ms[i]
                           ? state->activity_dot_started_ms[i]
                           : state->activity_anim_started_ms;
    uint32_t age = now - started;
    uint32_t color_elapsed =
        elapsed + activity_dot_color_offset_ms(age, i);
    uint32_t breath_elapsed =
        elapsed + i * ACTIVITY_DOT_PHASE_MS;
    lv_obj_set_style_bg_color(
        state->activity_dots[i], activity_color_at(color_elapsed), 0);
    lv_obj_set_style_bg_opa(
        state->activity_dots[i],
        activity_breath_opa_at(breath_elapsed), 0);
  }
}

bool classic_mount(
    void* raw_state, lv_obj_t* parent,
    const ThemeResources& resources) {
  auto* state = static_cast<ClassicThemeState*>(raw_state);
  *state = ClassicThemeState{};
  state->resources = resources;

  state->root = lv_obj_create(parent);
  strip_obj(state->root);
  lv_obj_set_size(
      state->root, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H);
  lv_obj_set_pos(state->root, 0, 0);
  lv_obj_set_style_bg_color(state->root, BG, 0);
  lv_obj_set_style_bg_opa(state->root, LV_OPA_COVER, 0);

  lv_obj_t* top_logo = make_label(
      state->root, "Codex", &lv_font_montserrat_32, CODEX_BLUE);
  lv_obj_align(top_logo, LV_ALIGN_TOP_LEFT, 22, 16);

  state->top_title = make_label(
      state->root, "剩余用量", resources.ui_font, TEXT);
  lv_obj_align(
      state->top_title, LV_ALIGN_TOP_MID, 0, TOP_TITLE_DEFAULT_Y);

  lv_obj_t* battery_icon = make_battery_icon(state);
  lv_obj_align(battery_icon, LV_ALIGN_TOP_RIGHT, -18, 25);
  state->top_battery = make_label(
      state->root, "--%", &lv_font_montserrat_32, DIM);
  lv_obj_align(state->top_battery, LV_ALIGN_TOP_RIGHT, -58, 16);

  lv_obj_t* panel_h5 = make_panel(state, 92);
  state->h5_content = make_panel_content(panel_h5);
  lv_obj_t* label = make_label(
      state->h5_content, "5h 剩余:", resources.ui_font, DIM);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
  state->h5_pct = make_label(
      state->h5_content, "--%", resources.percent_font, TEXT);
  lv_obj_align(state->h5_pct, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  state->h5_reset = make_label(
      state->h5_content, "-- 后重置", resources.ui_font, DIM);
  lv_obj_align(state->h5_reset, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  state->token_content = make_panel_content(panel_h5);
  label = make_label(
      state->token_content, "今日用量", resources.title_font, DIM);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
  state->token_today_value = make_label(
      state->token_content, "--", resources.token_font, TEXT);
  lv_obj_align(
      state->token_today_value, LV_ALIGN_BOTTOM_LEFT, 0,
      TOKEN_VALUE_Y_OFFSET);

  label = make_label(
      state->token_content, "近7天", resources.title_font, DIM);
  lv_obj_set_pos(label, TOKEN_RIGHT_COLUMN_X, 0);
  state->token_7d_value = make_label(
      state->token_content, "--", resources.token_font, TEXT);
  lv_obj_align(
      state->token_7d_value, LV_ALIGN_BOTTOM_LEFT,
      TOKEN_RIGHT_COLUMN_X, TOKEN_VALUE_Y_OFFSET);

  lv_obj_t* divider = lv_obj_create(state->token_content);
  strip_obj(divider);
  lv_obj_set_size(divider, 1, 100);
  lv_obj_set_pos(divider, 188, 4);
  lv_obj_set_style_bg_color(divider, lv_color_hex(0x303747), 0);
  lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
  lv_obj_add_flag(state->token_content, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* panel_d7 = make_panel(state, 270);
  lv_obj_t* content_d7 = make_panel_content(panel_d7);
  state->d7_heading = make_label(
      content_d7, "7d 剩余:", resources.ui_font, DIM);
  lv_obj_align(state->d7_heading, LV_ALIGN_TOP_LEFT, 0, 0);
  state->d7_pct = make_label(
      content_d7, "--%", resources.percent_font, TEXT);
  lv_obj_align(state->d7_pct, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  state->d7_reset = make_label(
      content_d7, "-- 后重置", resources.ui_font, DIM);
  lv_obj_align(state->d7_reset, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  for (int i = 0; i < ACTIVITY_MAX_DOTS; ++i) {
    state->activity_dots[i] =
        make_activity_dot(state->root);
  }
  return true;
}

void classic_update(
    void* raw_state, const DashboardViewModel& model) {
  auto* state = static_cast<ClassicThemeState*>(raw_state);
  update_usage(state, model);
  update_activity(state, model.running_count);
  update_battery(
      state, model.battery_percent, model.charging);
}

void classic_tick(void* raw_state, uint32_t now_ms) {
  auto* state = static_cast<ClassicThemeState*>(raw_state);
  tick_activity(state, now_ms);
}

void classic_unmount(void* raw_state) {
  auto* state = static_cast<ClassicThemeState*>(raw_state);
  state->root = nullptr;
}

const ThemeDashboardOps CLASSIC_DASHBOARD = {
    sizeof(ClassicThemeState),
    classic_mount,
    classic_update,
    classic_tick,
    classic_unmount,
};

const ThemePack CLASSIC_THEME = {
    "classic",
    "Classic",
    1,
    CODEXMETER_BURN_IN_DRIFT_MAX_PX,
    CLASSIC_DASHBOARD,
    nullptr,
    nullptr,
};

static_assert(
    sizeof(ClassicThemeState) <= CODEXMETER_THEME_STATE_BYTES,
    "Classic theme state exceeds ThemeRuntime storage");

}  // namespace

const ThemePack& classic_theme_pack() {
  return CLASSIC_THEME;
}
