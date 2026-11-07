#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include "resource_cache.hpp"
class OfflineTts {
  public:
    using Sink = std::function<bool(const int16_t *,size_t)>;
    using Cancel = std::function<bool()>;
    static bool assets_available(const std::string &folder);
    static bool assets_present(const std::string &folder);
    bool prepare(const std::string &folder, std::string &error);
    bool phonemes(const std::string &folder, const std::string &text, std::string &ipa, std::string &error);
    // One owning audio worker. The eSpeak runtime is deliberately never called concurrently.
    bool synthesize(const std::string &folder,const std::string &text,const Sink &,const Cancel &,std::string &error);
    bool failed() const { return failed_; }
    bool initialized() const { return initialized_; }
  private:
    bool initialized_ = false, failed_ = false;
    wordcard::ResourceCache assets_cache_;
    std::string cache_folder_;
};
