/**
 * @file app_http_upload.c
 * @brief WiFi HTTP image upload server with QR code overlay
 */

#include "tal_api.h"
#include "tal_network.h"
#include "tal_wifi.h"
#include "tal_memory.h"

#include "image_album.h"
#include "ai_picture.h"
#include "app_http_upload.h"
#include "app_badge_ui.h"

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
#include "lvgl.h"
#include "lv_vendor.h"
#include "qrcodegen.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define HTTP_PORT           8080
#define HTTP_MAX_IMAGE_SIZE (512 * 1024)
#define HTTP_HDR_BUF_SIZE   1024
#define HTTP_RETRY_MS       5000

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_http_thread = NULL;
static char sg_url[64] = {0};

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
static lv_obj_t *sg_qr_overlay = NULL;
static uint8_t *sg_qr_canvas_buf = NULL;
#endif

/* Minified upload page HTML */
static const char sg_html[] =
    "<!DOCTYPE html><html><head>"
    "<meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Smart Badge</title>"
    "<style>"
    "body{font-family:-apple-system,sans-serif;text-align:center;padding:20px;background:#f5f5f5;margin:0}"
    ".c{background:#fff;border-radius:16px;padding:24px;max-width:400px;margin:20px auto;"
    "box-shadow:0 2px 8px rgba(0,0,0,.1)}"
    "h2{margin-top:0}"
    "#pv{max-width:100%;max-height:300px;margin:16px 0;display:none;border-radius:8px}"
    "input[type=file]{display:none}"
    ".b{display:inline-block;padding:12px 32px;border-radius:24px;border:none;"
    "font-size:16px;cursor:pointer;margin:8px}"
    ".bp{background:#4CAF50;color:#fff}"
    ".bs{background:#e0e0e0;color:#333}"
    "#st{margin-top:16px;font-size:14px;color:#666}"
    "</style></head><body>"
    "<div class=\"c\">"
    "<h2>Smart Badge</h2>"
    "<p>Select a photo to upload</p>"
    "<img id=\"pv\"/>"
    "<button class=\"b bs\" onclick=\"document.getElementById('f').click()\">Choose Photo</button>"
    "<input type=\"file\" id=\"f\" accept=\"image/*\" onchange=\"onF(this)\"/><br>"
    "<button class=\"b bp\" id=\"ub\" style=\"display:none\" onclick=\"go()\">Upload</button>"
    "<div id=\"st\"></div></div>"
    "<script>"
    "var bl=null;"
    "function onF(i){"
    "var f=i.files[0];if(!f)return;"
    "var im=new Image();"
    "im.onload=function(){"
    "var s=466,c=document.createElement('canvas');"
    "c.width=s;c.height=s;"
    "var x=c.getContext('2d');"
    "var sc=Math.max(s/im.width,s/im.height);"
    "var w=im.width*sc,h=im.height*sc;"
    "x.drawImage(im,(s-w)/2,(s-h)/2,w,h);"
    "c.toBlob(function(b){"
    "bl=b;"
    "document.getElementById('pv').src=URL.createObjectURL(b);"
    "document.getElementById('pv').style.display='block';"
    "document.getElementById('ub').style.display='inline-block';"
    "document.getElementById('st').textContent='Ready: '+Math.round(b.size/1024)+' KB';"
    "},'image/jpeg',0.85);};"
    "im.src=URL.createObjectURL(f);}"
    "function go(){"
    "if(!bl)return;"
    "document.getElementById('st').textContent='Uploading...';"
    "document.getElementById('ub').disabled=true;"
    "fetch('/upload',{method:'POST',body:bl,"
    "headers:{'Content-Type':'image/jpeg'}})"
    ".then(function(r){return r.json()})"
    ".then(function(d){"
    "document.getElementById('st').textContent="
    "d.status==='ok'?'Upload successful!':'Upload failed';"
    "document.getElementById('ub').disabled=false;})"
    ".catch(function(e){"
    "document.getElementById('st').textContent='Error: '+e.message;"
    "document.getElementById('ub').disabled=false;});}"
    "</script></body></html>";

/***********************************************************
***********************function define**********************
***********************************************************/

static int __send_all(int fd, const void *data, int len)
{
    const uint8_t *p = (const uint8_t *)data;
    while (len > 0) {
        int n = tal_net_send(fd, p, len);
        if (n <= 0) {
            return -1;
        }
        p += n;
        len -= n;
    }
    return 0;
}

static void __send_response(int fd, int code, const char *content_type,
                            const void *body, int body_len)
{
    char hdr[256];
    const char *status_text = (code == 200) ? "OK" : "Not Found";
    int hdr_len = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        code, status_text, content_type, body_len);

    __send_all(fd, hdr, hdr_len);
    if (body && body_len > 0) {
        __send_all(fd, body, body_len);
    }
}

static int __parse_content_length(const char *headers)
{
    const char *p = strstr(headers, "Content-Length:");
    if (!p) {
        p = strstr(headers, "content-length:");
    }
    if (!p) {
        return -1;
    }
    return atoi(p + 15);
}

static void __handle_upload(int fd, const char *hdr_buf, int hdr_total,
                            int hdr_end_offset)
{
    int content_length = __parse_content_length(hdr_buf);
    if (content_length <= 0 || content_length > HTTP_MAX_IMAGE_SIZE) {
        const char *resp = "{\"status\":\"error\",\"msg\":\"invalid size\"}";
        __send_response(fd, 200, "application/json", resp, strlen(resp));
        return;
    }

    uint8_t *img_buf = (uint8_t *)tal_psram_malloc(content_length);
    if (!img_buf) {
        const char *resp = "{\"status\":\"error\",\"msg\":\"no memory\"}";
        __send_response(fd, 200, "application/json", resp, strlen(resp));
        return;
    }

    /* Copy body data already received in the header buffer */
    int body_in_hdr = hdr_total - hdr_end_offset;
    if (body_in_hdr > content_length) {
        body_in_hdr = content_length;
    }
    if (body_in_hdr > 0) {
        memcpy(img_buf, hdr_buf + hdr_end_offset, body_in_hdr);
    }

    int received = body_in_hdr;
    while (received < content_length) {
        int to_read = content_length - received;
        if (to_read > 4096) {
            to_read = 4096;
        }
        int n = tal_net_recv(fd, img_buf + received, to_read);
        if (n <= 0) {
            break;
        }
        received += n;
    }

    const char *resp;
    if (received == content_length) {
        char *album_name = ai_picture_get_album_name();
        IMAGE_ALBUM_HANDLE album = image_album_find_by_name(album_name);
        if (album) {
            char filename[32];
            snprintf(filename, sizeof(filename), "http_%u.jpg",
                     (uint32_t)(tal_time_get_posix() & 0xFFFFFFFF));

            ALBUM_IMAGE_SAVE_INFO_T info = {
                .filename  = filename,
                .format    = ALBUM_IMAGE_FORMAT_JPEG,
                .file_data = img_buf,
                .file_size = received,
                .seq       = 0,
                .timestamp = tal_time_get_posix_ms(),
            };

            OPERATE_RET rt = image_album_save(album, &info);
            if (rt == OPRT_OK) {
                PR_INFO("http_upload: saved %s (%d bytes)", filename, received);
                app_badge_ui_album_add_jpeg(img_buf, received);
                resp = "{\"status\":\"ok\"}";
            } else {
                PR_ERR("http_upload: save failed %d", rt);
                resp = "{\"status\":\"error\",\"msg\":\"save failed\"}";
            }
        } else {
            resp = "{\"status\":\"error\",\"msg\":\"album not found\"}";
        }
    } else {
        PR_ERR("http_upload: incomplete %d/%d", received, content_length);
        resp = "{\"status\":\"error\",\"msg\":\"incomplete\"}";
    }

    __send_response(fd, 200, "application/json", resp, strlen(resp));
    tal_psram_free(img_buf);
}

static void __handle_client(int client_fd)
{
    char hdr_buf[HTTP_HDR_BUF_SIZE];
    int total = 0;
    int hdr_end = -1;

    /* Read until we find \r\n\r\n or buffer full */
    while (total < HTTP_HDR_BUF_SIZE - 1) {
        int n = tal_net_recv(client_fd, hdr_buf + total, HTTP_HDR_BUF_SIZE - 1 - total);
        if (n <= 0) {
            return;
        }
        total += n;
        hdr_buf[total] = '\0';

        char *end = strstr(hdr_buf, "\r\n\r\n");
        if (end) {
            hdr_end = (int)(end - hdr_buf) + 4;
            break;
        }
    }

    if (hdr_end < 0) {
        return;
    }

    if (strncmp(hdr_buf, "GET / ", 6) == 0 || strncmp(hdr_buf, "GET / ", 6) == 0) {
        __send_response(client_fd, 200, "text/html; charset=utf-8",
                        sg_html, sizeof(sg_html) - 1);
    } else if (strncmp(hdr_buf, "POST /upload", 12) == 0) {
        __handle_upload(client_fd, hdr_buf, total, hdr_end);
    } else {
        const char *body = "404 Not Found";
        __send_response(client_fd, 404, "text/plain", body, strlen(body));
    }
}

static void __http_server_task(void *arg)
{
    int server_fd = -1;

    while (1) {
        server_fd = tal_net_socket_create(PROTOCOL_TCP);
        if (server_fd < 0) {
            PR_ERR("http_upload: socket create failed");
            tal_system_sleep(HTTP_RETRY_MS);
            continue;
        }

        if (tal_net_bind(server_fd, 0, HTTP_PORT) != 0) {
            PR_ERR("http_upload: bind failed");
            tal_net_close(server_fd);
            tal_system_sleep(HTTP_RETRY_MS);
            continue;
        }

        if (tal_net_listen(server_fd, 2) != 0) {
            PR_ERR("http_upload: listen failed");
            tal_net_close(server_fd);
            tal_system_sleep(HTTP_RETRY_MS);
            continue;
        }

        PR_INFO("http_upload: listening on port %d", HTTP_PORT);
        break;
    }

    while (1) {
        int client_fd = tal_net_accept(server_fd, NULL, NULL);
        if (client_fd <= 0) {
            tal_system_sleep(100);
            continue;
        }

        __handle_client(client_fd);
        tal_net_close(client_fd);
    }
}

static void __update_url(void)
{
    NW_IP_S ip = {0};
    OPERATE_RET rt = tal_wifi_get_ip(WF_STATION, &ip);
    if (rt != OPRT_OK || strlen(ip.ip) == 0) {
        sg_url[0] = '\0';
        return;
    }
    snprintf(sg_url, sizeof(sg_url), "http://%s:%d", ip.ip, HTTP_PORT);
    PR_INFO("http_upload: URL = %s", sg_url);
}

/* ==================== QR Code Overlay ==================== */

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)

static void __qr_dismiss_cb(lv_event_t *e)
{
    (void)e;
    lv_vendor_disp_lock();
    if (sg_qr_overlay) {
        lv_obj_delete(sg_qr_overlay);
        sg_qr_overlay = NULL;
    }
    if (sg_qr_canvas_buf) {
        tal_psram_free(sg_qr_canvas_buf);
        sg_qr_canvas_buf = NULL;
    }
    lv_vendor_disp_unlock();
}

void app_http_upload_show_qr(void)
{
    __update_url();
    if (sg_url[0] == '\0') {
        PR_WARN("http_upload: no IP, cannot show QR");
        return;
    }

    /* Generate QR code data */
    uint8_t qr_data[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    uint8_t qr_temp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];

    bool ok = qrcodegen_encodeText(sg_url, qr_temp, qr_data,
        qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN, 10,
        qrcodegen_Mask_AUTO, true);
    if (!ok) {
        PR_ERR("http_upload: QR encode failed");
        return;
    }

    int qr_size = qrcodegen_getSize(qr_data);
    int margin = 4;
    int total_modules = qr_size + margin * 2;
    int scale = 240 / total_modules;
    if (scale < 2) scale = 2;
    int img_px = total_modules * scale;

    /* Allocate canvas buffer (RGB565) */
    int canvas_bytes = img_px * img_px * 2;
    if (sg_qr_canvas_buf) {
        tal_psram_free(sg_qr_canvas_buf);
    }
    sg_qr_canvas_buf = (uint8_t *)tal_psram_malloc(canvas_bytes);
    if (!sg_qr_canvas_buf) {
        PR_ERR("http_upload: QR canvas alloc failed");
        return;
    }

    /* Fill white */
    memset(sg_qr_canvas_buf, 0xFF, canvas_bytes);

    /* Draw black modules */
    uint16_t *pixels = (uint16_t *)sg_qr_canvas_buf;
    for (int y = 0; y < qr_size; y++) {
        for (int x = 0; x < qr_size; x++) {
            if (qrcodegen_getModule(qr_data, x, y)) {
                int px = (x + margin) * scale;
                int py = (y + margin) * scale;
                for (int dy = 0; dy < scale; dy++) {
                    for (int dx = 0; dx < scale; dx++) {
                        pixels[(py + dy) * img_px + (px + dx)] = 0x0000;
                    }
                }
            }
        }
    }

    lv_vendor_disp_lock();

    if (sg_qr_overlay) {
        lv_obj_delete(sg_qr_overlay);
    }

    sg_qr_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(sg_qr_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(sg_qr_overlay, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(sg_qr_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_qr_overlay, 0, 0);
    lv_obj_set_style_radius(sg_qr_overlay, 0, 0);
    lv_obj_set_style_pad_all(sg_qr_overlay, 0, 0);
    lv_obj_center(sg_qr_overlay);
    lv_obj_add_flag(sg_qr_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sg_qr_overlay, __qr_dismiss_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(sg_qr_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(sg_qr_overlay);
    lv_label_set_text(title, "Scan to Upload");
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

    /* QR canvas */
    lv_obj_t *canvas = lv_canvas_create(sg_qr_overlay);
    lv_canvas_set_buffer(canvas, sg_qr_canvas_buf, img_px, img_px,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(canvas, img_px, img_px);
    lv_obj_center(canvas);

    /* URL hint */
    lv_obj_t *hint = lv_label_create(sg_qr_overlay);
    lv_label_set_text(hint, sg_url);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -80);

    /* Dismiss hint */
    lv_obj_t *tap_hint = lv_label_create(sg_qr_overlay);
    lv_label_set_text(tap_hint, "Tap to close");
    lv_obj_set_style_text_color(tap_hint, lv_color_hex(0xBBBBBB), 0);
    lv_obj_set_style_text_align(tap_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(tap_hint, LV_ALIGN_BOTTOM_MID, 0, -50);

    lv_vendor_disp_unlock();
}

#else

void app_http_upload_show_qr(void)
{
    __update_url();
    PR_INFO("http_upload: QR URL = %s", sg_url);
}

#endif

/* ==================== Init ==================== */

OPERATE_RET app_http_upload_init(void)
{
    __update_url();

    THREAD_CFG_T cfg = {
        .stackDepth = 4096 + 1024,
        .priority   = THREAD_PRIO_1,
        .thrdname   = "http_upload",
    };

    OPERATE_RET rt = tal_thread_create_and_start(
        &sg_http_thread, NULL, NULL, __http_server_task, NULL, &cfg);
    if (rt != OPRT_OK) {
        PR_ERR("http_upload: thread create failed %d", rt);
        return rt;
    }

    PR_INFO("http_upload: server started");
    return OPRT_OK;
}
