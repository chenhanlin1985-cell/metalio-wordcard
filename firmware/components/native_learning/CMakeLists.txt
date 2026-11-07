idf_component_register(SRCS "native_model.cpp" "native_transitions.cpp" "native_validate.cpp" "native_daily.cpp" "native_study.cpp" "native_views.cpp" "vocabulary_fsrs.cpp" "vocabulary_engine.cpp" "vocabulary_external.cpp" "vocabulary_store.cpp" "vocabulary_ui.cpp" "generated/learning_data.cpp"
 INCLUDE_DIRS "include" REQUIRES esp_psram learning_core)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
