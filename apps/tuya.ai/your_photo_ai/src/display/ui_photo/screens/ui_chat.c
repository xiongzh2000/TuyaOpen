#include "ui_chat.h"
void ui_chat_init(lv_obj_t *parent)          { (void)parent; }
void ui_chat_set_visible(bool visible)       { (void)visible; }
void ui_chat_append_user(const char *msg)    { (void)msg; }
void ui_chat_append_ai(const char *msg)      { (void)msg; }
void ui_chat_stream_begin(void)              {}
void ui_chat_stream_append(const char *c)    { (void)c; }
void ui_chat_stream_finish(void)             {}
void ui_chat_append_system(const char *msg)  { (void)msg; }
