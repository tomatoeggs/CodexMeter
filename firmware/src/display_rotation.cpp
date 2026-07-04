#include "display_rotation.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "device_log.h"

static uint16_t* rotation_buf = nullptr;
static size_t rotation_buf_pixels = 0;

static uint8_t normalize_rotation(uint8_t rotation) {
  return rotation & 0x03;
}

bool display_rotation_init() {
#ifndef BOARD_HAS_PSRAM
  device_logf("WARN", "display rotation disabled no_psram");
  return false;
#else
  rotation_buf_pixels = (size_t)CODEXMETER_SCREEN_W * (size_t)CODEXMETER_SCREEN_H;
  rotation_buf =
      (uint16_t*)heap_caps_malloc(rotation_buf_pixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  if (!rotation_buf) {
    rotation_buf_pixels = 0;
    device_logf("WARN", "display rotation buffer alloc failed");
    return false;
  }
  device_logf("INFO", "display rotation buffer ready pixels=%u",
              (unsigned int)rotation_buf_pixels);
  return true;
#endif
}

bool display_rotation_available() {
  return rotation_buf != nullptr;
}

static bool rotate_rect(const uint16_t* src, int32_t sx, int32_t sy, int32_t w,
                        int32_t h, uint8_t rotation, int32_t* dx, int32_t* dy,
                        int32_t* dw, int32_t* dh) {
  size_t pixels = (size_t)w * (size_t)h;
  if (!rotation_buf || pixels > rotation_buf_pixels) return false;

  const int32_t side = CODEXMETER_SCREEN_W;
  switch (normalize_rotation(rotation)) {
    case 1:
      *dw = h;
      *dh = w;
      *dx = side - sy - h;
      *dy = sx;
      for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
          rotation_buf[x * h + (h - 1 - y)] = src[y * w + x];
        }
      }
      return true;

    case 2:
      *dw = w;
      *dh = h;
      *dx = side - sx - w;
      *dy = side - sy - h;
      for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
          rotation_buf[(h - 1 - y) * w + (w - 1 - x)] = src[y * w + x];
        }
      }
      return true;

    case 3:
      *dw = h;
      *dh = w;
      *dx = sy;
      *dy = side - sx - w;
      for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
          rotation_buf[(w - 1 - x) * h + y] = src[y * w + x];
        }
      }
      return true;

    default:
      *dx = sx;
      *dy = sy;
      *dw = w;
      *dh = h;
      return false;
  }
}

void display_rotation_draw(Arduino_CO5300* gfx, int32_t x, int32_t y, int32_t w,
                           int32_t h, const uint16_t* pixels, uint8_t rotation) {
  if (!gfx) return;
  rotation = normalize_rotation(rotation);
  if (rotation == 0 || !rotation_buf) {
    gfx->draw16bitRGBBitmap(x, y, (uint16_t*)pixels, w, h);
    return;
  }

  int32_t dx = x;
  int32_t dy = y;
  int32_t dw = w;
  int32_t dh = h;
  if (!rotate_rect(pixels, x, y, w, h, rotation, &dx, &dy, &dw, &dh)) {
    gfx->draw16bitRGBBitmap(x, y, (uint16_t*)pixels, w, h);
    return;
  }
  gfx->draw16bitRGBBitmap(dx, dy, rotation_buf, dw, dh);
}

const uint16_t* display_rotation_frame(const uint16_t* pixels, uint8_t rotation) {
  rotation = normalize_rotation(rotation);
  if (rotation == 0 || !rotation_buf) return pixels;

  int32_t dx = 0;
  int32_t dy = 0;
  int32_t dw = CODEXMETER_SCREEN_W;
  int32_t dh = CODEXMETER_SCREEN_H;
  if (!rotate_rect(pixels, 0, 0, CODEXMETER_SCREEN_W, CODEXMETER_SCREEN_H, rotation, &dx,
                   &dy, &dw, &dh)) {
    return pixels;
  }
  return rotation_buf;
}
