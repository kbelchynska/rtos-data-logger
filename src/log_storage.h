#pragma once

#include <stdbool.h>
#include <stdint.h>

// Робота з partition "logdata". Викликається ЛИШЕ з Flash Writer Task.
//
// Метаданих у flash немає. Позицію запису знаходимо при старті:
// перший сектор, у якого перший запис порожній (0xFF), і є наступний вільний.

bool     log_storage_init(void);
uint16_t log_storage_sectors_total(void);
uint16_t log_storage_sectors_used(void);
bool     log_storage_is_full(void);

// Стирає наступний сектор і пише в нього рівно 1 сектор з буфера
bool log_storage_write_sector(const uint8_t *sector_buf);

bool log_storage_erase_all(void);

// Друкує всі записи в UART у вигляді CSV
void log_storage_dump(void);
