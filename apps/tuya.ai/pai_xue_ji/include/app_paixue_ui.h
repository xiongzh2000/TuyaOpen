/**
 * @file app_paixue_ui.h
 * @brief Custom UI for pai_xue_ji (photo learning device)
 */

#ifndef __APP_PAIXUE_UI_H__
#define __APP_PAIXUE_UI_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RECOG_MODE_IMAGE,
    RECOG_MODE_CHINESE,
    RECOG_MODE_ENGLISH,
} RECOG_MODE_E;

OPERATE_RET app_paixue_ui_register(void);

void app_paixue_ui_show_card(const char *json_str);

#ifdef __cplusplus
}
#endif

#endif /* __APP_PAIXUE_UI_H__ */
