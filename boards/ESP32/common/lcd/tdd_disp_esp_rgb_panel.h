/**
 * @file tdd_disp_esp_rgb_panel.h
 * @brief One-shot init + register helper for RGB parallel-interface LCD panels via ESP-IDF esp_lcd.
 *
 * Internally configures the ESP32-S3 LCD peripheral in RGB mode using
 * esp_lcd_new_rgb_panel(), then registers the panel with TuyaOpen TDL display
 * via tdd_disp_esp_lcd_register(). Suitable for dumb RGB panels (ST7262, EK79007, etc.)
 * that need no SPI/I2C init commands.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_ESP_RGB_PANEL_H__
#define __TDD_DISP_ESP_RGB_PANEL_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_esp_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/* RGB timing parameters */
typedef struct {
    uint32_t pclk_hz;
    int      hsync_pulse_width;
    int      hsync_back_porch;
    int      hsync_front_porch;
    int      vsync_pulse_width;
    int      vsync_back_porch;
    int      vsync_front_porch;
    struct {
        unsigned int pclk_active_neg : 1;
        unsigned int de_idle_high    : 1;
        unsigned int pclk_idle_high  : 1;
    } flags;
} LCD_RGB_TIMING_T;

/* Hardware configuration for an RGB panel */
typedef struct {
    int  data_width;                /* 8 or 16 */
    int  data_gpio_nums[16];       /* RGB data pins (B first, then G, then R) */
    int  hsync_gpio_num;
    int  vsync_gpio_num;
    int  de_gpio_num;
    int  pclk_gpio_num;
    int  disp_gpio_num;            /* display enable pin; -1 if unused */
    int  num_fbs;                  /* number of frame buffers (1 or 2) */
    int  bounce_buffer_size_px;    /* bounce buffer size in pixels; 0 = no bounce */
    LCD_RGB_TIMING_T timings;
} LCD_RGB_PANEL_HW_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initialise an RGB parallel LCD panel and register it as a TuyaOpen TDD display device.
 *
 * @param[in] name Device name used for later lookup (e.g. "lcd").
 * @param[in] hw   Hardware configuration (pinout / timing / frame buffer count).
 * @param[in] cfg  Generic display configuration (width / height / pixel fmt / bl).
 *
 * @return OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tdd_disp_esp_rgb_panel_register(char *name,
                                            LCD_RGB_PANEL_HW_CFG_T *hw,
                                            TDD_DISP_ESP_LCD_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ESP_RGB_PANEL_H__ */
