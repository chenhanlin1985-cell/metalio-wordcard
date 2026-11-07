idf_component_register(SRCS "main.cpp" "usb_disk.cpp" "power_control.cpp" "text_waveform.cpp" "wordcard_ui.cpp" "learning_screens.cpp" "dictionary_screens.cpp" "audio_player.cpp" "generated/builtin_decks.cpp"
    INCLUDE_DIRS "." "generated"
    REQUIRES learning_core learning_web espeak_ng speech_tts metalio_board esp_partition spi_flash esp_psram esp_timer nvs_flash fatfs sdmmc esp_driver_sdmmc esp_driver_gpio esp_driver_uart esp_driver_i2s esp_driver_usb_serial_jtag espressif__esp_tinyusb)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
# TinyUSB defines its weak default in the same translation unit as the caller,
# so --wrap does not intercept that reference. Rename only the storage adapter's
# strong callback, then provide our real strong callback in usb_disk.cpp.
idf_component_get_property(msc_adapter_lib espressif__esp_tinyusb COMPONENT_LIB)
target_compile_definitions(${msc_adapter_lib} PRIVATE
    tud_msc_start_stop_cb=wordcard_storage_start_stop_cb)
