#pragma once

// П'ять задач системи
void sensor_task(void *arg);    // BME280 + DS1307 -> sensor_queue
void uart_task(void *arg);      // UART -> command_queue
void display_task(void *arg);   // display_queue -> SSD1306
void manager_task(void *arg);   // логіка, RAM-буфер, Light Sleep
void flash_task(void *arg);     // flash_queue -> partition "logdata"

void uart_cmd_init(void);       // драйвер UART ставимо до старту задач
