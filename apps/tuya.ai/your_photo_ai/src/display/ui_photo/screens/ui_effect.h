#ifndef __UI_EFFECT_H__
#define __UI_EFFECT_H__
#include "lvgl/lvgl.h"
void ui_effect_init(lv_obj_t *parent);
void ui_effect_set_visible(bool visible);
void ui_effect_show_result(const uint8_t *jpeg, uint32_t len);
void ui_effect_show_source_thumbnail(const uint8_t *jpeg, uint32_t len);
#endif
