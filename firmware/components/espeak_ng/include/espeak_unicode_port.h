#pragma once
#include <wctype.h>
#include <ucd/ucd.h>
// Picolibc's C locale classifies IPA letters such as U+026A as non-alphabetic.
// dictionary.c then omits the tie inside eI/aI/oU, changing neural token IDs.
// Apply only to that translation unit; preserve the pinned vendor sources.
static inline int wordcard_espeak_iswalpha(wint_t c) {
    return ucd_isalpha((codepoint_t)c);
}
#undef iswalpha
#define iswalpha wordcard_espeak_iswalpha
