# TuyaOpen 嵌入式编程技术手册（AI Agent 用）

> 本文档整理自 https://www.tuyaopen.ai/zh/docs/peripheral/ 系列文档，供 AI 模型辅助 TuyaOpen 嵌入式开发时查阅。

---

## 目录

1. [架构概览](#架构概览)
2. [系统编程：线程与定时器](#系统编程线程与定时器)
3. [内存管理](#内存管理)
4. [KV 持久化存储](#kv-持久化存储)
5. [网络：Wi-Fi Station API](#网络wi-fi-station-api)
6. [图形显示：TDL/TDD 层](#图形显示tdltdd-层)
7. [LVGL 应用开发](#lvgl-应用开发)
8. [音频驱动](#音频驱动)
9. [硬件接口 TKL API](#硬件接口-tkl-api)
   - [GPIO](#gpio)
   - [UART](#uart)
   - [I2C](#i2c)
   - [SPI](#spi)
   - [PWM](#pwm)
   - [ADC](#adc)
10. [外设驱动支持列表](#外设驱动支持列表)

---

## 架构概览

TuyaOpen 的外设层分三级抽象：

```
应用层（App / ai_components）
        ↓
TAL（Tuya Abstraction Layer）  ← 主要编程接口，跨平台
        ↓
TDL（Tuya Driver Layer）       ← 外设管理抽象（显示、音频等）
        ↓
TDD（Tuya Device Driver）      ← 具体芯片驱动实例（LCD、编解码器等）
        ↓
TKL（Tuya Kernel Layer）       ← 硬件寄存器级接口（GPIO/UART/I2C/SPI…）
```

- **TAL**：日常开发用这层（线程、定时器、内存、KV、Wi-Fi、日志）
- **TDL/TDD**：外设注册和管理（显示屏、音频设备）
- **TKL**：直接操作硬件，通常由驱动层使用，App 层较少直接调用

---

## 系统编程：线程与定时器

头文件：`tal_thread.h`、`tal_sw_timer.h`、`tal_mutex.h`、`tal_semaphore.h`、`tal_queue.h`

### 线程

```c
// 线程配置
THREAD_CFG_T cfg = {
    .thrdname   = "my_task",
    .stackDepth = 4096,          // 最小 4096，ESP32-S3 自动追加 1024
    .priority   = THREAD_PRIO_3, // 1=实时控制 3=业务逻辑 5=后台任务
};

THREAD_HANDLE handle = NULL;
tal_thread_create_and_start(&handle, NULL, NULL, my_task_fn, NULL, &cfg);

// 线程函数内部自删
static void my_task_fn(void *arg) {
    while (tal_thread_get_state(handle) == THREAD_STATE_RUNNING) {
        // 业务逻辑
        tal_system_sleep(100);  // 最小 10ms
    }
    tal_thread_delete(handle);
    handle = NULL;
}
```

**优先级参考**：

| 优先级 | 用途 |
|--------|------|
| `THREAD_PRIO_1` | 音频、实时控制 |
| `THREAD_PRIO_3` | 传感器读取、业务逻辑 |
| `THREAD_PRIO_5` | 日志、后台同步 |

> ⚠️ 线程只能从自身内部删除，从外部删除会导致资源状态不一致。

### 软件定时器

```c
TIMER_ID timer_id = NULL;

// 创建定时器
tal_sw_timer_create(my_timer_cb, NULL, &timer_id);

// 启动（周期 1000ms，循环触发）
tal_sw_timer_start(timer_id, 1000, TAL_TIMER_CYCLE);

// 启动（单次触发）
tal_sw_timer_start(timer_id, 5000, TAL_TIMER_ONCE);

// 停止 / 删除
tal_sw_timer_stop(timer_id);
tal_sw_timer_delete(timer_id);

// 回调函数签名
static void my_timer_cb(TIMER_ID timer_id, void *arg) {
    // 定时任务
}
```

### 互斥锁

```c
MUTEX_HANDLE mutex = NULL;
tal_mutex_create_init(&mutex);
tal_mutex_lock(mutex);
// 临界区
tal_mutex_unlock(mutex);
tal_mutex_release(mutex);
```

### 信号量

```c
SEM_HANDLE sem = NULL;
tal_semaphore_create_init(&sem, 0, 1);  // 初始值 0，最大值 1
tal_semaphore_post(sem);                // 发信号
tal_semaphore_wait(sem, 1000);          // 等待，超时 1000ms
tal_semaphore_release(sem);
```

### 消息队列

```c
QUEUE_HANDLE queue = NULL;
tal_queue_create_init(&queue, sizeof(MY_MSG_T), 16);  // 16 个元素
tal_queue_post(queue, &msg, 0);                        // 发送，不等待
tal_queue_fetch(queue, &msg, 1000);                    // 接收，超时 1000ms
tal_queue_free(queue);
```

---

## 内存管理

### 内存层级（按平台）

| 内存类型 | 函数 | 适用场景 | 平台 |
|---------|------|---------|------|
| 片上 SRAM | `tal_malloc` / `tal_free` | 栈、小 DMA 缓冲、延迟敏感数据 | 全平台 |
| PSRAM（片外扩展）| `tal_psram_malloc` / `tal_psram_free` | 音频、图形、AI 大缓冲区 | T5AI、ESP32（需 `ENABLE_EXT_RAM=y`）|
| NOR Flash | TKL Flash API | 固件、OTA、用户持久化数据 | T5AI、ESP32 |
| OS 虚拟内存 | `tal_malloc`（透明） | 堆内存 | Linux |

```c
// 普通堆分配
uint8_t *buf = (uint8_t *)tal_malloc(1024);
if (buf == NULL) { /* 处理失败 */ }
tal_free(buf);

// PSRAM 分配（大缓冲区）
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
uint8_t *big_buf = (uint8_t *)tal_psram_malloc(512 * 1024);
tal_psram_free(big_buf);
#endif

// 查询剩余堆大小（监控用）
uint32_t free_heap  = tal_system_get_free_heap_size();
uint32_t free_psram = tal_psram_get_free_heap_size();
```

---

## KV 持久化存储

TAL KV 基于 FlashDB，数据持久化到 Flash，掉电不丢失。

### 初始化（只需一次，在 user_main 中）

```c
tal_kv_init(&(tal_kv_cfg_t){
    .seed = "vmlkasdh93dlvlcy",   // 加密种子（固定，不可改）
    .key  = "dflfuap134ddlduq",   // 加密密钥（固定，不可改）
});
```

### 读写删

```c
// 写入
const char *val = "{\"volume\":70}";
tal_kv_set("my_key", (const uint8_t *)val, strlen(val));

// 读取（返回的 value 需用 tal_kv_free 释放）
uint8_t *value = NULL;
size_t   len   = 0;
if (OPRT_OK == tal_kv_get("my_key", &value, &len)) {
    // 使用 value
    tal_kv_free(value);
}

// 删除
tal_kv_del("my_key");
```

> ⚠️ `tal_kv_get` 返回的 `value` 必须用 `tal_kv_free` 释放，不能用 `tal_free`。

---

## 网络：Wi-Fi Station API

头文件：`tal_wifi.h`

### 初始化与连接

```c
tal_wifi_init(WIFI_EVENT_CB);         // 注册事件回调
tal_wifi_station_connect(ssid, passwd); // 连接 AP
```

### 状态查询

```c
WF_STATION_STAT_E status;
tal_wifi_station_get_status(&status);
// STAT_CONNECT = 已连接，STAT_CONN_FAIL = 失败
```

### AP 扫描

```c
AP_IF_S *ap_list = NULL;
uint32_t ap_num  = 0;
tal_wifi_all_ap_scan(&ap_list, &ap_num);
// ap_list[i].ssid, ap_list[i].rssi
tal_wifi_release_ap(ap_list);
```

### 注意事项

- T5AI 仅支持 **2.4GHz**，5GHz 不可用
- ESP32 Wi-Fi 启用时 ADC2 不可用
- ESP32 自动开启省电模式（降低功耗但增加延迟）
- Tuya 云连接场景使用 `netmgr` 模块而非直接调用 `tal_wifi`，见 `tuya_main.c`

---

## 图形显示：TDL/TDD 层

### 架构

```
LVGL（lv_vendor 封装）
        ↓
TDL（tdl_display）  ← 统一接口：open/close/refresh/backlight
        ↓
TDD（具体 LCD 驱动注册）  ← 各芯片实例
        ↓
TKL（SPI/RGB/I2C 等硬件接口）
```

### 支持的接口和芯片

| 接口类型 | 支持的芯片 |
|---------|-----------|
| SPI | ST7789、ST7305、ST7306 |
| RGB（并行）| ILI9488、GC9A01、ILI9341、ST7789 |
| MCU 8080 | ST7796、ST7789 |
| QSPI | ST7735S |
| I2C（OLED）| SSD1306（128×32 / 128×64）|

### 驱动注册方式

```c
// SPI LCD（如 ST7789）
tdd_disp_spi_device_register(DISPLAY_NAME, &spi_cfg);

// RGB 大屏（如 ILI9341）
tdd_disp_rgb_device_register(DISPLAY_NAME, &rgb_cfg);

// 8080 / QSPI / I2C：在 board 层 C 文件中实现注册
// 参考：lcd_st7789_80.c / lcd_sh8601.c / oled_ssd1306.c
```

### TDL 核心接口

```c
TDL_DISP_HANDLE hdl = tdl_display_find(DISPLAY_NAME);
tdl_display_open(hdl, &cfg);

// 帧缓冲
TDL_DISP_FB_T fb;
tdl_display_fb_create(hdl, width, height, &fb);   // 在 SRAM/PSRAM 分配
tdl_display_refresh(hdl, &fb);                     // 刷新到屏幕
tdl_display_fb_release(hdl, &fb);

// 背光
tdl_display_set_backlight(hdl, 80);  // 0-100

tdl_display_close(hdl);
```

---

## LVGL 应用开发

TuyaOpen 使用 `lv_vendor` 层封装 LVGL，源码位于 `src/liblvgl/v9/port/lv_vendor.h`。

### 初始化流程

```c
void user_main(void)
{
    // 1. 注册硬件（LCD、触摸等）
    board_register_hardware();

    // 2. 初始化 LVGL 厂商层（传入 display 名称）
    lv_vendor_init(DISPLAY_NAME);

    // 3. 加锁后创建 UI
    lv_vendor_disp_lock();
    /* 创建所有 LVGL 控件 */
    lv_vendor_disp_unlock();

    // 4. 启动 LVGL 线程（优先级 5，栈 8KB）
    lv_vendor_start(5, 1024 * 8);
}
```

### 在其他线程更新 UI

**所有 LVGL 操作必须在 lock/unlock 之间执行**：

```c
lv_vendor_disp_lock();
lv_label_set_text(label, "新内容");
lv_obj_set_style_text_color(label, lv_color_hex(0xFF0000), 0);
lv_vendor_disp_unlock();
```

### 常用控件示例

```c
lv_vendor_disp_lock();

// 标签
lv_obj_t *label = lv_label_create(lv_screen_active());
lv_label_set_text(label, "Hello TuyaOpen");
lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

// 图片
lv_obj_t *img = lv_img_create(lv_screen_active());
lv_img_set_src(img, &icon_my_image);  // LV_IMG_DECLARE(icon_my_image) 声明

// 流式文字追加
lv_label_ins_text(label, LV_LABEL_POS_LAST, "追加文字");

lv_vendor_disp_unlock();
```

### ai_ui_disp_msg（ai_components 封装）

`ai_components/ai_ui` 层封装了线程安全的消息分发：

```c
// 异步（不阻塞）
ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)THINKING, strlen(THINKING));
ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_NEUTRAL, strlen(EMOJI_NEUTRAL));
ai_ui_disp_msg(AI_UI_DISP_NOTIFICATION, (uint8_t *)"提示文字", strlen("提示文字"));

// 同步（阻塞直到 UI 线程处理完）
ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_OPEN, NULL, 0);
```

### 示例项目路径

| 示例 | 路径 |
|------|------|
| 基础 LVGL | `examples/graphics/lvgl_demo` |
| 标签控件 | `examples/graphics/lvgl_label` |
| GIF 动画 | `examples/graphics/lvgl_gif` |
| 摄像头预览 | `examples/graphics/lvgl_camera` |

---

## 音频驱动

头文件：`tdl_audio.h`、`tdd_audio_xxx.h`

### 设备发现与打开

```c
TDL_AUDIO_HANDLE hdl = NULL;
tdl_audio_find(AUDIO_NAME, &hdl);   // 按名称查找设备

TDL_AUDIO_CFG_T cfg = {
    .mic_cb = my_mic_callback,       // 麦克风数据回调
};
tdl_audio_open(hdl, &cfg);
```

### 播放控制

```c
// 播放 PCM 数据
tdl_audio_play(hdl, pcm_buf, pcm_len);

// 停止播放
tdl_audio_play_stop(hdl);

// 音量（0-100）
tdl_audio_volume_set(hdl, 70);

// 关闭
tdl_audio_close(hdl);
```

### T5AI 音频配置结构

```c
TDD_AUDIO_T5AI_T audio_cfg = {
    .aec_enable  = 1,                    // 回声消除（需硬件支持）
    .sample_rate = TKL_AUDIO_SAMPLE_16K,
    .data_bits   = TKL_AUDIO_DATABITS_16,
    .channel     = TKL_AUDIO_CHANNEL_MONO,
    .spk_pin     = SPEAKER_EN_PIN,       // 扬声器使能引脚
};
```

### Kconfig 配置

| 选项 | 说明 |
|------|------|
| `ENABLE_COMP_AI_AUDIO` | 启用音频组件 |
| `ENABLE_AEC` | 启用回声消除（AEC，需硬件支持） |
| `ENABLE_COMP_AI_AUDIO_CODEC_OPUS` | 启用 Opus 编解码器 |

> ⚠️ 硬件不支持 AEC 时必须关闭 `ENABLE_AEC`，否则会影响唤醒词和对话功能。

---

## 硬件接口 TKL API

> TKL 层直接操作芯片寄存器，通常由 TDD 驱动层调用。App 层需要直接操作硬件时使用。

### GPIO

头文件：`tkl_gpio.h`

```c
// 输出配置
TUYA_GPIO_BASE_CFG_T cfg = {
    .mode      = TUYA_GPIO_PUSH_PULL,  // 推挽输出
    .direct    = TUYA_GPIO_OUTPUT,
    .level     = TUYA_GPIO_LEVEL_LOW,
};
tkl_gpio_init(TUYA_GPIO_NUM_5, &cfg);
tkl_gpio_write(TUYA_GPIO_NUM_5, TUYA_GPIO_LEVEL_HIGH);

// 输入 + 中断
TUYA_GPIO_BASE_CFG_T in_cfg = {
    .mode   = TUYA_GPIO_PULLUP,
    .direct = TUYA_GPIO_INPUT,
};
tkl_gpio_init(TUYA_GPIO_NUM_10, &in_cfg);

TUYA_GPIO_IRQ_T irq = {
    .mode = TUYA_GPIO_IRQ_RISE,       // 上升沿触发
    .cb   = my_gpio_irq_callback,
    .arg  = NULL,
};
tkl_gpio_irq_init(TUYA_GPIO_NUM_10, &irq);
tkl_gpio_irq_enable(TUYA_GPIO_NUM_10);
```

**GPIO 引脚编号**：`TUYA_GPIO_NUM_0` ~ `TUYA_GPIO_NUM_60`（与芯片原生编号不同）

**GPIO 模式**：上拉输入、下拉输入、高阻输入、浮空输入、推挽输出、开漏输出、开漏+上拉输出

**中断触发类型**：上升沿、下降沿、双边沿、低电平、高电平

### UART

头文件：`tkl_uart.h`

```c
TUYA_UART_BASE_CFG_T cfg = {
    .baudrate  = 115200,
    .parity    = TUYA_UART_PARITY_TYPE_NONE,
    .databits  = TUYA_UART_DATA_LEN_8BIT,
    .stopbits  = TUYA_UART_STOP_LEN_1BIT,
    .flowctrl  = TUYA_UART_FLOWCTRL_NONE,
};
tkl_uart_init(TUYA_UART_NUM_0, &cfg);

// 写
tkl_uart_write(TUYA_UART_NUM_0, (uint8_t *)"hello\r\n", 7);

// 读（轮询）
uint8_t buf[64];
int32_t len = tkl_uart_read(TUYA_UART_NUM_0, buf, sizeof(buf));

// 中断接收回调
tkl_uart_rx_irq_cb_reg(TUYA_UART_NUM_0, my_rx_callback);

// 等待数据（阻塞）
tkl_uart_wait_for_data(TUYA_UART_NUM_0, 500);  // 500ms 超时
```

### I2C

头文件：`tkl_i2c.h`

```c
TUYA_IIC_BASE_CFG_T cfg = {
    .role       = TUYA_IIC_MODE_MASTER,
    .speed      = TUYA_IIC_BUS_SPEED_400K,
    .addr_width = TUYA_IIC_ADDRESS_7BIT,
};
tkl_i2c_init(TUYA_I2C_NUM_0, &cfg);

// Master 发送（到从机地址 0x3C）
tkl_i2c_master_send(TUYA_I2C_NUM_0, 0x3C, buf, len, false);

// Master 接收
tkl_i2c_master_receive(TUYA_I2C_NUM_0, 0x3C, buf, len, false);
```

**支持速率**：100K、400K、1M、3.4MHz

**地址宽度**：7-bit、10-bit

### SPI

头文件：`tkl_spi.h`

```c
TUYA_SPI_BASE_CFG_T cfg = {
    .role      = TUYA_SPI_ROLE_MASTER,
    .mode      = TUYA_SPI_MODE0,       // CPOL=0, CPHA=0
    .databits  = TUYA_SPI_DATA_BIT8,
    .bitorder  = TUYA_SPI_ORDER_MSB2LSB,
    .freq_hz   = 10000000,             // 10MHz
};
tkl_spi_init(TUYA_SPI_NUM_0, &cfg);

// 发送
tkl_spi_send(TUYA_SPI_NUM_0, tx_buf, len, 1000);

// 接收
tkl_spi_recv(TUYA_SPI_NUM_0, rx_buf, len, 1000);

// 全双工
tkl_spi_transfer(TUYA_SPI_NUM_0, tx_buf, rx_buf, len, 1000);
```

**SPI 模式**：MODE0（CPOL=0/CPHA=0）~ MODE3（CPOL=1/CPHA=1）

### PWM

头文件：`tkl_pwm.h`

```c
TUYA_PWM_BASE_CFG_T cfg = {
    .polarity   = TUYA_PWM_POSITIVE,
    .duty       = 100,      // 占空比分子
    .cycle      = 1000,     // 占空比分母，duty/cycle = 10%
    .frequency  = 1000,     // 1000Hz
};
tkl_pwm_init(TUYA_PWM_NUM_0, &cfg);
tkl_pwm_start(TUYA_PWM_NUM_0);

// 动态调整占空比
TUYA_PWM_BASE_CFG_T new_cfg = cfg;
new_cfg.duty = 500;  // 50%
tkl_pwm_info_set(TUYA_PWM_NUM_0, &new_cfg);

tkl_pwm_stop(TUYA_PWM_NUM_0);
tkl_pwm_deinit(TUYA_PWM_NUM_0);
```

### ADC

头文件：`tkl_adc.h`

```c
TUYA_ADC_BASE_CFG_T cfg = {
    .ch_list.data = TUYA_ADC_CH_0,
    .ch_nums      = 1,
    .width        = 12,               // 12-bit 分辨率
    .freq         = 1000,             // 采样频率
    .type         = TUYA_ADC_EXTERNAL_SAMPLE_EXTERNAL_REF,
    .mode         = TUYA_ADC_SINGLE,  // 单次采样
};
tkl_adc_init(TUYA_ADC_NUM_0, &cfg);

int32_t val;
tkl_adc_read_single_channel(TUYA_ADC_NUM_0, 0, &val);

// 读取电压（mV）
tkl_adc_read_voltage(TUYA_ADC_NUM_0, &val);

tkl_adc_deinit(TUYA_ADC_NUM_0);
```

**支持分辨率**：8、10、12、16 bit

**支持通道数**：最多 16 通道（`ch_0` ~ `ch_15`）

---

## 外设驱动支持列表

所有驱动源码位于 `src/peripherals/` 或对应平台目录。

### 输入设备

| 类型 | 驱动说明 |
|------|---------|
| 按键 | GPIO 或 ADC 基础驱动，`tdl_button_manage` 封装 |
| 摇杆 | ADC 基础 |
| 旋转编码器 | GPIO 基础 |

### 输出设备

| 类型 | 芯片/型号 |
|------|---------|
| LED | GPIO 控制 |
| 可寻址灯 | WS2812、SM16703P、YX1903B |
| 红外 | 收发模块 |

### 显示屏

| 接口 | 支持芯片 | 像素格式 |
|------|---------|---------|
| SPI | ST7789、ST7305、ST7306 | RGB565 |
| RGB 并行 | ILI9488、GC9A01、ILI9341、ST7789 | RGB565/RGB888 |
| MCU 8080 | ST7796、ST7789 | RGB565 |
| QSPI | ST7735S | RGB565 |
| I2C | SSD1306 | 单色（128×32 / 128×64）|

### 触摸屏

| 芯片 | 接口 | 特性 |
|------|------|------|
| CST816x | I2C | 电容单点 |
| GT911 | I2C | 电容多点 |
| FT5x06 | I2C | 电容多点 |

### 音频编解码

| 型号 | 用途 |
|------|------|
| 平台内置音频 | T5AI、ESP32 片内 |
| ES8311 | 单声道编解码 |
| ES8388 | 立体声编解码 |
| ES8389 | 立体声编解码（升级版）|
| 无编解码 DAC | 直出 PWM/I2S |

### 摄像头

| 型号 | 接口 | 分辨率 |
|------|------|--------|
| OV2640 | DVP | 最高 200W |
| GC2145 | DVP | 最高 200W |

### 其他

| 外设 | 说明 |
|------|------|
| UART 透传 | 串口数据透传模块 |
| BMI270 | IMU 传感器（加速度计 + 陀螺仪） |
| IO 扩展芯片 | GPIO 扩展 |

---

## 参考链接

- LVGL 应用指南：https://www.tuyaopen.ai/zh/docs/peripheral/tutorials/lvgl-application-guide
- 显示驱动集成指南：https://www.tuyaopen.ai/zh/docs/peripheral/tutorials/display-driver-guide
- 显示驱动概述：https://www.tuyaopen.ai/zh/docs/peripheral/display
- 音频驱动：https://www.tuyaopen.ai/zh/docs/peripheral/audio
- 线程与定时器：https://www.tuyaopen.ai/zh/docs/peripheral/tutorials/thread-timer-patterns
- Wi-Fi Station：https://www.tuyaopen.ai/zh/docs/peripheral/tutorials/wifi-station-tutorial
- 内存管理：https://www.tuyaopen.ai/zh/docs/peripheral/memory/overview
- 外设支持列表：https://www.tuyaopen.ai/zh/docs/peripheral/support_peripheral_list
- TKL GPIO：https://www.tuyaopen.ai/zh/docs/tkl-api/tkl_gpio
- TKL UART：https://www.tuyaopen.ai/zh/docs/tkl-api/tkl_uart
- TKL I2C：https://www.tuyaopen.ai/zh/docs/tkl-api/tkl_i2c
- TKL SPI：https://www.tuyaopen.ai/zh/docs/tkl-api/tkl_spi
- TKL PWM：https://www.tuyaopen.ai/zh/docs/tkl-api/tkl_pwm
- TKL ADC：https://www.tuyaopen.ai/zh/docs/tkl-api/tkl_adc
