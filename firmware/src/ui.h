#pragma once

#include "model.h"

void ui_init();
void ui_update_usage(const UsageModel& usage);
void ui_show_alert(const AlertModel& alert);
void ui_set_battery(int pct, bool charging);
void ui_tick();
void ui_dismiss_alert();
bool ui_alert_visible();
