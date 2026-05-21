/**
 * @file app_http_upload.h
 * @brief WiFi HTTP image upload - phone browser uploads images via local network
 */

#ifndef __APP_HTTP_UPLOAD_H__
#define __APP_HTTP_UPLOAD_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET app_http_upload_init(void);

void app_http_upload_show_qr(void);

#ifdef __cplusplus
}
#endif

#endif
