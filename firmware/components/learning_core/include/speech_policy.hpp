#pragma once
#include <cstdint>
#include <functional>
#include <string>
namespace wordcard {
enum class SpeechSource { None, Sd, Synthesis };
// Plain text only; reject unsupported/control input instead of silently cutting it.
bool speech_text(const std::string &input, std::string &output);
SpeechSource speech_source(bool sd_valid, bool tts_ready, const std::string &text);
class SpeechResampler {
  public:
    explicit SpeechResampler(unsigned input_rate) : rate_(input_rate) {}
    bool push(const int16_t *, size_t, const std::function<bool(const int16_t *, size_t)> &);
    uint64_t samples() const { return emitted_; }
  private:
    unsigned rate_;
    uint64_t index_ = 0, next_ = 0, emitted_ = 0;
    int16_t previous_ = 0;
};
} // namespace wordcard
