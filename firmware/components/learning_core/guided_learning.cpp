#include "guided_learning.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <iomanip>
#include <sstream>
namespace wordcard {
namespace {
std::vector<std::string> tokens(const std::string &text) {
    std::istringstream in(text);
    std::vector<std::string> out;
    std::string s;
    while (in >> s)
        out.push_back(s);
    return out;
}
std::string normalized(const std::string &s) {
    std::string r;
    for (unsigned char ch : s)
        if (std::isalnum(ch) || ch == '\'' || ch == '-')
            r += char(std::tolower(ch));
    return r;
}
uint32_t hash(const std::string &s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}
unsigned count_bits(unsigned n) {
    unsigned count = 0;
    while (n) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}
} // namespace
bool GuidedLearning::unlocked(int c, int n) const {
    return c >= 0 && c < guided_course_count && n >= 0 && n < guided_node_count &&
           (n == 0 || (state.completed[c] & (1u << (n - 1))));
}
bool GuidedLearning::begin(int c, int n) {
    if (state.active || !unlocked(c, n))
        return false;
    const auto completed = state.completed, listened = state.listened;
    state = {};
    state.completed = completed;
    state.listened = listened;
    state.course = c;
    state.node = n;
    state.active = true;
    heard_ = false;
    profile.last_course = c;
    if(n>=4){
        const int first=n==4?12:14;
        for(int i=first;i<first+2;++i)
            state.pending.push_back({i,n==4?GuidedKind::ReadDialogue:GuidedKind::ListenDialogue,i-first,false});
        state.total=2;
        return true;
    }
    int slot = 0;
    const int start = n * 3;
    // Every lesson introduces three items, uses them, then revisits them through listening.
    for (int i = start; i < start + 3; ++i) {
        state.pending.push_back({i, GuidedKind::Introduce, 0, false});
        state.pending.push_back(
            {i, GuidedKind::Meaning, slot++, false});
    }
    for (int i = start; i < start + 3; ++i)
        state.pending.push_back(
            {i, i < 6 ? GuidedKind::ListenWord : GuidedKind::ListenMeaning, slot++, false});
    // Interleaved review is part of the route; no date-based spacing is claimed here.
    if (n > 0)
        for (int i = start - 2; i < start; ++i)
            state.pending.push_back({i, GuidedKind::Meaning, slot++, false});
    state.total = slot;
    return true;
}
const GuidedItem *GuidedLearning::current() const {
    if (!state.active || state.pending.empty())
        return nullptr;
    return &guided_courses()[state.course].items[state.pending.front().item];
}
bool GuidedLearning::listening() const {
    if (!current())
        return false;
    const auto &s = state.pending.front();
    return !s.reading && (s.kind == GuidedKind::ListenWord || s.kind == GuidedKind::ListenMeaning || s.kind == GuidedKind::ListenDialogue);
}
std::vector<std::string> GuidedLearning::options() const {
    if (!current())
        return {};
    const auto &step = state.pending.front();
    const auto &items = guided_courses()[state.course].items;
    if (step.kind == GuidedKind::Introduce)
        return {};
    std::vector<std::string> out;
    if(dialogue()) {
        const auto &task=guided_courses()[state.course].tasks[step.item-12];
        out.assign(task.options.begin(),task.options.end());
        const size_t shift=hash(current()->card.id)%out.size();
        std::rotate(out.begin(),out.begin()+shift,out.end());
    } else if (step.kind == GuidedKind::Order) {
        out = tokens(current()->card.front);
        // Deterministic Fisher-Yates: reproducible after resume, unlike sorted word banks.
        uint32_t rng = hash(current()->card.id);
        for (size_t n = out.size(); n > 1; --n) {
            rng = rng * 1664525u + 1013904223u;
            std::swap(out[n - 1], out[rng % n]);
        }
    } else {
        const bool english = step.kind == GuidedKind::ListenWord && !step.reading;
        auto value = [&](int i) { return english ? items[i].card.front : items[i].meaning; };
        out.push_back(value(step.item));
        const int base = step.item < 6 ? 0 : 6;
        for (int i = 1; i < 6 && out.size() < 3; ++i) {
            auto s = value(base + (step.item - base + i) % 6);
            if (std::find(out.begin(), out.end(), s) == out.end())
                out.push_back(s);
        }
        const size_t shift = hash(current()->card.id + std::to_string(int(step.kind))) % out.size();
        std::rotate(out.begin(), out.begin() + shift, out.end());
    }
    return out;
}
bool GuidedLearning::choose(int n) {
    if (!current() || state.checked)
        return false;
    const auto opts = options();
    if (n < 0 || n >= int(opts.size()))
        return false;
    if (state.pending.front().kind == GuidedKind::Order) {
        if (std::find(state.selected.begin(), state.selected.end(), n) != state.selected.end())
            return false;
        state.selected.push_back(n);
    } else
        state.selected = {n};
    return true;
}
bool GuidedLearning::remove(int n) {
    if (state.checked || n < 0 || n >= int(state.selected.size()))
        return false;
    state.selected.erase(state.selected.begin() + n);
    return true;
}
bool GuidedLearning::ready_to_check() const {
    if (!current() || state.checked || state.pending.front().kind == GuidedKind::Introduce)
        return false;
    return (!listening() || heard_) && (state.pending.front().kind == GuidedKind::Order
                                            ? state.selected.size() == options().size()
                                            : state.selected.size() == 1);
}
bool GuidedLearning::check() {
    if (!ready_to_check())
        return false;
    const auto &step = state.pending.front();
    const auto opts = options();
    bool correct = false;
    if (step.kind == GuidedKind::Order) {
        const auto expected = tokens(current()->card.front);
        correct = true;
        for (size_t i = 0; i < expected.size(); ++i)
            if (normalized(expected[i]) != normalized(opts[state.selected[i]]))
                correct = false;
    } else {
        const std::string answer = expected_answer();
        correct = opts[state.selected[0]] == answer;
    }
    const unsigned mask = 1u << step.slot;
    if (!(state.attempted_mask & mask) && correct)
        ++state.first_correct;
    state.attempted_mask |= mask;
    state.checked = true;
    state.correct = correct;
    if (correct) {
        state.solved_mask |= mask;
        state.solved = int(count_bits(state.solved_mask));
    } else
        ++state.errors;
    // Dialogue comprehension has authored evidence, not a word-meaning memory record.
    if(!dialogue()) review.observe(current()->card, current()->meaning,
                   listening() ? RecallSkill::Listening : RecallSkill::Meaning, correct);
    return true;
}
bool GuidedLearning::advance() {
    if (!current())
        return false;
    const auto step = state.pending.front();
    if (step.kind != GuidedKind::Introduce && !state.checked)
        return false;
    state.pending.pop_front();
    if (step.kind != GuidedKind::Introduce && !state.correct)
        state.pending.push_back(step);
    state.selected.clear();
    state.checked = state.correct = false;
    heard_ = false;
    if (state.pending.empty()) {
        state.active = false;
        state.finished = true;
        state.completed[state.course] |= 1u << state.node;
        if (!state.skipped)
            state.listened[state.course] |= 1u << state.node;
    }
    return true;
}
bool GuidedLearning::skip_listening() {
    if (!listening() || state.checked)
        return false;
    auto &s = state.pending.front();
    if(!dialogue()) review.defer_listening(current()->card,current()->meaning);
    s.reading = true;
    state.skipped_mask |= 1u << s.slot;
    state.skipped = int(count_bits(state.skipped_mask));
    state.selected.clear();
    heard_ = false;
    return true;
}
GuidedView GuidedLearning::view(int browse) const {
    GuidedView v;
    v.resume_title = guided_courses()[state.course].title;
    v.course = browse >= 0 && browse < guided_course_count ? browse : state.course;
    v.track = guided_courses()[v.course].track;
    v.profile=profile;v.draft=draft;
    v.recommended=recommended_course();v.recommended_node=next_node(v.recommended);
    v.recommendation_reason=recommendation_reason();
    v.catalog=catalog();v.catalog_track=catalog_track;v.catalog_level=catalog_level;
    v.catalog_page=std::max(0,std::min(catalog_page,(int(v.catalog.size())-1)/4));
    v.node = state.node;
    v.completed = state.completed;
    v.listened = state.listened;
    v.active = state.active;
    v.finished = state.finished;
    for (size_t i = 0; i < guided_course_count; ++i)
        v.titles[i] = guided_courses()[i].title;
    const auto &c = guided_courses()[v.course];
    v.title = c.title;
    v.goal = c.goal;
    v.total = state.total;
    v.solved = state.solved;
    v.errors = state.errors;
    v.skipped = state.skipped;
    v.first_correct = state.first_correct;
    if (!current())
        return v;
    v.kind = state.pending.front().kind;
    v.reading = state.pending.front().reading;
    v.heard = heard_;
    v.text = current()->card.front;
    v.meaning = current()->meaning;
    v.ipa = current()->card.ipa;
    v.usage = current()->card.back;
    v.answer = v.text + "\n" + v.meaning;
    v.selected = state.selected;
    v.options = options();
    v.checked = state.checked;
    v.correct = state.correct;
    v.can_check = ready_to_check();
    for (int i : v.selected) {
        if (!v.selected_text.empty())
            v.selected_text += ' ';
        v.selected_text += v.options[i];
    }
    v.prompt = v.kind == GuidedKind::Introduce
                   ? (state.pending.front().item < 6 ? "认识一个新词" : "学一句实用表达")
               : v.kind == GuidedKind::Order ? "排成刚学的句子"
               : listening()                 ? "听一听，选择答案"
                                             : "选择正确的意思";
    if(dialogue()) {
        const auto &task=c.tasks[state.pending.front().item-12];
        v.passage=task.passage;v.prompt=task.question;v.explanation=task.explanation;
        v.answer=expected_answer();
    }
    return v;
}
std::string GuidedLearning::encode_legacy() const {
    std::ostringstream s;
    s << state.course << ' ' << state.node << ' ' << state.total << ' ' << state.solved << ' '
      << state.errors << ' ' << state.skipped << ' ' << state.first_correct << ' '
      << state.solved_mask << ' ' << state.attempted_mask << ' ' << state.skipped_mask << ' '
      << state.active << ' ' << state.finished << ' ' << state.checked << ' ' << state.correct
      << '\n';
    for (auto c : state.completed)
        s << c << ' ';
    s << '\n';
    for (auto c : state.listened)
        s << c << ' ';
    s << '\n' << state.pending.size() << '\n';
    for (auto q : state.pending)
        s << q.item << ' ' << int(q.kind) << ' ' << q.slot << ' ' << q.reading << '\n';
    s << state.selected.size();
    for (int n : state.selected)
        s << ' ' << n;
    s << '\n';
    const auto p = s.str();
    return "guided-v1 " + std::to_string(hash(p)) + "\n" + p;
}
bool GuidedLearning::decode_legacy(const std::string &bytes, std::string &error) {
    auto invalid = [&]() {
        error = "Invalid guided progress/version/checksum";
        return false;
    };
    if (bytes.size() > 16384)
        return invalid();
    const auto nl = bytes.find('\n');
    if (nl == std::string::npos)
        return invalid();
    std::istringstream header(bytes.substr(0, nl));
    std::string version, extra;
    uint32_t checksum = 0;
    if (!(header >> version >> checksum) || (header >> extra) || version != "guided-v1" ||
        hash(bytes.substr(nl + 1)) != checksum)
        return invalid();
    GuidedLearning next;
    auto &s = next.state;
    std::istringstream in(bytes.substr(nl + 1));
    if (!(in >> s.course >> s.node >> s.total >> s.solved >> s.errors >> s.skipped >>
          s.first_correct >> s.solved_mask >> s.attempted_mask >> s.skipped_mask >> s.active >>
          s.finished >> s.checked >> s.correct))
        return invalid();
    if (s.course < 0 || s.course >= 12 || s.node < 0 || s.node >= 4 || s.total < 0 || s.total > 8 ||
        s.solved < 0 || s.solved > s.total || s.errors < 0 || s.errors > 1000000 || s.skipped < 0 ||
        s.skipped > s.total || s.first_correct < 0 || s.first_correct > s.total ||
        (s.active && s.finished) || (s.correct && !s.checked))
        return invalid();
    for (int i=0;i<12;++i)
        if (!(in >> s.completed[i]) || (s.completed[i] != 0 && s.completed[i] != 1 && s.completed[i] != 3 && s.completed[i] != 7 && s.completed[i] != 15))
            return invalid();
    for (size_t i = 0; i < 12; ++i)
        if (!(in >> s.listened[i]) || (s.listened[i] & ~s.completed[i]))
            return invalid();
    size_t n = 0;
    if (!(in >> n) || n > 11)
        return invalid();
    std::set<int> slots;
    for (size_t i = 0; i < n; ++i) {
        GuidedStep q;
        int kind;
        if (!(in >> q.item >> kind >> q.slot >> q.reading) || q.item < 0 || q.item > 11 ||
            kind < 0 || kind > 4 || q.slot < 0 || q.slot >= s.total)
            return invalid();
        q.kind = GuidedKind(kind);
        if (kind != 0 && !slots.insert(q.slot).second)
            return invalid();
        s.pending.push_back(q);
    }
    if (!(in >> n) || n > 20)
        return invalid();
    std::set<int> selections;
    const auto opts = next.options();
    for (size_t i = 0; i < n; ++i) {
        int v;
        if (!(in >> v) || v < 0 || v >= int(opts.size()) || !selections.insert(v).second)
            return invalid();
        s.selected.push_back(v);
    }
    if (in >> extra)
        return invalid();
    const unsigned bits = (1u << s.total) - 1;
    if (s.active != !s.pending.empty() || (s.active && !next.unlocked(s.course, s.node)) ||
        (s.finished &&
         (s.solved != s.total || !s.total || !(s.completed[s.course] & (1u << s.node)))) ||
        s.solved_mask > bits || s.attempted_mask > bits || s.skipped_mask > bits ||
        (s.solved_mask & ~s.attempted_mask) || count_bits(s.solved_mask) != unsigned(s.solved) ||
        count_bits(s.skipped_mask) != unsigned(s.skipped) ||
        (s.checked &&
         (!s.active || s.pending.front().kind == GuidedKind::Introduce || s.selected.empty())) ||
        (s.active && s.pending.front().kind != GuidedKind::Order && s.selected.size() > 1))
        return invalid();
    static const char *legacy_ids[]={"daily-01","daily-02","daily-03","daily-04","tech-01","tech-02","tech-03","tech-04","work-01","work-02","work-03","work-04"};
    const auto completed=s.completed,listened=s.listened;
    const int old_course=s.course;s.completed={};s.listened={};
    for(int i=0;i<12;++i){
        auto it=std::find_if(guided_courses().begin(),guided_courses().end(),[&](const GuidedCourse&c){return c.id==legacy_ids[i];});
        if(it==guided_courses().end())return invalid();
        int mapped=int(it-guided_courses().begin());s.completed[mapped]=completed[i];s.listened[mapped]=listened[i];
        if(i==old_course)s.course=mapped;
    }
    if(s.total||s.active||s.finished)next.profile.last_course=s.course;
    *this = std::move(next);
    return true;
}
bool GuidedLearning::save(const std::string &path, std::string &error) const {
    const auto bytes=encode();
    if(bytes.size()>4*1024*1024+32768){error="Learning state exceeds safe storage limit; previous file retained";return false;}
    return atomic_write(path, bytes, error);
}
bool GuidedLearning::load(const std::string &path, std::string &error) {
    if (!recover_backup(path, error))
        return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "Cannot read guided progress";
        return false;
    }
    std::string bytes;
    char c;
    while (in.get(c)) {
        bytes += c;
        if (bytes.size() > 4*1024*1024+32768) {
            error = "Oversized guided progress";
            return false;
        }
    }
    return decode(bytes, error);
}
std::string GuidedLearning::encode() const {
    const auto g=encode_session(),r=review.encode();
    const auto payload=std::to_string(g.size())+" "+std::to_string(r.size())+"\n"+g+r;
    return "guided-v3 "+std::to_string(hash(payload))+"\n"+payload;
}
bool GuidedLearning::decode(const std::string &bytes,std::string &error) {
    if(bytes.rfind("guided-v1 ",0)==0)return decode_legacy(bytes,error);
    auto invalid=[&](){error="Invalid unified learning state/version/checksum";return false;};
    if(bytes.size()>4*1024*1024+32768)return invalid();
    const auto nl=bytes.find('\n');if(nl==std::string::npos)return invalid();
    std::istringstream header(bytes.substr(0,nl));std::string v,extra;uint32_t sum;
    if(!(header>>v>>sum)||(header>>extra)||(v!="guided-v2"&&v!="guided-v3")||hash(bytes.substr(nl+1))!=sum)return invalid();
    const auto start=bytes.find('\n',nl+1);if(start==std::string::npos)return invalid();
    size_t a=0,b=0;std::istringstream lengths(bytes.substr(nl+1,start-nl-1));
    if(!(lengths>>a>>b)||(lengths>>extra)||a>16384||b>4*1024*1024||a+b!=bytes.size()-start-1)return invalid();
    GuidedLearning next;
    if(!(v=="guided-v3"?next.decode_session(bytes.substr(start+1,a),error):next.decode_legacy(bytes.substr(start+1,a),error))||!next.review.decode(bytes.substr(start+1+a,b)))return invalid();
    *this=std::move(next);return true;
}
#include "guided_curriculum.inc"
} // namespace wordcard
