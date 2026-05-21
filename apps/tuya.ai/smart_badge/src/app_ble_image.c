/**
 * @file app_ble_image.c
 * @brief BLE image transfer - receive JPEG from phone via custom NimBLE GATT service
 */

#include "ble_gatt.h"
#include "ble_uuid.h"
#include "ble_att.h"
#include "ble_hs_mbuf.h"
#include "tuya_ble_mbuf.h"

#undef LIST_HEAD

#include "tal_api.h"
#include "tal_memory.h"

#include "image_album.h"
#include "ai_picture.h"
#include "app_ble_image.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define CMD_START  0x01
#define CMD_FINISH 0x02
#define RSP_READY  0x81
#define RSP_DONE   0x82

#define STATUS_OK            0
#define STATUS_SIZE_MISMATCH 1
#define STATUS_NO_MEMORY     2
#define STATUS_SAVE_FAILED   3

#define BLE_IMAGE_MAX_SIZE (512 * 1024)

/***********************************************************
***********************variable define**********************
***********************************************************/

/* Service UUID:  12340001-5678-1234-5678-123456789ABC (LE byte order) */
static const ble_uuid128_t sg_svc_uuid = BLE_UUID128_INIT(
    0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12, 0x78, 0x56,
    0x34, 0x12, 0x78, 0x56, 0x01, 0x00, 0x34, 0x12
);

/* Control UUID: 12340002-5678-1234-5678-123456789ABC */
static const ble_uuid128_t sg_ctrl_uuid = BLE_UUID128_INIT(
    0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12, 0x78, 0x56,
    0x34, 0x12, 0x78, 0x56, 0x02, 0x00, 0x34, 0x12
);

/* Data UUID:    12340003-5678-1234-5678-123456789ABC */
static const ble_uuid128_t sg_data_uuid = BLE_UUID128_INIT(
    0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12, 0x78, 0x56,
    0x34, 0x12, 0x78, 0x56, 0x03, 0x00, 0x34, 0x12
);

static struct {
    uint8_t  *buf;
    uint32_t  total_size;
    uint32_t  received;
    uint16_t  conn_handle;
    uint16_t  ctrl_handle;
    BOOL_T    active;
} sg_rx = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static int __ctrl_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);
static int __data_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);

static struct ble_gatt_chr_def sg_chars[] = {
    {
        .uuid      = &sg_ctrl_uuid.u,
        .access_cb = __ctrl_access_cb,
        .flags     = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &sg_rx.ctrl_handle,
    },
    {
        .uuid      = &sg_data_uuid.u,
        .access_cb = __data_access_cb,
        .flags     = BLE_GATT_CHR_F_WRITE_NO_RSP,
    },
    { 0 }
};

static const struct ble_gatt_svc_def sg_svcs[] = {
    {
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &sg_svc_uuid.u,
        .characteristics = sg_chars,
    },
    { 0 }
};

static void __send_notify(uint8_t rsp_type, uint8_t status, const void *data, uint16_t data_len)
{
    if (sg_rx.conn_handle == 0 || sg_rx.ctrl_handle == 0) {
        return;
    }

    uint8_t buf[8];
    uint16_t len = 0;

    buf[len++] = rsp_type;
    buf[len++] = status;
    if (data && data_len > 0 && (len + data_len) <= sizeof(buf)) {
        memcpy(&buf[len], data, data_len);
        len += data_len;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, len);
    if (om) {
        ble_gattc_notify_custom(sg_rx.conn_handle, sg_rx.ctrl_handle, om);
    }
}

static void __handle_start(uint16_t conn_handle, uint32_t total_size)
{
    if (sg_rx.buf) {
        tal_psram_free(sg_rx.buf);
        sg_rx.buf = NULL;
    }

    if (total_size == 0 || total_size > BLE_IMAGE_MAX_SIZE) {
        PR_ERR("ble_img: invalid size %u", total_size);
        sg_rx.conn_handle = conn_handle;
        __send_notify(RSP_READY, STATUS_NO_MEMORY, NULL, 0);
        return;
    }

    sg_rx.buf = (uint8_t *)tal_psram_malloc(total_size);
    if (!sg_rx.buf) {
        PR_ERR("ble_img: alloc %u failed", total_size);
        sg_rx.conn_handle = conn_handle;
        __send_notify(RSP_READY, STATUS_NO_MEMORY, NULL, 0);
        return;
    }

    sg_rx.total_size  = total_size;
    sg_rx.received    = 0;
    sg_rx.conn_handle = conn_handle;
    sg_rx.active      = TRUE;

    uint16_t mtu = ble_att_mtu(conn_handle);
    PR_INFO("ble_img: START size=%u mtu=%u", total_size, mtu);

    __send_notify(RSP_READY, STATUS_OK, &mtu, sizeof(mtu));
}

static void __handle_finish(void)
{
    if (!sg_rx.active || !sg_rx.buf) {
        return;
    }

    sg_rx.active = FALSE;
    PR_INFO("ble_img: FINISH recv=%u expect=%u", sg_rx.received, sg_rx.total_size);

    uint8_t status = STATUS_OK;

    if (sg_rx.received != sg_rx.total_size) {
        status = STATUS_SIZE_MISMATCH;
        PR_ERR("ble_img: size mismatch");
    } else {
        char *album_name = ai_picture_get_album_name();
        IMAGE_ALBUM_HANDLE album = image_album_find_by_name(album_name);
        if (album) {
            char filename[32];
            snprintf(filename, sizeof(filename), "ble_%u.jpg",
                     (uint32_t)(tal_time_get_posix() & 0xFFFFFFFF));

            ALBUM_IMAGE_SAVE_INFO_T info = {
                .filename  = filename,
                .format    = ALBUM_IMAGE_FORMAT_JPEG,
                .file_data = sg_rx.buf,
                .file_size = sg_rx.received,
                .seq       = 0,
                .timestamp = tal_time_get_posix_ms(),
            };

            OPERATE_RET rt = image_album_save(album, &info);
            if (rt != OPRT_OK) {
                PR_ERR("ble_img: save failed %d", rt);
                status = STATUS_SAVE_FAILED;
            } else {
                PR_INFO("ble_img: saved %s (%u bytes)", filename, sg_rx.received);
            }
        } else {
            PR_ERR("ble_img: album not found");
            status = STATUS_SAVE_FAILED;
        }
    }

    __send_notify(RSP_DONE, status, &sg_rx.received, sizeof(sg_rx.received));

    tal_psram_free(sg_rx.buf);
    sg_rx.buf        = NULL;
    sg_rx.received   = 0;
    sg_rx.total_size = 0;
}

static int __ctrl_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return 0;
    }

    uint16_t len = os_mbuf_len(ctxt->om);
    if (len < 1) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint8_t cmd;
    os_mbuf_copydata(ctxt->om, 0, 1, &cmd);

    switch (cmd) {
    case CMD_START: {
        if (len < 5) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint32_t total_size = 0;
        os_mbuf_copydata(ctxt->om, 1, 4, &total_size);
        __handle_start(conn_handle, total_size);
        break;
    }
    case CMD_FINISH:
        __handle_finish();
        break;
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

static int __data_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return 0;
    }

    if (!sg_rx.active || !sg_rx.buf) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint16_t len = os_mbuf_len(ctxt->om);
    if (sg_rx.received + len > sg_rx.total_size) {
        PR_ERR("ble_img: overflow %u+%u > %u", sg_rx.received, len, sg_rx.total_size);
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    os_mbuf_copydata(ctxt->om, 0, len, sg_rx.buf + sg_rx.received);
    sg_rx.received += len;

    return 0;
}

OPERATE_RET app_ble_image_init(void)
{
    int rc;

    ble_att_set_preferred_mtu(512);

    rc = ble_gatts_count_cfg(sg_svcs);
    if (rc != 0) {
        PR_ERR("ble_img: count_cfg failed %d", rc);
        return OPRT_COM_ERROR;
    }

    rc = ble_gatts_add_svcs(sg_svcs);
    if (rc != 0) {
        PR_ERR("ble_img: add_svcs failed %d", rc);
        return OPRT_COM_ERROR;
    }

    ble_gatts_start();

    PR_INFO("ble_img: image transfer service registered");
    return OPRT_OK;
}
