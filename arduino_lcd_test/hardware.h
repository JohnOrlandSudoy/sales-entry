#pragma once

#include <Arduino_GFX_Library.h>

bool board_hardware_init(void);
Arduino_GFX *board_get_gfx(void);
void board_display_flush(void);
void board_touch_wire_begin(void);
