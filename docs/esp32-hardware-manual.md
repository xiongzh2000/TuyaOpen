# TuyaOpen ESP32 支持手册（AI Agent 用）

> 整理自 https://www.tuyaopen.ai/zh/docs/hardware-specific/espressif/

---

## 目录

1. [概览：TuyaOpen 与 ESP-IDF 的关系](#概览)
2. [ESP32 快速开始](#快速开始)
3. [支持的功能与芯片对比](#支持的功能与芯片对比)
4. [GPIO 引脚映射](#gpio-引脚映射)
5. [适配新开发板](#适配新开发板)
6. [OTA 固件升级](#ota-固件升级)

---

## 概览

### TuyaOpen 与 ESP-IDF 的关系

TuyaOpen **构建于 ESP-IDF 之上**，而非替代它。

```
TuyaOpen 应用层（tal_api / ai_components）
        ↓
TKL 适配层（tkl_gpio.c / tkl_wifi.c …）← 将 TAL 调用转换为 ESP-IDF 调用
        ↓
ESP-IDF（FreeRTOS / lwIP / Wi-Fi 驱动 / 蓝牙控制器）
        ↓
ESP32 硬件
```

### API 选择决策表

| 需求 | 推荐方案 | 理由 |
|------|---------|------|
| Wi-Fi、GPIO、UART、I2C、PWM 等基础外设 | TuyaOpen TKL/TAL API | 跨平台，代码可在 T5AI/ESP32/Linux 复用 |
| Tuya Cloud、设备管理、OTA、授权 | TuyaOpen 云服务 | Tuya 生态集成 |
| AI 功能（ASR、TTS、LLM Agent）| TuyaOpen AI SDK | Tuya AI Agent 集成 |
| ESP32 专有功能（ULP、ESP-NOW、USB OTG）| 直接 ESP-IDF | TuyaOpen 未提供抽象 |

**经验法则**：所有跨平台功能使用 TuyaOpen API；仅 ESP32 特有且 TuyaOpen 未抽象的功能才直接调用 ESP-IDF。

### 核心优势

1. **Tuya Cloud 集成** — 设备激活、远程控制、OTA、数据点开箱即用
2. **跨平台可移植性** — 相同代码可在 T5AI、T2、T3、树莓派、ESP32 上运行
3. **AI 能力** — Tuya AI Agent、语音交互、LLM 服务统一 SDK
4. **产品化路径** — 内置授权码管理、OTA、应用配网
5. **可复用外设驱动** — 显示屏、音频编解码器、按键、LED、传感器驱动库

---

## 快速开始

### 硬件准备

- ESP32 系列开发板（ESP32 / ESP32-S3 / ESP32-C3 / ESP32-C6）
- USB 数据线（支持数据传输）
- 电脑（Linux / macOS / Windows）
- 2.4GHz Wi-Fi 网络
- 涂鸦授权码（使用 Tuya Cloud 功能时需要，本地 GPIO 示例不需要）

### 完整流程

```bash
# 1. 克隆并激活环境
git clone https://github.com/tuya/TuyaOpen.git
cd TuyaOpen
. ./export.sh

# 2. 进入示例项目
cd apps/tuya_cloud/switch_demo        # 云端开关（需授权码）
# 或
cd examples/gpio/gpio_output          # 本地 GPIO 示例（无需授权码）

# 3. 选择 ESP32 开发板配置
tos.py config choice
# 从列表中选择对应的 .config，如：ESP32S3.config、DNESP32S3_BOX.config 等

# 4. 编译（首次会下载 ESP-IDF 工具链，需要时间）
tos.py build

# 5. Linux 首次烧录需要串口权限
sudo usermod -aG dialout $USER   # 需重启系统

# 6. 烧录
tos.py flash
tos.py flash --port /dev/ttyUSB0   # 手动指定串口

# 7. 查看日志
tos.py monitor
# 或使用 ESP-IDF 原生监控
tos.py idf monitor
```

### ESP32 与标准流程的差异

| 操作 | 标准（T5AI）| ESP32 |
|------|-----------|-------|
| 工具链 | TuyaOpen 内置 | 首次编译自动下载 ESP-IDF 工具链 |
| 串口监控 | `tos.py monitor` | `tos.py monitor` 或 `tos.py idf monitor` |
| ESP 专有配置 | 不适用 | `tos.py idf menuconfig` |

---

## 支持的功能与芯片对比

### 芯片规格对比

| 特性 | ESP32 | ESP32-S3 | ESP32-C3 | ESP32-C6 |
|------|-------|---------|---------|---------|
| CPU | 双核 Xtensa LX6 | 双核 Xtensa LX7 | 单核 RISC-V | 单核 RISC-V |
| Wi-Fi | 802.11 b/g/n | 802.11 b/g/n | 802.11 b/g/n | 802.11 b/g/n/ax（Wi-Fi 6）|
| 蓝牙 | 经典 BT + BLE 4.2 | BLE 5.0 | BLE 5.0 | BLE 5.0 |
| USB | 无 | USB OTG | 无 | 无 |

### 已实现的 TKL 外设

| 外设 | 支持状态 | 备注 |
|------|---------|------|
| Wi-Fi | ✅ | 仅 2.4GHz |
| BLE | ✅ | |
| GPIO | ✅ | 直接映射，见引脚映射章节 |
| UART | ✅ | |
| PWM | ✅ | |
| ADC | ✅ | ESP32 经典版 Wi-Fi 启用时 ADC2 不可用 |
| I2C | ✅ | |
| Flash | ✅ | |
| Timer | ✅ | |
| WatchDog | ✅ | |
| RTC | ✅ | |
| SPI | ❌ | 未实现 |

### 音频驱动支持

| 驱动 | 芯片 | 采样率 | 接口 |
|------|------|-------|------|
| ES8311 | ES8311 | 16kHz | I2S |
| ES8388 | ES8388 | 16kHz | I2S |
| ES8389 | ES8389 | 16kHz | I2S |
| 无编解码 DAC | 直出 | 16kHz | I2S/DAC |

### 显示屏驱动支持

| 接口 | 驱动芯片 | 像素格式 |
|------|---------|---------|
| SPI | ST7789 | RGB565 |
| 并行 8-bit（8080）| ST7789 | RGB565 |
| I2C（OLED）| SSD1306 | 单色 |
| QSPI（AMOLED）| SH8601 | RGB565 |

### 已预配置的开发板（`config/` 目录）

| 配置文件名 | 开发板 | 特性 |
|-----------|-------|------|
| `DNESP32S3_BOX.config` | 正点原子 ESP32S3 BOX | LCD + 音频 |
| `DNESP32S3_BOX2_WIFI.config` | 正点原子 BOX2（Wi-Fi 版）| LCD + 4G + 充电管理 |
| `ESP32S3_BREAD_COMPACT_WIFI.config` | ESP32S3 面包板紧凑版 | 基础 |
| `WAVESHARE_ESP32S3_TOUCH_AMOLED_1_8.config` | 微雪 ESP32S3 1.8寸 AMOLED | 触摸 AMOLED |
| `XINGZHI_ESP32S3_Cube_0_96OLED_WIFI.config` | 星智 0.96 OLED | OLED 小屏 |
| 更多... | 见 `config/` 目录 | |

---

## GPIO 引脚映射

### 映射规则（1:1 直接映射）

ESP32 平台的 TUYA_GPIO 编号与 ESP32 物理 GPIO 编号**完全一致**：

```
TUYA_GPIO_NUM_0  → GPIO0
TUYA_GPIO_NUM_18 → GPIO18
TUYA_GPIO_NUM_48 → GPIO48（ESP32-S3 专有）
```

映射定义在 `platform/ESP32/tuya_open_sdk/platform/vendor_bsp/tkl_pin.c` 的 `pinmap[]` 数组中。

### 各芯片 GPIO 范围

| 芯片 | 可用 GPIO 范围 |
|------|--------------|
| ESP32 经典版 | GPIO 0 ~ 39 |
| ESP32-S3 | GPIO 0 ~ 48 |
| ESP32-C3 | GPIO 0 ~ 21 |
| ESP32-C6 | GPIO 0 ~ 30 |

### Pinmux（引脚复用）

ESP32 的 I2C、PWM、UART 等外设不固定到特定引脚，可通过 Pinmux 灵活路由：

```c
// 必须在外设初始化之前调用
TUYA_PIN_FUNC_E func = TUYA_PIN_FUNC_I2C0_SCL;
tkl_io_pinmux_config(TUYA_GPIO_NUM_5, func);

// 之后再初始化外设
tkl_i2c_init(TUYA_I2C_NUM_0, &cfg);
```

> ⚠️ `tkl_io_pinmux_config()` **必须在** 对应外设的 `init()` 之前调用，否则不生效。

---

## 适配新开发板

### 命令生成模板

```bash
tos.py new board
# 按提示选择：
# - 平台：ESP32
# - 芯片：esp32 / esp32s3 / esp32c3 / esp32c6
# - 板子名称：建议格式 {厂商}_{芯片}_{型号}，如 WAVESHARE_ESP32S3_TOUCH_AMOLED
```

**生成目录结构**：`boards/ESP32/{BOARD_NAME}/`

```
boards/ESP32/MY_BOARD/
├── Kconfig           # 芯片选择和引脚配置
├── CMakeLists.txt    # 构建配置
├── board_com_api.h   # 硬件注册 API 声明
└── board_com_api.c   # board_register_hardware() 实现
```

### Kconfig 配置（芯片选择）

```kconfig
# boards/ESP32/MY_BOARD/Kconfig
config BOARD_MY_BOARD
    bool "My Custom ESP32 Board"

if BOARD_MY_BOARD
    choice CHIP_CHOICE
        prompt "Select chip"
        default CHIP_ESP32S3

        config CHIP_ESP32S3
            bool "ESP32-S3"
    endchoice

    # 启用功能模块
    config ENABLE_WIFI
        bool
        default y
    
    config ENABLE_DISPLAY
        bool
        default y
        select ENABLE_LIBLVGL
endif
```

### board_com_api.c 实现

```c
// boards/ESP32/MY_BOARD/board_com_api.c
#include "board_com_api.h"
#include "tdd_disp_spi_device.h"
#include "tdd_audio_es8311.h"

OPERATE_RET board_register_hardware(void)
{
    OPERATE_RET rt = OPRT_OK;

    // 注册显示屏（SPI LCD）
    TDD_DISP_SPI_CFG_T lcd_cfg = {
        .spi_port = TUYA_SPI_NUM_2,
        .cs_pin   = TUYA_GPIO_NUM_10,
        .dc_pin   = TUYA_GPIO_NUM_46,
        .rst_pin  = TUYA_GPIO_NUM_9,
    };
    TUYA_CALL_ERR_LOG(tdd_disp_spi_device_register(DISPLAY_NAME, &lcd_cfg));

    // 注册音频
    TDD_AUDIO_ES8311_T audio_cfg = {
        .i2c_port    = TUYA_I2C_NUM_0,
        .i2s_port    = TUYA_I2S_NUM_0,
        .sample_rate = TKL_AUDIO_SAMPLE_16K,
        .spk_pin     = TUYA_GPIO_NUM_7,
    };
    TUYA_CALL_ERR_LOG(tdd_audio_es8311_register(AUDIO_NAME, &audio_cfg));

    return rt;
}

// 有 LVGL 触摸屏时还需实现：
void board_display_on(void)  { /* 背光开 */ }
void board_display_off(void) { /* 背光关 */ }
```

### 命名规范

- 全大写字母 + 下划线
- 格式：`{厂商}_{芯片}_{型号}`
- 示例：`WAVESHARE_ESP32S3_TOUCH_AMOLED`、`ALIENTEK_ESP32S3_BOX`

### 验证步骤

```bash
# 第一步：基础编译验证（GPIO 示例）
cd examples/gpio/gpio_output
cp boards/ESP32/MY_BOARD/my_board.config app_default.config
tos.py build

# 第二步：云连接验证（Switch Demo）
cd apps/tuya_cloud/switch_demo
tos.py config choice   # 选择自定义板子配置
tos.py build && tos.py flash
```

### 未支持芯片的适配

1. 在 `platform/ESP32/build_setup.py` 注册新芯片名称
2. 在 TKL 驱动层（`tkl_gpio.c`、`tkl_wifi.c` 等）添加条件编译分支
3. 创建基础板子验证编译通过

---

## OTA 固件升级

### 前提条件

- 设备已连接涂鸦云（完成授权和配网）
- 在 `tuya_config.h` 中定义 `TUYA_DEVICE_FIRMWAREKEY`（同一 PID 下有多个固件时必须设置）

### 完整 OTA 流程

#### Step 1：创建固件 Key

涂鸦 IoT 平台 → 产品开发 → 新增自定义固件 → 系统自动生成 `firmware_key`

#### Step 2：写入固件 Key 并更新版本号

```c
// include/tuya_config.h
// #define TUYA_DEVICE_FIRMWAREKEY "your_firmware_key_here"   // 多固件时取消注释

// tuya_main.c
ret = tuya_iot_init(&ai_client, &(const tuya_iot_config_t){
    .software_ver  = PROJECT_VERSION,   // 必须高于当前设备版本
    .productkey    = TUYA_PRODUCT_ID,
    .uuid          = license.uuid,
    .authkey       = license.authkey,
    // .firmware_key  = TUYA_DEVICE_FIRMWAREKEY,   // 多固件时启用
    .event_handler = user_event_handler_on,
});
```

```ini
# app_default.config
CONFIG_PROJECT_VERSION="1.0.2"   # 递增版本号
```

#### Step 3：编译并上传固件

```bash
tos.py build
# 产物位于 .build/bin/：
# {name}_QIO_{ver}.bin  ← 生产固件（全量）
# {name}_UA_{ver}.bin   ← 用户区固件
# {name}_UG_{ver}.bin   ← 差分升级包
```

上传到涂鸦 IoT 平台 → 产品开发 → 固件升级 → 上传固件版本

#### Step 4：固件上架

将上传的版本状态改为"已上架"，才能用于 OTA 推送。

#### Step 5：配置升级规则

涂鸦 IoT 平台配置：
- 选择固件版本
- 升级方式（静默/提示）
- 待升级的版本号范围
- 地区范围

#### Step 6：灰度验证

先将测试设备加入白名单进行验证，确认升级成功后再扩大比例。

#### Step 7：发布

官方建议**灰度发布**（从 5% 开始），可随时暂停排查问题；确认稳定后再全量发布。

### 关键约束

| 约束 | 说明 |
|------|------|
| 版本号必须递增 | 新版本号必须高于设备当前版本，否则不触发升级 |
| `firmware_key` 必要性 | 同一 PID 下有多个固件时必须设置，否则平台无法确定推送目标 |
| 优先灰度发布 | 避免大规模推送引入未发现的问题 |
| OTA 推送处理 | 在 `TUYA_EVENT_UPGRADE_NOTIFY` 事件中实现下载和升级逻辑 |

### 设备端 OTA 事件处理

```c
void user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event)
{
    switch (event->id) {
    case TUYA_EVENT_UPGRADE_NOTIFY: {
        cJSON *upgrade = event->value.asJSON;
        cJSON *url     = cJSON_GetObjectItem(upgrade, "httpsUrl");
        cJSON *version = cJSON_GetObjectItem(upgrade, "version");
        // 下载并应用固件更新
        PR_INFO("OTA: version=%s url=%s",
                version->valuestring, url->valuestring);
        break;
    }
    default:
        break;
    }
}
```

---

## 参考链接

- ESP32 概览：https://www.tuyaopen.ai/zh/docs/hardware-specific/espressif/overview-esp32
- ESP32 快速开始：https://www.tuyaopen.ai/zh/docs/hardware-specific/espressif/esp32-quick-start
- 支持功能列表：https://www.tuyaopen.ai/zh/docs/hardware-specific/espressif/esp32-supported-features
- GPIO 引脚映射：https://www.tuyaopen.ai/zh/docs/hardware-specific/espressif/esp32-pin-mapping
- 适配新开发板：https://www.tuyaopen.ai/zh/docs/hardware-specific/espressif/esp32-new-board
- OTA 固件升级：https://www.tuyaopen.ai/zh/docs/hardware-specific/espressif/esp32-ota
