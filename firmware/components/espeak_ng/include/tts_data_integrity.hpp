#pragma once
#include <cstdint>
struct TtsDataFile {const char *name; uint32_t bytes,crc;};
inline constexpr TtsDataFile TTS_DATA_FILES[]={
{"en_dict",166916,0x1f55c9cau},
{"intonations",2040,0xc6caa14du},
{"lang/gmw/en",148,0x6c58c9e3u},
{"lang/gmw/en-US",272,0xd86af2e3u},
{"phondata",550424,0xd86ea5cu},
{"phonindex",39062,0xf84d3b59u},
{"phontab",55764,0x73d60dc3u},
};
