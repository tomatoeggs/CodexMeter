#pragma once

#include <Arduino.h>
#include <stddef.h>

#include "theme.h"

class ThemeRuntime {
 public:
  bool begin(lv_obj_t* host, const ThemeResources& resources);
  bool switch_to(const char* id);
  bool next(int direction = 1);
  void update(const DashboardViewModel& model);
  void tick(uint32_t now_ms);
  void shutdown();

  const char* current_id() const;
  const char* current_name() const;
  uint8_t drift_margin_px() const;
 bool ready() const { return active_ != nullptr; }

 private:
  bool mount_pack(const ThemePack* pack);

  lv_obj_t* host_ = nullptr;
  ThemeResources resources_{};
  const ThemePack* active_ = nullptr;
  DashboardViewModel model_{};
  bool has_model_ = false;
  alignas(max_align_t)
      uint8_t state_[CODEXMETER_THEME_STATE_BYTES]{};
};
