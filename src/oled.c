#include "oled.h"

#include <stdio.h>
#include <time.h>
#include "app_config.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "u8g2.h"
#include "u8g2_hal.h"

static const char *TAG = "OLED";
static u8g2_t s_u8g2;
static bool s_ok;

bool oled_init(void)
{
    if (i2c_master_probe(i2c_bus_handle(), SSD1306_ADDR, 100) != ESP_OK)
    {
        ESP_LOGE(TAG, "No SSD1306 at 0x%02X, display disabled", SSD1306_ADDR);
        return false;
    }
    i2c_master_dev_handle_t dev;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SSD1306_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle(), &dev_cfg, &dev));

    u8g2_hal_set_i2c_device(dev);
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&s_u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb,
                                           u8g2_esp32_gpio_and_delay_cb);
    u8g2_InitDisplay(&s_u8g2);
    u8g2_SetPowerSave(&s_u8g2, 0);
    s_ok = true;
    return true;
}

void oled_render_text(const char *line1, const char *line2)
{
    if (!s_ok)
        return;
    u8g2_ClearBuffer(&s_u8g2);
    u8g2_SetFont(&s_u8g2, u8g2_font_7x14B_tr);
    u8g2_DrawStr(&s_u8g2, 0, 24, line1);
    u8g2_SetFont(&s_u8g2, u8g2_font_6x10_tr);
    if (line2)
        u8g2_DrawStr(&s_u8g2, 0, 44, line2);
}

//  14:30:05    09.11.26     <- час з DS1307
//  23.45C      56.8%        <- великий шрифт
//  P 754 mmHg
//  LOG ON  B 12/409  S 3/64 <- стан логера (або банер LOG FULL)
void oled_render(const display_snapshot_t *s)
{
    if (!s_ok)
        return;
    char buf[40];
    u8g2_ClearBuffer(&s_u8g2);

    // - рядок часу -
    u8g2_SetFont(&s_u8g2, u8g2_font_6x10_tr);
    if (s->rtc_ok)
    {
        time_t ts = s->last.timestamp;
        struct tm t;
        gmtime_r(&ts, &t);
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
        u8g2_DrawStr(&s_u8g2, 0, 9, buf);
        snprintf(buf, sizeof(buf), "%02d.%02d.%02d", t.tm_mday, t.tm_mon + 1, t.tm_year % 100);
        u8g2_DrawStr(&s_u8g2, 128 - u8g2_GetStrWidth(&s_u8g2, buf), 9, buf);
    }
    else
    {
        u8g2_DrawStr(&s_u8g2, 0, 9, "RTC ERROR");
    }
    u8g2_DrawHLine(&s_u8g2, 0, 12, 128);

    // - показання -
    if (s->bme_ok)
    {
        u8g2_SetFont(&s_u8g2, u8g2_font_7x14B_tr);
        snprintf(buf, sizeof(buf), "%.2fC", s->last.temp_c_x100 / 100.0f);
        u8g2_DrawStr(&s_u8g2, 0, 28, buf);
        snprintf(buf, sizeof(buf), "%.1f%%", s->last.hum_pct_x100 / 100.0f);
        u8g2_DrawStr(&s_u8g2, 128 - u8g2_GetStrWidth(&s_u8g2, buf), 28, buf);
        u8g2_SetFont(&s_u8g2, u8g2_font_6x10_tr);
        snprintf(buf, sizeof(buf), "P %u mmHg", s->last.pres_mmhg);
        u8g2_DrawStr(&s_u8g2, 0, 41, buf);
    }
    else
    {
        u8g2_SetFont(&s_u8g2, u8g2_font_7x14B_tr);
        u8g2_DrawStr(&s_u8g2, 0, 32, "BME280 ERROR");
    }

    // - стан логера -
    if (s->partition_full)
    {
        // Інвертований банер, щоб "LOG FULL" було видно здалеку
        u8g2_DrawBox(&s_u8g2, 0, 46, 128, 18);
        u8g2_SetDrawColor(&s_u8g2, 0);
        u8g2_SetFont(&s_u8g2, u8g2_font_7x14B_tr);
        u8g2_DrawStr(&s_u8g2, 18, 60, "! LOG FULL !");
        u8g2_SetDrawColor(&s_u8g2, 1);
    }
    else
    {
        u8g2_SetFont(&s_u8g2, u8g2_font_5x8_tr);
        if (s->message[0])
        {
            u8g2_DrawStr(&s_u8g2, 0, 52, s->message);
        }
        else if (s->logging_on)
        {
            snprintf(buf, sizeof(buf), "next log in %lus", (unsigned long)s->next_log_in_s);
            u8g2_DrawStr(&s_u8g2, 0, 52, buf);
        }
        snprintf(buf, sizeof(buf), "LOG %s  B%u/%u  S%u/%u", s->logging_on ? "ON " : "OFF", s->buffered,
                 (unsigned)RECORDS_PER_SECTOR, s->sectors_used, s->sectors_total);
        u8g2_DrawStr(&s_u8g2, 0, 63, buf);
    }
}

void oled_send(void)
{
    if (s_ok)
        u8g2_SendBuffer(&s_u8g2);
}
