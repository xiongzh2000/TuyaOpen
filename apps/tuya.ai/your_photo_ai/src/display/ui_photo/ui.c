#include "ui.h"
#include "screens/ui_home.h"
#include "screens/ui_chat.h"
#include "screens/ui_recognize.h"
#include "screens/ui_effect.h"
#include "ai_ui_icon_font.h"

void ui_init(void)
{
    const lv_font_t *text_font = ai_ui_get_text_font();
    if (text_font) {
        lv_obj_set_style_text_font(lv_scr_act(), text_font, 0);
    }
    ui_home_screen_init();
    if (text_font) {
        lv_obj_set_style_text_font(ui_home, text_font, 0);
    }
    lv_disp_load_scr(ui_home);
}

void ui_destroy(void)
{
    ui_home_screen_destroy();
}

void ui_set_emotion(const char *e)             { ui_home_set_status_label(e); }
void ui_set_device_status(const char *s)       { ui_home_set_status_label(s); }
void ui_set_notification(const char *m)        { ui_home_set_status_label(m); }
void ui_set_wifi_status(AI_UI_WIFI_STATUS_E s) { ui_home_set_wifi(s); }

void ui_chat_set_user_msg(const char *m)       { ui_chat_append_user(m); }
void ui_chat_set_ai_msg(const char *m)         { ui_chat_append_ai(m); }
void ui_chat_stream_start(void)                { ui_chat_stream_begin(); }
void ui_chat_stream_data(const char *c)        { ui_chat_stream_append(c); }
void ui_chat_stream_end(void)                  { ui_chat_stream_finish(); }
void ui_chat_set_system_msg(const char *m)     { ui_chat_append_system(m); }

void ui_recognize_camera_open(void)                                           { ui_home_camera_open(); }
void ui_recognize_camera_close(void)                                          { ui_home_camera_close(); }
void ui_recognize_camera_yuv_flush(const uint8_t *yuv, uint32_t w, uint32_t h) { ui_home_camera_yuv_flush(yuv, w, h); }
void ui_recognize_set_thumbnail(const uint8_t *j, uint32_t l)                { ui_recognize_show_thumbnail(j, l); }
void ui_recognize_set_result(const char *t)                                   { ui_recognize_show_result(t); }

void ui_effect_album_open(void)  {}
void ui_effect_album_close(void) {}
void ui_effect_set_result_image(const uint8_t *j, uint32_t l)  { ui_effect_show_result(j, l); }
void ui_effect_set_source(const uint8_t *j, uint32_t l)        { ui_effect_show_source_thumbnail(j, l); }
