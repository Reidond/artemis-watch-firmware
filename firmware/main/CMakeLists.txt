file(GLOB_RECURSE SRCS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/*.c")

# Fonts: files in ui/fonts/local/ (git-ignored, from `tools/gen_fonts.sh pragmata`)
# replace the checked-in ones of the same name. Reconfigure after adding or
# removing them.
list(FILTER SRCS EXCLUDE REGEX "/ui/fonts/local/")
file(GLOB LOCAL_FONTS "${CMAKE_CURRENT_LIST_DIR}/ui/fonts/local/*.c")
foreach(local ${LOCAL_FONTS})
    get_filename_component(name ${local} NAME)
    list(REMOVE_ITEM SRCS "${CMAKE_CURRENT_LIST_DIR}/ui/fonts/${name}")
    list(APPEND SRCS ${local})
endforeach()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
    REQUIRES
        driver esp_driver_gpio esp_driver_ledc esp_driver_spi esp_driver_i2c
        esp_adc esp_lcd esp_timer efuse nvs_flash console esp_app_format
        esp_driver_usb_serial_jtag mbedtls bt json app_update
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
