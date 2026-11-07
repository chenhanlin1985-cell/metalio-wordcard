#pragma once
#include <cstddef>
#include <cstdint>
struct AmyAsset {const char *name;size_t bytes;uint32_t crc;};
inline constexpr AmyAsset AMY_ASSETS[]={{"front_f32.bin",1816420,162140191u},{"dec_f32.bin",4001700,3210913339u}};
inline constexpr AmyAsset AMY_SMALL_ASSETS[]={{"front_f32.bin",1417572,546443547u},{"dec_f32.bin",2923300,1630613947u}};
inline constexpr AmyAsset AMY_SMALL_PACKED_ASSETS[]={{"front_f32.bin",1417572,546443547u},{"dec_q16.bin",1484980,3219730198u}};
