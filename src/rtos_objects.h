#pragma once

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// Усі черги, м'ютекс і хендли задач в одному місці
extern QueueHandle_t     g_sensor_queue;        // Sensor  -> Manager : sensor_sample_t
extern QueueHandle_t     g_command_queue;       // UART    -> Manager : command_t
extern QueueHandle_t     g_display_queue;       // Manager -> Display : display_snapshot_t
extern QueueHandle_t     g_flash_queue;         // Manager -> Flash   : flash_request_t
extern QueueHandle_t     g_flash_result_queue;  // Flash   -> Manager : flash_result_t

extern SemaphoreHandle_t g_i2c_mutex;           // одна шина I2C на три пристрої
extern SemaphoreHandle_t g_display_done;        // Display -> Manager: кадр відправлено, можна спати

extern TaskHandle_t      g_sensor_task;         // Manager будить його нотифікацією

// Flash Writer зайнятий (запис/стирання/вивід логів): Manager у цей час не спить
extern volatile bool     g_flash_busy;
// Час останнього символу з UART (esp_timer, мкс): поки людина друкує, не спимо
extern volatile int64_t  g_last_uart_activity_us;

void rtos_objects_create(void);

static inline void i2c_lock(void)   { xSemaphoreTake(g_i2c_mutex, portMAX_DELAY); }
static inline void i2c_unlock(void) { xSemaphoreGive(g_i2c_mutex); }
