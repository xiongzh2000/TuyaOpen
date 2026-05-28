/**
 * @file app_ui_action.c
 * @brief UI action handler for pai_xue_ji — supports three recognition modes
 *        (image_recognition, chinese_recognition, english_recognition).
 */
#include "tal_api.h"
#include <string.h>

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
#include "ai_ui_manage.h"

extern OPERATE_RET ai_mode_handle_key(int event, void *arg);

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
#include "image_album.h"
#include "ai_picture.h"
#include "ai_picture_input.h"
#endif

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
#include "ai_video_input.h"
#include "tuya_ai_agent.h"
#include "tuya_ai_input.h"
#endif

static void __do_recognize(uint8_t *jpeg, uint32_t jpeg_len, const char *mode_text)
{
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    uint64_t timestamp = tal_system_get_millisecond();

    tuya_ai_agent_set_event_param("{\"clm_intent\":\"ai_image\"}");

    tuya_ai_input_start(TRUE);
    tuya_ai_image_input(timestamp, jpeg, jpeg_len, jpeg_len);
    tuya_ai_text_input((uint8_t *)mode_text, strlen(mode_text), strlen(mode_text));
    tuya_ai_input_stop();
#endif
}

static void __app_ui_action_handle(AI_UI_ACTION_E action, uint8_t *data, uint32_t len)
{
    switch (action) {
    case AI_UI_ACT_TALK_KEY: {
        if (data && len >= 1) {
            int key_evt = (int)data[0];
            PR_NOTICE("touch talk key event: %d", key_evt);
            ai_mode_handle_key(key_evt, NULL);
        }
    } break;

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    case AI_UI_ACT_OPEN_CAMERA:
        ai_video_start();
        ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_OPEN, NULL, 0);
        break;

    case AI_UI_ACT_TAKE_PHOTO: {
        uint8_t *jpeg = NULL;
        uint32_t jpeg_len = 0;
        ai_video_get_jpeg_frame(&jpeg, &jpeg_len);

        if (jpeg && jpeg_len) {
#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
            char name[AI_PICTURE_NAME_MAX_LEN + 1] = {0};
            ai_picture_save_to_album(jpeg, jpeg_len, NULL, name);
#endif

            if (data && len > 0) {
                char mode_text[32] = {0};
                size_t copy_len = len < sizeof(mode_text) - 1 ? len : sizeof(mode_text) - 1;
                memcpy(mode_text, data, copy_len);
                PR_NOTICE("paixue: recognize mode='%s'", mode_text);
                __do_recognize(jpeg, jpeg_len, mode_text);
            }

            ai_video_jpeg_image_free(&jpeg);
        }
    } break;

    case AI_UI_ACT_CLOSE_CAMER:
        ai_video_stop();
        ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_CLOSE, NULL, 0);
        break;

    case AI_UI_ACT_CAMERA_AI_ON:
    case AI_UI_ACT_CAMERA_AI_OFF:
        break;
#endif

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
    case AI_UI_ACT_OPEN_ALBUM: {
        char *album_name = ai_picture_get_album_name();
        if (album_name) {
            ai_ui_disp_msg_sync(AI_UI_DISP_ALBUM_OPEN, (uint8_t *)album_name, strlen(album_name));
        }
    } break;

    case AI_UI_ACT_CLOSE_ALBUM:
        ai_ui_disp_msg_sync(AI_UI_DISP_ALBUM_CLOSE, NULL, 0);
        break;

    case AI_UI_ACT_ADD_IMG_ATTACH: {
        ai_ui_disp_msg_sync(AI_UI_DISP_ADD_CHAT_ATTACH_IMG, data, strlen((char *)data));
        ai_picture_input_add_from_album((char *)data, NULL);
    } break;

    case AI_UI_ACT_DEL_IMG_ATTACH: {
        ai_picture_input_del_from_album((char *)data);
    } break;
#endif

    default:
        break;
    }
}

void app_ui_action_register(void)
{
    ai_ui_action_cb_register(__app_ui_action_handle);
}

#endif
