/**
 * @file ch422g.c
 * @brief CH422G 12-bit I2C IO expander helper (implementation).
 *
 * CH422G uses distinct I2C slave addresses for each register function
 * (no register-offset byte after the address):
 *   0x24 (7-bit): write system parameters (mode config)
 *   0x38 (7-bit): write OC pin output levels (pins 0-7)
 *   0x23 (7-bit): write EXIO pin output levels (pins 0-3)
 *   0x26 (7-bit): read EXIO pin input levels
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "ch422g.h"

#include "esp_err.h"
#include "esp_log.h"

#include "driver/i2c_master.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "CH422G"

/* CH422G I2C slave addresses (7-bit) */
#define CH422G_ADDR_MODE     (0x24)
#define CH422G_ADDR_OC_OUT   (0x38)
#define CH422G_ADDR_EXIO_OUT (0x23)
#define CH422G_ADDR_EXIO_IN  (0x26)

/* System parameter bits */
#define CH422G_MODE_OC_PP    (1 << 0) /* OC push-pull output enable */
#define CH422G_MODE_SLEEP    (1 << 1)
#define CH422G_MODE_OD_EN    (1 << 2) /* open-drain enable */
#define CH422G_MODE_X_INT    (1 << 4) /* interrupt enable */
#define CH422G_MODE_A_SCAN   (1 << 5) /* auto-scan enable */
#define CH422G_MODE_IO_OE    (1 << 6) /* EXIO push-pull output enable */

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t dev_mode;
    i2c_master_dev_handle_t dev_oc_out;
    i2c_master_dev_handle_t dev_exio_out;
    i2c_master_dev_handle_t dev_exio_in;
    uint8_t                 oc_state;
    uint8_t                 exio_state;
} CH422G_CONFIG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static CH422G_CONFIG_T sg_ch422g = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static i2c_master_bus_handle_t __i2c_init(int i2c_num, int scl_io, int sda_io)
{
    i2c_master_bus_handle_t i2c_bus = NULL;
    esp_err_t               esp_rt  = ESP_OK;

    esp_rt = i2c_master_get_bus_handle(i2c_num, &i2c_bus);
    if (esp_rt == ESP_OK && i2c_bus) {
        return i2c_bus;
    }

    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port          = i2c_num,
        .sda_io_num        = sda_io,
        .scl_io_num        = scl_io,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority     = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = 1,
            },
    };
    esp_rt = i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(esp_rt));
        return NULL;
    }

    return i2c_bus;
}

static int __add_device(i2c_master_bus_handle_t bus, uint16_t addr, i2c_master_dev_handle_t *out)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = 400 * 1000,
        .scl_wait_us     = 0,
        .flags =
            {
                .disable_ack_check = 0,
            },
    };
    esp_err_t rt = i2c_master_bus_add_device(bus, &dev_cfg, out);
    if (rt != ESP_OK || NULL == *out) {
        ESP_LOGE(TAG, "Failed to add device 0x%02X: %s", addr, esp_err_to_name(rt));
        return -1;
    }
    return 0;
}

int ch422g_init(CH422G_HW_CFG_T *hw)
{
    if (sg_ch422g.dev_mode) {
        ESP_LOGI(TAG, "CH422G already initialized");
        return 0;
    }

    if (NULL == hw) {
        ESP_LOGE(TAG, "hw cfg is NULL");
        return -1;
    }

    sg_ch422g.i2c_bus = __i2c_init(hw->i2c_port, hw->scl_io, hw->sda_io);
    if (!sg_ch422g.i2c_bus) {
        return -1;
    }

    if (__add_device(sg_ch422g.i2c_bus, CH422G_ADDR_MODE,     &sg_ch422g.dev_mode)     != 0 ||
        __add_device(sg_ch422g.i2c_bus, CH422G_ADDR_OC_OUT,   &sg_ch422g.dev_oc_out)   != 0 ||
        __add_device(sg_ch422g.i2c_bus, CH422G_ADDR_EXIO_OUT, &sg_ch422g.dev_exio_out) != 0 ||
        __add_device(sg_ch422g.i2c_bus, CH422G_ADDR_EXIO_IN,  &sg_ch422g.dev_exio_in)  != 0) {
        return -1;
    }

    /* Set all outputs to 0 before enabling output mode to avoid glitches */
    sg_ch422g.oc_state   = 0;
    sg_ch422g.exio_state = 0;
    i2c_master_transmit(sg_ch422g.dev_oc_out,   &sg_ch422g.oc_state,   1, 100);
    i2c_master_transmit(sg_ch422g.dev_exio_out, &sg_ch422g.exio_state, 1, 100);

    /* Enable OC push-pull output + EXIO push-pull output */
    uint8_t mode = CH422G_MODE_OC_PP | CH422G_MODE_IO_OE;
    esp_err_t rt = i2c_master_transmit(sg_ch422g.dev_mode, &mode, 1, 100);
    if (rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set mode: %s", esp_err_to_name(rt));
        return -1;
    }

    ESP_LOGI(TAG, "CH422G initialized successfully");
    return 0;
}

int ch422g_set_oc_level(uint32_t pin_mask, int level)
{
    if (NULL == sg_ch422g.dev_oc_out) {
        ESP_LOGE(TAG, "CH422G not initialized");
        return -1;
    }

    uint8_t mask = (uint8_t)(pin_mask & 0xFF);
    if (level) {
        sg_ch422g.oc_state |= mask;
    } else {
        sg_ch422g.oc_state &= ~mask;
    }

    esp_err_t rt = i2c_master_transmit(sg_ch422g.dev_oc_out, &sg_ch422g.oc_state, 1, 100);
    if (rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set OC level: %s", esp_err_to_name(rt));
        return -1;
    }

    return 0;
}

int ch422g_set_exio_level(uint32_t pin_mask, int level)
{
    if (NULL == sg_ch422g.dev_exio_out) {
        ESP_LOGE(TAG, "CH422G not initialized");
        return -1;
    }

    uint8_t mask = (uint8_t)(pin_mask & 0x0F);
    if (level) {
        sg_ch422g.exio_state |= mask;
    } else {
        sg_ch422g.exio_state &= ~mask;
    }

    esp_err_t rt = i2c_master_transmit(sg_ch422g.dev_exio_out, &sg_ch422g.exio_state, 1, 100);
    if (rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set EXIO level: %s", esp_err_to_name(rt));
        return -1;
    }

    return 0;
}

int ch422g_get_exio_level(uint32_t pin_mask, uint32_t *level)
{
    if (NULL == sg_ch422g.dev_exio_in) {
        ESP_LOGE(TAG, "CH422G not initialized");
        return -1;
    }

    if (NULL == level) {
        ESP_LOGE(TAG, "level pointer is NULL");
        return -1;
    }

    uint8_t value = 0;
    esp_err_t rt = i2c_master_receive(sg_ch422g.dev_exio_in, &value, 1, 100);
    if (rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read EXIO: %s", esp_err_to_name(rt));
        return -1;
    }

    *level = value & (pin_mask & 0x0F);
    return 0;
}
