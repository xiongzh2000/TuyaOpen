# TuyaOpen tos.py 工具手册（AI Agent 用）

> 整理自 https://www.tuyaopen.ai/zh/docs/tos-tools/

---

## 概览

`tos.py` 是 TuyaOpen 的统一命令行工具，封装了编译、烧录、配置、调试全流程。使用前需先激活环境：

```bash
. ./export.sh   # 每次新开终端必须执行
```

---

## 完整命令列表

| 命令 | 功能 |
|------|------|
| `tos.py version` | 显示版本信息 |
| `tos.py check` | 校验依赖工具，下载/更新 submodule |
| `tos.py config choice` | 交互式选择预设板子配置 |
| `tos.py config menu` | 打开可视化 Kconfig 菜单 |
| `tos.py config save` | 保存当前配置到 `app_default.config` |
| `tos.py build` | 编译项目，生成固件 |
| `tos.py clean` | 清理 ninja 编译产物（保留 CMake 缓存）|
| `tos.py flash` | 烧录固件到设备 |
| `tos.py monitor` | 查看串口日志 |
| `tos.py update` | 同步依赖到指定 commit |
| `tos.py idf` | 透传命令给 ESP-IDF（仅 ESP32）|
| `tos.py dev bac` | 批量编译所有配置（CI 用）|
| `tos.py new` | 创建项目或平台模板 |

**全局选项**：`-d/--debug`（显示调试信息）、`-h/--help`

---

## config 命令

### 选择预设配置（推荐）

```bash
tos.py config choice
# 交互列出 config/ 目录下的 .config 文件，选择对应板子
```

### 可视化菜单配置

```bash
tos.py config menu
# 打开 menuconfig 界面，修改后保存
```

> ⚠️ 非 TTY 环境（CI、脚本）不能用 `config menu`，直接编辑 `app_default.config` 或 `cp config/XXX.config app_default.config`

### 保存配置

```bash
tos.py config save
# 把当前 using.config 中的差量保存回 app_default.config
```

---

## build 命令

```bash
# 在项目目录下执行
cd apps/tuya.ai/your_chat_bot
tos.py build

# 产物路径
.build/bin/{app_name}_QIO_{version}.bin   # 烧录固件
.build/lib/                               # 静态库
```

---

## clean 命令

```bash
tos.py clean          # 清理 ninja 编译产物（.o、.a、.bin），保留 CMake 缓存
rm -rf .build         # 完全清理（修改 app_default.config 后必须用这个）
tos.py clean -f       # 深度清理（等同于 rm -rf .build）
```

> ⚠️ **关键**：修改 `app_default.config` 或切换板子配置后，必须 `rm -rf .build` 而非 `tos.py clean`，否则 `using.config` 不会刷新，新配置不生效。

---

## flash 命令

```bash
tos.py flash                    # 自动检测串口，交互选择
tos.py flash -p /dev/ttyUSB0    # 指定串口
tos.py flash -b 921600          # 指定波特率（默认 921600）
```

**T5AI 双串口**：编号较小的是烧录口，较大的是日志口。

**底层实现**：`tos.py flash` 内部调用 `tyutool_cli`。

### 烧录工具 tyutool（GUI 版）

- 下载地址：https://github.com/tuya/tyutool/releases/tag/v3.0.5
- 支持 Windows / Linux / macOS
- GUI 版用于烧录和授权，功能与 `tos.py flash` 等价
- 报错 `flashSize is not defined` → 升级到 v3.0.5

**CH34x 驱动问题**：
- Windows：安装 WCH CH343 驱动
- macOS：安装驱动后需在系统偏好设置中允许安全设置

---

## monitor 命令

```bash
tos.py monitor                   # 默认波特率
tos.py monitor -p /dev/ttyUSB1   # 指定日志口
tos.py monitor -b 115200         # 指定波特率
```

按 `Ctrl + C` 退出。

---

## idf 命令（仅 ESP32）

将命令透传给 ESP-IDF 的 `idf.py`，在 TuyaOpen 工程目录下使用 ESP-IDF 原生工具：

```bash
tos.py idf menuconfig                    # ESP-IDF menuconfig
tos.py idf fullclean                     # ESP-IDF 完全清理
tos.py idf flash                         # ESP-IDF 烧录
tos.py idf --idf-flags="-v" build        # 透传额外参数（-v 详细输出）
tos.py idf --idf-flags="-D MY_MACRO=1" build
```

**前置条件**：项目已配置为 ESP32 平台（`tos.py config choice` 选择了 ESP32 配置）

---

## dev 命令（批量编译）

```bash
# 编译当前项目下所有 config/*.config，输出到 dist/
tos.py dev bac -d ./dist

# 同时保存编译日志
tos.py dev bac -d ./dist -o ./logs
```

CI 流水线用于验证所有板子配置均能编译通过。

---

## 常见问题速查

| 问题 | 解决方法 |
|------|---------|
| `tos.py: command not found` | 未执行 `. ./export.sh`，重新激活 |
| 配置改了但编译没变化 | `rm -rf .build` 后重新编译 |
| `flashSize is not defined` | 升级 tyutool 到 v3.0.5 |
| 烧录失败，CH34x 驱动问题 | 安装 WCH CH343 驱动 |
| `check` 失败，submodule 未初始化 | `git submodule update --init` |
| ESP32 配置无法用 `config menu` | 检查终端是否为 TTY，或用 `tos.py idf menuconfig` |
