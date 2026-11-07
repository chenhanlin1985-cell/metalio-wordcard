#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
// Equal-gray hold policy informed by Kiikoread's Apache-2.0
// display_main_gray_waveform at 108402737d1fc0f28060b13c4bcc3cbe884b93d9.
// Keep every changed-pixel sequence from this board's factory GL16 table.
// The DU endpoint substitution left visible ghosts on the connected panel.
namespace wordcard {
inline unsigned wave_action(const uint8_t *table,unsigned phase,unsigned to,unsigned from) {
    return (table[(phase*16+to)*4+from/4] >> (6-2*(from%4))) & 3;
}
inline bool make_text_waveform(const uint8_t *gray,unsigned gray_phases,
                              uint8_t *out,size_t capacity) {
    if(!gray || !out || !gray_phases || gray_phases>64 ||
       capacity<gray_phases*64)return false;
    std::memset(out,0,gray_phases*64);
    for(unsigned f=0;f<gray_phases;++f)for(unsigned to=0;to<16;++to)
        for(unsigned from=0;from<16;++from) {
            unsigned action=0;
            if(to!=from)action=wave_action(gray,f,to,from);
            out[(f*16+to)*4+from/4]|=uint8_t(action << (6-2*(from%4)));
        }
    return true;
}
constexpr unsigned text_cleanup_interval=12;
}
