# 图生图功能设计文档

**日期**：2026-05-07  
**范围**：`your_chat_bot` 打印机固件  
**目标**：在现有文生图（打印）基础上，新增图生图能力——用户可通过摄像头拍照或从相册选图，加上语音指令，让 AI 修改图片并打印。

---

## 1. 背景

当前打印流程：语音 → AI 文生图 → `AI_USER_EVT_GENERATE_PICTURE` → `app_print_img_from_album()`。

底层已有完整的图+语音联合输入管道：`ai_mode_hold_vad_change()` 在语音开始时，若 `ai_picture_input` 队列不为空，会先发图再发语音流，AI 可在同一 session 内处理两者。无需改动 `ai_mode` 和 `ai_picture_output`，只需在 UI 和 App 层打通入口。

---

## 2. 数据流

```
用户点 "+" → "图生图" → "拍照" 或 "从相册"
        ↓                         ↓
   打开摄像头                  打开相册选图
   用户拍照                    用户选中图片
   保存到相册                  ────────────
        ↓
   ai_picture_input_add_from_album(name, NULL)
        ↓
   屏幕通知 "图片已就绪，请说出编辑指令"
        ↓
   用户用现有方式说话（长按按键 / 唤醒词）
        ↓
   ai_mode_hold_vad_change():
     tuya_ai_image_input(图)  ← 同一 session
     tuya_ai_audio_input(语音)
        ↓
   AI 生成改图
        ↓
   ai_picture_output_save_to_album() → AI_USER_EVT_GENERATE_PICTURE
        ↓
   app_print_img_from_album()  ← 现有打印链路，零改动
```

---

## 3. 涉及文件

| 文件 | 改动内容 |
|------|---------|
| `ai_components/ai_ui/include/ai_ui_manage.h` | `AI_UI_ACTION_E` 增加 2 个枚举 |
| `ai_components/assets/include/lang_config.h` | 增加 4 个字符串常量 |
| `ai_components/ai_ui/src/wechat/ai_ui_wechat_chat.c` | "+"弹窗加第 4 项 + 二级来源弹窗 |
| `apps/tuya.ai/your_chat_bot/src/app_ui_action.c` | 处理 2 个新 Action，修改 2 个已有 case |

---

## 4. 详细变更

### 4.1 新增 Action 枚举（`ai_ui_manage.h`）

在 `AI_UI_ACT_PRINT_IMG` 之前插入：

```c
AI_UI_ACT_IMG2IMG_FROM_CAMERA,  // 图生图：用摄像头拍源图
AI_UI_ACT_IMG2IMG_FROM_ALBUM,   // 图生图：从相册选源图
```

### 4.2 字符串常量（`lang_config.h`）

```c
#define IMG2IMG        "图生图"
#define IMG2IMG_CAMERA "拍照"
#define IMG2IMG_ALBUM  "从相册"
#define IMG2IMG_READY  "图片已就绪，请说出编辑指令"
```

### 4.3 UI 层（`ai_ui_wechat_chat.c`）

**结构体** `AI_UI_WECHAT_CHAT_T` 增加字段：
```c
lv_obj_t *img2img_popup;
```

**"+"主弹窗**：
- 在第 3 项（add_img_btn）之后追加第 4 项 "图生图"（`icon_photo_app` 图标）
- 主弹窗高度：`POPUP_ITEM_H * 3 + 4` → `POPUP_ITEM_H * 4 + 4`
- 点击回调 `__popup_img2img_cb`：关闭主弹窗，显示 `img2img_popup`

**`img2img_popup`** 二级弹窗（在 `ai_ui_wechat_chat_init` 中创建，默认 `HIDDEN`）：
- 宽 120px，高 `POPUP_ITEM_H * 2 + 4`，位置对齐 `plus_btn` 上方
- 子项 1："拍照" → 回调 `__img2img_camera_cb` → `ai_ui_notify_action(AI_UI_ACT_IMG2IMG_FROM_CAMERA, NULL, 0)`
- 子项 2："从相册" → 回调 `__img2img_album_cb` → `ai_ui_notify_action(AI_UI_ACT_IMG2IMG_FROM_ALBUM, NULL, 0)`
- 点击聊天内容区关闭（与主弹窗共用 `__popup_dismiss` 逻辑，一并隐藏两个弹窗）

### 4.4 App 层（`app_ui_action.c`）

**新增静态变量**：
```c
static bool sg_img2img_pending = false;
```

**新 case：`AI_UI_ACT_IMG2IMG_FROM_CAMERA`**（在 `ENABLE_COMP_AI_VIDEO` + `ENABLE_COMP_AI_PICTURE` 宏保护下）：
```
1. sg_img2img_pending = true
2. ai_video_set_yuv_frame_flush_cb(__display_camera_yuv_fram)
3. ai_video_start()
4. ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_OPEN, NULL, 0)
```

**修改 `AI_UI_ACT_TAKE_PHOTO`**：在 `if (jpeg && jpeg_len)` 块内，`ai_picture_save_to_album` 之后、`sg_ai_vision_enabled` 分支之前，插入优先级更高的 `sg_img2img_pending` 分支：
```
if (sg_img2img_pending) {
    sg_img2img_pending = false;
    ai_video_stop();
    ai_video_set_yuv_frame_flush_cb(NULL);
    ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_CLOSE, NULL, 0);
    ai_picture_input_add_from_album(name, NULL);
    ai_ui_disp_msg(AI_UI_DISP_NOTIFICATION, (uint8_t *)IMG2IMG_READY, strlen(IMG2IMG_READY));
}
```

**新 case：`AI_UI_ACT_IMG2IMG_FROM_ALBUM`**（在 `ENABLE_COMP_AI_PICTURE` 宏保护下）：
```
1. sg_img2img_pending = true
2. ai_ui_notify_action(AI_UI_ACT_OPEN_IMG_ATTACH_LIST, NULL, 0)
   — 直接复用现有相册多选流程，选中后会触发 AI_UI_ACT_ADD_IMG_ATTACH
```

**修改 `AI_UI_ACT_ADD_IMG_ATTACH`**：现有入队逻辑之后追加：
```
if (sg_img2img_pending) {
    sg_img2img_pending = false;
    ai_ui_disp_msg(AI_UI_DISP_NOTIFICATION, (uint8_t *)IMG2IMG_READY, strlen(IMG2IMG_READY));
}
```

---

## 5. 边界情况

| 情况 | 处理 |
|------|------|
| 用户打开摄像头后关闭（未拍照） | `AI_UI_ACT_CLOSE_CAMER` case 末尾清 `sg_img2img_pending = false` |
| 相册页面未选图直接返回 | `AI_UI_ACT_CLOSE_ALBUM` case 末尾清 `sg_img2img_pending = false` |
| 图片队列已满（上限 3 张） | `ai_picture_input_add_from_album` 返回 `OPRT_EXCEED_UPPER_LIMIT`，不崩溃，通知不显示 |
| AI 未输出图片（只文字回复） | `GENERATE_PICTURE` 不触发，打印机不动作，符合预期 |
| 仅在 wechat UI 有此按钮 | `chatbot` / `oled` UI 不需要修改，两个新 Action 枚举不破坏它们 |

---

## 6. 编译宏依赖

图生图功能需同时满足：
- `ENABLE_COMP_AI_VIDEO=1`（摄像头路径）
- `ENABLE_COMP_AI_PICTURE=1`（图片队列）
- `ENABLE_AI_CHAT_GUI_WECHAT=1`（UI 入口）
- `ENABLE_PRINTER=1`（结果打印）

默认配置 `app_default.config` 已开启上述全部选项。
