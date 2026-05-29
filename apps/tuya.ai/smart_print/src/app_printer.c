/**
 * @file app_printer.c
 * @brief app_printer module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include <string.h>

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
#include "tdl_printer_manage.h"
#include "tal_image_jpeg_codec.h"

#include "lvgl.h"

extern const uint8_t note_template_data[];
extern lv_font_t font_puhui_18_2;
#define NOTE_TEMPLATE_WIDTH   384
#define NOTE_TEMPLATE_HEIGHT  525
#define NOTE_TEMPLATE_TEXT_Y  300
#define NOTE_TEMPLATE_TEXT_H  120
#define NOTE_TEMPLATE_TEXT_X  70
#define NOTE_TEMPLATE_TEXT_W  244
#define NOTE_TEMPLATE_BPR     (NOTE_TEMPLATE_WIDTH / 8)

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
#include "image_album.h"
#include "ai_picture.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_PRINTER_HANDLE sg_printer_hdl = NULL;
static MUTEX_HANDLE       sg_print_mutex = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __printer_mutex_init(void)
{
    if (sg_print_mutex != NULL) {
        return OPRT_OK;
    }
    return tal_mutex_create_init(&sg_print_mutex);
}

static OPERATE_RET __printer_lock_acquire(void)
{
    if (sg_print_mutex == NULL) {
        __printer_mutex_init();
    }
    return tal_mutex_lock(sg_print_mutex);
}

static void __printer_lock_release(void)
{
    if (sg_print_mutex != NULL) {
        tal_mutex_unlock(sg_print_mutex);
    }
}

OPERATE_RET app_print_jpeg_img(uint8_t *jpeg, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == jpeg || 0 == len) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET lock_rt = __printer_lock_acquire();
    if (lock_rt != OPRT_OK) {
        PR_WARN("print: printer is busy, rejecting request");
        return OPRT_COM_ERROR;
    }

    /* Reject non-JPEG payloads up front. The image album can now hold both
     * JPEG and PNG (a recent feature), but the printer pipeline only knows
     * how to decode JPEG. Without this guard tal_image_jpeg_get_info()
     * just returns OPRT_INVALID_PARM (-2) which is hard to read at the
     * call site. JPEG SOI marker = 0xFF 0xD8 0xFF. */
    if (len < 3 ||
        jpeg[0] != 0xFF || jpeg[1] != 0xD8 || jpeg[2] != 0xFF) {
        PR_ERR("print: unsupported image format "
               "(magic=%02x %02x %02x, len=%u). "
               "Only JPEG is supported.",
               (len > 0) ? jpeg[0] : 0,
               (len > 1) ? jpeg[1] : 0,
               (len > 2) ? jpeg[2] : 0, len);
        __printer_lock_release();
        return OPRT_NOT_SUPPORTED;
    }

    TAL_IMAGE_JPEG_INFO_T jpeg_info = {0};
    rt = tal_image_jpeg_get_info(jpeg, len, &jpeg_info);
    if (rt != OPRT_OK) {
        PR_ERR("print: jpeg_get_info failed, rt:%d", rt);
        __printer_lock_release();
        return rt;
    }

    if (NULL == sg_printer_hdl) {
        rt = tdl_printer_find(PRINTER_NAME, &sg_printer_hdl);
        if (rt != OPRT_OK) {
            PR_ERR("print: printer_find failed, rt:%d", rt);
            __printer_lock_release();
            return rt;
        }
    }

    rt = tdl_printer_open(sg_printer_hdl, NULL);
    if (rt != OPRT_OK) {
        PR_ERR("print: printer_open failed, rt:%d", rt);
        __printer_lock_release();
        return rt;
    }

    TDL_PRINTER_DEV_INFO_T dev_info = {0};
    tdl_printer_get_dev_info(sg_printer_hdl, &dev_info);
    uint16_t print_width = (uint16_t)dev_info.dots_per_line;
    if (0 == print_width) {
        PR_ERR("print: invalid dots_per_line");
        tdl_printer_close(sg_printer_hdl);
        __printer_lock_release();
        return OPRT_INVALID_PARM;
    }

    uint16_t src_width  = (uint16_t)jpeg_info.width;
    uint16_t src_height = (uint16_t)jpeg_info.height;

    /* Decode directly to printer dimensions (scale down if image is wider) */
    uint16_t out_w = (src_width > print_width) ? print_width : src_width;
    uint16_t out_h = (src_width > print_width)
                     ? (uint16_t)((uint32_t)src_height * print_width / src_width)
                     : src_height;
    if (out_h == 0) {
        out_h = 1;
    }
    uint32_t bitmap_bytes = ((uint32_t)(out_w + 7u) / 8u) * out_h;

    uint8_t *bitmap_buf = (uint8_t *)Malloc(bitmap_bytes);
    if (NULL == bitmap_buf) {
        PR_ERR("print: malloc %u bytes for bitmap failed", bitmap_bytes);
        tdl_printer_close(sg_printer_hdl);
        __printer_lock_release();
        return OPRT_MALLOC_FAILED;
    }

    TAL_IMAGE_JPEG_OUTPUT_T out = {
        .out_buf      = bitmap_buf,
        .out_buf_size = bitmap_bytes,
        .out_width    = out_w,
        .out_height   = out_h,
    };
    rt = tal_image_jpeg_decode_bitmap(jpeg, (uint32_t)len, &out, 128);
    if (rt != OPRT_OK) {
        PR_ERR("print: jpeg_decode_bitmap failed, rt:%d", rt);
        Free(bitmap_buf);
        tdl_printer_close(sg_printer_hdl);
        __printer_lock_release();
        return rt;
    }

    tdl_printer_start(sg_printer_hdl);

    uint16_t x_offset = (print_width > out_w) ? (print_width - out_w) / 2 : 0;
    rt = tdl_printer_send_bitmap(sg_printer_hdl, x_offset, out_w, out_h, bitmap_buf);
    Free(bitmap_buf);
    if (rt != OPRT_OK) {
        PR_ERR("print: send_bitmap failed, rt:%d", rt);
    }

    tdl_printer_end(sg_printer_hdl);

    tdl_printer_close(sg_printer_hdl);

    __printer_lock_release();

    return rt;
}

OPERATE_RET app_print_text(const char *text)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == text || text[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET lock_rt = __printer_lock_acquire();
    if (lock_rt != OPRT_OK) {
        PR_WARN("print: printer is busy, rejecting request");
        return OPRT_COM_ERROR;
    }

    if (NULL == sg_printer_hdl) {
        rt = tdl_printer_find(PRINTER_NAME, &sg_printer_hdl);
        if (rt != OPRT_OK) {
            PR_ERR("print: printer_find failed, rt:%d", rt);
            __printer_lock_release();
            return rt;
        }
    }

    rt = tdl_printer_open(sg_printer_hdl, NULL);
    if (rt != OPRT_OK) {
        PR_ERR("print: printer_open failed, rt:%d", rt);
        __printer_lock_release();
        return rt;
    }

    /* 1. Copy full template to RAM */
    uint32_t mono_size = NOTE_TEMPLATE_BPR * NOTE_TEMPLATE_HEIGHT;
    uint8_t *mono_buf = (uint8_t *)tal_psram_malloc(mono_size);
    if (!mono_buf) {
        PR_ERR("print: psram alloc mono %u failed", mono_size);
        tdl_printer_close(sg_printer_hdl);
        __printer_lock_release();
        return OPRT_MALLOC_FAILED;
    }
    memcpy(mono_buf, note_template_data, mono_size);

    /* 2. Render text with LVGL v9 canvas */
    uint16_t tw = NOTE_TEMPLATE_TEXT_W;
    uint16_t th = NOTE_TEMPLATE_TEXT_H;
    uint32_t rgb_size = tw * th * 2;
    uint8_t *rgb_buf = (uint8_t *)tal_psram_malloc(rgb_size);
    if (!rgb_buf) {
        PR_ERR("print: psram alloc rgb %u failed", rgb_size);
        tal_psram_free(mono_buf);
        tdl_printer_close(sg_printer_hdl);
        __printer_lock_release();
        return OPRT_MALLOC_FAILED;
    }

    lv_obj_t *canvas = lv_canvas_create(lv_screen_active());
    lv_canvas_set_buffer(canvas, rgb_buf, tw, th, LV_COLOR_FORMAT_RGB565);
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = &font_puhui_18_2;
    label_dsc.color = lv_color_black();
    label_dsc.align = LV_TEXT_ALIGN_CENTER;
    label_dsc.text = text;

    /* Render text centered vertically */
    int text_y = (th - 22) / 2;
    lv_area_t coords = {0, text_y, tw - 1, th - 1};
    lv_draw_label(&layer, &label_dsc, &coords);

    lv_canvas_finish_layer(canvas, &layer);

    /* 3. Convert RGB565 text pixels to 1-bit, OR into template */
    uint16_t *px = (uint16_t *)rgb_buf;
    for (int y = 0; y < th; y++) {
        for (int x = 0; x < tw; x++) {
            uint16_t c = px[y * tw + x];
            uint8_t r5 = (c >> 11) & 0x1F;
            uint8_t g6 = (c >> 5) & 0x3F;
            uint8_t b5 = c & 0x1F;
            uint16_t brightness = (r5 << 3) + (g6 << 2) + (b5 << 3);
            if (brightness < 384) {
                int bx = NOTE_TEMPLATE_TEXT_X + x;
                int by = NOTE_TEMPLATE_TEXT_Y + y;
                mono_buf[by * NOTE_TEMPLATE_BPR + bx / 8] |= (0x80 >> (bx % 8));
            }
        }
    }

    lv_obj_delete(canvas);
    tal_psram_free(rgb_buf);

    /* 4. Print complete bitmap */
    uint8_t esc_init[] = {0x1B, 0x40};
    tdl_printer_send(sg_printer_hdl, esc_init, sizeof(esc_init));

    tdl_printer_send_bitmap(sg_printer_hdl, 0,
                            NOTE_TEMPLATE_WIDTH, NOTE_TEMPLATE_HEIGHT,
                            mono_buf);

    tdl_printer_paper_feed(sg_printer_hdl, 3);

    tal_psram_free(mono_buf);

    tdl_printer_end(sg_printer_hdl);
    tdl_printer_close(sg_printer_hdl);

    __printer_lock_release();

    return rt;
}

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)

OPERATE_RET app_print_img_from_album(const char *filename)
{
    OPERATE_RET rt = OPRT_OK;
    IMAGE_ALBUM_HANDLE album_hdl = image_album_find_by_name(ai_picture_get_album_name());
    if (NULL == album_hdl) {
        PR_ERR("print: album not found");
        return OPRT_INVALID_PARM;
    }

    uint8_t *jpeg_data = NULL;
    size_t   jpeg_size = 0;
    rt = image_album_read(album_hdl, filename, 0, &jpeg_data, &jpeg_size);
    if (rt != OPRT_OK || NULL == jpeg_data) {
        PR_ERR("print: image_album_read \"%s\" failed, rt:%d", filename, rt);
        return (rt != OPRT_OK) ? rt : OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_LOG(app_print_jpeg_img(jpeg_data, (uint32_t)jpeg_size));

    image_album_free_file_data(jpeg_data);
    jpeg_data = NULL;

    return rt;
}
#endif

#endif