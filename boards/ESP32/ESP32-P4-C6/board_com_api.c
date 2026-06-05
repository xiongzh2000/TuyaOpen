/**
 * @file board_com_api.c
 * @brief Board-level hardware registration for ESP32-P4-C6 dev board.
 *        Hardware same as Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3:
 *        4.3" 480x800 IPS (ILI9881C, MIPI DSI), GT911 touch, ES8311 audio codec.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "board_com_api.h"

#include "tkl_pinmux.h"
#include "tkl_gpio.h"
#include "tdd_disp_esp_mipi_dsi_panel.h"
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
#define TOUCH_RST_IO (-1)

/* ES8311 audio */
#define I2S_ID        (0)
#define I2S_MCLK_IO   (13)
#define I2S_SCLK_IO   (12)
#define I2S_LRCLK_IO  (10)
#define I2S_DOUT_IO    (9)
#define I2S_DIN_IO    (11)
#define PA_EN_IO      (53)

/*
 * ILI9881C vendor init commands for the 4.3" 480x800 panel.
 */
static const uint8_t ili9881c_page3[] = {0x98, 0x81, 0x03};
static const uint8_t ili9881c_page4[] = {0x98, 0x81, 0x04};
static const uint8_t ili9881c_page1[] = {0x98, 0x81, 0x01};
static const uint8_t ili9881c_page0[] = {0x98, 0x81, 0x00};

static const uint8_t p3_01[] = {0x00}; static const uint8_t p3_02[] = {0x00};
static const uint8_t p3_03[] = {0x73}; static const uint8_t p3_04[] = {0x73};
static const uint8_t p3_05[] = {0x00}; static const uint8_t p3_06[] = {0x06};
static const uint8_t p3_07[] = {0x02}; static const uint8_t p3_08[] = {0x00};
static const uint8_t p3_09[] = {0x01}; static const uint8_t p3_0a[] = {0x01};
static const uint8_t p3_0f[] = {0x01}; static const uint8_t p3_10[] = {0x01};
static const uint8_t p3_11[] = {0x01}; static const uint8_t p3_12[] = {0x01};
static const uint8_t p3_13[] = {0x01}; static const uint8_t p3_14[] = {0x01};
static const uint8_t p3_15[] = {0x00}; static const uint8_t p3_16[] = {0x00};
static const uint8_t p3_17[] = {0x00}; static const uint8_t p3_18[] = {0x00};
static const uint8_t p3_19[] = {0x00}; static const uint8_t p3_1a[] = {0x00};
static const uint8_t p3_1b[] = {0x00}; static const uint8_t p3_1c[] = {0x00};
static const uint8_t p3_1d[] = {0x00}; static const uint8_t p3_1e[] = {0x40};
static const uint8_t p3_1f[] = {0xC0}; static const uint8_t p3_20[] = {0x06};
static const uint8_t p3_21[] = {0x01}; static const uint8_t p3_22[] = {0x06};
static const uint8_t p3_23[] = {0x01}; static const uint8_t p3_24[] = {0x88};
static const uint8_t p3_25[] = {0x88}; static const uint8_t p3_26[] = {0x00};
static const uint8_t p3_27[] = {0x00}; static const uint8_t p3_28[] = {0x3B};
static const uint8_t p3_29[] = {0x03}; static const uint8_t p3_2a[] = {0x00};
static const uint8_t p3_2b[] = {0x00}; static const uint8_t p3_2c[] = {0x00};
static const uint8_t p3_2d[] = {0x00}; static const uint8_t p3_2e[] = {0x00};
static const uint8_t p3_2f[] = {0x00}; static const uint8_t p3_30[] = {0x00};
static const uint8_t p3_31[] = {0x00}; static const uint8_t p3_32[] = {0x00};
static const uint8_t p3_33[] = {0x00}; static const uint8_t p3_34[] = {0x00};
static const uint8_t p3_35[] = {0x00}; static const uint8_t p3_36[] = {0x00};
static const uint8_t p3_37[] = {0x00}; static const uint8_t p3_38[] = {0x00};
static const uint8_t p3_39[] = {0x00}; static const uint8_t p3_3a[] = {0x00};
static const uint8_t p3_3b[] = {0x00}; static const uint8_t p3_3c[] = {0x00};
static const uint8_t p3_3d[] = {0x00}; static const uint8_t p3_3e[] = {0x00};
static const uint8_t p3_3f[] = {0x00}; static const uint8_t p3_40[] = {0x00};
static const uint8_t p3_41[] = {0x00}; static const uint8_t p3_42[] = {0x00};
static const uint8_t p3_43[] = {0x00}; static const uint8_t p3_44[] = {0x00};

static const uint8_t p0_colmod[] = {0x55};
static const uint8_t p0_tear[] = {0x00};

static const LCD_MIPI_DSI_INIT_CMD_T sg_ili9881c_init_cmds[] = {
    {0xFF, ili9881c_page3, 3, 0},
    {0x01, p3_01, 1, 0}, {0x02, p3_02, 1, 0}, {0x03, p3_03, 1, 0}, {0x04, p3_04, 1, 0},
    {0x05, p3_05, 1, 0}, {0x06, p3_06, 1, 0}, {0x07, p3_07, 1, 0}, {0x08, p3_08, 1, 0},
    {0x09, p3_09, 1, 0}, {0x0A, p3_0a, 1, 0}, {0x0F, p3_0f, 1, 0}, {0x10, p3_10, 1, 0},
    {0x11, p3_11, 1, 0}, {0x12, p3_12, 1, 0}, {0x13, p3_13, 1, 0}, {0x14, p3_14, 1, 0},
    {0x15, p3_15, 1, 0}, {0x16, p3_16, 1, 0}, {0x17, p3_17, 1, 0}, {0x18, p3_18, 1, 0},
    {0x19, p3_19, 1, 0}, {0x1A, p3_1a, 1, 0}, {0x1B, p3_1b, 1, 0}, {0x1C, p3_1c, 1, 0},
    {0x1D, p3_1d, 1, 0}, {0x1E, p3_1e, 1, 0}, {0x1F, p3_1f, 1, 0}, {0x20, p3_20, 1, 0},
    {0x21, p3_21, 1, 0}, {0x22, p3_22, 1, 0}, {0x23, p3_23, 1, 0}, {0x24, p3_24, 1, 0},
    {0x25, p3_25, 1, 0}, {0x26, p3_26, 1, 0}, {0x27, p3_27, 1, 0}, {0x28, p3_28, 1, 0},
    {0x29, p3_29, 1, 0}, {0x2A, p3_2a, 1, 0}, {0x2B, p3_2b, 1, 0}, {0x2C, p3_2c, 1, 0},
    {0x2D, p3_2d, 1, 0}, {0x2E, p3_2e, 1, 0}, {0x2F, p3_2f, 1, 0}, {0x30, p3_30, 1, 0},
    {0x31, p3_31, 1, 0}, {0x32, p3_32, 1, 0}, {0x33, p3_33, 1, 0}, {0x34, p3_34, 1, 0},
    {0x35, p3_35, 1, 0}, {0x36, p3_36, 1, 0}, {0x37, p3_37, 1, 0}, {0x38, p3_38, 1, 0},
    {0x39, p3_39, 1, 0}, {0x3A, p3_3a, 1, 0}, {0x3B, p3_3b, 1, 0}, {0x3C, p3_3c, 1, 0},
    {0x3D, p3_3d, 1, 0}, {0x3E, p3_3e, 1, 0}, {0x3F, p3_3f, 1, 0}, {0x40, p3_40, 1, 0},
    {0x41, p3_41, 1, 0}, {0x42, p3_42, 1, 0}, {0x43, p3_43, 1, 0}, {0x44, p3_44, 1, 0},
    {0xFF, ili9881c_page4, 3, 0},
    {0xFF, ili9881c_page1, 3, 0},
    {0xFF, ili9881c_page0, 3, 0},
    {0x3A, p0_colmod, 1, 0},
    {0x35, p0_tear, 1, 0},
    {0x11, NULL, 0, 150},
    {0x29, NULL, 0, 20},
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

    TUYA_GPIO_BASE_CFG_T bl_cfg = {
        .mode   = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level  = TUYA_GPIO_LEVEL_LOW,
    };
    tkl_gpio_init(LCD_BL_IO, &bl_cfg);

    LCD_MIPI_DSI_PANEL_HW_CFG_T dsi_hw = {
        .num_data_lanes    = 2,
        .lane_bit_rate_mbps = 1000,
        .dpi_clk_mhz      = 80,
        .reset_gpio_num    = LCD_RST_IO,
        .ldo_chan          = DSI_PHY_LDO_CHAN,
        .ldo_voltage_mv   = DSI_PHY_LDO_VOLTAGE_MV,
        .timings = {
            .hsync_pulse_width = 40,
            .hsync_back_porch  = 140,
            .hsync_front_porch = 40,
            .vsync_pulse_width = 4,
            .vsync_back_porch  = 16,
            .vsync_front_porch = 16,
        },
        .init_cmds = sg_ili9881c_init_cmds,
        .init_cmds_size = sizeof(sg_ili9881c_init_cmds) / sizeof(sg_ili9881c_init_cmds[0]),
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
        .es8311_addr    = 0x18,
        .dma_desc_num   = 6,
        .dma_frame_num  = 240,
        .default_volume = 80,
    };

    return tdd_audio_8311_codec_register(AUDIO_CODEC_NAME, codec_cfg);
}

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    tkl_io_pinmux_config(I2C_SCL_IO, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(I2C_SDA_IO, TUYA_IIC0_SDA);

    TUYA_CALL_ERR_LOG(__board_register_display());
    TUYA_CALL_ERR_LOG(__board_register_audio());

    return rt;
}
