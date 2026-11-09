#include "log_storage.h"

#include <stdio.h>
#include <time.h>
#include "app_types.h"
#include "esp_log.h"
#include "esp_partition.h"

static const char *TAG = "STORAGE";
static const esp_partition_t *s_part;
static uint16_t s_total;        // скільки секторів використовуємо
static uint16_t s_next_sector;  // куди писати наступний сектор

static bool sector_is_empty(uint16_t sector)
{
    uint32_t ts = 0;
    esp_partition_read(s_part, (size_t)sector * FLASH_SECTOR_SIZE, &ts, sizeof(ts));
    return ts == EMPTY_TIMESTAMP;
}

bool log_storage_init(void)
{
    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY,
                                      LOG_PARTITION_LABEL);
    if (s_part == NULL) {
        ESP_LOGE(TAG, "Partition '%s' not found: check partitions.csv", LOG_PARTITION_LABEL);
        return false;
    }
    s_total = s_part->size / FLASH_SECTOR_SIZE;
    if (LOG_MAX_SECTORS > 0 && LOG_MAX_SECTORS < s_total) s_total = LOG_MAX_SECTORS;

    // Сектори пишуться строго по черзі, тому перший порожній = позиція запису
    s_next_sector = s_total;
    for (uint16_t i = 0; i < s_total; i++) {
        if (sector_is_empty(i)) { s_next_sector = i; break; }
    }
    ESP_LOGI(TAG, "'%s' at 0x%06lx, %u sectors, %u used", LOG_PARTITION_LABEL,
             (unsigned long)s_part->address, s_total, s_next_sector);
    return true;
}

uint16_t log_storage_sectors_total(void) { return s_total; }
uint16_t log_storage_sectors_used(void)  { return s_next_sector; }
bool     log_storage_is_full(void)       { return s_part == NULL || s_next_sector >= s_total; }

bool log_storage_write_sector(const uint8_t *sector_buf)
{
    if (log_storage_is_full()) return false;

    size_t offset = (size_t)s_next_sector * FLASH_SECTOR_SIZE;
    // Без wear leveling: просто стерли сектор і записали
    esp_err_t err = esp_partition_erase_range(s_part, offset, FLASH_SECTOR_SIZE);
    if (err == ESP_OK) err = esp_partition_write(s_part, offset, sector_buf, FLASH_SECTOR_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sector %u write failed: %s", s_next_sector, esp_err_to_name(err));
        return false;
    }
    s_next_sector++;
    return true;
}

bool log_storage_erase_all(void)
{
    if (s_part == NULL) return false;
    esp_err_t err = esp_partition_erase_range(s_part, 0, (size_t)s_total * FLASH_SECTOR_SIZE);
    if (err != ESP_OK) return false;
    s_next_sector = 0;
    return true;
}

void log_storage_dump(void)
{
    // Читаємо по одному запису: не потрібен другий буфер розміром із сектор
    unsigned n = 0;
    printf("\n# idx,datetime_utc,temp_c,hum_pct,pres_mmhg\n");
    for (uint16_t s = 0; s < s_next_sector; s++) {
        for (unsigned r = 0; r < RECORDS_PER_SECTOR; r++) {
            log_record_t rec;
            esp_partition_read(s_part, (size_t)s * FLASH_SECTOR_SIZE + r * sizeof(rec), &rec, sizeof(rec));
            if (rec.timestamp == EMPTY_TIMESTAMP) break;   // хвіст сектора після flush
            time_t ts = rec.timestamp;
            struct tm t;
            gmtime_r(&ts, &t);
            printf("%u,%04d-%02d-%02d %02d:%02d:%02d,%.2f,%.2f,%u\n", n++, t.tm_year + 1900, t.tm_mon + 1,
                   t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, rec.temp_c_x100 / 100.0, rec.hum_pct_x100 / 100.0,
                   rec.pres_mmhg);
        }
    }
    printf("# %u records in %u sectors\n\n", n, s_next_sector);
}
