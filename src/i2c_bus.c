#include "i2c_bus.h"

#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "I2C";
static i2c_master_bus_handle_t s_bus;

void i2c_bus_init(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &s_bus));
    ESP_LOGI(TAG, "Bus ready: SDA=GPIO%d SCL=GPIO%d", I2C_SDA_GPIO, I2C_SCL_GPIO);
}

i2c_master_bus_handle_t i2c_bus_handle(void)
{
    return s_bus;
}
