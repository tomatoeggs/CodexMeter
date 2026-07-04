#pragma once

#include <Arduino.h>

void device_logf(const char* level, const char* fmt, ...);
void device_log_dump(Stream& out, size_t limit = 0);
void device_log_clear();
size_t device_log_count();
