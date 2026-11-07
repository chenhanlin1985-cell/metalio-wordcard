/* SPDX-License-Identifier: Apache-2.0
 * Integer dot products using Espressif PIE v2. Instruction semantics checked
 * against esp-dl's dotprod and PIE reference. No speculative end loads.
 * Core-1 HP-stack worker only. FreeRTOS owns coprocessor context.
 * Each XACC block has <=256 products: 256*32768^2=2^38 < 2^39.
 * Extract all 40 bits without saturating/rounding; accumulate blocks in 64 bits.
 */
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32S31
 .section .iram1,"ax",@progbits
 .align 4
 .macro AMY_DOT name, bits
 .global \name
 .type \name,@function
\name:
 /* a0=x, a1=w (16-byte aligned), a2=n positive multiple of 16. */
 addi sp,sp,-16
 li t0,0
 li t1,0
1:
 li t2,256
 bge a2,t2,2f
 mv t2,a2
2:
 sub a2,a2,t2
 srli t3,t2,4
 esp.zero.xacc
3:
 .if \bits == 8
 esp.vldext.s8.ip q0,q1,a1,16
 .else
 esp.vld.128.ip q0,a1,16
 esp.vld.128.ip q1,a1,16
 .endif
 esp.vld.128.ip q2,a0,16
 esp.vld.128.ip q3,a0,16
 esp.vmulas.s16.xacc q0,q2
 esp.vmulas.s16.xacc q1,q3
 addi t3,t3,-1
 bnez t3,3b
 /* The full-width signed store avoids version-dependent MOVX low/high
  * register extraction. This is the dedicated worker's aligned HP stack. */
 mv a3,sp
 esp.st.s.xacc.ip a3,0
 lw a4,0(sp)
 lw a3,4(sp)
 add t4,t0,a4
 sltu t5,t4,t0
 add t1,t1,a3
 add t1,t1,t5
 mv t0,t4
 bnez a2,1b
 mv a0,t0
 mv a1,t1
 addi sp,sp,16
 ret
 .size \name,.-\name
 .endm
 AMY_DOT amy_s31_dot16,16
 AMY_DOT amy_s31_dot8,8
 /* Eight output channels per dot. Each S16 QACC lane accumulates in 64
  * bits. w is [input][8 outputs], x is n padded to a multiple of 8.
  * a0=x, a1=w, a2=int64[8] aligned HP output, a3=n. */
 .macro AMY_MAT8 name,bits
 .global \name
 .type \name,@function
\name:
 esp.zero.qacc
 srli t0,a3,3
1:
 esp.vld.128.ip q0,a0,16
 .if \bits == 16
 .irp lane,0,1,2,3,4,5,6,7
 esp.vld.128.ip q1,a1,16
 esp.vsmulas.s16.qacc q1,q0,\lane
 .endr
 .else
 esp.vldext.s8.ip q1,q2,a1,16
 esp.vsmulas.s16.qacc q1,q0,0
 esp.vsmulas.s16.qacc q2,q0,1
 esp.vldext.s8.ip q1,q2,a1,16
 esp.vsmulas.s16.qacc q1,q0,2
 esp.vsmulas.s16.qacc q2,q0,3
 esp.vldext.s8.ip q1,q2,a1,16
 esp.vsmulas.s16.qacc q1,q0,4
 esp.vsmulas.s16.qacc q2,q0,5
 esp.vldext.s8.ip q1,q2,a1,16
 esp.vsmulas.s16.qacc q1,q0,6
 esp.vsmulas.s16.qacc q2,q0,7
 .endif
 addi t0,t0,-1
 bnez t0,1b
 esp.st.qacc.l.l.128.ip a2,16
 esp.st.qacc.l.h.128.ip a2,16
 esp.st.qacc.h.l.128.ip a2,16
 esp.st.qacc.h.h.128.ip a2,16
 ret
 .size \name,.-\name
 .endm
 AMY_MAT8 amy_s31_mat8_16,16
 AMY_MAT8 amy_s31_mat8_8,8
 /* ESP-DL places compute kernels in cached .text. Keep the identical HP
  * instruction-storage variant above for measured placement comparison. */
 .text
 .align 4
 AMY_MAT8 amy_s31_mat8_16_cached,16
 AMY_MAT8 amy_s31_mat8_8_cached,8
#endif
