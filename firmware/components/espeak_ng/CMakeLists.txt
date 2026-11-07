file(GLOB ENGINE_SRCS "vendor/src/libespeak-ng/*.c" "vendor/src/ucd/*.c")
list(FILTER ENGINE_SRCS EXCLUDE REGEX "/(event|fifo)\\.c$")
idf_component_register(SRCS ${ENGINE_SRCS} "port.c" "offline_tts.cpp"
    INCLUDE_DIRS "include" "vendor/src" "vendor/src/ucd/include"
    REQUIRES esp_psram learning_core)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error -Wno-unused-variable -Wno-unused-function)
set_source_files_properties("vendor/src/libespeak-ng/dictionary.c" PROPERTIES
    COMPILE_OPTIONS "-include${CMAKE_CURRENT_SOURCE_DIR}/include/espeak_unicode_port.h")
