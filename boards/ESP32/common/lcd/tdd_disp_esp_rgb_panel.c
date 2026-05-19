/**
 * @file tdd_disp_esp_rgb_panel.c
 * @brief RGB parallel-interface LCD panel register helper.
 *
 * Uses ESP-IDF's esp_lcd_new_rgb_panel() to drive dumb RGB panels (ST7262, etc.)
 * via the ESP32-S3 LCD peripheral. The RGB peripheral has its own GDMA and
 * continuously refreshes the panel from a PSRAM frame buffer, so no SPI bounce
 * buffers are needed — this driver registers directly with TDL display layer,
 * bypassing the SPI-oriented tdd_disp_esp_lcd adapter.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_disp_esp_rgb_panel.h"

#include "tal_memory.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"

#include "tdl_display_driver.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "tdd_disp_esp_rgb"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    esp_lcd_panel_handle_t panel;
    TDD_DISP_ESP_LCD_CFG_T cfg;
} DISP_RGB_DEV_T;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __rgb_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_RGB_DEV_T *dev = (DISP_RGB_DEV_T *)device;
    if (NULL == dev || NULL == dev->panel) {
        return OPRT_INVALID_PARM;
    }
    esp_lcd_panel_disp_on_off(dev->panel, true);
    return OPRT_OK;
}

static OPERATE_RET __rgb_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    DISP_RGB_DEV_T *dev = (DISP_RGB_DEV_T *)device;
    if (NULL == dev || NULL == dev->panel || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    int x1 = frame_buff->x_start;
    int y1 = frame_buff->y_start;
    int x2 = frame_buff->x_start + frame_buff->width;
    int y2 = frame_buff->y_start + frame_buff->height;

    esp_lcd_panel_draw_bitmap(dev->panel, x1, y1, x2, y2, frame_buff->frame);

    if (frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }

    return OPRT_OK;
}

static OPERATE_RET __rgb_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_RGB_DEV_T *dev = (DISP_RGB_DEV_T *)device;
    if (NULL == dev || NULL == dev->panel) {
        return OPRT_INVALID_PARM;
    }
    esp_lcd_panel_disp_on_off(dev->panel, false);
    return OPRT_OK;
}

static int __lcd_rgb_panel_init(LCD_RGB_PANEL_HW_CFG_T *hw, uint16_t width, uint16_t height,
                                esp_lcd_panel_handle_t *out_panel)
{
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src            = LCD_CLK_SRC_DEFAULT,
        .data_width         = hw->data_width,
        .num_fbs            = hw->num_fbs,
        .bounce_buffer_size_px = hw->bounce_buffer_size_px,
        .psram_trans_align  = 64,
        .sram_trans_align   = 4,
        .hsync_gpio_num     = hw->hsync_gpio_num,
        .vsync_gpio_num     = hw->vsync_gpio_num,
        .de_gpio_num        = hw->de_gpio_num,
        .pclk_gpio_num      = hw->pclk_gpio_num,
        .disp_gpio_num      = hw->disp_gpio_num,
        .timings = {
            .h_res              = width,
            .v_res              = height,
            .pclk_hz            = hw->timings.pclk_hz,
            .hsync_pulse_width  = hw->timings.hsync_pulse_width,
            .hsync_back_porch   = hw->timings.hsync_back_porch,
            .hsync_front_porch  = hw->timings.hsync_front_porch,
            .vsync_pulse_width  = hw->timings.vsync_pulse_width,
            .vsync_back_porch   = hw->timings.vsync_back_porch,
            .vsync_front_porch  = hw->timings.vsync_front_porch,
            .flags = {
                .pclk_active_neg = hw->timings.flags.pclk_active_neg,
                .de_idle_high    = hw->timings.flags.de_idle_high,
                .pclk_idle_high  = hw->timings.flags.pclk_idle_high,
            },
        },
        .flags = {
            .fb_in_psram = 1,
        },
    };

    for (int i = 0; i < hw->data_width && i < 16; i++) {
        panel_config.data_gpio_nums[i] = hw->data_gpio_nums[i];
    }

    esp_err_t rt = esp_lcd_new_rgb_panel(&panel_config, out_panel);
    if (rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RGB panel: %s", esp_err_to_name(rt));
        return -1;
    }

    rt = esp_lcd_panel_reset(*out_panel);
    if (rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset panel: %s", esp_err_to_name(rt));
        return -1;
    }

    rt = esp_lcd_panel_init(*out_panel);
    if (rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init panel: %s", esp_err_to_name(rt));
        return -1;
    }

    ESP_LOGI(TAG, "RGB panel initialized: %dx%d, pclk=%luHz", width, height, hw->timings.pclk_hz);
    return 0;
}

OPERATE_RET tdd_disp_esp_rgb_panel_register(char *name,
                                            LCD_RGB_PANEL_HW_CFG_T *hw,
                                            TDD_DISP_ESP_LCD_CFG_T *cfg)
{
    if (NULL == name || NULL == hw || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    DISP_RGB_DEV_T *dev = tal_malloc(sizeof(DISP_RGB_DEV_T));
    if (NULL == dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev, 0, sizeof(DISP_RGB_DEV_T));
    memcpy(&dev->cfg, cfg, sizeof(TDD_DISP_ESP_LCD_CFG_T));

    if (__lcd_rgb_panel_init(hw, cfg->width, cfg->height, &dev->panel) != 0) {
        tal_free(dev);
        return OPRT_COM_ERROR;
    }

    TDD_DISP_DEV_INFO_T dev_info = {
        .type     = TUYA_DISPLAY_RGB,
        .width    = cfg->width,
        .height   = cfg->height,
        .fmt      = cfg->pixel_fmt,
        .rotation = cfg->rotation,
        .is_swap  = cfg->is_swap,
        .has_vram = true,
    };
    memcpy(&dev_info.bl,    &cfg->bl,    sizeof(TUYA_DISPLAY_BL_CTRL_T));
    memcpy(&dev_info.power, &cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));

    TDD_DISP_INTFS_T intfs = {
        .open  = __rgb_open,
        .flush = __rgb_flush,
        .close = __rgb_close,
    };

    OPERATE_RET rt = tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)dev, &intfs, &dev_info);
    if (rt != OPRT_OK) {
        tal_free(dev);
    }

    return rt;
}
