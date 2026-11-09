#pragma once

#include <stdbool.h>

// Виклики НЕ беруть м'ютекс I2C самі: це робить задача, яка їх викликає.
bool bme280_init(void);
bool bme280_read(float *temp_c, float *pres_hpa, float *hum_rh);
