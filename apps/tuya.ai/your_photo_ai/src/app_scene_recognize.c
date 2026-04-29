/**
 * @file app_scene_recognize.c
 * @brief Recognize scene: take photo or pick album image → AI image recognition.
 *        Injects ai_image intent via tuya_ai_agent_set_event_param before input_start.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "app_scene_recognize.h"

#include <string.h>

#include "tal_api.h"
#include "tuya_ai_biz.h"
#include "tuya_ai_agent.h"
#include "ai_ui_manage.h"
#include "ai_picture.h"
#include "ai_picture_input.h"
#include "ai_video_input.h"

/***********************************************************
************************macro define************************
***********************************************************/

/* event_param JSON to inject ai_image intent on next session */
#define RECOGNIZE_EVENT_PARAM \
    "{\"sys.device.clm.intent\":{\"value\":\"ai_image\"},\"text\":{\"value\":\"image_recognition\"}}"

/***********************************************************
***********************variable define**********************
***********************************************************/

/* last photo name saved during take_photo / pick_album */
static char sg_current_photo[AI_PICTURE_NAME_MAX_LEN + 1] = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

void app_scene_recognize_submit(void *data)
{
    (void)data;
    OPERATE_RET rt = OPRT_OK;

    if (sg_current_photo[0] == '\0') {
        PR_ERR("[scene_recognize] submit: no photo");
        return;
    }

    PR_NOTICE("[scene_recognize] submit start: %s", sg_current_photo);
    tuya_ai_agent_set_event_param(RECOGNIZE_EVENT_PARAM);

    TUYA_CALL_ERR_LOG(ai_picture_input_add_from_album(sg_current_photo, NULL));
    tuya_ai_input_start(true);
    TUYA_CALL_ERR_LOG(ai_picture_input_from_album());
    tuya_ai_input_stop();
    ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)"STANDBY", 7);
    PR_NOTICE("[scene_recognize] submit done");
}

OPERATE_RET app_scene_recognize_init(void)
{
    PR_DEBUG("[scene_recognize] init");
    return OPRT_OK;
}

void app_scene_recognize_take_photo(void)
{
    OPERATE_RET rt      = OPRT_OK;
    uint8_t    *jpeg     = NULL;
    uint32_t    jpeg_len = 0;

    rt = ai_video_get_jpeg_frame(&jpeg, &jpeg_len);
    if (OPRT_OK != rt || NULL == jpeg || 0 == jpeg_len) {
        PR_ERR("[scene_recognize] get_jpeg_frame failed: %d", rt);
        return;
    }

    /* save to album */
    memset(sg_current_photo, 0, sizeof(sg_current_photo));
    rt = ai_picture_save_to_album(jpeg, jpeg_len, NULL, sg_current_photo);
    if (OPRT_OK != rt) {
        PR_ERR("[scene_recognize] save_to_album failed: %d", rt);
        ai_video_jpeg_image_free(&jpeg);
        return;
    }

    /* show thumbnail in UI (async) */
    ai_ui_disp_msg(AI_UI_DISP_CAMERA_THUMB, jpeg, (int)jpeg_len);

    ai_video_jpeg_image_free(&jpeg);
}

void app_scene_recognize_pick_album(const char *name)
{
    if (NULL == name || name[0] == '\0') {
        PR_ERR("[scene_recognize] pick_album: invalid name");
        return;
    }

    PR_DEBUG("[scene_recognize] pick_album: %s", name);
    strncpy(sg_current_photo, name, AI_PICTURE_NAME_MAX_LEN);
    sg_current_photo[AI_PICTURE_NAME_MAX_LEN] = '\0';
}

void app_scene_recognize_continue_chat(void)
{
    PR_DEBUG("[scene_recognize] continue_chat → STANDBY");
    ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)"STANDBY", 7);
}
