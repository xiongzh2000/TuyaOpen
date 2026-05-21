#!/bin/bash
set -e

# ============================================================
# TuyaOpen 通用编译 + 烧录 + 日志脚本
#
# 用法:
#   ./build_flash.sh <项目路径> [动作]
#
# 项目路径:
#   可以是完整路径或相对于 TuyaOpen 根目录的路径
#   支持简写: 项目名 → 自动在 apps/tuya.ai/ 下查找
#
# 动作:
#   all     - 编译 + 烧录 + 日志 (默认)
#   build   - 仅编译
#   flash   - 仅烧录
#   monitor - 仅日志
#   bf      - 编译 + 烧录
#   fm      - 烧录 + 日志
#
# 示例:
#   ./build_flash.sh smart_badge
#   ./build_flash.sh smart_badge build
#   ./build_flash.sh apps/tuya.ai/smart_print bf
#   ./build_flash.sh smart_badge monitor
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}"
MONITOR_BAUD=460800

# ---------- 解析项目路径 ----------
resolve_project() {
    local input="$1"

    if [ -d "${input}" ] && [ -f "${input}/CMakeLists.txt" ]; then
        APP_DIR="$(cd "${input}" && pwd)"
        return 0
    fi

    if [ -d "${PROJECT_ROOT}/${input}" ] && [ -f "${PROJECT_ROOT}/${input}/CMakeLists.txt" ]; then
        APP_DIR="${PROJECT_ROOT}/${input}"
        return 0
    fi

    local guess="${PROJECT_ROOT}/apps/tuya.ai/${input}"
    if [ -d "${guess}" ] && [ -f "${guess}/CMakeLists.txt" ]; then
        APP_DIR="${guess}"
        return 0
    fi

    echo "[ERROR] 找不到项目: ${input}"
    echo "        尝试过:"
    echo "          ${input}"
    echo "          ${PROJECT_ROOT}/${input}"
    echo "          ${guess}"
    echo ""
    list_projects
    return 1
}

list_projects() {
    echo "可用项目:"
    for cml in "${PROJECT_ROOT}"/apps/tuya.ai/*/CMakeLists.txt; do
        local dir="$(dirname "${cml}")"
        local name="$(basename "${dir}")"
        [ "${name}" = "ai_components" ] && continue
        echo "  ${name}"
    done
}

# ---------- 串口检测 ----------
# T5AI 开发板: 排序后数字小的 = 烧录口, 数字大的 = 日志口
detect_ports() {
    local ports=($(ls /dev/cu.usbmodem* 2>/dev/null | sort))
    if [ ${#ports[@]} -lt 2 ]; then
        echo "[WARN] 未检测到两个 USB 串口，请检查开发板连接"
        echo "       当前: ${ports[*]:-无}"
        FLASH_PORT=""
        MONITOR_PORT=""
        return 1
    fi
    FLASH_PORT="${ports[0]}"
    MONITOR_PORT="${ports[1]}"
    echo "[INFO] 烧录口: ${FLASH_PORT}"
    echo "[INFO] 日志口: ${MONITOR_PORT}"
}

# ---------- 激活环境 ----------
activate_env() {
    cd "${PROJECT_ROOT}" && . ./export.sh
}

# ---------- 编译 ----------
do_build() {
    echo ""
    echo "==================== 编译: $(basename "${APP_DIR}") ===================="
    activate_env
    cd "${APP_DIR}"

    rm -rf dist .build
    tos.py clean -f
    tos.py build

    echo "[OK] 编译完成"
}

# ---------- 烧录 ----------
do_flash() {
    echo ""
    echo "==================== 烧录: $(basename "${APP_DIR}") ===================="
    detect_ports || return 1
    activate_env
    cd "${APP_DIR}"

    echo "[INFO] 使用烧录口: ${FLASH_PORT}"
    tos.py flash -p "${FLASH_PORT}"

    echo "[OK] 烧录完成"
}

# ---------- 日志 ----------
do_monitor() {
    echo ""
    echo "==================== 日志: $(basename "${APP_DIR}") ===================="
    detect_ports || return 1
    activate_env
    cd "${APP_DIR}"

    echo "[INFO] 日志口: ${MONITOR_PORT}, 波特率: ${MONITOR_BAUD}"
    echo "[INFO] Ctrl+C 退出"
    tos.py monitor -p "${MONITOR_PORT}" -b ${MONITOR_BAUD}
}

# ---------- 入口 ----------
if [ $# -lt 1 ]; then
    echo "用法: $0 <项目名> [build|flash|monitor|bf|fm|all]"
    echo ""
    list_projects
    exit 1
fi

resolve_project "$1"
MODE="${2:-all}"

echo "[INFO] 项目: ${APP_DIR}"
echo "[INFO] 动作: ${MODE}"

case "${MODE}" in
    build)   do_build ;;
    flash)   do_flash ;;
    monitor) do_monitor ;;
    bf)      do_build && do_flash ;;
    fm)      do_flash && do_monitor ;;
    all)     do_build && do_flash && do_monitor ;;
    *)
        echo "未知动作: ${MODE}"
        echo "可选: build | flash | monitor | bf | fm | all"
        exit 1
        ;;
esac
