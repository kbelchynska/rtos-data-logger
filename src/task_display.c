// Display Task: бере свіжий знімок з display_queue і малює його на SSD1306.

#include "app_types.h"
#include "oled.h"
#include "rtos_objects.h"
#include "tasks.h"

void display_task(void *arg)
{
    display_snapshot_t snap;
    while (1) {
        if (xQueueReceive(g_display_queue, &snap, portMAX_DELAY) != pdTRUE) continue;

        oled_render(&snap);   // лише RAM-буфер, шина не потрібна

        i2c_lock();
        oled_send();          // ~1 КБ по I2C
        i2c_unlock();

        // Кажемо Manager: кадр на екрані, шина вільна, можна засинати
        xSemaphoreGive(g_display_done);
    }
}
