/**
 * @file app_qmi8658.h
 * @brief QMI8658 6-axis IMU driver interface
 */

#ifndef __APP_QMI8658_H__
#define __APP_QMI8658_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET app_qmi8658_init(void);
OPERATE_RET app_qmi8658_read_accel(float *ax, float *ay, float *az);

#ifdef __cplusplus
}
#endif

#endif /* __APP_QMI8658_H__ */
