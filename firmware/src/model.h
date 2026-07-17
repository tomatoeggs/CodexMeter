#pragma once

#include <Arduino.h>

struct UsageModel {
  bool valid = false;
  int h5_remaining = -1;
  long h5_reset = 0;
  int d7_remaining = -1;
  long d7_reset = 0;
  bool has_today_tokens = false;
  uint64_t today_tokens = 0;
  bool has_last_7d_tokens = false;
  uint64_t last_7d_tokens = 0;
  char status[24] = "waiting";
  long updated_at = 0;
  uint32_t received_ms = 0;
};

struct AlertModel {
  bool valid = false;
  bool has_running_count = false;
  int running_count = 0;
  char id[32] = "";
  char title[48] = "任务完成！";
  char body[240] = "";
  long received_at = 0;
};

struct ActivityModel {
  bool valid = false;
  int running_count = 0;
  long updated_at = 0;
};

struct ControlModel {
  bool valid = false;
  bool screen_on = true;
  char command[16] = "";
  char reason[32] = "";
  long updated_at = 0;
};

enum PayloadKind {
  PAYLOAD_NONE,
  PAYLOAD_USAGE,
  PAYLOAD_ALERT,
  PAYLOAD_ACTIVITY,
  PAYLOAD_CONTROL,
};

PayloadKind parse_payload(
    const char* json,
    UsageModel* usage,
    AlertModel* alert,
    ActivityModel* activity,
    ControlModel* control);
void usage_apply_demo(UsageModel* usage);
void usage_apply_token_demo(UsageModel* usage);
void alert_apply_demo(AlertModel* alert);
void activity_apply_demo(ActivityModel* activity, int running_count);
