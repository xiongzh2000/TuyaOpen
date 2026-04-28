#include "tal_api.h"
#include "netmgr.h"
#include "ai_chat_main.h"
#include "app_photo_main.h"
#include "app_scene_chat.h"
#include "app_scene_recognize.h"
#include "app_scene_effect.h"
#include "app_display.h"

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "tkl_wifi.h"
#endif

#define PRINTF_FREE_HEAP_TIME (10 * 1000)
#define DISP_NET_STATUS_TIME  (1 * 1000)

static TIMER_ID sg_heap_tm;

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
static AI_UI_WIFI_STATUS_E sg_wifi_status = AI_UI_WIFI_STATUS_DISCONNECTED;
static TIMER_ID sg_net_tm;
extern void app_ui_action_register(void);

static void __update_wifi_status(void)
{
    AI_UI_WIFI_STATUS_E status = AI_UI_WIFI_STATUS_DISCONNECTED;
    netmgr_status_e net = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &net);
    if (net == NETMGR_LINK_UP) {
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
        int8_t rssi = 0;
#ifndef PLATFORM_T5
        tkl_wifi_station_get_conn_ap_rssi(&rssi);
#endif
        status = (rssi >= -60) ? AI_UI_WIFI_STATUS_GOOD :
                 (rssi >= -70) ? AI_UI_WIFI_STATUS_FAIR :
                                 AI_UI_WIFI_STATUS_WEAK;
#else
        status = AI_UI_WIFI_STATUS_GOOD;
#endif
    }
    if (status != sg_wifi_status) {
        sg_wifi_status = status;
        ai_ui_disp_msg(AI_UI_DISP_NETWORK, (uint8_t *)&status, sizeof(status));
    }
}

static void __net_tm_cb(TIMER_ID id, void *arg) { (void)id; (void)arg; __update_wifi_status(); }
#endif

static void __heap_tm_cb(TIMER_ID id, void *arg)
{
    (void)id; (void)arg;
    PR_INFO("Free heap: %d", tal_system_get_free_heap_size());
}

static void __ai_event_handler(AI_NOTIFY_EVENT_T *event)
{
    if (!event) return;
    switch (event->type) {
    case AI_USER_EVT_GENERATE_PICTURE:
        app_scene_effect_on_result((const char *)event->data);
        break;
    default:
        break;
    }
}

OPERATE_RET app_photo_main_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_CHAT_MODE_CFG_T cfg = {
        .default_mode = AI_CHAT_MODE_HOLD,
        .default_vol  = 70,
        .evt_cb       = __ai_event_handler,
    };
    TUYA_CALL_ERR_RETURN(ai_chat_init(&cfg));

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    TUYA_CALL_ERR_RETURN(app_display_init());
#endif

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    app_ui_action_register();
#endif

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    TUYA_CALL_ERR_LOG(ai_video_init());
#endif

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
    TUYA_CALL_ERR_RETURN(ai_picture_init());
#endif

    TUYA_CALL_ERR_LOG(app_scene_chat_init());
    TUYA_CALL_ERR_LOG(app_scene_recognize_init());
    TUYA_CALL_ERR_LOG(app_scene_effect_init());

    tal_sw_timer_create(__heap_tm_cb, NULL, &sg_heap_tm);
    tal_sw_timer_start(sg_heap_tm, PRINTF_FREE_HEAP_TIME, TAL_TIMER_CYCLE);

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    ai_ui_disp_msg(AI_UI_DISP_NETWORK, (uint8_t *)&sg_wifi_status, sizeof(sg_wifi_status));
    tal_sw_timer_create(__net_tm_cb, NULL, &sg_net_tm);
    tal_sw_timer_start(sg_net_tm, DISP_NET_STATUS_TIME, TAL_TIMER_CYCLE);
#endif
    return OPRT_OK;
}
