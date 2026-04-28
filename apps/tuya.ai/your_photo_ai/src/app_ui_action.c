/**
 * @file app_ui_action.c
 * @brief Full UI action dispatch for all three photo AI scenes.
 *        Routes UI events to scene modules and manages camera lifecycle.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include <string.h>

#include "ai_ui_manage.h"
#include "app_photo_main.h"
#include "app_scene_chat.h"
#include "app_scene_recognize.h"
#include "app_scene_effect.h"

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
#include "ai_video_input.h"
#endif

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
#include "ai_picture.h"
#include "ai_picture_input.h"
#endif

#include "tdl_button_manage.h"
#include "ai_manage_mode.h"

/***********************************************************
************************macro define************************
***********************************************************/

/* HOLD pseudo-actions — must match ui_events.c definitions */
#define APP_ACT_HOLD_START (AI_UI_ACT_MAX + 100)
#define APP_ACT_HOLD_END   (AI_UI_ACT_MAX + 101)

/***********************************************************
***********************variable define**********************
***********************************************************/

/* true when effect scene has armed voice capture */
static bool sg_effect_voice_pending = false;

/***********************************************************
***********************function define**********************
***********************************************************/

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
/**
 * @brief YUV frame flush callback — forwards camera frames to the display
 */
static void __display_camera_yuv(TDL_CAMERA_FRAME_T *frame)
{
    AI_UI_VIDEO_T video = {0};

    if (NULL == frame) {
        return;
    }

    video.width  = frame->width;
    video.height = frame->height;
    video.yuv422 = frame->data;
    video.len    = frame->data_len;

    ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_FLUSH, (uint8_t *)&video, sizeof(AI_UI_VIDEO_T));
}

/**
 * @brief Start camera + show camera UI
 */
static void __camera_open(void)
{
    ai_video_set_yuv_frame_flush_cb(__display_camera_yuv);
    ai_video_start();
    ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_OPEN, NULL, 0);
}

/**
 * @brief Stop camera + hide camera UI
 */
static void __camera_close(void)
{
    ai_video_stop();
    ai_video_set_yuv_frame_flush_cb(NULL);
    ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_CLOSE, NULL, 0);
}
#endif /* ENABLE_COMP_AI_VIDEO */

static void __app_ui_action_handle(AI_UI_ACTION_E raw_action, uint8_t *data, uint32_t len)
{
    int action = (int)raw_action;

    switch (action) {

    /* ── scene switch ──────────────────────────────────────────── */
    case APP_ACT_SWITCH_CHAT:
        ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)"STANDBY", 7);
        break;

    case APP_ACT_SWITCH_RECOGNIZE:
        /* nothing extra on switch */
        break;

    case APP_ACT_SWITCH_EFFECT:
        sg_effect_voice_pending = false;
        break;

    /* ── HOLD voice (shared across scenes) ─────────────────────── */
    case APP_ACT_HOLD_START:
        if (sg_effect_voice_pending) {
            /* effect scene armed voice — inject intent before session starts */
            app_scene_effect_prepare_voice();
        }
        ai_mode_handle_key(TDL_BUTTON_LONG_PRESS_START, NULL);
        break;

    case APP_ACT_HOLD_END:
        ai_mode_handle_key(TDL_BUTTON_PRESS_UP, NULL);
        sg_effect_voice_pending = false;
        break;

    /* ── recognize scene ──────────────────────────────────────── */
    case APP_ACT_RECOGNIZE_TAKE_PHOTO:
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
        __camera_open();
        app_scene_recognize_take_photo();   /* captures frame, saves, submits */
        __camera_close();
#endif
        break;

    case APP_ACT_RECOGNIZE_PICK_ALBUM: {
#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
        char *album_name = ai_picture_get_album_name();
        if (album_name) {
            ai_ui_disp_msg_sync(AI_UI_DISP_ALBUM_OPEN,
                                (uint8_t *)album_name, (int)strlen(album_name));
        }
#endif
        break;
    }

    case APP_ACT_RECOGNIZE_CONTINUE_CHAT:
        app_scene_recognize_continue_chat();
        ai_ui_notify_action((AI_UI_ACTION_E)APP_ACT_SWITCH_CHAT, NULL, 0);
        break;

    /* ── effect scene ─────────────────────────────────────────── */
    case APP_ACT_EFFECT_TAKE_PHOTO:
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
        __camera_open();
        app_scene_effect_take_photo();      /* captures frame, saves, shows thumbnail */
        __camera_close();
#endif
        sg_effect_voice_pending = true;     /* user can now speak a style */
        break;

    case APP_ACT_EFFECT_PICK_ALBUM: {
#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
        char *album_name = ai_picture_get_album_name();
        if (album_name) {
            ai_ui_disp_msg_sync(AI_UI_DISP_ALBUM_OPEN,
                                (uint8_t *)album_name, (int)strlen(album_name));
        }
#endif
        sg_effect_voice_pending = true;
        break;
    }

    case APP_ACT_EFFECT_APPLY_STYLE:
        if (data && len > 0) {
            char style[64] = {0};
            snprintf(style, sizeof(style), "%.*s", (int)len, (char *)data);
            app_scene_effect_apply_style(style);
        }
        break;

    /* ── generic camera actions (from LVGL camera button) ─────── */
    case AI_UI_ACT_OPEN_CAMERA:
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
        __camera_open();
#endif
        break;

    case AI_UI_ACT_CLOSE_CAMER:
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
        __camera_close();
#endif
        break;

    default:
        break;
    }
}

void app_ui_action_register(void)
{
    ai_ui_action_cb_register(__app_ui_action_handle);
}

#endif /* ENABLE_COMP_AI_DISPLAY */
