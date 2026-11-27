#pragma once
#include <string>
#include <vector>
namespace wordcard {
// Kept as the device Canvas contract; no DOM, HTML or script types.
struct WebBlock {
    std::string kind, text, area;
    int action = -1;
    bool enabled = true, selected = false, primary = false;
    std::string role = {}, detail = {};
    int picture = -1;
    std::string picture_source = {};
    int picture_width=160,picture_height=160;
    std::string example_audio = {}; // SD-relative recording for a card-example block.
};
struct WebControl {
    std::string text;
    int action = -1;
    bool selected = false, enabled = true;
};
struct WebPage {
    std::string title, typed, prompt, route, key, toast, error, speech_note, gap_mask;
    std::vector<WebBlock> blocks;
    std::vector<WebControl> top, tabs;
    std::vector<std::string> actions;
    bool secondary=false; // Explicit More Learning scope, including prepared frames.
    bool keyboard = false, shift = false, symbols = false, can_submit = false, corrupt = false;
    int home_new_words=0,home_new_target=0;
};
} // namespace wordcard
