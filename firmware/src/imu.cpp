#include "imu.h"

#include <SensorQMI8658.hpp>
#include <Wire.h>
#include <math.h>

#include "config.h"
#include "device_log.h"

#define IMU_POLL_MS 100UL
#define IMU_STABLE_MS 350UL
#define IMU_TILT_THRESHOLD 0.50f
#define IMU_FILTER_ALPHA 0.35f

static SensorQMI8658 imu;
static bool imu_ready = false;
static bool auto_rotation = true;
static uint8_t current_rotation = 0;
static uint8_t candidate_rotation = 0;
static uint32_t candidate_since_ms = 0;
static uint32_t last_poll_ms = 0;
static bool has_sample = false;
static bool has_filtered_sample = false;
static float last_ax = 0.0f;
static float last_ay = 0.0f;
static float last_az = 0.0f;
static float filtered_ax = 0.0f;
static float filtered_ay = 0.0f;
static float filtered_az = 0.0f;

static uint8_t normalize_quadrant(uint8_t quadrant) {
  return quadrant & 0x03;
}

static uint8_t accel_to_rotation(float ax, float ay) {
  float abs_ax = fabsf(ax);
  float abs_ay = fabsf(ay);
  if (abs_ax < IMU_TILT_THRESHOLD && abs_ay < IMU_TILT_THRESHOLD) {
    return 255;
  }
  if (abs_ay > abs_ax) return (ay > 0.0f) ? 3 : 1;
  return (ax > 0.0f) ? 0 : 2;
}

static void update_sample(float ax, float ay, float az) {
  last_ax = ax;
  last_ay = ay;
  last_az = az;
  has_sample = true;

  if (!has_filtered_sample) {
    filtered_ax = ax;
    filtered_ay = ay;
    filtered_az = az;
    has_filtered_sample = true;
    return;
  }

  filtered_ax = filtered_ax * (1.0f - IMU_FILTER_ALPHA) + ax * IMU_FILTER_ALPHA;
  filtered_ay = filtered_ay * (1.0f - IMU_FILTER_ALPHA) + ay * IMU_FILTER_ALPHA;
  filtered_az = filtered_az * (1.0f - IMU_FILTER_ALPHA) + az * IMU_FILTER_ALPHA;
}

bool imu_init() {
  imu_ready = imu.begin(
      Wire, QMI8658_L_SLAVE_ADDRESS, CODEXMETER_I2C_SDA, CODEXMETER_I2C_SCL);
  if (!imu_ready) {
    device_logf("WARN", "QMI8658 init failed");
    return false;
  }

  imu.configAccelerometer(
      SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_LOWPOWER_21Hz,
      SensorQMI8658::LPF_MODE_3);
  imu.enableAccelerometer();
  candidate_rotation = current_rotation;
  device_logf("INFO", "QMI8658 ready auto_rotation=1");
  return true;
}

void imu_tick() {
  if (!imu_ready) return;

  uint32_t now = millis();
  if (now - last_poll_ms < IMU_POLL_MS) return;
  last_poll_ms = now;

  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  if (!imu.getAccelerometer(ax, ay, az)) return;
  update_sample(ax, ay, az);

  if (!auto_rotation) return;

  uint8_t target = accel_to_rotation(filtered_ax, filtered_ay);
  if (target == 255 || target == current_rotation) {
    candidate_rotation = current_rotation;
    candidate_since_ms = now;
    return;
  }

  if (target != candidate_rotation) {
    candidate_rotation = target;
    candidate_since_ms = now;
    return;
  }

  if (now - candidate_since_ms < IMU_STABLE_MS) return;

  current_rotation = target;
  device_logf("INFO", "orientation rotation=%u auto=1", current_rotation);
}

bool imu_available() {
  return imu_ready;
}

bool imu_auto_rotation_enabled() {
  return auto_rotation;
}

void imu_set_auto_rotation(bool enabled) {
  auto_rotation = enabled;
  candidate_rotation = current_rotation;
  candidate_since_ms = millis();
  device_logf("INFO", "orientation auto=%d rotation=%u", enabled ? 1 : 0,
              current_rotation);
}

bool imu_set_rotation_quadrant(uint8_t quadrant) {
  if (quadrant > 3) return false;
  auto_rotation = false;
  current_rotation = normalize_quadrant(quadrant);
  candidate_rotation = current_rotation;
  candidate_since_ms = millis();
  device_logf("INFO", "orientation rotation=%u auto=0", current_rotation);
  return true;
}

uint8_t imu_rotation_quadrant() {
  return current_rotation;
}

bool imu_last_sample(float* ax, float* ay, float* az) {
  if (!has_sample) return false;
  if (ax) *ax = last_ax;
  if (ay) *ay = last_ay;
  if (az) *az = last_az;
  return true;
}

const char* imu_rotation_label(uint8_t quadrant) {
  switch (normalize_quadrant(quadrant)) {
    case 1:
      return "90";
    case 2:
      return "180";
    case 3:
      return "270";
    default:
      return "0";
  }
}

void imu_print_status(Stream& out) {
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  bool sample = imu_last_sample(&ax, &ay, &az);
  out.printf(
      "IMU ready=%d auto=%d rotation=%u deg=%s sample=%d ax=%.3f ay=%.3f az=%.3f\n",
      imu_ready ? 1 : 0, auto_rotation ? 1 : 0, current_rotation,
      imu_rotation_label(current_rotation), sample ? 1 : 0, ax, ay, az);
}
