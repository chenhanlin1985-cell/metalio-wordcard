#pragma once
#include "epd_internals.h"
const EpdWaveform *wordcard_text_waveform();
// Null means unavailable: caller must explicitly fall back to a complete GC16.
const EpdWaveformPhases *wordcard_control_phases();
