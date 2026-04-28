if (CONFIG_ENABLE_COMP_AI_DISPLAY STREQUAL "y")

set(APP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

list(APPEND APP_MODULE_SRCS ${APP_MODULE_PATH}/app_display.c)
list(APPEND APP_MODULE_SRCS ${APP_MODULE_PATH}/tuya_lvgl.c)
set(APP_MODULE_INC ${APP_MODULE_PATH})

if (CONFIG_ENABLE_GUI2_PHOTO STREQUAL "y")
    list(APPEND APP_MODULE_INC ${APP_MODULE_PATH}/ui_photo)
    add_subdirectory(${APP_MODULE_PATH}/ui_photo)
else()
    list(APPEND APP_MODULE_INC ${APP_MODULE_PATH}/ui_photo)
    add_subdirectory(${APP_MODULE_PATH}/ui_photo)
endif()

target_sources(${EXAMPLE_LIB} PRIVATE ${APP_MODULE_SRCS})
target_include_directories(${EXAMPLE_LIB} PRIVATE ${APP_MODULE_INC})

endif()
