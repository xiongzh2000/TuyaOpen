/**
 * @file app_badge_ui.h
 * @brief Smart badge 3-page swipeable UI interface
 */

#ifndef __APP_BADGE_UI_H__
#define __APP_BADGE_UI_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET app_badge_ui_register(void);
void app_badge_ui_album_add_jpeg(const uint8_t *data, uint32_t len);
void app_badge_ui_load_album(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_BADGE_UI_H__ */
