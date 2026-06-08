/**
 * @file ai_ui_video_player.h
 * @brief 视频播放测试 — 解码器注册接口（跨平台，无平台依赖）。
 *        board 层实现并注册具体解码器；ai_ui 视图通过本接口取 RGB565 帧。
 */
#ifndef __AI_UI_VIDEO_PLAYER_H__
#define __AI_UI_VIDEO_PLAYER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VIDEO_TEST_FMT_MP4_H264 = 0, /* doubao_v1.mp4   minimp4 解封装 + esp_h264 软解 */
    VIDEO_TEST_FMT_H264_RAW,     /* doubao_v1.264   esp_h264 软解（无解封装） */
    VIDEO_TEST_FMT_MJPEG,        /* doubao_v1.mjpeg 硬件 JPEG 解码 */
    VIDEO_TEST_FMT_MAX,
} VIDEO_TEST_FMT_E;

typedef struct {
    /* 每解出并缩放好一帧 RGB565（VT_DST_W×VT_DST_H）时回调；运行在解码线程上下文。 */
    void (*on_frame)(const uint8_t *rgb565, int w, int h, void *user);
    /* 进度/状态：格式名、实时解码 fps、已解帧序号。解码线程上下文。 */
    void (*on_status)(const char *fmt_name, int decode_fps, int frame_idx, void *user);
    /* 结束：ok=1 正常播完，ok=0 出错（err 为原因）。解码线程上下文。 */
    void (*on_done)(int ok, const char *err, void *user);
} VIDEO_TEST_PLAYER_CBS_T;

typedef struct {
    /* 异步开始播放 path 指定文件（按 fmt 选解码路径）；自起线程，立即返回。 */
    OPERATE_RET (*play)(const char *path, VIDEO_TEST_FMT_E fmt,
                        const VIDEO_TEST_PLAYER_CBS_T *cbs, void *user);
    /* 请求停止并回收解码线程/缓冲（阻塞直到线程退出）。 */
    void (*stop)(void);
} VIDEO_TEST_PLAYER_OPS_T;

/* board 启动时注册具体实现。 */
OPERATE_RET ai_ui_video_player_register(const VIDEO_TEST_PLAYER_OPS_T *ops);
/* 取已注册实现；未注册返回 NULL。 */
const VIDEO_TEST_PLAYER_OPS_T *ai_ui_video_player_get(void);

#ifdef __cplusplus
}
#endif
#endif /* __AI_UI_VIDEO_PLAYER_H__ */
