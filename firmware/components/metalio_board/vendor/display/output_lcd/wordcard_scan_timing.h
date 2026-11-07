#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Timing constraints informed by Kiikoread's Apache-2.0 scan solver at
 * 108402737d1fc0f28060b13c4bcc3cbe884b93d9. Recompute for S31's 8-bit bus;
 * never copy the S3 16-bit line-data length. Optical validation is required.
 * This profile retains the board's 20 MHz clock and existing voltage. */
typedef struct {
    int start_clocks, back_clocks, data_clocks, end_clocks;
    int line_clocks, line_us, gate_high_01us, gate_low_01us;
} WordcardScanTiming;

static inline bool wordcard_scan_solve(unsigned pclk_hz, int pixels, int bus_width,
                                      WordcardScanTiming *out) {
    if (!out || pclk_hz < 12000000 || pclk_hz > 24000000 || pclk_hz % 1000000 ||
        pixels <= 0 || pixels > 4096 || (bus_width != 8 && bus_width != 16) ||
        pixels % (bus_width / 2)) return false;
    const int mhz = (int)(pclk_hz / 1000000);
    const int start = (3 * mhz + 9) / 10; // >=0.3 us
    const int back = (8 * mhz + 9) / 10;  // >=0.8 us
    const int data = pixels / (bus_width / 2);
    int line_us = (start + back + data + 4 + mhz - 1) / mhz;
    if (line_us < 16) line_us = 16;
    const int total = line_us * mhz;
    int gate = ((start + back + data) * 10 + mhz - 1) / mhz;
    if (gate < 134) gate = 134;
    if (gate > line_us * 10 - 12) gate = line_us * 10 - 12;
    *out = (WordcardScanTiming){start, back, data, total-start-back-data,
                               total, line_us, gate, line_us*10-gate};
    return true;
}
