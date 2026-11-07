# Local QuickJS-NG patch

Base: v0.17.0, commit `6d46d07d04041b40f4f49eaa7fdebe44c314c699`.

`quickjs.c` / `quickjs.h` add `JS_SetInterruptInterval(ctx, interval)` and one
context-owned interval field. Zero/unset and invalid values retain the upstream
10,000-poll default. Only speculative page preparation selects 256; restoration
and foreground work restore 10,000. This is cooperative cancellation frequency,
not a change to JavaScript semantics, memory limits, or the five-second deadline.

The native host build in `tests/native_learning` compiles these exact vendored
sources, rather than linking a historical upstream-only `qjs.lib`.

S31's existing `-fno-strict-aliasing` compatibility flag is retained. No reference
checkout under `references/quickjs-ng` is modified.
