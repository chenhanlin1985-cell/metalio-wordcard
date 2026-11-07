idf_component_register(SRCS "learning_web.cpp" "generated/web_bundle.cpp"
    "vendor/quickjs.c" "vendor/libregexp.c" "vendor/libunicode.c" "vendor/dtoa.c"
    INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "vendor" REQUIRES esp_timer esp_psram native_learning)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_definitions(${COMPONENT_LIB} PRIVATE _GNU_SOURCE __STDC_NO_ATOMICS__=1 NDEBUG)
set_source_files_properties(vendor/quickjs.c vendor/libregexp.c vendor/libunicode.c vendor/dtoa.c
    # RV32 picolibc defines uint32_t as unsigned long. QuickJS uses int*
    # views of uint32_t iterator indices; at -O2 strict alias analysis can
    # discard the updated cursor and make finite string iteration stall.
    # Scope this compatibility requirement to the vendored C engine only.
    PROPERTIES COMPILE_OPTIONS "-fno-strict-aliasing;-Wno-error;-Wno-unused-function;-Wno-format;-Wno-sign-compare")
