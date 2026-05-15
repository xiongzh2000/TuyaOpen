/**
 * @file ch422g.h
 * @brief CH422G 12-bit I2C IO expander helper (ESP-IDF / TuyaOpen integration).
 *
 * The CH422G has 12 GPIOs: OC0-OC7 (push-pull/open-drain outputs) and
 * EXIO0-EXIO3 (bidirectional). Unlike TCA9554/XL9555, CH422G uses different
 * I2C slave addresses for each function instead of register offsets.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __CH422G_H__
#define __CH422G_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    int i2c_port;
    int scl_io;
    int sda_io;
} CH422G_HW_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initialise the CH422G expander on the given I2C bus.
 * @param[in] hw Hardware configuration; must be non-NULL on the first call.
 *               Subsequent calls return 0 immediately and ignore @p hw.
 * @return 0 on success, -1 on failure.
 */
int ch422g_init(CH422G_HW_CFG_T *hw);

/**
 * @brief Set OC output levels (pins 0-7).
 * @param[in] pin_mask Bit mask of OC pins to drive.
 * @param[in] level    0 = low, non-zero = high.
 * @return 0 on success, -1 on failure.
 */
int ch422g_set_oc_level(uint32_t pin_mask, int level);

/**
 * @brief Set EXIO output levels (pins 0-3).
 * @param[in] pin_mask Bit mask of EXIO pins (bit 0 = EXIO0, bit 3 = EXIO3).
 * @param[in] level    0 = low, non-zero = high.
 * @return 0 on success, -1 on failure.
 */
int ch422g_set_exio_level(uint32_t pin_mask, int level);

/**
 * @brief Read EXIO input levels (pins 0-3).
 * @param[in]  pin_mask Bit mask of EXIO pins to read.
 * @param[out] level    Output: per-pin level bits.
 * @return 0 on success, -1 on failure.
 */
int ch422g_get_exio_level(uint32_t pin_mask, uint32_t *level);

#ifdef __cplusplus
}
#endif

#endif /* __CH422G_H__ */
