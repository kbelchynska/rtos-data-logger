#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "app_config.h"

// -
// Один запис у flash. Бінарний, 10 байт, без вирівнювання.
// Порожня (стерта) flash = 0xFF, тому timestamp == 0xFFFFFFFF = "запису нема".
// -
typedef struct __attribute__((packed))
{
    uint32_t timestamp;    // Unix-час (секунди) з DS1307
    int16_t temp_c_x100;   // 23.45 °C -> 2345
    uint16_t hum_pct_x100; // 56.78 %  -> 5678
    uint16_t pres_mmhg;    // мм рт. ст.
} log_record_t;

_Static_assert(sizeof(log_record_t) == 10, "log_record_t must be 10 bytes");

#define RECORDS_PER_SECTOR (FLASH_SECTOR_SIZE / sizeof(log_record_t)) // 409
#define EMPTY_TIMESTAMP 0xFFFFFFFFu

// Sensor Task -> Manager Task (sensor_queue)
typedef struct
{
    log_record_t rec;
    bool rtc_ok;
    bool bme_ok;
} sensor_sample_t;

// UART Task -> Manager Task (command_queue)
typedef enum
{
    CMD_HELP,
    CMD_STATUS,
    CMD_LOG_ON,
    CMD_LOG_OFF,
    CMD_FLUSH,
    CMD_ERASE,    // опційно
    CMD_DUMP,     // опційно: читання логів
    CMD_TIME_SET, // встановити час DS1307
} cmd_type_t;

typedef struct
{
    cmd_type_t type;
    char arg[24]; // для CMD_TIME_SET: "YYYY-MM-DD HH:MM:SS"
} command_t;

// Manager Task -> Display Task (display_queue)
typedef struct
{
    log_record_t last;
    bool rtc_ok, bme_ok;
    bool logging_on;
    bool partition_full;
    uint16_t buffered; // записів у RAM-буфері
    uint16_t sectors_used;
    uint16_t sectors_total;
    uint32_t next_log_in_s;
    char message[22]; // короткий статус: "FLUSHED", "ERASED"...
} display_snapshot_t;

// Manager Task -> Flash Writer Task (flash_queue)
typedef enum
{
    FLASH_REQ_WRITE,
    FLASH_REQ_ERASE,
    FLASH_REQ_DUMP
} flash_req_type_t;

typedef struct
{
    flash_req_type_t type;
    const uint8_t *buffer; // для WRITE: той самий єдиний RAM-буфер (1 сектор)
    uint16_t count;        // для WRITE: скільки записів у буфері
} flash_request_t;

// Flash Writer Task -> Manager Task (flash_result_queue)
typedef enum
{
    FLASH_RES_INIT,
    FLASH_RES_WRITE,
    FLASH_RES_ERASE,
    FLASH_RES_DUMP
} flash_res_type_t;

typedef struct
{
    flash_res_type_t type;
    bool ok;
    bool full; // після цієї операції вільних секторів не лишилось
    uint16_t sectors_used;
    uint16_t sectors_total;
} flash_result_t;
