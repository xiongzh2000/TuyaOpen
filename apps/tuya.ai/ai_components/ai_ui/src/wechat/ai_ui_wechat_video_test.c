/**
 * @file ai_ui_wechat_video_test.c
 * @brief 视频播放测试视图（全屏 overlay + canvas + 状态条 + 三格式切换 + 关闭）。
 *        解码器由 board 层通过 ai_ui_video_player_register 注册；本视图负责 UI 与贴帧。
 *
 * 线程模型：解码线程回调只写共享缓冲(decode_buf)+置 dirty（tal_mutex 保护），绝不触碰
 * LVGL；一个 lv_timer 在 LVGL 任务上下文里把帧/状态应用到 canvas/label。避免解码线程
 * 持 LVGL 锁与按钮回调里阻塞式 stop() 形成死锁，并用双缓冲消除画面撕裂。
 */
#include <string.h>
#include <stdio.h>
#include "lvgl.h"
#include "lv_vendor.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_mutex.h"
#include "lang_config.h"
#include "ai_ui_wechat_common.h"
#include "ai_ui_video_player.h"

#if defined(ENABLE_VIDEO_PLAY_TEST) && (ENABLE_VIDEO_PLAY_TEST == 1)

#define VT_DST_W 450
#define VT_DST_H 800
#define VT_BUF_BYTES ((uint32_t)VT_DST_W * VT_DST_H * 2)

static const char *const s_paths[VIDEO_TEST_FMT_MAX] = {
    [VIDEO_TEST_FMT_MP4_H264] = "/sdcard/doubao_v1.mp4",
    [VIDEO_TEST_FMT_H264_RAW] = "/sdcard/doubao_v1.264",
    [VIDEO_TEST_FMT_MJPEG]    = "/sdcard/doubao_v1.mjpeg",
};
static const char *const s_fmt_btn[VIDEO_TEST_FMT_MAX] = {"MP4", "H.264", "MJPEG"};

static struct {
    lv_obj_t    *overlay;
    lv_obj_t    *canvas;
    lv_obj_t    *status;
    uint8_t     *decode_buf;   /* 解码线程写入（mutex 保护） */
    uint8_t     *disp_buf;     /* canvas 缓冲，仅 LVGL 任务读写 */
    MUTEX_HANDLE mutex;
    lv_timer_t  *timer;
    volatile int frame_dirty;
    volatile int status_dirty;
    char         pending_status[96];
    int          cur_fmt;
} sg_vt;

/* ---- 注册存储 ---- */
static const VIDEO_TEST_PLAYER_OPS_T *sg_ops = NULL;
OPERATE_RET ai_ui_video_player_register(const VIDEO_TEST_PLAYER_OPS_T *ops)
{
    sg_ops = ops;
    return OPRT_OK;
}
const VIDEO_TEST_PLAYER_OPS_T *ai_ui_video_player_get(void)
{
    return sg_ops;
}

/* ---- player 回调（解码线程上下文）：只写共享缓冲 + 置 dirty，绝不碰 LVGL ---- */
static void __on_frame(const uint8_t *rgb565, int w, int h, void *user)
{
    (void)user;
    if (NULL == rgb565 || NULL == sg_vt.decode_buf || NULL == sg_vt.mutex ||
        w != VT_DST_W || h != VT_DST_H) {
        return;
    }
    tal_mutex_lock(sg_vt.mutex);
    memcpy(sg_vt.decode_buf, rgb565, VT_BUF_BYTES);
    sg_vt.frame_dirty = 1;
    tal_mutex_unlock(sg_vt.mutex);
}

static void __on_status(const char *fmt_name, int fps, int frame_idx, void *user)
{
    (void)user;
    if (NULL == sg_vt.mutex) {
        return;
    }
    tal_mutex_lock(sg_vt.mutex);
    snprintf(sg_vt.pending_status, sizeof(sg_vt.pending_status), "%s  %dx%d  %dfps  #%d",
             fmt_name ? fmt_name : "?", 1280, 720, fps, frame_idx);
    sg_vt.status_dirty = 1;
    tal_mutex_unlock(sg_vt.mutex);
}

static void __on_done(int ok, const char *err, void *user)
{
    (void)user;
    if (NULL == sg_vt.mutex) {
        return;
    }
    tal_mutex_lock(sg_vt.mutex);
    snprintf(sg_vt.pending_status, sizeof(sg_vt.pending_status), "%s: %s",
             s_fmt_btn[sg_vt.cur_fmt], ok ? "播放结束" : (err ? err : "出错"));
    sg_vt.status_dirty = 1;
    tal_mutex_unlock(sg_vt.mutex);
}

static const VIDEO_TEST_PLAYER_CBS_T s_cbs = {
    .on_frame  = __on_frame,
    .on_status = __on_status,
    .on_done   = __on_done,
};

/* ---- LVGL 定时器（LVGL 任务上下文，已在 disp 锁内）：应用攒下的帧/状态 ---- */
static void __vt_timer_cb(lv_timer_t *t)
{
    (void)t;
    int do_frame = 0, do_status = 0;
    char txt[96];

    if (NULL == sg_vt.mutex) {
        return;
    }
    tal_mutex_lock(sg_vt.mutex);
    if (sg_vt.frame_dirty && sg_vt.disp_buf && sg_vt.decode_buf) {
        memcpy(sg_vt.disp_buf, sg_vt.decode_buf, VT_BUF_BYTES);
        sg_vt.frame_dirty = 0;
        do_frame = 1;
    }
    if (sg_vt.status_dirty) {
        sg_vt.status_dirty = 0;
        memcpy(txt, sg_vt.pending_status, sizeof(txt));
        do_status = 1;
    }
    tal_mutex_unlock(sg_vt.mutex);

    if (do_frame && sg_vt.canvas) {
        lv_canvas_set_buffer(sg_vt.canvas, sg_vt.disp_buf, VT_DST_W, VT_DST_H, LV_COLOR_FORMAT_RGB565);
        lv_obj_center(sg_vt.canvas);
    }
    if (do_status && sg_vt.status) {
        lv_label_set_text(sg_vt.status, txt);
    }
}

/* 以下函数均在 LVGL 任务上下文（事件回调）调用，已持 disp 锁，可直接调 LVGL */
static void __play_fmt(int fmt)
{
    const VIDEO_TEST_PLAYER_OPS_T *ops = ai_ui_video_player_get();
    if (NULL == ops || NULL == ops->play) {
        lv_label_set_text(sg_vt.status, "本平台未注册视频解码器");
        return;
    }
    if (ops->stop) {
        ops->stop();
    }
    sg_vt.cur_fmt = fmt;
    lv_label_set_text(sg_vt.status, "准备解码...");
    ops->play(s_paths[fmt], (VIDEO_TEST_FMT_E)fmt, &s_cbs, NULL);
}

static void __fmt_btn_cb(lv_event_t *e)
{
    int fmt = (int)(intptr_t)lv_event_get_user_data(e);
    __play_fmt(fmt);
}

static void __close_btn_cb(lv_event_t *e)
{
    (void)e;
    const VIDEO_TEST_PLAYER_OPS_T *ops = ai_ui_video_player_get();
    if (ops && ops->stop) {
        ops->stop();
    }
    if (sg_vt.overlay) {
        lv_obj_add_flag(sg_vt.overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

void ai_ui_wechat_video_test_init(lv_obj_t *parent)
{
    if (NULL == sg_vt.mutex) {
        tal_mutex_create_init(&sg_vt.mutex);
    }

    sg_vt.overlay = lv_obj_create(parent);
    lv_obj_set_size(sg_vt.overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(sg_vt.overlay, lv_color_black(), 0);
    lv_obj_set_style_pad_all(sg_vt.overlay, 0, 0);
    lv_obj_clear_flag(sg_vt.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_vt.overlay, LV_OBJ_FLAG_HIDDEN);

    sg_vt.canvas = lv_canvas_create(sg_vt.overlay);
    lv_obj_center(sg_vt.canvas);

    sg_vt.status = lv_label_create(sg_vt.overlay);
    lv_obj_set_style_text_color(sg_vt.status, lv_color_white(), 0);
    lv_obj_align(sg_vt.status, LV_ALIGN_TOP_MID, 0, 8);
    lv_label_set_text(sg_vt.status, "视频测试");

    static const lv_align_t pos[VIDEO_TEST_FMT_MAX] = {LV_ALIGN_BOTTOM_LEFT, LV_ALIGN_BOTTOM_MID, LV_ALIGN_BOTTOM_RIGHT};
    static const int xoff[VIDEO_TEST_FMT_MAX] = {8, 0, -8};
    for (int i = 0; i < VIDEO_TEST_FMT_MAX; i++) {
        lv_obj_t *b = lv_obj_create(sg_vt.overlay);
        lv_obj_set_size(b, 90, 40);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(b, pos[i], xoff[i], -8);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, s_fmt_btn[i]);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, __fmt_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    lv_obj_t *cb = lv_obj_create(sg_vt.overlay);
    lv_obj_set_size(cb, 40, 40);
    lv_obj_add_flag(cb, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(cb, LV_ALIGN_TOP_RIGHT, -8, 4);
    lv_obj_t *cl = lv_label_create(cb);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE);
    lv_obj_center(cl);
    lv_obj_add_event_cb(cb, __close_btn_cb, LV_EVENT_CLICKED, NULL);

    if (NULL == sg_vt.timer) {
        sg_vt.timer = lv_timer_create(__vt_timer_cb, 30, NULL);
    }
}

void ai_ui_wechat_video_test_open(void)
{
    if (NULL == sg_vt.overlay) {
        return;
    }
    if (NULL == sg_vt.decode_buf) {
        sg_vt.decode_buf = (uint8_t *)tal_malloc(VT_BUF_BYTES);
    }
    if (NULL == sg_vt.disp_buf) {
        sg_vt.disp_buf = (uint8_t *)tal_malloc(VT_BUF_BYTES);
    }
    if (NULL == sg_vt.decode_buf || NULL == sg_vt.disp_buf) {
        PR_ERR("video_test: disp buffer malloc failed");
        lv_obj_clear_flag(sg_vt.overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(sg_vt.overlay);
        lv_label_set_text(sg_vt.status, "内存不足");
        return;
    }
    lv_obj_clear_flag(sg_vt.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(sg_vt.overlay);
    __play_fmt(VIDEO_TEST_FMT_MP4_H264);
}

#endif /* ENABLE_VIDEO_PLAY_TEST */
