// screens/ui_chat.c
#include "ui_chat.h"
#include "ui_events.h"

#define CHAT_MSG_MAX_LEN 512

static lv_obj_t *sg_container   = NULL;
static lv_obj_t *sg_msg_list    = NULL;
static lv_obj_t *sg_ai_label    = NULL;
static lv_obj_t *sg_hold_btn    = NULL;
static char      sg_stream_buf[CHAT_MSG_MAX_LEN];

void ui_chat_init(lv_obj_t *parent)
{
    sg_container = lv_obj_create(parent);
    lv_obj_set_size(sg_container, lv_obj_get_width(parent), lv_obj_get_height(parent));
    lv_obj_align(sg_container, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(sg_container, lv_color_hex(0x0d0d1a), 0);
    lv_obj_set_flex_flow(sg_container, LV_FLEX_FLOW_COLUMN);

    sg_msg_list = lv_obj_create(sg_container);
    lv_obj_set_size(sg_msg_list, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(sg_msg_list, 1);
    lv_obj_set_flex_flow(sg_msg_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(sg_msg_list, lv_color_hex(0x0d0d1a), 0);
    lv_obj_set_scroll_dir(sg_msg_list, LV_DIR_VER);

    sg_hold_btn = lv_btn_create(sg_container);
    lv_obj_set_size(sg_hold_btn, lv_pct(80), 50);
    lv_obj_align(sg_hold_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(sg_hold_btn, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_add_event_cb(sg_hold_btn, btn_hold_pressed,  LV_EVENT_PRESSED,  NULL);
    lv_obj_add_event_cb(sg_hold_btn, btn_hold_released, LV_EVENT_RELEASED, NULL);
    lv_obj_t *lbl = lv_label_create(sg_hold_btn);
    lv_label_set_text(lbl, "● 按住说话");
    lv_obj_center(lbl);
}

void ui_chat_set_visible(bool visible)
{
    if (!sg_container) return;
    if (visible) lv_obj_clear_flag(sg_container, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(sg_container, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *__add_bubble(const char *text, lv_color_t bg, lv_align_t align)
{
    lv_obj_t *row = lv_obj_create(sg_msg_list);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bubble = lv_label_create(row);
    lv_label_set_text(bubble, text);
    lv_label_set_long_mode(bubble, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(bubble, lv_pct(75));
    lv_obj_set_style_bg_color(bubble, bg, 0);
    lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(bubble, 8, 0);
    lv_obj_set_style_radius(bubble, 8, 0);
    lv_obj_set_style_text_color(bubble, lv_color_white(), 0);
    lv_obj_align(bubble, align, 0, 0);
    return bubble;
}

void ui_chat_append_user(const char *msg)
{
    if (!msg) return;
    __add_bubble(msg, lv_palette_main(LV_PALETTE_BLUE), LV_ALIGN_RIGHT_MID);
    lv_obj_scroll_to_y(sg_msg_list, LV_COORD_MAX, LV_ANIM_OFF);
}

void ui_chat_append_ai(const char *msg)
{
    if (!msg) return;
    __add_bubble(msg, lv_color_hex(0x2a2a4a), LV_ALIGN_LEFT_MID);
    lv_obj_scroll_to_y(sg_msg_list, LV_COORD_MAX, LV_ANIM_OFF);
    sg_ai_label = NULL;
}

void ui_chat_stream_begin(void)
{
    sg_stream_buf[0] = '\0';
    sg_ai_label = __add_bubble("...", lv_color_hex(0x2a2a4a), LV_ALIGN_LEFT_MID);
}

void ui_chat_stream_append(const char *chunk)
{
    if (!sg_ai_label || !chunk) return;
    strncat(sg_stream_buf, chunk, CHAT_MSG_MAX_LEN - strlen(sg_stream_buf) - 1);
    lv_label_set_text(sg_ai_label, sg_stream_buf);
    lv_obj_scroll_to_y(sg_msg_list, LV_COORD_MAX, LV_ANIM_OFF);
}

void ui_chat_stream_finish(void)
{
    sg_ai_label = NULL;
    sg_stream_buf[0] = '\0';
}

void ui_chat_append_system(const char *msg)
{
    if (!msg) return;
    lv_obj_t *lbl = lv_label_create(sg_msg_list);
    lv_label_set_text(lbl, msg);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), 0);
    lv_obj_center(lbl);
}
