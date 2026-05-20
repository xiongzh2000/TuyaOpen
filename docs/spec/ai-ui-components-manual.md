# TuyaOpen AI UI 组件技术手册（AI Agent 用）

> 整理自 https://www.tuyaopen.ai/zh/docs/applications/tuya.ai/ai-components/

---

## 架构概述

`ai_ui_manage` 是 AI 应用 UI 层的调度核心，采用消息队列 + 独立线程的异步架构。

```
业务代码（ai_chat_main / app_ui_action）
        ↓  ai_ui_disp_msg() / ai_ui_disp_msg_sync()
  ai_ui_manage（消息队列 + UI 线程）
        ↓  回调分发
  具体 UI 实现（wechat / chatbot / oled / 自定义）
        ↓
  LVGL（lv_vendor 层）
        ↓
  LCD / OLED 硬件
```

---

## 消息类型（AI_UI_DISP_TYPE_E）

| 枚举值 | 含义 | data 格式 |
|--------|------|-----------|
| `AI_UI_DISP_USER_MSG` | 用户说话文字 | `char *` UTF-8 |
| `AI_UI_DISP_AI_MSG` | AI 回复文字（完整） | `char *` UTF-8 |
| `AI_UI_DISP_AI_MSG_STREAM_START` | 流式回复开始 | NULL |
| `AI_UI_DISP_AI_MSG_STREAM_DATA` | 流式回复数据块 | `char *` UTF-8 |
| `AI_UI_DISP_AI_MSG_STREAM_END` | 流式回复结束 | NULL |
| `AI_UI_DISP_AI_MSG_STREAM_INTERRUPT` | 流式回复被打断 | NULL |
| `AI_UI_DISP_SYSTEM_MSG` | 系统消息（灰色气泡）| `char *` UTF-8 |
| `AI_UI_DISP_EMOTION` | 情绪表情 | `char *` emoji name |
| `AI_UI_DISP_STATUS` | 状态文字（状态栏）| `char *` 如 `THINKING` |
| `AI_UI_DISP_NOTIFICATION` | 临时通知（3 秒消失）| `char *` UTF-8 |
| `AI_UI_DISP_NETWORK` | 网络状态图标 | `AI_UI_WIFI_STATUS_E *` |
| `AI_UI_DISP_CHAT_MODE` | 对话模式文字 | `char *` |
| `AI_UI_DISP_CAMERA_OPEN` | 打开摄像头预览 | NULL |
| `AI_UI_DISP_CAMERA_FLUSH` | 刷新摄像头帧 | `AI_UI_VIDEO_T *` |
| `AI_UI_DISP_CAMERA_THUMB` | 显示拍照缩略图 | JPEG `uint8_t *` |
| `AI_UI_DISP_CAMERA_CLOSE` | 关闭摄像头预览 | NULL |
| `AI_UI_DISP_USER_IMAGE_LINK` | 用户侧图片链接 | `char *` 文件名 |
| `AI_UI_DISP_AI_IMAGE_LINK` | AI 侧图片链接 | `char *` 文件名 |
| `AI_UI_DISP_ADD_CHAT_ATTACH_IMG` | 添加聊天附图 | `AI_UI_IMG_T *` |
| `AI_UI_DISP_CLEAR_CHAT_ATTACH` | 清空聊天附图 | NULL |
| `AI_UI_DISP_ALBUM_OPEN` | 打开相册 | `char *` album name |
| `AI_UI_DISP_ALBUM_VIEW_NEXT` | 相册下一张 | NULL |
| `AI_UI_DISP_ALBUM_VIEW_PREV` | 相册上一张 | NULL |
| `AI_UI_DISP_ALBUM_VIEW_ALL` | 相册全部图片 | NULL |
| `AI_UI_DISP_ALBUM_SELECT_IMG` | 进入相册选图模式 | NULL |
| `AI_UI_DISP_ALBUM_RELOAD` | 重载相册列表 | NULL |
| `AI_UI_DISP_ALBUM_CLOSE` | 关闭相册 | NULL |
| `AI_UI_DISP_PRINT_RESULT` | 打印结果 | `int32_t *`（0=成功）|

---

## Wi-Fi 状态枚举（AI_UI_WIFI_STATUS_E）

```c
#define AI_UI_WIFI_STATUS_DISCONNECTED  0
#define AI_UI_WIFI_STATUS_GOOD          1
#define AI_UI_WIFI_STATUS_FAIR          2
#define AI_UI_WIFI_STATUS_WEAK          3
```

---

## Action 枚举（AI_UI_ACTION_E）

UI 层通过 `ai_ui_action_cb_register()` 向 App 层上报用户操作：

| 枚举值 | 触发场景 |
|--------|---------|
| `AI_UI_ACT_OPEN_CAMERA` | 用户点击打开摄像头 |
| `AI_UI_ACT_TAKE_PHOTO` | 用户点击拍照 |
| `AI_UI_ACT_CLOSE_CAMER` | 用户关闭摄像头 |
| `AI_UI_ACT_CAMERA_AI_ON` | 开启 AI 视觉模式 |
| `AI_UI_ACT_CAMERA_AI_OFF` | 关闭 AI 视觉模式 |
| `AI_UI_ACT_OPEN_ALBUM` | 打开相册（浏览模式）|
| `AI_UI_ACT_VIEW_PREV_IMG` | 查看上一张 |
| `AI_UI_ACT_VIEW_NEXT_IMG` | 查看下一张 |
| `AI_UI_ACT_VIEW_ALL_IMG` | 查看所有图片 |
| `AI_UI_ACT_DELETE_IMG` | 删除图片（data=文件名）|
| `AI_UI_ACT_BATCH_DELETE_IMG` | 批量删除（data=`AI_UI_BATCH_DELETE_T *`）|
| `AI_UI_ACT_CLOSE_ALBUM` | 关闭相册 |
| `AI_UI_ACT_OPEN_IMG_ATTACH_LIST` | 打开相册选图（附图模式）|
| `AI_UI_ACT_ADD_IMG_ATTACH` | 添加附图（data=文件名）|
| `AI_UI_ACT_DEL_IMG_ATTACH` | 删除附图（data=文件名）|
| `AI_UI_ACT_IMG2IMG_FROM_CAMERA` | 图生图：摄像头拍源图 |
| `AI_UI_ACT_IMG2IMG_FROM_ALBUM` | 图生图：相册选源图 |
| `AI_UI_ACT_PRINT_IMG` | 打印图片（data=文件名）|

---

## 核心 API

### 发送消息

```c
// 异步：消息投入队列后立即返回
OPERATE_RET ai_ui_disp_msg(AI_UI_DISP_TYPE_E tp, uint8_t *data, int len);

// 同步：阻塞直到 UI 线程处理完（注意：不能在 UI 线程内调用，会死锁）
OPERATE_RET ai_ui_disp_msg_sync(AI_UI_DISP_TYPE_E tp, uint8_t *data, int len);
```

### 注册 Action 回调

```c
// App 层注册，UI 层用户操作时回调
void ai_ui_action_cb_register(AI_UI_ACTION_CB action_cb);

// 回调函数签名
typedef void (*AI_UI_ACTION_CB)(AI_UI_ACTION_E action, uint8_t *data, uint32_t len);
```

### 注册 UI 实现

```c
// 注册主 UI 接口（状态栏、情绪、通知等）
OPERATE_RET ai_ui_register(AI_UI_INTFS_T *intfs);

// 注册聊天界面接口（消息气泡等）
OPERATE_RET ai_ui_chat_register(AI_UI_CHAT_INTFS_T *intfs);

// 注册相册界面接口
OPERATE_RET ai_ui_image_album_register(AI_UI_ALBUM_INTFS_T *intfs);

// 注册摄像头界面接口
OPERATE_RET ai_ui_camera_register(AI_UI_CAMERA_INTFS_T *intfs);

// 初始化 UI 模块（注册完成后调用）
OPERATE_RET ai_ui_init(void);

// 上报 UI Action（UI 内部调用）
void ai_ui_notify_action(AI_UI_ACTION_E action, uint8_t *data, uint32_t len);
```

---

## 三种内置 UI 风格

### 1. WeChat 风格（`ai_ui_chat_wechat`）

- 气泡式消息：用户消息绿色右侧，AI 消息白色左侧
- 最多 20 条消息，自动删除最旧的
- 支持流式文字、摄像头预览、图片查看
- 底部"+"弹窗：相机、相册、添加附图、图生图

**Kconfig**：`ENABLE_AI_CHAT_GUI_WECHAT=y`

```c
#include "ai_ui_chat_wechat.h"

ai_ui_chat_wechat_register();   // 注册 wechat UI
ai_ui_init();                   // 初始化 UI 管理器
```

### 2. Chatbot 风格（`ai_ui_chat_chatbot`）

- 居中大字展示最新消息（用户/AI/系统三种颜色）
- 顶部状态栏：模式、状态、通知、网络信号
- 中央区域显示情绪表情 + 聊天文字

**Kconfig**：`ENABLE_AI_CHAT_GUI_CHATBOT=y`

```c
#include "ai_ui_chat_chatbot.h"

ai_ui_chat_chatbot_register();
ai_ui_init();
```

### 3. OLED 风格（`ai_ui_chat_oled`）

- 滚动字幕式，适配小尺寸单色屏
- 支持 128×64 和 128×32 两种规格
- 自动按 Kconfig 中 `AI_CHAT_GUI_OLED_SIZE` 选项切换布局

**Kconfig**：`ENABLE_AI_CHAT_GUI_OLED=y`，`AI_CHAT_GUI_OLED_SIZE_128_64` 或 `AI_CHAT_GUI_OLED_SIZE_128_32`

```c
#include "ai_ui_chat_oled.h"

ai_ui_chat_oled_register();
ai_ui_init();
```

---

## 自定义 UI

开启 `ENABLE_AI_CHAT_CUSTOM_UI=y`（`ENABLE_COMP_AI_DISPLAY=y` 子选项），实现并注册自己的接口结构体：

```c
AI_UI_INTFS_T intfs = {
    .disp_init         = my_ui_init,
    .disp_emotion      = my_ui_set_emotion,
    .disp_ai_mode_state = my_ui_set_status,
    .disp_notification = my_ui_set_notification,
    .disp_wifi_state   = my_ui_set_network,
    .disp_ai_chat_mode = my_ui_set_chat_mode,
};
ai_ui_register(&intfs);

AI_UI_CHAT_INTFS_T chat_intfs = {
    .disp_user_msg            = my_ui_user_msg,
    .disp_ai_msg              = my_ui_ai_msg,
    .disp_ai_msg_stream_start = my_ui_stream_start,
    .disp_ai_msg_stream_data  = my_ui_stream_data,
    .disp_ai_msg_stream_end   = my_ui_stream_end,
    .disp_system_msg          = my_ui_system_msg,
};
ai_ui_chat_register(&chat_intfs);

ai_ui_init();
```

---

## 关键约束

| 约束 | 说明 |
|------|------|
| `disp_msg_sync` 不能在 UI 线程调用 | 会造成死锁 |
| 所有 LVGL 操作在 `lv_vendor_disp_lock/unlock` 内 | 否则多线程竞争崩溃 |
| `ENABLE_COMP_AI_DISPLAY=y` 才编译 UI 模块 | Kconfig 控制 |
| 流式文字开启需 `ENABLE_AI_UI_TEXT_STREAMING=y` | 否则 `STREAM_DATA` 消息无效 |
