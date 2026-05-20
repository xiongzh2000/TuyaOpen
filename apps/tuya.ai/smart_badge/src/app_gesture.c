/**
 * @file app_gesture.c
 * @brief IMU-based gesture detection for smart badge (QMI8658)
 */

#include "tal_api.h"
#include <math.h>

#include "app_qmi8658.h"
#include "app_gesture.h"

#define GESTURE_POLL_MS         200
#define SHAKE_THRESHOLD_G       2.0f
#define SHAKE_COUNT_TRIGGER     3
#define TAP_THRESHOLD_G         2.5f
#define FLIP_Z_THRESHOLD_G      (-0.3f)

#define COOLDOWN_SHAKE_MS       1500
#define COOLDOWN_TAP_MS         800
#define COOLDOWN_FLIP_MS        2000

#define STEP_FILTER_SIZE        2
#define STEP_AVG_SIZE           10
#define STEP_MIN_AMPLITUDE      0.03f
#define STEP_MIN_INTERVAL_MS    300

static GESTURE_CB_T     sg_cb = NULL;
static THREAD_HANDLE    sg_thread = NULL;
static uint32_t         sg_cooldown_until = 0;

static uint32_t         sg_step_count = 0;
static float            sg_mag_buf[STEP_FILTER_SIZE];
static int              sg_mag_idx = 0;
static bool             sg_mag_buf_full = false;
static float            sg_step_avg_buf[STEP_AVG_SIZE];
static int              sg_step_avg_idx = 0;
static bool             sg_step_avg_full = false;
static bool             sg_step_above = false;
static uint32_t         sg_last_step_ms = 0;

static float __magnitude(float ax, float ay, float az)
{
    return sqrtf(ax * ax + ay * ay + az * az);
}

static float __step_filter_avg(void)
{
    int count = sg_mag_buf_full ? STEP_FILTER_SIZE : sg_mag_idx;
    if (count == 0) return 1.0f;
    float sum = 0;
    for (int i = 0; i < count; i++) sum += sg_mag_buf[i];
    return sum / count;
}

static float __step_running_avg(void)
{
    int count = sg_step_avg_full ? STEP_AVG_SIZE : sg_step_avg_idx;
    if (count == 0) return 1.0f;
    float sum = 0;
    for (int i = 0; i < count; i++) sum += sg_step_avg_buf[i];
    return sum / count;
}

static void __step_detect(float mag, uint32_t now)
{
    sg_mag_buf[sg_mag_idx] = mag;
    sg_mag_idx = (sg_mag_idx + 1) % STEP_FILTER_SIZE;
    if (sg_mag_idx == 0) sg_mag_buf_full = true;
    float filtered = __step_filter_avg();

    sg_step_avg_buf[sg_step_avg_idx] = filtered;
    sg_step_avg_idx = (sg_step_avg_idx + 1) % STEP_AVG_SIZE;
    if (sg_step_avg_idx == 0) sg_step_avg_full = true;
    float avg = __step_running_avg();

    if (!sg_step_above && filtered > avg + STEP_MIN_AMPLITUDE) {
        sg_step_above = true;
    } else if (sg_step_above && filtered < avg) {
        sg_step_above = false;
        if (now - sg_last_step_ms >= STEP_MIN_INTERVAL_MS) {
            sg_step_count++;
            sg_last_step_ms = now;
        }
    }
}

static void __gesture_poll_task(void *arg)
{
    (void)arg;

    int shake_cnt = 0;
    float prev_az = 1.0f;
    bool was_shaking = false;

    for (;;) {
        tal_system_sleep(GESTURE_POLL_MS);

        float ax, ay, az;
        if (app_qmi8658_read_accel(&ax, &ay, &az) != OPRT_OK) {
            continue;
        }

        float mag = __magnitude(ax, ay, az);
        uint32_t now = tal_system_get_millisecond();

        __step_detect(mag, now);

        if (now < sg_cooldown_until) {
            shake_cnt = 0;
            was_shaking = false;
            prev_az = az;
            continue;
        }

        /* Tap: sharp Z-axis spike */
        if (fabsf(az) > TAP_THRESHOLD_G && mag > TAP_THRESHOLD_G) {
            PR_NOTICE("gesture: TAP detected (az=%.2f)", az);
            if (sg_cb) sg_cb(GESTURE_TAP);
            sg_cooldown_until = now + COOLDOWN_TAP_MS;
            shake_cnt = 0;
            was_shaking = false;
            prev_az = az;
            continue;
        }

        /* Shake: sustained high acceleration */
        if (mag > SHAKE_THRESHOLD_G) {
            shake_cnt++;
            was_shaking = true;
            if (shake_cnt >= SHAKE_COUNT_TRIGGER) {
                PR_NOTICE("gesture: SHAKE detected (mag=%.2f)", mag);
                if (sg_cb) sg_cb(GESTURE_SHAKE);
                sg_cooldown_until = now + COOLDOWN_SHAKE_MS;
                shake_cnt = 0;
                was_shaking = false;
            }
        } else {
            if (was_shaking && shake_cnt > 0 && shake_cnt < SHAKE_COUNT_TRIGGER) {
                shake_cnt = 0;
            }
            was_shaking = false;
            shake_cnt = 0;
        }

        /* Flip: Z-axis crosses from positive to negative */
        if (prev_az > 0.3f && az < FLIP_Z_THRESHOLD_G && mag < 1.5f) {
            PR_NOTICE("gesture: FLIP detected (prev_az=%.2f, az=%.2f)", prev_az, az);
            if (sg_cb) sg_cb(GESTURE_FLIP);
            sg_cooldown_until = now + COOLDOWN_FLIP_MS;
        }

        prev_az = az;
    }
}

OPERATE_RET app_gesture_init(GESTURE_CB_T cb)
{
    OPERATE_RET rt = OPRT_OK;

    PR_NOTICE("gesture: initializing QMI8658...");
    rt = app_qmi8658_init();
    if (rt != OPRT_OK) {
        PR_ERR("gesture: QMI8658 init failed, rt=%d", rt);
        return rt;
    }

    sg_cb = cb;

    THREAD_CFG_T cfg = {
        .thrdname   = "gesture",
        .priority   = THREAD_PRIO_3,
        .stackDepth = 4096,
    };
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_thread, NULL, NULL,
                                                     __gesture_poll_task, NULL, &cfg));

    PR_NOTICE("gesture detection started");
    return OPRT_OK;
}

uint32_t app_gesture_get_steps(void)
{
    return sg_step_count;
}

void app_gesture_reset_steps(void)
{
    sg_step_count = 0;
}
