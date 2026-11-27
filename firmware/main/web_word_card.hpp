#pragma once
#include "learning_web.hpp"
#include "../components/native_learning/include/word_formation.hpp"

namespace wordcard {
inline const WebBlock *word_card_head(const WebPage &page) {
    for (const auto &block : page.blocks)
        if (block.role == "card-word") return &block;
    return nullptr;
}

inline const WebBlock *card_example(const WebPage &page, int index) {
    if (page.keyboard || index < 0 || size_t(index) >= page.blocks.size()) return nullptr;
    const auto &b = page.blocks[size_t(index)];
    return b.role == "card-example" && !b.text.empty() ? &b : nullptr;
}

inline WebBlock example_control(size_t index, const std::string &area, bool replay) {
    WebBlock button{"button", replay ? "再听一遍" : "听例句", area};
    button.role = "example-audio";
    button.action = int(index);
    return button;
}

// Keep the original block index so a multi-word lesson plays the tapped sentence.
inline void project_example_controls(WebPage &page) {
    for (size_t i = 0; i < page.blocks.size(); ++i)
        if (card_example(page, int(i)))
            page.blocks[i] = example_control(i, page.blocks[i].area, false);
}

inline WebBlock card_line(const std::string &text, const std::string &role) {
    WebBlock b{"text", text, "body"};
    b.role = role;
    return b;
}

// Only a revealed lexical entry supplies card-word. Question prompts and book
// titles also use large type, but must never acquire hints or an answer face.
inline const char *layout_word_card(WebPage &page, bool reverse) {
    if (!reverse) project_example_controls(page);
    const auto *head = word_card_head(page);
    if (!head || page.keyboard) return nullptr;
    const auto parts = word_formation(head->text);
    const bool notes = std::any_of(page.blocks.begin(), page.blocks.end(),
        [](const WebBlock &b) { return b.role == "card-root"; });
    const bool has_details = !parts.parts.empty() || head->picture >= 0 ||
        std::any_of(page.blocks.begin(), page.blocks.end(), [](const WebBlock &b) {
            return b.role == "detail" || b.role == "card-root";
        });
    const char *label = !parts.parts.empty() || notes ? "词根卡" : "词卡详情";
    std::vector<WebBlock> out;
    auto title = *head;
    title.role = "word";
    title.picture = -1;
    out.push_back(std::move(title));
    if (reverse) {
        page.title = label;
        page.top.clear();
        page.tabs.clear();
        for (const auto &b : page.blocks)
            if (b.role == "meaning") {out.push_back(b); break;}
        for (const auto &part : parts.parts)
            out.push_back(card_line(part.line(), "card-part"));
        if (!parts.spelling_note.empty())
            out.push_back(card_line(parts.spelling_note, "source"));
        for (auto b : page.blocks) {
            if (b.role != "detail" && b.role != "card-root") continue;
            b.role = "reader-copy";
            out.push_back(std::move(b));
        }
    } else {
        for (const auto &b : page.blocks)
            if (b.role == "ipa" || b.role == "meaning") out.push_back(b);
        for (const auto &part : parts.parts)
            out.push_back(card_line(part.line(), "card-part"));
        if (parts.parts.empty())
            for (const auto &b : page.blocks)
                if (b.role == "card-root") out.push_back(card_line(b.text, "card-part"));
        for (const auto &b : page.blocks) {
            if (b.role != "card-word" && b.role != "ipa" && b.role != "meaning" &&
                b.role != "detail" && b.role != "card-root") out.push_back(b);
        }
    }
    if (head->picture >= 0) {
        auto illustration = *head;
        illustration.text.clear(); illustration.detail.clear();
        illustration.role = reverse ? "picture" : "card-illustration";
        illustration.kind = "text";
        out.push_back(std::move(illustration));
    }
    page.blocks = std::move(out);
    return has_details ? label : nullptr;
}
} // namespace wordcard
