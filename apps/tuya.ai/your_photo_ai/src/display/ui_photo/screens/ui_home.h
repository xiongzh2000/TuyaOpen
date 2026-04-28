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
#endif
