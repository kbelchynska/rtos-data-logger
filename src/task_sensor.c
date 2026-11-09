// Sensor Task: за сигналом Manager читає DS1307 і BME280, кладе результат у sensor_queue.

#include <math.h>
#include "app_types.h"
#include "bme280.h"
#include "ds1307.h"
#include "rtos_objects.h"
#include "tasks.h"

#define HPA_TO_MMHG 0.750062f

void sensor_task(void *arg)
{
    while (1) {
        // Спимо, доки Manager не скаже "новий цикл" (раз на секунду після пробудження)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensor_sample_t s = {0};
        time_t now = 0;
        float t = 0, p = 0, h = 0;

        i2c_lock();
        s.rtc_ok = ds1307_read(&now);
        s.bme_ok = bme280_read(&t, &p, &h);
        i2c_unlock();

        s.rec.timestamp = (uint32_t)now;
        s.rec.temp_c_x100 = (int16_t)lroundf(t * 100.0f);
        s.rec.hum_pct_x100 = (uint16_t)lroundf(h * 100.0f);
        s.rec.pres_mmhg = (uint16_t)lroundf(p * HPA_TO_MMHG);

        xQueueSend(g_sensor_queue, &s, 0);
    }
}
