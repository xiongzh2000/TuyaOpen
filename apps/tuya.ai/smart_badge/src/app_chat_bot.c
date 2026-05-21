/**
 * @file app_chat_bot.c
 * @brief Smart badge AI chat initialization
 */

#include "tal_api.h"

#include "netmgr.h"

#include "ai_chat_main.h"
#include "app_chat_bot.h"

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "tkl_wifi.h"
#endif

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
#include "ai_picture.h"
#if defined(ENABLE_COMP_AI_PICTURE_HOSTING_DLD) && (ENABLE_COMP_AI_PICTURE_HOSTING_DLD == 1)
#include "ai_picture_output.h"
#endif
#endif

#include "tkl_gpio.h"
#include "app_badge_ui.h"
#include "app_gesture.h"
#include "app_http_upload.h"
#include "skill_emotion.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define PRINTF_FREE_HEAP_TTIME (10 * 1000)
#define DISP_NET_STATUS_TIME   (1 * 1000)

/***********************************************************
***********************variable define**********************
***********************************************************/
static TIMER_ID sg_printf_heap_tm;

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
static AI_UI_WIFI_STATUS_E sg_wifi_status = AI_UI_WIFI_STATUS_DISCONNECTED;
static TIMER_ID            sg_disp_status_tm;
#endif

/***********************************************************
***********************function define**********************
***********************************************************/
#if defined(BOARD_CHOICE_WAVESHARE_T5AI_TOUCH_AMOLED_1_75) && (BOARD_CHOICE_WAVESHARE_T5AI_TOUCH_AMOLED_1_75 == 1)
static void __sd_gpio_silence(void)
{
    /* SD card slot shares 3V3 rail with touch I2C; when a card is inserted,
       floating SDIO GPIO lines cause electrical noise that disrupts I2C.
       Drive them high (matching external 10K pull-ups) to suppress this. */
    static const TUYA_GPIO_NUM_E sd_pins[] = {
        TUYA_GPIO_NUM_2,   // SD_CLK
        TUYA_GPIO_NUM_3,   // SD_CMD
        TUYA_GPIO_NUM_4,   // SD_D0
        TUYA_GPIO_NUM_5,   // SD_D1
        TUYA_GPIO_NUM_10,  // SD_D2
        TUYA_GPIO_NUM_11,  // SD_D3
    };
    TUYA_GPIO_BASE_CFG_T cfg = {
        .mode   = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level  = TUYA_GPIO_LEVEL_HIGH,
    };
    for (int i = 0; i < (int)(sizeof(sd_pins) / sizeof(sd_pins[0])); i++) {
        tkl_gpio_init(sd_pins[i], &cfg);
        tkl_gpio_write(sd_pins[i], TUYA_GPIO_LEVEL_HIGH);
    }
    PR_NOTICE("SD card GPIO pins silenced (IO2-5, IO10-11 → output high)");
}
#endif

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
extern void app_ui_action_register(void);
#endif

#if defined(ENABLE_IMAGE_ALBUM_STORAGE_SD) && (ENABLE_IMAGE_ALBUM_STORAGE_SD == 1)
static void __load_album_task(void *arg)
{
    (void)arg;
    app_badge_ui_load_album();
}
#endif

static void __printf_free_heap_tm_cb(TIMER_ID timer_id, void *arg)
{
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    uint32_t free_heap       = tal_system_get_free_heap_size();
    uint32_t free_psram_heap = tal_psram_get_free_heap_size();
    PR_INFO("Free heap size:%d, Free psram heap size:%d", free_heap, free_psram_heap);
#else
    uint32_t free_heap = tal_system_get_free_heap_size();
    PR_INFO("Free heap size:%d", free_heap);
#endif
}

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
static void __display_net_status_update(void)
{
    AI_UI_WIFI_STATUS_E wifi_status = AI_UI_WIFI_STATUS_DISCONNECTED;
    netmgr_status_e     net_status  = NETMGR_LINK_DOWN;

    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &net_status);
    if (net_status == NETMGR_LINK_UP) {
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
        int8_t rssi = 0;
#ifndef PLATFORM_T5
        tkl_wifi_station_get_conn_ap_rssi(&rssi);
#endif
        if (rssi >= -60) {
            wifi_status = AI_UI_WIFI_STATUS_GOOD;
        } else if (rssi >= -70) {
            wifi_status = AI_UI_WIFI_STATUS_FAIR;
        } else {
            wifi_status = AI_UI_WIFI_STATUS_WEAK;
        }
#else
        wifi_status = AI_UI_WIFI_STATUS_GOOD;
#endif
    } else {
        wifi_status = AI_UI_WIFI_STATUS_DISCONNECTED;
    }

    if (wifi_status != sg_wifi_status) {
        sg_wifi_status = wifi_status;
        ai_ui_disp_msg(AI_UI_DISP_NETWORK, (uint8_t *)&wifi_status, sizeof(AI_UI_WIFI_STATUS_E));
    }
}

static void __display_status_tm_cb(TIMER_ID timer_id, void *arg)
{
    __display_net_status_update();
}

#endif

static void __gesture_detect_cb(GESTURE_TYPE_E gesture)
{
    PR_NOTICE("gesture detected: %d", gesture);

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    const char *emotion = NULL;
    const char *msg = NULL;

    switch (gesture) {
    case GESTURE_SHAKE:
        emotion = EMOJI_SURPRISE;
        msg = "Don't shake me!";
        break;
    case GESTURE_TAP:
        emotion = EMOJI_ANGRY;
        msg = "Hey! Don't tap me!";
        break;
    case GESTURE_FLIP:
        emotion = EMOJI_SLEEP;
        msg = "Zzz...";
        break;
    default:
        return;
    }

    if (emotion) {
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)emotion, strlen(emotion));
    }
    if (msg) {
        ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)msg, strlen(msg));
    }
#endif
}

OPERATE_RET app_chat_bot_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(BOARD_CHOICE_WAVESHARE_T5AI_TOUCH_AMOLED_1_75) && (BOARD_CHOICE_WAVESHARE_T5AI_TOUCH_AMOLED_1_75 == 1)
    __sd_gpio_silence();
#endif

#if defined(ENABLE_AI_CHAT_CUSTOM_UI) && (ENABLE_AI_CHAT_CUSTOM_UI == 1)
    TUYA_CALL_ERR_LOG(app_badge_ui_register());
#endif

    AI_CHAT_MODE_CFG_T ai_chat_cfg = {
        .default_mode = AI_CHAT_MODE_HOLD,
        .default_vol  = 70,
        .evt_cb       = NULL,
    };
    TUYA_CALL_ERR_LOG(ai_chat_init(&ai_chat_cfg));

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
    TUYA_CALL_ERR_LOG(ai_picture_init());
#if defined(ENABLE_COMP_AI_PICTURE_HOSTING_DLD) && (ENABLE_COMP_AI_PICTURE_HOSTING_DLD == 1)
    TUYA_CALL_ERR_LOG(ai_picture_output_dld_init(466, 466));
#endif
#endif

#if defined(ENABLE_IMAGE_ALBUM_STORAGE_SD) && (ENABLE_IMAGE_ALBUM_STORAGE_SD == 1)
    {
        THREAD_HANDLE load_thd = NULL;
        THREAD_CFG_T load_cfg = {
            .thrdname   = "load_album",
            .priority   = THREAD_PRIO_3,
            .stackDepth = 8192,
        };
        tal_thread_create_and_start(&load_thd, NULL, NULL, __load_album_task, NULL, &load_cfg);
    }
#endif

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    app_ui_action_register();
#endif

    tal_sw_timer_create(__printf_free_heap_tm_cb, NULL, &sg_printf_heap_tm);
    tal_sw_timer_start(sg_printf_heap_tm, PRINTF_FREE_HEAP_TTIME, TAL_TIMER_CYCLE);

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
    ai_ui_disp_msg(AI_UI_DISP_NETWORK, (uint8_t *)&sg_wifi_status, sizeof(AI_UI_WIFI_STATUS_E));

    ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)INITIALIZING, strlen(INITIALIZING));
    ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_NEUTRAL, strlen(EMOJI_NEUTRAL));

    tal_sw_timer_create(__display_status_tm_cb, NULL, &sg_disp_status_tm);
    tal_sw_timer_start(sg_disp_status_tm, DISP_NET_STATUS_TIME, TAL_TIMER_CYCLE);
#endif

    TUYA_CALL_ERR_LOG(app_gesture_init(__gesture_detect_cb));

    TUYA_CALL_ERR_LOG(app_http_upload_init());

    return OPRT_OK;
}
