#include "bme280.h"

#include "app_config.h"
#include "bme280_compensate.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

#define REG_CHIP_ID   0xD0
#define REG_CTRL_HUM  0xF2
#define REG_CTRL_MEAS 0xF4
#define REG_DATA      0xF7

static const char *TAG = "BME280";
static i2c_master_dev_handle_t s_dev;
static bme280_calib_t s_calib;
static bool s_ok;

static esp_err_t read_regs(uint8_t reg, uint8_t *out, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, out, len, 100);
}

static esp_err_t write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

bool bme280_init(void)
{
    if (i2c_master_probe(i2c_bus_handle(), BME280_ADDR, 100) != ESP_OK) {
        ESP_LOGE(TAG, "No device at 0x%02X", BME280_ADDR);
        return false;
    }
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BME280_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle(), &dev_cfg, &s_dev));

    uint8_t id = 0;
    if (read_regs(REG_CHIP_ID, &id, 1) != ESP_OK || id != 0x60) {
        ESP_LOGE(TAG, "chip id 0x%02X, expected 0x60", id);
        return false;
    }
    uint8_t tp[26], h[7];
    if (read_regs(0x88, tp, sizeof(tp)) != ESP_OK || read_regs(0xE1, h, sizeof(h)) != ESP_OK) {
        return false;
    }
    bme280_parse_calib(&s_calib, tp, h);
    s_ok = true;
    ESP_LOGI(TAG, "Ready at 0x%02X", BME280_ADDR);
    return true;
}

bool bme280_read(float *temp_c, float *pres_hpa, float *hum_rh)
{
    if (!s_ok) return false;

    // Forced mode: датчик робить один вимір і засинає сам, між вимірами майже не споживає
    if (write_reg(REG_CTRL_HUM, 0x01) != ESP_OK) return false;
    if (write_reg(REG_CTRL_MEAS, (1 << 5) | (1 << 2) | 0x01) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t raw[8];
    if (read_regs(REG_DATA, raw, sizeof(raw)) != ESP_OK) return false;

    int32_t adc_p = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_t = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);
    int32_t adc_h = ((int32_t)raw[6] << 8) | raw[7];

    int32_t t = bme280_compensate_T(&s_calib, adc_t);   // першою: рахує t_fine
    uint32_t p = bme280_compensate_P(&s_calib, adc_p);
    uint32_t hh = bme280_compensate_H(&s_calib, adc_h);

    *temp_c = t / 100.0f;
    *pres_hpa = (p / 256.0f) / 100.0f;
    *hum_rh = hh / 1024.0f;
    return true;
}
