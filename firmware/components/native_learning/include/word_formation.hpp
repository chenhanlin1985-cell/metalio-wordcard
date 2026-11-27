#pragma once
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace wordcard {
struct WordPart {
    const char *spelling;
    const char *role;
    const char *meaning;

    std::string line() const {
        return std::string(role) + "    " + spelling + "    " + meaning;
    }
};

struct WordFormation {
    std::string formula;
    std::vector<WordPart> parts;
    std::string spelling_note;
    bool compound = false;

    std::string detail_text() const {
        std::string text;
        for (const auto &part : parts) text += "\n" + part.line();
        if (!spelling_note.empty()) text += "\n" + spelling_note;
        return text;
    }
};

namespace formation_data {
enum class PartRole : std::uint8_t { Base, Root, Prefix, Suffix };
struct PartData { const char *form, *meaning; PartRole role; };
struct Alias { const char *word, *target; };
struct Entry { const char *word; std::uint16_t offset; std::uint8_t count; };
#include "word_formation_data.inc"
} // namespace formation_data

inline WordFormation word_formation(std::string word) {
    using namespace formation_data;
    for (char &c : word) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    auto alias = std::lower_bound(std::begin(aliases), std::end(aliases), word,
        [](const Alias &row, std::string_view key) { return row.word < key; });
    const bool grouped = alias != std::end(aliases) && word == alias->word;
    if (grouped) word = alias->target;
    auto it = std::lower_bound(std::begin(entries), std::end(entries), word,
        [](const Entry &entry, std::string_view key) { return entry.word < key; });
    if (it == std::end(entries) || word != it->word) return {};

    WordFormation result;
    auto first = part_refs + it->offset;
    auto end = first + it->count;
    result.compound = std::count_if(first, end, [](std::uint16_t index) {
        return part_data[index].role == PartRole::Base;
    }) >= 2;
    std::string joined;
    result.parts.reserve(it->count);
    for (auto p = first; p != end; ++p) {
        const auto &part = part_data[*p];
        const char *role = "词基";
        switch (part.role) {
            case PartRole::Base: if (result.compound) role = "组成词"; break;
            case PartRole::Root: role = "词根"; break;
            case PartRole::Prefix: role = "前缀"; break;
            case PartRole::Suffix: role = "后缀"; break;
        }
        result.parts.push_back({part.form, role, part.meaning});
        if (!result.formula.empty()) result.formula += " + ";
        result.formula += part.form;
        for (char c : std::string_view(part.form)) if (c != '-' && c != ' ') joined += c;
    }
    std::string written;
    for (char c : word) if (c != '-' && c != ' ') written += c;
    if (joined != written) {
        result.spelling_note = joined + "e" == written ? "整词末尾还有字母 e。" :
            "上面列的是构词成分的原形，合起来拼作 " + word + "。";
    }
    if (grouped) result.spelling_note = "这里拆的是 " + word + "。" + result.spelling_note;
    return result;
}
} // namespace wordcard
