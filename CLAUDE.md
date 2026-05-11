# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

TuyaOpen is a cross-platform IoT SDK written in C/C++. It targets Tuya T-series MCUs (T2, T3, T5AI), ESP32, BK7231X, LN882H, and Linux/Ubuntu. The LINUX target compiles and runs natively on the host, making it the go-to for development and testing without hardware.

## Communication

Always respond in Simplified Chinese unless explicitly asked otherwise.

## Environment Setup

```bash
cd /path/to/TuyaOpen && . ./export.sh
```

This creates `.venv/`, installs Python deps, and exports `OPEN_SDK_ROOT`, `OPEN_SDK_PYTHON`, `OPEN_SDK_PIP`. Required before any `tos.py` commands.

## Build Commands

```bash
# Verify tools and submodules
tos.py check

# Build a project (run from its directory)
cd examples/get-started/sample_project
tos.py build

# Clean
tos.py clean

# Build all platforms for CI validation
tos.py dev bac -d ./dist
```

Build outputs: `<project>/.build/` (intermediates), `<project>/dist/` (final binaries).

Platform SDKs are fetched on first build per `platform/platform_config.yaml` and cached under `platform/<NAME>/`.

## Configuration

Projects use Kconfig. Each project has an `app_default.config` specifying build options (board choice, LWIP tuning, feature flags). To change config non-interactively, edit `app_default.config` directly -- avoid `tos.py config choice`/`config menu` in non-TTY environments.

To suppress platform update prompts in automation:
```bash
mkdir -p .cache && touch .cache/.dont_prompt_update_platform
```

## Formatting and Linting

Code style is enforced by clang-format (`.clang-format` at root, Linux brace style, 4-space indent, 120 column limit).

```bash
# Check specific files
python tools/check_format.py --debug --files <file>

# Check a directory
python tools/check_format.py --debug --dir <dir>

# PR-style check against a base branch
python tools/check_format.py --base <branch>
```

Third-party code listed in `.clang-format-ignore` is excluded (cJSON, FlashDB, littlefs, lwip, coreHTTP, coreMQTT, etc.).

## Architecture

### Layered Component Model

```
apps/examples          Application layer (user projects)
       |
    src/               SDK components (linked into libtuyaos.a)
       |
  platform/<TARGET>/   Hardware abstraction (toolchain, BSP, build scripts)
  boards/<TARGET>/     Board-specific config and overlays
```

**TAL (Tuya Abstraction Layer)** -- `src/tal_*` directories provide OS-agnostic APIs for system primitives (thread, mutex, semaphore, timer, memory, log, OTA, filesystem, sleep), networking (WiFi, BLE, wired, cellular), drivers, and security. Each TAL component has `include/` (public headers) and `src/` (implementation). Platform ports implement the underlying OSAL.

**Cloud/AI Services** -- `src/tuya_cloud_service/` handles Tuya Cloud connectivity (MQTT transport, TLS, device authorization, schema, LAN, OTA file storage, weather). `src/tuya_ai_service/` provides AI agent integration, codec, and monitoring.

**Libraries** -- `src/lib*` wraps third-party libraries: cJSON, MQTT (coreMQTT), HTTP (coreHTTP), TLS (mbedTLS), LWIP, LVGL (v8/v9), u8g2, libjpeg-turbo.

### CMake Build System

The top-level `CMakeLists.txt` auto-discovers components: every subdirectory of `src/` with a `CMakeLists.txt` is added. Components register themselves into `COMPONENT_LIBS` and `COMPONENT_PUBINC` lists. All component object files are merged into a single static library `libtuyaos.a`. The application project (from `examples/` or `apps/`) links as `libtuyaapp.a` against `libtuyaos.a`.

Platform-specific toolchain is loaded from `platform/<TARGET>/toolchain_file.cmake`, and the platform build script (`build_example.sh` or `build_example.py`) produces the final binary.

### Component CMakeLists Pattern

```cmake
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)
aux_source_directory(${MODULE_PATH}/src LIB_SRCS)   # or file(GLOB_RECURSE ...)
set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

add_library(${MODULE_NAME})
target_sources(${MODULE_NAME} PRIVATE ${LIB_SRCS})

# Register into parent scope
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
```

Components gated by Kconfig wrap their body in `if (CONFIG_ENABLE_XXX STREQUAL "y") ... endif()`.

### Application Project Structure

```
<project>/
  CMakeLists.txt        # Adds sources to EXAMPLE_LIB (tuyaapp)
  app_default.config    # Kconfig defaults
  src/                  # Application source files
```

### Submodules

External dependencies managed as git submodules: FlashDB, littlefs (KV storage), cJSON, backoffAlgorithm. Run `tos.py check` to ensure they are initialized.

## 技术文档

`docs/` 目录下有完整的 TuyaOpen 嵌入式开发技术手册，可通过 `tuyaopen-docs` MCP server（`@modelcontextprotocol/server-filesystem`）按需读取。

遇到以下问题时，**先搜索 `docs/` 目录的对应文档再回答**，不要凭训练数据猜测 API 细节：

| 问题类型 | 文档文件 |
|---------|---------|
| 编译报错、烧录失败、配网问题、环境激活 | `faq-manual.md` |
| tos.py 命令参数 | `tos-tools-manual.md` |
| CMake 组件写法、Kconfig 语法、`app_default.config` 配置 | `build-system-manual.md` |
| LVGL、显示屏、音频、GPIO/UART/I2C/SPI/PWM/ADC | `tuyaopen-embedded-programming-manual.md` |
| AI UI 消息类型、Action 枚举、聊天界面注册 | `ai-ui-components-manual.md` |
| ESP32 引脚映射、板子适配、OTA | `esp32-hardware-manual.md` |
| T5AI 快速上手（首次接触硬件） | `t5ai-getting-started-for-backend.md` |

> `lang_config.h` 是自动生成文件，字符串常量修改必须改 `assets/language/zh-CN/language.json`，不能直接改头文件（clean 后会被覆盖）。

## Supported Platforms

Defined in `platform/platform_config.yaml`. Each entry specifies a git repo, branch, and pinned commit. Platforms are fetched lazily on first build.

| Target  | Host-buildable | Notes |
|---------|---------------|-------|
| LINUX   | Yes           | Native ELF on Ubuntu/macOS |
| T2      | Cross-compile | Tuya BK7231 WiFi MCU |
| T3      | Cross-compile | Tuya WiFi MCU |
| T5AI    | Cross-compile | Tuya AI-capable MCU |
| ESP32   | Cross-compile | Espressif ESP32/C3/S3 |
| BK7231X | Cross-compile | Beken WiFi MCU |
| LN882H  | Cross-compile | Lightning Semi WiFi MCU |
