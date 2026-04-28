/**
 * @file app_scene_effect.c
 * @brief Effect scene: apply AI style to a photo via generateImage trigger.
 *        Source photo is captured from camera or selected from album.
 *        Voice mode injects event_param intent and queues photo before speaking.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "app_scene_effect.h"

#include <string.h>
#include <stdio.h>

#include "tal_api.h"
#include "tal_event.h"
#include "tuya_ai_biz.h"
#include "tuya_ai_agent.h"
#include "ai_ui_manage.h"
#include "ai_picture.h"
#include "ai_picture_input.h"
#include "ai_picture_output.h"
#include "ai_video_input.h"
#include "image_album.h"

/***********************************************************
************************macro define************************
***********************************************************/

#define STR_(x) #x
#define STR(x)  STR_(x)

#define EFFECT_OUTPUT_WIDTH  480
#define EFFECT_OUTPUT_HEIGHT 480

/* event_param JSON to inject generateImage intent for voice mode */
#define EFFECT_EVENT_PARAM                                                                        \
    "{\"sys.device.clm.intent\":{\"value\":\"generateImage\"}"                                    \
    ",\"sys.device.img_resize.width\":{\"value\":" STR(EFFECT_OUTPUT_WIDTH) "}"                  \
    ",\"sys.device.img_resize.height\":{\"value\":" STR(EFFECT_OUTPUT_HEIGHT) "}}"

/***********************************************************
***********************variable define**********************
***********************************************************/

/* current source photo album filename */
static char sg_current_photo[AI_PICTURE_NAME_MAX_LEN + 1] = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief ONETIME event callback: inject generateImage event_param on next AI session
 */
static int __effect_set_event_param_cb(void *data)
{
    (void)data;
    tuya_ai_agent_set_event_param(EFFECT_EVENT_PARAM);
    PR_DEBUG("[scene_effect] event_param injected (generateImage)");
    return 0;
}

OPERATE_RET app_scene_effect_init(void)
{
    PR_DEBUG("[scene_effect] init");
    return OPRT_OK;
}

void app_scene_effect_take_photo(void)
{
    OPERATE_RET rt      = OPRT_OK;
    uint8_t    *jpeg     = NULL;
    uint32_t    jpeg_len = 0;

    rt = ai_video_get_jpeg_frame(&jpeg, &jpeg_len);
    if (OPRT_OK != rt || NULL == jpeg || 0 == jpeg_len) {
        PR_ERR("[scene_effect] get_jpeg_frame failed: %d", rt);
        return;
    }

    /* save to album */
    memset(sg_current_photo, 0, sizeof(sg_current_photo));
    rt = ai_picture_save_to_album(jpeg, jpeg_len, NULL, sg_current_photo);
    if (OPRT_OK != rt) {
        PR_ERR("[scene_effect] save_to_album failed: %d", rt);
        ai_video_jpeg_image_free(&jpeg);
        return;
    }

    PR_DEBUG("[scene_effect] photo saved: %s", sg_current_photo);

    /* show source thumbnail */
    app_scene_effect_show_source_thumbnail(jpeg, jpeg_len);

    ai_video_jpeg_image_free(&jpeg);
}

void app_scene_effect_pick_album(const char *name)
{
    if (NULL == name || name[0] == '\0') {
        PR_ERR("[scene_effect] pick_album: invalid name");
        return;
    }

    PR_DEBUG("[scene_effect] pick_album: %s", name);
    strncpy(sg_current_photo, name, AI_PICTURE_NAME_MAX_LEN);
    sg_current_photo[AI_PICTURE_NAME_MAX_LEN] = '\0';
}

void app_scene_effect_apply_style(const char *style)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == style || style[0] == '\0') {
        PR_ERR("[scene_effect] apply_style: invalid style");
        return;
    }

    if (sg_current_photo[0] == '\0') {
        PR_ERR("[scene_effect] apply_style: no source photo");
        return;
    }

    PR_DEBUG("[scene_effect] apply_style: %s on photo: %s", style, sg_current_photo);

    /* set desired output size */
    TUYA_CALL_ERR_LOG(ai_picture_output_set_size(EFFECT_OUTPUT_WIDTH, EFFECT_OUTPUT_HEIGHT));

    /* add source photo to input queue */
    TUYA_CALL_ERR_LOG(ai_picture_input_add_from_album(sg_current_photo, NULL));

    /* build trigger param JSON with style — values must use {"value": ...} wrapper */
    char param[128] = {0};
    snprintf(param, sizeof(param), "{\"app.effect.style\":{\"value\":\"%s\"}}", style);

    /* trigger generateImage */
    rt = tuya_ai_agent_trigger(NULL, "generateImage", param);
    if (OPRT_OK != rt) {
        PR_ERR("[scene_effect] trigger generateImage failed: %d", rt);
    }
}

void app_scene_effect_prepare_voice(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (sg_current_photo[0] == '\0') {
        PR_ERR("[scene_effect] prepare_voice: no source photo");
        return;
    }

    PR_DEBUG("[scene_effect] prepare_voice: inject intent + queue photo");

    /* subscribe ONETIME to inject generateImage intent on next AI session */
    TUYA_CALL_ERR_LOG(tal_event_subscribe(EVENT_AI_SESSION_NEW,
                                          "effect_set_event_param",
                                          __effect_set_event_param_cb,
                                          SUBSCRIBE_TYPE_ONETIME));

    /* set output size */
    TUYA_CALL_ERR_LOG(ai_picture_output_set_size(EFFECT_OUTPUT_WIDTH, EFFECT_OUTPUT_HEIGHT));

    /* add source photo to input queue; it will be sent when voice session starts */
    TUYA_CALL_ERR_LOG(ai_picture_input_add_from_album(sg_current_photo, NULL));
}

void app_scene_effect_on_result(const char *album_name)
{
    if (NULL == album_name || album_name[0] == '\0') {
        PR_ERR("[scene_effect] on_result: invalid album_name");
        return;
    }

    PR_DEBUG("[scene_effect] on_result: %s", album_name);

    /* Send result image name to UI as AI image link */
    ai_ui_disp_msg(AI_UI_DISP_AI_IMAGE_LINK, (uint8_t *)album_name, (int)strlen(album_name));
}

void app_scene_effect_show_source_thumbnail(const uint8_t *jpeg, uint32_t len)
{
    if (NULL == jpeg || 0 == len) {
        return;
    }

    /* show thumbnail synchronously so it appears before camera closes */
    ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_THUMB, (uint8_t *)jpeg, (int)len);
}
