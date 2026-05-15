/**
 * @file waveshare_esp32s3_touch_lcd_7.c
 * @brief Board-level hardware registration for Waveshare ESP32-S3-Touch-LCD-7.
 *
 * Hardware: 7" 800x480 RGB IPS (ST7262), GT911 capacitive touch, CH422G IO expander.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"

#include "board_com_api.h"

#include "ch422g.h"
#include "tkl_pinmux.h"
#include "tdd_disp_esp_rgb_panel.h"
#include "tdd_tp_esp_gt911.h"
#include "tdl_display_manage.h"

/***********************************************************
************************macro define************************
***********************************************************/

/* I2C bus (shared by CH422G and GT911) */
#define I2C_NUM    (0)
#define I2C_SCL_IO (9)
#define I2C_SDA_IO (8)

/* CH422G IO expander pin assignments (OC pin index, bits for ch422g_set_oc_level) */
#define CH422G_PIN_TP_RST   (1 << 1) /* EXIO1: touch reset */
#define CH422G_PIN_LCD_BL   (1 << 2) /* EXIO2: LCD backlight enable */
#define CH422G_PIN_SD_CS    (1 << 4) /* EXIO4: SD card chip select */

/* GT911 touch */
#define TOUCH_INT_IO (4)
#define TOUCH_RST_IO (-1) /* reset via CH422G, not direct GPIO */

/* Display geometry */
#define DISPLAY_WIDTH    (800)
#define DISPLAY_HEIGHT   (480)

/* RGB data pins: B[3:7], G[2:7], R[3:7] mapped to D[0:15] */
#define RGB_D0  (14) /* B3 */
#define RGB_D1  (38) /* B4 */
#define RGB_D2  (18) /* B5 */
#define RGB_D3  (17) /* B6 */
#define RGB_D4  (10) /* B7 */
#define RGB_D5  (39) /* G2 */
#define RGB_D6  (0)  /* G3 */
#define RGB_D7  (45) /* G4 */
#define RGB_D8  (48) /* G5 */
#define RGB_D9  (47) /* G6 */
#define RGB_D10 (21) /* G7 */
#define RGB_D11 (1)  /* R3 */
#define RGB_D12 (2)  /* R4 */
#define RGB_D13 (42) /* R5 */
#define RGB_D14 (41) /* R6 */
#define RGB_D15 (40) /* R7 */

/* RGB sync / control */
#define RGB_HSYNC_IO (46)
#define RGB_VSYNC_IO (3)
#define RGB_DE_IO    (5)
#define RGB_PCLK_IO  (7)

/* RGB timing (ST7262 7" 800x480) */
#define RGB_PCLK_HZ            (13000000)
#define RGB_HSYNC_PULSE_WIDTH  (4)
#define RGB_HSYNC_BACK_PORCH   (8)
#define RGB_HSYNC_FRONT_PORCH  (8)
#define RGB_VSYNC_PULSE_WIDTH  (4)
#define RGB_VSYNC_BACK_PORCH   (8)
#define RGB_VSYNC_FRONT_PORCH  (8)

/***********************************************************
***********************function define**********************
***********************************************************/
static int __board_io_expander_init(void)
{
    CH422G_HW_CFG_T hw = {
        .i2c_port = I2C_NUM,
        .scl_io   = I2C_SCL_IO,
        .sda_io   = I2C_SDA_IO,
    };

    int rt = ch422g_init(&hw);
    if (rt != 0) {
        PR_ERR("ch422g_init failed");
        return rt;
    }

    /* Reset touch controller via CH422G */
    ch422g_set_oc_level(CH422G_PIN_TP_RST, 0);
    tal_system_sleep(10);
    ch422g_set_oc_level(CH422G_PIN_TP_RST, 1);
    tal_system_sleep(50);

    /* Enable LCD backlight */
    ch422g_set_oc_level(CH422G_PIN_LCD_BL, 1);

    return 0;
}

static OPERATE_RET __board_backlight_cb(uint8_t brightness, void *arg)
{
    (void)arg;
    ch422g_set_oc_level(CH422G_PIN_LCD_BL, brightness > 0 ? 1 : 0);
    return OPRT_OK;
}

static OPERATE_RET __board_register_display(void)
{
    OPERATE_RET rt = OPRT_OK;

    LCD_RGB_PANEL_HW_CFG_T rgb_hw = {
        .data_width     = 16,
        .data_gpio_nums = {
            RGB_D0,  RGB_D1,  RGB_D2,  RGB_D3,  RGB_D4,
            RGB_D5,  RGB_D6,  RGB_D7,  RGB_D8,  RGB_D9,
            RGB_D10, RGB_D11, RGB_D12, RGB_D13, RGB_D14, RGB_D15,
        },
        .hsync_gpio_num = RGB_HSYNC_IO,
        .vsync_gpio_num = RGB_VSYNC_IO,
        .de_gpio_num    = RGB_DE_IO,
        .pclk_gpio_num  = RGB_PCLK_IO,
        .disp_gpio_num  = -1,
        .num_fbs        = 1,
        .bounce_buffer_size_px = 0,
        .timings = {
            .pclk_hz            = RGB_PCLK_HZ,
            .hsync_pulse_width  = RGB_HSYNC_PULSE_WIDTH,
            .hsync_back_porch   = RGB_HSYNC_BACK_PORCH,
            .hsync_front_porch  = RGB_HSYNC_FRONT_PORCH,
            .vsync_pulse_width  = RGB_VSYNC_PULSE_WIDTH,
            .vsync_back_porch   = RGB_VSYNC_BACK_PORCH,
            .vsync_front_porch  = RGB_VSYNC_FRONT_PORCH,
            .flags = {
                .pclk_active_neg = 1,
                .de_idle_high    = 0,
                .pclk_idle_high  = 0,
            },
        },
    };

    TDD_DISP_ESP_LCD_CFG_T lcd_cfg = {
        .width     = DISPLAY_WIDTH,
        .height    = DISPLAY_HEIGHT,
        .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
        .rotation  = TUYA_DISPLAY_ROTATION_0,
        .is_swap   = 0,
    };

    TUYA_CALL_ERR_RETURN(tdd_disp_esp_rgb_panel_register(DISPLAY_NAME, &rgb_hw, &lcd_cfg));

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
    TUYA_CALL_ERR_RETURN(tdd_tp_esp_i2c_gt911_register(DISPLAY_NAME, &tp_cfg));

    return rt;
}

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* Configure I2C0 pin mapping before any I2C driver init */
    tkl_io_pinmux_config(I2C_SCL_IO, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(I2C_SDA_IO, TUYA_IIC0_SDA);

    TUYA_CALL_ERR_LOG(__board_io_expander_init());
    TUYA_CALL_ERR_LOG(__board_register_display());

    return rt;
}
