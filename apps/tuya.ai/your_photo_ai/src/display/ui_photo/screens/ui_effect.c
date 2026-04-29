#include "ui_effect.h"
#include "ui_events.h"
#include "tal_image_scale.h"

static lv_obj_t *sg_container     = NULL;
static lv_obj_t *sg_src_canvas    = NULL;
static lv_obj_t *sg_result_canvas = NULL;
static lv_obj_t *sg_loading_label = NULL;
static TAL_IMAGE_SCALE_OUT_T sg_src_out    = {0};
static TAL_IMAGE_SCALE_OUT_T sg_result_out = {0};

void ui_effect_init(lv_obj_t *parent)
{
    if (!parent) return;
    sg_container = lv_obj_create(parent);
    lv_obj_set_size(sg_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(sg_container, lv_color_hex(0x0d0d1a), 0);
    lv_obj_set_style_pad_all(sg_container, 0, 0);
    lv_obj_set_style_border_width(sg_container, 0, 0);
    lv_obj_clear_flag(sg_container, LV_OBJ_FLAG_SCROLLABLE);

    /* Top button row: camera / album */
    lv_obj_t *btn_row = lv_obj_create(sg_container);
    lv_obj_set_size(btn_row, lv_pct(100), 44);
    lv_obj_align(btn_row, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(btn_row, 0, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 2, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);

    lv_obj_t *b1 = lv_btn_create(btn_row);
    lv_obj_set_flex_grow(b1, 1);
    lv_obj_add_event_cb(b1, btn_effect_take_photo, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(b1), "拍照");

    lv_obj_t *b2 = lv_btn_create(btn_row);
    lv_obj_set_flex_grow(b2, 1);
    lv_obj_add_event_cb(b2, btn_effect_pick_album, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(b2), "相册");

    /* Source image thumbnail (top-left) */
    sg_src_canvas = lv_canvas_create(sg_container);
    lv_obj_set_size(sg_src_canvas, 120, 90);
    lv_obj_align(sg_src_canvas, LV_ALIGN_TOP_LEFT, 8, 52);

    /* Style preset buttons */
    static const char *styles[] = {"卡通", "水彩", "素描", "油画"};
    static lv_event_cb_t cbs[]  = {
        btn_effect_style_cartoon, btn_effect_style_watercolor,
        btn_effect_style_sketch,  btn_effect_style_oil
    };
    lv_obj_t *style_row = lv_obj_create(sg_container);
    lv_obj_set_size(style_row, lv_pct(100), 44);
    lv_obj_align(style_row, LV_ALIGN_TOP_LEFT, 0, 150);
    lv_obj_set_style_bg_opa(style_row, 0, 0);
    lv_obj_set_style_border_width(style_row, 0, 0);
    lv_obj_set_style_pad_all(style_row, 2, 0);
    lv_obj_set_flex_flow(style_row, LV_FLEX_FLOW_ROW);

    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_btn_create(style_row);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_height(btn, 40);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x333355), 0);
        lv_obj_add_event_cb(btn, cbs[i], LV_EVENT_CLICKED, NULL);
        lv_label_set_text(lv_label_create(btn), styles[i]);
    }

    /* HOLD voice button */
    lv_obj_t *hold_btn = lv_btn_create(sg_container);
    lv_obj_set_size(hold_btn, lv_pct(80), 42);
    lv_obj_align(hold_btn, LV_ALIGN_TOP_MID, 0, 202);
    lv_obj_add_event_cb(hold_btn, btn_effect_hold_pressed,  LV_EVENT_PRESSED,  NULL);
    lv_obj_add_event_cb(hold_btn, btn_effect_hold_released, LV_EVENT_RELEASED, NULL);
    lv_label_set_text(lv_label_create(hold_btn), "● 按住描述效果");

    /* Loading/status label */
    sg_loading_label = lv_label_create(sg_container);
    lv_label_set_text(sg_loading_label, "");
    lv_obj_align(sg_loading_label, LV_ALIGN_TOP_MID, 0, 252);
    lv_obj_set_style_text_color(sg_loading_label, lv_color_hex(0x8888ff), 0);

    /* Result image canvas */
    sg_result_canvas = lv_canvas_create(sg_container);
    lv_obj_set_size(sg_result_canvas, 200, 200);
    lv_obj_align(sg_result_canvas, LV_ALIGN_TOP_MID, 0, 275);
}

void ui_effect_set_visible(bool visible)
{
    if (!sg_container) return;
    if (visible) lv_obj_clear_flag(sg_container, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(sg_container, LV_OBJ_FLAG_HIDDEN);
}

void ui_effect_show_result(const uint8_t *jpeg, uint32_t len)
{
    if (!jpeg || len == 0 || !sg_result_canvas) return;

    TAL_IMAGE_JPEG_SCALE_IN_T in = {
        .method     = TAL_IMAGE_SCALE_MTH_BILINEAR,
        .mode       = TAL_IMAGE_SCALE_MODE_SIZE,
        .data       = jpeg,
        .size       = len,
        .out_width  = 200,
        .out_height = 200,
    };

    if (sg_result_out.buf) {
        lv_canvas_set_buffer(sg_result_canvas, NULL, 0, 0, LV_COLOR_FORMAT_RGB565);
        tal_image_scale_buf_free(&sg_result_out);
        sg_result_out.buf = NULL;
    }

    if (OPRT_OK != tal_image_jpeg_scale_rgb565(&in, &sg_result_out)) return;
    lv_canvas_set_buffer(sg_result_canvas, sg_result_out.buf, 200, 200, LV_COLOR_FORMAT_RGB565);

    if (sg_loading_label) lv_label_set_text(sg_loading_label, "");
}

void ui_effect_show_source_thumbnail(const uint8_t *jpeg, uint32_t len)
{
    if (!jpeg || len == 0 || !sg_src_canvas) return;

    TAL_IMAGE_JPEG_SCALE_IN_T in = {
        .method     = TAL_IMAGE_SCALE_MTH_BILINEAR,
        .mode       = TAL_IMAGE_SCALE_MODE_SIZE,
        .data       = jpeg,
        .size       = len,
        .out_width  = 120,
        .out_height = 90,
    };

    if (sg_src_out.buf) {
        lv_canvas_set_buffer(sg_src_canvas, NULL, 0, 0, LV_COLOR_FORMAT_RGB565);
        tal_image_scale_buf_free(&sg_src_out);
        sg_src_out.buf = NULL;
    }

    if (OPRT_OK != tal_image_jpeg_scale_rgb565(&in, &sg_src_out)) return;
    lv_canvas_set_buffer(sg_src_canvas, sg_src_out.buf, 120, 90, LV_COLOR_FORMAT_RGB565);
}
