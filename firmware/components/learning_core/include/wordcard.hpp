#pragma once
#include <cstdint>
#include <cstdio>
#include <functional>
#include <istream>
#include <map>
#include <string>
#include <vector>

namespace wordcard {
struct Card {
    std::string id, front, back, hint, tags, group, ipa;
};
struct Progress {
    uint32_t due_day = 0, seen = 0, again = 0, hard = 0, good = 0;
    uint16_t ease = 250, interval = 0;
    uint8_t box = 0;
};
using ProgressMap = std::map<std::string, Progress>;
struct StudyCounts {
    size_t total = 0, learned = 0, unseen = 0, due = 0;
};
StudyCounts study_counts(const std::vector<Card> &cards, const ProgressMap &progress, uint32_t day,
                         const std::string &group = "");
using Row = std::vector<std::string>;
bool read_csv(std::istream &input, const std::function<bool(const Row &)> &consume,
              std::string &error);
std::string csv_escape(const std::string &text);
bool load_deck(std::istream &input, std::vector<Card> &output, std::string &error);
std::string reverse_prompt(const std::string &meaning);
enum class Rating { Again, Hard, Good };
void review(Progress &progress, Rating rating, uint32_t today);
bool load_progress(const std::string &path, ProgressMap &output, std::string &error);
bool save_progress(const std::string &path, const ProgressMap &progress, std::string &error);
bool atomic_write(const std::string &path, const std::string &bytes, std::string &error);
bool recover_backup(const std::string &path, std::string &error);
bool exists(const std::string &path);

class Session {
  public:
    void begin(const std::vector<Card> *cards, const ProgressMap &progress, uint32_t day,
               const std::string &group = "", bool difficult = false);
    const Card *current() const;
    void reveal() { revealed_ = true; }
    bool revealed() const { return revealed_; }
    bool rate(Rating rating, ProgressMap &progress, uint32_t today);
    size_t size() const { return queue_.size(); }
    size_t position() const { return position_; }
    bool finished() const { return position_ >= queue_.size(); }

  private:
    const std::vector<Card> *cards_ = nullptr;
    std::vector<size_t> queue_;
    size_t position_ = 0;
    bool revealed_ = false;
};

struct AudioEntry {
    std::string path, source_text;
    bool review_needed = false;
};
bool safe_relative_path(const std::string &path);
bool load_audio_index(std::istream &input, std::map<std::string, AudioEntry> &output,
                      std::string &error);
class WaveReader {
  public:
    WaveReader() = default;
    WaveReader(const WaveReader &) = delete;
    WaveReader &operator=(const WaveReader &) = delete;
    ~WaveReader() { close(); }
    bool open(const std::string &path, std::string &error);
    size_t read(int16_t *output, size_t max_samples);
    void close();
    uint32_t samples() const { return samples_; }
    uint32_t remaining() const { return remaining_; }

  private:
    FILE *file_ = nullptr;
    uint32_t samples_ = 0, remaining_ = 0;
};
} // namespace wordcard
