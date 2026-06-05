/**
 * @file tdd_disp_esp_mipi_dsi_panel.c
 * @brief MIPI DSI panel register helper for ESP32-P4.
 *
 * Uses esp_lcd_st7701 component for panel initialization (DCS commands are
 * handled internally by the component), then registers with TuyaOpen TDL
 * display layer.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tdd_disp_esp_mipi_dsi_panel.h"

#include "tal_memory.h"
#include "tal_system.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_st7701.h"
#include "driver/gpio.h"

#include "tdl_display_driver.h"

#define TAG "tdd_disp_mipi_dsi"

typedef struct {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t dbi_io;
    esp_lcd_dsi_bus_handle_t dsi_bus;
    TDD_DISP_ESP_LCD_CFG_T cfg;
} DISP_MIPI_DSI_DEV_T;

static OPERATE_RET __dsi_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_MIPI_DSI_DEV_T *dev = (DISP_MIPI_DSI_DEV_T *)device;
    if (!dev || !dev->panel) return OPRT_INVALID_PARM;
    esp_lcd_panel_disp_on_off(dev->panel, true);
    return OPRT_OK;
}

static OPERATE_RET __dsi_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    DISP_MIPI_DSI_DEV_T *dev = (DISP_MIPI_DSI_DEV_T *)device;
    if (!dev || !dev->panel || !frame_buff) return OPRT_INVALID_PARM;

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

static OPERATE_RET __dsi_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_MIPI_DSI_DEV_T *dev = (DISP_MIPI_DSI_DEV_T *)device;
    if (!dev || !dev->panel) return OPRT_INVALID_PARM;
    esp_lcd_panel_disp_on_off(dev->panel, false);
    return OPRT_OK;
}

static int __mipi_dsi_panel_init(LCD_MIPI_DSI_PANEL_HW_CFG_T *hw,
                                  uint16_t width, uint16_t height,
                                  DISP_MIPI_DSI_DEV_T *dev)
{
    /* 1. Enable MIPI DSI PHY power via internal LDO */
    if (hw->ldo_chan > 0) {
        esp_ldo_channel_handle_t ldo_handle = NULL;
        esp_ldo_channel_config_t ldo_cfg = {
            .chan_id = hw->ldo_chan,
            .voltage_mv = hw->ldo_voltage_mv,
        };
        if (esp_ldo_acquire_channel(&ldo_cfg, &ldo_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable MIPI DSI PHY LDO");
            return -1;
        }
        ESP_LOGI(TAG, "MIPI DSI PHY LDO enabled (chan=%d, %dmV)", (int)hw->ldo_chan, (int)hw->ldo_voltage_mv);
    }

    /* 2. Create DSI bus */
    esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = 0,
        .num_data_lanes = hw->num_data_lanes,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = hw->lane_bit_rate_mbps,
    };
    ESP_LOGI(TAG, "Creating DSI bus: lanes=%d, bit_rate=%d Mbps", (int)hw->num_data_lanes, (int)hw->lane_bit_rate_mbps);
    if (esp_lcd_new_dsi_bus(&bus_cfg, &dev->dsi_bus) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create DSI bus");
        return -1;
    }
    ESP_LOGI(TAG, "DSI bus created OK");

    /* 3. Create DBI IO (for panel driver to send DCS commands) */
    esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    if (esp_lcd_new_panel_io_dbi(dev->dsi_bus, &dbi_cfg, &dev->dbi_io) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create DBI IO");
        return -1;
    }
    ESP_LOGI(TAG, "DBI IO created OK");

    /* 4. Create ST7701 panel via esp_lcd_st7701 component
     *    The component handles DCS init commands internally */
    esp_lcd_dpi_panel_config_t dpi_cfg = {
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = hw->dpi_clk_mhz,
        .virtual_channel = 0,
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .num_fbs = 1,
        .video_timing = {
            .h_size = width,
            .v_size = height,
            .hsync_pulse_width = hw->timings.hsync_pulse_width,
            .hsync_back_porch  = hw->timings.hsync_back_porch,
            .hsync_front_porch = hw->timings.hsync_front_porch,
            .vsync_pulse_width = hw->timings.vsync_pulse_width,
            .vsync_back_porch  = hw->timings.vsync_back_porch,
            .vsync_front_porch = hw->timings.vsync_front_porch,
        },
        .flags.use_dma2d = true,
    };

    st7701_vendor_config_t vendor_config = {
        .init_cmds = hw->init_cmds,
        .init_cmds_size = hw->init_cmds_size,
        .flags = {
            .use_mipi_interface = 1,
        },
        .mipi_config = {
            .dsi_bus = dev->dsi_bus,
            .dpi_config = &dpi_cfg,
        },
    };

    esp_lcd_panel_dev_config_t lcd_dev_cfg = {
        .bits_per_pixel = 16,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .reset_gpio_num = hw->reset_gpio_num,
        .vendor_config = &vendor_config,
    };

    ESP_LOGI(TAG, "Creating ST7701 panel (%dx%d, dpi_clk=%d MHz) ...", (int)width, (int)height, (int)hw->dpi_clk_mhz);
    if (esp_lcd_new_panel_st7701(dev->dbi_io, &lcd_dev_cfg, &dev->panel) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ST7701 panel");
        return -1;
    }

    ESP_LOGI(TAG, "Resetting panel ...");
    esp_lcd_panel_reset(dev->panel);

    ESP_LOGI(TAG, "Initializing panel ...");
    esp_lcd_panel_init(dev->panel);

    ESP_LOGI(TAG, "MIPI DSI ST7701 panel initialized: %dx%d, %d lanes, %d Mbps",
             (int)width, (int)height, (int)hw->num_data_lanes, (int)hw->lane_bit_rate_mbps);
    return 0;
}

OPERATE_RET tdd_disp_esp_mipi_dsi_panel_register(char *name,
                                                  LCD_MIPI_DSI_PANEL_HW_CFG_T *hw,
                                                  TDD_DISP_ESP_LCD_CFG_T *cfg)
{
    if (!name || !hw || !cfg) return OPRT_INVALID_PARM;

    DISP_MIPI_DSI_DEV_T *dev = tal_malloc(sizeof(DISP_MIPI_DSI_DEV_T));
    if (!dev) return OPRT_MALLOC_FAILED;
    memset(dev, 0, sizeof(DISP_MIPI_DSI_DEV_T));
    memcpy(&dev->cfg, cfg, sizeof(TDD_DISP_ESP_LCD_CFG_T));

    if (__mipi_dsi_panel_init(hw, cfg->width, cfg->height, dev) != 0) {
        tal_free(dev);
        return OPRT_COM_ERROR;
    }

    TDD_DISP_DEV_INFO_T dev_info = {
        .type     = TUYA_DISPLAY_MIPI,
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
        .open  = __dsi_open,
        .flush = __dsi_flush,
        .close = __dsi_close,
    };

    OPERATE_RET rt = tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)dev, &intfs, &dev_info);
    if (rt != OPRT_OK) {
        tal_free(dev);
    }
    return rt;
}
