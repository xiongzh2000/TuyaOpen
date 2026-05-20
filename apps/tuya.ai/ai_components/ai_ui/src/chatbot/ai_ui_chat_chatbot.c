/**
 * @file ai_ui_chat_chatbot.c
 * @brief Chatbot-style chat UI implementation.
 *
 * This file provides chatbot-style chat user interface implementation using LVGL,
 * including message display, emotion display, status bar, and theme support.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"

#if defined(ENABLE_AI_CHAT_GUI_CHATBOT) && (ENABLE_AI_CHAT_GUI_CHATBOT == 1)
#include "lvgl.h"
#include "lv_vendor.h"

#include "font_awesome_symbols.h"

#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"

#include "ai_ui_chat_chatbot.h"

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
#include "tal_image.h"
#endif

#if defined(CAT_FACE_UI) && (CAT_FACE_UI == 1)
#include "cat_faces.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* Theme color structure */
typedef struct {
    lv_color_t background;
    lv_color_t text;
    lv_color_t chat_background;
    lv_color_t user_bubble;
    lv_color_t assistant_bubble;
    lv_color_t system_bubble;
    lv_color_t system_text;
    lv_color_t border;
    lv_color_t low_battery;
}UI_THEME_COLORS_T;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *status_bar;
    lv_obj_t *content;
#if defined(CAT_FACE_UI) && (CAT_FACE_UI == 1)
    lv_obj_t *emotion_img;
#else
    lv_obj_t *emotion_label;
#endif
    lv_obj_t *chat_message_label;
    lv_obj_t *status_label;
    lv_obj_t *network_label;
    lv_obj_t *notification_label;
    lv_obj_t *mute_label;
    lv_obj_t *chat_mode_label;
} AI_UI_CHATBOT_T;


/***********************************************************
***********************typedef define***********************
***********************************************************/
static UI_THEME_COLORS_T sg_theme_colors;
static AI_UI_CHATBOT_T   sg_ui;
static AI_UI_FONT_LIST_T sg_font = {0};
static lv_timer_t       *sg_notification_tm = NULL;
static bool              sg_is_streaming = false;

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
static lv_obj_t   *sg_picture_canvas = NULL;
static uint8_t    *sg_picture_buffer = NULL;
static lv_timer_t *sg_picture_tm = NULL;

typedef struct {
    AI_UI_CHAT_LINK_CB cb;
    void              *cb_arg;
    uint32_t           len;
} UI_CHATBOT_LINK_DATA_T;
#endif

/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Initialize LVGL vendor.
 */
static void __lvgl_init(void)
{
    lv_vendor_init(DISPLAY_NAME);

    lv_vendor_start(5, 1024*8);
}

/**
 * @brief Initialize UI fonts.
 */
static void __ui_font_init(void)
{
    sg_font.text       = ai_ui_get_text_font();
    sg_font.icon       = ai_ui_get_icon_font();
    sg_font.emoji      = ai_ui_get_emo_font();
    sg_font.emoji_list = ai_ui_get_emo_list();
}

/**
 * @brief Initialize light theme colors.
 *
 * @param theme Pointer to the theme color structure.
 */
static void __ui_light_theme_init(UI_THEME_COLORS_T *theme)
{
    if (theme == NULL) {
        return;
    }

    theme->background = lv_color_white();
    theme->text = lv_color_black();
    theme->chat_background = lv_color_hex(0xE0E0E0);
    theme->user_bubble = lv_color_hex(0x95EC69);
    theme->assistant_bubble = lv_color_white();
    theme->system_bubble = lv_color_hex(0xE0E0E0);
    theme->system_text = lv_color_hex(0x666666);
    theme->border = lv_color_hex(0xE0E0E0);
    theme->low_battery = lv_color_black();
}

/**
 * @brief Initialize dark theme colors (unused).
 *
 * @param theme Pointer to the theme color structure.
 */
static __attribute__((unused)) void __ui_dark_theme_init(UI_THEME_COLORS_T *theme)
{
    if (theme == NULL) {
        return;
    }

    theme->background = lv_color_hex(0x121212);
    theme->text = lv_color_white();
    theme->chat_background = lv_color_hex(0x1E1E1E);
    theme->user_bubble = lv_color_hex(0x1A6C37);
    theme->assistant_bubble = lv_color_hex(0x333333);
    theme->system_bubble = lv_color_hex(0x2A2A2A);
    theme->system_text = lv_color_hex(0xAAAAAA);
    theme->border = lv_color_hex(0x333333);
    theme->low_battery = lv_color_hex(0x333333);
}

/**
 * @brief Notification timeout callback function.
 *
 * @param timer Pointer to the timer object.
 */
static void __ui_notification_timeout_cb(lv_timer_t *timer)
{
    lv_timer_del(sg_notification_tm);
    sg_notification_tm = NULL;

    lv_obj_add_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.status_label, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief Initialize chatbot-style UI.
 *
 * @return OPERATE_RET Operation result code.
 */
static OPERATE_RET __ui_init(void)
{
    __lvgl_init();

    lv_vendor_disp_lock();

    __ui_light_theme_init(&sg_theme_colors);
    __ui_font_init();

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, sg_font.text, 0);
    lv_obj_set_style_text_color(screen, sg_theme_colors.text, 0);
    lv_obj_set_style_bg_color(screen, sg_theme_colors.background, 0);

    /* Container */
    sg_ui.container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(sg_ui.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sg_ui.container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.container, 0, 0);
    lv_obj_set_style_pad_row(sg_ui.container, 0, 0);
    lv_obj_set_style_bg_color(sg_ui.container, sg_theme_colors.background, 0);
    lv_obj_set_style_border_color(sg_ui.container, sg_theme_colors.border, 0);

    // Status bar
    sg_ui.status_bar = lv_obj_create(sg_ui.container);
    lv_obj_set_size(sg_ui.status_bar, LV_HOR_RES, sg_font.text->line_height);
    lv_obj_set_style_radius(sg_ui.status_bar, 0, 0);

    // Content
    sg_ui.content = lv_obj_create(sg_ui.container);
    lv_obj_set_scrollbar_mode(sg_ui.content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(sg_ui.content, 0, 0);
    lv_obj_set_width(sg_ui.content, LV_HOR_RES);
    lv_obj_set_flex_grow(sg_ui.content, 1);
    lv_obj_set_flex_flow(sg_ui.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sg_ui.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);

    // Emotion
#if defined(CAT_FACE_UI) && (CAT_FACE_UI == 1)
    sg_ui.emotion_img = lv_image_create(sg_ui.content);
    lv_image_set_src(sg_ui.emotion_img, cat_face_get_by_emotion("NEUTRAL"));
    lv_obj_set_size(sg_ui.emotion_img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
#else
    sg_ui.emotion_label = lv_label_create(sg_ui.content);
    lv_obj_set_style_text_font(sg_ui.emotion_label, sg_font.emoji, 0);
    lv_label_set_text(sg_ui.emotion_label, sg_font.emoji_list[0].emo_icon);
#endif

    // Chat message
    sg_ui.chat_message_label = lv_label_create(sg_ui.content);
    lv_label_set_text(sg_ui.chat_message_label, "");
    lv_obj_set_width(sg_ui.chat_message_label, LV_HOR_RES * 0.9);         /* Limit width to 90% of screen width */
    lv_obj_set_height(sg_ui.chat_message_label, LV_VER_RES * 0.5);         /* Limit height to 50% of screen height */
    lv_label_set_long_mode(sg_ui.chat_message_label, LV_LABEL_LONG_WRAP); /* Set to automatic line break mode */
    lv_obj_set_style_text_align(sg_ui.chat_message_label, LV_TEXT_ALIGN_CENTER, 0); /* Set text to center alignment */
    lv_label_set_text(sg_ui.chat_message_label, "");

    /* Status bar */
    /* lv_obj_set_flex_flow(sg_ui.status_bar, LV_FLEX_FLOW_ROW); */
    lv_obj_set_style_pad_all(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_border_width(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_pad_column(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_pad_left(sg_ui.status_bar, 2, 0);
    lv_obj_set_style_bg_color(sg_ui.status_bar, sg_theme_colors.background, 0);

    sg_ui.chat_mode_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_style_text_color(sg_ui.chat_mode_label, sg_theme_colors.text, 0);
    lv_label_set_text(sg_ui.chat_mode_label, "");
    lv_obj_align(sg_ui.chat_mode_label, LV_ALIGN_LEFT_MID, 5, 0);

    // Notification label
    sg_ui.notification_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.notification_label, 1);
    lv_obj_set_style_text_align(sg_ui.notification_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sg_ui.notification_label, sg_theme_colors.text, 0);
    lv_label_set_text(sg_ui.notification_label, "");
    lv_obj_align(sg_ui.notification_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);

    // Status label
    sg_ui.status_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.status_label, 1);
    lv_label_set_long_mode(sg_ui.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(sg_ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sg_ui.status_label, sg_theme_colors.text, 0);
    lv_label_set_text(sg_ui.status_label, INITIALIZING);
    lv_obj_align(sg_ui.status_label, LV_ALIGN_CENTER, 0, 0);

    // Network status
    sg_ui.network_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_style_text_font(sg_ui.network_label, sg_font.icon, 0);
    lv_obj_set_style_text_color(sg_ui.network_label, sg_theme_colors.text, 0);
    lv_obj_align(sg_ui.network_label, LV_ALIGN_RIGHT_MID, -5, 0);

#if defined(ENABLE_CIRCLE_UI_STYLE) && (ENABLE_CIRCLE_UI_STYLE == 1)    
    lv_obj_set_style_pad_left(sg_ui.status_bar, LV_HOR_RES * 0.1, 0);
    lv_obj_set_style_pad_right(sg_ui.status_bar, LV_HOR_RES * 0.1, 0);
#endif

    lv_vendor_disp_unlock();

    return 0;
}

/**
 * @brief Set user message on UI.
 *
 * @param text Pointer to the user message text.
 */
static void __ui_set_user_msg(char *text)
{
    if (sg_ui.chat_message_label == NULL) {
        return;
    }
    
    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_message_label, text);
    lv_obj_set_style_bg_color(sg_ui.chat_message_label, sg_theme_colors.user_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.chat_message_label, sg_theme_colors.text, 0);
    lv_vendor_disp_unlock();
}

/**
 * @brief Set AI message on UI.
 *
 * @param text Pointer to the AI message text.
 */
static void __ui_set_ai_msg(char *text)
{
    if (sg_ui.chat_message_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_message_label, text);
    lv_obj_set_style_bg_color(sg_ui.chat_message_label, sg_theme_colors.assistant_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.chat_message_label, sg_theme_colors.text, 0);
    lv_vendor_disp_unlock();
}

/**
 * @brief Start AI message stream display.
 */
static void __ui_set_ai_msg_stream_start(void)
{
    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_message_label, "");
    lv_obj_set_style_bg_color(sg_ui.chat_message_label, sg_theme_colors.assistant_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.chat_message_label, sg_theme_colors.text, 0);
    lv_vendor_disp_unlock();

    sg_is_streaming = true;
}

/**
 * @brief Update AI message stream data.
 *
 * @param text Pointer to the text data to append.
 */
static void __ui_set_ai_msg_stream_data(char *text)
{
    if (sg_ui.chat_message_label == NULL || !sg_is_streaming) {
        return;
    }

    lv_vendor_disp_lock(); 
    lv_label_ins_text(sg_ui.chat_message_label, LV_LABEL_POS_LAST, text);
    lv_vendor_disp_unlock();
}

/**
 * @brief End AI message stream display.
 */
static void __ui_set_ai_msg_stream_end(void)
{
    sg_is_streaming = false;
}

/**
 * @brief Set system message on UI.
 *
 * @param text Pointer to the system message text.
 */
static void __ui_set_system_msg(char *text)
{
    if (sg_ui.chat_message_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_message_label, text);
    lv_obj_set_style_bg_color(sg_ui.chat_message_label, sg_theme_colors.system_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.chat_message_label, sg_theme_colors.system_text, 0);
    lv_vendor_disp_unlock();
}

/**
 * @brief Set emotion display on UI.
 *
 * @param emotion Pointer to the emotion name string.
 */
static void __ui_set_emotion(char *emotion)
{
#if defined(CAT_FACE_UI) && (CAT_FACE_UI == 1)
    if (NULL == sg_ui.emotion_img) {
        return;
    }

    const lv_img_dsc_t *face = cat_face_get_by_emotion(emotion);

    lv_vendor_disp_lock();
    lv_image_set_src(sg_ui.emotion_img, face);
    lv_vendor_disp_unlock();
#else
    if (NULL == sg_ui.emotion_label) {
        return;
    }

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
#endif
}

/**
 * @brief Set status text on UI.
 *
 * @param status Pointer to the status text string.
 */
static void __ui_set_status(char *status)
{
    if (sg_ui.status_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.status_label, status);
    lv_obj_set_style_text_color(sg_ui.status_label, sg_theme_colors.text, 0);
    lv_obj_set_style_text_align(sg_ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_vendor_disp_unlock();
}

/**
 * @brief Set notification text on UI.
 *
 * @param notification Pointer to the notification text string.
 */
static void __ui_set_notification(char *notification)
{
    if (sg_ui.notification_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.notification_label, notification);
    lv_obj_add_flag(sg_ui.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    if (NULL == sg_notification_tm) {
        sg_notification_tm = lv_timer_create(__ui_notification_timeout_cb, 3000, NULL);
    } else {
        lv_timer_reset(sg_notification_tm);
    }
    lv_vendor_disp_unlock();
}

/**
 * @brief Set network status icon on UI.
 *
 * @param wifi_status WiFi status (disconnected, good, fair, weak).
 */
static void __ui_set_network(AI_UI_WIFI_STATUS_E wifi_status)
{
    char *wifi_icon = ai_ui_get_wifi_icon(wifi_status);

    if (sg_ui.network_label == NULL || wifi_icon == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.network_label, wifi_icon);
    lv_vendor_disp_unlock();
}

/**
 * @brief Set chat mode text on UI.
 *
 * @param chat_mode Pointer to the chat mode text string.
 */
static void __ui_set_chat_mode(char *chat_mode)
{
    if (sg_ui.chat_mode_label == NULL || NULL == chat_mode) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_mode_label, chat_mode);
    lv_vendor_disp_unlock();
}

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
static void __ui_picture_return(void)
{
    if (sg_picture_tm) {
        lv_timer_del(sg_picture_tm);
        sg_picture_tm = NULL;
    }
    if (sg_picture_canvas) {
        lv_obj_delete(sg_picture_canvas);
        sg_picture_canvas = NULL;
    }
    if (sg_picture_buffer) {
        Free(sg_picture_buffer);
        sg_picture_buffer = NULL;
    }
    lv_obj_clear_flag(sg_ui.container, LV_OBJ_FLAG_HIDDEN);
}

static void __ui_picture_timeout_cb(lv_timer_t *timer)
{
    lv_vendor_disp_lock();
    __ui_picture_return();
    lv_vendor_disp_unlock();
}

static void __ui_picture_click_cb(lv_event_t *e)
{
    lv_vendor_disp_lock();
    __ui_picture_return();
    lv_vendor_disp_unlock();
}

static void __ui_disp_image(AI_UI_IMG_T *img)
{
    if (img == NULL || img->data == NULL || img->len == 0) {
        return;
    }

    uint8_t  *jpeg = img->data;
    uint32_t  jpeg_len = img->len;

    TAL_IMAGE_JPEG_INFO_T info = {0};
    if (tal_image_jpeg_get_info(jpeg, jpeg_len, &info) != OPRT_OK) {
        PR_ERR("chatbot: jpeg get info failed");
        return;
    }

    uint32_t rgb565_size = info.width * info.height * 2;
    uint8_t *rgb565_buf = Malloc(rgb565_size);
    if (rgb565_buf == NULL) {
        PR_ERR("chatbot: malloc rgb565 buf failed, size=%u", rgb565_size);
        return;
    }

    TAL_IMAGE_JPEG_OUTPUT_T out = {0};
    out.out_buf      = rgb565_buf;
    out.out_buf_size = rgb565_size;
    out.out_width    = info.width;
    out.out_height   = info.height;

    if (tal_image_jpeg_decode_rgb565(jpeg, jpeg_len, &out) != OPRT_OK) {
        PR_ERR("chatbot: jpeg decode rgb565 failed");
        Free(rgb565_buf);
        return;
    }

    PR_NOTICE("chatbot: disp_image w=%u h=%u size=%u", info.width, info.height, rgb565_size);

    lv_vendor_disp_lock();

    __ui_picture_return();

    if (sg_picture_buffer) {
        Free(sg_picture_buffer);
    }
    sg_picture_buffer = rgb565_buf;

    lv_obj_t *screen = lv_screen_active();
    sg_picture_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(sg_picture_canvas, sg_picture_buffer,
                         info.width, info.height, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(sg_picture_canvas, info.width, info.height);
    lv_obj_center(sg_picture_canvas);
    lv_obj_add_flag(sg_picture_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sg_picture_canvas, __ui_picture_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_flag(sg_ui.container, LV_OBJ_FLAG_HIDDEN);

    sg_picture_tm = lv_timer_create(__ui_picture_timeout_cb, 10000, NULL);

    lv_vendor_disp_unlock();
}

static void __ui_link_click_cb(lv_event_t *e)
{
    UI_CHATBOT_LINK_DATA_T *link_data = lv_event_get_user_data(e);
    if (link_data && link_data->cb) {
        link_data->cb(link_data->cb_arg);
    }
}

static void __ui_link_delete_cb(lv_event_t *e)
{
    UI_CHATBOT_LINK_DATA_T *link_data = lv_event_get_user_data(e);
    if (link_data) {
        if (link_data->cb_arg) {
            Free(link_data->cb_arg);
        }
        Free(link_data);
    }
}

static void __ui_disp_link(bool is_ai, char *text, AI_UI_CHAT_LINK_CB cb, void *cb_arg, uint32_t len)
{
    if (sg_ui.chat_message_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();

    lv_label_set_text(sg_ui.chat_message_label, text ? text : "");
    lv_obj_set_style_text_color(sg_ui.chat_message_label, lv_color_hex(0x576B95), 0);
    lv_obj_set_style_text_decor(sg_ui.chat_message_label, LV_TEXT_DECOR_UNDERLINE, 0);
    lv_obj_add_flag(sg_ui.chat_message_label, LV_OBJ_FLAG_CLICKABLE);

    if (cb) {
        UI_CHATBOT_LINK_DATA_T *link_data = Malloc(sizeof(UI_CHATBOT_LINK_DATA_T));
        if (link_data) {
            link_data->cb  = cb;
            link_data->len = len;
            if (len > 0 && cb_arg != NULL) {
                link_data->cb_arg = Malloc(len + 1);
                if (link_data->cb_arg) {
                    memcpy(link_data->cb_arg, cb_arg, len);
                    ((char *)link_data->cb_arg)[len] = '\0';
                }
            } else {
                link_data->cb_arg = cb_arg;
            }
            lv_obj_add_event_cb(sg_ui.chat_message_label, __ui_link_click_cb, LV_EVENT_CLICKED, link_data);
            lv_obj_add_event_cb(sg_ui.chat_message_label, __ui_link_delete_cb, LV_EVENT_DELETE, link_data);
        }
    }

    lv_vendor_disp_unlock();
}
#endif

/**
 * @brief Register chatbot-style chat UI implementation.
 *
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_chat_chatbot_register(void)
{
    AI_UI_INTFS_T intfs;
    memset(&intfs, 0, sizeof(AI_UI_INTFS_T));

    intfs.disp_init         = __ui_init;
    intfs.disp_emotion      = __ui_set_emotion;
    intfs.disp_ai_mode_state = __ui_set_status;
    intfs.disp_notification = __ui_set_notification;
    intfs.disp_wifi_state   = __ui_set_network;
    intfs.disp_ai_chat_mode = __ui_set_chat_mode;

    ai_ui_register(&intfs);

    AI_UI_CHAT_INTFS_T chat_intfs;
    memset(&chat_intfs, 0, sizeof(AI_UI_CHAT_INTFS_T));

    chat_intfs.disp_user_msg            = __ui_set_user_msg;
    chat_intfs.disp_ai_msg              = __ui_set_ai_msg;
    chat_intfs.disp_ai_msg_stream_start = __ui_set_ai_msg_stream_start;
    chat_intfs.disp_ai_msg_stream_data  = __ui_set_ai_msg_stream_data;
    chat_intfs.disp_ai_msg_stream_end   = __ui_set_ai_msg_stream_end;
    chat_intfs.disp_system_msg          = __ui_set_system_msg;

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
    chat_intfs.disp_image = __ui_disp_image;
    chat_intfs.disp_link  = __ui_disp_link;
#endif

    return ai_ui_chat_register(&chat_intfs);
}
#endif