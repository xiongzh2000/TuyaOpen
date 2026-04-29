#ifndef __UI_HOME_H__
#define __UI_HOME_H__
#include "lvgl/lvgl.h"
#include "ai_ui_manage.h"

extern lv_obj_t *ui_home;

void ui_home_screen_init(void);
void ui_home_screen_destroy(void);
void ui_home_switch_tab(int tab_idx);
void ui_home_set_status_label(const char *text);
void ui_home_set_wifi(AI_UI_WIFI_STATUS_E status);

/* Shared camera overlay — works for both recognize and effect tabs */
void ui_home_camera_open(void);
void ui_home_camera_close(void);
void ui_home_camera_yuv_flush(const uint8_t *yuv422, uint32_t w, uint32_t h);
#endif
