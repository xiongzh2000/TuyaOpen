/**
 * @file app_badge_ui.c
 * @brief Smart badge 3-page swipeable UI: Album | Clock | Chat
 */

#include "tal_api.h"

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)

#include "lvgl.h"
#include "lv_vendor.h"

#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"
#include "cat_faces.h"
#include "app_badge_ui.h"
#include "app_http_upload.h"
#include "skill_emotion.h"
#include "app_gesture.h"
#include "tuya_weather.h"

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
#include "tal_image.h"
#include "image_album.h"
extern IMAGE_ALBUM_HANDLE ai_picture_get_album_handle(void);
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define PAGE_W              LV_HOR_RES
#define PAGE_H              LV_VER_RES
#define CLOCK_UPDATE_MS     (3 * 1000)
#define STEP_DAILY_GOAL     1000
#define WEATHER_UPDATE_MS   (30 * 60 * 1000)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    lv_obj_t *page_container;

    /* Clock page */
    lv_obj_t *clock_page;
    lv_obj_t *clock_label;
    lv_obj_t *date_label;
    lv_obj_t *clock_emotion_img;
    lv_obj_t *wifi_label;
    lv_obj_t *status_label;
    lv_obj_t *step_arc;
    lv_obj_t *step_label;
    lv_obj_t *weather_label;
    lv_obj_t *battery_label;

    /* Chat page */
    lv_obj_t *chat_page;
    lv_obj_t *chat_status_label;
    lv_obj_t *chat_emotion_img;
    lv_obj_t *chat_msg_label;
    lv_obj_t *chat_img_canvas;

    /* Album page */
    lv_obj_t *album_page;
    lv_obj_t *album_scroll;
    lv_obj_t *album_hint_label;
} BADGE_UI_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static BADGE_UI_T sg_ui;
static AI_UI_FONT_LIST_T sg_font = {0};
static TIMER_ID sg_clock_timer = NULL;
static TIMER_ID sg_weather_timer = NULL;
static bool sg_is_streaming = false;
static int sg_last_reset_day = -1;

static uint8_t *sg_chat_img_buf = NULL;
static lv_timer_t *sg_chat_img_tm = NULL;

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
#define ALBUM_MAX_IMAGES 20
static uint8_t *sg_album_bufs[ALBUM_MAX_IMAGES];
static lv_obj_t *sg_album_slots[ALBUM_MAX_IMAGES];
static int sg_album_count = 0;
static lv_obj_t *sg_album_del_overlay = NULL;
static int sg_album_del_idx = -1;
static lv_obj_t *sg_album_upload_btn = NULL;
static void __ui_album_add_image(AI_UI_IMG_T *img);
#endif

/***********************************************************
***********************function define**********************
***********************************************************/

/* ==================== Weather ==================== */

static const char *__weather_text(int code)
{
    switch (code) {
        case 120: case 146: case 119: return "Sunny";
        case 142: case 129: return "Cloudy";
        case 132: return "Overcast";
        case 139: case 118: return "Drizzle";
        case 112: case 141: return "Rain";
        case 101: case 107: case 144: case 145: return "Downpour";
        case 108: case 111: case 122: case 123: return "Shower";
        case 143: case 102: case 110: case 136: return "Storm";
        case 104: case 128: return "Lt Snow";
        case 105: case 131: return "Snow";
        case 124: case 126: return "Blizzard";
        case 121: case 106: return "Fog";
        case 140: return "Haze";
        case 109: case 117: return "Dust";
        case 113: case 137: return "Sleet";
        default: return "Cloudy";
    }
}

static void __weather_update(void)
{
    if (!tuya_weather_allow_update()) return;

    WEATHER_CURRENT_CONDITIONS_T cond = {0};
    if (tuya_weather_get_current_conditions(&cond) != OPRT_OK) return;

    char buf[24];
    snprintf(buf, sizeof(buf), "%d`C %s", cond.temp, __weather_text(cond.weather));

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.weather_label, buf);
    lv_vendor_disp_unlock();
}

static void __weather_timer_cb(TIMER_ID timer_id, void *arg)
{
    __weather_update();
}

static int __weather_time_sync_cb(void *data)
{
    __weather_update();
    return OPRT_OK;
}

/* ==================== Clock Page ==================== */

static void __clock_update(void)
{
    TIME_T ts = tal_time_get_posix();

    lv_vendor_disp_lock();

    if (ts >= 1000000) {
        POSIX_TM_S tm = {0};
        tal_time_get_local_time_custom(0, &tm);

        if (sg_last_reset_day >= 0 && tm.tm_mday != sg_last_reset_day) {
            app_gesture_reset_steps();
        }
        sg_last_reset_day = tm.tm_mday;

        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
        lv_label_set_text(sg_ui.clock_label, buf);

        snprintf(buf, sizeof(buf), "%d/%d/%d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
        lv_label_set_text(sg_ui.date_label, buf);
    }

    uint32_t steps = app_gesture_get_steps();
    char step_buf[20];
    snprintf(step_buf, sizeof(step_buf), "%u / %d", (unsigned)steps, STEP_DAILY_GOAL);
    lv_label_set_text(sg_ui.step_label, step_buf);
    lv_arc_set_value(sg_ui.step_arc, (int32_t)(steps > STEP_DAILY_GOAL ? STEP_DAILY_GOAL : steps));

    lv_vendor_disp_unlock();
}

static void __clock_timer_cb(TIMER_ID timer_id, void *arg)
{
    __clock_update();
}

static void __create_clock_page(lv_obj_t *parent)
{
    sg_ui.clock_page = lv_obj_create(parent);
    lv_obj_set_size(sg_ui.clock_page, PAGE_W, PAGE_H);
    lv_obj_set_style_pad_all(sg_ui.clock_page, 0, 0);
    lv_obj_set_style_border_width(sg_ui.clock_page, 0, 0);
    lv_obj_set_style_radius(sg_ui.clock_page, 0, 0);
    lv_obj_set_style_bg_color(sg_ui.clock_page, lv_color_white(), 0);
    lv_obj_clear_flag(sg_ui.clock_page, LV_OBJ_FLAG_SCROLLABLE);

    /* Step progress arc (background layer) */
    sg_ui.step_arc = lv_arc_create(sg_ui.clock_page);
    lv_obj_set_size(sg_ui.step_arc, PAGE_W - 30, PAGE_H - 30);
    lv_obj_center(sg_ui.step_arc);
    lv_arc_set_rotation(sg_ui.step_arc, 135);
    lv_arc_set_bg_angles(sg_ui.step_arc, 0, 270);
    lv_arc_set_range(sg_ui.step_arc, 0, STEP_DAILY_GOAL);
    lv_arc_set_value(sg_ui.step_arc, 0);
    lv_arc_set_mode(sg_ui.step_arc, LV_ARC_MODE_NORMAL);
    lv_obj_set_style_arc_width(sg_ui.step_arc, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_width(sg_ui.step_arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(sg_ui.step_arc, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_obj_set_style_arc_color(sg_ui.step_arc, lv_color_hex(0x4CD964), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(sg_ui.step_arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(sg_ui.step_arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(sg_ui.step_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(sg_ui.step_arc, 0, LV_PART_KNOB);
    lv_obj_clear_flag(sg_ui.step_arc, LV_OBJ_FLAG_CLICKABLE);

    /* Inner container for centered content */
    lv_obj_t *inner = lv_obj_create(sg_ui.clock_page);
    lv_obj_set_size(inner, PAGE_W, PAGE_H);
    lv_obj_center(inner);
    lv_obj_set_style_bg_opa(inner, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(inner, 0, 0);
    lv_obj_set_style_pad_all(inner, 0, 0);
    lv_obj_set_flex_flow(inner, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(inner, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(inner, 8, 0);
    lv_obj_clear_flag(inner, LV_OBJ_FLAG_SCROLLABLE);

    /* Time */
    sg_ui.clock_label = lv_label_create(inner);
    lv_obj_set_style_text_font(sg_ui.clock_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(sg_ui.clock_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_letter_space(sg_ui.clock_label, 4, 0);
    lv_label_set_text(sg_ui.clock_label, "00:00");

    /* Cat face */
    sg_ui.clock_emotion_img = lv_image_create(inner);
    lv_image_set_src(sg_ui.clock_emotion_img, cat_face_get_by_emotion("NEUTRAL"));
    lv_obj_set_size(sg_ui.clock_emotion_img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    /* Date */
    sg_ui.date_label = lv_label_create(inner);
    lv_obj_set_style_text_color(sg_ui.date_label, lv_color_hex(0x999999), 0);
    lv_label_set_text(sg_ui.date_label, "--/--/--");

    /* Weather */
    sg_ui.weather_label = lv_label_create(inner);
    lv_obj_set_style_text_color(sg_ui.weather_label, lv_color_hex(0x4CD964), 0);
    lv_label_set_text(sg_ui.weather_label, "");

    /* WiFi */
    sg_ui.wifi_label = lv_label_create(inner);
    sg_font.icon = ai_ui_get_icon_font();
    lv_obj_set_style_text_font(sg_ui.wifi_label, sg_font.icon, 0);
    lv_obj_set_style_text_color(sg_ui.wifi_label, lv_color_hex(0x999999), 0);
    lv_label_set_text(sg_ui.wifi_label, "");

    /* Status */
    sg_ui.status_label = lv_label_create(inner);
    lv_obj_set_style_text_color(sg_ui.status_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_width(sg_ui.status_label, PAGE_W * 0.7);
    lv_obj_set_style_text_align(sg_ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(sg_ui.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(sg_ui.status_label, "");

    /* Step count label in arc gap (bottom) */
    sg_ui.step_label = lv_label_create(sg_ui.clock_page);
    lv_obj_set_style_text_font(sg_ui.step_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sg_ui.step_label, lv_color_hex(0x555555), 0);
    lv_obj_align(sg_ui.step_label, LV_ALIGN_BOTTOM_MID, 0, -45);
    lv_label_set_text(sg_ui.step_label, "0 / 1000");

    /* Battery percentage (top-right, inside arc) */
    sg_ui.battery_label = lv_label_create(sg_ui.clock_page);
    lv_obj_set_style_text_font(sg_ui.battery_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sg_ui.battery_label, lv_color_hex(0x999999), 0);
    lv_obj_align(sg_ui.battery_label, LV_ALIGN_TOP_RIGHT, -40, 35);
    lv_label_set_text(sg_ui.battery_label, "100%");
}

/* ==================== Chat Page ==================== */

static void __chat_img_timeout_cb(lv_timer_t *timer)
{
    lv_vendor_disp_lock();
    if (sg_chat_img_tm) {
        lv_timer_del(sg_chat_img_tm);
        sg_chat_img_tm = NULL;
    }
    if (sg_ui.chat_img_canvas) {
        lv_obj_delete(sg_ui.chat_img_canvas);
        sg_ui.chat_img_canvas = NULL;
    }
    if (sg_chat_img_buf) {
        Free(sg_chat_img_buf);
        sg_chat_img_buf = NULL;
    }
    lv_obj_clear_flag(sg_ui.chat_status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.chat_emotion_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.chat_msg_label, LV_OBJ_FLAG_HIDDEN);
    lv_vendor_disp_unlock();
}

static void __chat_img_click_cb(lv_event_t *e)
{
    lv_vendor_disp_lock();
    if (sg_chat_img_tm) {
        lv_timer_del(sg_chat_img_tm);
        sg_chat_img_tm = NULL;
    }
    if (sg_ui.chat_img_canvas) {
        lv_obj_delete(sg_ui.chat_img_canvas);
        sg_ui.chat_img_canvas = NULL;
    }
    if (sg_chat_img_buf) {
        Free(sg_chat_img_buf);
        sg_chat_img_buf = NULL;
    }
    lv_obj_clear_flag(sg_ui.chat_status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.chat_emotion_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.chat_msg_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_scroll_to_x(sg_ui.page_container, 0, LV_ANIM_ON);
    lv_vendor_disp_unlock();
}

static void __create_chat_page(lv_obj_t *parent)
{
    sg_ui.chat_page = lv_obj_create(parent);
    lv_obj_set_size(sg_ui.chat_page, PAGE_W, PAGE_H);
    lv_obj_set_style_pad_all(sg_ui.chat_page, 0, 0);
    lv_obj_set_style_border_width(sg_ui.chat_page, 0, 0);
    lv_obj_set_style_radius(sg_ui.chat_page, 0, 0);
    lv_obj_set_style_bg_color(sg_ui.chat_page, lv_color_white(), 0);
    lv_obj_set_flex_flow(sg_ui.chat_page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sg_ui.chat_page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(sg_ui.chat_page, 50, 0);
    lv_obj_set_style_pad_row(sg_ui.chat_page, 6, 0);
    lv_obj_clear_flag(sg_ui.chat_page, LV_OBJ_FLAG_SCROLLABLE);

    /* Status label (top) */
    sg_ui.chat_status_label = lv_label_create(sg_ui.chat_page);
    lv_obj_set_style_text_color(sg_ui.chat_status_label, lv_color_hex(0x999999), 0);
    lv_obj_set_width(sg_ui.chat_status_label, PAGE_W * 0.6);
    lv_obj_set_style_text_align(sg_ui.chat_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(sg_ui.chat_status_label, "");

    /* Cat emotion */
    sg_ui.chat_emotion_img = lv_image_create(sg_ui.chat_page);
    lv_image_set_src(sg_ui.chat_emotion_img, cat_face_get_by_emotion("NEUTRAL"));
    lv_obj_set_size(sg_ui.chat_emotion_img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    /* Message area (below cat) */
    sg_ui.chat_msg_label = lv_label_create(sg_ui.chat_page);
    lv_obj_set_style_text_color(sg_ui.chat_msg_label, lv_color_black(), 0);
    lv_obj_set_width(sg_ui.chat_msg_label, PAGE_W * 0.8);
    lv_obj_set_height(sg_ui.chat_msg_label, PAGE_H * 0.4);
    lv_label_set_long_mode(sg_ui.chat_msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(sg_ui.chat_msg_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(sg_ui.chat_msg_label, "");

    sg_ui.chat_img_canvas = NULL;
}

/* ==================== Album Page ==================== */

static void __album_upload_btn_cb(lv_event_t *e)
{
    (void)e;
    lv_vendor_disp_lock();
    if (sg_album_del_overlay) {
        lv_obj_delete(sg_album_del_overlay);
        sg_album_del_overlay = NULL;
    }
    lv_vendor_disp_unlock();
    app_http_upload_show_qr();
}

static void __create_album_page(lv_obj_t *parent)
{
    sg_ui.album_page = lv_obj_create(parent);
    lv_obj_set_size(sg_ui.album_page, PAGE_W, PAGE_H);
    lv_obj_set_style_pad_all(sg_ui.album_page, 0, 0);
    lv_obj_set_style_border_width(sg_ui.album_page, 0, 0);
    lv_obj_set_style_radius(sg_ui.album_page, 0, 0);
    lv_obj_set_style_bg_color(sg_ui.album_page, lv_color_black(), 0);
    lv_obj_clear_flag(sg_ui.album_page, LV_OBJ_FLAG_SCROLLABLE);

    /* Vertical scroll container for images */
    sg_ui.album_scroll = lv_obj_create(sg_ui.album_page);
    lv_obj_set_size(sg_ui.album_scroll, PAGE_W, PAGE_H);
    lv_obj_set_style_pad_all(sg_ui.album_scroll, 0, 0);
    lv_obj_set_style_border_width(sg_ui.album_scroll, 0, 0);
    lv_obj_set_style_radius(sg_ui.album_scroll, 0, 0);
    lv_obj_set_style_bg_opa(sg_ui.album_scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(sg_ui.album_scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sg_ui.album_scroll, 0, 0);
    lv_obj_set_scroll_snap_y(sg_ui.album_scroll, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(sg_ui.album_scroll, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(sg_ui.album_scroll, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_scroll_dir(sg_ui.album_scroll, LV_DIR_VER);

    /* Hint when empty */
    sg_ui.album_hint_label = lv_label_create(sg_ui.album_scroll);
    lv_obj_set_size(sg_ui.album_hint_label, PAGE_W, PAGE_H);
    lv_obj_set_style_text_color(sg_ui.album_hint_label, lv_color_hex(0x444444), 0);
    lv_obj_set_style_text_align(sg_ui.album_hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(sg_ui.album_hint_label, "\nNo images yet\n\nAsk AI to generate one!");
    lv_obj_set_flex_grow(sg_ui.album_hint_label, 0);

    /* Upload button (floating, only visible when album is empty) */
    sg_album_upload_btn = lv_button_create(sg_ui.album_page);
    lv_obj_set_size(sg_album_upload_btn, 50, 50);
    lv_obj_align(sg_album_upload_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_color(sg_album_upload_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_bg_opa(sg_album_upload_btn, LV_OPA_80, 0);
    lv_obj_set_style_radius(sg_album_upload_btn, 25, 0);
    lv_obj_set_style_shadow_width(sg_album_upload_btn, 8, 0);
    lv_obj_set_style_shadow_opa(sg_album_upload_btn, LV_OPA_30, 0);
    lv_obj_add_event_cb(sg_album_upload_btn, __album_upload_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(sg_album_upload_btn);
    lv_label_set_text(btn_label, "+");
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_center(btn_label);
}

/* ==================== AI UI Callbacks ==================== */

static OPERATE_RET __ui_init(void)
{
    lv_vendor_init(DISPLAY_NAME);
    lv_vendor_start(5, 1024 * 8);

    lv_vendor_disp_lock();

    sg_font.text = ai_ui_get_text_font();
    sg_font.icon = ai_ui_get_icon_font();

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_text_font(screen, sg_font.text, 0);

    /* 3-page horizontal scroll container */
    sg_ui.page_container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.page_container, PAGE_W, PAGE_H);
    lv_obj_set_style_pad_all(sg_ui.page_container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.page_container, 0, 0);
    lv_obj_set_style_radius(sg_ui.page_container, 0, 0);
    lv_obj_set_style_bg_opa(sg_ui.page_container, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(sg_ui.page_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(sg_ui.page_container, 0, 0);
    lv_obj_set_scroll_snap_x(sg_ui.page_container, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(sg_ui.page_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(sg_ui.page_container, LV_OBJ_FLAG_SCROLL_ONE);

    __create_album_page(sg_ui.page_container);
    __create_clock_page(sg_ui.page_container);
    __create_chat_page(sg_ui.page_container);

    lv_obj_scroll_to_x(sg_ui.page_container, PAGE_W, LV_ANIM_OFF);

    lv_vendor_disp_unlock();

    tal_sw_timer_create(__clock_timer_cb, NULL, &sg_clock_timer);
    tal_sw_timer_start(sg_clock_timer, CLOCK_UPDATE_MS, TAL_TIMER_CYCLE);
    __clock_update();

    tal_sw_timer_create(__weather_timer_cb, NULL, &sg_weather_timer);
    tal_sw_timer_start(sg_weather_timer, WEATHER_UPDATE_MS, TAL_TIMER_CYCLE);
    tal_event_subscribe("app.time.sync", "badge_weather", __weather_time_sync_cb, SUBSCRIBE_TYPE_NORMAL);

    return OPRT_OK;
}

static void __ui_set_emotion(char *emotion)
{
    const lv_img_dsc_t *face = cat_face_get_by_emotion(emotion);

    lv_vendor_disp_lock();
    if (sg_ui.clock_emotion_img)
        lv_image_set_src(sg_ui.clock_emotion_img, face);
    if (sg_ui.chat_emotion_img)
        lv_image_set_src(sg_ui.chat_emotion_img, face);
    lv_vendor_disp_unlock();
}

static void __ui_set_status(char *status)
{
    lv_vendor_disp_lock();
    if (sg_ui.status_label)
        lv_label_set_text(sg_ui.status_label, status);
    if (sg_ui.chat_status_label)
        lv_label_set_text(sg_ui.chat_status_label, status);
    lv_vendor_disp_unlock();
}

static void __ui_set_notification(char *notification)
{
    __ui_set_status(notification);
}

static void __ui_set_network(AI_UI_WIFI_STATUS_E wifi_status)
{
    char *icon = ai_ui_get_wifi_icon(wifi_status);
    if (sg_ui.wifi_label == NULL || icon == NULL) return;

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.wifi_label, icon);
    lv_vendor_disp_unlock();
}

static void __ui_set_chat_mode(char *chat_mode)
{
    (void)chat_mode;
}

/* ---- Chat callbacks ---- */

static void __ui_set_user_msg(char *text)
{
    if (sg_ui.chat_msg_label == NULL) return;

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_msg_label, text);
    lv_obj_set_style_text_color(sg_ui.chat_msg_label, lv_color_hex(0x07C160), 0);
    lv_vendor_disp_unlock();
}

static void __ui_set_ai_msg(char *text)
{
    if (sg_ui.chat_msg_label == NULL) return;

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_msg_label, text);
    lv_obj_set_style_text_color(sg_ui.chat_msg_label, lv_color_black(), 0);
    lv_vendor_disp_unlock();
}

static void __ui_stream_start(void)
{
    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_msg_label, "");
    lv_obj_set_style_text_color(sg_ui.chat_msg_label, lv_color_black(), 0);
    lv_vendor_disp_unlock();
    sg_is_streaming = true;
}

static void __ui_stream_data(char *text)
{
    if (sg_ui.chat_msg_label == NULL || !sg_is_streaming) return;

    lv_vendor_disp_lock();
    lv_label_ins_text(sg_ui.chat_msg_label, LV_LABEL_POS_LAST, text);
    lv_vendor_disp_unlock();
}

static void __ui_stream_end(void)
{
    sg_is_streaming = false;
}

static void __ui_set_system_msg(char *text)
{
    if (sg_ui.chat_msg_label == NULL) return;

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_msg_label, text);
    lv_obj_set_style_text_color(sg_ui.chat_msg_label, lv_color_hex(0x999999), 0);
    lv_vendor_disp_unlock();
}

/* ---- Image display ---- */

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)

static OPERATE_RET __decode_jpeg_to_rgb565(uint8_t *jpeg, uint32_t jpeg_len,
                                           uint8_t **out_buf, uint16_t *out_w, uint16_t *out_h)
{
    TAL_IMAGE_JPEG_INFO_T info = {0};
    if (tal_image_jpeg_get_info(jpeg, jpeg_len, &info) != OPRT_OK) return OPRT_COM_ERROR;

    uint32_t rgb_size = info.width * info.height * 2;
    uint8_t *rgb = Malloc(rgb_size);
    if (rgb == NULL) return OPRT_MALLOC_FAILED;

    TAL_IMAGE_JPEG_OUTPUT_T out = {
        .out_buf = rgb, .out_buf_size = rgb_size,
        .out_width = info.width, .out_height = info.height,
    };

    if (tal_image_jpeg_decode_rgb565(jpeg, jpeg_len, &out) != OPRT_OK) {
        Free(rgb);
        return OPRT_COM_ERROR;
    }

    *out_buf = rgb;
    *out_w = info.width;
    *out_h = info.height;
    return OPRT_OK;
}

/* Show image directly on chat page (replaces cat + text temporarily) */
static void __ui_disp_image(AI_UI_IMG_T *img)
{
    if (img == NULL || img->data == NULL || img->len == 0) return;

    uint8_t *rgb_buf = NULL;
    uint16_t w = 0, h = 0;
    if (__decode_jpeg_to_rgb565(img->data, img->len, &rgb_buf, &w, &h) != OPRT_OK) return;

    lv_vendor_disp_lock();

    /* Clean previous chat image */
    if (sg_ui.chat_img_canvas) {
        lv_obj_delete(sg_ui.chat_img_canvas);
        sg_ui.chat_img_canvas = NULL;
    }
    if (sg_chat_img_buf) {
        Free(sg_chat_img_buf);
    }
    sg_chat_img_buf = rgb_buf;

    /* Hide cat + text + status, show image full screen */
    lv_obj_add_flag(sg_ui.chat_status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_ui.chat_emotion_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_ui.chat_msg_label, LV_OBJ_FLAG_HIDDEN);

    sg_ui.chat_img_canvas = lv_canvas_create(sg_ui.chat_page);
    lv_canvas_set_buffer(sg_ui.chat_img_canvas, sg_chat_img_buf, w, h, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(sg_ui.chat_img_canvas, w, h);
    lv_obj_center(sg_ui.chat_img_canvas);
    lv_obj_add_flag(sg_ui.chat_img_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sg_ui.chat_img_canvas, __chat_img_click_cb, LV_EVENT_CLICKED, NULL);

    /* Scroll to chat page */
    lv_obj_scroll_to_x(sg_ui.page_container, PAGE_W * 2, LV_ANIM_ON);

    /* Auto-restore after 10 seconds */
    if (sg_chat_img_tm) {
        lv_timer_reset(sg_chat_img_tm);
    } else {
        sg_chat_img_tm = lv_timer_create(__chat_img_timeout_cb, 10000, NULL);
    }

    lv_vendor_disp_unlock();

    /* Also add to album */
    __ui_album_add_image(img);
}

/* ---- Album delete ---- */

static void __album_del_dismiss_cb(lv_event_t *e)
{
    (void)e;
    lv_vendor_disp_lock();
    if (sg_album_del_overlay) {
        lv_obj_delete(sg_album_del_overlay);
        sg_album_del_overlay = NULL;
    }
    lv_vendor_disp_unlock();
}

static void __album_del_confirm_cb(lv_event_t *e)
{
    (void)e;
    int idx = sg_album_del_idx;

    lv_vendor_disp_lock();
    if (sg_album_del_overlay) {
        lv_obj_delete(sg_album_del_overlay);
        sg_album_del_overlay = NULL;
    }

    if (idx >= 0 && idx < sg_album_count) {
        lv_obj_delete(sg_album_slots[idx]);
        Free(sg_album_bufs[idx]);

        for (int i = idx; i < sg_album_count - 1; i++) {
            sg_album_bufs[i] = sg_album_bufs[i + 1];
            sg_album_slots[i] = sg_album_slots[i + 1];
        }
        sg_album_count--;

        if (sg_album_count == 0) {
            lv_obj_clear_flag(sg_ui.album_hint_label, LV_OBJ_FLAG_HIDDEN);
            if (sg_album_upload_btn) {
                lv_obj_clear_flag(sg_album_upload_btn, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    lv_vendor_disp_unlock();
}

static void __album_long_press_cb(lv_event_t *e)
{
    lv_obj_t *slot = lv_event_get_target(e);
    int idx = -1;
    for (int i = 0; i < sg_album_count; i++) {
        if (sg_album_slots[i] == slot) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return;
    sg_album_del_idx = idx;

    lv_vendor_disp_lock();
    if (sg_album_del_overlay) {
        lv_obj_delete(sg_album_del_overlay);
    }

    sg_album_del_overlay = lv_obj_create(sg_ui.album_page);
    lv_obj_set_size(sg_album_del_overlay, PAGE_W, PAGE_H);
    lv_obj_set_style_bg_color(sg_album_del_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_album_del_overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(sg_album_del_overlay, 0, 0);
    lv_obj_set_style_radius(sg_album_del_overlay, 0, 0);
    lv_obj_set_style_pad_all(sg_album_del_overlay, 0, 0);
    lv_obj_center(sg_album_del_overlay);
    lv_obj_add_flag(sg_album_del_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sg_album_del_overlay, __album_del_dismiss_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(sg_album_del_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *del_btn = lv_button_create(sg_album_del_overlay);
    lv_obj_set_size(del_btn, 140, 50);
    lv_obj_align(del_btn, LV_ALIGN_CENTER, 0, -35);
    lv_obj_set_style_bg_color(del_btn, lv_color_hex(0xEE4444), 0);
    lv_obj_set_style_radius(del_btn, 25, 0);
    lv_obj_add_event_cb(del_btn, __album_del_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *del_label = lv_label_create(del_btn);
    lv_label_set_text(del_label, "Delete");
    lv_obj_set_style_text_color(del_label, lv_color_white(), 0);
    lv_obj_center(del_label);

    lv_obj_t *add_btn = lv_button_create(sg_album_del_overlay);
    lv_obj_set_size(add_btn, 140, 50);
    lv_obj_align(add_btn, LV_ALIGN_CENTER, 0, 35);
    lv_obj_set_style_bg_color(add_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_radius(add_btn, 25, 0);
    lv_obj_add_event_cb(add_btn, __album_upload_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *add_label = lv_label_create(add_btn);
    lv_label_set_text(add_label, "Upload");
    lv_obj_set_style_text_color(add_label, lv_color_white(), 0);
    lv_obj_center(add_label);

    lv_vendor_disp_unlock();
}

/* Add image to album vertical scroll */
static void __ui_album_add_image(AI_UI_IMG_T *img)
{
    if (sg_album_count >= ALBUM_MAX_IMAGES) return;
    if (img == NULL || img->data == NULL || img->len == 0) return;

    uint8_t *rgb_buf = NULL;
    uint16_t w = 0, h = 0;
    if (__decode_jpeg_to_rgb565(img->data, img->len, &rgb_buf, &w, &h) != OPRT_OK) return;

    lv_vendor_disp_lock();

    /* Hide hint and upload button on first image */
    if (sg_album_count == 0) {
        lv_obj_add_flag(sg_ui.album_hint_label, LV_OBJ_FLAG_HIDDEN);
        if (sg_album_upload_btn) {
            lv_obj_add_flag(sg_album_upload_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Create a full-page container for this image */
    lv_obj_t *slot = lv_obj_create(sg_ui.album_scroll);
    lv_obj_set_size(slot, PAGE_W, PAGE_H);
    lv_obj_set_style_pad_all(slot, 0, 0);
    lv_obj_set_style_border_width(slot, 0, 0);
    lv_obj_set_style_radius(slot, 0, 0);
    lv_obj_set_style_bg_color(slot, lv_color_black(), 0);
    lv_obj_clear_flag(slot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(slot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(slot, __album_long_press_cb, LV_EVENT_LONG_PRESSED, NULL);

    lv_obj_t *canvas = lv_canvas_create(slot);
    lv_canvas_set_buffer(canvas, rgb_buf, w, h, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(canvas, w, h);
    lv_obj_center(canvas);

    sg_album_bufs[sg_album_count] = rgb_buf;
    sg_album_slots[sg_album_count] = slot;
    sg_album_count++;

    /* Scroll to newest image */
    lv_obj_scroll_to_y(sg_ui.album_scroll, (sg_album_count - 1) * PAGE_H, LV_ANIM_OFF);

    lv_vendor_disp_unlock();
}

void app_badge_ui_album_add_jpeg(const uint8_t *data, uint32_t len)
{
    if (data == NULL || len == 0) return;
    AI_UI_IMG_T img = {
        .data = (uint8_t *)data,
        .len  = len,
    };
    __ui_album_add_image(&img);
}

static void __ui_disp_link(bool is_ai, char *text, AI_UI_CHAT_LINK_CB cb, void *cb_arg, uint32_t len)
{
    (void)is_ai;
    (void)text;
    (void)cb;
    (void)cb_arg;
    (void)len;
}
#endif

/* ==================== Register ==================== */

OPERATE_RET app_badge_ui_register(void)
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
    chat_intfs.disp_ai_msg_stream_start = __ui_stream_start;
    chat_intfs.disp_ai_msg_stream_data  = __ui_stream_data;
    chat_intfs.disp_ai_msg_stream_end   = __ui_stream_end;
    chat_intfs.disp_system_msg          = __ui_set_system_msg;

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
    chat_intfs.disp_image = __ui_disp_image;
    chat_intfs.disp_link  = __ui_disp_link;
#endif

    return ai_ui_chat_register(&chat_intfs);
}

#endif /* ENABLE_COMP_AI_DISPLAY */
