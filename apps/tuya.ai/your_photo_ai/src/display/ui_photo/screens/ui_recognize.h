#ifndef __UI_RECOGNIZE_H__
#define __UI_RECOGNIZE_H__
#include "lvgl/lvgl.h"
void ui_recognize_init(lv_obj_t *parent);
void ui_recognize_set_visible(bool visible);
void ui_recognize_show_camera(void);
void ui_recognize_hide_camera(void);
void ui_recognize_show_thumbnail(const uint8_t *jpeg, uint32_t len);
void ui_recognize_show_result(const char *text);
#endif
