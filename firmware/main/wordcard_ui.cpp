#include "wordcard_ui.hpp"
#include "dictionary_keyboard.hpp"
#include "web_input_field.hpp"
#include "shutdown_card.hpp"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <new>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif
namespace wordcard {
static uint16_t u16(const uint8_t *p) {
    return p[0] | uint16_t(p[1]) << 8;
}
static uint32_t u32(const uint8_t *p) {
    return u16(p) | uint32_t(u16(p + 2)) << 16;
}
static uint64_t u64(const uint8_t *p) {
    return u32(p) | uint64_t(u32(p + 4)) << 32;
}
bool Font::init(const uint8_t *p, size_t size) {
    cache_.clear();
    raster_.reset();
    data_ = nullptr;
    if (!p || size < 32 || std::memcmp(p, "EFNT", 4) || u16(p + 4) != 1)
        return false;
    const auto count = u32(p + 8);
    const auto index = u64(p + 12), start = u64(p + 20);
    if (index < 32 || count > 100000 || index + uint64_t(count) * 16 > start || start > size)
        return false;
    uint64_t previous = 0;
    for (uint32_t i = 0; i < count; ++i) {
        auto entry = p + index + size_t(i) * 16;
        const auto key = u64(entry);
        const auto off = u32(entry + 8);
        const auto n = u16(entry + 12);
        if ((i && key <= previous) || off < start || n < 10 || uint64_t(off) + n > size)
            return false;
        previous = key;
        const auto g = p + off;
        const uint32_t bpp = uint32_t(key & 65535);
        if (bpp != 1 && bpp != 2 && bpp != 4)
            return false;
        if (10 + (uint64_t(u16(g)) * u16(g + 2) * bpp + 7) / 8 > n)
            return false;
    }
    data_ = p;
    size_ = size;
    count_ = count;
    index_ = index;
    return true;
}
Font::Glyph Font::glyph(uint32_t codepoint, uint16_t font_size) const {
    Glyph out;
    if (!data_)
        return out;
    auto missing = [&]() -> Glyph {
        if (font_size != 30) return glyph(codepoint);
        if (fallback_) {
            const auto extra = fallback_->glyph(codepoint);
            if (extra.bitmap) return extra;
        }
        return codepoint == '?' ? out : glyph('?');
    };
    const uint64_t key = uint64_t(codepoint) << 32 | uint64_t(font_size) << 16 | 2;
    auto cached = cache_.find(key);
    if (cached != cache_.end())
        return cached->second;
    uint32_t lo = 0, hi = count_;
    while (lo < hi) {
        auto mid = lo + (hi - lo) / 2;
        if (u64(data_ + index_ + size_t(mid) * 16) < key)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == count_)
        return missing();
    auto e = data_ + index_ + size_t(lo) * 16;
    if (u64(e) != key)
        return missing();
    auto p = data_ + u32(e + 8);
    out.width = u16(p);
    out.height = u16(p + 2);
    out.x = int16_t(u16(p + 4));
    out.y = int16_t(u16(p + 6));
    out.advance = u16(p + 8);
    out.font_size = font_size;
    out.bitmap = p + 10;
    if (cache_.size() >= 512)
        cache_.clear();
    cache_[key] = out;
    return out;
}
static uint32_t next(const std::string &s, size_t &i) {
    unsigned char c = s[i++];
    if (c < 128)
        return c;
    int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
    uint32_t cp = c & ((1 << (6 - n)) - 1);
    if (i + size_t(n) > s.size())
        return 0xFFFD;
    while (n--) {
        auto b = static_cast<unsigned char>(s[i++]);
        if ((b & 0xC0) != 0x80)
            return 0xFFFD;
        cp = (cp << 6) | (b & 63);
    }
    return cp;
}
void Canvas::clear() {
    std::memset(fb_, 255, bytes);
}
void Canvas::pixel(int x, int y, bool ink) {
    gray(x,y,ink?0:15);
}
void Canvas::gray(int x,int y,uint8_t level) {
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;
    const int px = y, py = width - 1 - x;
    auto &b = fb_[(py * 1216 + px) / 2];
    const uint8_t mask = (px & 1) ? 0xF0 : 0x0F;
    b=uint8_t((b & ~mask) | ((level & 15) << ((px & 1)*4)));
}
uint8_t Canvas::shade(int x,int y) const {
    if(x<0 || x>=width || y<0 || y>=height)return 15;
    const int px=y,py=width-1-x;
    return (fb_[(py*1216+px)/2] >> ((px&1)*4)) & 15;
}
bool Canvas::black(int x, int y) const {
    const int px = y, py = width - 1 - x;
    return ((fb_[(py * 1216 + px) / 2] >> ((px & 1) * 4)) & 15) < 8;
}
void Canvas::fill(Rect r, bool ink) {
    if (r.w <= 0 || r.h <= 0) return;
    const int x0 = std::max(0, r.x), y0 = std::max(0, r.y);
    const int x1 = int(std::min<int64_t>(width, int64_t(r.x) + r.w));
    const int y1 = int(std::min<int64_t>(height, int64_t(r.y) + r.h));
    if (x0 >= x1 || y0 >= y1) return;
    const uint8_t packed = ink ? 0 : 255;
    // Logical columns are contiguous physical scanlines after the 90° rotation.
    // Keep each edge nibble; bulk-write only the completely covered bytes.
    for (int x = x0; x < x1 && !cancelled(); ++x) {
        auto *row = fb_ + (width - 1 - x) * (height / 2);
        int begin = y0;
        if (begin & 1) { row[begin / 2] = uint8_t((row[begin / 2] & 15) | (packed & 240)); ++begin; }
        const int end = y1 & ~1;
        if (end > begin) std::memset(row + begin / 2, packed, size_t(end - begin) / 2);
        if ((y1 & 1) && begin < y1) row[y1 / 2] = uint8_t((row[y1 / 2] & 240) | (packed & 15));
    }
}
void Canvas::border(Rect r, int n) {
    fill({r.x, r.y, r.w, n});
    fill({r.x, r.y + r.h - n, r.w, n});
    fill({r.x, r.y, n, r.h});
    fill({r.x + r.w - n, r.y, n, r.h});
}
int Canvas::text_width(const std::string &s, int scale) const {
    int result = 0;
    for (size_t i = 0; i < s.size();) {
        const auto g = font_.glyph(next(s, i), uint16_t(30 * scale));
        result += g.advance * (g.font_size == 30 * scale ? 1 : scale);
    }
    return result;
}
void Canvas::text(int x, int y, const std::string &s, int scale, bool ink) {
    for (size_t i = 0; i < s.size() && !cancelled();) {
        auto cp = next(s, i);
        auto g = font_.glyph(cp, uint16_t(30 * scale));
        const int factor = g.font_size == 30 * scale ? 1 : scale;
        if (!g.bitmap && cp != ' ')
            border({x, y + 5 * scale, 14 * scale, 22 * scale});
        else
            for (int gx = 0; gx < g.width; ++gx)
                for (int gy = 0; gy < g.height; ++gy) {
                    auto bit = (gy * g.width + gx) * 2;
                    auto alpha = (g.bitmap[bit / 8] >> (6 - bit % 8)) & 3;
                    if(alpha)for(int sy=0;sy<factor;++sy)for(int sx=0;sx<factor;++sx) {
                        const int px=x+(g.x+gx)*factor+sx;
                        const int py=y+32*scale+(-g.y-g.height+gy)*factor+sy;
                        const int old=shade(px,py),target=ink?0:15;
                        gray(px,py,uint8_t((old*(3-alpha)+target*alpha+1)/3));
                    }
                }
        x += g.advance * factor;
    }
}
void Canvas::centered(int y, const std::string &s, int scale, bool ink) {
    text((width - text_width(s, scale)) / 2, y, s, scale, ink);
}
std::string Canvas::ellipsize(const std::string &s, int max_width, int scale) const {
    if (text_width(s, scale) <= max_width)
        return s;
    const std::string suffix = "…";
    const int available = max_width - text_width(suffix, scale);
    if (available < 0)
        return "";
    int used = 0;
    size_t end = 0;
    for (size_t i = 0; i < s.size();) {
        const size_t start = i;
        const auto cp = next(s, i);
        const auto g = font_.glyph(cp, uint16_t(30 * scale));
        const int w = g.advance * (g.font_size == 30 * scale ? 1 : scale);
        if (used + w > available) {
            end = start;
            break;
        }
        used += w;
        end = i;
    }
    return s.substr(0, end) + suffix;
}
std::vector<std::string> Canvas::wrap(const std::string &s, int max, int scale) const {
    std::vector<std::string> lines;
    std::string line;
    int used = 0;
    // Keep Latin words, contractions, hyphenated terms and IPA runs together.
    // Chinese remains breakable per character; an overlong token has a UTF-8-safe fallback.
    auto flush = [&]() {
        while (!line.empty() && line.back() == ' ') line.pop_back();
        lines.push_back(line); line.clear(); used = 0;
    };
    auto word_char = [](uint32_t cp) {
        return cp > 32 && cp < 0x3000;
    };
    for (size_t i = 0; i < s.size();) {
        auto start = i;
        auto cp = next(s, i);
        if (cp == '\n') {
            flush();
            continue;
        }
        if (cp == '\r') continue;
        if (cp == ' ' || cp == '\t') {
            const int w = text_width(" ", scale);
            if (!line.empty() && used + w <= max) { line += ' '; used += w; }
            else if (!line.empty()) flush();
            continue;
        }
        if (word_char(cp)) {
            while (i < s.size()) {
                size_t end = i; const auto next_cp = next(s, end);
                if (!word_char(next_cp)) break;
                i = end;
            }
        }
        const auto token = s.substr(start, i - start);
        const int token_width = text_width(token, scale);
        if (used + token_width > max && !line.empty()) flush();
        if (token_width <= max) { line += token; used += token_width; continue; }
        for (size_t j = 0; j < token.size();) {
            size_t begin = j; next(token, j);
            auto piece = token.substr(begin, j - begin);
            const int w = text_width(piece, scale);
            if (used + w > max && !line.empty()) flush();
            line += piece; used += w;
        }
    }
    if (!line.empty() || lines.empty())
        lines.push_back(line);
    return lines;
}
#include "device_type.inc"
static void strong(Canvas &c, int x, int y, const std::string &s, int scale = 1, bool ink = true) {
    c.text(x, y, s, scale, ink);
    c.text(x + 1, y, s, scale, ink);
}
static std::string fit_px(Canvas &c, const std::string &s, int width, int pixels) {
    const auto lines = c.wrap_px(s, width, pixels);
    if (lines.size() <= 1) return s;
    std::string out = lines.front();
    while (!out.empty() && c.text_width_px(out + "…", pixels) > width) {
        size_t end = out.size() - 1;
        while (end > 0 && (static_cast<unsigned char>(out[end]) & 0xC0) == 0x80) --end;
        out.resize(end);
    }
    return out + "…";
}
void UI::button(Canvas &c, Rect r, Action a, const std::string &label, int value, bool primary,
                bool enabled) {
    buttons_.push_back({r, {a, value}, label, enabled, primary});
    c.fill(r, false);
    c.border(r, enabled ? (primary ? 5 : 3) : 1);
    // These two existing narrow keyboard keys must retain their full labels.
    // Keep their geometry; use the previously accepted 30 px functional text.
    const int pixels = (a == Action::DictionaryShift || a == Action::DictionaryErase) ? 30 : 36;
    const int line_height = pixels + 10;
    auto lines = c.wrap_px(label, r.w - 20, pixels);
    const size_t limit = std::max(1, r.h / line_height);
    if (lines.size() > limit) {
        lines.resize(limit);
        lines.back() = fit_px(c, lines.back() + "…", r.w - 20, pixels);
    }
    const int top = r.y + (r.h - int(lines.size()) * line_height) / 2;
    for (size_t i = 0; i < lines.size(); ++i) {
        const int x = r.x + (r.w - c.text_width_px(lines[i], pixels)) / 2;
        const int y = top + int(i) * line_height;
        c.text_px(x, y, lines[i], pixels);
        if (primary && enabled) c.text_px(x + 1, y, lines[i], pixels);
    }
}
Event UI::hit(int x, int y) const {
    if(keyboard_)return dictionary_keyboard_hit(x,y,keyboard_shift_,keyboard_symbols_,keyboard_search_,keyboard_recent_,keyboard_recall_).event;
    // Preserve exact hits first. The small outside tolerance compensates for
    // finger roll without allowing a nearby control to steal an exact hit.
    for (const auto &b : buttons_)
        if (b.enabled && b.rect.contains(x, y))
            return b.event;
    constexpr int slop = 10;
    const Button *nearest = nullptr;
    int nearest_distance = slop * slop + 1;
    bool ambiguous = false;
    for (const auto &b : buttons_) {
        if (!b.enabled) continue;
        const int dx = x < b.rect.x ? b.rect.x - x : x >= b.rect.x + b.rect.w ? x - b.rect.x - b.rect.w + 1 : 0;
        const int dy = y < b.rect.y ? b.rect.y - y : y >= b.rect.y + b.rect.h ? y - b.rect.y - b.rect.h + 1 : 0;
        const int distance = dx * dx + dy * dy;
        if (distance > slop * slop) continue;
        if (distance < nearest_distance) {
            nearest = &b;
            nearest_distance = distance;
            ambiguous = false;
        } else if (distance == nearest_distance && nearest &&
                   (b.event.action != nearest->event.action || b.event.value != nearest->event.value)) {
            ambiguous = true;
        }
    }
    return nearest && !ambiguous ? nearest->event : Event{};
}
static void chevron(Canvas &c, int x, int y, bool ink = true) {
    for (int i = 0; i < 13; ++i) {
        c.fill({x + i, y - 12 + i, 3, 3}, ink);
        c.fill({x + i, y + 12 - i, 3, 3}, ink);
    }
}
void UI::tabs(Canvas &c, Action selected) {
    c.fill({40, 1110, 604, 2});
    const Action actions[] = {Action::Home, guided_enabled_?Action::GuidedCourses:Action::Library, Action::Practice, Action::Profile};
    const char *labels[] = {more_learning_scope_?"返回背词":"学习", guided_enabled_?"课程":"词书", "练习", more_learning_scope_?"设备":"我的"};
    for (int i = 0; i < 4; ++i) {
        Rect rect{24 + i * 159, 1116, 159, 90};
        buttons_.push_back({rect, {actions[i]}, labels[i]});
        const int x = rect.x + (rect.w - c.text_width(labels[i])) / 2;
        if (selected == actions[i]) {
            strong(c, x, 1140, labels[i]);
            c.fill({x, 1190, c.text_width(labels[i]), 4});
        } else {
            c.text(x, 1140, labels[i]);
        }
    }
}
void UI::row(Canvas &c, Rect rect, Action action, const std::string &label, int value) {
    buttons_.push_back({rect, {action, value}, label});
    c.text_px(rect.x, rect.y + (rect.h - 46) / 2, fit_px(c, label, rect.w - 40, 36), 36);
    chevron(c, rect.x + rect.w - 18, rect.y + rect.h / 2);
}
void UI::home(Canvas &c, const View &v) {
    strong(c, 40, 32, "小小单词卡");
    const std::string date = v.date_label.empty() ? "未设日期" : v.date_label;
    c.text(644 - c.text_width(date), 32, date);
    c.fill({40, 98, 604, 2});

    strong(c, 40, 144, v.more_learning_scope?"词书速记":"今日学习", 2);
    button(c,{420,132,224,88},Action::Dictionary,"查词");
    const bool complete = v.review_due == 0 && v.planned_new == 0;
    c.text(40, 240,
           v.resume_available ? "上次的小节还在，接着学就好。"
           : complete         ? "今日计划完成，也可以自由练习。"
                      : "每节 " + std::to_string(v.lesson_size) + " 题起，先复习再学新词。");
    c.text(40, 316, "到期复习");
    c.text(370, 316, "今日新词余量");
    strong(c, 40, 354, std::to_string(v.review_due), 3);
    strong(c, 370, 354, std::to_string(v.planned_new), 3);
    c.fill({330, 320, 2, 124});

    const Rect primary{40, 486, 604, 112};
    const std::string label = v.resume_available ? "继续这一节"
                              : complete         ? "去练习"
                                                 : "开始这一节";
    const Action primary_action = v.resume_available ? Action::Resume
                                  : complete         ? Action::Practice
                                                     : Action::Start;
    buttons_.push_back({primary, {primary_action}, label, true, true});
    c.fill(primary, false);
    c.border(primary, 5);
    strong(c, 40 + (604 - c.text_width(label)) / 2, 522, label);
    c.fill({584, 541, 28, 3});
    chevron(c, 600, 542);

    const std::string book = v.book_name.empty() ? "尚未选择词书" : v.book_name;
    row(c, {40, 618, 604, 88}, Action::Library, book);
    row(c, {40, 708, 604, 88}, Action::Groups, v.group.empty() ? "选择课程 / 主题" : v.group);
    const size_t learned = v.course.total ? v.course.learned : v.learned;
    const size_t total = v.course.total ? v.course.total : v.total;
    c.text(40, 804, (v.course.total ? "本课已学 " : "词书已学 ") + std::to_string(learned) +
                      " / " + std::to_string(total) + " 项");
    c.fill({40, 860, 604, 2});
    if (total && learned) c.fill({40,857,std::max(2,int(604*std::min(learned,total)/total)),8});
    button(c, {40, 892, 340, 88}, Action::ListenCourse, "听本课句子", 0, false, v.course.sentences > 0);
    button(c, {404, 892, 240, 88}, Action::Plan, "学习计划");
    const std::string status = !v.notice.empty() ? v.notice
                               : v.writable      ? "离线学习 · 进度自动保存"
                                                 : "未能保存进度 · 仅本次有效";
    const auto lines = c.wrap(status, 604);
    for (size_t i = 0; i < std::min<size_t>(2, lines.size()); ++i)
        c.text(40, 1000 + int(i) * 40, lines[i]);
    tabs(c, Action::Home);
}
void UI::library(Canvas &c, const View &v) {
    strong(c, 40, 32, "小小单词卡");
    c.text(644 - c.text_width("词书"), 32, "词书");
    c.fill({40, 98, 604, 2});
    strong(c, 40, 144, "我的词书", 2);
    button(c,{420,132,224,88},Action::Dictionary,"查词");
    c.text(40, 240, v.books.empty() ? "暂无可用词书" : "选择一本，按你的节奏学。");
    const int begin = v.page * 4;
    for (int i = 0; i < 4 && begin + i < int(v.books.size()); ++i) {
        const int index = begin + i;
        const auto &book = v.books[index];
        const Rect rect{40, 300 + i * 164, 604, 148};
        buttons_.push_back({rect, {Action::Deck, index}, book.name});
        strong(c, 40, rect.y + 12, c.ellipsize(book.name, 460));
        if (index == v.selected_book)
            c.text(554, rect.y + 12, "在学");
        else
            chevron(c, 624, rect.y + 34);
        c.text(40, rect.y + 64, c.ellipsize(book.description, 604));
        c.text(40, rect.y + 104, std::to_string(book.total) + " 项");
        c.fill({40, rect.y + rect.h, 604, 1});
    }
    if (v.books.size() > 4) {
        if (v.page > 0)
            row(c, {40, 976, 250, 88}, Action::Page, "上一页", -1);
        const bool more = begin + 4 < int(v.books.size());
        if (more)
            row(c, {394, 976, 250, 88}, Action::Page, "下一页", 1);
    }
    tabs(c, Action::Library);
}
#include "guided_screens.inc"
#include "recall_screens.inc"
#include "assessment_screens.inc"
bool UI::render_input(Canvas &c,const View&v){
    if(v.screen==Screen::Dictionary&&painted_input_screen_==v.screen&&keyboard_&&
       keyboard_shift_==v.dictionary_shift&&keyboard_symbols_==v.dictionary_symbols&&keyboard_search_==v.dictionary_ready&&
       painted_input_notice_==v.notice&&painted_dictionary_recent_==v.dictionary_recent&&painted_dictionary_query_!=v.dictionary_query){
        dictionary_input_field(c,v);painted_dictionary_query_=v.dictionary_query;return true;
    }
    const auto&w=v.web;const auto&old=painted_web_input_;
    if(v.screen!=Screen::WebLearning||painted_input_screen_!=v.screen||!w.keyboard||!old.keyboard||
       w.route!="word-batch"||old.key!=w.key||old.gap_mask!=w.gap_mask||old.shift!=w.shift||old.symbols!=w.symbols||
       old.toast!=w.toast||old.speech_note!=w.speech_note||v.notice!=painted_input_notice_||old.actions!=w.actions||old.typed==w.typed||old.blocks.size()!=w.blocks.size())return false;
    for(size_t i=0;i<w.blocks.size();++i){const auto&a=old.blocks[i];const auto&b=w.blocks[i];if(a.text!=b.text||a.enabled!=b.enabled||a.selected!=b.selected||a.role!=b.role||a.detail!=b.detail)return false;}
    web_input_field(c,w);
    if(old.can_submit!=w.can_submit){
        buttons_.erase(std::remove_if(buttons_.begin(),buttons_.end(),[](const auto&b){return b.event.action==Action::DictionarySearch;}),buttons_.end());
        button(c,{518,1092,164,84},Action::DictionarySearch,"检查",0,true,w.can_submit);
        for(int y=1092;y<1176;++y)for(int x=518;x<682;++x)c.pixel(x,y,c.shade(x,y)<8);
    }
    painted_web_input_=w;return true;
}
void UI::render(Canvas &c, const View &v) {
    painted_input_screen_=v.screen;painted_web_input_=v.web;painted_input_notice_=v.notice;
    painted_dictionary_query_=v.dictionary_query;painted_dictionary_recent_=v.dictionary_recent;
    guided_enabled_=v.guided_enabled;more_learning_scope_=v.more_learning_scope;
    const bool recall_input=v.screen==Screen::RecallLesson&&v.recall_typing&&!v.recall.checked;
    keyboard_recall_=recall_input;
    keyboard_=v.screen==Screen::Dictionary||recall_input;keyboard_shift_=v.dictionary_shift;
    keyboard_symbols_=v.dictionary_symbols;keyboard_search_=recall_input?v.recall_ready:v.dictionary_ready;
    keyboard_recent_=recall_input?0:int(v.dictionary_recent.size());
    c.clear();
    buttons_.clear();
    answer_pages_ = 1;
    if(v.screen==Screen::WebLearning||v.screen==Screen::Booting||v.screen==Screen::WebTools||v.screen==Screen::WebImportConfirm||v.screen==Screen::WebPrompt||v.screen==Screen::DeviceSettings||v.screen==Screen::MoreLearning){web_pages(c,v);return;}
    if(v.screen>=Screen::AssessmentHome){assessment_pages(c,v);return;}
    if(v.screen==Screen::RecallHome||v.screen==Screen::RecallLesson||v.screen==Screen::RecallDone){recall_pages(c,v);return;}
    if(v.guided_enabled && ((v.screen==Screen::Home && !v.legacy_home) || v.screen==Screen::Welcome ||
       (v.screen==Screen::Practice && !v.legacy_practice) || v.screen==Screen::GuidedCourses ||
       v.screen==Screen::GuidedLesson || v.screen==Screen::GuidedPause || v.screen==Screen::GuidedDone || v.screen==Screen::GuidedListen || v.screen==Screen::LearningStart || v.screen==Screen::CourseDetail)){
        guided_pages(c,v);return;
    }
    if(v.screen==Screen::PowerOff){
        if(v.power_phase==2){draw_shutdown_card(c);return;}
        c.text(28,20,"小小单词卡");c.fill({24,78,636,2});
        c.centered(390,v.power_phase==0?"请松开电源键":v.power_phase==1?"正在保存":"正在关机",2);
        c.centered(522,v.power_phase==0?"松开后，保存进度并关机。":v.power_phase==1?"稍等片刻，正在保存学习进度。":"学习进度已保存。");
        c.centered(1000,"断电后，墨水屏仍会保留画面。");
        return;
    }
    if(v.screen==Screen::TtsTest){tts_test_page(c,v);return;}
    if(v.screen==Screen::UsbDisk){usb_page(c,v);return;}
    if (int(v.screen) >= int(Screen::Dictionary)) { dictionary_pages(c,v); return; }
    if (v.screen == Screen::Home) {
        home(c, v);
        return;
    }
    if (v.screen == Screen::Library) {
        library(c, v);
        return;
    }
    if (v.screen == Screen::Done || int(v.screen) >= int(Screen::Welcome)) {
        learning_pages(c, v);
        return;
    }
    const std::string title = v.screen == Screen::Study
                                  ? v.deck + " · " + (v.group.empty() ? "全部主题" : v.group)
                                  : "小小单词卡";
    c.text(28, 20, c.ellipsize(title, 610));
    c.fill({24, 78, 636, 2});
    if (v.screen != Screen::Home)
        button(c, {24, 96, 136, 88}, Action::Back, v.screen == Screen::Study ? "暂停" : "返回");
    if (!v.notice.empty()) {
        auto lines = c.wrap(v.notice, 624);
        int y = 1082;
        for (size_t i = 0; i < std::min<size_t>(2, lines.size()); ++i)
            c.text(30, y + int(i) * 40, lines[i]);
    }
    c.text(28, 1160, v.writable ? "离线学习 · 进度自动保存" : "体验模式 · 进度仅本次有效");
    if (v.screen == Screen::Groups) {
        c.text(200, 112, c.ellipsize(v.deck + " · 选择主题", 456));
        const int begin = v.page * 6;
        for (int i = 0; i < 6 && begin + i < int(v.groups.size()); ++i)
            button(c, {28, 206 + i * 116, 628, 98}, Action::Group, v.groups[begin + i], begin + i);
        button(c, {28, 936, 300, 96}, Action::Page, "上一页", -1, false, v.page > 0);
        button(c, {356, 936, 300, 96}, Action::Page, "下一页", 1, false,
               begin + 6 < int(v.groups.size()));
    } else if (v.screen == Screen::Study && v.card) {
        button(c, {180, 96, 136, 88}, Action::Favorite, v.favorite ? "已收藏" : "收藏");
        button(c, {336, 96, 136, 88}, Action::Undo, "撤销", 0, false, v.can_undo);
        c.text(510, 122, std::to_string(v.position + 1) + "/" + std::to_string(v.count));
        const bool hidden_audio = v.study_mode == StudyMode::Listening && !v.revealed;
        const bool reverse = v.study_mode == StudyMode::Reverse && !v.revealed;
        int scale = c.text_width(v.card->front, 3) <= 620 ? 3 : 2;
        auto front = c.wrap(v.card->front, 620, scale);
        if (front.size() > 3) {
            scale = 1;
            front = c.wrap(v.card->front, 620, scale);
        }
        const bool three_lines = !hidden_audio && !reverse && scale == 2 && front.size() == 3;
        int yy = three_lines ? 210 : 230;
        if (hidden_audio) {
            c.centered(250, "听音回忆", 2);
            c.centered(350, "听到这个词，你想到什么？");
            yy = 390;
        } else if (reverse) {
            const auto prompt = c.wrap(reverse_prompt(v.card->back), 604);
            answer_pages_ = std::max(1, int((prompt.size() + 3) / 4));
            const int first = std::min(v.answer_page, answer_pages_ - 1) * 4;
            for (size_t i = first; i < prompt.size() && i < size_t(first + 4); ++i)
                c.text(40, 230 + int(i - first) * 44, prompt[i]);
            if (answer_pages_ > 1)
                button(c, {40, 430, 604, 88}, Action::AnswerPage, "题面下一页");
            yy = 520;
        } else
            for (const auto &line : front) {
                c.centered(yy, line, scale);
                yy += three_lines ? 72 : scale * 44;
            }
        if (!reverse)
            button(c, {210, yy + (three_lines ? 20 : 30), 264, 88}, Action::Speak,
                   v.audio_busy        ? (v.audio_source==SpeechSource::Synthesis ? "合成播放中" : "SD 播放中")
                   : v.audio_pending   ? "发音待复核"
                   : v.audio_available ? (v.audio_source==SpeechSource::Synthesis ? "合成发音" : v.audio_source==SpeechSource::Sd ? "SD 发音" : "听发音")
                                       : "暂无发音",
                   0, false, v.audio_available && !v.audio_busy);
        if (!v.revealed) {
            c.centered(700, reverse        ? "先想一想英文怎么说"
                            : hidden_audio ? "想好后再看答案"
                                           : "先想一想它的意思");
            button(c, {28, 936, 628, 108}, Action::Reveal, "查看答案", 0, true);
        } else {
            c.fill({28, 550, 628, 2});
            const auto lines =
                c.wrap((v.card->ipa.empty() ? "" : v.card->ipa + "\n") + v.card->back, 616);
            answer_pages_ = std::max(1, int((lines.size() + 6) / 7));
            const int start = std::min(v.answer_page, answer_pages_ - 1) * 7;
            for (size_t i = start; i < lines.size() && i < size_t(start + 7); ++i)
                c.text(34, 574 + int(i - start) * 44, lines[i]);
            if (answer_pages_ > 1)
                button(c, {200, 874, 284, 88}, Action::AnswerPage,
                       "释义 " + std::to_string(v.answer_page + 1) + " / " +
                           std::to_string(answer_pages_));
            button(c, {28, 970, 192, 98}, Action::Rate, "不会", 0);
            button(c, {246, 970, 192, 98}, Action::Rate, "一般", 1);
            button(c, {464, 970, 192, 98}, Action::Rate, "记住", 2, true);
        }
    } else if (v.screen == Screen::Done) {
        c.centered(330, "本轮完成", 2);
        c.centered(480, "记住的词会在到期后出现");
        c.centered(546, "不会的词已在本轮再次练习");
        button(c, {28, 812, 628, 112}, Action::Home, "返回首页", 0, true);
    } else if (v.screen == Screen::Settings) {
        c.text(200, 112, "前光与音量");
        c.text(28, 240, "前光亮度  " + std::to_string(v.brightness) + "%");
        button(c, {28, 310, 300, 104}, Action::Brightness, "调暗", -10);
        button(c, {356, 310, 300, 104}, Action::Brightness, "调亮", 10);
        const char *modes[] = {"暖光", "冷光", "冷暖光", "关闭"};
        button(c, {28, 452, 628, 104}, Action::Light,
               std::string("色温：") + modes[std::clamp(v.cct, 0, 3)]);
        c.text(28, 642, "发音音量  " + std::to_string(v.volume) + "%");
        button(c, {28, 716, 300, 104}, Action::Volume, "音量减", -10);
        button(c, {356, 716, 300, 104}, Action::Volume, "音量加", 10);
        button(c, {28, 870, 300, 104}, Action::Tone, "试听提示音");
        button(c, {356, 870, 300, 104}, Action::SelfTest, "硬件自检");
        button(c, {28, 986, 628, 88}, Action::TtsTest, "TTS 发音与性能测试");
    } else if (v.screen == Screen::SelfTest) {
        c.text(200, 112, "硬件自检");
        int y = 206;
        for (const auto &line : v.diagnostics) {
            c.text(28, y, line);
            y += 48;
        }
        button(c, {28, 620, 300, 96}, Action::Tone, "喇叭提示音", 0, false, !v.audio_busy);
        button(c, {356, 620, 300, 96}, Action::Mic, "录音并回放", 0, false, !v.audio_busy);
        button(c, {28, 750, 300, 96}, Action::Vibe, "短震一次");
        button(c, {356, 750, 300, 96}, Action::Light, "切换前光");
        button(c, {28, 880, 300, 96}, Action::Refresh, "全屏刷新");
        button(c, {356, 880, 300, 96}, Action::Settings, "前光 / 音量");
        c.text(28, 1006, v.audio_status);
    }
}
#include "tts_test_screen.inc"
#include "usb_screen.inc"
} // namespace wordcard
#include "web_screens.inc"
