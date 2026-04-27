#include "tal_api.h"
#include "ai_chat_main.h"
#include "app_photo_main.h"

OPERATE_RET app_photo_main_init(void)
{
    AI_CHAT_MODE_CFG_T cfg = {
        .default_mode = AI_CHAT_MODE_HOLD,
        .default_vol  = 70,
        .evt_cb       = NULL,
    };
    return ai_chat_init(&cfg);
}
