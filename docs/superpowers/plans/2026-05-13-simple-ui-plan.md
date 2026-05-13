# Simple UI Style Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a fourth UI style "simple" — full-screen text + image pages with auto-print, no chat bubbles, no album.

**Architecture:** New `simple/` directory under `ai_ui/src/` containing two files. Registers `AI_UI_INTFS_T` and `AI_UI_CHAT_INTFS_T` callbacks. Wired in via Kconfig choice and `ai_chat_ui.c` ifdef ladder. Three LVGL pages (idle/text/image) swap visibility.

**Tech Stack:** C, LVGL v8, TuyaOpen TAL API, Kconfig

---

### Task 1: Kconfig — add ENABLE_AI_CHAT_GUI_SIMPLE choice

**Files:**
- Modify: `apps/tuya.ai/ai_components/ai_ui/Kconfig:16-27`

- [ ] **Step 1: Add the new choice entry**

In `apps/tuya.ai/ai_components/ai_ui/Kconfig`, inside the existing `choice` block (line 12-27), add `ENABLE_AI_CHAT_GUI_SIMPLE` after the OLED entry (line 24-25) and before `endchoice` (line 27):

```kconfig
        config ENABLE_AI_CHAT_GUI_SIMPLE
            select ENABLE_LIBLVGL
            bool "Simple style (text + image, auto-print)"
```

The resulting choice block should be:
```kconfig
    choice
        prompt "choose present ia chat ui"
        default ENABLE_AI_CHAT_GUI_WECHAT

        config ENABLE_AI_CHAT_GUI_WECHAT
            select ENABLE_LIBLVGL
            bool "Use WeChat-like ui"

        config ENABLE_AI_CHAT_GUI_CHATBOT
            select ENABLE_LIBLVGL
            bool "Use Chatbot ui"

        config ENABLE_AI_CHAT_GUI_OLED
            select ENABLE_LIBLVGL
            bool "Use OLED ui"

        config ENABLE_AI_CHAT_GUI_SIMPLE
            select ENABLE_LIBLVGL
            bool "Simple style (text + image, auto-print)"
    endchoice
```

- [ ] **Step 2: Commit**

```bash
git add apps/tuya.ai/ai_components/ai_ui/Kconfig
git commit -m "feat(ui): add ENABLE_AI_CHAT_GUI_SIMPLE Kconfig choice"
```

---

### Task 2: Header — create ai_ui_chat_simple.h

**Files:**
- Create: `apps/tuya.ai/ai_components/ai_ui/include/ai_ui_chat_simple.h`

- [ ] **Step 1: Create the header file**

```c
#ifndef __AI_CHAT_UI_SIMPLE_H__
#define __AI_CHAT_UI_SIMPLE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET ai_ui_chat_simple_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_CHAT_UI_SIMPLE_H__ */
```

- [ ] **Step 2: Commit**

```bash
git add apps/tuya.ai/ai_components/ai_ui/include/ai_ui_chat_simple.h
git commit -m "feat(ui): add simple UI header"
```

---

### Task 3: Main file — create ai_ui_simple_main.c

**Files:**
- Create: `apps/tuya.ai/ai_components/ai_ui/src/simple/ai_ui_simple_main.c`

This file handles: LVGL init, idle page (with "长按说话" hint and Wi-Fi icon), status/notification updates, and the `ai_ui_chat_simple_register()` entry point.

- [ ] **Step 1: Create the file**

```c
/**
 * @file ai_ui_simple_main.c
 * @brief Simple UI — main entry: LVGL init, idle page, status bar, register.
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_AI_CHAT_GUI_SIMPLE) && (ENABLE_AI_CHAT_GUI_SIMPLE == 1)

#include "lv_vendor.h"
#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"
#include "font_awesome_symbols.h"
#include "lang_config.h"

/* Defined in ai_ui_simple_chat.c */
extern void ai_ui_simple_chat_init(lv_obj_t *parent);
extern void ai_ui_simple_chat_register(void);

/***********************************************************
***********************variable define**********************
***********************************************************/
typedef struct {
    lv_obj_t *idle_page;
    lv_obj_t *hint_label;
    lv_obj_t *network_label;
    lv_obj_t *status_label;
} AI_UI_SIMPLE_MAIN_T;

static AI_UI_SIMPLE_MAIN_T sg_main = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __lvgl_init(void)
{
    lv_vendor_init(DISPLAY_NAME);
    lv_vendor_start(5, 1024 * 8);
}

static void __ui_set_emotion(char *emotion)
{
    (void)emotion;
}

static void __ui_set_status(char *status)
{
    if (sg_main.status_label == NULL || status == NULL) {
        return;
    }
    lv_vendor_disp_lock();
    lv_label_set_text(sg_main.status_label, status);
    lv_vendor_disp_unlock();
}

static void __ui_set_notification(char *notification)
{
    if (sg_main.status_label == NULL || notification == NULL) {
        return;
    }
    lv_vendor_disp_lock();
    lv_label_set_text(sg_main.status_label, notification);
    lv_vendor_disp_unlock();
}

static void __ui_set_network(AI_UI_WIFI_STATUS_E wifi_status)
{
    if (sg_main.network_label == NULL) {
        return;
    }
    const char *icon = "";
    switch (wifi_status) {
    case AI_UI_WIFI_STATUS_DISCONNECTED:
        icon = FONT_AWESOME_WIFI_OFF;
        break;
    case AI_UI_WIFI_STATUS_GOOD:
        icon = FONT_AWESOME_WIFI;
        break;
    case AI_UI_WIFI_STATUS_FAIR:
        icon = FONT_AWESOME_WIFI_FAIR;
        break;
    case AI_UI_WIFI_STATUS_WEAK:
        icon = FONT_AWESOME_WIFI_WEAK;
        break;
    default:
        break;
    }
    lv_vendor_disp_lock();
    lv_label_set_text(sg_main.network_label, icon);
    lv_vendor_disp_unlock();
}

static void __ui_set_chat_mode(char *mode)
{
    (void)mode;
}

void ai_ui_simple_show_idle(void)
{
    if (sg_main.idle_page) {
        lv_obj_clear_flag(sg_main.idle_page, LV_OBJ_FLAG_HIDDEN);
    }
}

void ai_ui_simple_hide_idle(void)
{
    if (sg_main.idle_page) {
        lv_obj_add_flag(sg_main.idle_page, LV_OBJ_FLAG_HIDDEN);
    }
}

static OPERATE_RET __ui_init(void)
{
    __lvgl_init();

    lv_vendor_disp_lock();

    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    const lv_font_t *text_font = ai_ui_get_text_font();
    const lv_font_t *icon_font = ai_ui_get_icon_font();

    /* Root screen */
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    /* ── Idle page ── */
    sg_main.idle_page = lv_obj_create(screen);
    lv_obj_set_size(sg_main.idle_page, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(sg_main.idle_page, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_main.idle_page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_main.idle_page, 0, 0);
    lv_obj_set_style_pad_all(sg_main.idle_page, 0, 0);
    lv_obj_set_style_radius(sg_main.idle_page, 0, 0);
    lv_obj_clear_flag(sg_main.idle_page, LV_OBJ_FLAG_SCROLLABLE);

    /* Hint text */
    sg_main.hint_label = lv_label_create(sg_main.idle_page);
    lv_obj_set_style_text_font(sg_main.hint_label, text_font, 0);
    lv_obj_set_style_text_color(sg_main.hint_label, lv_color_hex(0x888888), 0);
    lv_label_set_text(sg_main.hint_label, HOLD_TALK);
    lv_obj_center(sg_main.hint_label);

    /* Status label (below hint) */
    sg_main.status_label = lv_label_create(sg_main.idle_page);
    lv_obj_set_style_text_font(sg_main.status_label, text_font, 0);
    lv_obj_set_style_text_color(sg_main.status_label, lv_color_hex(0x666666), 0);
    lv_label_set_text(sg_main.status_label, INITIALIZING);
    lv_obj_align(sg_main.status_label, LV_ALIGN_BOTTOM_MID, 0, -40);

    /* Wi-Fi icon (top-right) */
    sg_main.network_label = lv_label_create(sg_main.idle_page);
    lv_obj_set_style_text_font(sg_main.network_label, icon_font, 0);
    lv_obj_set_style_text_color(sg_main.network_label, lv_color_white(), 0);
    lv_label_set_text(sg_main.network_label, "");
    lv_obj_align(sg_main.network_label, LV_ALIGN_TOP_RIGHT, -8, 8);

    /* ── Init chat sub-page ── */
    ai_ui_simple_chat_init(screen);

    lv_vendor_disp_unlock();

    return OPRT_OK;
}

OPERATE_RET ai_ui_chat_simple_register(void)
{
    AI_UI_INTFS_T intfs;
    memset(&intfs, 0, sizeof(AI_UI_INTFS_T));

    intfs.disp_init          = __ui_init;
    intfs.disp_emotion       = __ui_set_emotion;
    intfs.disp_ai_mode_state = __ui_set_status;
    intfs.disp_notification  = __ui_set_notification;
    intfs.disp_wifi_state    = __ui_set_network;
    intfs.disp_ai_chat_mode  = __ui_set_chat_mode;

    ai_ui_register(&intfs);

    ai_ui_simple_chat_register();

    return OPRT_OK;
}

#endif /* ENABLE_AI_CHAT_GUI_SIMPLE */
```

- [ ] **Step 2: Commit**

```bash
git add apps/tuya.ai/ai_components/ai_ui/src/simple/ai_ui_simple_main.c
git commit -m "feat(ui): add simple UI main — idle page, status, register"
```

---

### Task 4: Chat file — create ai_ui_simple_chat.c

**Files:**
- Create: `apps/tuya.ai/ai_components/ai_ui/src/simple/ai_ui_simple_chat.c`

This file handles: text page (ASR + AI reply), image page (full-screen display + auto-print), `AI_UI_CHAT_INTFS_T` registration.

- [ ] **Step 1: Create the file**

```c
/**
 * @file ai_ui_simple_chat.c
 * @brief Simple UI — text page (ASR + AI streaming) and image page (full-screen + auto-print).
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_AI_CHAT_GUI_SIMPLE) && (ENABLE_AI_CHAT_GUI_SIMPLE == 1)

#include "lvgl.h"
#include "lv_vendor.h"
#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"
#include "lang_config.h"
#include "tal_image_jpeg_codec.h"

/* From ai_ui_simple_main.c */
extern void ai_ui_simple_show_idle(void);
extern void ai_ui_simple_hide_idle(void);

/***********************************************************
************************macro define************************
***********************************************************/
#define IDLE_RETURN_TIMEOUT_MS  5000
#define ALBUM_FILENAME_MAX_LEN  64

/***********************************************************
***********************variable define**********************
***********************************************************/
typedef struct {
    lv_obj_t *text_page;
    lv_obj_t *user_label;
    lv_obj_t *ai_label;

    lv_obj_t *image_page;
    lv_obj_t *image_canvas;
    lv_obj_t *print_status_label;
    lv_timer_t *print_status_tm;

    lv_timer_t *idle_return_tm;

    char cur_img_name[ALBUM_FILENAME_MAX_LEN + 1];
} AI_UI_SIMPLE_CHAT_T;

static AI_UI_SIMPLE_CHAT_T sg_chat = {0};
static uint8_t *sg_picture_buffer = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

/* ── helpers ── */

static void __show_text_page(void)
{
    ai_ui_simple_hide_idle();
    lv_obj_clear_flag(sg_chat.text_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_chat.image_page, LV_OBJ_FLAG_HIDDEN);
}

static void __show_image_page(void)
{
    ai_ui_simple_hide_idle();
    lv_obj_add_flag(sg_chat.text_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_chat.image_page, LV_OBJ_FLAG_HIDDEN);
}

static void __idle_return_cb(lv_timer_t *timer)
{
    (void)timer;
    lv_timer_del(sg_chat.idle_return_tm);
    sg_chat.idle_return_tm = NULL;

    lv_obj_add_flag(sg_chat.text_page, LV_OBJ_FLAG_HIDDEN);
    ai_ui_simple_show_idle();
}

static void __cancel_idle_return(void)
{
    if (sg_chat.idle_return_tm) {
        lv_timer_del(sg_chat.idle_return_tm);
        sg_chat.idle_return_tm = NULL;
    }
}

static void __schedule_idle_return(void)
{
    __cancel_idle_return();
    sg_chat.idle_return_tm = lv_timer_create(__idle_return_cb, IDLE_RETURN_TIMEOUT_MS, NULL);
    lv_timer_set_repeat_count(sg_chat.idle_return_tm, 1);
}

/* ── print status overlay ── */

static void __print_status_timeout_cb(lv_timer_t *timer)
{
    (void)timer;
    lv_timer_del(sg_chat.print_status_tm);
    sg_chat.print_status_tm = NULL;
    if (sg_chat.print_status_label) {
        lv_obj_add_flag(sg_chat.print_status_label, LV_OBJ_FLAG_HIDDEN);
    }
}

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
static void __disp_print_result(bool ok)
{
    lv_vendor_disp_lock();
    if (sg_chat.print_status_label == NULL) {
        lv_vendor_disp_unlock();
        return;
    }
    lv_obj_set_style_text_font(sg_chat.print_status_label, ai_ui_get_text_font(), 0);
    lv_label_set_text(sg_chat.print_status_label, ok ? PRINT_SUCCESS : PRINT_FAILED);
    lv_obj_clear_flag(sg_chat.print_status_label, LV_OBJ_FLAG_HIDDEN);
    if (sg_chat.print_status_tm == NULL) {
        sg_chat.print_status_tm = lv_timer_create(__print_status_timeout_cb, 2000, NULL);
    } else {
        lv_timer_reset(sg_chat.print_status_tm);
    }
    lv_timer_set_repeat_count(sg_chat.print_status_tm, 1);
    lv_vendor_disp_unlock();
}
#endif

/* ── chat interface callbacks ── */

static void __ui_open_chat(void)
{
    __cancel_idle_return();
    lv_vendor_disp_lock();
    lv_label_set_text(sg_chat.user_label, "");
    lv_label_set_text(sg_chat.ai_label, "");
    __show_text_page();
    lv_vendor_disp_unlock();
}

static void __ui_close_chat(void)
{
    /* If image page is showing, stay there (persistent). Otherwise go idle. */
    if (lv_obj_has_flag(sg_chat.image_page, LV_OBJ_FLAG_HIDDEN)) {
        __schedule_idle_return();
    }
}

static void __ui_set_user_msg(char *msg)
{
    if (sg_chat.user_label == NULL || msg == NULL) {
        return;
    }
    lv_vendor_disp_lock();
    lv_label_set_text(sg_chat.user_label, msg);
    lv_vendor_disp_unlock();
}

static void __ui_set_ai_msg(char *msg)
{
    if (sg_chat.ai_label == NULL || msg == NULL) {
        return;
    }
    lv_vendor_disp_lock();
    lv_label_set_text(sg_chat.ai_label, msg);
    lv_vendor_disp_unlock();
}

static void __ui_stream_start(void)
{
    lv_vendor_disp_lock();
    lv_label_set_text(sg_chat.ai_label, "");
    lv_vendor_disp_unlock();
}

static void __ui_stream_data(char *data)
{
    if (sg_chat.ai_label == NULL || data == NULL) {
        return;
    }
    lv_vendor_disp_lock();
    lv_label_ins_text(sg_chat.ai_label, LV_LABEL_POS_LAST, data);
    lv_vendor_disp_unlock();
}

static void __ui_stream_end(void)
{
    /* nothing special */
}

static void __ui_disp_image(AI_UI_IMG_T *img)
{
    if (img == NULL || img->data == NULL || img->len == 0) {
        return;
    }

    __cancel_idle_return();

    uint8_t  *jpeg = img->data;
    uint32_t  len  = img->len;

    if (img->name != NULL) {
        strncpy(sg_chat.cur_img_name, img->name, ALBUM_FILENAME_MAX_LEN);
        sg_chat.cur_img_name[ALBUM_FILENAME_MAX_LEN] = '\0';
    } else {
        sg_chat.cur_img_name[0] = '\0';
    }

    TAL_IMAGE_JPEG_INFO_T info = {0};
    if (tal_image_jpeg_get_info(jpeg, len, &info) != OPRT_OK) {
        PR_ERR("simple: jpeg get info failed");
        return;
    }

    uint32_t rgb565_size = info.width * info.height * 2;
    uint8_t *rgb565_buf = Malloc(rgb565_size);
    if (rgb565_buf == NULL) {
        PR_ERR("simple: malloc rgb565 failed, size=%u", rgb565_size);
        return;
    }
    memset(rgb565_buf, 0, rgb565_size);

    TAL_IMAGE_JPEG_OUTPUT_T out = {0};
    out.out_buf      = rgb565_buf;
    out.out_buf_size = rgb565_size;
    out.out_width    = info.width;
    out.out_height   = info.height;

    if (tal_image_jpeg_decode_rgb565(jpeg, len, &out) != OPRT_OK) {
        PR_ERR("simple: jpeg decode rgb565 failed");
        Free(rgb565_buf);
        return;
    }

    lv_vendor_disp_lock();

    if (sg_chat.image_canvas == NULL) {
        sg_chat.image_canvas = lv_canvas_create(sg_chat.image_page);
    }

    if (sg_picture_buffer) {
        Free(sg_picture_buffer);
    }
    sg_picture_buffer = rgb565_buf;

    lv_canvas_set_buffer(sg_chat.image_canvas, rgb565_buf,
                         info.width, info.height, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(sg_chat.image_canvas, info.width, info.height);
    lv_obj_center(sg_chat.image_canvas);

    __show_image_page();
    lv_vendor_disp_unlock();

    /* Auto-print */
#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    if (sg_chat.cur_img_name[0] != '\0') {
        PR_NOTICE("simple: auto-print '%s'", sg_chat.cur_img_name);
        ai_ui_notify_action(AI_UI_ACT_PRINT_IMG,
                            (uint8_t *)sg_chat.cur_img_name,
                            (uint32_t)strlen(sg_chat.cur_img_name));
    }
#endif
}

/* ── init ── */

void ai_ui_simple_chat_init(lv_obj_t *parent)
{
    const lv_font_t *text_font = ai_ui_get_text_font();

    /* ── Text page ── */
    sg_chat.text_page = lv_obj_create(parent);
    lv_obj_set_size(sg_chat.text_page, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(sg_chat.text_page, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_chat.text_page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_chat.text_page, 0, 0);
    lv_obj_set_style_radius(sg_chat.text_page, 0, 0);
    lv_obj_set_style_pad_all(sg_chat.text_page, 16, 0);
    lv_obj_set_flex_flow(sg_chat.text_page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sg_chat.text_page, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(sg_chat.text_page, 20, 0);
    lv_obj_set_scrollbar_mode(sg_chat.text_page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(sg_chat.text_page, LV_OBJ_FLAG_HIDDEN);

    /* User ASR label */
    sg_chat.user_label = lv_label_create(sg_chat.text_page);
    lv_obj_set_style_text_font(sg_chat.user_label, text_font, 0);
    lv_obj_set_style_text_color(sg_chat.user_label, lv_color_hex(0x07C160), 0);
    lv_obj_set_width(sg_chat.user_label, LV_HOR_RES - 32);
    lv_label_set_long_mode(sg_chat.user_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(sg_chat.user_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(sg_chat.user_label, "");

    /* AI reply label */
    sg_chat.ai_label = lv_label_create(sg_chat.text_page);
    lv_obj_set_style_text_font(sg_chat.ai_label, text_font, 0);
    lv_obj_set_style_text_color(sg_chat.ai_label, lv_color_white(), 0);
    lv_obj_set_width(sg_chat.ai_label, LV_HOR_RES - 32);
    lv_label_set_long_mode(sg_chat.ai_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(sg_chat.ai_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(sg_chat.ai_label, "");

    /* ── Image page ── */
    sg_chat.image_page = lv_obj_create(parent);
    lv_obj_set_size(sg_chat.image_page, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(sg_chat.image_page, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_chat.image_page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_chat.image_page, 0, 0);
    lv_obj_set_style_radius(sg_chat.image_page, 0, 0);
    lv_obj_set_style_pad_all(sg_chat.image_page, 0, 0);
    lv_obj_clear_flag(sg_chat.image_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_chat.image_page, LV_OBJ_FLAG_HIDDEN);

    /* Print status overlay label (hidden by default) */
    sg_chat.print_status_label = lv_label_create(sg_chat.image_page);
    lv_obj_set_style_text_font(sg_chat.print_status_label, text_font, 0);
    lv_obj_set_style_text_color(sg_chat.print_status_label, lv_color_white(), 0);
    lv_obj_set_style_bg_color(sg_chat.print_status_label, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_chat.print_status_label, LV_OPA_70, 0);
    lv_obj_set_style_pad_all(sg_chat.print_status_label, 8, 0);
    lv_obj_set_style_radius(sg_chat.print_status_label, 8, 0);
    lv_label_set_text(sg_chat.print_status_label, PRINTING);
    lv_obj_align(sg_chat.print_status_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_flag(sg_chat.print_status_label, LV_OBJ_FLAG_HIDDEN);
}

/* ── register ── */

void ai_ui_simple_chat_register(void)
{
    AI_UI_CHAT_INTFS_T intfs;
    memset(&intfs, 0, sizeof(AI_UI_CHAT_INTFS_T));

    intfs.disp_open               = __ui_open_chat;
    intfs.disp_close              = __ui_close_chat;
    intfs.disp_user_msg           = __ui_set_user_msg;
    intfs.disp_ai_msg             = __ui_set_ai_msg;
    intfs.disp_ai_msg_stream_start = __ui_stream_start;
    intfs.disp_ai_msg_stream_data = __ui_stream_data;
    intfs.disp_ai_msg_stream_end  = __ui_stream_end;
    intfs.disp_image              = __ui_disp_image;
#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    intfs.disp_print_result       = __disp_print_result;
#endif

    ai_ui_chat_register(&intfs);
}

#endif /* ENABLE_AI_CHAT_GUI_SIMPLE */
```

- [ ] **Step 2: Commit**

```bash
git add apps/tuya.ai/ai_components/ai_ui/src/simple/ai_ui_simple_chat.c
git commit -m "feat(ui): add simple UI chat — text page, image page, auto-print"
```

---

### Task 5: CMakeLists — wire simple/ sources into build

**Files:**
- Modify: `apps/tuya.ai/ai_components/ai_ui/CMakeLists.txt`

- [ ] **Step 1: Add the simple variant block**

After the existing OLED block:
```cmake
elseif (CONFIG_ENABLE_AI_CHAT_GUI_OLED STREQUAL "y")
    file(GLOB OLED_SRCS ${COMP_MODULE_PATH}/src/oled/*.c)
    list(APPEND COMP_MODULE_SRCS ${OLED_SRCS})
```

Add before the closing `endif()`:
```cmake
elseif (CONFIG_ENABLE_AI_CHAT_GUI_SIMPLE STREQUAL "y")
    file(GLOB SIMPLE_SRCS ${COMP_MODULE_PATH}/src/simple/*.c)
    list(APPEND COMP_MODULE_SRCS ${SIMPLE_SRCS})
```

- [ ] **Step 2: Commit**

```bash
git add apps/tuya.ai/ai_components/ai_ui/CMakeLists.txt
git commit -m "feat(ui): wire simple/ sources into CMakeLists"
```

---

### Task 6: ai_chat_ui.c — add simple register call

**Files:**
- Modify: `apps/tuya.ai/ai_components/ai_main/src/ai_chat_ui.c:13-19,146-153`

- [ ] **Step 1: Add include and register call**

Add the include after the OLED include (line 17-18):
```c
#elif defined(ENABLE_AI_CHAT_GUI_SIMPLE) && (ENABLE_AI_CHAT_GUI_SIMPLE == 1)
#include "ai_ui_chat_simple.h"
```

Add the register call in `ai_chat_ui_init()` after the OLED register (line 150-151):
```c
#elif defined(ENABLE_AI_CHAT_GUI_SIMPLE) && (ENABLE_AI_CHAT_GUI_SIMPLE == 1)
    TUYA_CALL_ERR_RETURN(ai_ui_chat_simple_register());
```

The resulting code should look like:
```c
#if defined(ENABLE_AI_CHAT_GUI_WECHAT) && (ENABLE_AI_CHAT_GUI_WECHAT == 1)
#include "ai_ui_chat_wechat.h"
#elif defined(ENABLE_AI_CHAT_GUI_CHATBOT) && (ENABLE_AI_CHAT_GUI_CHATBOT == 1)
#include "ai_ui_chat_chatbot.h"
#elif defined(ENABLE_AI_CHAT_GUI_OLED) && (ENABLE_AI_CHAT_GUI_OLED == 1)
#include "ai_ui_chat_oled.h"
#elif defined(ENABLE_AI_CHAT_GUI_SIMPLE) && (ENABLE_AI_CHAT_GUI_SIMPLE == 1)
#include "ai_ui_chat_simple.h"
#endif
```

And in `ai_chat_ui_init()`:
```c
#if defined(ENABLE_AI_CHAT_GUI_WECHAT) && (ENABLE_AI_CHAT_GUI_WECHAT == 1)
    TUYA_CALL_ERR_RETURN(ai_ui_chat_wechat_register());
#elif defined(ENABLE_AI_CHAT_GUI_CHATBOT) && (ENABLE_AI_CHAT_GUI_CHATBOT == 1)
    TUYA_CALL_ERR_RETURN(ai_ui_chat_chatbot_register());
#elif defined(ENABLE_AI_CHAT_GUI_OLED) && (ENABLE_AI_CHAT_GUI_OLED == 1)
    TUYA_CALL_ERR_RETURN(ai_ui_chat_oled_register());
#elif defined(ENABLE_AI_CHAT_GUI_SIMPLE) && (ENABLE_AI_CHAT_GUI_SIMPLE == 1)
    TUYA_CALL_ERR_RETURN(ai_ui_chat_simple_register());
#else
#error "please select ai chat present ui"
#endif
```

- [ ] **Step 2: Commit**

```bash
git add apps/tuya.ai/ai_components/ai_main/src/ai_chat_ui.c
git commit -m "feat(ui): wire simple UI register into ai_chat_ui.c"
```

---

### Task 7: Board config — create a simple UI config preset

**Files:**
- Create: `apps/tuya.ai/your_chat_bot/config/TUYA_T5AI_BOARD_LCD_3.5_SIMPLE.config`

- [ ] **Step 1: Create config based on current app_default.config**

```
CONFIG_PROJECT_VERSION="1.0.1"
CONFIG_TUYA_PRODUCT_ID="kh0hig0fdtlzndvg"
CONFIG_ENABLE_COMP_AI_AUDIO_CODEC_OPUS=y
CONFIG_ENABLE_COMP_AI_PICTURE=y
CONFIG_ENABLE_COMP_AI_PICTURE_HOSTING_DLD=y
CONFIG_BUTTON_NAME="ai_chat_button"
CONFIG_BOARD_CHOICE_T5AI=y
CONFIG_TUYA_T5AI_BOARD_LCD_35565=y
CONFIG_TUYA_T5AI_BOARD_PRINTER_DP48=y
CONFIG_AI_INPUT_STACK_SIZE=28672
CONFIG_AI_CLIENT_STACK_SIZE=8192
CONFIG_ENABLE_MBEDTLS_SSL_MAX_CONTENT_LEN=4096
CONFIG_ENABLE_AI_CHAT_GUI_SIMPLE=y
```

Key differences from `app_default.config`:
- `CONFIG_ENABLE_AI_CHAT_GUI_SIMPLE=y` instead of default wechat
- Removed `CONFIG_ENABLE_AI_UI_TEXT_STREAMING=y` (stream text handled directly)
- Removed `CONFIG_ENABLE_COMP_AI_VIDEO=y` (no camera)
- Removed `CONFIG_LVGL_ENABLE_TP=y` (no touch interaction needed)
- Removed `CONFIG_ENABLE_BUTTON_2=y` (only hold button)

- [ ] **Step 2: Commit**

```bash
git add apps/tuya.ai/your_chat_bot/config/TUYA_T5AI_BOARD_LCD_3.5_SIMPLE.config
git commit -m "feat(ui): add simple UI board config preset"
```

---

### Task 8: Build verification

- [ ] **Step 1: Switch to simple config and build**

```bash
cd apps/tuya.ai/your_chat_bot
cp config/TUYA_T5AI_BOARD_LCD_3.5_SIMPLE.config app_default.config
rm -rf .build
tos.py build
```

Expected: Build succeeds with no errors.

- [ ] **Step 2: Verify config took effect**

```bash
grep "ENABLE_AI_CHAT_GUI_SIMPLE" .build/cache/using.config
```

Expected: `CONFIG_ENABLE_AI_CHAT_GUI_SIMPLE=y`

- [ ] **Step 3: Commit build config if needed**

If any adjustments were made during build, commit them.
