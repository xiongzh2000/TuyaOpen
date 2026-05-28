# 拍学机（pai_xue_ji）开发文档

## 概述

拍学机是基于 TuyaOpen SDK 的 AI 拍照学习设备，运行在涂鸦 T5AI 开发板 + 3.5 寸触摸屏（320x480）上。用户通过摄像头拍摄物体、文字，AI 云端识别后以卡片形式展示结果。

## 硬件平台

| 项目 | 规格 |
|------|------|
| 芯片 | T5AI (BK7258) |
| 屏幕 | 3.5" 320x480 RGB565 (ILI9488) |
| 触摸 | GT1151 电容触摸 |
| 摄像头 | GC2145 DVP |
| 存储 | SD 卡（SDIO + FATFS） |
| 产品ID | `p320pepzvmm1ghse` |
| 开发板 | TUYA_T5AI_BOARD (V102) |

## UI 布局 (320x480 竖屏)

```
┌────────────────────────────────┐
│ Status Bar (36px)       [WiFi] │
├────────────────────────────────┤
│                                │
│       Camera Preview           │
│       (320 x 240)             │
│                                │
├──────────┬──────────┬──────────┤
│          │          │          │
│  识万物   │  识中文   │  识英文   │
│          │          │          │
│  (image) │(chinese) │(english) │
│          │          │          │
└──────────┴──────────┴──────────┘
```

- **上方 (320x240)**：摄像头实时预览，启动后自动打开
- **下方 (320x204)**：三个识别模式按钮，横向等分排列

## 三种识别模式

### 1. 识万物（image_recognition）

识别图片中的物体，返回百科介绍。

**发送**：
- `clm_intent`: `ai_image`
- 图片：摄像头 JPEG 帧
- 文本：`image_recognition`

**云端返回 card**：
```json
{
  "type": "card",
  "name": "向日葵",
  "pinyin": "xiàng rì kuí",
  "relatedWord": "太阳花、葵花籽"
}
```

**卡片展示**：
| 字段 | 说明 |
|------|------|
| `name` | 物体名称，作为卡片标题 |
| `pinyin` | 名称拼音 |
| `relatedWord` | 相关词 |

---

### 2. 识中文（chinese_recognition）

识别图片中的中文文字，返回拼音标注。

**发送**：
- `clm_intent`: `ai_image`
- 图片：摄像头 JPEG 帧
- 文本：`chinese_recognition`

**云端返回 card**：
```json
{
  "word": "学习",
  "pinyin": "xué xí",
  "type": "card"
}
```

**卡片展示**：
| 字段 | 说明 |
|------|------|
| `word` | 识别到的中文，作为卡片标题 |
| `pinyin` | 拼音标注 |

---

### 3. 识英文（english_recognition）

识别图片中的英文文字，返回中文翻译。

**发送**：
- `clm_intent`: `ai_image`
- 图片：摄像头 JPEG 帧
- 文本：`english_recognition`

**云端返回 card**：
```json
{
  "word": "Hello",
  "cnWord": "你好",
  "type": "card"
}
```

**卡片展示**：
| 字段 | 说明 |
|------|------|
| `word` | 识别到的英文，作为卡片标题 |
| `cnWord` | 中文翻译 |

---

## 数据流

```
用户点击识别按钮
    │
    ▼
摄像头抓取 JPEG 帧
    │
    ▼
保存到相册 (SD 卡)
    │
    ▼
设置 event_param: {"clm_intent":"ai_image"}
    │
    ▼
tuya_ai_input_start(TRUE)
    │
    ▼
tuya_ai_image_input(jpeg)      ← 发送图片
    │
    ▼
tuya_ai_text_input(mode_text)  ← "image_recognition" / "chinese_recognition" / "english_recognition"
    │
    ▼
tuya_ai_input_stop()
    │
    ▼
云端处理，流式返回文本
    │
    ▼
设备端拼接流式文本
    │
    ▼
解析 JSON card → 卡片 UI 展示
```

## 文件清单

| 文件 | 作用 |
|------|------|
| `app_default.config` | 构建配置：自定义 UI、摄像头、图片、SD 卡 |
| `include/app_paixue_ui.h` | 自定义 UI 头文件，`RECOG_MODE_E` 枚举 |
| `include/app_chat_bot.h` | 聊天初始化头文件 |
| `src/app_paixue_ui.c` | **核心 UI**：三块布局、卡片展示、摄像头回调、AI 响应解析 |
| `src/app_ui_action.c` | 动作处理：拍照 + 识别模式分发、`clm_intent` 设置 |
| `src/app_chat_bot.c` | 初始化：注册自定义 UI、启动摄像头/图片/定时器 |
| `src/tuya_main.c` | 主入口：IoT 客户端、网络、事件处理 |

## 配置项 (app_default.config)

```
CONFIG_BOARD_CHOICE_T5AI=y                  # T5AI 平台
CONFIG_BOARD_CHOICE_TUYA_T5AI_BOARD=y       # 涂鸦 T5AI 开发板
CONFIG_TUYA_T5AI_BOARD_LCD_35565=y          # 3.5寸 480x320 RGB565 屏
CONFIG_TUYA_T5AI_BOARD_CAMERA=y             # GC2145 DVP 摄像头
CONFIG_ENABLE_AI_CHAT_CUSTOM_UI=y           # 自定义 UI 模式
CONFIG_ENABLE_COMP_AI_PICTURE=y             # 图片组件
CONFIG_ENABLE_COMP_AI_VIDEO=y               # 摄像头组件
CONFIG_SDIO_HOST=y                          # SD 卡主机
CONFIG_SDCARD=y                             # SD 卡驱动
CONFIG_FATFS=y                              # FAT 文件系统
CONFIG_FATFS_SDCARD=y                       # SD 卡文件系统
CONFIG_ENABLE_IMAGE_ALBUM_STORAGE_SD=y      # 相册 SD 卡持久化
CONFIG_LVGL_ENABLE_TP=y                     # 触摸面板
```

## 卡片结果 UI

识别结果以覆盖层卡片形式展示：
- 半透明黑色背景遮罩
- 居中白色圆角卡片 (700x340)
- 蓝色标题 + 黑色正文 + 灰色辅助信息
- 15 秒自动隐藏，点击遮罩可立即关闭
- 支持流式文本拼接后解析 JSON card
- 非 JSON 格式的 AI 回复也会以纯文本卡片展示
