# TuyaOpen 常见问题手册（AI Agent 用）

> 整理自 https://www.tuyaopen.ai/zh/docs/faqs

---

## 目录

1. [环境搭建与激活](#环境搭建与激活)
2. [编译问题](#编译问题)
3. [硬件与设备重置](#硬件与设备重置)
4. [配置与 Kconfig](#配置与-kconfig)
5. [授权码（UUID / AuthKey）](#授权码uuid--authkey)
6. [烧录与设备启动](#烧录与设备启动)
7. [网络与云连接](#网络与云连接)
8. [外设与功能](#外设与功能)
9. [涂鸦云配网（Smart Life App）](#涂鸦云配网smart-life-app)
10. [项目创建与结构](#项目创建与结构)
11. [Git 与子模块](#git-与子模块)
12. [Linux 运行时](#linux-运行时)

---

## 环境搭建与激活

**Q：如何激活 `tos.py`？为什么每次开终端都要重新激活？**

每次新开终端必须重新激活，它配置 Python 虚拟环境和 PATH：

```bash
# Linux / macOS
. ./export.sh

# Windows PowerShell
.\export.ps1
# 若报错执行权限：Set-ExecutionPolicy RemoteSigned -Scope LocalMachine

# Windows CMD
.\export.bat
```

---

**Q：激活失败怎么办？**

```bash
# Linux：安装缺少的 venv 工具
sudo apt-get install python3-venv

# 删除损坏的虚拟环境后重试
rm -rf ./.venv
. ./export.sh
```

- 确认 Python ≥ 3.8
- Windows 只能用 CMD 或 PowerShell（不能用 Git Bash / MSYS2）

---

**Q：Windows 为什么不能用 Git Bash 或 MSYS2？**

构建脚本依赖 Windows 原生路径处理，Git Bash / MSYS2 的 Linux 风格路径会导致兼容问题。只能用 CMD 或 PowerShell。

---

**Q：系统最低要求？**

| 项目 | 要求 |
|------|------|
| OS | Windows 10/11、Ubuntu 20.04/22.04/24.04 LTS、macOS |
| Git | ≥ 2.0.0 |
| CMake | ≥ 3.28.0 |
| Make | ≥ 3.0.0 |
| Ninja | ≥ 1.6.0 |
| Python | ≥ 3.8.0 |
| Linux 额外包 | `build-essential ninja-build cmake-curses-gui python3-pip python3-venv` |

---

**Q：怎么验证环境是否正确？**

```bash
tos.py version   # 显示版本号（fork 的仓库显示 [Unknown version] 属正常）
tos.py check     # 校验工具链，自动下载 submodule
```

---

## 编译问题

**Q：编译失败，提示缺少依赖？**

```bash
tos.py check   # 自动安装缺少的依赖
```

确认 Python 环境和 PATH 配置正确。

---

**Q：`tos.py check` 失败怎么办？**

| 原因 | 解决 |
|------|------|
| 工具版本太低 | 升级 git/cmake/make/ninja 到最低版本要求 |
| submodule 下载失败 | `git submodule update --init` |
| Python venv 问题 | `rm -rf ./.venv && . ./export.sh`，并确认 `python3-venv` 已安装 |

---

**Q：构建系统检测不到开发板？**

```bash
tos.py config choice   # 交互式选择目标板子
```

- 确认 `{项目根目录}/config/` 下存在对应的 `.config` 文件
- 不是所有应用都支持所有板子

---

**Q：什么时候需要创建新的 board BSP？**

自定义硬件/PCB 与现有板子不匹配时：

```bash
tos.py new board   # 生成 BSP 目录结构和配置文件
```

---

**Q：Windows 编译速度极慢或卡住？**

打开任务管理器（Ctrl+Shift+Esc），找到并结束 `MSPCManagerService` 进程。若无效，将 TuyaOpen 目录移至非系统盘（如 D:），并将该目录加入 Windows Security 排除列表。

---

**Q：修改配置后编译没有变化？**

`tos.py clean` 只清理 ninja 产物，不刷新 CMake/Kconfig 缓存。修改 `app_default.config` 后必须：

```bash
rm -rf .build    # 完全清理
tos.py build     # 重新全量编译
```

验证配置是否生效：检查 `.build/cache/using.config` 中对应项是否更新。

---

## 硬件与设备重置

**Q：如何重置配网信息？**

- **MCU 设备**：5 秒内快速重启 3 次，第 4 次启动时自动清除网络和配对状态
- **Linux 运行时**：手动删除 `tuyadb` 文件夹（存储网络和配对信息的 KV 缓存）

---

## 配置与 Kconfig

**Q：`config choice` 和 `config menu` 的区别？**

| 命令 | 用途 |
|------|------|
| `tos.py config choice` | 从预验证配置列表中选择（推荐）|
| `tos.py config menu` | 打开完整 menuconfig 界面手动配置（高级）|

两者执行前都会做深度清理（可能切换工具链）。

---

**Q：menuconfig 中某些选项无法勾选/取消？**

该选项被板子 Kconfig 中的 `select` 语句强制开启，例如：

```kconfig
# boards/T5AI/TUYA_T5AI_EVB/Kconfig
config ENABLE_WIFI
    bool
    select ENABLE_LIBLVGL   # 强制选中，无法手动关闭
```

要修改，需直接编辑板子的 Kconfig 文件。

---

**Q：如何保存自定义配置以便复用？**

```bash
tos.py config menu          # 修改配置
tos.py config save          # 保存，输入名称如 my_custom_board.config
# 保存到 config/ 目录后，以后用 tos.py config choice 选择
```

---

**Q：Windows 上 `config menu` 方向键不能用？**

用替代键：**h**（左）、**j**（下）、**k**（上）、**l**（右），空格切换，Enter 确认。或换用 CMD 和 PowerShell 互切。

---

## 授权码（UUID / AuthKey）

**Q：TuyaOpen 授权码和 TuyaOS 授权码有什么区别？**

**完全不通用**。TuyaOpen 的 UUID/AuthKey 只用于 TuyaOpen 框架，TuyaOS 的不能用于 TuyaOpen 项目，会导致云连接失败。

---

**Q：如何免费获取 TuyaOpen 授权码？**

每个开发者账号可免费领取 **2 个**授权码（每个价值 ¥20）：

1. 登录 [platform.tuya.com](https://platform.tuya.com/)
2. 创建产品（品类随意，后续可改）
3. 选择 T5 模组，上传任意占位固件
4. 在产品页面点击"免费领取 2 个授权码"
5. 在授权码清单中确认，获取 UUID 和 AuthKey

> ⚠️ 同一 UUID 同时只能有一台设备在线连接涂鸦云。切换设备前先在 App 中解除原设备配对。

其他获取方式：购买预烧录了 TuyaOpen 授权码的模组；涂鸦官方淘宝店购买。

---

**Q：如何将授权码写入设备？**

**方式一：串口命令行（推荐，无需重新编译）**

```bash
tos.py monitor -b 115200
# 在提示符下输入：
auth uuidxxxxxxxxxxxxxxxx keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

**方式二：固化到固件（仅适合单台设备测试）**

```c
// include/tuya_config.h
#define TUYA_OPENSDK_UUID    "uuidxxxxxxxxxxxxxxxx"
#define TUYA_OPENSDK_AUTHKEY "keyxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
```

修改后重新编译烧录。

**方式三：tyutool GUI 工具**  
下载地址：https://github.com/tuya/tyutool/releases/tag/v3.0.5

**方式四：TuyaOpen 串口 Web 工具**  
基于 Chrome 浏览器，无需安装。

**方式五：文件系统（Linux / 树莓派）**  
通过文件方式写入持久化存储。

> ⚠️ 写授权码用烧录口，不是日志口。

---

**Q：如何验证授权码写入正确？**

```bash
tos.py monitor
# 在提示符下输入：
auth-read
# 正确：显示完整 UUID 和 AuthKey
# 错误：显示 xxxxxxxxxxxxxxxx，需重新写入
```

---

**Q：设备日志出现"authorization read failure"？**

设备无法从存储中读取授权码，可能未写入或已损坏。按上述方式重新写入并重启设备。

---

## 烧录与设备启动

**Q：烧录失败或设备未被识别？**

| 检查项 | 操作 |
|-------|------|
| 确认数据线 | 使用支持数据传输的 USB 线（非纯充电线）|
| 安装串口驱动 | Windows：安装 CH340/CP210x 驱动；macOS：CH34x 驱动 |
| 换端口/线缆 | 换另一个 USB 口或换线 |
| 手动指定串口 | `tos.py flash --port /dev/ttyUSB0` |
| 确认连接 | Linux：`lsusb`；Windows：设备管理器 |

---

**Q：提示 "port [xxx] may be busy"？**

等约 1 分钟后重试。T5 系列在虚拟机中有已知延迟（串口出现但不能立即使用）。用 `ls /dev/tty*` 确认端口存在后等待一会再烧录。

---

**Q：T5 板子出现两个串口，用哪个？**

| 串口 | 用途 |
|------|------|
| 编号较小（Linux：ttyUSB0；Windows：端口名含字母 A）| 烧录口（烧固件和授权）|
| 编号较大（Linux：ttyUSB1；Windows：端口名含字母 B）| 日志口（monitor 查看日志）|

---

**Q：Linux 串口权限报错？**

```bash
sudo usermod -aG dialout $USER
# 必须重启系统才能生效
```

---

**Q：`flashSize is not defined` 报错？**

烧录工具兼容性问题，升级 tyutool 到 v3.0.5：  
https://github.com/tuya/tyutool/releases/tag/v3.0.5

---

**Q：tyutool_gui 被 Windows 报告为病毒？**

Windows Defender 误报。将工具放到非系统盘（如 D:），将该目录加入 Windows Security 排除列表即可。

---

**Q：设备无法启动？**

- 检查 USB 线和端口
- 确认供电正常（换端口或适配器）
- 排查短路
- 重新烧录固件
- 用 `tos.py monitor` 查看启动时的串口输出找错误

---

## 网络与云连接

**Q：设备无法连接 Wi-Fi？**

- **仅支持 2.4GHz**，5GHz 不可用
- 确认 SSID 和密码正确
- 将设备靠近路由器
- 检查路由器是否开启 MAC 地址过滤
- 重置设备并重新配网

---

**Q：无法连接涂鸦云？**

- 确认使用 TuyaOpen 专属 UUID/AuthKey（不是 TuyaOS 的）
- 确认设备有网络并能访问公网
- 查看设备日志中的连接错误信息
- 重置并重新配网

---

## 外设与功能

**Q：屏幕无显示？**

- 确认选择了支持显示屏的板子配置
- 在 `tos.py config menu` 中确认 LVGL 和显示驱动已启用
- 查看串口日志中的显示初始化消息
- 确认当前示例应用支持屏幕输出

---

**Q：AI Agent 无响应？**

1. 检查设备网络连接
2. 查看串口日志中的 AI 服务错误
3. 确认麦克风已连接且正常
4. 确认唤醒词检测已启用
5. 确认设备已正确配网并连接涂鸦云
6. 检查项目配置中 AI Agent 服务是否已启用

---

**Q：如何启用音频编解码驱动（麦克风/扬声器）？**

```bash
tos.py config menu   # 进入音频编解码配置区域
# 找到并启用对应板子的音频编解码器
```

- 先在板子文档和 Kconfig 中确认硬件支持
- 如果板子 Kconfig 含 `select ENABLE_AUDIO_CODECS` 则已预配置
- 如无预注册的音频设备，需自行实现音频驱动桥接
- 修改配置后重新编译

---

**Q：外设驱动（按键、显示等）不工作？**

1. 在 Kconfig 中确认外设已启用（`tos.py config menu`）
2. 检查 `src/peripherals/<外设>/Kconfig` 和 `boards/<平台>/<板子>/Kconfig` 中的配置
3. 确认板子硬件初始化代码中已注册该外设（`board_register_hardware()`）
4. 查看串口日志中的驱动初始化错误
5. 确认硬件连接和引脚配置与板子文档一致

---

## 涂鸦云配网（Smart Life App）

**Q：App 检测不到设备？**

- 确认 UUID 和 AuthKey 正确配置（用 `auth-read` 验证）
- 重置网络配置：MCU 设备 5 秒内重启 3 次；Linux 删除 `tuyadb` 文件夹

---

**Q：如何让设备进入配网模式？**

大多数 TuyaOpen 示例（switch_demo、your_chat_bot 等）：**5 秒内快速重启 3 次**，第 4 次启动进入配网模式。

查看串口日志中的 `STATE_START` 或 `TUYA_EVENT_BIND_START` 确认进入配网。

---

**Q：配网时无法连接 Wi-Fi？**

- 确认使用 **2.4GHz** 网络（MCU 不支持 5GHz）
- 确认 Wi-Fi 密码正确
- 将设备靠近路由器
- Linux：用 `ipconfig` 等工具确认网络接口正常

---

**Q：设备在 App 中出现但配对后无响应？**

- 查看串口日志找错误信息
- 确认云连接状态（日志中有 `MQTT Connected`）
- 确认固件中实现了对应的 DP（数据点）命令处理
- 确认 `tuya_iot_yield()` 循环正在运行

---

**Q：配对失败，提示授权错误？**

- 用 `auth-read` 命令验证 UUID/AuthKey 是否正确写入
- 确认使用 TuyaOpen 专属授权码（不是 TuyaOS 的）
- 日志中看到 `xxxxxxxxxxxxxxxx` 说明未写入成功，重新写入后重启

---

## 项目创建与结构

**Q：如何创建新项目？**

```bash
tos.py new project                          # 默认 base 框架
tos.py new project --framework arduino      # Arduino 框架

cd <新项目目录>
tos.py config choice    # 选择板子
tos.py build
```

模板来自 `tools/app_template/`。

---

**Q：`tos.py` 命令应该在哪个目录运行？**

**必须在应用项目目录下运行**（含 `CMakeLists.txt` 和 `app_default.config` 的目录）：

```bash
# ✅ 正确
cd apps/tuya_cloud/switch_demo
tos.py build

# ❌ 错误（从 TuyaOpen 根目录运行会报错）
tos.py build
```

---

**Q：`apps/` 和 `examples/` 的区别？**

| 目录 | 内容 |
|------|------|
| `apps/` | 完整功能的应用（switch_demo、your_chat_bot 等）|
| `examples/` | 演示单一功能或 API 的小型代码示例 |

两者都可用 `tos.py build` 编译。

---

**Q：如何清理编译产物？**

```bash
tos.py clean        # 标准清理：删除 ninja 产物，保留配置缓存
tos.py clean -f     # 深度清理：等同于 rm -rf .build
```

切换板子配置或遇到奇怪编译问题时用 `clean -f`。

---

## Git 与子模块

**Q：如何更新 TuyaOpen 依赖？**

```bash
git pull                 # 更新主仓库
tos.py update            # 按 platform/platform_config.yaml 更新平台依赖到锁定 commit
```

`tos.py update` 等同于切换到 `platform_config.yaml` 中指定的 commit。

---

## Linux 运行时

**Q：支持哪些 Linux 平台和架构？**

x86（32位）、x64（64位）、ARM（32/64位，如树莓派）。可能需要配置目标硬件的交叉编译工具链。

---

**Q：Linux 上如何实现 GPIO、SPI、I2C、PWM 等硬件支持？**

Linux 通过内核和 BSP 管理硬件，与 MCU 直接操作寄存器不同。需要：

1. 确保硬件通过 Linux 设备驱动或 sysfs 可访问
2. 编写桥接代码，将 TuyaOpen 驱动接口映射到 Linux 接口
3. 可借助 `wiringPi`、`libgpiod` 或内核 `/dev/*` 节点

---

**Q：TuyaOpen 是否提供 Linux 硬件抽象？**

框架提供云连接、设备配网、AI Agent 的抽象。GPIO、SPI、PWM、摄像头等**硬件外设支持需开发者自行实现**。

---

**Q：如何交叉编译 TuyaOpen 到 Linux 平台？**

```bash
# ARM 交叉编译示例
export CROSS_COMPILE=arm-linux-gnueabihf-
cmake -DCMAKE_TOOLCHAIN_FILE=... -DSYSROOT=...
```

修改 CMake 配置指定目标架构和 sysroot，在硬件上测试并排查动态库依赖问题。

---

**Q：Linux 设备文件未枚举或无法打开？**

```bash
dmesg          # 查看内核消息
lsmod          # 查看加载的内核模块
ls /dev/spidev*  /dev/video*  /sys/class/gpio/   # 确认设备文件存在
```

检查权限：以足够权限运行，或调整 udev 规则。

---

**Q：Linux 上能使用涂鸦云和 AI Agent 功能吗？**

可以。框架的云连接和 AI Agent 是跨平台的。只要实现了硬件桥接，设备逻辑、配网、云集成、OTA、AI 功能与 MCU 部署方式一致。

---

## 快速问题速查表

| 现象 | 解决方向 |
|------|---------|
| `tos.py: command not found` | 未执行 `. ./export.sh`，重新激活 |
| 配置改了编译没变化 | `rm -rf .build` 后重编 |
| `flashSize is not defined` | 升级 tyutool 到 v3.0.5 |
| 烧录 CH34x 驱动问题 | 安装 WCH CH343 驱动 |
| `check` 失败 submodule 未初始化 | `git submodule update --init` |
| `auth-read` 显示 `xxxxxxxx` | 授权码未写入，重新写入 |
| 设备配网失败 | 确认 2.4GHz Wi-Fi，确认 UUID/AuthKey 为 TuyaOpen 专属 |
| menuconfig 选项无法修改 | 被 `select` 强制开启，修改板子 Kconfig |
| Windows 编译极慢 | 结束 `MSPCManagerService` 进程 |
| Linux 串口权限报错 | `sudo usermod -aG dialout $USER` 后重启 |
| tyutool 被报毒 | 放到非系统盘，加入 Windows Security 排除列表 |
| 设备有两个串口不知用哪个 | 编号小的烧录，编号大的看日志 |
