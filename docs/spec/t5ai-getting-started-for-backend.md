# T5AI Board — TuyaOpen 开发参考（AI Agent 用）

> 本文档供 AI 模型在辅助 T5AI 嵌入式开发时参考，包含硬件规格、完整操作命令、已知错误处理。

---

## 硬件规格

| 参数 | 值 |
|------|---|
| 芯片 | T5-E1-IPEX（ARM Cortex-M33F @ 480MHz） |
| 内存 | 16 MB PSRAM + 640 KB SRAM |
| Flash | 8 MB |
| 无线 | Wi-Fi 2.4GHz 802.11b/g/n/ax + BLE 5.4 |
| 音频 | 2 麦克风（16bit/48KHz）+ 1 扬声器 |
| GPIO | 56 pin（SPI×2、QSPI×2、UART×3、I2C×2、I2S×3、SDIO、CAN、PWM×12） |
| 扩展 | TF 卡槽、DVP 摄像头、LCD 3.5寸 RGB |
| 供电 | USB Type-C 5V/1A（同时用于烧录和日志） |

**串口分配**：设备连接后出现两个串口，编号较小的为烧录口，较大的为日志口。

---

## 仓库结构关键路径

```
TuyaOpen/
├── export.sh                          # 环境激活脚本，每次开终端必须先执行
├── tos.py                             # 统一工具入口（编译/烧录/监控）
├── apps/tuya.ai/your_chat_bot/        # AI 聊天机器人 App
│   ├── app_default.config             # Kconfig 配置（板子选择、功能开关）
│   ├── config/                        # 各板子的预设配置文件
│   ├── include/tuya_config.h          # 设备凭证（UUID/AuthKey）
│   └── src/
├── apps/tuya.ai/ai_components/        # 共享 AI 组件库
│   └── assets/
│       ├── language/zh-CN/language.json  # 字符串源文件（⚠️ 唯一可修改处）
│       └── include/lang_config.h         # 自动生成，勿直接修改
└── platform/                          # 各芯片平台 SDK（首次编译自动下载）
```

---

## 环境搭建

### 系统依赖（Ubuntu/Debian）

```bash
sudo apt-get install lcov cmake-curses-gui build-essential ninja-build \
  wget git python3 python3-pip python3-venv libc6-i386 libsystemd-dev
```

### 激活环境

```bash
git clone https://github.com/tuya/TuyaOpen.git
cd TuyaOpen
. ./export.sh
```

**激活后可用环境变量**：`OPEN_SDK_ROOT`、`OPEN_SDK_PYTHON`、`OPEN_SDK_PIP`

> ⚠️ 每次重新打开终端必须重新执行 `. ./export.sh`。`OPEN_SDK_PYTHON` 未设置时，CMake 不会触发 `lang_config.h` 的自动生成。

### 验证

```bash
tos.py version
tos.py check    # 检查工具链和 submodule
```

### 常见环境问题

| 错误 | 原因 | 修复 |
|------|------|------|
| 激活失败 | 缺少 `python3-venv` | `sudo apt install python3-venv && rm -rf .venv && . ./export.sh` |
| submodule 未初始化 | 未拉取子模块 | `git submodule update --init` |

---

## 编译

```bash
cd apps/tuya.ai/your_chat_bot

# 选择板子配置（交互式）
tos.py config choice

# 或直接覆盖（非交互式，推荐）
cp config/TUYA_T5AI_BOARD_LCD_3.5_CAM_PRINTER.config app_default.config

# 编译
tos.py build
```

**产物路径**：`.build/bin/your_chat_bot_QIO_<version>.bin`

### 清理

```bash
tos.py clean        # 清理 ninja 产物，保留 CMake 缓存
rm -rf .build       # 完全清理，修改 app_default.config 后必须使用
```

> ⚠️ 修改 `app_default.config` 后若只执行 `tos.py clean`，CMake 缓存（`using.config`）不会刷新，新配置不生效。必须 `rm -rf .build` 后重新编译。

---

## 烧录

### 串口权限（Linux/macOS，首次）

```bash
sudo usermod -aG dialout $USER    # 需重启系统生效
```

### 烧录命令

```bash
tos.py flash    # 列出可用串口，选择编号较小的（烧录口）
```

### 已知错误处理

#### `flashSize is not defined`

**现象**：执行 `tos.py flash` 或使用 tyutool 烧录时报错 `flashSize is not defined`

**原因**：烧录工具版本兼容性问题

**修复**：使用 tyutool v3.0.5

- 下载地址：https://github.com/tuya/tyutool/releases/tag/v3.0.5
- 支持 GUI 和命令行两种使用方式

---

## 设备授权

每台设备需要唯一的 UUID + AuthKey 才能连接涂鸦云。

### 获取授权码

1. 登录 [platform.tuya.com](https://platform.tuya.com/)
2. 创建产品 → 选择 T5 模组 → 上传占位固件
3. 领取 2 个免费授权码（开发阶段免费）

> ⚠️ 一个 UUID 同时只能对应一台在线设备。

### 写入方式

**方式一：串口命令行（无需重新编译）**

```bash
tos.py monitor -b 115200
# 串口提示符下输入：
auth uuid[你的uuid] key[你的authkey]
```

**方式二：硬编码到固件**

编辑 `include/tuya_config.h`：

```c
#define TUYA_OPENSDK_UUID    "uuidxxxxxxxxxxxxxxxx"
#define TUYA_OPENSDK_AUTHKEY "keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
```

然后重新编译烧录。

---

## 配网

- App：**智能生活**（App Store / 各安卓市场）
- 进入配网模式：5 秒内重启设备 3 次
- 仅支持 **2.4GHz Wi-Fi**，5GHz 不可用

---

## 调试

```bash
tos.py monitor              # 实时日志，Ctrl+C 退出
tos.py monitor -b 115200    # 指定波特率
```

### 日志宏

```c
PR_DEBUG("msg %d", val);   // 调试级，可编译关闭
PR_INFO("msg");
PR_WARN("msg");
PR_ERR("msg %d", ret);
```

---

## 关键约束与易错点

| 场景 | 约束 |
|------|------|
| 字符串常量 | 只能修改 `language/zh-CN/language.json`，不能直接改 `lang_config.h`（自动生成文件，clean 后被覆盖） |
| 修改 Kconfig | 必须 `rm -rf .build` 后重新编译，`tos.py clean` 不够 |
| Wi-Fi 配网 | 仅支持 2.4GHz，5GHz 路由需切换频段 |
| 串口选择 | 两个串口中编号小的烧录，编号大的看日志 |
| 授权码 | 一个 UUID 只能同时绑定一台设备 |
| 烧录报错 `flashSize is not defined` | 使用 tyutool v3.0.5：https://github.com/tuya/tyutool/releases/tag/v3.0.5 |

---

## 参考链接

- T5AI 开发板概览：https://www.tuyaopen.ai/zh/docs/hardware-specific/tuya-t5/t5-ai-board/overview-t5-ai-board
- 快速开始：https://www.tuyaopen.ai/zh/docs/quick-start/enviroment-setup
- 免费授权码申请：https://www.tuyaopen.ai/zh/docs/faqs/get-developer-license
- tyutool v3.0.5：https://github.com/tuya/tyutool/releases/tag/v3.0.5
- TuyaOpen GitHub：https://github.com/tuya/TuyaOpen
