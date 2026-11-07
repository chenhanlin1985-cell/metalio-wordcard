# Amy integer decoder

Project implementation, Apache-2.0. The original Amy model/frontend/decoder
remain subject to the existing vendor/model notices.

The integer dot assembly uses PIE instruction semantics documented by Espressif:

- https://github.com/espressif/esp-dl/blob/master/esp-dl/dl/base/isa/esp32p4/dl_esp32p4_dotprod.S
- https://github.com/espressif/esp-dl/blob/master/esp-dl/dl/base/isa/esp32p4/dl_esp32p4_w8a16_conv2d.S
- https://github.com/espressif/esp-dl/blob/master/tools/agents/skills/esp32p4-pie-simd/references/instructions.md

Consulted 2026-10-27. These are instruction references, not a claim that the
whole ESP-DL runtime or its graph format is deployed. Our loop has no speculative
tail loads. The scalar-dot self-test splits XACC into blocks of at most 256
signed 16-bit products and uses the full signed store, avoiding MOVX extraction
differences. The production matrix kernel calculates eight output channels in
64-bit QACC lanes. Device tests check all eight lanes against scalar int64,
including extreme signed values and accumulations beyond 40 bits.

Regular convolutions reuse a 4 KiB HP SRAM weight tile over 16 time positions.
The SRAM belongs to the FP32 tile workspace and is reused only after both
workers join. Partial integer sums are combined exactly before one IEEE
round-to-nearest-even conversion to float. Core 0 processes separate tiles
with scalar int64; it never executes PIE instructions. Dynamic scheduling
balances the asymmetric workers. This is memory blocking, not a new model.

The shared layer descriptor is explicitly in HP SRAM. Keeping it on the audio
coordinator's LP stack made repeated worker field accesses dominate the kernel.
The SIMD worker claims 16 positions and the scalar worker one position at a
time; both are joined before reuse. Activation layout conversion is blocked
over 32 time positions. The S31 FPU's explicit RNE float-to-int instruction
is checked against the software ties-even reference on 69,630 finite inputs,
including every half-integer in the activation range; failures retain the
software reference. None of these changes alter the numeric cache identity.

INT16 quantizes all 23 decoder convolution tensors, including the three
transposed convolutions. Mixed uses 8-bit weights and 16-bit activations only
for the 18 residual convolutions. Other layers stay FP32. Both use per-output-
channel power-of-two weight scales, dynamic per-tensor activation scales and
explicit ties-to-even. Bias, residual arithmetic, nonlinearities, frontend and
22050-to-16000 FIR remain FP32/original as applicable. This is not static PTQ,
TQT training, or an all-integer graph.

640 KiB bounded PSRAM workspace is owned above the C cancellation guard.
It is allocated before the adaptive 24/16-frame decoder choice. The selector
checks contiguous arena space separately from total free memory, with 512 KiB
reserved for other work. The previous contiguous-arena-plus-reserve check
unnecessarily reduced even FP32 to 16 frames after test-history placement changed.
Cancellation joins both HP-stack workers before releasing it. PIE is permitted
only in the core-1 dedicated worker; core 0 and serial fallback use exact int64.
On-device overflow/extreme tests fail closed to scalar integer execution.

Models on SD remain the original validated FP32 assets. Weights are packed per
layer into temporary integer buffers. Thus this first implementation does not
yet reduce persistent model storage or model resident RAM. Conversion cost is
included in measured generation time. Audio caches have separate precision and
numeric-version identities; unchanged FP32 cache keys are retained.
