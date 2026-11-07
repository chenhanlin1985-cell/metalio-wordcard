Current production baseline: fix37a (2026-10-27). User ended speed experiments.
Only fixed Amy Small FP32 dual HP workers plus eSpeak fallback are built.
The previous original Amy, integer and PIE experiments are excluded from the
ELF. Six host and two device Small WAVs match fix36a byte-for-byte. Persistent
cache IDs and the 16kHz output pipeline are unchanged. Default normal speech
now uses SD -> existing Small/eSpeak cache -> Small -> eSpeak (30s deadline).
Current host entry: tools/speech_test.ps1. Legacy multi-engine test sources are
historical; reproduce them from the complete fix36a source ZIP, not this API.
See docs/2026-10-27-TTS生产方案收敛.md and outputs/tts-production-2026-10-27.

Latest device result: fix36a (2026-10-27), exact Small FP32 scheduling/kernel optimization.
See docs/2026-10-27-AmySmall-FP32瓶颈优化.md and outputs/amy-small-fp32-2026-10-27.
Small FP32 only: HP dual-worker tiled frontend, four-position arithmetic with
unchanged reduction order, output-channel/time tiles, parallel pointwise/FIR,
40ms yield interval (cancellation polling retained). Two existing HP stacks are
reused. Original Amy, integer modes, SD priority and eSpeak fallback remain.
Models, FP32 arithmetic, receptive fields, resampling coefficients and cache
identities are unchanged. Device daily/technology PCM matches fix35a bytewise.
Same final binary, profiling off, warm-model fresh synthesis:
daily 28.286328 -> 18.889822s; technology 34.375623 -> 23.281445s.
Scalar FPU loop unrolling is not claimed as FP32 SIMD arithmetic. PIE copy remains
an optional measured experiment, off by default. Normal learning remains
SD -> Nano -> eSpeak. No additional human listening acceptance is claimed.

Latest device result: fix35a (2026-10-27), exact Small context reuse and offline W16.
See docs/2026-10-27-AmySmall首次生成优化.md and outputs/amy-small-speed-2026-10-27.
Original Amy is preserved. Small FP32 stage0 reuse and receptive-field cropping
produce the identical old device PCM. Packed mode uses 23 immutable W16 layers,
per-output power-of-two scales and eight-channel SIMD ordering; dynamic A16,
exact64 accumulation and FP32 frontend/nonlinearities. Decoder blob SHA256:
53048510620bf886969c5f3a9c8e4558075a5c5714075cd84176e4cccadec55b.
Total model storage/residency 2,902,552 bytes, versus 4,340,872 bytes FP32;
runtime scratch/activation/cache allocations are separate. Packed output has a
separate cache identity. Measured daily baseline/reuse/Q16:
48.090/28.035/30.537s, technology:
58.552/34.092/36.886s.
Do not infer perceptual acceptance from numeric checks. Default learning remains
SD -> Nano -> eSpeak; Amy modes remain runtime-selectable experiments.

Latest device result: fix34a (2026-10-27), Amy Small added alongside original Amy.
See `docs/2026-10-27-AmySmall双核实测.md` and `outputs/amy-small-2026-10-27`.
Small public package is pinned at HF c532a5d21c078a16cb633718e9182bfd71a5b760,
amy-en-1p1m; all 108 tensors were hash-checked and widened exactly from FP16
storage to FP32. Runtime weights including metadata total 4,340,872 bytes.
Small uses duration hidden32 and decoder160/80/40/20; original assets stay intact.
Both use the existing dual FP32 kernels, chunk24, 12-frame halo, FIR and clocks.
Same-binary fresh synthesis: daily58.590→48.098s, tech77.669→58.521s. Small audio
lengths differ; speed is measured, not inferred from parameter count. Original
device WAVs are unchanged. Small device/host comparison and unapproved listening
status are recorded in the report. Model selection is lazy; only one Amy model
is resident, and all four modes have separate persistent audio-cache identities.

Latest device result: fix33n (2026-10-27), actual INT16 and mixed W8A16 integrated and flashed. Same-binary daily FP32/INT16/mixed compute 58.428/73.153/65.764 s; technology 77.708/96.483/86.909 s. Both integer modes are slower on these two sentences and remain experimental. Correcting arena selection restored 24-frame FP32, improving the prior technology result from 95.293 to 77.708 s without changing either accepted device WAV. Integer device/host comparisons have maximum 9 PCM LSB difference, not bit-exact; no new listening approval. Cache/reboot/cancellation and normal eSpeak fallback passed. Original model assets remain FP32; 640 KiB temporary workspace is additional. See INTEGER-NOTICE.md, docs/2026-10-27-Amy整数档S31实测.md and outputs/amy-integer-2026-10-27/device-result.json.

# Amy experimental S31 port

2026-10-27 integer extension: `amy_quant.c` and `amy_s31_quant.S` add actual
INT16 decoder and mixed residual W8A16 modes. See `INTEGER-NOTICE.md` for the
numeric contract and Espressif instruction references. The unchanged FP32 path
described below remains the quality baseline. Quantized layer buffers are
temporary, original model assets remain on SD, and numeric modes have distinct
cache identities. Device execution and speed are recorded separately in the
integer experiment report; host correctness is not a speed measurement.

Historical device result (2026-10-27, fix32e): daily warm compute 58.179 s versus fix31d 131.629 s (2.262x, 55.8% less time); cold prepare plus compute 63.016 s. Same-binary HP-stack single-core 104.302 s, dual-core 1.793x faster. PIE byte copy 58.235/58.225 s has no additional measured speed benefit and stays off by default. All daily modes have CRC916568839; final USB-readback WAV is byte-identical to fix31d/fix30c. Full technology sentence 95.293 s for 3.193 s speech, adaptive 16-frame block; 246 samples differ from host by 1 LSB (maximum 1 LSB), not a bit-exact host/device claim. Parallel PIE cancellation (906 ms observed upper bound), recovery, cache, USB reboot replay, and direct eSpeak regression passed. Host has 14168 kernel geometries, 6 PCM oracles, 161 G2P inputs, 69 Amy checks. Evidence: `outputs/amy-dual-2026-10-27/device-result.json`. No new listening approval or default learning engine change.

Pinned upstream: sanoTTS `f73b8382bc98a1c8de3488ba30d727ab82587828`.
`vendor/snt_front_f32.c,h`, `snt_piperlite.c,h` and `cp_id_tables_multi.h` are unchanged copies from `mcu/src` and `mcu/include`. Upstream LICENSE and LICENSE.MIT are retained. The original Amy blobs from `web/voices/amy` are external SD resources in `assets/amy-en-us`:

- front_f32.bin: 1816420 bytes, SHA256 `9bc304dd1db30b17a2efb751795137c6d26f25aaf9c96f05bc1dec7c86e0ce23`
- dec_f32.bin: 4001700 bytes, SHA256 `f520bcbf278607343e68170a06619a465a333d2d67476c6823d130e91580281b`

`kernels` is generated by `tools/amy_prepare.ps1`. The frontend retains cooperative cancellation. Decoder conv/transpose-conv use `tools/amy_conv_tiled.c.inc`: four output channels share each input read, preserving each output's input-channel/kernel summation order, padding, zero weights, tails and FP32 arithmetic. No fast-math, quantization or reassociation. In fix32d the two lanes atomically claim disjoint groups of four output channels, balancing work dynamically; the preceding fixed half/half experiment was slower. Each lane owns a separate aligned 2048-byte HP DRAM accumulator. For dense interior conv positions, two adjacent time positions keep eight accumulators in FPU registers for one input channel's K reduction, then commit to the tile. Padding, odd tails and zero-weight cases retain the original ordered tile updates. This avoids the earlier failed strategy of sweeping all input channels for each tiny time tile. Small ranges and unavailable-helper cases use the serial path.

`amy_parallel.c` implements two lazy persistent compute workers pinned to HP cores 1 and 0, each with a 4096-byte static HP stack. The existing audio task keeps its original stack and only coordinates/waits during decoder kernels. The single-core diagnostic also uses the core-1 HP worker, so both modes have the same stack placement. This avoids register spills and temporary accesses through the audio task's LP SRAM stack. Job context/input/output lifetimes extend through a completion semaphore. Cancellation and deadlines first publish an atomic stop flag and join the helper, then longjmp within C. Both compute workers poll the flag and return normally; neither calls a C++ cancellation callback or jumps into another task. Windows host tests run the same range kernels on two actual OS threads with equivalent event synchronization. 14168 geometries / 14286720 FP32 outputs and six full PCM oracles match unchanged upstream; a new cancellation test explicitly stops while a parallel job is pending and checks that it is joined before arena release.

The optional `amy_s31_copy.S` uses PIE 128-bit loads/stores only for exact byte copying on core 1. In fix32e the wrapper also requires the current task to be the dedicated core-1 HP-stack worker, besides checking core, alignment, complete blocks and HP/PSRAM address ranges; an on-device guard test checks eight lengths before enabling it. The serial final channel stays on memcpy. This task guard is essential: fix32d checked copied buffers but allowed the LP-stack audio task to use PIE, and a device reset PC identified FreeRTOS saving PIE context to that task's stack. Do not bypass lazy coprocessor ownership or mask interrupts to avoid it. It is not a vector FP32 multiply-accumulate kernel. Diagnostic runtime controls `tts amyopt single`, `tts amyopt dual`, `tts amyopt pie` are accepted only when idle; reboot defaults to dual with PIE copying off. Separate device timing/PCM results decide whether this extra memory-copy path helps.

The setjmp/longjmp boundary and every jumped-over frame remain C; C++ callbacks return before jumping and owning allocations stay above the guards. A single coordinating audio worker owns this non-reentrant engine; the compute workers may stay alive asleep, but cannot retain a job or arena after cancellation.

The full text frontend uses the device's existing eSpeak English data and upstream Amy codepoint mapping. The browser demo ships different dictionary/phontab files: its original raw IDs are not a valid same-data oracle. Tests run the unchanged original WASM G2P with the device's seven English resources; 161 inputs match. A separate executable links unchanged upstream float kernels and whole-utterance shim. Six complete text-to-PCM results match that oracle exactly after the same FIR. The earlier chunk-only oracle separately retained all six original demo WAV hashes. None of these host checks prove S31 PCM or listening quality.

The full duration/latent graph is retained. Decoder output uses 24 valid frames if the contiguous arena fits and total free memory retains 512 KiB after allocation, otherwise 16. Each side retains up to 12 context frames, cropping only context. Maximum scratch is 4804608 / 4003840 bytes respectively. No quantization, crossfade or invented boundary padding. Cancellation/failure drops unfinished PCM. The output is native 22050 Hz, converted to the board's actual 16000 Hz external I2S clock using 320-phase 129-tap Q30 FIR (6900 Hz cutoff, Kaiser beta 8.6). No audio module AT commands or clock changes.

Inactive Pico/Nano models are released before selecting Amy and before heap baselines. Model validation is lazy with existing metadata cache; it does not scan the SD at boot. Successful generated PCM uses a new Amy/version/filter-specific cache identity. SD audio remains first. Amy is an explicit experimental test mode, and runtime `tts engine amy` enables it for normal missing-audio text with eSpeak fallback; reboot restores Nano. Cancellation or playback failure cannot trigger a second engine after audio has started.

The diagnostic Amy mode allows up to 600 seconds and is labelled as a slow experiment; other diagnostic engines retain 120 seconds. Optional normal-learning Amy generation is bounded to 30 seconds before eSpeak fallback. Its PA remains off until PCM is ready. The default learning engine is not changed. First device attempts with a 120-second bound timed out without a complete sentence; memory-fit alone must not be described as practical instant TTS.

2026-10-27 S31 fix31d result: the same 38081-sample daily sentence took 131.522 s compute cold (4.631 s prepare) and 131.629 s compute warm, versus fix30c 280.028 s warm. Speedup 2.127x; 52.994% less compute time. USB readback WAV is byte-identical to fix30c (SHA256 601c5532472daa4b2543cd292d8005aced29b269521cbf5d62593132529da6f4). Cached replay, USB reboot replay, cancellation without persistence and subsequent forced regeneration passed. Evidence: outputs/amy-speed-2026-10-27/device-result.json. This remains slow experimental TTS, not instantaneous speech or a claim of exhausting S31 performance.
