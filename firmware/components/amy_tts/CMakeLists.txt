idf_component_register(SRCS "kernels/snt_front_f32.c" "kernels/snt_piperlite.c" "amy_compute.c" "amy_parallel.c" "amy_frontend.cpp" "amy_tts.cpp"
 INCLUDE_DIRS "include" "vendor" REQUIRES espeak_ng learning_core esp_psram esp_timer freertos)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
