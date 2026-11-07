#pragma once
#include <cstdint>
namespace wordcard {
inline bool audio_result_matches(uint32_t received,uint32_t expected,bool playing,int kind){return received!=0&&received==expected&&playing&&(kind==1||kind==2||kind==3);}
}
