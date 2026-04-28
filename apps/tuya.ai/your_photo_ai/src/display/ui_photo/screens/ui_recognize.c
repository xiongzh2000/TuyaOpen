#include "ui_recognize.h"
void ui_recognize_init(lv_obj_t *parent)                        { (void)parent; }
void ui_recognize_set_visible(bool v)                           { (void)v; }
void ui_recognize_show_camera(void)                             {}
void ui_recognize_hide_camera(void)                             {}
void ui_recognize_show_thumbnail(const uint8_t *j, uint32_t l) { (void)j; (void)l; }
void ui_recognize_show_result(const char *t)                    { (void)t; }
