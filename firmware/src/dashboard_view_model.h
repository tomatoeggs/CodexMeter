#pragma once

#include <Arduino.h>

#include "model.h"

enum class DashboardDataState : uint8_t {
  Waiting = 0,
  Ready,
  Stale,
  Error,
};

struct DashboardViewModel {
  DashboardDataState data_state = DashboardDataState::Waiting;
  bool usage_valid = false;
  bool token_usage_mode = false;

  int h5_remaining = -1;
  int d7_remaining = -1;
  int32_t h5_reset_seconds = -1;
  int32_t d7_reset_seconds = -1;

  bool has_today_tokens = false;
  uint64_t today_tokens = 0;
  bool has_last_7d_tokens = false;
  uint64_t last_7d_tokens = 0;
  bool has_today_share = false;
  uint16_t today_share_permille = 0;

  int running_count = 0;
  int battery_percent = -1;
  bool charging = false;

  char h5_percent_text[8] = "--%";
  char d7_percent_text[8] = "--%";
  char h5_reset_text[24] = "-- 后重置";
  char d7_reset_text[24] = "-- 后重置";
  char today_tokens_text[24] = "--";
  char last_7d_tokens_text[24] = "--";
};

DashboardViewModel build_dashboard_view_model(
    const UsageModel& usage, const ActivityModel& activity,
    int battery_percent, bool charging, uint32_t now_ms);
