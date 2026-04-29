#include "app_display.h"
#include "tal_api.h"
#include "ai_ui_manage.h"
#include "tuya_lvgl.h"

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)

#include "ui.h"

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
#include "ai_picture.h"
#include "image_album.h"
#endif

typedef struct {
    uint32_t type;
    int      len;
    uint8_t *data;
} DISP_MSG_T;

#define DISP_QUEUE_SIZE 8

typedef struct {
    QUEUE_HANDLE  queue_hdl;
    THREAD_HANDLE thrd_hdl;
} PHOTO_DISPLAY_T;

static PHOTO_DISPLAY_T sg_disp = {0};
static volatile bool sg_disp_inited = false;

#define DISP_TYPE(tab, cmd) (((tab) << 16) | (cmd))
#define DISP_TAB(type)      (((type) >> 16) & 0xFF)
#define DISP_CMD(type)      ((type) & 0xFFFF)

#define TAB_GLOBAL  0
#define TAB_CHAT    1
#define TAB_CAMERA  2
#define TAB_ALBUM   3

#define CMD_EMOTION       1
#define CMD_STATUS        2
#define CMD_NOTIFICATION  3
#define CMD_WIFI          4
#define CMD_CHAT_MODE     5
#define CMD_USER_MSG      12
#define CMD_AI_MSG        13
#define CMD_STREAM_START  14
#define CMD_STREAM_DATA   15
#define CMD_STREAM_END    16
#define CMD_SYSTEM_MSG    17
#define CMD_CAM_OPEN      20
#define CMD_CAM_CLOSE     21
#define CMD_CAM_THUMB     22
#define CMD_ALBUM_OPEN    30
#define CMD_ALBUM_CLOSE   31

static void __enqueue(uint32_t type, uint8_t *data, int len)
{
    DISP_MSG_T msg = {.type = type, .len = len, .data = NULL};
    if (data && len > 0) {
        msg.data = Malloc(len + 1);
        if (!msg.data) return;
        memcpy(msg.data, data, len);
        msg.data[len] = 0;
    }
    if (OPRT_OK != tal_queue_post(sg_disp.queue_hdl, &msg, 0)) {
        if (msg.data) Free(msg.data);
    }
}

static void __process_msg(DISP_MSG_T *msg)
{
    tuya_lvgl_mutex_lock();
    uint8_t  tab = (uint8_t)DISP_TAB(msg->type);
    uint16_t cmd = (uint16_t)DISP_CMD(msg->type);

    if (tab == TAB_GLOBAL) {
        switch (cmd) {
        case CMD_EMOTION:      ui_set_emotion((char *)msg->data);       break;
        case CMD_STATUS:       ui_set_device_status((char *)msg->data); break;
        case CMD_NOTIFICATION: ui_set_notification((char *)msg->data);  break;
        case CMD_WIFI:
            if (msg->data) ui_set_wifi_status(*(AI_UI_WIFI_STATUS_E *)msg->data);
            break;
        default: break;
        }
    } else if (tab == TAB_CHAT) {
        switch (cmd) {
        case CMD_USER_MSG:     ui_chat_set_user_msg((char *)msg->data);   break;
        case CMD_AI_MSG:       ui_chat_set_ai_msg((char *)msg->data);     break;
        case CMD_STREAM_START: ui_chat_stream_start();                    break;
        case CMD_STREAM_DATA:  ui_chat_stream_data((char *)msg->data);    break;
        case CMD_STREAM_END:   ui_chat_stream_end();                      break;
        case CMD_SYSTEM_MSG:   ui_chat_set_system_msg((char *)msg->data); break;
        default: break;
        }
    } else if (tab == TAB_CAMERA) {
        switch (cmd) {
        case CMD_CAM_OPEN:  ui_recognize_camera_open();                      break;
        case CMD_CAM_CLOSE: ui_recognize_camera_close();                     break;
        case CMD_CAM_THUMB:
            ui_recognize_camera_close();
            ui_recognize_set_thumbnail(msg->data, msg->len);
            ui_effect_set_source(msg->data, msg->len);
            break;
        default: break;
        }
    } else if (tab == TAB_ALBUM) {
        switch (cmd) {
        case CMD_ALBUM_OPEN:  ui_effect_album_open();  break;
        case CMD_ALBUM_CLOSE: ui_effect_album_close(); break;
        default: break;
        }
    }
    tuya_lvgl_mutex_unlock();
}

static void __disp_task(void *arg)
{
    (void)arg;
    tuya_lvgl_init();

    PR_NOTICE("disp_task: lvgl init done, calling ui_init");
    tuya_lvgl_mutex_lock();
    ui_init();
    tuya_lvgl_mutex_unlock();
    PR_NOTICE("disp_task: ui_init done");

    while (1) {
        DISP_MSG_T msg = {0};
        if (OPRT_OK == tal_queue_fetch(sg_disp.queue_hdl, &msg, QUEUE_WAIT_FOREVER)) {
            __process_msg(&msg);
            if (msg.data) Free(msg.data);
        }
    }
}

/* ---------- INTFS callbacks ---------- */

static void __disp_emotion(char *s)
    { __enqueue(DISP_TYPE(TAB_GLOBAL, CMD_EMOTION), (uint8_t *)s, s ? (int)strlen(s) : 0); }
static void __disp_status(char *s)
    { __enqueue(DISP_TYPE(TAB_GLOBAL, CMD_STATUS), (uint8_t *)s, s ? (int)strlen(s) : 0); }
static void __disp_notification(char *s)
    { __enqueue(DISP_TYPE(TAB_GLOBAL, CMD_NOTIFICATION), (uint8_t *)s, s ? (int)strlen(s) : 0); }
static void __disp_wifi(AI_UI_WIFI_STATUS_E s)
    { __enqueue(DISP_TYPE(TAB_GLOBAL, CMD_WIFI), (uint8_t *)&s, (int)sizeof(s)); }
static void __disp_chat_mode(char *s)  { (void)s; }
static void __disp_other(uint32_t t, uint8_t *d, int l) { (void)t; (void)d; (void)l; }

static void __chat_open(void)  {}
static void __chat_close(void) {}
static void __chat_user_msg(char *s)
    { __enqueue(DISP_TYPE(TAB_CHAT, CMD_USER_MSG), (uint8_t *)s, s ? (int)strlen(s) : 0); }
static void __chat_ai_msg(char *s)
    { __enqueue(DISP_TYPE(TAB_CHAT, CMD_AI_MSG), (uint8_t *)s, s ? (int)strlen(s) : 0); }
static void __chat_stream_start(void)
    { __enqueue(DISP_TYPE(TAB_CHAT, CMD_STREAM_START), NULL, 0); }
static void __chat_stream_data(char *s)
    { __enqueue(DISP_TYPE(TAB_CHAT, CMD_STREAM_DATA), (uint8_t *)s, s ? (int)strlen(s) : 0); }
static void __chat_stream_end(void)
    { __enqueue(DISP_TYPE(TAB_CHAT, CMD_STREAM_END), NULL, 0); }
static void __chat_system_msg(char *s)
    { __enqueue(DISP_TYPE(TAB_CHAT, CMD_SYSTEM_MSG), (uint8_t *)s, s ? (int)strlen(s) : 0); }
static void __chat_image(AI_UI_IMG_T *img)      { (void)img; }
static void __chat_link(bool is_ai, char *text, AI_UI_CHAT_LINK_CB cb, void *arg, uint32_t len)
{
    (void)text; (void)cb;
#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
    if (is_ai && arg && len > 0) {
        IMAGE_ALBUM_HANDLE hdl = image_album_find_by_name(ai_picture_get_album_name());
        uint8_t *jpeg_data = NULL;
        size_t   jpeg_size = 0;
        if (hdl && OPRT_OK == image_album_read(hdl, (char *)arg, 0, &jpeg_data, &jpeg_size) && jpeg_data) {
            tuya_lvgl_mutex_lock();
            ui_effect_set_result_image(jpeg_data, (uint32_t)jpeg_size);
            tuya_lvgl_mutex_unlock();
            image_album_free_file_data(jpeg_data);
        }
    }
#else
    (void)is_ai; (void)arg; (void)len;
#endif
}
static void __chat_add_attach(AI_UI_IMG_T *img) { (void)img; }
static void __chat_clear_attach(void)           {}

static void __cam_open(void)
    { __enqueue(DISP_TYPE(TAB_CAMERA, CMD_CAM_OPEN), NULL, 0); }
static void __cam_close(void)
    { __enqueue(DISP_TYPE(TAB_CAMERA, CMD_CAM_CLOSE), NULL, 0); }
static void __cam_thumb(uint8_t *jpeg, uint32_t len)
    { __enqueue(DISP_TYPE(TAB_CAMERA, CMD_CAM_THUMB), jpeg, (int)len); }
static void __cam_yuv_flush(AI_UI_VIDEO_T *v)
{
    if (!v || !v->yuv422 || v->width == 0 || v->height == 0) return;
    ui_recognize_camera_yuv_flush(v->yuv422, v->width, v->height);
}

static void __album_open(void)
    { __enqueue(DISP_TYPE(TAB_ALBUM, CMD_ALBUM_OPEN), NULL, 0); }
static void __album_close(void)
    { __enqueue(DISP_TYPE(TAB_ALBUM, CMD_ALBUM_CLOSE), NULL, 0); }
static void __album_image(AI_UI_IMG_T *img)                              { (void)img; }
static void __album_all_thumb(AI_UI_IMG_T *arr, uint32_t cnt)            { (void)arr; (void)cnt; }
static void __album_select_thumb(AI_UI_IMG_T *arr, uint32_t cnt, uint8_t max)
    { (void)arr; (void)cnt; (void)max; }

OPERATE_RET app_display_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (sg_disp_inited) return OPRT_OK;
    sg_disp_inited = true;

    TUYA_CALL_ERR_RETURN(tal_queue_create_init(&sg_disp.queue_hdl, sizeof(DISP_MSG_T), DISP_QUEUE_SIZE));

    AI_UI_INTFS_T intfs = {
        .disp_init          = app_display_init,
        .disp_emotion       = __disp_emotion,
        .disp_ai_mode_state = __disp_status,
        .disp_notification  = __disp_notification,
        .disp_wifi_state    = __disp_wifi,
        .disp_ai_chat_mode  = __disp_chat_mode,
        .disp_other_msg     = __disp_other,
    };
    ai_ui_register(&intfs);

    AI_UI_CHAT_INTFS_T chat_intfs = {
        .disp_open                = __chat_open,
        .disp_close               = __chat_close,
        .disp_user_msg            = __chat_user_msg,
        .disp_ai_msg              = __chat_ai_msg,
        .disp_ai_msg_stream_start = __chat_stream_start,
        .disp_ai_msg_stream_data  = __chat_stream_data,
        .disp_ai_msg_stream_end   = __chat_stream_end,
        .disp_system_msg          = __chat_system_msg,
        .disp_image               = __chat_image,
        .disp_link                = __chat_link,
        .disp_add_chat_attch_img  = __chat_add_attach,
        .disp_clear_chat_attach   = __chat_clear_attach,
    };
    ai_ui_chat_register(&chat_intfs);

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    AI_UI_CAMERA_INTFS_T cam_intfs = {
        .disp_open               = __cam_open,
        .disp_yuv_flush          = __cam_yuv_flush,
        .disp_set_thumbnail_jpeg = __cam_thumb,
        .disp_close              = __cam_close,
    };
    ai_ui_camera_register(&cam_intfs);
#endif

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
    AI_UI_ALBUM_INTFS_T album_intfs = {
        .disp_open                  = __album_open,
        .disp_image                 = __album_image,
        .disp_all_img_thumb_list    = __album_all_thumb,
        .disp_select_img_thumb_list = __album_select_thumb,
        .disp_close                 = __album_close,
    };
    ai_ui_image_album_register(&album_intfs);
#endif

    THREAD_CFG_T cfg = {
        .priority   = THREAD_PRIO_2,
        .stackDepth = 8192,
        .thrdname   = "disp",
    };
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_disp.thrd_hdl, NULL, NULL, __disp_task, NULL, &cfg));

    PR_NOTICE("app display: INTFS registered");
    return rt;
}

OPERATE_RET app_display_camera_start(uint16_t w, uint16_t h)
{
    (void)w;
    (void)h;
    return OPRT_OK;
}

OPERATE_RET app_display_camera_flush(uint8_t *data, uint16_t w, uint16_t h)
{
    (void)data;
    (void)w;
    (void)h;
    return OPRT_OK;
}

OPERATE_RET app_display_camera_end(void)
{
    return OPRT_OK;
}

#endif /* ENABLE_COMP_AI_DISPLAY */
