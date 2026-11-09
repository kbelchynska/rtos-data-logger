#pragma once

// =====================================================================
//  Налаштування системи. Усе, що варто міняти, зібрано тут.
// =====================================================================

// ---- Логування ----
#define LOG_INTERVAL_MIN      1        // N хвилин між записами у flash (вимога ДЗ)

// Демо-режим для відео: запис щосекунди і лише LOG_DEMO_SECTORS секторів,
// щоб побачити "LOG FULL" за кілька хвилин, а не за тижні.
#define LOG_DEMO_MODE         0
#define LOG_DEMO_SECTORS      1        // 1 сектор = 409 записів = ~7 хв у демо-режимі

#if LOG_DEMO_MODE
#define LOG_INTERVAL_S        1
#define LOG_MAX_SECTORS       LOG_DEMO_SECTORS
#else
#define LOG_INTERVAL_S        (LOG_INTERVAL_MIN * 60)
#define LOG_MAX_SECTORS       0        // 0 = увесь partition
#endif

#define LOG_PARTITION_LABEL   "logdata"
#define FLASH_SECTOR_SIZE     4096

// ---- Сон ----
#define WAKE_PERIOD_MS        1000     // прокидаємось щосекунди
#define UART_AWAKE_WINDOW_S   15       // після символу з UART не спимо стільки секунд,
                                       // щоб можна було спокійно набрати команду

// ---- I2C: BME280 + DS1307 + SSD1306 на одній шині ----
#define I2C_PORT              I2C_NUM_0
#define I2C_SDA_GPIO          8
#define I2C_SCL_GPIO          9
#define I2C_FREQ_HZ           400000
#define BME280_ADDR           0x76
#define DS1307_ADDR           0x68
#define SSD1306_ADDR          0x3C

// ---- UART ----
#define CMD_UART_PORT         UART_NUM_0   // той самий порт, що й консоль (роз'єм "COM"/"UART" на платі)
#define CMD_UART_BAUD         115200

// ---- Задачі: стек (байти) і пріоритет ----
#define SENSOR_TASK_STACK     4096
#define UART_TASK_STACK       4096
#define DISPLAY_TASK_STACK    4096
#define MANAGER_TASK_STACK    6144
#define FLASH_TASK_STACK      4096

#define MANAGER_TASK_PRIO     6
#define SENSOR_TASK_PRIO      5
#define FLASH_TASK_PRIO       4
#define UART_TASK_PRIO        4
#define DISPLAY_TASK_PRIO     3
