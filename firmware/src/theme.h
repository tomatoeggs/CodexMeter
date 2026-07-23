#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <stddef.h>

#include "dashboard_view_model.h"
#include "model.h"

constexpr size_t CODEXMETER_THEME_STATE_BYTES = 1024;

struct ThemeResources {
  const lv_font_t* ui_font = nullptr;
  const lv_font_t* percent_font = nullptr;
  const lv_font_t* title_font = nullptr;
  const lv_font_t* body_font = nullptr;
  const lv_font_t* token_font = nullptr;
};

struct StartupViewModel {
  char product_name[32] = "CodexMeter";
  char theme_name[32] = "";
};

struct CompletionViewModel {
  AlertModel alert;
  int running_count = 0;
};

struct ThemeDashboardOps {
  size_t state_size;
  bool (*mount)(
      void* state, lv_obj_t* parent, const ThemeResources& resources);
  void (*update)(
      void* state, const DashboardViewModel& model);
  void (*tick)(void* state, uint32_t now_ms);
  void (*unmount)(void* state);
};

struct ThemeStartupOps {
  size_t state_size;
  bool (*mount)(
      void* state, lv_obj_t* parent, const ThemeResources& resources);
  void (*update)(void* state, const StartupViewModel& model);
  void (*tick)(void* state, uint32_t now_ms);
  void (*unmount)(void* state);
};

struct ThemeCompletionOps {
  size_t state_size;
  bool (*mount)(
      void* state, lv_obj_t* parent, const ThemeResources& resources);
  void (*update)(void* state, const CompletionViewModel& model);
  void (*tick)(void* state, uint32_t now_ms);
  void (*unmount)(void* state);
};

struct ThemePack {
  const char* id;
  const char* display_name;
  uint16_t manifest_version;
  uint8_t drift_margin_px;
  ThemeDashboardOps dashboard;
  const ThemeStartupOps* startup;
  const ThemeCompletionOps* completion;
};
