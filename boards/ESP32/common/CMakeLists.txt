set(COMMON_PATH ${CMAKE_CURRENT_LIST_DIR})

# add audio source files
file(GLOB_RECURSE AUDIO_SRCS "${COMMON_PATH}/audio/*.c")

# add lcd source files
file(GLOB_RECURSE LCD_SRCS "${COMMON_PATH}/lcd/*.c")

# add led source files
file(GLOB_RECURSE LED_SRCS "${COMMON_PATH}/led/*.c")

# add io_expander source files
file(GLOB_RECURSE IO_EXPANDER_SRCS "${COMMON_PATH}/io_expander/*.c")

# add tp source files
file(GLOB_RECURSE TP_SRCS "${COMMON_PATH}/tp/*.c")

# add button source files
file(GLOB_RECURSE BUTTON_SRCS "${COMMON_PATH}/button/*.c")

set(BOARD_SRC "")
set(BOARD_INC "${COMMON_PATH}")

if (CONFIG_ENABLE_DISPLAY)
    list(APPEND BOARD_SRC
        "${COMMON_PATH}"
        "${LCD_SRCS}"
        "${IO_EXPANDER_SRCS}"
    )

    list(APPEND BOARD_INC
        "${COMMON_PATH}/lcd"
        "${COMMON_PATH}/io_expander"
    )
endif()

if (CONFIG_ENABLE_TP)
    list(APPEND BOARD_SRC
        "${TP_SRCS}"
    )

    list(APPEND BOARD_INC
        "${COMMON_PATH}/tp"
    )
endif()

# common/audio/*.c are ES8311/ES8388 codec drivers that depend on the
# audio_codecs component (tdl_audio_driver.h). Gate on ENABLE_AUDIO_CODECS
# (set by audio_player), not ENABLE_AUDIO, so apps without a codec stack
# (e.g. switch_demo) don't try to compile these and fail on the missing header.
if (CONFIG_ENABLE_AUDIO_CODECS)
    list(APPEND BOARD_SRC
        "${AUDIO_SRCS}"
    )

    list(APPEND BOARD_INC
        "${COMMON_PATH}/audio"
    )
endif()

if (CONFIG_ENABLE_LED)
    list(APPEND BOARD_SRC
        "${LED_SRCS}"
    )

    list(APPEND BOARD_INC
        "${COMMON_PATH}/led"
    )
endif()

if (CONFIG_ENABLE_BUTTON)
    list(APPEND BOARD_SRC
        "${BUTTON_SRCS}"
    )

    list(APPEND BOARD_INC
        "${COMMON_PATH}/button"
    )
endif()
