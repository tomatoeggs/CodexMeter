#include "device_settings.h"

#include <Preferences.h>
#include <stddef.h>
#include <string.h>

#include "config.h"
#include "device_log.h"

namespace {

constexpr uint32_t SETTINGS_MAGIC = 0x434D5354UL;  // CMST
constexpr uint16_t SETTINGS_VERSION = 1;
constexpr const char* SETTINGS_NAMESPACE = "codexmeter";
constexpr const char* SETTINGS_KEY = "settings";

#pragma pack(push, 1)
struct PersistedSettingsV1 {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  char theme_id[24];
  uint8_t auto_theme_enabled;
  uint16_t auto_theme_interval_minutes;
  uint8_t brightness_percent;
  uint8_t volume_percent;
  uint32_t crc32;
};
#pragma pack(pop)

Preferences preferences;
DeviceSettings settings;
bool ready = false;
bool storage_available = false;
bool dirty = false;
uint32_t dirty_since_ms = 0;

uint32_t crc32_bytes(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      uint32_t mask = -(crc & 1UL);
      crc = (crc >> 1) ^ (0xEDB88320UL & mask);
    }
  }
  return ~crc;
}

uint8_t clamp_percent(uint8_t value) {
  return value > 100 ? 100 : value;
}

uint16_t clamp_interval(uint16_t minutes) {
  if (minutes < CODEXMETER_THEME_AUTO_MIN_MINUTES) {
    return CODEXMETER_THEME_AUTO_MIN_MINUTES;
  }
  if (minutes > CODEXMETER_THEME_AUTO_MAX_MINUTES) {
    return CODEXMETER_THEME_AUTO_MAX_MINUTES;
  }
  return minutes;
}

bool valid_theme_id(const char* value) {
  if (!value || !value[0]) return false;
  size_t len = strnlen(value, sizeof(settings.theme_id));
  if (len == sizeof(settings.theme_id)) return false;
  for (size_t i = 0; i < len; ++i) {
    char ch = value[i];
    bool valid =
        (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' ||
        ch == '-';
    if (!valid) return false;
  }
  return true;
}

void reset_defaults() {
  settings = DeviceSettings{};
  settings.brightness_percent = CODEXMETER_BRIGHTNESS_DEFAULT;
  settings.auto_theme_interval_minutes =
      CODEXMETER_THEME_AUTO_DEFAULT_MINUTES;
  settings.volume_percent = CODEXMETER_VOLUME_DEFAULT;
}

bool decode_record(const PersistedSettingsV1& record) {
  if (record.magic != SETTINGS_MAGIC || record.version != SETTINGS_VERSION ||
      record.size != sizeof(record)) {
    return false;
  }
  uint32_t expected =
      crc32_bytes(reinterpret_cast<const uint8_t*>(&record),
                  offsetof(PersistedSettingsV1, crc32));
  if (expected != record.crc32 || !valid_theme_id(record.theme_id)) {
    return false;
  }

  strlcpy(settings.theme_id, record.theme_id, sizeof(settings.theme_id));
  settings.auto_theme_enabled = record.auto_theme_enabled != 0;
  settings.auto_theme_interval_minutes =
      clamp_interval(record.auto_theme_interval_minutes);
  settings.brightness_percent = clamp_percent(record.brightness_percent);
  if (settings.brightness_percent < CODEXMETER_BRIGHTNESS_MIN) {
    settings.brightness_percent = CODEXMETER_BRIGHTNESS_MIN;
  }
  settings.volume_percent = clamp_percent(record.volume_percent);
  return true;
}

PersistedSettingsV1 encode_record() {
  PersistedSettingsV1 record{};
  record.magic = SETTINGS_MAGIC;
  record.version = SETTINGS_VERSION;
  record.size = sizeof(record);
  strlcpy(record.theme_id, settings.theme_id, sizeof(record.theme_id));
  record.auto_theme_enabled = settings.auto_theme_enabled ? 1 : 0;
  record.auto_theme_interval_minutes = settings.auto_theme_interval_minutes;
  record.brightness_percent = settings.brightness_percent;
  record.volume_percent = settings.volume_percent;
  record.crc32 =
      crc32_bytes(reinterpret_cast<const uint8_t*>(&record),
                  offsetof(PersistedSettingsV1, crc32));
  return record;
}

void mark_dirty() {
  if (!ready) return;
  dirty = true;
  dirty_since_ms = millis();
}

}  // namespace

void device_settings_init() {
  if (ready) return;
  reset_defaults();

  if (!preferences.begin(SETTINGS_NAMESPACE, false)) {
    device_logf("WARN", "settings NVS unavailable; using defaults");
    ready = true;
    return;
  }
  storage_available = true;

  PersistedSettingsV1 record{};
  size_t stored = preferences.getBytesLength(SETTINGS_KEY);
  bool loaded =
      stored == sizeof(record) &&
      preferences.getBytes(SETTINGS_KEY, &record, sizeof(record)) ==
          sizeof(record) &&
      decode_record(record);
  if (loaded) {
    device_logf(
        "INFO", "settings loaded theme=%s auto=%d every=%umin bright=%u volume=%u",
        settings.theme_id, settings.auto_theme_enabled ? 1 : 0,
        settings.auto_theme_interval_minutes, settings.brightness_percent,
        settings.volume_percent);
  } else if (stored > 0) {
    device_logf("WARN", "settings invalid; using defaults");
  } else {
    device_logf("INFO", "settings defaults");
  }
  ready = true;
}

const DeviceSettings& device_settings_get() {
  if (!ready) device_settings_init();
  return settings;
}

void device_settings_set_theme(const char* theme_id) {
  if (!valid_theme_id(theme_id) ||
      strcmp(settings.theme_id, theme_id) == 0) {
    return;
  }
  strlcpy(settings.theme_id, theme_id, sizeof(settings.theme_id));
  mark_dirty();
}

void device_settings_set_auto_theme(bool enabled) {
  if (settings.auto_theme_enabled == enabled) return;
  settings.auto_theme_enabled = enabled;
  mark_dirty();
}

void device_settings_set_auto_theme_interval(uint16_t minutes) {
  minutes = clamp_interval(minutes);
  if (settings.auto_theme_interval_minutes == minutes) return;
  settings.auto_theme_interval_minutes = minutes;
  mark_dirty();
}

void device_settings_set_brightness(uint8_t percent) {
  percent = clamp_percent(percent);
  if (percent < CODEXMETER_BRIGHTNESS_MIN) {
    percent = CODEXMETER_BRIGHTNESS_MIN;
  }
  if (settings.brightness_percent == percent) return;
  settings.brightness_percent = percent;
  mark_dirty();
}

void device_settings_set_volume(uint8_t percent) {
  percent = clamp_percent(percent);
  if (settings.volume_percent == percent) return;
  settings.volume_percent = percent;
  mark_dirty();
}

void device_settings_flush() {
  if (!ready || !dirty) return;
  if (!storage_available) {
    dirty = false;
    device_logf("WARN", "settings not saved; NVS unavailable");
    return;
  }
  PersistedSettingsV1 record = encode_record();
  size_t written =
      preferences.putBytes(SETTINGS_KEY, &record, sizeof(record));
  if (written == sizeof(record)) {
    dirty = false;
    device_logf(
        "INFO", "settings saved theme=%s auto=%d every=%umin bright=%u volume=%u",
        settings.theme_id, settings.auto_theme_enabled ? 1 : 0,
        settings.auto_theme_interval_minutes, settings.brightness_percent,
        settings.volume_percent);
  } else {
    dirty_since_ms = millis();
    device_logf("WARN", "settings save failed bytes=%u", (unsigned int)written);
  }
}

void device_settings_tick() {
  if (!dirty) return;
  if (millis() - dirty_since_ms < CODEXMETER_SETTINGS_WRITE_DELAY_MS) return;
  device_settings_flush();
}
