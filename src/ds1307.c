#include "ds1307.h"

#include "app_config.h"
#include "esp_log.h"
#include "i2c_bus.h"

static const char *TAG = "DS1307";
static i2c_master_dev_handle_t s_dev;
static bool s_ok;

static uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
static uint8_t dec2bcd(uint8_t d) { return (uint8_t)(((d / 10) << 4) | (d % 10)); }

// struct tm (UTC) -> Unix-час без mktime(), щоб не залежати від TZ
time_t ds1307_tm_to_unix(const struct tm *t)
{
    int y = t->tm_year + 1900, m = t->tm_mon + 1;
    if (m <= 2)
    {
        y;
        m += 12;
    }
    int64_t days = 365LL * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + t->tm_mday - 719469;
    return (time_t)(days * 86400 + t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec);
}

bool ds1307_init(void)
{
    if (i2c_master_probe(i2c_bus_handle(), DS1307_ADDR, 100) != ESP_OK)
    {
        ESP_LOGE(TAG, "No device at 0x%02X", DS1307_ADDR);
        return false;
    }
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS1307_ADDR,
        .scl_speed_hz = 100000, // DS1307 підтримує лише 100 кГц
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle(), &dev_cfg, &s_dev));

    // Біт 7 регістра секунд (CH, Clock Halt) = 1 у нового чипа: годинник стоїть. Запускаємо.
    uint8_t reg = 0x00, sec = 0;
    if (i2c_master_transmit_receive(s_dev, &reg, 1, &sec, 1, 100) == ESP_OK && (sec & 0x80))
    {
        uint8_t buf[2] = {0x00, (uint8_t)(sec & 0x7F)};
        i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
        ESP_LOGW(TAG, "Clock was halted, started it. Set the time: time YYYY-MM-DD HH:MM:SS");
    }
    s_ok = true;
    ESP_LOGI(TAG, "Ready at 0x%02X", DS1307_ADDR);
    return true;
}

bool ds1307_read(time_t *out_unix)
{
    if (!s_ok)
        return false;
    uint8_t reg = 0x00, r[7];
    // Один burst-read усіх 7 регістрів: секунди не "перескочать" між читаннями полів
    if (i2c_master_transmit_receive(s_dev, &reg, 1, r, sizeof(r), 100) != ESP_OK)
        return false;

    struct tm t = {
        .tm_sec = bcd2dec(r[0] & 0x7F),
        .tm_min = bcd2dec(r[1]),
        .tm_hour = bcd2dec(r[2] & 0x3F), // 24-годинний режим
        .tm_mday = bcd2dec(r[4]),
        .tm_mon = bcd2dec(r[5]) - 1,
        .tm_year = bcd2dec(r[6]) + 100, // DS1307 зберігає 00..99 -> 2000..2099
    };
    *out_unix = ds1307_tm_to_unix(&t);
    return true;
}

bool ds1307_write(time_t unix_time)
{
    if (!s_ok)
        return false;
    struct tm t;
    gmtime_r(&unix_time, &t);
    uint8_t buf[8] = {
        0x00,
        (uint8_t)(dec2bcd(t.tm_sec) & 0x7F), // CH = 0: годинник іде
        dec2bcd(t.tm_min),
        (uint8_t)(dec2bcd(t.tm_hour) & 0x3F),
        (uint8_t)(t.tm_wday + 1),
        dec2bcd(t.tm_mday),
        dec2bcd(t.tm_mon + 1),
        dec2bcd(t.tm_year % 100),
    };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100) == ESP_OK;
}
