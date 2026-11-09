#pragma once

#include <stdbool.h>
#include "app_types.h"

// Малювання в RAM-буфер u8g2 не чіпає шину; шину чіпає лише oled_send().
// Тому м'ютекс I2C потрібен тільки навколо oled_send() і oled_init().
bool oled_init(void);
void oled_render(const display_snapshot_t *s);
void oled_render_text(const char *line1, const char *line2);
void oled_send(void);
