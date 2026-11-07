#pragma once
#include "offline_tts.hpp"
#include <vector>
bool amy_phoneme_ids(OfflineTts &,const std::string &folder,const std::string &text,std::vector<int32_t> &,std::string &error);
