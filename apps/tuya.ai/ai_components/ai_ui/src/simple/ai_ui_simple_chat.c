/**
 * @file ai_ui_simple_chat.c
 * @brief Simple UI — image-only page with auto-print on arrival.
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_AI_CHAT_GUI_SIMPLE) && (ENABLE_AI_CHAT_GUI_SIMPLE == 1)

#include "lvgl.h"
#include "lv_vendor.h"
#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"
#include "ai_ui_image_album.h"
#include "image_album.h"
#include "lang_config.h"
#include "tal_image_jpeg_codec.h"

extern void ai_ui_simple_show_idle(void);
extern void ai_ui_simple_hide_idle(void);

#ifndef ALBUM_FILENAME_MAX_LEN
#define ALBUM_FILENAME_MAX_LEN 64
#endif

typedef struct {
    lv_obj_t   *image_page;
    lv_obj_t   *image_canvas;
    lv_obj_t   *print_status_label;
    lv_timer_t *print_result_tm;
    char        cur_img_name[ALBUM_FILENAME_MAX_LEN + 1];
} AI_UI_SIMPLE_CHAT_T;

static AI_UI_SIMPLE_CHAT_T sg_chat = {0};
static uint8_t *sg_picture_buffer = NULL;

/* ── print result overlay ── */

static void __print_result_timeout_cb(lv_timer_t *timer)
{
    (void)timer;
    lv_timer_del(sg_chat.print_result_tm);
    sg_chat.print_result_tm = NULL;
    if (sg_chat.print_status_label) {
        lv_obj_add_flag(sg_chat.print_status_label, LV_OBJ_FLAG_HIDDEN);
    }
}

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
static void __ui_disp_print_result(bool ok)
{
    if (sg_chat.print_status_label == NULL) {
        return;
    }
    lv_vendor_disp_lock();
    lv_label_set_text(sg_chat.print_status_label, ok ? PRINT_SUCCESS : PRINT_FAILED);
    lv_obj_clear_flag(sg_chat.print_status_label, LV_OBJ_FLAG_HIDDEN);
    if (sg_chat.print_result_tm == NULL) {
        sg_chat.print_result_tm = lv_timer_create(__print_result_timeout_cb, 2000, NULL);
        lv_timer_set_repeat_count(sg_chat.print_result_tm, 1);
    } else {
        lv_timer_reset(sg_chat.print_result_tm);
    }
    lv_vendor_disp_unlock();
}
#endif

/* ── image display helper (display only, no print) ── */

static void __ui_show_image(AI_UI_IMG_T *img)
{
    if (img == NULL || img->data == NULL || img->len == 0) {
        return;
    }

    TAL_IMAGE_JPEG_INFO_T info = {0};
    if (tal_image_jpeg_get_info(img->data, img->len, &info) != OPRT_OK) {
        PR_ERR("simple: jpeg get info failed");
        return;
    }

    uint32_t rgb565_size = (uint32_t)info.width * info.height * 2;
    uint8_t *rgb565_buf = (uint8_t *)Malloc(rgb565_size);
    if (rgb565_buf == NULL) {
        PR_ERR("simple: malloc rgb565 failed, size=%u", rgb565_size);
        return;
    }
    memset(rgb565_buf, 0, rgb565_size);

    TAL_IMAGE_JPEG_OUTPUT_T out = {0};
    out.out_buf      = rgb565_buf;
    out.out_buf_size = rgb565_size;
    out.out_width    = info.width;
    out.out_height   = info.height;

    if (tal_image_jpeg_decode_rgb565(img->data, img->len, &out) != OPRT_OK) {
        PR_ERR("simple: jpeg decode rgb565 failed");
        Free(rgb565_buf);
        return;
    }

    lv_vendor_disp_lock();

    if (sg_chat.image_canvas == NULL) {
        sg_chat.image_canvas = lv_canvas_create(sg_chat.image_page);
    }

    if (sg_picture_buffer != NULL) {
        Free(sg_picture_buffer);
    }
    sg_picture_buffer = rgb565_buf;

    lv_canvas_set_buffer(sg_chat.image_canvas, rgb565_buf,
                         (int32_t)info.width, (int32_t)info.height,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(sg_chat.image_canvas, (int32_t)info.width, (int32_t)info.height);
    lv_obj_center(sg_chat.image_canvas);

    lv_obj_clear_flag(sg_chat.image_page, LV_OBJ_FLAG_HIDDEN);
    ai_ui_simple_hide_idle();

    lv_vendor_disp_unlock();
}

/* ── disp_image: save name + auto-print (display may fail on raw data) ── */

static void __ui_disp_image(AI_UI_IMG_T *img)
{
    if (img == NULL || img->data == NULL || img->len == 0) {
        return;
    }

    if (img->name != NULL) {
        strncpy(sg_chat.cur_img_name, img->name, ALBUM_FILENAME_MAX_LEN);
        sg_chat.cur_img_name[ALBUM_FILENAME_MAX_LEN] = '\0';
    } else {
        sg_chat.cur_img_name[0] = '\0';
    }
    PR_NOTICE("simple: disp_image name='%s'", sg_chat.cur_img_name);

    __ui_show_image(img);

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    if (sg_chat.cur_img_name[0] != '\0') {
        PR_NOTICE("simple: auto-print '%s'", sg_chat.cur_img_name);
        lv_vendor_disp_lock();
        if (sg_chat.print_status_label) {
            lv_label_set_text(sg_chat.print_status_label, PRINTING);
            lv_obj_clear_flag(sg_chat.print_status_label, LV_OBJ_FLAG_HIDDEN);
        }
        lv_vendor_disp_unlock();
        ai_ui_notify_action(AI_UI_ACT_PRINT_IMG,
                            (uint8_t *)sg_chat.cur_img_name,
                            (uint32_t)strlen(sg_chat.cur_img_name));
    }
#endif
}

/* ── disp_link: display from album (no print, print already triggered by disp_image) ── */

static void __ui_disp_link(bool is_ai, char *text, AI_UI_CHAT_LINK_CB cb, void *cb_arg, uint32_t len)
{
    (void)is_ai;
    (void)text;
    (void)cb;
    (void)len;

    char *name = (char *)cb_arg;
    if (name == NULL) {
        return;
    }

    AI_UI_IMG_T img = {0};
    ai_ui_image_album_get_img(name, &img);
    if (img.data) {
        __ui_show_image(&img);
        image_album_free_file_data(img.data);
    }
}

/* ── chat callbacks (no-op for text, keep idle visible) ── */

static void __ui_open(void)   { /* keep idle/image as-is */ }
static void __ui_close(void)  { /* image persists, idle stays */ }

/* ── init ── */

void ai_ui_simple_chat_init(lv_obj_t *parent)
{
    const lv_font_t *text_font = ai_ui_get_text_font();

    sg_chat.image_page = lv_obj_create(parent);
    lv_obj_set_size(sg_chat.image_page, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(sg_chat.image_page, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_chat.image_page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_chat.image_page, 0, 0);
    lv_obj_set_style_radius(sg_chat.image_page, 0, 0);
    lv_obj_set_style_pad_all(sg_chat.image_page, 0, 0);
    lv_obj_set_scrollbar_mode(sg_chat.image_page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(sg_chat.image_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_chat.image_page, LV_OBJ_FLAG_HIDDEN);

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    sg_chat.print_status_label = lv_label_create(sg_chat.image_page);
    lv_obj_set_style_text_font(sg_chat.print_status_label, text_font, 0);
    lv_obj_set_style_text_color(sg_chat.print_status_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(sg_chat.print_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(sg_chat.print_status_label, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_chat.print_status_label, LV_OPA_70, 0);
    lv_obj_set_style_radius(sg_chat.print_status_label, 8, 0);
    lv_obj_set_style_pad_hor(sg_chat.print_status_label, 12, 0);
    lv_obj_set_style_pad_ver(sg_chat.print_status_label, 6, 0);
    lv_obj_align(sg_chat.print_status_label, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_label_set_text(sg_chat.print_status_label, "");
    lv_obj_add_flag(sg_chat.print_status_label, LV_OBJ_FLAG_HIDDEN);
#endif
}

void ai_ui_simple_chat_register(void)
{
    AI_UI_CHAT_INTFS_T intfs;
    memset(&intfs, 0, sizeof(AI_UI_CHAT_INTFS_T));

    intfs.disp_open  = __ui_open;
    intfs.disp_close = __ui_close;
    intfs.disp_image = __ui_disp_image;
    intfs.disp_link  = __ui_disp_link;
#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    intfs.disp_print_result = __ui_disp_print_result;
#endif

    ai_ui_chat_register(&intfs);
}

#endif /* ENABLE_AI_CHAT_GUI_SIMPLE */
