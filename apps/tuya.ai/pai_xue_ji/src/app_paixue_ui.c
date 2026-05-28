/**
 * @file app_paixue_ui.c
 * @brief Custom three-block UI for pai_xue_ji (photo learning device).
 *        Layout (320x480 portrait): top=camera preview, bottom=three recognition buttons.
 *        Displays AI card results as overlay cards.
 */

#include "tal_api.h"

#if defined(ENABLE_AI_CHAT_CUSTOM_UI) && (ENABLE_AI_CHAT_CUSTOM_UI == 1)

#include "lvgl.h"
#include "lv_vendor.h"
#include "cJSON.h"

#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"
#include "font_awesome_symbols.h"
#include "lang_config.h"
#include "app_paixue_ui.h"

#if defined(ENABLE_TP) && (ENABLE_TP == 1)
#define TALK_EVT_PRESS_UP         1
#define TALK_EVT_SINGLE_CLICK     2
#define TALK_EVT_LONG_PRESS_START 5
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define STATUS_BAR_HEIGHT 36
#define CAM_AREA_HEIGHT   240
#define BTN_AREA_HEIGHT   (480 - STATUS_BAR_HEIGHT - CAM_AREA_HEIGHT)
#define BTN_COUNT         3
#define CARD_WIDTH        290
#define CARD_HEIGHT       320
#define CARD_PAD          16
#define CARD_AUTO_HIDE_MS 15000

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *status_bar;
    lv_obj_t *status_label;
    lv_obj_t *emotion_label;
    lv_obj_t *network_label;
    lv_obj_t *notification_label;

    lv_obj_t *cam_area;
    lv_obj_t *cam_canvas;
    lv_obj_t *btn_panel;
    lv_obj_t *recog_btns[BTN_COUNT];

    lv_obj_t *card_overlay;
    lv_obj_t *card_panel;
    lv_obj_t *card_title;
    lv_obj_t *card_line1;
    lv_obj_t *card_line2;
    lv_obj_t *card_line3;

    lv_obj_t   *stream_label;
    bool        is_streaming;
    RECOG_MODE_E cur_mode;
} PAIXUE_UI_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static PAIXUE_UI_T sg_ui = {0};
static lv_timer_t *sg_notification_tm = NULL;
static lv_timer_t *sg_card_hide_tm = NULL;
static uint8_t    *sg_cam_rgb_buf = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

/* ── card auto-hide timer ── */
static void __card_hide_cb(lv_timer_t *timer)
{
    (void)timer;
    lv_timer_del(sg_card_hide_tm);
    sg_card_hide_tm = NULL;
    if (sg_ui.card_overlay) {
        lv_obj_add_flag(sg_ui.card_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void __card_overlay_click_cb(lv_event_t *e)
{
    (void)e;
    if (sg_card_hide_tm) {
        lv_timer_del(sg_card_hide_tm);
        sg_card_hide_tm = NULL;
    }
    lv_obj_add_flag(sg_ui.card_overlay, LV_OBJ_FLAG_HIDDEN);
}

/* ── recognition button click ── */
static void __recog_btn_cb(lv_event_t *e)
{
    int mode = (int)(intptr_t)lv_event_get_user_data(e);
    sg_ui.cur_mode = (RECOG_MODE_E)mode;

    PR_NOTICE("paixue: recog btn mode=%d", mode);

    switch (mode) {
    case RECOG_MODE_IMAGE:
        ai_ui_notify_action(AI_UI_ACT_TAKE_PHOTO, (uint8_t *)"image_recognition", 17);
        break;
    case RECOG_MODE_CHINESE:
        ai_ui_notify_action(AI_UI_ACT_TAKE_PHOTO, (uint8_t *)"chinese_recognition", 19);
        break;
    case RECOG_MODE_ENGLISH:
        ai_ui_notify_action(AI_UI_ACT_TAKE_PHOTO, (uint8_t *)"english_recognition", 19);
        break;
    default:
        break;
    }
}

/* ── notification timeout ── */
static void __notification_timeout_cb(lv_timer_t *timer)
{
    (void)timer;
    lv_timer_del(sg_notification_tm);
    sg_notification_tm = NULL;
    lv_obj_add_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.emotion_label, LV_OBJ_FLAG_HIDDEN);
}

/* ── card overlay creation (lazy) ── */
static void __ensure_card_overlay(void)
{
    if (sg_ui.card_overlay) {
        return;
    }

    const lv_font_t *text_font = ai_ui_get_text_font();

    sg_ui.card_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sg_ui.card_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(sg_ui.card_overlay, 0, 0);
    lv_obj_set_style_bg_color(sg_ui.card_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_ui.card_overlay, LV_OPA_60, 0);
    lv_obj_set_style_border_width(sg_ui.card_overlay, 0, 0);
    lv_obj_set_style_radius(sg_ui.card_overlay, 0, 0);
    lv_obj_clear_flag(sg_ui.card_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_ui.card_overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(sg_ui.card_overlay, __card_overlay_click_cb, LV_EVENT_CLICKED, NULL);

    sg_ui.card_panel = lv_obj_create(sg_ui.card_overlay);
    lv_obj_set_size(sg_ui.card_panel, CARD_WIDTH, CARD_HEIGHT);
    lv_obj_center(sg_ui.card_panel);
    lv_obj_set_style_bg_color(sg_ui.card_panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(sg_ui.card_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sg_ui.card_panel, 20, 0);
    lv_obj_set_style_border_width(sg_ui.card_panel, 0, 0);
    lv_obj_set_style_pad_all(sg_ui.card_panel, CARD_PAD, 0);
    lv_obj_set_style_shadow_width(sg_ui.card_panel, 20, 0);
    lv_obj_set_style_shadow_color(sg_ui.card_panel, lv_color_hex(0x333333), 0);
    lv_obj_set_style_shadow_opa(sg_ui.card_panel, LV_OPA_40, 0);
    lv_obj_set_flex_flow(sg_ui.card_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sg_ui.card_panel, 16, 0);
    lv_obj_clear_flag(sg_ui.card_panel, LV_OBJ_FLAG_SCROLLABLE);

    sg_ui.card_title = lv_label_create(sg_ui.card_panel);
    lv_obj_set_style_text_font(sg_ui.card_title, text_font, 0);
    lv_obj_set_style_text_color(sg_ui.card_title, lv_color_hex(0x1677FF), 0);
    lv_obj_set_width(sg_ui.card_title, LV_PCT(100));
    lv_label_set_long_mode(sg_ui.card_title, LV_LABEL_LONG_WRAP);
    lv_label_set_text(sg_ui.card_title, "");

    sg_ui.card_line1 = lv_label_create(sg_ui.card_panel);
    lv_obj_set_style_text_font(sg_ui.card_line1, text_font, 0);
    lv_obj_set_width(sg_ui.card_line1, LV_PCT(100));
    lv_label_set_long_mode(sg_ui.card_line1, LV_LABEL_LONG_WRAP);
    lv_label_set_text(sg_ui.card_line1, "");

    sg_ui.card_line2 = lv_label_create(sg_ui.card_panel);
    lv_obj_set_style_text_font(sg_ui.card_line2, text_font, 0);
    lv_obj_set_width(sg_ui.card_line2, LV_PCT(100));
    lv_label_set_long_mode(sg_ui.card_line2, LV_LABEL_LONG_WRAP);
    lv_label_set_text(sg_ui.card_line2, "");

    sg_ui.card_line3 = lv_label_create(sg_ui.card_panel);
    lv_obj_set_style_text_font(sg_ui.card_line3, text_font, 0);
    lv_obj_set_style_text_color(sg_ui.card_line3, lv_color_hex(0x666666), 0);
    lv_obj_set_width(sg_ui.card_line3, LV_PCT(100));
    lv_label_set_long_mode(sg_ui.card_line3, LV_LABEL_LONG_WRAP);
    lv_label_set_text(sg_ui.card_line3, "");
}

/* ── show a card result ── */
void app_paixue_ui_show_card(const char *json_str)
{
    if (!json_str) {
        return;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!type || !cJSON_IsString(type) || strcmp(type->valuestring, "card") != 0) {
        cJSON_Delete(root);
        return;
    }

    lv_vendor_disp_lock();
    __ensure_card_overlay();

    cJSON *name       = cJSON_GetObjectItem(root, "name");
    cJSON *word       = cJSON_GetObjectItem(root, "word");
    cJSON *pinyin     = cJSON_GetObjectItem(root, "pinyin");
    cJSON *relatedWord = cJSON_GetObjectItem(root, "relatedWord");
    cJSON *cnWord     = cJSON_GetObjectItem(root, "cnWord");

    if (name && cJSON_IsString(name)) {
        /* image_recognition card */
        char title_buf[256];
        snprintf(title_buf, sizeof(title_buf), "%s", name->valuestring);
        lv_label_set_text(sg_ui.card_title, title_buf);

        if (pinyin && cJSON_IsString(pinyin)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s: %s", RECOG_PINYIN, pinyin->valuestring);
            lv_label_set_text(sg_ui.card_line1, buf);
        } else {
            lv_label_set_text(sg_ui.card_line1, "");
        }

        if (relatedWord && cJSON_IsString(relatedWord)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s: %s", RECOG_RELATED, relatedWord->valuestring);
            lv_label_set_text(sg_ui.card_line2, buf);
        } else {
            lv_label_set_text(sg_ui.card_line2, "");
        }
        lv_label_set_text(sg_ui.card_line3, "");

    } else if (word && cJSON_IsString(word)) {
        lv_label_set_text(sg_ui.card_title, word->valuestring);

        if (pinyin && cJSON_IsString(pinyin)) {
            /* chinese_recognition card */
            char buf[256];
            snprintf(buf, sizeof(buf), "%s: %s", RECOG_PINYIN, pinyin->valuestring);
            lv_label_set_text(sg_ui.card_line1, buf);
            lv_label_set_text(sg_ui.card_line2, "");
        } else if (cnWord && cJSON_IsString(cnWord)) {
            /* english_recognition card */
            char buf[256];
            snprintf(buf, sizeof(buf), "%s: %s", RECOG_TRANSLATION, cnWord->valuestring);
            lv_label_set_text(sg_ui.card_line1, buf);
            lv_label_set_text(sg_ui.card_line2, "");
        } else {
            lv_label_set_text(sg_ui.card_line1, "");
            lv_label_set_text(sg_ui.card_line2, "");
        }
        lv_label_set_text(sg_ui.card_line3, "");
    }

    lv_obj_clear_flag(sg_ui.card_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(sg_ui.card_overlay);

    if (sg_card_hide_tm) {
        lv_timer_reset(sg_card_hide_tm);
    } else {
        sg_card_hide_tm = lv_timer_create(__card_hide_cb, CARD_AUTO_HIDE_MS, NULL);
    }

    lv_vendor_disp_unlock();
    cJSON_Delete(root);
}

/* ── try to parse card JSON from AI stream text ── */
static bool __try_parse_card(const char *text)
{
    if (!text) {
        return false;
    }

    const char *start = strchr(text, '{');
    if (!start) {
        return false;
    }

    const char *end = strrchr(text, '}');
    if (!end || end <= start) {
        return false;
    }

    size_t len = end - start + 1;
    char *json_buf = tal_malloc(len + 1);
    if (!json_buf) {
        return false;
    }
    memcpy(json_buf, start, len);
    json_buf[len] = '\0';

    cJSON *root = cJSON_Parse(json_buf);
    if (!root) {
        tal_free(json_buf);
        return false;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    bool is_card = (type && cJSON_IsString(type) && strcmp(type->valuestring, "card") == 0);
    cJSON_Delete(root);

    if (is_card) {
        app_paixue_ui_show_card(json_buf);
    }

    tal_free(json_buf);
    return is_card;
}

/* ── status bar callbacks ── */
static void __ui_set_emotion(char *emotion)
{
    if (!sg_ui.emotion_label) {
        return;
    }

    AI_UI_FONT_LIST_T sg_font;
    sg_font.emoji      = ai_ui_get_emo_font();
    sg_font.emoji_list = ai_ui_get_emo_list();

    char *emo_icon = sg_font.emoji_list[0].emo_icon;
    for (int i = 0; i < FONT_EMO_ICON_MAX_NUM; i++) {
        if (strcmp(emotion, sg_font.emoji_list[i].emo_name) == 0) {
            emo_icon = sg_font.emoji_list[i].emo_icon;
            break;
        }
    }

    lv_vendor_disp_lock();
    lv_obj_set_style_text_font(sg_ui.emotion_label, sg_font.emoji, 0);
    lv_label_set_text(sg_ui.emotion_label, emo_icon);
    lv_vendor_disp_unlock();
}

static void __ui_set_status(char *status)
{
    if (!sg_ui.status_label) {
        return;
    }
    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.status_label, status);
    lv_vendor_disp_unlock();
}

static void __ui_set_notification(char *notification)
{
    if (!sg_ui.notification_label) {
        return;
    }
    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.notification_label, notification);
    lv_obj_add_flag(sg_ui.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_ui.emotion_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);

    if (!sg_notification_tm) {
        sg_notification_tm = lv_timer_create(__notification_timeout_cb, 3000, NULL);
    } else {
        lv_timer_reset(sg_notification_tm);
    }
    lv_vendor_disp_unlock();
}

static void __ui_set_network(AI_UI_WIFI_STATUS_E wifi_status)
{
    char *wifi_icon = ai_ui_get_wifi_icon(wifi_status);
    if (!sg_ui.network_label || !wifi_icon) {
        return;
    }
    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.network_label, wifi_icon);
    lv_vendor_disp_unlock();
}

static void __ui_set_chat_mode(char *chat_mode)
{
    (void)chat_mode;
}

/* ── camera page callbacks ── */
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
static void __cam_disp_open(void)
{
    /* camera is always visible on main page, nothing to do */
}

static void __cam_disp_yuv_flush(AI_UI_VIDEO_T *video)
{
    if (!sg_ui.cam_area || !video || !video->yuv422 || video->len == 0) {
        return;
    }

    uint32_t w = video->width;
    uint32_t h = video->height;
    uint32_t rgb_size = w * h * 2;

    if (!sg_cam_rgb_buf) {
        sg_cam_rgb_buf = tal_malloc(rgb_size);
        if (!sg_cam_rgb_buf) {
            return;
        }
    }

    /* YUV422 to RGB565 conversion */
    uint8_t *yuv = video->yuv422;
    uint16_t *rgb = (uint16_t *)sg_cam_rgb_buf;
    for (uint32_t i = 0; i < w * h / 2; i++) {
        int y0 = yuv[0];
        int u  = yuv[1] - 128;
        int y1 = yuv[2];
        int v  = yuv[3] - 128;

        int r, g, b;
        r = y0 + ((v * 359) >> 8);
        g = y0 - ((u * 88 + v * 183) >> 8);
        b = y0 + ((u * 454) >> 8);
        r = (r < 0) ? 0 : (r > 255) ? 255 : r;
        g = (g < 0) ? 0 : (g > 255) ? 255 : g;
        b = (b < 0) ? 0 : (b > 255) ? 255 : b;
        rgb[0] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);

        r = y1 + ((v * 359) >> 8);
        g = y1 - ((u * 88 + v * 183) >> 8);
        b = y1 + ((u * 454) >> 8);
        r = (r < 0) ? 0 : (r > 255) ? 255 : r;
        g = (g < 0) ? 0 : (g > 255) ? 255 : g;
        b = (b < 0) ? 0 : (b > 255) ? 255 : b;
        rgb[1] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);

        yuv += 4;
        rgb += 2;
    }

    lv_vendor_disp_lock();
    if (!sg_ui.cam_canvas) {
        sg_ui.cam_canvas = lv_canvas_create(sg_ui.cam_area);
    }
    lv_canvas_set_buffer(sg_ui.cam_canvas, sg_cam_rgb_buf, w, h, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(sg_ui.cam_canvas, w, h);
    lv_obj_center(sg_ui.cam_canvas);
    lv_vendor_disp_unlock();
}

static void __cam_disp_set_thumb(uint8_t *jpeg, uint32_t len)
{
    (void)jpeg;
    (void)len;
}

static void __cam_disp_close(void)
{
    /* camera stays visible */
}
#endif

/* ── chat callbacks (for AI response text) ── */
static void __chat_disp_open(void)
{
}

static void __chat_disp_close(void)
{
}

static void __chat_disp_user_msg(char *text)
{
    (void)text;
}

static void __chat_disp_ai_msg(char *text)
{
    __try_parse_card(text);
}

static void __chat_disp_stream_start(void)
{
    sg_ui.is_streaming = true;

    lv_vendor_disp_lock();
    __ui_set_status((char *)RECOGNIZING);
    lv_vendor_disp_unlock();
}

static char sg_stream_buf[4096];
static uint32_t sg_stream_len = 0;

static void __chat_disp_stream_data(char *text)
{
    if (!text || !sg_ui.is_streaming) {
        return;
    }

    size_t add_len = strlen(text);
    if (sg_stream_len + add_len < sizeof(sg_stream_buf) - 1) {
        memcpy(sg_stream_buf + sg_stream_len, text, add_len);
        sg_stream_len += add_len;
        sg_stream_buf[sg_stream_len] = '\0';
    }
}

static void __chat_disp_stream_end(void)
{
    sg_ui.is_streaming = false;

    if (sg_stream_len > 0) {
        if (!__try_parse_card(sg_stream_buf)) {
            /* plain text AI response — show as a simple card */
            lv_vendor_disp_lock();
            __ensure_card_overlay();
            lv_label_set_text(sg_ui.card_title, RECOG_RESULT);
            lv_label_set_text(sg_ui.card_line1, sg_stream_buf);
            lv_label_set_text(sg_ui.card_line2, "");
            lv_label_set_text(sg_ui.card_line3, "");
            lv_obj_clear_flag(sg_ui.card_overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(sg_ui.card_overlay);
            if (sg_card_hide_tm) {
                lv_timer_reset(sg_card_hide_tm);
            } else {
                sg_card_hide_tm = lv_timer_create(__card_hide_cb, CARD_AUTO_HIDE_MS, NULL);
            }
            lv_vendor_disp_unlock();
        }
    }

    sg_stream_len = 0;
    sg_stream_buf[0] = '\0';

    lv_vendor_disp_lock();
    __ui_set_status((char *)STANDBY);
    lv_vendor_disp_unlock();
}

static void __chat_disp_system_msg(char *text)
{
    (void)text;
}

static void __chat_disp_image(AI_UI_IMG_T *img)
{
    (void)img;
}

static void __chat_disp_link(bool is_ai, char *text, AI_UI_CHAT_LINK_CB cb, void *cb_arg, uint32_t len)
{
    (void)is_ai;
    (void)text;
    (void)cb;
    (void)cb_arg;
    (void)len;
}

/* ── UI init ── */
static OPERATE_RET __ui_init(void)
{
    lv_vendor_init(DISPLAY_NAME);
    lv_vendor_start(5, 1024 * 8);

    lv_vendor_disp_lock();

    const lv_font_t *text_font = ai_ui_get_text_font();
    const lv_font_t *icon_font = ai_ui_get_icon_font();

    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    /* Root screen */
    sg_ui.screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sg_ui.screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(sg_ui.screen, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_pad_all(sg_ui.screen, 0, 0);
    lv_obj_set_style_border_width(sg_ui.screen, 0, 0);
    lv_obj_set_style_radius(sg_ui.screen, 0, 0);
    lv_obj_set_style_text_font(sg_ui.screen, text_font, 0);
    lv_obj_set_style_text_color(sg_ui.screen, lv_color_white(), 0);
    lv_obj_clear_flag(sg_ui.screen, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Status bar ── */
    sg_ui.status_bar = lv_obj_create(sg_ui.screen);
    lv_obj_set_size(sg_ui.status_bar, LV_HOR_RES, STATUS_BAR_HEIGHT);
    lv_obj_set_pos(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_radius(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_border_width(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_pad_all(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_bg_color(sg_ui.status_bar, lv_color_hex(0x16213E), 0);
    lv_obj_clear_flag(sg_ui.status_bar, LV_OBJ_FLAG_SCROLLABLE);

    sg_ui.emotion_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_style_text_font(sg_ui.emotion_label, icon_font, 0);
    lv_label_set_text(sg_ui.emotion_label, FONT_AWESOME_AI_CHIP);
    lv_obj_align(sg_ui.emotion_label, LV_ALIGN_LEFT_MID, 10, 0);

    sg_ui.status_label = lv_label_create(sg_ui.status_bar);
    lv_label_set_text(sg_ui.status_label, INITIALIZING);
    lv_obj_center(sg_ui.status_label);

    sg_ui.notification_label = lv_label_create(sg_ui.status_bar);
    lv_label_set_text(sg_ui.notification_label, "");
    lv_obj_center(sg_ui.notification_label);
    lv_obj_add_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);

    sg_ui.network_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_style_text_font(sg_ui.network_label, icon_font, 0);
    lv_label_set_text(sg_ui.network_label, "");
    lv_obj_align(sg_ui.network_label, LV_ALIGN_RIGHT_MID, -10, 0);

    /* ── Content area below status bar ── */

    /* Camera area (top, full width) */
    sg_ui.cam_area = lv_obj_create(sg_ui.screen);
    lv_obj_set_size(sg_ui.cam_area, LV_HOR_RES, CAM_AREA_HEIGHT);
    lv_obj_set_pos(sg_ui.cam_area, 0, STATUS_BAR_HEIGHT);
    lv_obj_set_style_bg_color(sg_ui.cam_area, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_ui.cam_area, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_ui.cam_area, 0, 0);
    lv_obj_set_style_radius(sg_ui.cam_area, 0, 0);
    lv_obj_set_style_pad_all(sg_ui.cam_area, 0, 0);
    lv_obj_clear_flag(sg_ui.cam_area, LV_OBJ_FLAG_SCROLLABLE);

    /* Bottom button panel (three buttons in a row) */
    sg_ui.btn_panel = lv_obj_create(sg_ui.screen);
    lv_obj_set_size(sg_ui.btn_panel, LV_HOR_RES, BTN_AREA_HEIGHT);
    lv_obj_set_pos(sg_ui.btn_panel, 0, STATUS_BAR_HEIGHT + CAM_AREA_HEIGHT);
    lv_obj_set_style_bg_color(sg_ui.btn_panel, lv_color_hex(0x0F3460), 0);
    lv_obj_set_style_bg_opa(sg_ui.btn_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_ui.btn_panel, 0, 0);
    lv_obj_set_style_radius(sg_ui.btn_panel, 0, 0);
    lv_obj_set_style_pad_all(sg_ui.btn_panel, 6, 0);
    lv_obj_set_style_pad_column(sg_ui.btn_panel, 6, 0);
    lv_obj_set_flex_flow(sg_ui.btn_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sg_ui.btn_panel, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(sg_ui.btn_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* Three recognition buttons (side by side) */
    static const struct {
        const char *icon;
        uint32_t    color;
    } btn_cfg[BTN_COUNT] = {
        {FONT_AWESOME_IMAGE,   0xE94560},
        {FONT_AWESOME_COMMENT, 0x0F3460},
        {FONT_AWESOME_GLOBE,   0x533483},
    };

    const char *btn_labels[BTN_COUNT] = {RECOG_ALL, RECOG_CHINESE, RECOG_ENGLISH};

    int btn_w = (LV_HOR_RES - 6 * 2 - 6 * (BTN_COUNT - 1)) / BTN_COUNT;
    int btn_h = BTN_AREA_HEIGHT - 12;

    for (int i = 0; i < BTN_COUNT; i++) {
        lv_obj_t *btn = lv_obj_create(sg_ui.btn_panel);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_set_style_bg_color(btn, lv_color_hex(btn_cfg[i].color), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(btn, 6, 0);

        lv_obj_t *icon_label = lv_label_create(btn);
        lv_obj_set_style_text_font(icon_label, icon_font, 0);
        lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);
        lv_label_set_text(icon_label, btn_cfg[i].icon);

        lv_obj_t *text_label = lv_label_create(btn);
        lv_obj_set_style_text_font(text_label, text_font, 0);
        lv_obj_set_style_text_color(text_label, lv_color_white(), 0);
        lv_label_set_text(text_label, btn_labels[i]);

        lv_obj_add_event_cb(btn, __recog_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        sg_ui.recog_btns[i] = btn;
    }

    lv_vendor_disp_unlock();

    /* Auto-open camera */
    ai_ui_notify_action(AI_UI_ACT_OPEN_CAMERA, NULL, 0);

    return OPRT_OK;
}

/* ── Register ── */
OPERATE_RET app_paixue_ui_register(void)
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

    AI_UI_CHAT_INTFS_T chat_intfs;
    memset(&chat_intfs, 0, sizeof(AI_UI_CHAT_INTFS_T));

    chat_intfs.disp_open               = __chat_disp_open;
    chat_intfs.disp_close              = __chat_disp_close;
    chat_intfs.disp_user_msg           = __chat_disp_user_msg;
    chat_intfs.disp_ai_msg             = __chat_disp_ai_msg;
    chat_intfs.disp_ai_msg_stream_start = __chat_disp_stream_start;
    chat_intfs.disp_ai_msg_stream_data = __chat_disp_stream_data;
    chat_intfs.disp_ai_msg_stream_end  = __chat_disp_stream_end;
    chat_intfs.disp_system_msg         = __chat_disp_system_msg;
    chat_intfs.disp_image              = __chat_disp_image;
    chat_intfs.disp_link               = __chat_disp_link;

    ai_ui_chat_register(&chat_intfs);

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    AI_UI_CAMERA_INTFS_T cam_intfs;
    memset(&cam_intfs, 0, sizeof(AI_UI_CAMERA_INTFS_T));

    cam_intfs.disp_open               = __cam_disp_open;
    cam_intfs.disp_yuv_flush          = __cam_disp_yuv_flush;
    cam_intfs.disp_set_thumbnail_jpeg = __cam_disp_set_thumb;
    cam_intfs.disp_close              = __cam_disp_close;

    ai_ui_camera_register(&cam_intfs);
#endif

    return OPRT_OK;
}

#endif /* ENABLE_AI_CHAT_CUSTOM_UI */
