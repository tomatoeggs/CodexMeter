#include "model.h"

#include <ArduinoJson.h>

static void copy_text(char* dst, size_t dst_size, const char* src) {
  if (!src) src = "";
  strlcpy(dst, src, dst_size);
}

PayloadKind parse_payload(
    const char* json,
    UsageModel* usage,
    AlertModel* alert,
    ActivityModel* activity) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.printf("JSON parse error: %s\n", err.c_str());
    return PAYLOAD_NONE;
  }

  const char* kind = doc["k"] | "";
  if (strcmp(kind, "usage") == 0) {
    usage->valid = true;
    usage->h5_remaining = doc["h5"] | -1;
    usage->h5_reset = doc["h5r"] | 0L;
    usage->d7_remaining = doc["d7"] | -1;
    usage->d7_reset = doc["d7r"] | 0L;
    usage->updated_at = doc["t"] | 0L;
    usage->received_ms = millis();
    copy_text(usage->status, sizeof(usage->status), doc["st"] | "ok");
    return PAYLOAD_USAGE;
  }

  if (strcmp(kind, "alert") == 0) {
    alert->valid = true;
    copy_text(alert->id, sizeof(alert->id), doc["id"] | "");
    copy_text(alert->title, sizeof(alert->title), doc["title"] | "任务完成！");
    copy_text(alert->body, sizeof(alert->body), doc["body"] | "Codex 任务已完成");
    alert->received_at = doc["t"] | 0L;
    return PAYLOAD_ALERT;
  }

  if (strcmp(kind, "activity") == 0) {
    activity->valid = true;
    activity->running_count = doc["run"] | 0;
    if (activity->running_count < 0) activity->running_count = 0;
    activity->updated_at = doc["t"] | 0L;
    return PAYLOAD_ACTIVITY;
  }

  Serial.printf("Unknown payload kind: %s\n", kind);
  return PAYLOAD_NONE;
}

void usage_apply_demo(UsageModel* usage) {
  long now = time(nullptr);
  if (now < 1000) now = 1783070000;
  usage->valid = true;
  usage->h5_remaining = 72;
  usage->h5_reset = now + 73 * 60;
  usage->d7_remaining = 84;
  usage->d7_reset = now + 3 * 24 * 60 * 60;
  usage->updated_at = now;
  usage->received_ms = millis();
  copy_text(usage->status, sizeof(usage->status), "ok");
}

void alert_apply_demo(AlertModel* alert) {
  long now = time(nullptr);
  if (now < 1000) now = 1783070000;
  alert->valid = true;
  copy_text(alert->id, sizeof(alert->id), "demo");
  copy_text(alert->title, sizeof(alert->title), "任务完成！");
  copy_text(alert->body, sizeof(alert->body), "Codex 已完成一个测试任务");
  alert->received_at = now;
}

void activity_apply_demo(ActivityModel* activity, int running_count) {
  long now = time(nullptr);
  if (now < 1000) now = 1783070000;
  activity->valid = true;
  activity->running_count = running_count < 0 ? 0 : running_count;
  activity->updated_at = now;
}
