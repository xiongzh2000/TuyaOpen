/**
 * @file tdd_disp_esp_mipi_dsi_panel.c
 * @brief MIPI DSI panel register helper for ESP32-P4.
 *
 * Uses ESP-IDF's MIPI DSI bus + DBI (command) + DPI (video) APIs.
 * Sends vendor-specific init commands via DBI, then DPI provides continuous
 * video refresh from a PSRAM frame buffer.
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
#include "driver/gpio.h"
#include "esp_rom_sys.h"

#include "tdl_display_driver.h"

#define TAG "tdd_disp_mipi_dsi"

typedef struct {
    esp_lcd_panel_handle_t dpi_panel;
    esp_lcd_panel_io_handle_t dbi_io;
    esp_lcd_dsi_bus_handle_t dsi_bus;
    TDD_DISP_ESP_LCD_CFG_T cfg;
} DISP_MIPI_DSI_DEV_T;

static OPERATE_RET __dsi_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_MIPI_DSI_DEV_T *dev = (DISP_MIPI_DSI_DEV_T *)device;
    if (!dev || !dev->dpi_panel) return OPRT_INVALID_PARM;
    esp_lcd_panel_disp_on_off(dev->dpi_panel, true);
    return OPRT_OK;
}

static OPERATE_RET __dsi_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    DISP_MIPI_DSI_DEV_T *dev = (DISP_MIPI_DSI_DEV_T *)device;
    if (!dev || !dev->dpi_panel || !frame_buff) return OPRT_INVALID_PARM;

    int x1 = frame_buff->x_start;
    int y1 = frame_buff->y_start;
    int x2 = frame_buff->x_start + frame_buff->width;
    int y2 = frame_buff->y_start + frame_buff->height;

    esp_lcd_panel_draw_bitmap(dev->dpi_panel, x1, y1, x2, y2, frame_buff->frame);

    if (frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }
    return OPRT_OK;
}

static OPERATE_RET __dsi_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_MIPI_DSI_DEV_T *dev = (DISP_MIPI_DSI_DEV_T *)device;
    if (!dev || !dev->dpi_panel) return OPRT_INVALID_PARM;
    esp_lcd_panel_disp_on_off(dev->dpi_panel, false);
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
        ESP_LOGI(TAG, "MIPI DSI PHY LDO enabled (chan=%d, %lumV)", hw->ldo_chan, hw->ldo_voltage_mv);
    }

    /* 2. Create DSI bus */
    esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = 0,
        .num_data_lanes = hw->num_data_lanes,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = hw->lane_bit_rate_mbps,
    };
    ESP_LOGI(TAG, "Creating DSI bus: lanes=%d, bit_rate=%lu Mbps ...", hw->num_data_lanes, hw->lane_bit_rate_mbps);
    esp_err_t dsi_err = esp_lcd_new_dsi_bus(&bus_cfg, &dev->dsi_bus);
    ESP_LOGI(TAG, "esp_lcd_new_dsi_bus returned: 0x%x", dsi_err);
    if (dsi_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create DSI bus");
        return -1;
    }

    /* 3. Create DBI IO (for sending init commands) */
    esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_LOGI(TAG, "Step 3: Creating DBI IO ...");
    if (esp_lcd_new_panel_io_dbi(dev->dsi_bus, &dbi_cfg, &dev->dbi_io) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create DBI IO");
        return -1;
    }
    ESP_LOGI(TAG, "Step 3: DBI IO created OK");

    /* Wait for DSI bus to stabilize before sending DCS commands */
    esp_rom_delay_us(100 * 1000);

    /* 4. Send vendor-specific init commands */
    if (hw->init_cmds && hw->init_cmds_size > 0) {
        ESP_LOGI(TAG, "Step 4: Sending %d vendor init commands ...", hw->init_cmds_size);
        for (int i = 0; i < hw->init_cmds_size; i++) {
            const LCD_MIPI_DSI_INIT_CMD_T *cmd = &hw->init_cmds[i];
            esp_rom_delay_us(500); /* Busy-wait: keep CPU active for DSI DBI FIFO drain */
            esp_lcd_panel_io_tx_param(dev->dbi_io, cmd->cmd, cmd->data, cmd->data_len);
            if (cmd->delay_ms > 0) {
                esp_rom_delay_us(cmd->delay_ms * 1000);
            }
        }
        ESP_LOGI(TAG, "Step 4: Sent %d vendor init commands OK", hw->init_cmds_size);
    }

    /* 5. Create DPI panel (continuous video mode) */
    ESP_LOGI(TAG, "Step 5: Creating DPI panel (%dx%d, clk=%d MHz) ...", (int)width, (int)height, (int)hw->dpi_clk_mhz);
    esp_lcd_dpi_panel_config_t dpi_cfg = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = hw->dpi_clk_mhz,
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
        .flags = {
            .use_dma2d = true,
        },
    };
    esp_err_t dpi_err = esp_lcd_new_panel_dpi(dev->dsi_bus, &dpi_cfg, &dev->dpi_panel);
    ESP_LOGI(TAG, "Step 5: esp_lcd_new_panel_dpi returned: 0x%x", dpi_err);
    if (dpi_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create DPI panel");
        return -1;
    }

    /* 6. Hardware reset if GPIO specified */
    if (hw->reset_gpio_num >= 0) {
        gpio_config_t rst_cfg = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << hw->reset_gpio_num,
        };
        gpio_config(&rst_cfg);
        gpio_set_level(hw->reset_gpio_num, 0);
        tal_system_sleep(20);
        gpio_set_level(hw->reset_gpio_num, 1);
        tal_system_sleep(120);
    }

    esp_lcd_panel_init(dev->dpi_panel);

    ESP_LOGI(TAG, "MIPI DSI panel initialized: %dx%d, %d lanes, %lu Mbps",
             width, height, hw->num_data_lanes, hw->lane_bit_rate_mbps);
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
