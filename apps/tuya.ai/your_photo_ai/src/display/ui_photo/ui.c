#include "ui.h"
void ui_init(void)    {}
void ui_destroy(void) {}
void ui_set_emotion(const char *e)       { (void)e; }
void ui_set_device_status(const char *s) { (void)s; }
void ui_set_notification(const char *s)  { (void)s; }
void ui_set_wifi_status(AI_UI_WIFI_STATUS_E s) { (void)s; }
void ui_chat_set_user_msg(const char *s) { (void)s; }
void ui_chat_set_ai_msg(const char *s)   { (void)s; }
void ui_chat_stream_start(void) {}
void ui_chat_stream_data(const char *s)  { (void)s; }
void ui_chat_stream_end(void) {}
void ui_chat_set_system_msg(const char *s) { (void)s; }
void ui_recognize_camera_open(void)  {}
void ui_recognize_camera_close(void) {}
void ui_recognize_set_thumbnail(const uint8_t *j, uint32_t l) { (void)j; (void)l; }
void ui_recognize_set_result(const char *t)  { (void)t; }
void ui_effect_album_open(void)  {}
void ui_effect_album_close(void) {}
void ui_effect_set_result_image(const uint8_t *j, uint32_t l) { (void)j; (void)l; }
