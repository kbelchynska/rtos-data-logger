// Flash Writer Task: усі операції з partition "logdata" в одній задачі.

#include "app_types.h"
#include "esp_log.h"
#include "log_storage.h"
#include "rtos_objects.h"
#include "tasks.h"

static const char *TAG = "FLASH";

static void reply(flash_res_type_t type, bool ok)
{
    flash_result_t res = {
        .type = type,
        .ok = ok,
        .full = log_storage_is_full(),
        .sectors_used = log_storage_sectors_used(),
        .sectors_total = log_storage_sectors_total(),
    };
    xQueueSend(g_flash_result_queue, &res, portMAX_DELAY);
}

void flash_task(void *arg)
{
    reply(FLASH_RES_INIT, log_storage_init());

    flash_request_t req;
    while (1) {
        xQueueReceive(g_flash_queue, &req, portMAX_DELAY);
        g_flash_busy = true;

        switch (req.type) {
        case FLASH_REQ_WRITE: {
            uint16_t sector = log_storage_sectors_used();
            bool ok = log_storage_write_sector(req.buffer);
            if (ok) {
                ESP_LOGI(TAG, "Sector %u/%u written (%u records)", sector + 1, log_storage_sectors_total(),
                         req.count);
            }
            reply(FLASH_RES_WRITE, ok);
            break;
        }
        case FLASH_REQ_ERASE: {
            ESP_LOGI(TAG, "Erasing %u sectors...", log_storage_sectors_total());
            reply(FLASH_RES_ERASE, log_storage_erase_all());
            break;
        }
        case FLASH_REQ_DUMP:
            log_storage_dump();
            reply(FLASH_RES_DUMP, true);
            break;
        }

        g_flash_busy = false;
    }
}
