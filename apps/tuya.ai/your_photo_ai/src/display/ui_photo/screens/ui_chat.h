#ifndef __UI_CHAT_H__
#define __UI_CHAT_H__
#include "lvgl/lvgl.h"
void ui_chat_init(lv_obj_t *parent);
void ui_chat_set_visible(bool visible);
void ui_chat_append_user(const char *msg);
void ui_chat_append_ai(const char *msg);
void ui_chat_stream_begin(void);
void ui_chat_stream_append(const char *chunk);
void ui_chat_stream_finish(void);
void ui_chat_append_system(const char *msg);
#endif
