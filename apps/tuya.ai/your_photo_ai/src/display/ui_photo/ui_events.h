#ifndef __UI_EVENTS_H__
#define __UI_EVENTS_H__
#include "lvgl/lvgl.h"

void tab_chat_clicked(lv_event_t *e);
void tab_recognize_clicked(lv_event_t *e);
void tab_effect_clicked(lv_event_t *e);

void btn_hold_pressed(lv_event_t *e);
void btn_hold_released(lv_event_t *e);

void btn_recognize_take_photo(lv_event_t *e);
void btn_recognize_pick_album(lv_event_t *e);
void btn_recognize_continue(lv_event_t *e);

void btn_effect_take_photo(lv_event_t *e);
void btn_effect_pick_album(lv_event_t *e);
void btn_effect_style_cartoon(lv_event_t *e);
void btn_effect_style_watercolor(lv_event_t *e);
void btn_effect_style_sketch(lv_event_t *e);
void btn_effect_style_oil(lv_event_t *e);
void btn_effect_hold_pressed(lv_event_t *e);
void btn_effect_hold_released(lv_event_t *e);
#endif
