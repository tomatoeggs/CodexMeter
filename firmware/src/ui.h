#pragma once

#include "model.h"

struct UiSystemHooks {
  void (*apply_brightness)(int percent) = nullptr;
};

void ui_init(const UiSystemHooks& hooks);
void ui_update_usage(const UsageModel& usage);
void ui_update_activity(const ActivityModel& activity);
void ui_show_alert(const AlertModel& alert);
void ui_set_battery(int pct, bool charging);
void ui_show_brightness(int pct);
void ui_tick();

void ui_set_display_active(bool active);
void ui_middle_short_press();
void ui_settings_move(int direction);
bool ui_settings_visible();
void ui_close_settings();

void ui_dismiss_alert();
bool ui_alert_visible();

bool ui_set_theme(const char* id, bool persist);
bool ui_next_theme(int direction, bool persist);
const char* ui_theme_id();
const char* ui_theme_name();
void ui_set_auto_theme(bool enabled);
bool ui_auto_theme_enabled();
void ui_set_auto_theme_interval(uint16_t minutes);
uint16_t ui_auto_theme_interval();
void ui_set_volume(int percent);
int ui_volume();
