/**
 * @file app_gesture.h
 * @brief Gesture detection interface for smart badge
 */

#ifndef __APP_GESTURE_H__
#define __APP_GESTURE_H__

#include "tuya_cloud_types.h"

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_SHAKE,
    GESTURE_TAP,
    GESTURE_FLIP,
} GESTURE_TYPE_E;

typedef void (*GESTURE_CB_T)(GESTURE_TYPE_E gesture);

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET app_gesture_init(GESTURE_CB_T cb);
uint32_t app_gesture_get_steps(void);
void app_gesture_reset_steps(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_GESTURE_H__ */
