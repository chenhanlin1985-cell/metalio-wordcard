/* SPDX-License-Identifier: MIT
 * Exact byte copy, no numerical conversion. Core 1 only; caller checks HP
 * memory, 16-byte alignment and positive complete blocks. No over-read. */
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32S31
    .section .iram1,"ax",@progbits
    .align 4
    .global amy_s31_copy16
    .type amy_s31_copy16,@function
amy_s31_copy16:
    beqz a2,.Lamy_copy_done
.Lamy_copy_loop:
    esp.vld.128.ip q0,a1,16
    esp.vst.128.ip q0,a0,16
    addi a2,a2,-1
    bnez a2,.Lamy_copy_loop
.Lamy_copy_done:
    ret
    .size amy_s31_copy16,.-amy_s31_copy16
#endif
