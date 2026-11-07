#include "learning_web.hpp"
#include "native_json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
using namespace wordcard;
using J = StudyJson;
namespace fs = std::filesystem;
static int checks = 0, actions = 0;
static void check(bool b, const std::string &s) {
    ++checks;
    if (!b)
        throw std::runtime_error(s);
}
static J read(const fs::path &p) {
    std::ifstream f(p);
    return J::parse(std::string(std::istreambuf_iterator<char>(f), {}));
}
static J status(LearningWeb &m) {
    std::string v;
    check(m.status(v), "status");
    return J::parse(v);
}
static WebPage page(LearningWeb &m) {
    WebPage p;
    check(m.page(p), m.error());
    return p;
}
static void action(LearningWeb &m, const std::string &a, const std::string &key = "",
                   const J &value = nullptr) {
    const auto p = page(m);
    for (size_t i = 0; i < p.actions.size(); ++i) {
        const auto v = J::parse(p.actions[i]);
        if (str(v, "action") == a && (key.empty() || str(v, key) == str(value))) {
            check(m.action(int(i)), m.error());
            ++actions;
            page(m);
            return;
        }
    }
    throw std::runtime_error("Missing visible action " + a + " at " + p.route);
}
int main(int argc, char **argv) {
    try {
        check(argc == 3, "fixture/output required");
        const fs::path fixture = argv[1], out = argv[2], dir = out / "profile";
        check(!fs::exists(out), "Use a new test output; never overwrite a learner profile");
        fs::create_directories(dir);
        for (const auto *name : {"course-v2.json", "profile-v2.json"})
            fs::copy_file(fixture / name, dir / name);
        const auto answers = read(fixture / "answers.json");
        {
            auto s = read(dir / "course-v2.json");
            s["nativeMigrationSentinel"] = J{{"keep", true}, {"nested", J::array({1, "未来字段"})}};
            std::ofstream(dir / "course-v2.json") << s.dump();
        }
        auto m = std::make_unique<LearningWeb>();
        check(m->init(dir.string()), m->error());
        page(*m);
        check(m->native_active(), "Home must enter C++");
        const auto initialScripts = m->script_calls();
        bool lookup = false, audio = false, storage = false, pause = false;
        int rounds = 0;
        for (; rounds < 180; ++rounds) {
            const auto st = status(*m);
            check(!has(st, "courseError") || !flag(st, "courseError"), "No model error");
            check(m->native_active(), "Every learning route stays native");
            check(m->script_calls() == initialScripts, "No JavaScript call during native flow");
            const auto s = read(dir / "course-v2.json");
            check(flag(s["nativeMigrationSentinel"], "keep"), "Preserve unknown fields");
            const auto b = field(s, "batch"), q = field(s, "session");
            if (str(st["next"], "type") == "done" && b.is_null() && q.is_null())
                break;
            if (str(st, "route") == "home" || str(st, "route") == "word-result" ||
                str(st, "route") == "focus-summary") {
                action(*m, "w-day-next");
                continue;
            }
            if (!b.is_null()) {
                const auto phase = str(b, "phase");
                if (phase == "overview") {
                    action(*m, "w-begin");
                    continue;
                }
                if (phase == "learn") {
                    if (!lookup) {
                        const auto before = b;
                        action(*m, "lookup");
                        for (char c : std::string("city"))
                            check(m->key("key", std::string(1, c)), "lookup key");
                        check(m->key("submit"), "lookup submit");
                        page(*m);
                        check(str(status(*m), "route") == "word", "lookup detail");
                        action(*m, "audio");
                        std::string p;
                        int serial;
                        bool stop;
                        check(m->take_audio(p, serial, stop) && !stop && !m->audio_text(p).empty(),
                              "lookup audio");
                        check(m->playback(serial, true), "lookup audio completion");
                        page(*m);
                        action(*m, "add");
                        action(*m, "lookup-return");
                        check(read(dir / "course-v2.json")["batch"] == before, "Lookup exact saved position");
                        lookup = true;
                    }
                    if (!pause) {
                        action(*m, "w-pause");
                        check(str(status(*m), "route") == "home", "Pause saves and returns");
                        action(*m, "w-day-next");
                        pause = true;
                    }
                    action(*m, "w-learn", "delta", "1");
                    continue;
                }
                if (!field(b, "checked").is_null()) {
                    check(flag(b["checked"], "correct"), "Expected correct batch answer");
                    action(*m, "w-next");
                    continue;
                }
                const auto item = b["queue"][size_t(num(b, "at"))];
                const auto id = str(item, "id"), kind = str(item, "kind");
                if (kind == "meaning") {
                    action(*m, "w-choose", "n", b["choices"][id]["answer"]);
                    action(*m, "w-check");
                } else {
                    if (!storage) {
                        const auto before = status(*m);
                        fs::rename(dir, out / "held");
                        check(m->type("x"), "Storage error surfaced as native page");
                        check(status(*m)["batch"] == before["batch"], "Failed SD write cannot advance input");
                        fs::rename(out / "held", dir);
                        check(m->command("error-close"), "Dismiss and retry native storage failure");
                        page(*m);
                        storage = true;
                    }
                    const auto answer =
                        kind == "gap" ? str(b["gaps"][id], "answer") : str(answers["catalog"][id], "word");
                    check(m->type(answer), "Native spelling");
                    page(*m);
                    check(m->key("submit"), "Native submit");
                    page(*m);
                }
                continue;
            }
            if (!q.is_null()) {
                if (!field(q, "checked").is_null()) {
                    check(flag(q["checked"], "correct"), "Correct course answer");
                    action(*m, "next");
                    continue;
                }
                const auto question = q["queue"][size_t(num(q, "at"))];
                const J *task = nullptr;
                for (const auto &t : answers["tasks"][str(q, "lesson")])
                    if (t["id"] == question) {
                        task = &t;
                        break;
                    }
                check(task != nullptr, "Course question mapping");
                if (str(*task, "kind") == "listen" && !truth(field(q["heard"], str(num(q, "at"))))) {
                    action(*m, "audio");
                    std::string p;
                    int serial;
                    bool stop;
                    check(m->take_audio(p, serial, stop) && !stop, "Listen request");
                    check(m->playback(serial + 1, true), "Stale callback ignored");
                    check(!flag(status(*m)["session"], "heard"), "Stale callback has no credit");
                    if (!audio) {
                        action(*m, "audio");
                        check(m->take_audio(p, serial, stop) && stop, "Stop request");
                        check(!flag(status(*m)["session"], "heard"), "Stop has no credit");
                        action(*m, "audio");
                        check(m->take_audio(p, serial, stop) && !stop, "Replay request");
                        const auto cancelled = serial;
                        check(m->cancel_audio(), "App audio cancellation stays native");
                        check(m->take_audio(p, serial, stop) && stop, "App cancellation stops output");
                        check(m->playback(cancelled, true), "Cancelled callback accepted without credit");
                        check(!flag(status(*m)["session"], "heard"),
                              "App cancellation cannot earn listening credit");
                        page(*m);
                        action(*m, "audio");
                        check(m->take_audio(p, serial, stop) && !stop, "Replay after app cancel");
                        audio = true;
                    }
                    check(m->playback(serial, true), "Complete audio");
                    page(*m);
                }
                action(*m, "choose", "n", (*task)["answer"]);
                action(*m, "check");
                continue;
            }
            throw std::runtime_error("Unexpected route " + str(st, "route"));
        }
        check(rounds < 180 && lookup && audio && storage && pause,
              "Full due review/new/lookup/listen/read flow");
        const auto final = status(*m);
        for (const auto &t : final["stages"])
            check(t["done"] == t["total"], "All daily stages complete");
        const auto saved = read(dir / "course-v2.json");
        check(m->cancel_audio(), "App Home cancels through C++");
        check(m->home(), "Home");
        check(m->script_calls() == initialScripts, "App Home must not enter JavaScript");
        page(*m);
        action(*m, "go", "route", "books");
        check(!m->native_active(), "Cold catalogue boundary");
        check(m->home(), "Return from catalogue");
        page(*m);
        check(m->native_active(), "Return to native");
        check(read(dir / "course-v2.json") == saved, "Cold boundary preserves progress");
        for(const char *route:{"books","dictionary"}) {
            const auto beforeCalls=m->script_calls();
            action(*m,"go","route",route);
            check(!m->native_active(),"Root navigation reaches legacy catalogue/dictionary");
            check(m->script_calls()==beforeCalls+1,"Root handoff is one restore/navigation command");
            check(page(*m).route==route,"Target root visible");
            check(m->home(),"Return from read-only root");page(*m);
            check(m->native_active(),"Native root restored after navigation");
            check(read(dir / "course-v2.json")==saved,"Root handoff never writes progress");
        }
        m.reset();
        m = std::make_unique<LearningWeb>();
        check(m->init(dir.string()), m->error());
        page(*m);
        check(m->native_active(), "Restart native");
        check(status(*m)["stages"] == final["stages"], "Restart preserves completion");
        check(read(dir / "course-v2.json") == saved, "Read-only restart does not rewrite progress");
        std::cout << "PASS native facade: " << checks << " checks, " << actions
                  << " visible actions; zero JS calls through study, storage rollback, audio gates, lookup, "
                     "cold handoff and restart\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
