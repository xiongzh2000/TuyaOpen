#include "ui_home.h"
#include "ui_chat.h"
#include "ui_recognize.h"
#include "ui_effect.h"
#include "ui_events.h"
#include "ai_ui_manage.h"
#include "app_photo_main.h"
#include "lv_vendor.h"
#include "tal_api.h"
#include "tal_system.h"

lv_obj_t *ui_home = NULL;

static lv_obj_t *sg_content_area  = NULL;
static lv_obj_t *sg_status_label  = NULL;
static lv_obj_t *sg_tab_btns[3]   = {NULL};
static int        sg_active_tab   = -1;

/* ---- shared camera overlay (used by both recognize and effect tabs) ---- */
#define CAM_PREV_W 240
#define CAM_PREV_H 180

static lv_obj_t *sg_cam_overlay = NULL;
static lv_obj_t *sg_cam_canvas  = NULL;
static uint8_t  *sg_cam_buf     = NULL;

/* Stride-sample UYVY YUV422 → RGB565 */
static void __yuv_to_rgb565(const uint8_t *yuv422, uint32_t src_w, uint32_t src_h,
                             uint8_t *out, uint32_t dst_w, uint32_t dst_h)
{
    uint32_t x_step = src_w / dst_w;
    uint32_t y_step = src_h / dst_h;
    if (x_step == 0) x_step = 1;
    if (y_step == 0) y_step = 1;
    uint16_t *dst = (uint16_t *)out;
    for (uint32_t dy = 0; dy < dst_h; dy++) {
        uint32_t sy = dy * y_step;
        if (sy >= src_h) sy = src_h - 1;
        const uint8_t *row = yuv422 + sy * src_w * 2; /* UYVY: 2 bytes/pixel */
        for (uint32_t dx = 0; dx < dst_w; dx++) {
            uint32_t sx   = dx * x_step;
            if (sx >= src_w) sx = src_w - 1;
            uint32_t base = (sx & ~1U) * 2; /* 4-byte UYVY block */
            uint8_t u  = row[base + 0];
            uint8_t yv = (sx & 1) ? row[base + 3] : row[base + 1];
            uint8_t v  = row[base + 2];
            int32_t c  = (int32_t)yv - 16;
            int32_t d  = (int32_t)u  - 128;
            int32_t e  = (int32_t)v  - 128;
            int32_t r  = (298 * c + 409 * e + 128) >> 8;
            int32_t g  = (298 * c - 100 * d - 208 * e + 128) >> 8;
            int32_t b  = (298 * c + 516 * d + 128) >> 8;
            r = r < 0 ? 0 : r > 255 ? 255 : r;
            g = g < 0 ? 0 : g > 255 ? 255 : g;
            b = b < 0 ? 0 : b > 255 ? 255 : b;
            *dst++ = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
        }
    }
}

static void __cam_overlay_clicked(lv_event_t *e)
{
    (void)e;
    if (sg_active_tab == 1) {
        ai_ui_notify_action((AI_UI_ACTION_E)APP_ACT_RECOGNIZE_TAKE_PHOTO, NULL, 0);
    } else if (sg_active_tab == 2) {
        ai_ui_notify_action((AI_UI_ACTION_E)APP_ACT_EFFECT_TAKE_PHOTO, NULL, 0);
    }
}

void ui_home_camera_open(void)
{
    if (!sg_content_area || sg_cam_overlay) return;

    sg_cam_buf = tal_psram_malloc(CAM_PREV_W * CAM_PREV_H * 2);
    if (!sg_cam_buf) {
        PR_ERR("camera buf psram alloc failed");
        return;
    }
    memset(sg_cam_buf, 0, CAM_PREV_W * CAM_PREV_H * 2);

    sg_cam_overlay = lv_obj_create(sg_content_area);
    lv_obj_set_size(sg_cam_overlay, lv_pct(100), lv_pct(100));
    lv_obj_align(sg_cam_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(sg_cam_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_cam_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(sg_cam_overlay, 0, 0);
    lv_obj_set_style_border_width(sg_cam_overlay, 0, 0);
    lv_obj_clear_flag(sg_cam_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(sg_cam_overlay, __cam_overlay_clicked, LV_EVENT_CLICKED, NULL);

    sg_cam_canvas = lv_canvas_create(sg_cam_overlay);
    lv_obj_set_size(sg_cam_canvas, CAM_PREV_W, CAM_PREV_H);
    lv_obj_align(sg_cam_canvas, LV_ALIGN_CENTER, 0, 0);
    lv_canvas_set_buffer(sg_cam_canvas, sg_cam_buf, CAM_PREV_W, CAM_PREV_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_clear_flag(sg_cam_canvas, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *lbl = lv_label_create(sg_cam_overlay);
    lv_label_set_text(lbl, "点击拍摄");
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
}

void ui_home_camera_close(void)
{
    if (!sg_cam_overlay) return;
    lv_obj_del(sg_cam_overlay);
    sg_cam_overlay = NULL;
    sg_cam_canvas  = NULL;
    if (sg_cam_buf) {
        tal_psram_free(sg_cam_buf);
        sg_cam_buf = NULL;
    }
}

void ui_home_camera_yuv_flush(const uint8_t *yuv422, uint32_t w, uint32_t h)
{
    if (!sg_cam_canvas || !yuv422 || w == 0 || h == 0) return;
    /* YUV conversion outside LVGL lock — avoids starving lv_timer_handler */
    __yuv_to_rgb565(yuv422, w, h, sg_cam_buf, CAM_PREV_W, CAM_PREV_H);
    /* Lock only for the LVGL API call */
    lv_vendor_disp_lock();
    if (sg_cam_canvas) lv_obj_invalidate(sg_cam_canvas);
    lv_vendor_disp_unlock();
}

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
    lv_obj_set_style_pad_all(ui_home, 0, 0);
    lv_obj_set_style_border_width(ui_home, 0, 0);

    /* Status bar (top 30px) */
    lv_obj_t *top_bar = lv_obj_create(ui_home);
    lv_obj_set_size(top_bar, LV_HOR_RES, 30);
    lv_obj_align(top_bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
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
    lv_obj_set_style_pad_all(sg_content_area, 0, 0);
    lv_obj_set_style_border_width(sg_content_area, 0, 0);
    lv_obj_clear_flag(sg_content_area, LV_OBJ_FLAG_SCROLLABLE);

    /* Tab bar (bottom 50px) */
    static const char *tab_labels[] = {"Chat", "Recog", "Effect"};
    lv_obj_t *tab_bar = lv_obj_create(ui_home);
    lv_obj_set_size(tab_bar, LV_HOR_RES, 50);
    lv_obj_align(tab_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_pad_all(tab_bar, 2, 0);
    lv_obj_set_style_border_width(tab_bar, 0, 0);
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
    if (idx == sg_active_tab) return;

    if (sg_active_tab >= 0) {
        static const int act_map[] = {
            APP_ACT_SWITCH_CHAT,
            APP_ACT_SWITCH_RECOGNIZE,
            APP_ACT_SWITCH_EFFECT,
        };
        ai_ui_notify_action((AI_UI_ACTION_E)act_map[idx], NULL, 0);
    }

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
