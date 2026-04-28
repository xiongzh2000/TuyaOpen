#include "ui_events.h"
#include "ai_ui_manage.h"
#include "app_photo_main.h"
#include <string.h>

static void __fire(int act, uint8_t *data, uint32_t len)
{
    ai_ui_notify_action((AI_UI_ACTION_E)act, data, len);
}

void tab_chat_clicked(lv_event_t *e)      { (void)e; __fire(APP_ACT_SWITCH_CHAT, NULL, 0); }
void tab_recognize_clicked(lv_event_t *e) { (void)e; __fire(APP_ACT_SWITCH_RECOGNIZE, NULL, 0); }
void tab_effect_clicked(lv_event_t *e)    { (void)e; __fire(APP_ACT_SWITCH_EFFECT, NULL, 0); }

/* HOLD pseudo-actions — handled in app_ui_action.c */
#define APP_ACT_HOLD_START (AI_UI_ACT_MAX + 100)
#define APP_ACT_HOLD_END   (AI_UI_ACT_MAX + 101)

void btn_hold_pressed(lv_event_t *e)   { (void)e; __fire(APP_ACT_HOLD_START, NULL, 0); }
void btn_hold_released(lv_event_t *e)  { (void)e; __fire(APP_ACT_HOLD_END,   NULL, 0); }

void btn_recognize_take_photo(lv_event_t *e)  { (void)e; __fire(APP_ACT_RECOGNIZE_TAKE_PHOTO, NULL, 0); }
void btn_recognize_pick_album(lv_event_t *e)  { (void)e; __fire(APP_ACT_RECOGNIZE_PICK_ALBUM, NULL, 0); }
void btn_recognize_continue(lv_event_t *e)    { (void)e; __fire(APP_ACT_RECOGNIZE_CONTINUE_CHAT, NULL, 0); }

void btn_effect_take_photo(lv_event_t *e)     { (void)e; __fire(APP_ACT_EFFECT_TAKE_PHOTO, NULL, 0); }
void btn_effect_pick_album(lv_event_t *e)     { (void)e; __fire(APP_ACT_EFFECT_PICK_ALBUM, NULL, 0); }
void btn_effect_hold_pressed(lv_event_t *e)   { (void)e; __fire(APP_ACT_HOLD_START, NULL, 0); }
void btn_effect_hold_released(lv_event_t *e)  { (void)e; __fire(APP_ACT_HOLD_END,   NULL, 0); }

static void __apply_style(const char *style)
{
    __fire(APP_ACT_EFFECT_APPLY_STYLE, (uint8_t *)style, strlen(style));
}
void btn_effect_style_cartoon(lv_event_t *e)    { (void)e; __apply_style("cartoon"); }
void btn_effect_style_watercolor(lv_event_t *e) { (void)e; __apply_style("watercolor"); }
void btn_effect_style_sketch(lv_event_t *e)     { (void)e; __apply_style("sketch"); }
void btn_effect_style_oil(lv_event_t *e)        { (void)e; __apply_style("oil_painting"); }
