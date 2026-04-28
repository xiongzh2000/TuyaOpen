#include "tal_api.h"
#include "ai_ui_manage.h"
#include "app_photo_main.h"
#include "app_scene_chat.h"
#include "app_scene_recognize.h"
#include "app_scene_effect.h"

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
static void __app_ui_action_handle(AI_UI_ACTION_E action, uint8_t *data, uint32_t len)
{
    (void)data;
    (void)len;
    switch ((int)action) {
    case APP_ACT_SWITCH_CHAT:
        break;
    case APP_ACT_SWITCH_RECOGNIZE:
        break;
    case APP_ACT_SWITCH_EFFECT:
        break;
    default:
        break;
    }
}

void app_ui_action_register(void)
{
    ai_ui_action_cb_register(__app_ui_action_handle);
}
#endif
