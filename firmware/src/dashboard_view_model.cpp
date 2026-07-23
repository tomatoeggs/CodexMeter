#include "dashboard_view_model.h"

#include <stdio.h>
#include <string.h>

namespace {

DashboardDataState data_state_for(const UsageModel& usage) {
  if (!usage.valid || strcmp(usage.status, "waiting") == 0) {
    return DashboardDataState::Waiting;
  }
  if (strcmp(usage.status, "stale") == 0) {
    return DashboardDataState::Stale;
  }
  if (strcmp(usage.status, "ok") == 0) {
    return DashboardDataState::Ready;
  }
  return DashboardDataState::Error;
}

int normalized_percent(int value) {
  if (value < 0) return -1;
  if (value > 100) return 100;
  return value;
}

int32_t seconds_until_reset(
    long reset_epoch, const UsageModel& usage, uint32_t now_ms) {
  if (reset_epoch <= 0 || usage.updated_at <= 0) return -1;
  long current_epoch =
      usage.updated_at +
      static_cast<long>((now_ms - usage.received_ms) / 1000UL);
  long seconds = reset_epoch - current_epoch;
  if (seconds <= 0) return 0;
  if (seconds > INT32_MAX) return INT32_MAX;
  return static_cast<int32_t>(seconds);
}

void format_percent(char* output, size_t size, int value) {
  if (value < 0) {
    strlcpy(output, "--%", size);
    return;
  }
  snprintf(output, size, "%d%%", value);
}

void format_h5_reset(
    char* output, size_t size, int32_t seconds) {
  if (seconds < 0) {
    strlcpy(output, "-- 后重置", size);
    return;
  }
  long minutes = (seconds + 59L) / 60L;
  snprintf(
      output, size, "%02ld:%02ld 后重置",
      minutes / 60L, minutes % 60L);
}

void format_d7_reset(
    char* output, size_t size, int32_t seconds) {
  if (seconds < 0) {
    strlcpy(output, "-- 后重置", size);
    return;
  }
  long days =
      (seconds + 24L * 60L * 60L - 1L) /
      (24L * 60L * 60L);
  snprintf(output, size, "%ldd 后重置", days);
}

void format_compact_tokens(
    char* output, size_t size, bool valid, uint64_t tokens) {
  if (!valid) {
    strlcpy(output, "--", size);
    return;
  }

  static const char* UNITS[] = {"", "K", "M", "B", "T"};
  double scaled = static_cast<double>(tokens);
  int unit = 0;
  while (scaled >= 1000.0 && unit < 4) {
    scaled /= 1000.0;
    unit++;
  }

  if (unit == 0) {
    snprintf(
        output, size, "%llu",
        static_cast<unsigned long long>(tokens));
  } else if (scaled >= 100.0) {
    snprintf(output, size, "%.0f%s", scaled, UNITS[unit]);
  } else {
    snprintf(output, size, "%.1f%s", scaled, UNITS[unit]);
    size_t len = strlen(output);
    if (len >= 3 &&
        output[len - 3] == '.' && output[len - 2] == '0') {
      output[len - 3] = output[len - 1];
      output[len - 2] = '\0';
    }
  }
}

}  // namespace

DashboardViewModel build_dashboard_view_model(
    const UsageModel& usage, const ActivityModel& activity,
    int battery_percent, bool charging, uint32_t now_ms) {
  DashboardViewModel model;
  model.data_state = data_state_for(usage);
  model.usage_valid = usage.valid;
  model.h5_remaining = normalized_percent(usage.h5_remaining);
  model.d7_remaining = normalized_percent(usage.d7_remaining);
  model.token_usage_mode =
      model.h5_remaining < 0 && model.d7_remaining >= 0;
  model.h5_reset_seconds =
      seconds_until_reset(usage.h5_reset, usage, now_ms);
  model.d7_reset_seconds =
      seconds_until_reset(usage.d7_reset, usage, now_ms);

  model.has_today_tokens = usage.has_today_tokens;
  model.today_tokens = usage.today_tokens;
  model.has_last_7d_tokens = usage.has_last_7d_tokens;
  model.last_7d_tokens = usage.last_7d_tokens;
  if (model.has_today_tokens && model.has_last_7d_tokens &&
      model.last_7d_tokens > 0) {
    double ratio =
        static_cast<double>(model.today_tokens) /
        static_cast<double>(model.last_7d_tokens);
    uint32_t share =
        ratio >= 1.0
            ? 1000U
            : static_cast<uint32_t>(ratio * 1000.0 + 0.5);
    model.has_today_share = true;
    model.today_share_permille = static_cast<uint16_t>(share);
  }

  model.running_count =
      activity.valid && activity.running_count > 0
          ? activity.running_count
          : 0;
  model.battery_percent =
      battery_percent < 0
          ? -1
          : (battery_percent > 100 ? 100 : battery_percent);
  model.charging = charging;

  format_percent(
      model.h5_percent_text, sizeof(model.h5_percent_text),
      model.h5_remaining);
  format_percent(
      model.d7_percent_text, sizeof(model.d7_percent_text),
      model.d7_remaining);
  format_h5_reset(
      model.h5_reset_text, sizeof(model.h5_reset_text),
      model.h5_reset_seconds);
  format_d7_reset(
      model.d7_reset_text, sizeof(model.d7_reset_text),
      model.d7_reset_seconds);
  format_compact_tokens(
      model.today_tokens_text, sizeof(model.today_tokens_text),
      model.has_today_tokens, model.today_tokens);
  format_compact_tokens(
      model.last_7d_tokens_text,
      sizeof(model.last_7d_tokens_text),
      model.has_last_7d_tokens, model.last_7d_tokens);
  return model;
}
