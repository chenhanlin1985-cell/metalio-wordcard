#include "learning_system.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace wordcard;
static int checks = 0;
static void check(bool x, const char *message) {
    ++checks;
    if (!x)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    try {
        check(argc == 2, "work path");
        std::vector<Card> cards;
        for (int i = 0; i < 30; ++i)
            cards.push_back({"id" + std::to_string(i), "word", "意思", "", "", ""});
        LearningSystem engine;
        engine.bind(&cards);
        engine.state.new_limit = 5;
        engine.state.memory[0]["id20"].seen = 1;
        engine.state.memory[0]["id20"].due_day = 99;
        check(engine.begin("test.csv", "", 100), "begin");
        check(engine.current()->id == "id20", "due words before new");
        check(engine.state.lesson.initial == 6, "global daily fresh budget");
        check(!engine.rate(Rating::Good, 100, true, 0), "no grade before reveal");
        engine.pause();
        check(!engine.reveal(), "paused cannot reveal");
        engine.resume();
        check(engine.reveal(), "reveal");
        check(engine.rate(Rating::Again, 100, true, 0), "rate due");
        check(engine.state.lesson.queue.back() == "id20", "spaced repeat queued");
        check(!engine.rate(Rating::Good, 100, true, 0), "duplicate stale event rejected");
        check(engine.undo(), "undo grade");
        check(engine.state.lesson.queue.size() == 6 && engine.state.lesson.position == 0,
              "undo restores queue");
        check(engine.state.days[100].attempts == 0 && engine.state.memory[0]["id20"].seen == 1,
              "undo all counters");
        check(engine.rate(Rating::Good, 100, true, 0), "regrade");
        while (engine.current()) {
            engine.reveal();
            check(engine.rate(Rating::Good, 100, true, engine.state.lesson.position),
                  "complete lesson");
        }
        check(engine.state.days[100].fresh == 5 && engine.state.days[100].reviewed == 1,
              "distinct day totals");
        check(engine.distinct_in_lesson() == 6, "distinct lesson total");
        check(!engine.begin("other.csv", "", 100), "new limit survives wordbook switch");
        check(engine.begin("test.csv", "", 101), "new budget next day");
        engine.reveal();
        engine.pause();
        const std::string path = std::string(argv[1]) + "/system-test-state.csv";
        std::string error;
        check(save_learning_state(path, engine.state, error), "save snapshot");
        LearningState restored;
        check(load_learning_state(path, restored, error), "load snapshot");
        check(restored.lesson.revealed && restored.lesson.paused && restored.lesson.position == 0,
              "restore exact phase");
        check(restored.memory[0].size() == 6 && restored.days[100].fresh == 5,
              "restore all memory");
        LearningSystem reboot;
        reboot.state = restored;
        reboot.bind(&cards);
        reboot.resume();
        check(reboot.validate_lesson("test.csv"), "resume id validation");
        check(!reboot.validate_lesson("different.csv"), "book mismatch");
        check(reboot.rate(Rating::Good, 101, true, 0), "post reboot grade");
        check(save_learning_state(path, reboot.state, error), "save with undo event");
        check(load_learning_state(path, restored, error), "restore undo event");
        reboot.state = restored;
        check(reboot.undo(), "undo after restart");
        check(reboot.state.lesson.position == 0, "undo recovered position");
        reboot.end();
        std::set<std::string> audio = {"id0", "id1"};
        check(reboot.begin("test.csv", "", 102, StudyMode::Listening, false, &audio), "listening");
        check(reboot.state.lesson.initial == 2, "exclude missing audio");
        check(reboot.validate_lesson_audio(audio), "listening resume with intact resources");
        check(!reboot.validate_lesson_audio({"id0"}) && reboot.state.lesson.position == 0,
              "missing remaining audio blocks resume without consuming question");
        const auto visual = reboot.state.memory[0].at("id0").seen;
        reboot.reveal();
        check(reboot.rate(Rating::Good, 102, true, 0), "grade listening");
        check(reboot.state.memory[0].at("id0").seen == visual &&
                  reboot.state.memory[1].at("id0").seen == 1,
              "independent direction");
        check(reboot.state.days[102].fresh == 0 && reboot.state.days[102].listening == 1,
              "listening not new vocabulary");
        reboot.end();
        reboot.state.favorites.insert("id4");
        check(reboot.begin("test.csv", "", 103, StudyMode::Visual, false, nullptr, true) &&
                  reboot.current()->id == "id4",
              "favorite selection");
        std::vector<CatalogBook> books;
        std::istringstream catalog("id,file,name,description\ntravel,travel.csv,旅行英语,日常\n");
        check(load_catalog(catalog, books, error) && books.size() == 1, "catalog import");
        for (const auto &bad : {"id,file,name,description\na,../x.csv,坏,\n",
                                "id,file,name,description\na,x.csv,一,\na,y.csv,二,\n"}) {
            std::istringstream in(bad);
            check(!load_catalog(in, books, error) && books.size() == 1,
                  "transactional invalid catalog");
        }
        check(valid_date(2028, 2, 29) && !valid_date(2026, 2, 29) && !valid_date(2026, 4, 31),
              "calendar bounds");
        reboot.state.player_book = "test.csv";
        reboot.state.player_group = "中文主题";
        reboot.state.player_id = "id9";
        check(save_learning_state(path, reboot.state, error) &&
                  load_learning_state(path, restored, error),
              "save player/favorites");
        check(restored.player_id == "id9" && restored.player_group == "中文主题" &&
                  restored.lesson.favorites && restored.favorites.count("id4"),
              "bookmark and favorites survive restart");
        std::ifstream imported(std::string(argv[1]) + "/imported-decks.csv");
        std::vector<Card> imported_cards;
        check(load_deck(imported, imported_cards, error) &&
                  imported_cards[0].id == "custom:daily:one" && !imported_cards[0].ipa.empty(),
              "real importer output accepted by firmware");
        std::ifstream imported_audio(std::string(argv[1]) + "/imported-audio.d.csv");
        std::map<std::string, AudioEntry> imported_index;
        check(load_audio_index(imported_audio, imported_index, error) &&
                  imported_index.count(imported_cards[0].id),
              "imported audio linked by real parser");
        std::ifstream imported_catalog(std::string(argv[1]) + "/imported-catalog.d.csv");
        check(load_catalog(imported_catalog, books, error) &&
                  books[0].filename == "custom-daily.csv",
              "imported catalog accepted by firmware");
        check(reverse_prompt("adj. 美丽的。What a beautiful day!").find("beautiful") ==
                  std::string::npos,
              "reverse masks English examples");
        LearningSystem practice;
        practice.bind(&cards);
        practice.state.lesson_size = 5;
        check(practice.begin("test.csv", "", 200), "bounded short lesson");
        size_t attempts = 0;
        while (practice.current() && attempts < 30) {
            practice.reveal();
            practice.rate(Rating::Again, 200, true, practice.state.lesson.position);
            ++attempts;
        }
        check(attempts == 10 && !practice.current(), "repeat cap gives explicit endpoint");
        check(practice.state.days[200].fresh == 5 && practice.state.days[200].reviewed == 0 &&
                  practice.state.days[200].attempts == 10,
              "repeated failures count distinct words once");
        std::set<std::string> all_audio;
        for (const auto &c : cards)
            all_audio.insert(c.id);
        const auto recent_id = practice.state.events.back().id;
        check(practice.begin("test.csv", "", 200, StudyMode::Listening, false, &all_audio),
              "cross-direction lesson");
        check(std::find(practice.state.lesson.queue.begin(), practice.state.lesson.queue.end(),
                        recent_id) == practice.state.lesson.queue.end(),
              "recently revealed words not immediately retested in another direction");
        check(!practice.begin("test.csv", "", 200), "active lesson cannot be replaced");
        practice.end();
        practice.state.new_limit = 0;
        practice.begin("test.csv", "", 200);
        check(std::all_of(practice.state.lesson.queue.begin(), practice.state.lesson.queue.end(),
                          [&](const auto &id) {
                              return practice.state.memory[0].count(id) &&
                                     practice.state.memory[0].at(id).seen;
                          }),
              "zero fresh quota cannot open new words");
        practice.end();
        check(practice.begin("test.csv", "", 201), "zero fresh quota still permits due review");
        {
            std::ofstream bad(path, std::ios::binary);
            bad << "#wordcard-state-v2,123\nA,broken\n";
        }
        const auto old = restored.sequence;
        check(!load_learning_state(path, restored, error) && restored.sequence == old,
              "corruption protects in-memory state");
        std::filesystem::remove(path);
        check(load_learning_state(path, restored, error), "recover previous committed backup");
        std::cout << "PASS " << checks
                  << " learning-system checks: daily budgets, review priority, resume, undo, "
                     "independent listening, migration-safe state, catalog and date\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
