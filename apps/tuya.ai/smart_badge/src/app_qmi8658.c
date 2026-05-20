/**
 * @file app_qmi8658.c
 * @brief QMI8658 6-axis IMU driver for smart badge
 */

#include "tkl_i2c.h"
#include "tal_log.h"
#include "tal_mutex.h"
#include "tal_system.h"
#include "app_qmi8658.h"

#define QMI8658_I2C_PORT        TUYA_I2C_NUM_0

#define QMI8658_REG_WHO_AM_I    0x00
#define QMI8658_REG_CTRL1       0x02
#define QMI8658_REG_CTRL2       0x03
#define QMI8658_REG_CTRL3       0x04
#define QMI8658_REG_CTRL7       0x08
#define QMI8658_REG_AX_L        0x35
#define QMI8658_WHO_AM_I_VAL    0x05

#define QMI8658_ACC_RANGE_4G    0x10
#define QMI8658_ACC_ODR_104HZ   0x05
#define QMI8658_GYR_RANGE_512   0x30
#define QMI8658_GYR_ODR_104HZ   0x05

/* ±4g sensitivity: 8192 LSB/g */
#define QMI8658_ACC_SENSITIVITY  8192.0f

static bool sg_inited = false;
static uint8_t sg_i2c_addr = 0;
static MUTEX_HANDLE sg_i2c_mutex = NULL;

static OPERATE_RET __i2c_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    tal_mutex_lock(sg_i2c_mutex);
    OPERATE_RET rt = tkl_i2c_master_send(QMI8658_I2C_PORT, sg_i2c_addr, buf, 2, FALSE);
    tal_mutex_unlock(sg_i2c_mutex);
    return rt;
}

static OPERATE_RET __i2c_read_reg_unlocked(uint8_t addr, uint8_t reg, uint8_t *val)
{
    OPERATE_RET rt;
    rt = tkl_i2c_master_send(QMI8658_I2C_PORT, addr, &reg, 1, TRUE);
    if (rt != OPRT_OK) return rt;
    return tkl_i2c_master_receive(QMI8658_I2C_PORT, addr, val, 1, FALSE);
}

static OPERATE_RET __probe_address(uint8_t addr)
{
    uint8_t chip_id = 0;
    tal_mutex_lock(sg_i2c_mutex);
    OPERATE_RET rt = __i2c_read_reg_unlocked(addr, QMI8658_REG_WHO_AM_I, &chip_id);
    tal_mutex_unlock(sg_i2c_mutex);
    if (rt != OPRT_OK) {
        PR_DEBUG("QMI8658 probe 0x%02X: I2C error %d", addr, rt);
        return rt;
    }
    if (chip_id != QMI8658_WHO_AM_I_VAL) {
        PR_DEBUG("QMI8658 probe 0x%02X: wrong chip_id 0x%02X", addr, chip_id);
        return OPRT_COM_ERROR;
    }
    PR_NOTICE("QMI8658 found at 0x%02X, chip_id=0x%02X", addr, chip_id);
    return OPRT_OK;
}

OPERATE_RET app_qmi8658_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (sg_i2c_mutex == NULL) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_i2c_mutex));
    }

    PR_NOTICE("QMI8658 init: probing I2C addresses...");

    /* Wait for I2C bus to settle (touch driver may still be initializing) */
    tal_system_sleep(500);

    int retry;
    for (retry = 0; retry < 5; retry++) {
        if (__probe_address(0x6B) == OPRT_OK) {
            sg_i2c_addr = 0x6B;
            break;
        }
        if (__probe_address(0x6A) == OPRT_OK) {
            sg_i2c_addr = 0x6A;
            break;
        }
        PR_NOTICE("QMI8658 probe retry %d/5...", retry + 1);
        tal_system_sleep(200);
    }

    if (sg_i2c_addr == 0) {
        PR_ERR("QMI8658 not found on I2C_NUM_0 at 0x6A or 0x6B after %d retries", retry);
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(__i2c_write_reg(QMI8658_REG_CTRL1, 0x60));
    TUYA_CALL_ERR_RETURN(__i2c_write_reg(QMI8658_REG_CTRL2, QMI8658_ACC_RANGE_4G | QMI8658_ACC_ODR_104HZ));
    TUYA_CALL_ERR_RETURN(__i2c_write_reg(QMI8658_REG_CTRL3, QMI8658_GYR_RANGE_512 | QMI8658_GYR_ODR_104HZ));
    TUYA_CALL_ERR_RETURN(__i2c_write_reg(QMI8658_REG_CTRL7, 0x03));

    sg_inited = true;
    PR_NOTICE("QMI8658 initialized at 0x%02X: ±4g, 104Hz", sg_i2c_addr);

    return OPRT_OK;
}

OPERATE_RET app_qmi8658_read_accel(float *ax, float *ay, float *az)
{
    if (!sg_inited) return OPRT_COM_ERROR;

    uint8_t buf[6] = {0};
    uint8_t reg = QMI8658_REG_AX_L;

    tal_mutex_lock(sg_i2c_mutex);
    OPERATE_RET rt = tkl_i2c_master_send(QMI8658_I2C_PORT, sg_i2c_addr, &reg, 1, TRUE);
    if (rt == OPRT_OK) {
        rt = tkl_i2c_master_receive(QMI8658_I2C_PORT, sg_i2c_addr, buf, 6, FALSE);
    }
    tal_mutex_unlock(sg_i2c_mutex);

    if (rt != OPRT_OK) return rt;

    int16_t raw_x = (int16_t)(buf[1] << 8 | buf[0]);
    int16_t raw_y = (int16_t)(buf[3] << 8 | buf[2]);
    int16_t raw_z = (int16_t)(buf[5] << 8 | buf[4]);

    *ax = (float)raw_x / QMI8658_ACC_SENSITIVITY;
    *ay = (float)raw_y / QMI8658_ACC_SENSITIVITY;
    *az = (float)raw_z / QMI8658_ACC_SENSITIVITY;

    return OPRT_OK;
}
