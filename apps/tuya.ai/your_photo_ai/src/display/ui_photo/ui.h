#ifndef __UI_H__
#define __UI_H__
#include "lvgl/lvgl.h"
#include "ai_ui_manage.h"
void ui_init(void);
void ui_destroy(void);
void ui_set_emotion(const char *emotion);
void ui_set_device_status(const char *status);
void ui_set_notification(const char *msg);
void ui_set_wifi_status(AI_UI_WIFI_STATUS_E status);
void ui_chat_set_user_msg(const char *msg);
void ui_chat_set_ai_msg(const char *msg);
void ui_chat_stream_start(void);
void ui_chat_stream_data(const char *chunk);
void ui_chat_stream_end(void);
void ui_chat_set_system_msg(const char *msg);
void ui_recognize_camera_open(void);
void ui_recognize_camera_close(void);
void ui_recognize_camera_yuv_flush(const uint8_t *yuv422, uint32_t w, uint32_t h);
void ui_recognize_set_thumbnail(const uint8_t *jpeg, uint32_t len);
void ui_recognize_set_result(const char *text);
void ui_effect_album_open(void);
void ui_effect_album_close(void);
void ui_effect_set_result_image(const uint8_t *jpeg, uint32_t len);
void ui_effect_set_source(const uint8_t *jpeg, uint32_t len);
#endif
