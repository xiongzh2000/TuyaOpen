/**
 * @file board_com_api.c
 * @brief Board-level hardware registration for ESP32-P4-C6 dev board
 *        (Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3).
 *
 * Hardware: 4.3" 480x800 (ST7701, MIPI DSI), GT911 touch, ES8311 audio codec.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "board_com_api.h"

#include "tkl_pinmux.h"
#include "tkl_gpio.h"
#include "tdd_disp_esp_mipi_dsi_panel.h"

/* Binary-compatible with st7701_lcd_init_cmd_t from esp_lcd_st7701 component */
typedef struct {
    int cmd;
    const void *data;
    size_t data_bytes;
    unsigned int delay_ms;
} st7701_init_cmd_t;
#include "tdd_tp_esp_gt911.h"
#include "tdl_display_manage.h"
#include "tdd_audio_8311_codec.h"

/* I2C bus (shared by GT911 and ES8311) */
#define I2C_NUM    (0)
#define I2C_SCL_IO (8)
#define I2C_SDA_IO (7)

/* Display */
#define DISPLAY_WIDTH  (480)
#define DISPLAY_HEIGHT (800)
#define LCD_BL_IO      (26)
#define LCD_RST_IO     (27)

/* MIPI DSI PHY power: LDO channel 3, 2.5V */
#define DSI_PHY_LDO_CHAN       (3)
#define DSI_PHY_LDO_VOLTAGE_MV (2500)

/* GT911 touch */
#define TOUCH_INT_IO (-1)
#define TOUCH_RST_IO (23)

/* ES8311 audio */
#define I2S_ID        (0)
#define I2S_MCLK_IO   (13)
#define I2S_SCLK_IO   (12)
#define I2S_LRCLK_IO  (10)
#define I2S_DOUT_IO    (9)
#define I2S_DIN_IO    (11)
#define PA_EN_IO      (53)

/*
 * ST7701 vendor-specific init commands for Waveshare 4.3" 480x800 panel.
 * Taken from official Waveshare demo:
 * https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4.3
 */
static const st7701_init_cmd_t sg_st7701_init_cmds[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x17, 0x08}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0x40, 0xC9, 0x94, 0x0E, 0x10, 0x05, 0x0B, 0x09, 0x08, 0x26, 0x04, 0x52, 0x10, 0x69, 0x6B, 0x69}, 16, 0},
    {0xB1, (uint8_t[]){0x40, 0xD2, 0x98, 0x0C, 0x92, 0x07, 0x09, 0x08, 0x07, 0x25, 0x02, 0x0E, 0x0C, 0x6E, 0x78, 0x55}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x5D}, 1, 0},
    {0xB1, (uint8_t[]){0x4E}, 1, 0},
    {0xB2, (uint8_t[]){0x87}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4E}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xB9, (uint8_t[]){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t[]){0x03}, 1, 0},
    {0xBC, (uint8_t[]){0x00}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x04, 0xA0, 0x00, 0xA0, 0x05, 0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40}, 11, 0},
    {0xE2, (uint8_t[]){0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00}, 13, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0, 0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0}, 16, 0},
    {0xEB, (uint8_t[]){0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t[]){0x08, 0x01}, 2, 0},
    {0xED, (uint8_t[]){0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B}, 16, 0},
    {0xEF, (uint8_t[]){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x29, (uint8_t[]){0x00}, 0, 0},
};

static OPERATE_RET __board_backlight_cb(uint8_t brightness, void *arg)
{
    (void)arg;
    tkl_gpio_write(LCD_BL_IO, brightness > 0 ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW);
    return OPRT_OK;
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* Backlight GPIO init */
    TUYA_GPIO_BASE_CFG_T bl_cfg = {
        .mode   = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level  = TUYA_GPIO_LEVEL_LOW,
    };
    tkl_gpio_init(LCD_BL_IO, &bl_cfg);

    LCD_MIPI_DSI_PANEL_HW_CFG_T dsi_hw = {
        .num_data_lanes     = 2,
        .lane_bit_rate_mbps = 500,    /* ST7701: 500 Mbps (per official demo) */
        .dpi_clk_mhz       = 30,     /* ST7701: 30 MHz (per official demo) */
        .reset_gpio_num     = LCD_RST_IO,
        .ldo_chan           = DSI_PHY_LDO_CHAN,
        .ldo_voltage_mv     = DSI_PHY_LDO_VOLTAGE_MV,
        .timings = {
            .hsync_pulse_width = 12,
            .hsync_back_porch  = 42,
            .hsync_front_porch = 42,
            .vsync_pulse_width = 8,
            .vsync_back_porch  = 2,
            .vsync_front_porch = 60,
        },
        .init_cmds      = sg_st7701_init_cmds,
        .init_cmds_size = sizeof(sg_st7701_init_cmds) / sizeof(sg_st7701_init_cmds[0]),
    };

    TDD_DISP_ESP_LCD_CFG_T lcd_cfg = {
        .width     = DISPLAY_WIDTH,
        .height    = DISPLAY_HEIGHT,
        .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
        .rotation  = TUYA_DISPLAY_ROTATION_0,
        .is_swap   = 0,
    };

    TUYA_CALL_ERR_RETURN(tdd_disp_esp_mipi_dsi_panel_register(DISPLAY_NAME, &dsi_hw, &lcd_cfg));
    TUYA_CALL_ERR_RETURN(tdl_disp_custom_backlight_register(DISPLAY_NAME, __board_backlight_cb, NULL));

    TDD_TP_ESP_GT911_CFG_T tp_cfg = {
        .i2c_port   = I2C_NUM,
        .i2c_scl_io = I2C_SCL_IO,
        .i2c_sda_io = I2C_SDA_IO,
        .rst_io     = TOUCH_RST_IO,
        .int_io     = TOUCH_INT_IO,
        .tp = {
            .tp_cfg = {
                .x_max = DISPLAY_WIDTH,
                .y_max = DISPLAY_HEIGHT,
                .flags = {
                    .swap_xy  = 0,
                    .mirror_x = 0,
                    .mirror_y = 0,
                },
            },
        },
    };
    TUYA_CALL_ERR_LOG(tdd_tp_esp_i2c_gt911_register(DISPLAY_NAME, &tp_cfg));

    return rt;
}

static OPERATE_RET __board_register_audio(void)
{
    TDD_AUDIO_8311_CODEC_T codec_cfg = {
        .i2c_id         = I2C_NUM,
        .i2c_sda_io     = I2C_SDA_IO,
        .i2c_scl_io     = I2C_SCL_IO,
        .mic_sample_rate = 16000,
        .spk_sample_rate = 16000,
        .i2s_id         = I2S_ID,
        .i2s_mck_io     = I2S_MCLK_IO,
        .i2s_bck_io     = I2S_SCLK_IO,
        .i2s_ws_io      = I2S_LRCLK_IO,
        .i2s_do_io      = I2S_DOUT_IO,
        .i2s_di_io      = I2S_DIN_IO,
        .gpio_output_pa = PA_EN_IO,
        .es8311_addr    = 0x30,  /* esp_codec_dev uses 8-bit left-aligned addr (0x18<<1) */
        .dma_desc_num   = 6,
        .dma_frame_num  = 240,
        .default_volume = 80,
    };

    return tdd_audio_8311_codec_register(AUDIO_CODEC_NAME, codec_cfg);
}

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* Note: Do NOT call tkl_io_pinmux_config for I2C on ESP32-P4.
     * ESP-IDF i2c_new_master_bus() configures the GPIO internally.
     * Calling pinmux before that conflicts and causes I2C NACK. */

    TUYA_CALL_ERR_LOG(__board_register_display());
    TUYA_CALL_ERR_LOG(__board_register_audio());

    return rt;
}
