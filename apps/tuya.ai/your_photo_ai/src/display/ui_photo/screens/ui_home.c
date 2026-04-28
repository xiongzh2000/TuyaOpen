#include "ui_home.h"
#include "ui_chat.h"
#include "ui_recognize.h"
#include "ui_effect.h"
#include "ui_events.h"

lv_obj_t *ui_home = NULL;

static lv_obj_t *sg_content_area  = NULL;
static lv_obj_t *sg_status_label  = NULL;
static lv_obj_t *sg_tab_btns[3]   = {NULL};
static int        sg_active_tab   = 0;

static void __tab_click(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    ui_home_switch_tab(idx);
}

void ui_home_screen_init(void)
{
    ui_home = lv_obj_create(NULL);
    lv_obj_set_size(ui_home, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(ui_home, lv_color_black(), 0);

    /* Status bar (top 30px) */
    lv_obj_t *top_bar = lv_obj_create(ui_home);
    lv_obj_set_size(top_bar, LV_HOR_RES, 30);
    lv_obj_align(top_bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x1a1a2e), 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    sg_status_label = lv_label_create(top_bar);
    lv_label_set_text(sg_status_label, "Ready");
    lv_obj_align(sg_status_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(sg_status_label, lv_color_white(), 0);

    /* Content area (middle) */
    sg_content_area = lv_obj_create(ui_home);
    lv_obj_set_size(sg_content_area, LV_HOR_RES, LV_VER_RES - 30 - 50);
    lv_obj_align(sg_content_area, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_obj_set_style_bg_color(sg_content_area, lv_color_hex(0x0d0d1a), 0);
    lv_obj_clear_flag(sg_content_area, LV_OBJ_FLAG_SCROLLABLE);

    /* Tab bar (bottom 50px) */
    static const char *tab_labels[] = {"Chat", "Recog", "Effect"};
    lv_obj_t *tab_bar = lv_obj_create(ui_home);
    lv_obj_set_size(tab_bar, LV_HOR_RES, 50);
    lv_obj_align(tab_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_flex_flow(tab_bar, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(tab_bar, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 3; i++) {
        sg_tab_btns[i] = lv_btn_create(tab_bar);
        lv_obj_set_flex_grow(sg_tab_btns[i], 1);
        lv_obj_set_height(sg_tab_btns[i], 46);
        lv_obj_add_event_cb(sg_tab_btns[i], __tab_click, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *lbl = lv_label_create(sg_tab_btns[i]);
        lv_label_set_text(lbl, tab_labels[i]);
        lv_obj_center(lbl);
    }

    ui_chat_init(sg_content_area);
    ui_recognize_init(sg_content_area);
    ui_effect_init(sg_content_area);

    ui_home_switch_tab(0);
}

void ui_home_switch_tab(int idx)
{
    sg_active_tab = idx;
    ui_chat_set_visible(idx == 0);
    ui_recognize_set_visible(idx == 1);
    ui_effect_set_visible(idx == 2);

    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_bg_color(sg_tab_btns[i],
            (i == idx) ? lv_palette_main(LV_PALETTE_BLUE) : lv_color_hex(0x333355), 0);
    }
}

void ui_home_set_status_label(const char *text)
{
    if (sg_status_label && text) lv_label_set_text(sg_status_label, text);
}

void ui_home_set_wifi(AI_UI_WIFI_STATUS_E status)
{
    if (!sg_status_label) return;
    switch (status) {
    case AI_UI_WIFI_STATUS_GOOD: lv_label_set_text(sg_status_label, "WiFi OK");  break;
    case AI_UI_WIFI_STATUS_FAIR: lv_label_set_text(sg_status_label, "WiFi ~");   break;
    case AI_UI_WIFI_STATUS_WEAK: lv_label_set_text(sg_status_label, "WiFi Low"); break;
    default:                      lv_label_set_text(sg_status_label, "No WiFi");  break;
    }
}

void ui_home_screen_destroy(void)
{
    if (ui_home) {
        lv_obj_del(ui_home);
        ui_home = NULL;
    }
}
