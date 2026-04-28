#include "app_scene_effect.h"
#include "tal_log.h"

OPERATE_RET app_scene_effect_init(void) { return OPRT_OK; }
void app_scene_effect_take_photo(void) {}
void app_scene_effect_pick_album(const char *name) { (void)name; }
void app_scene_effect_apply_style(const char *style) { (void)style; }
void app_scene_effect_prepare_voice(void) {}
void app_scene_effect_on_result(const char *album_name) { (void)album_name; }
void app_scene_effect_show_source_thumbnail(const uint8_t *jpeg, uint32_t len) { (void)jpeg; (void)len; }
