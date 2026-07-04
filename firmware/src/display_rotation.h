#pragma once

#include <Arduino_GFX_Library.h>
#include <stdint.h>

bool display_rotation_init();
bool display_rotation_available();
void display_rotation_draw(Arduino_CO5300* gfx, int32_t x, int32_t y, int32_t w,
                           int32_t h, const uint16_t* pixels, uint8_t rotation);
const uint16_t* display_rotation_frame(const uint16_t* pixels, uint8_t rotation);
