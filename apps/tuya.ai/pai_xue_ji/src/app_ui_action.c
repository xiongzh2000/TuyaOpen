/**
 * @file app_ui_action.c
 * @brief UI action handler for pai_xue_ji
 */
#include "tal_api.h"
#include <string.h>

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
#include "ai_ui_manage.h"

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
#include "image_album.h"
#include "ai_picture.h"
#include "ai_picture_input.h"
#endif

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
#include "ai_video_input.h"
#endif

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
static bool sg_ai_vision_enabled = false;
#endif

static void __app_ui_action_handle(AI_UI_ACTION_E action, uint8_t *data, uint32_t len)
{
    switch (action) {
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    case AI_UI_ACT_OPEN_CAMERA:
        sg_ai_vision_enabled = false;
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
            ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_THUMB, jpeg, jpeg_len);

            if (sg_ai_vision_enabled) {
                ai_video_stop();
                ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_CLOSE, NULL, 0);
                ai_ui_disp_msg(AI_UI_DISP_USER_IMAGE_LINK, (uint8_t *)name, strlen(name));
                ai_picture_input_recognize(jpeg, jpeg_len);
            }
#endif
            ai_video_jpeg_image_free(&jpeg);
        }
    } break;

    case AI_UI_ACT_CLOSE_CAMER:
        ai_video_stop();
        ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_CLOSE, NULL, 0);
        break;

    case AI_UI_ACT_CAMERA_AI_ON:
        sg_ai_vision_enabled = true;
        break;

    case AI_UI_ACT_CAMERA_AI_OFF:
        sg_ai_vision_enabled = false;
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
