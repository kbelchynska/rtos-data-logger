#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Tiny RTC DS1307 (I2C 0x68). Час зберігається як UTC.
// Виклики НЕ беруть м'ютекс I2C самі: це робить задача, яка їх викликає.
bool ds1307_init(void);
bool ds1307_read(time_t *out_unix);
bool ds1307_write(time_t unix_time);

// struct tm у UTC -> Unix-час (як timegm, але без залежності від TZ)
time_t ds1307_tm_to_unix(const struct tm *t);
