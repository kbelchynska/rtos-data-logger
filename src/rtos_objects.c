#include "rtos_objects.h"

#include "app_types.h"
#include "esp_err.h"

QueueHandle_t     g_sensor_queue;
QueueHandle_t     g_command_queue;
QueueHandle_t     g_display_queue;
QueueHandle_t     g_flash_queue;
QueueHandle_t     g_flash_result_queue;
SemaphoreHandle_t g_i2c_mutex;
SemaphoreHandle_t g_display_done;
TaskHandle_t      g_sensor_task;
volatile bool     g_flash_busy;
volatile int64_t  g_last_uart_activity_us;

void rtos_objects_create(void)
{
    g_sensor_queue       = xQueueCreate(4, sizeof(sensor_sample_t));
    g_command_queue      = xQueueCreate(8, sizeof(command_t));
    g_display_queue      = xQueueCreate(1, sizeof(display_snapshot_t));   // завжди лише свіжий кадр
    g_flash_queue        = xQueueCreate(4, sizeof(flash_request_t));
    g_flash_result_queue = xQueueCreate(4, sizeof(flash_result_t));
    g_i2c_mutex          = xSemaphoreCreateMutex();
    g_display_done       = xSemaphoreCreateBinary();

    configASSERT(g_sensor_queue && g_command_queue && g_display_queue && g_flash_queue &&
                 g_flash_result_queue && g_i2c_mutex && g_display_done);
}
