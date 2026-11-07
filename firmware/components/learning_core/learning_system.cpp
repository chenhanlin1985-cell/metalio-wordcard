#include "learning_system.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace wordcard {
bool is_sentence(const Card &card) {
    return ("|" + card.tags + "|").find("|sentence|") != std::string::npos;
}
CourseProgress course_progress(const std::vector<Card> &cards, const ProgressMap &memory,
                               const std::string &group) {
    CourseProgress result;
    if (group.empty()) return result;
    for (const auto &card : cards) {
        if (card.group != group) continue;
        ++result.total;
        if (is_sentence(card)) ++result.sentences;
        const auto it = memory.find(card.id);
        if (it != memory.end() && it->second.seen) ++result.learned;
    }
    result.remaining = result.total - result.learned;
    return result;
}
static size_t slot(StudyMode m) {
    return static_cast<size_t>(m);
}
void LearningSystem::bind(const std::vector<Card> *cards) {
    cards_ = cards;
    index_.clear();
    if (cards)
        for (size_t i = 0; i < cards->size(); ++i)
            index_[(*cards)[i].id] = i;
}
PlanCounts LearningSystem::counts(uint32_t day, const std::string &group, StudyMode mode) const {
    PlanCounts out;
    if (!cards_)
        return out;
    const auto c = study_counts(*cards_, state.memory[slot(mode)], day, group);
    out.due = c.due;
    out.unseen = c.unseen;
    const auto daily = state.days.find(day);
    out.today_new = daily == state.days.end() ? 0 : daily->second.fresh;
    const size_t remaining =
        mode == StudyMode::Visual
            ? (out.today_new < state.new_limit ? state.new_limit - out.today_new : 0)
            : state.lesson_size;
    out.fresh = std::min(out.unseen, remaining);
    return out;
}
bool LearningSystem::begin(const std::string &book, const std::string &group, uint32_t day,
                           StudyMode mode, bool difficult, const std::set<std::string> *audio,
                           bool favorites, const std::set<std::string> *selection) {
    if (!cards_ || state.lesson.active())
        return false;
    std::vector<const Card *> due, fresh;
    const auto &memory = state.memory[slot(mode)];
    std::set<std::string> primed;
    size_t recent = 0;
    for (auto it = state.events.rbegin(); it != state.events.rend() && recent < 3; ++it, ++recent)
        if (it->day == day && it->mode != mode)
            primed.insert(it->id);
    for (const auto &card : *cards_) {
        if (selection && !selection->count(card.id)) continue;
        if (!group.empty() && card.group != group)
            continue;
        if (favorites && !state.favorites.count(card.id))
            continue;
        if (mode == StudyMode::Listening && (!audio || !audio->count(card.id)))
            continue;
        if (primed.count(card.id))
            continue;
        const auto it = memory.find(card.id);
        const Progress p = it == memory.end() ? Progress{} : it->second;
        if (difficult && !(p.again || p.hard >= 2 || p.ease < 220))
            continue;
        if (p.seen && (p.due_day <= day || difficult || favorites))
            due.push_back(&card);
        else if (!p.seen)
            fresh.push_back(&card);
    }
    std::stable_sort(due.begin(), due.end(), [&](const Card *a, const Card *b) {
        return memory.at(a->id).due_day < memory.at(b->id).due_day;
    });
    Lesson lesson;
    lesson.serial = ++state.sequence;
    lesson.day = day;
    lesson.book = book;
    lesson.group = group;
    lesson.mode = mode;
    lesson.difficult = difficult;
    lesson.favorites = favorites;
    lesson.paused = false;
    for (const auto *card : due) {
        if (lesson.queue.size() == state.lesson_size)
            break;
        lesson.queue.push_back(card->id);
    }
    size_t remaining = counts(day, group, mode).fresh;
    for (const auto *card : fresh) {
        if (!remaining || lesson.queue.size() == state.lesson_size)
            break;
        lesson.queue.push_back(card->id);
        --remaining;
    }
    lesson.initial = uint32_t(lesson.queue.size());
    state.lesson = std::move(lesson);
    return state.lesson.active();
}
const Card *LearningSystem::current() const {
    const auto &l = state.lesson;
    if (!cards_ || !l.active())
        return nullptr;
    const auto it = index_.find(l.queue[l.position]);
    return it == index_.end() ? nullptr : &(*cards_)[it->second];
}
bool LearningSystem::begin_remaining(const std::string &book, const std::string &group, uint32_t day) {
    if (!cards_ || group.empty() || state.lesson.active()) return false;
    Lesson lesson;
    lesson.serial = state.sequence + 1;
    lesson.day = day; lesson.book = book; lesson.group = group; lesson.paused = false;
    for (const auto &card : *cards_) {
        if (card.group != group) continue;
        auto it = state.memory[0].find(card.id);
        if (it == state.memory[0].end() || !it->second.seen) lesson.queue.push_back(card.id);
        if (lesson.queue.size() >= state.lesson_size) break;
    }
    if (lesson.queue.empty()) return false;
    ++state.sequence;
    lesson.initial = uint32_t(lesson.queue.size());
    state.lesson = std::move(lesson);
    return true;
}
bool LearningSystem::reveal() {
    if (!current() || state.lesson.paused || state.lesson.revealed)
        return false;
    state.lesson.revealed = true;
    return true;
}
bool LearningSystem::rate(Rating rating, uint32_t day, bool dated, uint32_t expected_position) {
    auto &l = state.lesson;
    const Card *card = current();
    if (!card || l.paused || !l.revealed || l.position != expected_position)
        return false;
    auto &memory = state.memory[slot(l.mode)];
    auto &last = state.last_day[slot(l.mode)];
    ReviewEvent e;
    e.serial = ++state.sequence;
    e.lesson = l.serial;
    e.day = day;
    e.dated = dated;
    e.position = l.position;
    e.queue_size = uint32_t(l.queue.size());
    e.id = card->id;
    e.mode = l.mode;
    e.rating = rating;
    e.existed = memory.count(e.id) != 0;
    e.before = e.existed ? memory.at(e.id) : Progress{};
    e.previous_day = last.count(e.id) ? last.at(e.id) : 0;
    e.first_today = e.previous_day != day;
    e.fresh = !e.before.seen && l.mode == StudyMode::Visual;
    review(memory[e.id], rating, day);
    last[e.id] = day;
    auto &d = state.days[day];
    ++d.attempts;
    if (e.fresh)
        ++d.fresh;
    if (e.first_today && l.mode == StudyMode::Visual && !e.fresh)
        ++d.reviewed;
    if (e.first_today && l.mode == StudyMode::Listening)
        ++d.listening;
    if (rating != Rating::Again)
        ++d.recalled;
    state.events.push_back(e);
    if (state.events.size() > 512)
        state.events.erase(state.events.begin());
    // At most one return, at least two intervening prompts. A short tail defers to tomorrow.
    if (rating == Rating::Again && l.queue.size() - l.position > 2 &&
        std::count(l.queue.begin(), l.queue.end(), e.id) == 1)
        l.queue.push_back(e.id);
    ++l.position;
    l.revealed = false;
    return true;
}
bool LearningSystem::can_undo() const {
    return !state.events.empty() && state.events.back().lesson == state.lesson.serial &&
           state.events.back().position + 1 == state.lesson.position;
}
bool LearningSystem::undo() {
    if (!can_undo())
        return false;
    const auto e = state.events.back();
    auto &m = state.memory[slot(e.mode)];
    auto &last = state.last_day[slot(e.mode)];
    if (e.existed)
        m[e.id] = e.before;
    else
        m.erase(e.id);
    if (e.previous_day)
        last[e.id] = e.previous_day;
    else
        last.erase(e.id);
    auto &d = state.days[e.day];
    --d.attempts;
    if (e.fresh)
        --d.fresh;
    if (e.first_today && e.mode == StudyMode::Visual && !e.fresh)
        --d.reviewed;
    if (e.first_today && e.mode == StudyMode::Listening)
        --d.listening;
    if (e.rating != Rating::Again)
        --d.recalled;
    state.lesson.position = e.position;
    state.lesson.queue.resize(e.queue_size);
    state.lesson.revealed = true;
    state.lesson.paused = false;
    state.events.pop_back();
    return true;
}
size_t LearningSystem::distinct_in_lesson() const {
    std::set<std::string> ids;
    for (const auto &e : state.events)
        if (e.lesson == state.lesson.serial)
            ids.insert(e.id);
    return ids.size();
}
size_t LearningSystem::difficult_in_lesson() const {
    std::set<std::string> ids;
    for (const auto &e : state.events)
        if (e.lesson == state.lesson.serial && e.rating == Rating::Again)
            ids.insert(e.id);
    return ids.size();
}
bool LearningSystem::validate_lesson(const std::string &book) const {
    if (state.lesson.book != book)
        return false;
    for (const auto &id : state.lesson.queue)
        if (!index_.count(id))
            return false;
    return true;
}
bool LearningSystem::validate_lesson_audio(const std::set<std::string> &available) const {
    const auto &l = state.lesson;
    if (l.mode != StudyMode::Listening || !l.active())
        return true;
    return std::all_of(l.queue.begin() + l.position, l.queue.end(),
                       [&](const auto &id) { return available.count(id) != 0; });
}
static uint32_t hash(const std::string &s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}
static void progress_out(std::ostream &o, const Progress &p) {
    o << ',' << p.due_day << ',' << p.seen << ',' << p.again << ',' << p.hard << ',' << p.good
      << ',' << p.ease << ',' << p.interval << ',' << unsigned(p.box);
}
bool save_learning_state(const std::string &path, const LearningState &s, std::string &error) {
    std::ostringstream o;
    o << "A," << s.sequence << ',' << s.new_limit << ',' << s.lesson_size << ',' << s.onboarded
      << ',' << s.haptics << '\n';
    for (size_t mode = 0; mode < 3; ++mode) {
        for (const auto &p : s.memory[mode]) {
            o << "P," << mode << ',' << csv_escape(p.first);
            progress_out(o, p.second);
            o << '\n';
        }
        for (const auto &p : s.last_day[mode])
            o << "L," << mode << ',' << csv_escape(p.first) << ',' << p.second << '\n';
    }
    for (const auto &p : s.days) {
        const auto &d = p.second;
        o << "D," << p.first << ',' << d.attempts << ',' << d.fresh << ',' << d.reviewed << ','
          << d.listening << ',' << d.recalled << '\n';
    }
    for (const auto &id : s.favorites)
        o << "F," << csv_escape(id) << '\n';
    const auto &l = s.lesson;
    o << "S," << l.serial << ',' << l.day << ',' << l.position << ',' << l.initial << ','
      << csv_escape(l.book) << ',' << csv_escape(l.group) << ',' << slot(l.mode) << ','
      << l.revealed << ',' << l.paused << ',' << l.difficult << ',' << l.favorites << '\n';
    o << "V," << csv_escape(s.player_book) << ',' << csv_escape(s.player_group) << ','
      << csv_escape(s.player_id) << '\n';
    for (const auto &id : l.queue)
        o << "Q," << csv_escape(id) << '\n';
    for (const auto &e : s.events) {
        o << "E," << e.serial << ',' << e.lesson << ',' << e.day << ',' << e.position << ','
          << e.queue_size << ',' << e.previous_day << ',' << slot(e.mode) << ',' << int(e.rating)
          << ',' << csv_escape(e.id) << ',' << e.existed << ',' << e.first_today << ',' << e.fresh
          << ',' << e.dated;
        progress_out(o, e.before);
        o << '\n';
    }
    const auto payload = o.str();
    return atomic_write(
        path, "#wordcard-state-v2," + std::to_string(hash(payload)) + '\n' + payload, error);
}
static bool num(const std::string &s, uint32_t &n, uint32_t max = UINT32_MAX) {
    if (s.empty())
        return false;
    uint64_t value = 0;
    for (char c : s) {
        if (c < '0' || c > '9')
            return false;
        value = value * 10 + c - '0';
        if (value > max)
            return false;
    }
    n = uint32_t(value);
    return true;
}
static bool progress_in(const Row &r, size_t start, Progress &p) {
    if (r.size() != start + 8)
        return false;
    uint32_t n[8]{};
    const uint32_t max[] = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX,
                            UINT32_MAX, 300,        365,        7};
    for (size_t i = 0; i < 8; ++i)
        if (!num(r[start + i], n[i], max[i]))
            return false;
    if (n[5] < 130)
        return false;
    p.due_day = n[0];
    p.seen = n[1];
    p.again = n[2];
    p.hard = n[3];
    p.good = n[4];
    p.ease = uint16_t(n[5]);
    p.interval = uint16_t(n[6]);
    p.box = uint8_t(n[7]);
    return true;
}
bool load_learning_state(const std::string &path, LearningState &out, std::string &error) {
    if (!recover_backup(path, error))
        return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "Cannot open learning state";
        return false;
    }
    file.seekg(0, std::ios::end);
    if (file.tellg() > 16 * 1024 * 1024) {
        error = "Learning state exceeds 16 MiB";
        return false;
    }
    file.seekg(0);
    std::string marker;
    std::getline(file, marker);
    std::ostringstream rest;
    rest << file.rdbuf();
    const auto payload = rest.str();
    const std::string prefix = "#wordcard-state-v2,";
    uint32_t expected = 0;
    if (marker.rfind(prefix, 0) || !num(marker.substr(prefix.size()), expected) ||
        hash(payload) != expected) {
        error = "Learning state checksum/version mismatch";
        return false;
    }
    LearningState s;
    bool meta = false, lesson = false;
    std::istringstream input(payload);
    const bool ok = read_csv(
        input,
        [&](const Row &r) {
            if (r.empty())
                return false;
            auto n = [&](size_t i, uint32_t &v, uint32_t max = UINT32_MAX) {
                return i < r.size() && num(r[i], v, max);
            };
            uint32_t v[13]{};
            if (r[0] == "A") {
                if (meta || r.size() != 6 || !n(1, s.sequence) || !n(2, v[0], 50) ||
                    !n(3, v[1], 12) || v[1] < 5 || !n(4, v[2], 1) || !n(5, v[3], 1))
                    return false;
                s.new_limit = v[0];
                s.lesson_size = v[1];
                s.onboarded = v[2];
                s.haptics = v[3];
                meta = true;
            } else if (r[0] == "P") {
                if (r.size() != 11 || !n(1, v[0], 2) || r[2].empty() || s.memory[v[0]].count(r[2]))
                    return false;
                Progress p;
                if (!progress_in(r, 3, p))
                    return false;
                s.memory[v[0]][r[2]] = p;
            } else if (r[0] == "L") {
                if (r.size() != 4 || !n(1, v[0], 2) || !n(3, v[1]) || r[2].empty() ||
                    s.last_day[v[0]].count(r[2]))
                    return false;
                s.last_day[v[0]][r[2]] = v[1];
            } else if (r[0] == "D") {
                if (r.size() != 7)
                    return false;
                for (size_t i = 0; i < 6; ++i)
                    if (!n(i + 1, v[i]))
                        return false;
                if (s.days.count(v[0]) || v[2] > v[1] || v[3] > v[1] || v[4] > v[1] || v[5] > v[1])
                    return false;
                s.days[v[0]] = {v[1], v[2], v[3], v[4], v[5]};
            } else if (r[0] == "F") {
                if (r.size() != 2 || r[1].empty() || !s.favorites.insert(r[1]).second)
                    return false;
            } else if (r[0] == "S") {
                if (lesson || r.size() != 12)
                    return false;
                for (size_t i = 0; i < 4; ++i)
                    if (!n(i + 1, v[i]))
                        return false;
                if (!n(7, v[4], 2) || !n(8, v[5], 1) || !n(9, v[6], 1) || !n(10, v[7], 1) ||
                    !n(11, v[8], 1))
                    return false;
                auto &l = s.lesson;
                l.serial = v[0];
                l.day = v[1];
                l.position = v[2];
                l.initial = v[3];
                l.book = r[5];
                l.group = r[6];
                l.mode = StudyMode(v[4]);
                l.revealed = v[5];
                l.paused = v[6];
                l.difficult = v[7];
                l.favorites = v[8];
                lesson = true;
            } else if (r[0] == "V") {
                if (r.size() != 4)
                    return false;
                s.player_book = r[1];
                s.player_group = r[2];
                s.player_id = r[3];
            } else if (r[0] == "Q") {
                if (r.size() != 2 || r[1].empty() || s.lesson.queue.size() >= 24)
                    return false;
                s.lesson.queue.push_back(r[1]);
            } else if (r[0] == "E") {
                if (r.size() != 22 || s.events.size() >= 512)
                    return false;
                for (size_t i = 0; i < 8; ++i)
                    if (!n(i + 1, v[i], i == 6 ? 2 : i == 7 ? 2 : UINT32_MAX))
                        return false;
                for (size_t i = 0; i < 4; ++i)
                    if (!n(i + 10, v[i + 8], 1))
                        return false;
                ReviewEvent e;
                e.serial = v[0];
                e.lesson = v[1];
                e.day = v[2];
                e.position = v[3];
                e.queue_size = v[4];
                e.previous_day = v[5];
                e.mode = StudyMode(v[6]);
                e.rating = Rating(v[7]);
                e.id = r[9];
                e.existed = v[8];
                e.first_today = v[9];
                e.fresh = v[10];
                e.dated = v[11];
                if (e.id.empty() || e.queue_size > 24 || e.position >= e.queue_size ||
                    !progress_in(r, 14, e.before) ||
                    (!s.events.empty() && e.serial <= s.events.back().serial))
                    return false;
                s.events.push_back(e);
            } else
                return false;
            return true;
        },
        error);
    if (!ok || !meta || !lesson || s.lesson.position > s.lesson.queue.size() ||
        s.lesson.initial > 12 || s.lesson.initial > s.lesson.queue.size() ||
        s.lesson.serial > s.sequence ||
        (!s.events.empty() && s.events.back().serial > s.sequence)) {
        error = "Invalid learning state structure";
        return false;
    }
    out = std::move(s);
    return true;
}
bool load_catalog(std::istream &in, std::vector<CatalogBook> &out, std::string &error) {
    std::vector<CatalogBook> next;
    std::set<std::string> ids, files;
    bool header = false;
    bool ok = read_csv(
        in,
        [&](const Row &r) {
            if (!header) {
                header = true;
                return r == Row{"id", "file", "name", "description"};
            }
            if (r.size() != 4 || r[0].empty() || r[0].size() > 96 || r[2].empty() ||
                r[2].size() > 200 || r[3].size() > 300 || !safe_relative_path(r[1]) ||
                r[1].find('/') != std::string::npos || r[1].size() > 96 || r[1].size() < 5 ||
                r[1].substr(r[1].size() - 4) != ".csv" || !ids.insert(r[0]).second ||
                !files.insert(r[1]).second || next.size() >= 64)
                return false;
            next.push_back({r[0], r[1], r[2], r[3], 0});
            return true;
        },
        error);
    if (!ok || !header) {
        error = "Invalid wordbook catalog";
        return false;
    }
    out = std::move(next);
    return true;
}
bool valid_date(int y, int m, int d) {
    if (y < 2024 || y > 2099 || m < 1 || m > 12 || d < 1)
        return false;
    const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return d <= days[m - 1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}
} // namespace wordcard
