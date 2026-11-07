#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Experimental dark-control policy. Only select complete old->new sequences
 * from this panel's own GL16/GC16 tables; no EDC waveform bytes, new pulses,
 * deleted drive phases, or invented old colors. Both sources must be 48 phases.
 * Black includes equal-black recovery. White holds only when already white.
 * Intermediate target grays use GC16, including antialiased glyph edges.
 * Selection happens once before rendering, not in the scan-time pixel loop. */
static inline bool wordcard_make_control_waveform(const uint8_t *gl,unsigned gl_phases,
                                                  const uint8_t *gc,unsigned gc_phases,
                                                  uint8_t *out,size_t capacity) {
    if(!gl||!gc||!out||gl_phases!=48||gc_phases!=gl_phases||capacity<gl_phases*64u||
       out==gl||out==gc)return false;
    memset(out,0,gl_phases*64u);
    for(unsigned f=0;f<gl_phases;++f)for(unsigned to=0;to<16;++to)
        for(unsigned from=0;from<16;++from) {
            if(to==15&&from==15)continue;
            const uint8_t *source=(to==0||to==15)?gl:gc;
            const unsigned shift=6-2*(from%4),offset=(f*16+to)*4+from/4;
            out[offset]|=(uint8_t)(source[offset]&(3u<<shift));
        }
    return true;
}
