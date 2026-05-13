# TuyaOpen 构建系统手册（AI Agent 用）

> 整理自 https://www.tuyaopen.ai/zh/docs/build-system/

---

## 构建流程

`tos.py build` 的完整执行链（`tools/cli_command/cli_build.py`）：

```
1. 环境校验        校验 SDK submodule 已初始化
2. 配置初始化      验证 .build/cache/using.config 与当前 Kconfig 一致
3. 平台下载        按 platform/platform_config.yaml 拉取平台代码（首次）
4. 平台 Hook       执行 platform_prepare.py / build_setup.py
5. CMake 配置      在 .build/ 生成 Ninja 构建文件
6. Ninja 编译      编译应用目标
7. 产物验证        检查 .build/bin/ 下的固件文件
```

---

## 目录结构

```
TuyaOpen/
├── export.sh                    # 环境激活脚本
├── tos.py                       # 工具入口
├── tools/cli_command/           # 构建驱动（cli_build.py、cli_config.py）
├── src/                         # SDK 组件（每个子目录一个 CMakeLists.txt）
│   ├── tal_system/              # 系统 TAL 组件
│   ├── tal_wifi/                # Wi-Fi TAL 组件
│   ├── liblvgl/                 # LVGL 库
│   └── ...
├── platform/
│   ├── platform_config.yaml     # 各平台 repo/branch/commit 锁定
│   └── T5AI/                   # 首次编译后自动拉取
├── boards/                      # 板级 BSP 配置

<project>/
├── CMakeLists.txt               # 应用 CMake（添加源文件到 EXAMPLE_LIB）
├── app_default.config           # Kconfig 默认值（唯一可以手动编辑的配置文件）
├── Kconfig                      # 应用层 Kconfig 定义
├── src/                         # 应用源码
└── .build/
    ├── cache/using.config       # 合并后的最终配置（自动生成，勿手动修改）
    ├── bin/                     # 固件输出目录
    └── lib/                     # 静态库
```

---

## Kconfig 配置系统

### 配置文件关系

```
app_default.config（用户维护）
        +
platform Kconfig / SDK Kconfig / board Kconfig
        ↓  CMake 配置阶段合并
.build/cache/using.config（最终配置，只读）
        ↓  编译时作为宏定义注入
C 代码中的 #if defined(CONFIG_XXX)
```

### app_default.config 格式

```ini
CONFIG_PROJECT_VERSION="1.0.1"
CONFIG_TUYA_PRODUCT_ID="xxxxxxxxxxxxxxxx"
CONFIG_ENABLE_COMP_AI_VIDEO=y
CONFIG_ENABLE_COMP_AI_PICTURE=y
CONFIG_ENABLE_COMP_AI_DISPLAY=y
CONFIG_BOARD_CHOICE_T5AI=y
CONFIG_TUYA_T5AI_BOARD_LCD_35565=y
CONFIG_TUYA_T5AI_BOARD_CAMERA=y
CONFIG_TUYA_T5AI_BOARD_PRINTER_DP48=y
# CONFIG_ENABLE_BATTERY is not set    ← 注释掉的表示 n
```

### 关键约束

| 场景 | 正确操作 |
|------|---------|
| 修改 `app_default.config` | 必须 `rm -rf .build` 后重编，`tos.py clean` 不够 |
| 非 TTY 环境改配置 | 直接编辑 `app_default.config` 或 `cp config/XXX.config app_default.config` |
| 切换板子 | `tos.py config choice` 或覆盖 `app_default.config` |
| 验证配置是否生效 | 检查 `.build/cache/using.config` 对应项是否已更新 |

### Kconfig 语法（在 Kconfig 文件中定义配置项）

```kconfig
menu "configure app (your_chat_bot)"

config TUYA_PRODUCT_ID
    string "product ID of project"
    default "p320pepzvmm1ghse"

config ENABLE_BATTERY
    bool "enable the battery module"
    default n

menuconfig ENABLE_COMP_AI_DISPLAY
    bool "enable ai ui"
    default y
    select ENABLE_LIBLVGL     # 自动选中依赖项

if(ENABLE_COMP_AI_DISPLAY)
    config ENABLE_AI_CHAT_CUSTOM_UI
        bool "enable ai chat custom ui"
        default n
    
    choice
        prompt "choose ui style"
        default ENABLE_AI_CHAT_GUI_WECHAT
        
        config ENABLE_AI_CHAT_GUI_WECHAT
            bool "WeChat style"
        config ENABLE_AI_CHAT_GUI_CHATBOT
            bool "Chatbot style"
        config ENABLE_AI_CHAT_GUI_OLED
            bool "OLED style"
    endchoice
endif

rsource "../ai_components/Kconfig"   # 引入子目录 Kconfig

endmenu
```

---

## CMake 组件模型

### SDK 组件 CMakeLists.txt 模板

```cmake
# src/my_component/CMakeLists.txt
set(MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(MODULE_NAME ${MODULE_PATH} NAME)

# 收集源文件
aux_source_directory(${MODULE_PATH}/src LIB_SRCS)
# 或递归收集：
# file(GLOB_RECURSE LIB_SRCS ${MODULE_PATH}/src/*.c)

set(LIB_PUBLIC_INC ${MODULE_PATH}/include)

# 创建库目标
add_library(${MODULE_NAME})
target_sources(${MODULE_NAME} PRIVATE ${LIB_SRCS})
target_include_directories(${MODULE_NAME}
    PUBLIC ${LIB_PUBLIC_INC}
)

# 注册到父作用域（SDK 自动发现机制）
list(APPEND COMPONENT_LIBS ${MODULE_NAME})
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
list(APPEND COMPONENT_PUBINC ${LIB_PUBLIC_INC})
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
```

**SDK 自动发现**：顶层 `CMakeLists.txt` 自动遍历 `src/` 下所有含 `CMakeLists.txt` 的子目录。所有组件的 `.o` 文件合并为 `libtuyaos.a`。

### Kconfig 条件控制组件编译

```cmake
if (CONFIG_ENABLE_BATTERY STREQUAL "y")
    add_subdirectory(${APP_PATH}/src/battery)
endif()

if (CONFIG_ENABLE_COMP_AI_VIDEO STREQUAL "y")
    add_subdirectory(${COMP_PATH}/ai_video)
endif()
```

### 应用 CMakeLists.txt 模板

```cmake
# <project>/CMakeLists.txt
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(APP_NAME ${APP_PATH} NAME)

# 收集应用源文件
aux_source_directory(${APP_PATH}/src APP_SRCS)
set(APP_INC ${APP_PATH}/include)

add_library(${EXAMPLE_LIB})        # EXAMPLE_LIB 由构建系统定义
target_sources(${EXAMPLE_LIB} PRIVATE ${APP_SRCS})
target_include_directories(${EXAMPLE_LIB} PRIVATE ${APP_INC})

# 添加子目录
add_subdirectory(${APP_PATH}/../ai_components)

if (CONFIG_ENABLE_BATTERY STREQUAL "y")
    add_subdirectory(${APP_PATH}/src/battery)
endif()
```

---

## 平台配置（platform_config.yaml）

```yaml
- name: T5AI
  repo: https://github.com/tuya/TuyaOpen-T5AI.git
  branch: master
  commit: abc1234567
  
- name: ESP32
  repo: https://github.com/tuya/TuyaOpen-ESP32.git
  branch: master
  commit: def5678901
```

平台 SDK 首次编译时自动克隆到 `platform/<NAME>/`，由 `tos.py update` 管理版本锁定。

---

## 多平台编译

```bash
# 批量编译所有平台配置（CI 验证）
tos.py dev bac -d ./dist

# 切换到 ESP32 平台
cp config/DNESP32S3_BOX.config app_default.config
rm -rf .build
tos.py build
```

---

## 构建产物

| 文件 | 说明 |
|------|------|
| `.build/bin/{name}_QIO_{ver}.bin` | 主固件（用于烧录）|
| `.build/bin/{name}_UA_{ver}.bin` | OTA 升级包 |
| `.build/bin/{name}_UG_{ver}.bin` | OTA 差分包 |
| `.build/lib/libtuyaos.a` | SDK 静态库 |
| `.build/lib/libtuyaapp.a` | 应用静态库 |
| `.build/cache/using.config` | 最终合并配置（只读）|

---

## CMake 关键变量

| 变量 | 说明 |
|------|------|
| `EXAMPLE_LIB` | 应用库名，由构建系统注入 |
| `COMPONENT_LIBS` | SDK 组件库列表（需 set PARENT_SCOPE）|
| `COMPONENT_PUBINC` | SDK 组件公共头文件路径（需 set PARENT_SCOPE）|
| `CONFIG_PROJECT_NAME` | 项目名（来自 Kconfig）|
| `CONFIG_PLATFORM_CHOICE` | 平台选择（T5AI / ESP32 等）|
| `CONFIG_CHIP_CHOICE` | 芯片型号 |
| `CONFIG_BOARD_CHOICE` | 开发板型号 |
| `OPEN_SDK_PYTHON` | Python 解释器路径（由 export.sh 导出，CMake custom_command 依赖此变量）|

> ⚠️ `OPEN_SDK_PYTHON` 未设置时，CMake 跳过 `lang_config.h` 等自动生成文件的 custom_command。不通过 `export.sh` 直接调用 `tos.py` 时需注意手动生成。
