idf_component_register(SRCS "wordcard.cpp" "learning_system.cpp" "dictionary.cpp" "speech_policy.cpp" "guided_learning.cpp" "guided_content.cpp" "assessment.cpp" "assessment_content.cpp" INCLUDE_DIRS "include")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
