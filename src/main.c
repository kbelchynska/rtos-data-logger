// Модуль 6.8 — RTOS Data Logger (ESP32-S3 + FreeRTOS)
//
//  Sensor ──sensor_queue──►            ──display_queue──► Display ──► SSD1306
//                           Manager ◄──flash_result_q──┐
//  UART ───command_queue──►  (sleep) ──flash_queue────► Flash Writer ──► "logdata"
//
//  I2C (BME280 + DS1307 + SSD1306) захищена g_i2c_mutex.

#include "app_types.h"
#include "bme280.h"
#include "ds1307.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "oled.h"
#include "rtos_objects.h"
#include "tasks.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    rtos_objects_create();
    i2c_bus_init();

    // Задачі ще не запущені, але м'ютекс уже є: беремо його для порядку
    i2c_lock();
    bool oled_ok = oled_init();
    bool rtc_ok = ds1307_init();
    bool bme_ok = bme280_init();
    oled_render_text("RTOS Logger", "starting...");
    oled_send();
    i2c_unlock();

    ESP_LOGI(TAG, "OLED %s, DS1307 %s, BME280 %s", oled_ok ? "OK" : "FAIL", rtc_ok ? "OK" : "FAIL",
             bme_ok ? "OK" : "FAIL");
    ESP_LOGI(TAG, "Record %u bytes, %u records/sector, log every %d s%s", (unsigned)sizeof(log_record_t),
             (unsigned)RECORDS_PER_SECTOR, LOG_INTERVAL_S, LOG_DEMO_MODE ? " (DEMO MODE)" : "");

    uart_cmd_init();

    xTaskCreate(flash_task,   "flash",   FLASH_TASK_STACK,   NULL, FLASH_TASK_PRIO,   NULL);
    xTaskCreate(sensor_task,  "sensor",  SENSOR_TASK_STACK,  NULL, SENSOR_TASK_PRIO,  &g_sensor_task);
    xTaskCreate(display_task, "display", DISPLAY_TASK_STACK, NULL, DISPLAY_TASK_PRIO, NULL);
    xTaskCreate(uart_task,    "uart",    UART_TASK_STACK,    NULL, UART_TASK_PRIO,    NULL);
    xTaskCreate(manager_task, "manager", MANAGER_TASK_STACK, NULL, MANAGER_TASK_PRIO, NULL);
}
