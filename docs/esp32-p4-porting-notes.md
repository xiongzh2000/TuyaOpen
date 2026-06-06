# ESP32-P4 (P4 + C6) 适配踩坑记录

记录在 TuyaOpen 上给打印机项目（`apps/tuya.ai/smart_print`）适配
**ESP32-P4-C6 / Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3** 的全过程问题与解法。

> 硬件：ESP32-P4（主控，无 WiFi/BT 射频）+ ESP32-C6（WiFi/BT 协处理器），
> 4.3" 480×800 ST7701 MIPI-DSI 屏，GT911 电容触摸，ES8311 喇叭，ES7210 麦克风。

---

## 0. 最重要的一条经验

**ESP32-P4 必须用官方 platform 的 `esp_hosted` 方案，不要自己拼 `CONFIG_ESP_HOST_WIFI_ENABLED`。**

P4 本身没有 WiFi/BT 射频，射频全在外挂的 C6 上。早期我们用
`CONFIG_ESP_HOST_WIFI_ENABLED`（esp_wifi 的 host 库）把 WiFi"拼"起来，
结果是 **host 侧所有 API 都返回成功（init/scan/SoftAP 全"OK"），但命令根本
没送到 C6 射频**——热点广播不出去、手机扫不到、MAC 全 0。这是"空跑"。

官方 ESP32 platform（`tuya/TuyaOpen-esp32` 的 `0422216` 及以后）用的是：
```yaml
espressif/esp_wifi_remote: 0.14.*   # target == esp32p4
espressif/esp_hosted:      1.4.*    # target == esp32p4
```
+ `sdkconfig_esp32p4_c6` 变体，C6 跑 esp_hosted slave 固件。
切到这套之后 WiFi/BT 才真正通过 C6 工作。

**所以适配 P4 第一步：确认 platform 仓库是带 esp_hosted 的版本（commit 0422216+），
platform_config.yaml 指过去。** 后面大量自己写的 WiFi/BT/MAC hack 都是在错误
方案上打补丁，切到官方方案后全部删掉了。

---

## 1. 编译/工具链

| 问题 | 现象 | 解法 |
|------|------|------|
| 缺 P4 工具链分支 | `libtuyaos.a` 编出来是 arm64（host cc），链接报 `tuya_app_main` undefined / `file format not recognized` | `platform/ESP32/toolchain_file.cmake` 加 `esp32p4` 分支，新建 `tools/esp32p4/toolchain_esp32p4.cmake`，用 `riscv32-esp-elf-gcc` |
| float ABI 不匹配 | 链接 `cannot move location counter backwards` / `failed to merge target specific data` | P4 工具链 CFLAGS 用 `-march=rv32imafc_zicsr_zifencei -mabi=ilp32f`（P4 有 F 扩展，单精度浮点 ABI），不能用 soft-float |
| Xtensa 汇编 | `tkl_flash.c` 内联汇编 `mov a5,a1` 在 RISC-V 报 `unrecognized opcode` | `#if defined(__riscv)` 用 `mv %0, sp`，否则用原 Xtensa `mov %0, a1` |
| PSRAM 地址宏 | `tkl_flash.c` `SOC_EXTRAM_DATA_LOW` undeclared | P4 用 `SOC_EXTRAM_LOW`/`SOC_EXTRAM_HIGH`，加 `#elif defined(SOC_EXTRAM_LOW)` 分支 |
| 32M Flash 分区 | `set_partitions` 只支持到 16M | `build_example.py` 加 `CONFIG_PLATFORM_FLASHSIZE_32M` 分支 + `partitions_32M.csv` |

> ⚠️ 上面这些 `tkl_flash.c` / `build_example.py` 的 hack 是在**旧 platform** 上做的。
> 切到官方 0422216 后，platform 层这些大多已由官方处理，不需要了。保留记录是为了
> 理解问题，不是让你再改一遍。

---

## 2. 屏幕 ST7701 MIPI-DSI

### 2.1 屏幕芯片认错（最大的坑）

最初按某些资料以为是 **ILI9881C**，手写 DCS 初始化命令通过 `esp_lcd_panel_io_tx_param`
逐条发，结果**发到第 20 条就卡死**（DSI DBI FIFO 满，且芯片不对根本不应答）。

**真相：屏幕是 ST7701**（对照 Waveshare 官方 demo 才发现）。解法：
- 引入 `espressif/esp_lcd_st7701` 组件，用 `esp_lcd_new_panel_st7701()` 让组件管理初始化
- 用官方 demo 的 ST7701 初始化命令序列（`0x77,0x01` 开头，不是 ILI9881C 的 `0x98,0x81`）
- 结构体 `st7701_lcd_init_cmd_t` 字段是 `{int cmd; const void *data; size_t data_bytes; unsigned int delay_ms}`，
  和我们自定义的 `LCD_MIPI_DSI_INIT_CMD_T`（uint8_t/uint8_t）**二进制不兼容**，强转会内存错位崩溃，
  必须直接用组件的类型

### 2.2 DSI 参数（来自官方 demo）

| 参数 | 错误值（猜的） | 正确值（官方） |
|------|------|------|
| lane_bit_rate | 1000 Mbps | **500 Mbps** |
| dpi_clock | 80 MHz | **30 MHz** |
| hsync back/pulse/front | 140/40/40 | **42/12/42** |
| vsync back/pulse/front | 16/4/16 | **2/8/60** |

### 2.3 DPI underrun —— 屏幕下半蓝屏

现象：屏幕上半正常、下半蓝色，刷屏 `lcd.dsi.dpi: can't fetch data from external
memory fast enough, underrun happens`。

根因：**PSRAM 速度只有 20MHz，带宽喂不饱 DPI 取帧缓冲**。

解法：`sdkconfig_esp32p4_c6` 里把 PSRAM 提到 200MHz：
```
CONFIG_IDF_EXPERIMENTAL_FEATURES=y   # 200M 依赖这个
CONFIG_SPIRAM_SPEED_200M=y
```
> 注意：切到官方 esp_hosted 后 WiFi 真正占用 PSRAM 带宽，比"空跑"时竞争更大，
> 200MHz 是必须的。

### 2.4 头文件依赖

`esp_lcd_st7701.h` 在 IDF managed_components 里，**上层 TuyaOpen CMake 编 board 文件时
找不到**。解法：board 层（`board_com_api.c`）用一个二进制兼容的 `st7701_init_cmd_t`
本地结构体定义初始化命令数组，避免直接 include 组件头文件；只在 platform 层
（tuyaos_adapter，能拿到 managed_components include）真正 include `esp_lcd_st7701.h`。

组件依赖要在两个地方都加：
- `tuya_open_sdk/main/idf_component.yml`：`espressif/esp_lcd_st7701` + `espressif/esp_lcd_touch_gt911`
- `tuyaos_adapter/CMakeLists.txt` 的 `REQUIRES`

---

## 3. 触摸 GT911

### 3.1 间歇性 read_cfg NACK（时好时坏）

现象：`GT911 detected at 0x5D`（probe 成功）后立刻 `read_cfg NACK / GT911 init failed`，
而且**重启有时成功有时失败**。

根因：`esp_lcd_touch_new_i2c_gt911()` 内部会先**硬 reset GT911（拉 RST/GPIO23）再读配置**。
GT911 在 reset 时会**从 INT 引脚电平重新锁存 I2C 地址**（0x5D 或 0x14），reset 后地址漂移，
而 panel_io 已绑定 probe 时的 0x5D → read_cfg NACK。

解法（两步）：
1. 初始化前先 `i2c_master_probe` 探测 0x5D/0x14 两个地址 + 重试（参考官方 BSP），
   确认设备就绪并选对地址
2. **probe 成功后把 `tp_cfg.rst_gpio_num = GPIO_NUM_NC`**——既然设备已就绪在该地址，
   就别让组件再 reset（reset 才是地址漂移的元凶）。POR 已经复位过了，不需要二次 reset。

代码在 `boards/ESP32/common/tp/tdd_tp_esp_gt911.c`。

---

## 4. 音频 ES8311 + ES7210

### 4.1 ES8311 I2C 地址错误（一直 NACK / assert 崩溃）

现象：`I2C_If: Fail to write to dev 18` 反复，最终 `codec_8311_init assert (codec_if_ != NULL)` 崩溃重启。

根因：board 里写死 `es8311_addr = 0x18`（7 位地址），但 **esp_codec_dev 的
`.addr` 字段用 8 位左对齐格式**（`0x18 << 1 = 0x30`）。其他能工作的 S3 板和官方
demo 都用 `0x30`。

解法：`es8311_addr = 0x30`。

### 4.2 麦克风采到静音 —— ASR 识别为空（最隐蔽）

现象：按对话按钮，录音/上传链路全正常（日志一堆 `upload stream len:2560`），
但云端 `ASR text:` 是空的、`invalid text data ... 0`。

根因：**Waveshare P4 板的麦克风接在独立的 ES7210 ADC（I2C 0x80）上，不是 ES8311**。
官方 demo 是 ES8311 做喇叭、ES7210 做麦克风两颗芯片。我们的 `tdd_audio_8311_codec`
拿 ES8311 自己的 ADC 当麦克风，那个 ADC 根本没接麦克风 → 全静音 → ASR 无内容。

解法：`tdd_audio_8311_codec.c` 里，P4 上把录音输入设备改成 ES7210：
```c
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    // 麦克风在独立 ES7210 (I2C 0x80)，ES8311 退为纯喇叭
    es7210_codec_cfg_t es7210_cfg = { .ctrl_if = es7210_ctrl_if,
                                      .mic_selected = ES7120_SEL_MIC1 | ES7120_SEL_MIC2 };
    input_dev_ = esp_codec_dev_new(&(esp_codec_dev_cfg_t){
        .dev_type = ESP_CODEC_DEV_TYPE_IN, .codec_if = es7210_codec_new(&es7210_cfg), ... });
#else
    // 其他芯片仍用 ES8311 做麦克风
#endif
```
ES7210 和 ES8311 共享同一条 I2C bus（不同地址）和同一路 I2S（duplex）。

### 4.3 I2S GPIO

官方 `tkl_i2s.c` 只有 ESP32-S3 的 `sg_i2s_gpio_cfg`，P4 编译报
`sg_i2s_gpio_cfg undeclared`。加 P4 分支：bclk=12 / ws=10 / data=11。

### 4.4 audio_codecs 组件门控

`boards/ESP32/common/CMakeLists.txt` 用 `CONFIG_ENABLE_AUDIO` 门控 `common/audio/*.c`，
但这些 ES8311/ES7210 codec 驱动实际依赖 **audio_codecs 组件**（`tdl_audio_driver.h`），
对应 `CONFIG_ENABLE_AUDIO_CODECS`。无 codec 栈的 app（如 switch_demo）`ENABLE_AUDIO=y`
但 `ENABLE_AUDIO_CODECS=n`，编译就缺头文件。门控改用 `CONFIG_ENABLE_AUDIO_CODECS`。

---

## 5. WiFi / 蓝牙 / 配网

### 5.1 WiFi PM 空指针崩溃

现象：`pm_set_sleep_type` 跳 NULL（MEPC=0）崩溃。

根因：`tkl_wifi_set_lp_mode` → `esp_wifi_set_ps()` 在 P4 远程 WiFi 下解引用了未实现的
pm 回调。（这是**旧 ESP_HOST_WIFI 方案**下的问题，切到 esp_hosted 后不存在了。）

### 5.2 蓝牙 init 失败 + 配网搜不到设备

旧方案下 P4 没有可用蓝牙，`ENABLE_BLUETOOTH` 默认开 → BLE init 失败
（`tuya ble init failed -28678`）→ 涂鸦 App 默认走蓝牙扫描发现设备 → 搜不到。

**切到官方 esp_hosted 后，P4 的蓝牙通过 C6 真正工作**（PR #80 用 `--wrap` 让
tkl_bt.c 和 esp_hosted 的 hci_rx_handler 共存），蓝牙配网/自动发现可用，
不再需要禁用蓝牙。

### 5.3 配网方式

- 涂鸦 App "自动发现设备" 靠的是 **BLE 广播**。P4 蓝牙能用（esp_hosted）后才能自动发现。
- WiFi AP 配网（SoftAP 热点 `SmartLife-xxxx`）走的是 `NETCFG_TUYA_WIFI_AP`，
  需要 SoftAP 真正在 C6 上广播（esp_hosted 通了才行）。
- MAC 全 0（热点叫 `SmartLife-0000`）也是 esp_hosted 没通的症状之一；通了之后
  MAC 正常（从 C6 同步），热点名变 `SmartLife-XXXX`。

---

## 6. App 层（与 P4 无关，但适配中暴露）

### 6.1 屏幕对话按钮无反应

现象：点屏幕 talk 按钮，日志有 `talk_btn: PRESSED/RELEASED`，但不录音。

根因：`smart_print/src/app_ui_action.c` **漏了 `AI_UI_ACT_TALK_KEY` 的 case**
（只有 `pai_xue_ji`/`smart_badge` 实现了）。UI 按钮把事件抛出来没人接。

解法：补 case，转发给 `ai_mode_handle_key(key_evt, NULL)`，喂给对话模式（press-to-talk）。

### 6.2 缺物理按键导致崩溃

`ai_chat` mode 0 注册物理按键（`ai_chat_button`）失败（board 没注册）。早期在
ES8311 崩溃修好后暴露出 button 缺失导致的二次崩溃。注意 board 的
`ENABLE_BUTTON` / `CONFIG_BUTTON_NAME` 与 app 期望一致。

### 6.3 条件编译语法 bug

`ai_ui_wechat_chat.c` 有一处 `} && (ENABLE_COMP_AI_VIDEO == 1)` 的笔误，
应为 `#endif` + `#if`。

---

## 7. 长期维护提醒

- platform 层（`platform/ESP32`）的 P4 board 适配（GT911/ST7701 组件声明、
  PSRAM 200M、I2S GPIO）目前是官方 commit 上的本地 patch，**每次官方更新 platform 会丢**。
  已提 PR 给 `tuya/TuyaOpen-esp32`（分支 `esp32p4-board-patch`），合并后
  把 `platform_config.yaml` 指向新 commit 即可。
- 本地若需临时固定 platform 版本不被 tos 提示更新：
  `mkdir -p .cache && touch .cache/.dont_prompt_update_platform`
- board 层硬件驱动（`boards/ESP32/ESP32-P4-C6/board_com_api.c`、`common/lcd|tp|audio`）
  在主仓库 dev 分支，不受 platform 更新影响。

---

## 8. 最终验证通过的功能

- ✅ 编译/烧录（RISC-V rv32imafc + ilp32f）
- ✅ 屏幕 ST7701 MIPI-DSI 480×800 全屏显示，无 DPI underrun
- ✅ GT911 触摸（稳定，不再间歇 NACK）
- ✅ ES8311 喇叭 + ES7210 麦克风，WakeNet 唤醒 + 按键对话 ASR 识别
- ✅ WiFi 配网（esp_hosted → C6），连云、MQTT
- ✅ 蓝牙（esp_hosted，可自动发现配网）
