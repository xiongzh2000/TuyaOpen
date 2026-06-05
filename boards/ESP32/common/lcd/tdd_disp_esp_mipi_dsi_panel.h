/**
 * @file tdd_disp_esp_mipi_dsi_panel.h
 * @brief MIPI DSI panel register helper for ESP32-P4.
 *
 * Wraps ESP-IDF esp_lcd MIPI DSI bus/DBI/DPI APIs into a single registration
 * call, then registers with the TuyaOpen TDL display layer. The caller provides
 * vendor-specific init commands (DCS sequences) that are sent via the DBI
 * interface before continuous DPI video starts.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_ESP_MIPI_DSI_PANEL_H__
#define __TDD_DISP_ESP_MIPI_DSI_PANEL_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_esp_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t cmd;
    const uint8_t *data;
    uint8_t data_len;
    uint16_t delay_ms;
} LCD_MIPI_DSI_INIT_CMD_T;

typedef struct {
    uint8_t  num_data_lanes;
    uint32_t lane_bit_rate_mbps;
    uint32_t dpi_clk_mhz;
    int      reset_gpio_num;
    uint8_t  ldo_chan;
    uint32_t ldo_voltage_mv;
    struct {
        uint16_t hsync_pulse_width;
        uint16_t hsync_back_porch;
        uint16_t hsync_front_porch;
        uint16_t vsync_pulse_width;
        uint16_t vsync_back_porch;
        uint16_t vsync_front_porch;
    } timings;
    const LCD_MIPI_DSI_INIT_CMD_T *init_cmds;
    uint16_t init_cmds_size;
} LCD_MIPI_DSI_PANEL_HW_CFG_T;

/**
 * @brief Initialise a MIPI DSI LCD panel and register it as a TuyaOpen TDD display device.
 *
 * @param[in] name Device name (e.g. "lcd").
 * @param[in] hw   Hardware configuration (DSI bus, timing, vendor init commands).
 * @param[in] cfg  Display geometry, pixel format, rotation.
 *
 * @return OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tdd_disp_esp_mipi_dsi_panel_register(char *name,
                                                  LCD_MIPI_DSI_PANEL_HW_CFG_T *hw,
                                                  TDD_DISP_ESP_LCD_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ESP_MIPI_DSI_PANEL_H__ */
