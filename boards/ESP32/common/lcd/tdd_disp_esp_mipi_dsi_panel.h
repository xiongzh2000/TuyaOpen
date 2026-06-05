/**
 * @file tdd_disp_esp_mipi_dsi_panel.h
 * @brief MIPI DSI panel register helper for ESP32-P4.
 *
 * Uses esp_lcd_st7701 component for panel initialization via MIPI DSI.
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
    const void *init_cmds;   /* Pointer to st7701_lcd_init_cmd_t array (cast in .c file) */
    uint16_t init_cmds_size;
} LCD_MIPI_DSI_PANEL_HW_CFG_T;

OPERATE_RET tdd_disp_esp_mipi_dsi_panel_register(char *name,
                                                  LCD_MIPI_DSI_PANEL_HW_CFG_T *hw,
                                                  TDD_DISP_ESP_LCD_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ESP_MIPI_DSI_PANEL_H__ */
