#ifndef __APP_SCENE_EFFECT_H__
#define __APP_SCENE_EFFECT_H__
#include "tuya_cloud_types.h"
OPERATE_RET app_scene_effect_init(void);
void app_scene_effect_take_photo(void);
void app_scene_effect_pick_album(const char *name);
void app_scene_effect_apply_style(const char *style);
void app_scene_effect_submit_style(void *data);
void app_scene_effect_prepare_voice(void);
void app_scene_effect_on_result(const char *album_name);
void app_scene_effect_show_source_thumbnail(const uint8_t *jpeg, uint32_t len);
#endif
