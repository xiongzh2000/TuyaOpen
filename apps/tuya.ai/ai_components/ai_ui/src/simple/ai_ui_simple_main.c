/**
 * @file ai_ui_simple_main.c
 * @brief Simple-style UI — main entry: LVGL init, idle screen, status text, Wi-Fi icon, sub-page register.
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_AI_CHAT_GUI_SIMPLE) && (ENABLE_AI_CHAT_GUI_SIMPLE == 1)

#include "lv_vendor.h"

#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"
#include "font_awesome_symbols.h"
#include "lang_config.h"
#include "tal_image_jpeg_codec.h"
#include "ai_audio_player.h"
#include "welcome_img.h"
#include "listen_img.h"
#include "generating_img.h"
#include "image_gen_alert.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define STATUS_TEXT_BOTTOM_OFFSET  20

/***********************************************************
***********************typedef define***********************\
***********************************************************/
typedef struct {
    lv_obj_t *idle_page;
    lv_obj_t *welcome_canvas;
    lv_obj_t *hint_label;
    lv_obj_t *status_label;
    lv_obj_t *network_label;
} AI_UI_SIMPLE_MAIN_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_UI_SIMPLE_MAIN_T sg_main = {0};
static lv_timer_t          *sg_notification_tm = NULL;
static char                 sg_saved_status[128] = {0};
static uint8_t             *sg_welcome_rgb565 = NULL;
static uint8_t             *sg_listen_rgb565 = NULL;
static uint8_t             *sg_gen_rgb565 = NULL;

/***********************************************************
***********************extern declare**********************
***********************************************************/
extern void ai_ui_simple_chat_init(lv_obj_t *parent);
extern void ai_ui_simple_chat_register(void);

/***********************************************************
***********************function define**********************\
***********************************************************/

static void __lvgl_init(void)
{
    lv_vendor_init(DISPLAY_NAME);
    lv_vendor_start(5, 1024 * 8);
}

static uint8_t *__decode_jpeg_to_rgb565(const uint8_t *jpeg, uint32_t jpeg_len)
{
    uint32_t rgb_size = 320 * 480 * 2;
    uint8_t *rgb_buf = Malloc(rgb_size);
    if (rgb_buf == NULL) {
        return NULL;
    }
    TAL_IMAGE_JPEG_OUTPUT_T out = {0};
    out.out_buf = rgb_buf;
    out.out_buf_size = rgb_size;
    out.out_width = 320;
    out.out_height = 480;
    if (tal_image_jpeg_decode_rgb565((uint8_t *)jpeg, jpeg_len, &out) != OPRT_OK) {
        Free(rgb_buf);
        return NULL;
    }
    return rgb_buf;
}

static void __switch_idle_image(uint8_t *rgb565_buf)
{
    if (sg_main.welcome_canvas == NULL || rgb565_buf == NULL) {
        return;
    }
    lv_canvas_set_buffer(sg_main.welcome_canvas, rgb565_buf, 320, 480, LV_COLOR_FORMAT_RGB565);
}

/* ── idle page show / hide (called from chat file) ── */

void ai_ui_simple_show_idle(void)
{
    if (sg_main.idle_page == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    if (sg_welcome_rgb565) {
        __switch_idle_image(sg_welcome_rgb565);
    }
    lv_obj_clear_flag(sg_main.idle_page, LV_OBJ_FLAG_HIDDEN);
    lv_vendor_disp_unlock();
}

void ai_ui_simple_hide_idle(void)
{
    if (sg_main.idle_page == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_obj_add_flag(sg_main.idle_page, LV_OBJ_FLAG_HIDDEN);
    lv_vendor_disp_unlock();
}

/* ── notification timer callback ── */

static void __ui_notification_timeout_cb(lv_timer_t *timer)
{
    lv_timer_del(sg_notification_tm);
    sg_notification_tm = NULL;

    if (sg_main.status_label == NULL) {
        return;
    }

    lv_label_set_text(sg_main.status_label, sg_saved_status);
}

/* ── AI_UI_INTFS_T callbacks ── */

static void __ui_set_emotion(char *emotion)
{
    /* Simple style has no emotion display — intentionally ignored */
    (void)emotion;
}

static void __ui_set_status(char *status)
{
    if (sg_main.status_label == NULL || status == NULL) {
        return;
    }

    strncpy(sg_saved_status, status, sizeof(sg_saved_status) - 1);
    sg_saved_status[sizeof(sg_saved_status) - 1] = '\0';

    if (sg_notification_tm != NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_main.status_label, status);

    if (strcmp(status, LISTENING) == 0 && sg_listen_rgb565) {
        __switch_idle_image(sg_listen_rgb565);
        lv_obj_clear_flag(sg_main.idle_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(sg_main.idle_page);
    } else if (strcmp(status, THINKING) == 0 && sg_gen_rgb565) {
        __switch_idle_image(sg_gen_rgb565);
        lv_obj_clear_flag(sg_main.idle_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(sg_main.idle_page);
        ai_audio_play_data(AI_AUDIO_CODEC_MP3, (uint8_t *)image_gen_alert_data, IMAGE_GEN_ALERT_SIZE);
    } else if (strcmp(status, STANDBY) == 0 && sg_welcome_rgb565) {
        __switch_idle_image(sg_welcome_rgb565);
    }

    lv_vendor_disp_unlock();
}

static void __ui_set_notification(char *notification)
{
    if (sg_main.status_label == NULL || notification == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_main.status_label, notification);

    if (NULL == sg_notification_tm) {
        sg_notification_tm = lv_timer_create(__ui_notification_timeout_cb, 3000, NULL);
    } else {
        lv_timer_reset(sg_notification_tm);
    }
    lv_vendor_disp_unlock();
}

static void __ui_set_network(AI_UI_WIFI_STATUS_E wifi_status)
{
    char *wifi_icon = ai_ui_get_wifi_icon(wifi_status);

    if (sg_main.network_label == NULL || wifi_icon == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_main.network_label, wifi_icon);
    lv_vendor_disp_unlock();
}

static void __ui_set_chat_mode(char *chat_mode)
{
    /* Simple style has no dedicated chat-mode display — intentionally ignored */
    (void)chat_mode;
}

/* ── UI init ── */

static OPERATE_RET __ui_init(void)
{
    __lvgl_init();

    lv_vendor_disp_lock();

    const lv_font_t *text_font = ai_ui_get_text_font();
    const lv_font_t *icon_font = ai_ui_get_icon_font();

    lv_obj_t *screen = lv_scr_act();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);

    /* ── Idle page ── */
    sg_main.idle_page = lv_obj_create(screen);
    lv_obj_set_size(sg_main.idle_page, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(sg_main.idle_page, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_main.idle_page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_main.idle_page, 0, 0);
    lv_obj_set_style_pad_all(sg_main.idle_page, 0, 0);
    lv_obj_set_style_radius(sg_main.idle_page, 0, 0);
    lv_obj_clear_flag(sg_main.idle_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(sg_main.idle_page, LV_SCROLLBAR_MODE_OFF);

    /* Welcome image — decode JPEG to RGB565 canvas */
    sg_welcome_rgb565 = __decode_jpeg_to_rgb565(welcome_img_data, WELCOME_IMG_SIZE);
    sg_listen_rgb565 = __decode_jpeg_to_rgb565(listen_img_data, LISTEN_IMG_SIZE);
    sg_gen_rgb565 = __decode_jpeg_to_rgb565(generating_img_data, GENERATING_IMG_SIZE);

    if (sg_welcome_rgb565) {
        sg_main.welcome_canvas = lv_canvas_create(sg_main.idle_page);
        lv_canvas_set_buffer(sg_main.welcome_canvas, sg_welcome_rgb565, 320, 480, LV_COLOR_FORMAT_RGB565);
        lv_obj_set_size(sg_main.welcome_canvas, 320, 480);
        lv_obj_center(sg_main.welcome_canvas);
    }

    /* Bottom status label */
    sg_main.status_label = lv_label_create(sg_main.idle_page);
    lv_obj_set_style_text_font(sg_main.status_label, text_font, 0);
    lv_obj_set_style_text_color(sg_main.status_label, lv_color_white(), 0);
    lv_label_set_long_mode(sg_main.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(sg_main.status_label, LV_HOR_RES - 40);
    lv_obj_set_style_text_align(sg_main.status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(sg_main.status_label, INITIALIZING);
    lv_obj_align(sg_main.status_label, LV_ALIGN_BOTTOM_MID, 0, -STATUS_TEXT_BOTTOM_OFFSET);

    /* Top-right Wi-Fi icon */
    sg_main.network_label = lv_label_create(sg_main.idle_page);
    lv_obj_set_style_text_font(sg_main.network_label, icon_font, 0);
    lv_obj_set_style_text_color(sg_main.network_label, lv_color_white(), 0);
    lv_label_set_text(sg_main.network_label, FONT_AWESOME_WIFI_OFF);
    lv_obj_align(sg_main.network_label, LV_ALIGN_TOP_RIGHT, -5, 5);

    /* Let the chat file create its own pages on the same screen */
    ai_ui_simple_chat_init(screen);

    lv_vendor_disp_unlock();

    return OPRT_OK;
}

/* ── public entry point ── */

/**
 * @brief Register simple-style chat UI implementation.
 *
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_chat_simple_register(void)
{
    AI_UI_INTFS_T intfs;
    memset(&intfs, 0, sizeof(AI_UI_INTFS_T));

    intfs.disp_init          = __ui_init;
    intfs.disp_emotion       = __ui_set_emotion;
    intfs.disp_ai_mode_state = __ui_set_status;
    intfs.disp_notification  = __ui_set_notification;
    intfs.disp_wifi_state    = __ui_set_network;
    intfs.disp_ai_chat_mode  = __ui_set_chat_mode;

    ai_ui_register(&intfs);

    ai_ui_simple_chat_register();

    return OPRT_OK;
}

#endif /* ENABLE_AI_CHAT_GUI_SIMPLE */
