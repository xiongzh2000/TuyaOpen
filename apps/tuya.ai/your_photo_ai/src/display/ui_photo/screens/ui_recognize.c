// screens/ui_recognize.c
#include "ui_recognize.h"
#include "ui_events.h"
#include "tal_api.h"
#include "tal_image_scale.h"

static lv_obj_t *sg_container      = NULL;
static lv_obj_t *sg_thumbnail_img  = NULL;
static lv_obj_t *sg_result_label   = NULL;
static lv_obj_t *sg_continue_btn   = NULL;
static lv_obj_t *sg_canvas          = NULL;
static TAL_IMAGE_SCALE_OUT_T sg_scale_out = {0};

void ui_recognize_init(lv_obj_t *parent)
{
    if (!parent) return;
    sg_container = lv_obj_create(parent);
    lv_obj_set_size(sg_container, lv_obj_get_width(parent), lv_obj_get_height(parent));
    lv_obj_set_style_bg_color(sg_container, lv_color_hex(0x0d0d1a), 0);
    lv_obj_clear_flag(sg_container, LV_OBJ_FLAG_SCROLLABLE);

    /* Top button row */
    lv_obj_t *btn_row = lv_obj_create(sg_container);
    lv_obj_set_size(btn_row, lv_pct(100), 44);
    lv_obj_align(btn_row, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(btn_row, 0, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);

    lv_obj_t *btn_cam = lv_btn_create(btn_row);
    lv_obj_set_flex_grow(btn_cam, 1);
    lv_obj_add_event_cb(btn_cam, btn_recognize_take_photo, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btn_cam), "拍照");

    lv_obj_t *btn_album = lv_btn_create(btn_row);
    lv_obj_set_flex_grow(btn_album, 1);
    lv_obj_add_event_cb(btn_album, btn_recognize_pick_album, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btn_album), "相册");

    /* Thumbnail placeholder */
    sg_thumbnail_img = lv_img_create(sg_container);
    lv_obj_set_size(sg_thumbnail_img, 160, 120);
    lv_obj_align(sg_thumbnail_img, LV_ALIGN_TOP_MID, 0, 50);

    /* Result label */
    sg_result_label = lv_label_create(sg_container);
    lv_label_set_long_mode(sg_result_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(sg_result_label, lv_pct(90));
    lv_obj_align(sg_result_label, LV_ALIGN_TOP_MID, 0, 180);
    lv_label_set_text(sg_result_label, "拍照或选图后，AI 将为您介绍图中物品");
    lv_obj_set_style_text_color(sg_result_label, lv_color_hex(0xaaaaaa), 0);

    /* Continue chat button (hidden until result arrives) */
    sg_continue_btn = lv_btn_create(sg_container);
    lv_obj_align(sg_continue_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_size(sg_continue_btn, lv_pct(70), 42);
    lv_obj_add_flag(sg_continue_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(sg_continue_btn, btn_recognize_continue, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(sg_continue_btn);
    lv_label_set_text(lbl, "继续聊聊这张图 →");
    lv_obj_center(lbl);
}

void ui_recognize_set_visible(bool visible)
{
    if (!sg_container) return;
    if (visible) lv_obj_clear_flag(sg_container, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(sg_container, LV_OBJ_FLAG_HIDDEN);
}

void ui_recognize_show_camera(void)  {}
void ui_recognize_hide_camera(void)  {}

void ui_recognize_show_thumbnail(const uint8_t *jpeg, uint32_t len)
{
    if (!jpeg || len == 0 || !sg_container) return;

    TAL_IMAGE_JPEG_SCALE_IN_T in = {
        .method = TAL_IMAGE_SCALE_MTH_BILINEAR,
        .mode = TAL_IMAGE_SCALE_MODE_SIZE,
        .data = jpeg,
        .size = len,
        .out_width = 160,
        .out_height = 120
    };

    /* Free previous buffer if exists */
    if (sg_scale_out.buf) {
        if (sg_canvas) lv_canvas_set_buffer(sg_canvas, NULL, 0, 0, LV_COLOR_FORMAT_RGB565);
        tal_image_scale_buf_free(&sg_scale_out);
        sg_scale_out.buf = NULL;
    }

    /* Decode and scale JPEG to RGB565 */
    if (OPRT_OK != tal_image_jpeg_scale_rgb565(&in, &sg_scale_out)) return;

    if (!sg_canvas) {
        sg_canvas = lv_canvas_create(sg_container);
        lv_obj_set_size(sg_canvas, 160, 120);
        lv_obj_align(sg_canvas, LV_ALIGN_TOP_MID, 0, 50);
    }
    lv_canvas_set_buffer(sg_canvas, sg_scale_out.buf, 160, 120, LV_COLOR_FORMAT_RGB565);
    if (sg_thumbnail_img) lv_obj_add_flag(sg_thumbnail_img, LV_OBJ_FLAG_HIDDEN);
}

void ui_recognize_show_result(const char *text)
{
    if (!text || !sg_result_label) return;
    lv_label_set_text(sg_result_label, text);
    lv_obj_set_style_text_color(sg_result_label, lv_color_white(), 0);
    if (sg_continue_btn) lv_obj_clear_flag(sg_continue_btn, LV_OBJ_FLAG_HIDDEN);
}
