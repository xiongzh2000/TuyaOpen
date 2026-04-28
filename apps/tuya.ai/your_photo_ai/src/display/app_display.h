#ifndef __APP_DISPLAY_H__
#define __APP_DISPLAY_H__
#include "tuya_cloud_types.h"

OPERATE_RET app_display_init(void);

OPERATE_RET app_display_camera_start(uint16_t width, uint16_t height);
OPERATE_RET app_display_camera_flush(uint8_t *data, uint16_t width, uint16_t height);
OPERATE_RET app_display_camera_end(void);
#endif
