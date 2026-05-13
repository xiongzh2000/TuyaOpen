# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

`your_chat_bot` is a TuyaOpen AI voice chatbot application. It captures voice via microphone, performs speech recognition via Tuya AI cloud, enables conversation, and renders real-time chat content on an LCD screen.

The app sits under `apps/tuya.ai/your_chat_bot/` and depends on the shared `apps/tuya.ai/ai_components/` library (a sibling directory). Build and tool commands follow the parent repo's `tos.py` workflow — see `TuyaOpen/CLAUDE.md` for environment setup and build commands.

## Building for a Specific Board

Board configs live in `config/`. To switch boards:

```bash
# From the project directory
tos config_choice      # interactive board picker
# or copy a preset directly:
cp config/TUYA_T5AI_BOARD_LCD_3.5.config app_default.config
tos build
```

The default `app_default.config` targets `TUYA_T5AI_BOARD_LCD_3.5`.

## Device Credentials

Before flashing, set real credentials in `include/tuya_config.h`:

```c
#define TUYA_OPENSDK_UUID    "uuidxxxxxxxxxxxxxxxx"
#define TUYA_OPENSDK_AUTHKEY "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
```

Obtain UUID/AUTHKEY from the Tuya IoT platform (2 free licenses per account).

## Application Architecture

### Entry & Lifecycle (`src/tuya_main.c`)

`user_main()` is the application entry point (wrapped in a FreeRTOS thread on non-Linux targets). It:
1. Initializes TAL subsystems (KV, timer, workqueue, CLI, authorize)
2. Calls `reset_netconfig_start()` for the 3× restart → factory reset logic
3. Initializes Tuya IoT client with product key/UUID/authkey
4. Brings up the network manager (WiFi BLE+AP provisioning or wired)
5. Calls `board_register_hardware()` then `app_chat_bot_init()`
6. Enters the `tuya_iot_yield()` loop

DP ID `3` = volume. Volume changes from APP arrive via `TUYA_EVENT_DP_RECEIVE_OBJ` → `audio_dp_obj_proc()` → `ai_chat_set_volume()`.

### Chat Bot Init (`src/app_chat_bot.c`)

`app_chat_bot_init()` is the orchestrator. It calls `ai_chat_init()` (from `ai_components/ai_main`) with the default mode and volume, then conditionally initializes:
- `app_ui_action_register()` — registers UI action callbacks (camera, album, printer)
- `ai_video_init()` — if `ENABLE_COMP_AI_VIDEO`
- `ai_mcp_init()` — if `ENABLE_COMP_AI_MCP`
- `ai_picture_init()` — if `ENABLE_COMP_AI_PICTURE`

### UI Actions (`src/app_ui_action.c`)

Registers `__app_ui_action_handle()` as the UI action callback via `ai_ui_action_cb_register()`. Handles: camera open/close/photo, AI vision toggle, image album CRUD, bulk delete, image attachment for chat, and printing. Only compiled when `ENABLE_COMP_AI_DISPLAY=1`.

### Shared AI Components (`../ai_components/`)

| Component   | Purpose |
|-------------|---------|
| `ai_agent`  | Cloud AI communication — sends text/file/image to Tuya AI, handles role switching and alerts |
| `ai_audio`  | Mic input capture (`ai_audio_input`) and TTS playback (`ai_audio_player`) |
| `ai_main`   | Orchestrates chat modes; public API is `ai_chat_init()`, `ai_chat_set_volume()`, `ai_chat_get_volume()` |
| `ai_mode`   | Four interaction modes: hold (press-to-talk), oneshot (button toggle + VAD), wakeup (wake-word, one turn), free (wake-word, continuous) |
| `ai_ui`     | LVGL-based UI with three built-in styles: wechat, chatbot, oled; stream-text display |
| `ai_picture`| JPEG capture, persistent image album, cloud image upload/download |
| `ai_video`  | YUV camera frame capture and JPEG encoding |
| `ai_mcp`    | MCP server for tool-calling capabilities |
| `ai_skills` | Reusable AI skill definitions |

### Display (`src/display2/`)

LVGL-based display layer. `tuya_lvgl.c` wraps LVGL init; `app_display.c` runs a message queue + dedicated thread. `app_ui_helper.c` provides helper wrappers. Only built when `ENABLE_CHAT_DISPLAY=1`.

## Key Kconfig Flags

| Flag | Effect |
|------|--------|
| `ENABLE_COMP_AI_AUDIO` | Audio input + playback (required for voice chat) |
| `ENABLE_COMP_AI_DISPLAY` | LCD UI via LVGL |
| `ENABLE_COMP_AI_VIDEO` | Camera capture for vision features |
| `ENABLE_COMP_AI_PICTURE` | Image album and cloud picture send/receive |
| `ENABLE_COMP_AI_MCP` | MCP tool-calling server |
| `ENABLE_AEC` | Echo cancellation — disable if hardware lacks AEC support |
| `ENABLE_BATTERY` | Battery ADC monitoring (`src/battery/`) |
| `ENABLE_PRINTER` | Thermal printer output (`src/app_printer.c`) |
| `ENABLE_AI_CHAT_GUI_WECHAT/CHATBOT/OLED` | UI style selection |
| `ENABLE_AI_UI_TEXT_STREAMING` | Stream AI reply text token by token |

## Network Reset

Three rapid restarts (RST button) trigger network config clear. Logic is in `src/reset_netcfg.c`; counter stored in KV under key `rst_cnt`.
