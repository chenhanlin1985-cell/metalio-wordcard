idf_component_register(SRCS "speech_tts.cpp"
 INCLUDE_DIRS "include" REQUIRES espeak_ng amy_tts learning_core esp_timer esp_psram)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
