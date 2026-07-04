#pragma once

#include <Arduino.h>

bool imu_init();
void imu_tick();

bool imu_available();
bool imu_auto_rotation_enabled();
void imu_set_auto_rotation(bool enabled);
bool imu_set_rotation_quadrant(uint8_t quadrant);
uint8_t imu_rotation_quadrant();

bool imu_last_sample(float* ax, float* ay, float* az);
const char* imu_rotation_label(uint8_t quadrant);
void imu_print_status(Stream& out);
