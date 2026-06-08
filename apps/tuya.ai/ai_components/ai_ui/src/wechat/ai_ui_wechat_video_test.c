/**
 * @file ai_ui_wechat_video_test.c
 * @brief 视频播放测试视图（全屏 overlay + canvas + 状态条 + 三格式切换 + 关闭）。
 *        解码器由 board 层通过 ai_ui_video_player_register 注册；本视图负责 UI 与贴帧。
 */
#include <string.h>
#include <stdio.h>
#include "lvgl.h"
#include "lv_vendor.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "lang_config.h"
#include "ai_ui_wechat_common.h"
#include "ai_ui_video_player.h"

#if defined(ENABLE_VIDEO_PLAY_TEST) && (ENABLE_VIDEO_PLAY_TEST == 1)

#define VT_DST_W 450
#define VT_DST_H 800

static const char *const s_paths[VIDEO_TEST_FMT_MAX] = {
    [VIDEO_TEST_FMT_MP4_H264] = "/sdcard/doubao_v1.mp4",
    [VIDEO_TEST_FMT_H264_RAW] = "/sdcard/doubao_v1.264",
    [VIDEO_TEST_FMT_MJPEG]    = "/sdcard/doubao_v1.mjpeg",
};
static const char *const s_fmt_btn[VIDEO_TEST_FMT_MAX] = {"MP4", "H.264", "MJPEG"};

static struct {
    lv_obj_t *overlay;
    lv_obj_t *canvas;
    lv_obj_t *status;
    uint8_t  *disp_buf;   /* 持久 RGB565 显示缓冲（VT_DST_W*VT_DST_H*2），on_frame 拷入 */
    char      status_txt[96];
    int       cur_fmt;
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

/* ---- player 回调（解码线程上下文，须加 LVGL 锁）---- */
static void __on_frame(const uint8_t *rgb565, int w, int h, void *user)
{
    (void)user;
    if (NULL == rgb565 || NULL == sg_vt.disp_buf || w != VT_DST_W || h != VT_DST_H) {
        return;
    }
    memcpy(sg_vt.disp_buf, rgb565, (uint32_t)w * h * 2);
    lv_vendor_disp_lock();
    if (sg_vt.canvas) {
        lv_canvas_set_buffer(sg_vt.canvas, sg_vt.disp_buf, w, h, LV_COLOR_FORMAT_RGB565);
        lv_obj_center(sg_vt.canvas);
    }
    lv_vendor_disp_unlock();
}

static void __on_status(const char *fmt_name, int fps, int frame_idx, void *user)
{
    (void)user;
    snprintf(sg_vt.status_txt, sizeof(sg_vt.status_txt), "%s  %dx%d  %dfps  #%d",
             fmt_name ? fmt_name : "?", 1280, 720, fps, frame_idx);
    lv_vendor_disp_lock();
    if (sg_vt.status) {
        lv_label_set_text(sg_vt.status, sg_vt.status_txt);
    }
    lv_vendor_disp_unlock();
}

static void __on_done(int ok, const char *err, void *user)
{
    (void)user;
    snprintf(sg_vt.status_txt, sizeof(sg_vt.status_txt), "%s: %s",
             s_fmt_btn[sg_vt.cur_fmt], ok ? "播放结束" : (err ? err : "出错"));
    lv_vendor_disp_lock();
    if (sg_vt.status) {
        lv_label_set_text(sg_vt.status, sg_vt.status_txt);
    }
    lv_vendor_disp_unlock();
}

static const VIDEO_TEST_PLAYER_CBS_T s_cbs = {
    .on_frame  = __on_frame,
    .on_status = __on_status,
    .on_done   = __on_done,
};

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

    /* 底部三个格式切换按钮 */
    static const lv_align_t pos[VIDEO_TEST_FMT_MAX] = {LV_ALIGN_BOTTOM_LEFT, LV_ALIGN_BOTTOM_MID, LV_ALIGN_BOTTOM_RIGHT};
    static const int xoff[VIDEO_TEST_FMT_MAX] = {8, 0, -8};
    for (int i = 0; i < VIDEO_TEST_FMT_MAX; i++) {
        lv_obj_t *b = lv_obj_create(sg_vt.overlay);
        lv_obj_set_size(b, 96, 40);
        lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(b, pos[i], xoff[i], -8);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, s_fmt_btn[i]);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, __fmt_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    /* 关闭按钮（右上角） */
    lv_obj_t *cb = lv_obj_create(sg_vt.overlay);
    lv_obj_set_size(cb, 48, 48);
    lv_obj_set_style_pad_all(cb, 0, 0);
    lv_obj_clear_flag(cb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cb, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(cb, LV_ALIGN_TOP_RIGHT, -8, 4);
    lv_obj_t *cl = lv_label_create(cb);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE);
    lv_obj_center(cl);
    lv_obj_add_event_cb(cb, __close_btn_cb, LV_EVENT_CLICKED, NULL);
}

/* "+"菜单点「视频测试」时调用：显示并默认播放 MP4 */
void ai_ui_wechat_video_test_open(void)
{
    if (NULL == sg_vt.overlay) {
        return;
    }
    if (NULL == sg_vt.disp_buf) {
        sg_vt.disp_buf = (uint8_t *)tal_malloc((uint32_t)VT_DST_W * VT_DST_H * 2);
    }
    lv_obj_clear_flag(sg_vt.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(sg_vt.overlay);
    __play_fmt(VIDEO_TEST_FMT_MP4_H264);
}

#endif /* ENABLE_VIDEO_PLAY_TEST */
