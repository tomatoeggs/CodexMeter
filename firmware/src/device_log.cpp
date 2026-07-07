#include "device_log.h"

#include <esp_heap_caps.h>
#include <stdarg.h>
#include <stdio.h>

#define DEVICE_LOG_CAPACITY 48
#define DEVICE_LOG_LEVEL_SIZE 8
#define DEVICE_LOG_MESSAGE_SIZE 144

struct DeviceLogEntry {
  uint32_t seq = 0;
  uint32_t ms = 0;
  char level[DEVICE_LOG_LEVEL_SIZE] = "";
  char message[DEVICE_LOG_MESSAGE_SIZE] = "";
};

static DeviceLogEntry entries[DEVICE_LOG_CAPACITY];
static DeviceLogEntry snapshot[DEVICE_LOG_CAPACITY];
static uint32_t next_seq = 1;
static size_t head = 0;
static size_t count = 0;
static portMUX_TYPE log_mux = portMUX_INITIALIZER_UNLOCKED;

static void copy_clean(char* dst, size_t dst_size, const char* src) {
  if (dst_size == 0) return;
  if (!src) src = "";

  size_t pos = 0;
  while (*src && pos < dst_size - 1) {
    char c = *src++;
    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    dst[pos++] = c;
  }
  dst[pos] = '\0';
}

void device_logf(const char* level, const char* fmt, ...) {
  char message[DEVICE_LOG_MESSAGE_SIZE];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  uint32_t seq = 0;
  uint32_t ms = 0;
  char level_copy[DEVICE_LOG_LEVEL_SIZE];
  char message_copy[DEVICE_LOG_MESSAGE_SIZE];

  portENTER_CRITICAL(&log_mux);
  DeviceLogEntry& entry = entries[head];
  entry.seq = next_seq++;
  entry.ms = millis();
  copy_clean(entry.level, sizeof(entry.level), level);
  copy_clean(entry.message, sizeof(entry.message), message);
  seq = entry.seq;
  ms = entry.ms;
  strlcpy(level_copy, entry.level, sizeof(level_copy));
  strlcpy(message_copy, entry.message, sizeof(message_copy));
  head = (head + 1) % DEVICE_LOG_CAPACITY;
  if (count < DEVICE_LOG_CAPACITY) count++;
  portEXIT_CRITICAL(&log_mux);

  Serial.printf(
      "LOG %lu %lu %s %s\n", (unsigned long)seq, (unsigned long)ms, level_copy,
      message_copy);
}

void device_log_heap(const char* phase) {
  if (!phase || !phase[0]) phase = "-";

  uint32_t internal =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  uint32_t internal_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  uint32_t psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  uint32_t psram_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  device_logf(
      "INFO", "heap %s i=%lu il=%lu p=%lu pl=%lu", phase,
      (unsigned long)internal, (unsigned long)internal_largest,
      (unsigned long)psram, (unsigned long)psram_largest);
}

void device_log_dump(Stream& out, size_t limit) {
  size_t actual = 0;

  portENTER_CRITICAL(&log_mux);
  actual = count;
  if (limit > 0 && limit < actual) actual = limit;
  size_t oldest = (head + DEVICE_LOG_CAPACITY - count) % DEVICE_LOG_CAPACITY;
  size_t skip = count - actual;
  for (size_t i = 0; i < actual; i++) {
    size_t idx = (oldest + skip + i) % DEVICE_LOG_CAPACITY;
    snapshot[i] = entries[idx];
  }
  portEXIT_CRITICAL(&log_mux);

  out.printf("LOGS_START %u\n", (unsigned int)actual);
  for (size_t i = 0; i < actual; i++) {
    const DeviceLogEntry& entry = snapshot[i];
    out.printf(
        "LOG %lu %lu %s %s\n", (unsigned long)entry.seq, (unsigned long)entry.ms,
        entry.level, entry.message);
  }
  out.println("LOGS_END");
}

void device_log_clear() {
  portENTER_CRITICAL(&log_mux);
  head = 0;
  count = 0;
  portEXIT_CRITICAL(&log_mux);
}

size_t device_log_count() {
  portENTER_CRITICAL(&log_mux);
  size_t current = count;
  portEXIT_CRITICAL(&log_mux);
  return current;
}
