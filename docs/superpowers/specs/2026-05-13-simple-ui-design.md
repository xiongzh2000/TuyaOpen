# Simple UI Style Design Spec

## Overview

新增第四种 UI 风格 `simple`，与 wechat/chatbot/oled 并列，通过 Kconfig 选择。面向低配打印机产品：只支持长按对话，屏幕显示 ASR + AI 回复文字，收到图片后全屏展示并自动打印，图片常驻不折叠。

屏幕硬件不变：320×480 3.5 寸 LCD。

## 状态机

3 个状态，单屏切换：

```
IDLE（待机）──按住按键──▶ TEXT（文字）──收到图片──▶ IMAGE（图片）
  ▲                        │                        │
  └──对话结束/超时──────────┘                        │
  ▲                                                 │
  └──────────下次按按键时切回 TEXT───────────────────┘
```

- **IDLE**: 黑屏或简单状态文字（如"长按说话"）。
- **TEXT**: 居中显示 ASR 识别文字，随后流式显示 AI 回复。对话结束且无图片时，几秒后回 IDLE。
- **IMAGE**: 全屏展示图片，常驻不折叠，同时自动触发打印。下次长按按键开始新对话时切回 TEXT。

## 不做的功能

- 聊天气泡/消息列表
- 图片相册（album）、图片折叠/附件
- 弹窗菜单（+号菜单、打印确认弹窗）
- 表情显示、摄像头/视频、MCP
- 多模式切换（只保留 hold 模式）

## 接口实现

通过 `ai_ui_register()` 和 `ai_ui_chat_register()` 注册回调，复用现有 `ai_ui_manage.c` 消息分发机制。

### AI_UI_INTFS_T 回调

| 回调 | 实现 |
|------|------|
| `disp_init` | 创建 LVGL 屏幕，初始化 3 个页面容器 |
| `disp_emotion` | 忽略 |
| `disp_ai_mode_state` | TEXT 页面底部显示状态文字（如"思考中..."） |
| `disp_notification` | TEXT 页面显示通知 |
| `disp_wifi_state` | IDLE 页面显示 Wi-Fi 状态图标 |
| `disp_ai_chat_mode` | 忽略（只有 hold 模式） |

### AI_UI_CHAT_INTFS_T 回调

| 回调 | 实现 |
|------|------|
| `disp_open` | 切到 TEXT 页面，清空旧内容 |
| `disp_close` | 几秒后切回 IDLE |
| `disp_user_msg` | TEXT 页面显示 ASR 文字 |
| `disp_ai_msg` | TEXT 页面显示 AI 完整回复 |
| `disp_ai_msg_stream_start` | TEXT 页面清空 AI 区域 |
| `disp_ai_msg_stream_data` | TEXT 页面追加流式文字 |
| `disp_ai_msg_stream_end` | 无特殊处理 |
| `disp_image` | 切到 IMAGE 页面全屏展示，自动调 `app_print_jpeg_img` 打印 |
| `disp_print_result` | IMAGE 页面短暂叠加"打印成功/失败"文字 |
| 其他（link, attach, clear_attach） | 不实现 |

## 文件结构

```
ai_ui/src/simple/
  ai_ui_simple_main.c    — disp_init, AI_UI_INTFS_T 注册, Wi-Fi 状态
  ai_ui_simple_chat.c    — AI_UI_CHAT_INTFS_T 注册, TEXT/IMAGE 页面, 自动打印
```

## Kconfig

`ai_ui/Kconfig` 新增：
```kconfig
config ENABLE_AI_CHAT_GUI_SIMPLE
    bool "Simple style (text + image, no chat bubbles)"
```

字体复用现有 `FONT_TEXT_SIZE_18_2`（和 wechat 一致）。

## 自动打印逻辑

`disp_image` 回调中：
1. 将 RGB565 图片全屏渲染到 IMAGE 页面
2. 通过 `ai_ui_notify_action(AI_UI_ACT_PRINT_IMG, ...)` 触发打印（复用现有 `app_ui_action.c` 中的打印流程）
3. 不弹确认框，直接打印

## 依赖

- 复用 `ai_ui_manage.c` 消息队列和分发
- 复用 `ai_ui_icon_font.c` 字体管理
- 复用 `app_printer.c` 打印（已有居中修复）
- 复用 `ai_ui_stream_text.c` 流式文字（如适用）
- 不依赖 `ai_ui_image_album.c`、`ai_ui_camera.c`
