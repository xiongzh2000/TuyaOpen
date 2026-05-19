/**
 * @file reset_netcfg.h
 * @brief Reset network configuration interface
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __RESET_NETCFG_H__
#define __RESET_NETCFG_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int reset_netconfig_start(void);
int reset_netconfig_check(void);

#ifdef __cplusplus
}
#endif

#endif /* __RESET_NETCFG_H__ */
