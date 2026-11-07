#include "wordcard_ui.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace wordcard;
namespace fs = std::filesystem;
static int checks = 0;
static void require(bool pass, const std::string &why) {
    ++checks;
    if (!pass)
        throw std::runtime_error(why);
}
static void answer(GuidedLearning &g) {
    auto v = g.view();
    if (v.kind == GuidedKind::Order) {
        std::istringstream in(v.text);
        std::string word;
        std::set<int> used;
        while (in >> word) {
            int found = -1;
            for (int i = 0; i < int(v.options.size()); ++i)
                if (!used.count(i) && v.options[i] == word) {
                    found = i;
                    break;
                }
            require(found >= 0, "word bank contains every token");
            used.insert(found);
            require(g.choose(found), "pick token");
        }
    } else {
        const auto target = v.kind == GuidedKind::ListenWord && !v.reading ? v.text : v.meaning;
        auto it = std::find(v.options.begin(), v.options.end(), target);
        require(it != v.options.end(), "correct choice present");
        require(g.choose(int(it - v.options.begin())), "select choice");
    }
}
static void roundtrip(GuidedLearning &g) {
    GuidedLearning restored;
    std::string error;
    require(restored.decode(g.encode(), error), "roundtrip " + error);
    require(g.encode() == restored.encode(), "lossless state roundtrip");
    g = std::move(restored);
}
int main(int argc, char **argv) {
    try {
        require(argc == 3, "font and isolated output arguments");
        fs::create_directories(argv[2]);
        std::ifstream f(argv[1], std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), {}), fb(Canvas::bytes);
        Font font;
        require(font.init(data.data(), data.size()), "device font");
        Canvas c(fb.data(), font);
        UI ui;
        // Exercise the retained sentence-builder renderer explicitly: current
        // listening/reading courses do not otherwise visit every legacy kind.
        for(const auto &tokens:std::vector<std::vector<std::string>>{
            {"Are","you","available","on","Friday","morning","?"},
            {"Send","the","log","and","the","steps","to","reproduce","the","crash","."}}){
            View v;v.guided_enabled=true;v.screen=Screen::GuidedLesson;
            v.guided.active=true;v.guided.kind=GuidedKind::Order;v.guided.options=tokens;
            ui.render(c,v);
            for(const auto &b:ui.buttons())if(b.event.action==Action::GuidedChoose){
                require(c.text_width_px(b.label,36)<=b.rect.w-20,"whole sentence tile at displayed size: "+b.label);
                require(b.rect.y+b.rect.h<=906,"sentence tiles stay above undo");
            }
        }
        auto render = [&](GuidedLearning &g, Screen screen, const std::string &name = "") {
            View v;
            v.guided_enabled = true;
            v.guided = g.view();
            v.screen = screen;
            v.writable = true;
            v.audio_available = true;
            v.audio_source = SpeechSource::Sd;
            if(screen==Screen::GuidedListen){
                const auto &item=guided_courses()[v.guided.course].items[6];
                v.guided.text=item.card.front;v.guided.ipa=item.card.ipa;v.guided.meaning=item.meaning;
            }
            ui.render(c, v);
            for (const auto &b : ui.buttons()) {
                require(b.rect.x >= 0 && b.rect.y >= 0 && b.rect.x + b.rect.w <= 684 &&
                            b.rect.y + b.rect.h <= 1216,
                        "button within canvas: " + b.label);
                require(c.wrap(b.label, b.rect.w - 16).size() <= size_t(std::max(1, b.rect.h / 38)),
                        "untruncated button: " + b.label);
                if (v.guided.kind == GuidedKind::Order && b.event.action == Action::GuidedChoose)
                    require(c.text_width_px(b.label,36) <= b.rect.w-20,
                            "complete sentence token at actual button size: " + b.label);
                if (b.enabled) {
                    auto e = ui.hit(b.rect.x + b.rect.w / 2, b.rect.y + b.rect.h / 2);
                    require(e.action == b.event.action && e.value == b.event.value,
                            "button hit matches " + b.label);
                }
            }
            for (size_t i = 0; i < ui.buttons().size(); ++i)
                for (size_t j = i + 1; j < ui.buttons().size(); ++j) {
                    const auto &a = ui.buttons()[i].rect, &b = ui.buttons()[j].rect;
                    require(a.x >= b.x + b.w || b.x >= a.x + a.w || a.y >= b.y + b.h ||
                                b.y >= a.y + a.h,
                            "buttons do not overlap");
                }
            if (!name.empty()) {
                std::ofstream out(fs::path(argv[2]) / (name + ".pgm"), std::ios::binary);
                out << "P5\n684 1216\n255\n";
                for (int y = 0; y < 1216; ++y)
                    for (int x = 0; x < 684; ++x)
                        out.put(char(c.shade(x, y) * 17));
            }
        };
        GuidedLearning g;
        render(g, Screen::Home, "00-course-path");
        render(g, Screen::GuidedCourses, "01-course-picker");
        render(g, Screen::GuidedListen, "01b-sentence-listening");
        require(!g.begin(0, 1), "locked lesson refused");
        require(!g.begin(-1, 0) && !g.begin(guided_course_count, 0), "bad course refused");
        require(g.begin(0, 0), "begin lesson");
        require(!g.begin(4, 0), "cannot replace unfinished lesson");
        render(g, Screen::GuidedLesson, "02-new-word");
        roundtrip(g);
        require(!g.check(), "introductions not graded");
        require(g.advance(), "start exercise");
        require(!g.check() && !g.advance(), "must choose then check");
        render(g,Screen::GuidedLesson,"02b-choose-meaning");
        auto v = g.view();
        int wrong = 0;
        while (v.options[wrong] == v.meaning)
            ++wrong;
        require(g.choose(wrong),"select wrong option");
        render(g,Screen::GuidedLesson,"02c-selected-answer");
        require(g.check() && !g.state.correct, "wrong answer");
        require(!g.check() && !g.choose(0), "feedback cannot double score or alter answer");
        roundtrip(g);
        render(g, Screen::GuidedLesson, "03-wrong-feedback");
        require(g.advance() && g.state.pending.back().item == 0, "wrong question queued later");
        render(g, Screen::GuidedPause, "04-pause");
        int guard = 0;
        while (g.state.active) {
            require(++guard < 50, "bounded wrong retry flow");
            render(g, Screen::GuidedLesson);
            if (g.view().kind != GuidedKind::Introduce) {
                if(g.listening())render(g, Screen::GuidedLesson, "05-listening");
                answer(g);
                if (g.listening()) {
                    require(!g.check(), "unheard audio never graded");
                    g.played(false);
                    require(!g.check(), "failed playback not graded");
                    g.played(true);
                }
                require(g.check() && g.state.correct, "correctly grade question");
            }
            roundtrip(g);
            require(g.advance(), "advance after checking");
        }
        require(g.state.errors == 1 && g.state.first_correct == 5 && g.state.solved == 6,
                "honest summary after retry");
        render(g, Screen::GuidedDone, "06-section-complete");
        render(g, Screen::Home, "07-next-node");
        std::string error;
        const auto statefile = (fs::path(argv[2]) / "progress.txt").string();
        require(g.save(statefile, error), "atomic save");
        require(g.save(statefile, error), "second commit preserves backup");
        fs::remove(statefile);
        GuidedLearning recovered;
        require(recovered.load(statefile, error), "recover interrupted rename from backup");
        require(recovered.encode() == g.encode(), "recover actual session");
        auto corrupt = g.encode();
        corrupt.back() = 'x';
        const auto before = recovered.encode();
        require(!recovered.decode(corrupt, error) && recovered.encode() == before,
                "checksum failure leaves in-memory state untouched");
        require(!recovered.decode(g.encode().substr(0, 40), error), "truncated state rejected");
        // Exercise all 48 lessons using real content, including listening skips and duplicate word
        // tokens.
        for (int course = 0; course < 12; ++course) {
            GuidedLearning all;
            for (int node = 0; node < 4; ++node) {
                require(all.begin(course, node), "sequential unlock");
                if(course==4 && node==0){render(all,Screen::GuidedCourses,"08a-tech-courses");render(all,Screen::GuidedListen,"08b-tech-listening");}
                int steps = 0;
                bool skipped = false;
                while (all.state.active) {
                    require(++steps < 30, "normal lesson terminates");
                    auto view = all.view();
                    const auto *item = all.current();
                    require(item && !item->card.ipa.empty() && !item->meaning.empty(),
                            "complete existing content");
                    if (view.kind == GuidedKind::Introduce) {
                        require(c.wrap(view.text, 604, 2).size() <= 3,
                                "intro English fits: " + view.text);
                        require(c.wrap(view.ipa, 604).size() <= 3, "IPA fits: " + view.text);
                    }
                    render(all, Screen::GuidedLesson);
                    if (course == 4 && node == 2 && steps == 1)
                        render(all, Screen::GuidedLesson, "08-tech-sentence");
                    if (view.kind != GuidedKind::Introduce) {
                        if (all.listening() && !skipped) {
                            require(all.skip_listening(), "skip listening without penalty");
                            skipped = true;
                        }
                        if (view.kind == GuidedKind::Order) {
                            require(all.choose(0) && !all.choose(0),
                                    "tile can be picked only once");
                            require(all.remove(0), "remove selected tile");
                        if (course == 4 && node==2 && steps == 2)
                                render(all, Screen::GuidedLesson, "09-word-bank");
                        }
                        answer(all);
                        if (all.listening())
                            all.played(true);
                        require(all.check() && all.state.correct,
                                "all 12 course items grade correctly");
                        if (course == 4 && node == 2 && steps == 2)
                            render(all, Screen::GuidedLesson, "10-correct-feedback");
                    }
                    roundtrip(all);
                    require(all.advance(), "next question");
                }
                require(all.state.skipped == 1 && all.state.errors == 0,
                        "skip not recorded as hearing or failure");
                require(all.state.listened[course]==0,"skipped hearing remains incomplete on the course path");
                render(all, Screen::GuidedDone);
            }
            require(all.state.completed[course] == 15, "four original sections retained; new dialogue sections still pending");
            if (course == 4) {
                render(all, Screen::GuidedDone, "11-course-complete");
                render(all, Screen::Home, "12-tech-path");
            }
        }
        std::cout << "PASS: " << checks
                  << " guided learning, persistence, answer, all-course UI checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
}
