cmake_minimum_required(VERSION 3.20)
project(native_study_tests C CXX)
set(CMAKE_CXX_STANDARD 17)
get_filename_component(R "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_executable(native_model_cli "${R}/tests/native_model_cli.cpp"
 "${R}/firmware/components/native_learning/native_model.cpp"
 "${R}/firmware/components/native_learning/native_transitions.cpp"
 "${R}/firmware/components/native_learning/native_validate.cpp"
 "${R}/firmware/components/native_learning/native_daily.cpp"
 "${R}/firmware/components/native_learning/native_study.cpp"
 "${R}/firmware/components/native_learning/native_views.cpp"
 "${R}/firmware/components/native_learning/vocabulary_fsrs.cpp"
 "${R}/firmware/components/native_learning/vocabulary_engine.cpp"
 "${R}/firmware/components/native_learning/vocabulary_store.cpp"
 "${R}/firmware/components/native_learning/vocabulary_external.cpp"
 "${R}/firmware/components/native_learning/vocabulary_ui.cpp"
 "${R}/firmware/components/native_learning/generated/learning_data.cpp"
 "${R}/firmware/components/learning_web/vendor/libunicode.c")
target_include_directories(native_model_cli PRIVATE "${R}/firmware/components/native_learning/include" "${R}/firmware/components/learning_web/vendor")
target_compile_options(native_model_cli PRIVATE /utf-8)
target_compile_options(native_model_cli PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
target_compile_definitions(native_model_cli PRIVATE _CRT_SECURE_NO_WARNINGS)
get_target_property(native_sources native_model_cli SOURCES)
list(REMOVE_ITEM native_sources "${R}/tests/native_model_cli.cpp")
add_executable(native_canvas "${R}/tests/native_canvas_test.cpp" ${native_sources}
 "${R}/firmware/main/wordcard_ui.cpp" "${R}/firmware/main/learning_screens.cpp" "${R}/firmware/main/dictionary_screens.cpp"
 "${R}/firmware/components/learning_core/wordcard.cpp" "${R}/firmware/components/learning_core/learning_system.cpp"
 "${R}/firmware/components/learning_core/guided_content.cpp" "${R}/firmware/components/learning_core/guided_learning.cpp"
 "${R}/firmware/components/learning_core/assessment.cpp" "${R}/firmware/components/learning_core/assessment_content.cpp")
target_include_directories(native_canvas PRIVATE "${R}/firmware/components/native_learning/include" "${R}/firmware/components/learning_web/vendor" "${R}/firmware/main" "${R}/firmware/components/learning_core/include")
target_compile_options(native_canvas PRIVATE /utf-8 $<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
target_compile_definitions(native_canvas PRIVATE _CRT_SECURE_NO_WARNINGS)
