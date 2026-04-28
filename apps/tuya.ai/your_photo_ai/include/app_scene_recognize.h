#ifndef __APP_SCENE_RECOGNIZE_H__
#define __APP_SCENE_RECOGNIZE_H__
#include "tuya_cloud_types.h"
OPERATE_RET app_scene_recognize_init(void);
void app_scene_recognize_take_photo(void);
void app_scene_recognize_pick_album(const char *name);
void app_scene_recognize_continue_chat(void);
#endif
