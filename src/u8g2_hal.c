#include "u8g2_hal.h"

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "U8G2_HAL";
static i2c_master_dev_handle_t s_dev_handle;

void u8g2_hal_set_i2c_device(i2c_master_dev_handle_t dev_handle)
{
    s_dev_handle = dev_handle;
}

uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    // u8g2 надсилає I2C-транзакцію шматками по 32 байти (SEND) між START/END_TRANSFER
    static uint8_t buffer[32];
    static uint8_t buf_idx;

    switch (msg) {
        case U8X8_MSG_BYTE_START_TRANSFER:
            buf_idx = 0;
            break;

        case U8X8_MSG_BYTE_SEND: {
            const uint8_t *data = (const uint8_t *)arg_ptr;
            for (uint8_t i = 0; i < arg_int && buf_idx < sizeof(buffer); i++) {
                buffer[buf_idx++] = data[i];
            }
            break;
        }

        case U8X8_MSG_BYTE_END_TRANSFER: {
            esp_err_t err = i2c_master_transmit(s_dev_handle, buffer, buf_idx, 100);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "i2c_master_transmit failed: %s", esp_err_to_name(err));
            }
            break;
        }

        case U8X8_MSG_BYTE_INIT:
        default:
            break;
    }
    return 1;
}

uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg) {
        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int));
            break;

        case U8X8_MSG_DELAY_10MICRO:
            esp_rom_delay_us(10);
            break;

        case U8X8_MSG_DELAY_100NANO:
            esp_rom_delay_us(1);
            break;

        case U8X8_MSG_GPIO_AND_DELAY_INIT:
        case U8X8_MSG_GPIO_RESET:
        default:
            // апаратний I2C, окремі GPIO для reset/clock/data не використовуються
            break;
    }
    return 1;
}
