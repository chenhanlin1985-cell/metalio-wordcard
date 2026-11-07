# Includes the portable quality_regions.c used for black-control restoration.
file(GLOB DISPLAY_SOURCES "vendor/display/*.c" "vendor/display/output_common/*.c" "vendor/display/output_lcd/*.c" "vendor/display/waveforms/*.c" "vendor/bus/*.c")
idf_component_register(
    SRCS ${DISPLAY_SOURCES} "vendor/periph/frontlight.cc" "vendor/input/metalio_vibe.cc"
        "vendor/common/settings.cc" "vendor/common/pcf8563.cc" "vendor/common/power_i2c.c" "board_support.cpp"
    INCLUDE_DIRS "." "vendor" "vendor/display" "vendor/display/output_common" "vendor/display/output_lcd" "vendor/display/waveforms" "vendor/bus" "vendor/periph" "vendor/input" "vendor/common"
    REQUIRES driver esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_uart esp_driver_rmt
        esp_driver_dma esp_driver_ledc esp_driver_sdmmc esp_timer esp_adc esp_lcd esp_hal_lcd
        esp_hal_i2s esp_hal_rmt esp_hw_support esp_mm esp_partition fatfs nvs_flash sdmmc)
# The upstream C++ drivers intentionally zero-fill newer SDK aggregate fields.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-function -Wno-missing-field-initializers)
# Keep full 48-phase tables as the diagnostic baseline. Earlier trials changed
# timing and refresh policy too; they do not isolate the effect of trimming.
target_compile_definitions(${COMPONENT_LIB} PRIVATE MY_WAVEFORM_TRIM=0)
