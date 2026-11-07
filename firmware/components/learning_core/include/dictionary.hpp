#pragma once
#include "wordcard.hpp"
namespace wordcard {
struct DictionaryEntry {
    std::string word, phonetic, translation, definition, exchange;
    uint32_t index = 0;
    bool supplemental = false;
    bool course_note = false;
};
struct DictionaryMatch { std::string word; uint32_t index = 0; std::string meaning; };
class Dictionary {
  public:
    ~Dictionary();
    bool open(const std::string &folder, std::string &error);
    void close();
    bool ready() const { return index_ && data_; }
    uint32_t count() const { return count_; }
    bool search(const std::string &, std::vector<DictionaryMatch> &, std::string &error);
    bool read(uint32_t index, DictionaryEntry &, std::string &error);
  private:
    FILE *index_ = nullptr, *data_ = nullptr;
    uint32_t count_ = 0, data_size_ = 0;
    bool row(uint32_t, std::vector<std::string> &, std::string &error);
};
std::string dictionary_key(const std::string &word);
// Collapse whitespace without losing acronym case (IT and it are different words).
std::string dictionary_query_text(const std::string &word);
int dictionary_exact_match(const std::string &, const std::vector<DictionaryMatch> &);
std::string personal_word_id(const std::string &word);
Card dictionary_card(const DictionaryEntry &);
bool add_personal_word(const std::string &path, const Card &, bool &already_added, std::string &error);
// Replace only after a complete, valid read; an interrupted refresh keeps the current lesson usable.
bool reload_personal_words(const std::string &path, std::vector<Card> &, std::string &error);
enum class VocabularyFilter { All, Due, New };
struct VocabularyItem {
    size_t index = 0;
    std::string word, meaning, status;
};
struct VocabularyPage {
    size_t total = 0, due = 0, fresh = 0, matching = 0, pages = 1;
    uint32_t next_day = 0;
    int page = 0;
    std::vector<VocabularyItem> items;
};
bool vocabulary_matches(const Card &, const ProgressMap &, uint32_t day, VocabularyFilter);
VocabularyPage vocabulary_page(const std::vector<Card> &, const ProgressMap &, uint32_t day,
                               VocabularyFilter, int page);
} // namespace wordcard
