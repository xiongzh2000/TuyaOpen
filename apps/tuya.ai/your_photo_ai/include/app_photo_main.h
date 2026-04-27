#ifndef __APP_PHOTO_MAIN_H__
#define __APP_PHOTO_MAIN_H__
#include "tuya_cloud_types.h"
#include "ai_ui_manage.h"

typedef enum {
    APP_ACT_SWITCH_CHAT             = AI_UI_ACT_MAX,
    APP_ACT_SWITCH_RECOGNIZE,
    APP_ACT_SWITCH_EFFECT,
    APP_ACT_RECOGNIZE_TAKE_PHOTO,
    APP_ACT_RECOGNIZE_PICK_ALBUM,
    APP_ACT_RECOGNIZE_CONTINUE_CHAT,
    APP_ACT_EFFECT_TAKE_PHOTO,
    APP_ACT_EFFECT_PICK_ALBUM,
    APP_ACT_EFFECT_APPLY_STYLE,     // data = "cartoon"/"watercolor"/"sketch"/"oil_painting"
} APP_PHOTO_UI_ACT_E;

OPERATE_RET app_photo_main_init(void);
#endif
