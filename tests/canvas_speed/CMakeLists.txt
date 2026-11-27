cmake_minimum_required(VERSION 3.20)
project(canvas_raster_tests CXX)
set(CMAKE_CXX_STANDARD 17)
get_filename_component(R "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
if(NOT BASELINE)
 message(FATAL_ERROR "A frozen pre-change source tree is required")
endif()
file(TO_CMAKE_PATH "${BASELINE}" BASELINE)
foreach(kind before after)
 if(kind STREQUAL before)
  set(UI "${BASELINE}/firmware/main")
 else()
  set(UI "${R}/firmware/main")
 endif()
 add_executable(canvas_${kind} "${R}/tests/canvas_raster_test.cpp"
  "${UI}/wordcard_ui.cpp" "${UI}/learning_screens.cpp" "${UI}/dictionary_screens.cpp"
  "${R}/firmware/components/learning_core/wordcard.cpp" "${R}/firmware/components/learning_core/learning_system.cpp"
  "${R}/firmware/components/learning_core/guided_content.cpp" "${R}/firmware/components/learning_core/guided_learning.cpp"
  "${R}/firmware/components/learning_core/assessment.cpp" "${R}/firmware/components/learning_core/assessment_content.cpp")
 target_include_directories(canvas_${kind} PRIVATE "${UI}" "${R}/firmware/components/learning_core/include" "${R}/firmware/components/native_learning/include")
 target_compile_options(canvas_${kind} PRIVATE /utf-8 /EHsc)
 target_compile_definitions(canvas_${kind} PRIVATE _CRT_SECURE_NO_WARNINGS)
endforeach()
