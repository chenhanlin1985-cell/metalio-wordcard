cmake_minimum_required(VERSION 3.20)
project(web_firmware_host C CXX)
add_executable(display_refresh_budget "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/display_refresh_budget_test.cpp")
target_compile_features(display_refresh_budget PRIVATE cxx_std_17)
target_include_directories(display_refresh_budget PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/main")
set(CMAKE_CXX_STANDARD 17)
get_filename_component(R "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_LIBC OFF CACHE BOOL "" FORCE)
add_library(qjs STATIC "${R}/firmware/components/learning_web/vendor/quickjs.c" "${R}/firmware/components/learning_web/vendor/libregexp.c" "${R}/firmware/components/learning_web/vendor/libunicode.c" "${R}/firmware/components/learning_web/vendor/dtoa.c")
target_compile_definitions(qjs PRIVATE _GNU_SOURCE __STDC_NO_ATOMICS__=1 QUICKJS_NG_BUILD NDEBUG _CRT_SECURE_NO_WARNINGS)
target_compile_options(qjs PRIVATE /utf-8 /std:c11)
add_executable(webhost "${R}/tests/web_firmware_test.cpp"
 "${R}/firmware/components/learning_web/learning_web.cpp"
 "${R}/firmware/components/learning_web/generated/web_bundle.cpp"
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
 "${R}/firmware/main/wordcard_ui.cpp" "${R}/firmware/main/learning_screens.cpp" "${R}/firmware/main/dictionary_screens.cpp"
 "${R}/firmware/components/learning_core/dictionary.cpp"
 "${R}/firmware/components/learning_core/wordcard.cpp" "${R}/firmware/components/learning_core/learning_system.cpp"
 "${R}/firmware/components/learning_core/guided_content.cpp" "${R}/firmware/components/learning_core/guided_learning.cpp"
 "${R}/firmware/components/learning_core/assessment.cpp" "${R}/firmware/components/learning_core/assessment_content.cpp")
include_directories("${R}/firmware/components/native_learning/include")
target_include_directories(webhost PRIVATE "${R}/firmware/components/learning_web/vendor" "${R}/firmware/components/learning_web/include" "${R}/firmware/main" "${R}/firmware/components/learning_core/include")
target_compile_options(webhost PRIVATE /utf-8 /EHsc)
target_compile_definitions(webhost PRIVATE _CRT_SECURE_NO_WARNINGS WORDCARD_REFERENCE_TEST)
target_link_libraries(webhost PRIVATE qjs)
get_target_property(preview_sources webhost SOURCES)
list(REMOVE_ITEM preview_sources "${R}/tests/web_firmware_test.cpp")
add_executable(native_preview "${R}/tools/native_preview.cpp" ${preview_sources})
target_include_directories(native_preview PRIVATE "${R}/firmware/components/learning_web/vendor" "${R}/firmware/components/learning_web/include" "${R}/firmware/main" "${R}/firmware/components/learning_core/include")
target_compile_options(native_preview PRIVATE /utf-8 /EHsc)
target_compile_definitions(native_preview PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(native_preview PRIVATE qjs)
foreach(name keyboard_input power_button power_shutdown playback_gain tts_audio_cache)
 add_executable(${name} "${R}/tests/${name}_test.cpp" "${R}/firmware/components/learning_core/wordcard.cpp")
 target_include_directories(${name} PRIVATE "${R}/firmware/main" "${R}/firmware/components/learning_core/include")
 target_compile_options(${name} PRIVATE /utf-8 /EHsc)
 target_compile_definitions(${name} PRIVATE _CRT_SECURE_NO_WARNINGS)
endforeach()
add_executable(dictionary_flow "${R}/tests/dictionary_flow_test.cpp" "${R}/firmware/components/learning_core/dictionary.cpp" "${R}/firmware/components/learning_core/speech_policy.cpp" ${preview_sources})
target_include_directories(dictionary_flow PRIVATE "${R}/firmware/components/learning_web/vendor" "${R}/firmware/components/learning_web/include" "${R}/firmware/main" "${R}/firmware/components/learning_core/include")
target_compile_options(dictionary_flow PRIVATE /utf-8 /EHsc)
target_compile_definitions(dictionary_flow PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(dictionary_flow PRIVATE qjs)

# Full native UI regression targets use the same CMake/SDK environment as webhost.
function(add_full_ui_test name test_source)
 add_executable(${name} "${R}/tests/${test_source}" ${preview_sources})
 target_include_directories(${name} PRIVATE "${R}/firmware/components/learning_web/vendor" "${R}/firmware/components/learning_web/include" "${R}/firmware/main" "${R}/firmware/components/learning_core/include")
 target_compile_options(${name} PRIVATE /utf-8 /EHsc)
 target_compile_definitions(${name} PRIVATE _CRT_SECURE_NO_WARNINGS)
 target_link_libraries(${name} PRIVATE qjs)
endfunction()
add_full_ui_test(guided_learning guided_learning_test.cpp)
add_full_ui_test(assessment_ui assessment_test.cpp)
add_full_ui_test(learning_review learning_review_test.cpp)
add_full_ui_test(course_dictionary course_dictionary_test.cpp)
add_full_ui_test(native_facade native_facade_test.cpp)
add_full_ui_test(vocabulary_ui vocabulary_ui_test.cpp)
add_full_ui_test(menu_navigation menu_navigation_test.cpp)
add_executable(vocabulary_host "${R}/tests/vocabulary_host.cpp"
 "${R}/firmware/components/native_learning/vocabulary_fsrs.cpp"
 "${R}/firmware/components/native_learning/vocabulary_engine.cpp")
target_compile_options(vocabulary_host PRIVATE /utf-8 /EHsc)
target_compile_definitions(vocabulary_host PRIVATE _CRT_SECURE_NO_WARNINGS)
target_sources(course_dictionary PRIVATE "${R}/firmware/components/learning_core/dictionary.cpp" "${R}/firmware/components/learning_core/speech_policy.cpp")
add_executable(guided_app "${R}/tests/guided_app_test.cpp" "${R}/firmware/components/learning_core/wordcard.cpp" "${R}/firmware/components/learning_core/guided_content.cpp" "${R}/firmware/components/learning_core/guided_learning.cpp" "${R}/firmware/components/learning_core/learning_system.cpp")
target_include_directories(guided_app PRIVATE "${R}/firmware/main" "${R}/firmware/components/learning_core/include")
target_compile_options(guided_app PRIVATE /utf-8 /EHsc)
target_compile_definitions(guided_app PRIVATE _CRT_SECURE_NO_WARNINGS)
add_executable(recall_app "${R}/tests/recall_app_test.cpp" "${R}/firmware/components/learning_core/wordcard.cpp" "${R}/firmware/components/learning_core/dictionary.cpp" "${R}/firmware/components/learning_core/guided_content.cpp" "${R}/firmware/components/learning_core/guided_learning.cpp" "${R}/firmware/components/learning_core/learning_system.cpp")
target_include_directories(recall_app PRIVATE "${R}/firmware/main" "${R}/firmware/components/learning_core/include")
target_compile_options(recall_app PRIVATE /utf-8 /EHsc)
target_compile_definitions(recall_app PRIVATE _CRT_SECURE_NO_WARNINGS)

add_executable(vocabulary_intent "${R}/tests/vocabulary_intent_test.cpp"
 "${R}/firmware/components/native_learning/vocabulary_fsrs.cpp"
 "${R}/firmware/components/native_learning/vocabulary_engine.cpp")
target_compile_options(vocabulary_intent PRIVATE /utf-8 /EHsc)
target_compile_definitions(vocabulary_intent PRIVATE _CRT_SECURE_NO_WARNINGS)

add_executable(vocabulary_external "${R}/tests/vocabulary_external_test.cpp"
 "${R}/firmware/components/native_learning/vocabulary_fsrs.cpp"
 "${R}/firmware/components/native_learning/vocabulary_engine.cpp"
 "${R}/firmware/components/native_learning/vocabulary_external.cpp"
 "${R}/firmware/components/native_learning/vocabulary_store.cpp"
 "${R}/firmware/components/native_learning/vocabulary_ui.cpp")
target_compile_options(vocabulary_external PRIVATE /utf-8 /EHsc)
target_compile_definitions(vocabulary_external PRIVATE _CRT_SECURE_NO_WARNINGS)

# Alias exposure tests use the production personal dictionary registration path.
target_sources(vocabulary_intent PRIVATE
 "${R}/firmware/components/native_learning/vocabulary_external.cpp"
 "${R}/firmware/components/native_learning/vocabulary_store.cpp"
 "${R}/firmware/components/native_learning/vocabulary_ui.cpp")

foreach(target vocabulary_external vocabulary_intent)
 target_sources(${target} PRIVATE "${R}/firmware/components/learning_core/dictionary.cpp" "${R}/firmware/components/learning_core/wordcard.cpp")
 target_include_directories(${target} PRIVATE "${R}/firmware/components/learning_core/include")
endforeach()
add_full_ui_test(custom_words custom_words_test.cpp)

add_full_ui_test(recovery recovery_test.cpp)
target_compile_definitions(recovery PRIVATE WORDCARD_TEST_STORAGE)

# Fast, resource-free checks. Full UI tests take external resource arguments.
enable_testing()
add_test(NAME display_refresh_budget COMMAND display_refresh_budget)
add_test(NAME keyboard_input COMMAND keyboard_input)
add_test(NAME power_button COMMAND power_button)
add_test(NAME power_shutdown COMMAND power_shutdown)
add_test(NAME playback_gain COMMAND playback_gain)
add_test(NAME tts_audio_cache COMMAND tts_audio_cache "${CMAKE_CURRENT_BINARY_DIR}/test-data")
