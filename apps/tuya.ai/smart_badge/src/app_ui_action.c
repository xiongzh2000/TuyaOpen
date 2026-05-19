/**
 * @file app_ui_action.c
 * @brief UI action handler for smart_badge
 */
#include "tal_api.h"
#include <string.h>

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
#include "ai_ui_manage.h"

extern OPERATE_RET ai_mode_handle_key(int event, void *arg);

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

    default:
        break;
    }
}

void app_ui_action_register(void)
{
    ai_ui_action_cb_register(__app_ui_action_handle);
}

#endif
