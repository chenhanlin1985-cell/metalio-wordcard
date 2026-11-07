#include "learning_web.hpp"
#include "vocabulary.hpp"
#include "wordcard_ui.hpp"
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <algorithm>
using namespace wordcard;
namespace fs = std::filesystem;
static int checks = 0;
void check(bool x, const std::string &why) {
    ++checks;
    if (!x)
        throw std::runtime_error(why);
}
std::string read(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}
VJ state(const fs::path &dir) {
    auto e = VJ::parse(read(dir / "vocabulary/state.json"));
    auto frame = VJ::parse(str(e, "data"));
    auto s = frame["state"];
    std::ifstream log(dir / "vocabulary/events.ndjson", std::ios::binary);
    log.seekg(num(frame, "journalOffset"));
    std::string line;
    while (std::getline(log, line)) {
        auto j = VJ::parse(str(VJ::parse(line), "data"));
        s = s.patch(j["patch"]);
    }
    return s;
}
// Synthetic profiles only: exercise production model projections and the actual
// Canvas renderer without importing, answering, or rewriting any user profile.
void layout_checks(const fs::path &resources, const fs::path &out, const Font &font) {
    VocabularyCatalog catalog;
    check(catalog.load(resources.generic_string()), "Layout catalog");
    VocabularyEngine engine(catalog);
    constexpr int64_t now = 1793750400000LL;
    auto reduce = [&](const VJ &s, const VJ &a) {
        VJ events;
        return engine.reduce(s, a, now, events);
    };
    auto fresh = engine.create(now);
    auto chosen = reduce(fresh, VJ{{"type", "chooseStage"}, {"stage", "foundation"}});
    auto learning = reduce(chosen, VJ{{"type", "begin"}, {"intent", "new"}});
    auto advanceLearn = [&](const VJ &s) {
        return reduce(s, VJ{{"type", "learnNext"}, {"sessionId", s["session"]["id"]},
                            {"learnAt", s["session"]["learnAt"]}});
    };
    auto paused = advanceLearn(advanceLearn(learning));
    auto meaning = learning;
    while (str(meaning["session"], "phase") == "learn") meaning = advanceLearn(meaning);
    auto task = engine.current(meaning);
    auto feedback = reduce(meaning, VJ{{"type", "answer"}, {"taskId", task["uid"]},
                                       {"value", catalog.get(str(task, "id"))["meaning"]}});
    auto completed = chosen;
    VJ newWords = VJ::array();
    for (const auto &e : catalog.entries)
        if (e.eligible && e.stage == "foundation" && newWords.size() < 10) {
            completed["seen"][e.id] = now;
            newWords.push_back(e.id);
        }
    completed["days"][VocabularyEngine::date(now)] =
        VJ{{"new", newWords}, {"reviews", VJ::array()}, {"answers", 30}};
    auto due = completed;
    const auto wordId = str(newWords[0]);
    due["cards"][wordId]["meaning"] = VocabularyEngine::empty_card(now);
    auto exhausted = chosen;
    for (const auto &e : catalog.entries)
        if (e.eligible && e.stage == "foundation") exhausted["seen"][e.id] = now;
    auto unchosenComplete = completed;
    unchosenComplete["stageChosen"] = false;

    UI ui;
    View view;
    view.screen = Screen::WebLearning;
    std::vector<uint8_t> pixels(Canvas::bytes);
    auto render = [&](const WebPage &p, const std::string &name, int pageIndex = 0) {
        view.web = p;
        view.web_page = pageIndex;
        Canvas canvas(pixels.data(), font);
        ui.render(canvas, view);
        std::ofstream image(out / "frames" / (name + ".pgm"), std::ios::binary);
        image << "P5\n684 1216\n255\n";
        for (int y = 0; y < 1216; ++y)
            for (int x = 0; x < 684; ++x) {
                const char shade = char(canvas.shade(x, y) * 17);
                image.write(&shade, 1);
            }
        VJ controls = VJ::array();
        const auto &buttons = ui.buttons();
        for (size_t i = 0; i < buttons.size(); ++i) {
            const auto &b = buttons[i];
            check(b.rect.x >= 0 && b.rect.y >= 0 && b.rect.x + b.rect.w <= 684 &&
                      b.rect.y + b.rect.h <= 1216, "Layout bounds " + name + ": " + b.label);
            for (size_t j = 0; j < i; ++j) {
                const auto &r = buttons[j].rect;
                check(b.rect.x >= r.x + r.w || r.x >= b.rect.x + b.rect.w ||
                          b.rect.y >= r.y + r.h || r.y >= b.rect.y + b.rect.h,
                      "Layout touch targets overlap " + name + ": " + b.label + "/" + buttons[j].label);
            }
            controls.push_back(VJ{{"label", b.label}, {"x", b.rect.x}, {"y", b.rect.y},
                                  {"w", b.rect.w}, {"h", b.rect.h}, {"enabled", b.enabled},
                                  {"primary", b.primary}, {"action", int(b.event.action)},
                                  {"value", b.event.value}});
        }
        std::ofstream(out / "frames" / (name + ".json")) <<
            VJ{{"title", p.title}, {"route", p.route}, {"page", pageIndex},
               {"pages", ui.answer_pages()}, {"buttons", controls},
               {"synthetic", true}, {"realDevice", false}}.dump(2);
    };
    auto button = [&](const std::string &label) -> Button {
        for (const auto &b : ui.buttons()) if (b.label == label) return b;
        throw std::runtime_error("Missing rendered control " + label);
    };
    auto footerButton = [&](const std::string &label) -> Button {
        for (const auto &b : ui.buttons()) if (b.label == label && b.rect.y >= 700) return b;
        throw std::runtime_error("Missing footer control " + label);
    };
    auto role = [&](const WebPage &p, const std::string &name) -> std::string {
        for (const auto &b : p.blocks) if (b.role == name) return b.text;
        throw std::runtime_error("Missing home role " + name);
    };
    auto open = [&](const std::string &name, const VJ &s) {
        const auto dir = out / ("learning-web-layout-" + name);
        fs::create_directories(dir / "vocabulary");
        const auto data = VJ{{"state", s}, {"journalOffset", 0}}.dump();
        uint32_t hash = 2166136261u;
        for (const unsigned char c : data) hash = (hash ^ c) * 16777619u;
        std::ofstream(dir / "vocabulary/state.json", std::ios::binary)
            << VJ{{"data", data}, {"checksum", hash}}.dump() << '\n';
        auto model = std::make_unique<NativeVocabulary>(dir.generic_string());
        check(model->enter(VJ(), now), "Open synthetic layout " + name);
        check(str(VJ::parse(model->status()), "courseError").empty(), "Layout fixture loads " + name);
        return model;
    };
    auto dispatch = [&](NativeVocabulary &model, const std::string &action,
                        const std::string &key = "", const std::string &value = "") {
        const auto p = model.page();
        for (const auto &raw : p.actions) {
            const auto a = VJ::parse(raw);
            if (str(a, "action") == action && (key.empty() || str(a, key) == value)) {
                check(model.dispatch(raw), "Layout action " + action);
                check(str(VJ::parse(model.status()), "courseError").empty(), "Layout action succeeds " + action + ": " + str(VJ::parse(model.status()), "courseError"));
                return;
            }
        }
        std::string available;
        for (const auto &raw : p.actions) available += "\n  " + raw;
        throw std::runtime_error("Missing native layout action " + action + " " + key + "=" + value +
                                 " on " + p.route + " (" + p.title + "); available:" + available);
    };
    auto home = [&](const std::string &name, const VJ &s, const std::string &label,
                    const std::string &next, bool enabled = true) {
        auto model = open(name, s);
        const auto dir = out / ("learning-web-layout-" + name);
        const auto checkpoint = read(dir / "vocabulary/state.json");
        const auto p = model->page();
        render(p, name);
        check(ui.answer_pages() == 1, "Home never paginates " + name);
        const auto primary = button(label);
        check(primary.primary && primary.rect.y == 980 && primary.rect.w == 636 &&
                  primary.rect.h == 100 && primary.enabled == enabled, "Home single bottom CTA " + name);
        const auto primaryAction = VJ::parse(p.actions.at(size_t(primary.event.value)));
        check(label == "选择下一词库" || label == "查看暂停词组" || label == "返回生词本"
                  ? str(primaryAction, "action") == "v-go" &&
                    str(primaryAction, "value") == (label == "选择下一词库" ? "levels" : label == "返回生词本" ? "notebook" : "paused")
                  : str(primaryAction, "action") == "v-start" && str(primaryAction, "intent") == "new" && flag(primaryAction, "allowExtra"), "Home CTA explicitly starts or resumes new learning " + name);
        check(std::count_if(ui.buttons().begin(), ui.buttons().end(),
                  [](const Button &b) { return b.primary; }) == 1, "Exactly one home primary " + name);
        check(std::count_if(ui.buttons().begin(), ui.buttons().end(),
                  [](const Button &b) { return b.label == "设置"; }) == 1, "One home settings entry " + name);
        check(button("设置").rect.y == 12 && ui.buttons().size() == 6,
              "Home exposes settings, primary and four destinations " + name);
        if (!next.empty()) check(role(p, "vocab-home-next") == next, "Home next-step copy " + name);
        const auto hit = ui.hit(primary.rect.x + 12, primary.rect.y + 12);
        check(enabled ? hit.action == Action::WebAction : hit.action == Action::None,
              "Home CTA enabled state affects touch " + name);
        for (const auto *destination : {"settings", "levels", "review", "search", "home"}) {
            check(model->dispatch(VJ{{"action", "v-go"}, {"value", destination}}.dump()), "Read-only route");
            model->page();
        }
        model->page();
        check(state(dir) == s && read(dir / "vocabulary/state.json") == checkpoint &&
                  !fs::exists(dir / "vocabulary/events.ndjson"),
              "Render and navigation preserve every progress byte " + name);
        return model;
    };
    home("layout-home-fresh", fresh, "选择起点", "每组几个词，学完再一起检查");
    home("layout-home-ready", chosen, "开始背词", "每组几个词，学完再一起检查");
    home("layout-home-paused", paused, "继续学习", "认识单词 · 3/5");
    home("layout-home-meaning", meaning, "继续学习", "词义检查 · 1/5");
    home("layout-home-feedback", feedback, "继续学习", "词义检查 · 1/5 · 查看反馈");
    home("layout-home-complete", completed, "再学一组", "已达今日目标，还可以再学一组");
    auto dueHome = home("layout-home-due", due, "再学一组", "已达今日目标，还可以再学一组");
    home("layout-home-unchosen-complete", unchosenComplete, "选择起点", "");
    auto exhaustedModel = home("layout-home-exhausted", exhausted, "选择下一词库",
                               "本档新词已学完，可以选择下一档");
    dispatch(*exhaustedModel, "v-go", "value", "levels");
    check(exhaustedModel->page().route == "levels", "Exhaustion opens levels without begin error");
    dispatch(*dueHome, "v-start", "intent", "new");
    check(str(state(out / "learning-web-layout-layout-home-due")["session"], "source") == "new",
          "Home starts another new group even when a review is due and the daily goal is reached");

    auto almostGoal = chosen;
    almostGoal["serial"] = 600;
    almostGoal["settings"] = VJ{{"mode", "staged"}, {"dailyNew", 5}};
    almostGoal["days"][VocabularyEngine::date(now)] = VJ{
        {"new", VJ::array({newWords[0], newWords[1], newWords[2]})},
        {"reviews", VJ::array()}, {"answers", 0}};
    for (size_t i = 0; i < 3; ++i) almostGoal["seen"][str(newWords[i])] = now;
    auto softGoalHome = home("layout-home-soft-goal", almostGoal, "开始背词", "每组几个词，学完再一起检查");
    const auto softGoalDir = out / "learning-web-layout-layout-home-soft-goal";
    dispatch(*softGoalHome, "v-start", "intent", "new");
    check(state(softGoalDir)["session"]["ids"].size() == 5,
          "Home's explicit start keeps a complete group when today is three of a five-word goal");
    for (int i = 0; i < 5; ++i) dispatch(*softGoalHome, "v-do", "type", "learnNext");
    const auto softGoalState = state(softGoalDir);
    check(str(softGoalState["session"], "phase") == "tasks" &&
              str(engine.current(softGoalState), "kind") == "meaning" &&
              softGoalState["days"][VocabularyEngine::date(now)]["new"].size() == 8,
          "Visible soft-goal learning reaches a real meaning check and records all five presentations");
    render(softGoalHome->page(), "layout-soft-goal-meaning");
    check(button("不认识").rect.y == 1080, "Soft-goal group retains the actual meaning-question controls");

    auto choiceModel = open("stage-choice", paused);
    const auto choiceDir = out / "learning-web-layout-stage-choice";
    const auto choiceSnapshot = read(choiceDir / "vocabulary/state.json");
    dispatch(*choiceModel, "v-go", "value", "levels");
    dispatch(*choiceModel, "v-stage", "value", "everyday");
    check(choiceModel->page().route == "levels" && state(choiceDir) == paused &&
              read(choiceDir / "vocabulary/state.json") == choiceSnapshot &&
              !fs::exists(choiceDir / "vocabulary/events.ndjson"),
          "Choosing a different level only changes the local selection until confirmation");
    render(choiceModel->page(), "layout-level-selection");
    check(button("使用所选词库").primary && button("使用所选词库").rect.y == 980,
          "Level confirmation stays at the bottom above navigation");
    dispatch(*choiceModel, "v-back");
    check(choiceModel->page().route == "home" && state(choiceDir) == paused,
          "Canceling level selection returns home without parking the current group or writing progress");
    dispatch(*choiceModel, "v-go", "value", "levels");
    dispatch(*choiceModel, "v-stage", "value", "foundation");
    dispatch(*choiceModel, "v-use-stage", "value", "foundation");
    check(choiceModel->page().route == "home" && state(choiceDir) == paused &&
              !fs::exists(choiceDir / "vocabulary/events.ndjson"),
          "Confirming the current level does not write a revision or park the active group");
    dispatch(*choiceModel, "v-go", "value", "levels");
    dispatch(*choiceModel, "v-stage", "value", "everyday");
    dispatch(*choiceModel, "v-use-stage", "value", "everyday");
    const auto changedChoice = state(choiceDir);
    check(choiceModel->page().route == "home" && str(changedChoice, "selectedStage") == "everyday" &&
              changedChoice["session"].is_null() && changedChoice["paused"].size() == 1 &&
              changedChoice["paused"][0] == paused["session"],
          "Confirmed level change preserves the exact previous session in paused groups");

    auto model = open("footers", learning);
    dispatch(*model, "v-start");
    auto learnPage = model->page();
    render(learnPage, "layout-learn");
    render(learnPage, "layout-learn-last", ui.answer_pages() - 1);
    const auto wordAudio = button("听单词"), exampleAudio = button("听例句");
    check(wordAudio.rect.y == exampleAudio.rect.y && wordAudio.rect.w == 312 &&
              exampleAudio.rect.x == 348, "Learning audio actions share one footer row");
    check(button("下一个").rect.y == 1080 && button("下一个").primary,
          "Learning continuation is the full-width bottom action");
    dispatch(*model, "lookup");
    check(model->type("appointment") && model->input("submit", ""), "Open native detail fixture");
    auto detailPage = model->page();
    render(detailPage, "layout-detail");
    render(detailPage, "layout-detail-last", ui.answer_pages() - 1);
    check(button("听单词").rect.y == button("听例句").rect.y &&
              button("加入生词本").rect.y == 1080 && button("加入生词本").primary,
          "Dictionary keeps audio paired above bottom notebook action");
    const auto beforeNavigation = state(out / "learning-web-layout-footers");
    auto longPage = detailPage;
    for (auto &b : longPage.blocks) if (b.role == "example") {
        // Deliberately long projection fixture, rather than a modified catalog.
        for (int i = 0; i < 22; ++i) b.text += " Please remember the appointment tomorrow morning.";
    }
    render(longPage, "layout-long-detail-first");
    const int total = ui.answer_pages();
    check(total > 2, "Long detail creates multiple actual Canvas screens");
    for (int i = 0; i < total; ++i) {
        render(longPage, "layout-long-detail-" + std::to_string(i + 1), i);
        check(button("听单词").rect.y == button("听例句").rect.y,
              "Paginated detail retains paired audio");
        if (i + 1 < total) {
            check(button("下一屏").rect.y == 1080 && button("下一屏").primary,
                  "Pagination replaces final action at bottom");
            check(std::none_of(ui.buttons().begin(), ui.buttons().end(), [](const Button &b) {
                return b.label == "加入生词本";
            }), "Pagination hides final notebook action until last screen");
        } else check(button("加入生词本").rect.y == 1080, "Last screen restores notebook action");
    }
    check(state(out / "learning-web-layout-footers") == beforeNavigation,
          "All long-content screen renders preserve native progress");

    auto meaningModel = open("meaning", meaning);
    dispatch(*meaningModel, "v-start");
    const auto meaningPage = meaningModel->page();
    render(meaningPage, "layout-meaning");
    render(meaningPage, "layout-meaning-last", ui.answer_pages() - 1);
    check(button("不认识").rect.y == 1080, "Meaning unknown action stays at bottom");
    int options = 0;
    for (const auto &b : meaningPage.blocks) if (b.role == "option") {
        ++options;
        const auto rendered = button(b.text);
        check(rendered.rect.w == 636 && rendered.rect.y >= 700 &&
                  rendered.rect.y + rendered.rect.h <= 1080, "Meaning choices occupy lower full-width rows");
    }
    check(options == 3, "Meaning retains all three choices");
    auto reviewModel = open("review", due);
    dispatch(*reviewModel, "v-go", "value", "review");
    render(reviewModel->page(), "layout-review");
    check(button("开始复习").rect.y == 980 && button("开始复习").primary &&
              button("我的单词本 · 0 词").rect.y < 980, "Review primary follows secondary action above tabs");
    const auto reviewStart = VJ::parse(reviewModel->page().actions.at(size_t(button("开始复习").event.value)));
    check(str(reviewStart, "intent") == "review" && !flag(reviewStart, "allowExtra"),
          "Review primary explicitly requests review only");
    check(button("回首页").event.action == Action::Back, "Root review advertises home as its return destination");
    dispatch(*reviewModel, "v-back");
    check(reviewModel->page().route == "home", "Root review back actually returns home");
    auto emptyReview = open("review-empty", chosen);
    dispatch(*emptyReview, "v-go", "value", "review");
    render(emptyReview->page(), "layout-review-empty");
    const auto emptyStart = button("暂无到期复习");
    check(emptyStart.primary && !emptyStart.enabled &&
              ui.hit(emptyStart.rect.x + 12, emptyStart.rect.y + 12).action == Action::None,
          "An empty review has a disabled review action, never a disguised learning or home action");

    std::string dictionaryId;
    for (const auto &entry : catalog.entries)
        if (entry.word == "appointment") { dictionaryId = entry.id; break; }
    check(!dictionaryId.empty(), "Dictionary navigation word exists");
    auto navState = chosen;
    navState["notebook"].push_back(dictionaryId);
    auto navigation = open("navigation", navState);
    const auto navDir = out / "learning-web-layout-navigation";
    dispatch(*navigation, "v-go", "value", "search");
    for (const auto *query : {"", "a", "zzzz-not-present"}) {
        check(navigation->type(query), "Native search accepts synthetic query");
        auto searchPage = navigation->page();
        render(searchPage, "layout-search-" + std::to_string(std::string(query).size()));
        const auto fullDictionary = button("完整词典");
        check(fullDictionary.event.action == Action::Dictionary && fullDictionary.rect.x == 224 &&
                  fullDictionary.rect.y == 12 && fullDictionary.rect.w == 228,
              "Full dictionary is a stable native top action regardless of suggestion count");
        check(std::count_if(ui.buttons().begin(), ui.buttons().end(), [](const Button &b) {
                  return b.event.action == Action::Dictionary;
              }) == 1, "Full dictionary has one visible entry without a body fallback");
        for (const auto &block : searchPage.blocks)
            check(block.role != "device-dictionary", "Search suggestions do not carry the old dictionary fallback");
    }
    check(state(navDir) == navState, "Typing search queries and rendering the keyboard do not write learning state");
    check(navigation->type("appointment"), "Restore query before nested notebook navigation");
    render(navigation->page(), "layout-search-notebook-entry");
    check(button("生词本").event.action == Action::Vocabulary,
          "Search notebook button uses the actual native keyboard action");
    check(navigation->dispatch(VJ{{"action", "u-notebook"}}.dump()), "Dispatch native keyboard notebook entry");
    render(navigation->page(), "layout-notebook-from-search");
    check(navigation->page().route == "notebook" && button("返回查词").event.action == Action::Back,
          "Notebook opened from search advertises its actual return destination");
    dispatch(*navigation, "v-go", "value", "notebook-words");
    dispatch(*navigation, "v-open", "value", dictionaryId);
    render(navigation->page(), "layout-notebook-detail");
    check(button("返回列表").event.action == Action::Back, "Notebook word detail returns to list");
    dispatch(*navigation, "v-back");
    check(navigation->page().route == "notebook-words", "First detail back returns to list");
    dispatch(*navigation, "v-back");
    check(navigation->page().route == "notebook", "List returns to notebook");
    dispatch(*navigation, "v-back");
    check(navigation->page().route == "search" && navigation->page().typed == "appointment",
          "Second back restores search and its exact input");
    dispatch(*navigation, "v-back");
    check(navigation->page().route == "home", "Search returns to home without entering another learning flow");
    dispatch(*navigation, "v-go", "value", "review");
    dispatch(*navigation, "v-go", "value", "notebook");
    render(navigation->page(), "layout-notebook-from-review");
    check(button("返回复习").event.action == Action::Back, "Review notebook labels the review return path");
    dispatch(*navigation, "v-go", "value", "notebook-words");
    dispatch(*navigation, "v-open", "value", dictionaryId);
    dispatch(*navigation, "v-back");
    dispatch(*navigation, "v-back");
    dispatch(*navigation, "v-back");
    check(navigation->page().route == "review", "Nested notebook detail unwinds back to review");
    dispatch(*navigation, "v-back");
    check(navigation->page().route == "home", "Review back returns home");
    const auto afterNavigation = state(navDir);
    check(afterNavigation["session"] == navState["session"] && afterNavigation["paused"] == navState["paused"] &&
              afterNavigation["seen"] == navState["seen"] && afterNavigation["cards"] == navState["cards"],
          "Dictionary/notebook navigation never starts a session or changes remembered skills");

    auto seenNotebook = navState;
    seenNotebook["seen"][dictionaryId] = now;
    auto notebookReview = open("notebook-review", seenNotebook);
    dispatch(*notebookReview, "v-go", "value", "search");
    check(notebookReview->type("appoint"), "Keep search text beneath notebook-review navigation");
    check(notebookReview->dispatch(VJ{{"action", "u-notebook"}}.dump()), "Open all-seen notebook from keyboard");
    render(notebookReview->page(), "layout-notebook-review-entry");
    check(button("去复习").primary, "An already-seen notebook links explicitly to review");
    dispatch(*notebookReview, "v-go", "value", "review");
    render(notebookReview->page(), "layout-review-from-notebook");
    check(button("返回生词").event.action == Action::Back,
          "Review reached from notebook labels its actual parent instead of claiming home");
    dispatch(*notebookReview, "v-back");
    check(notebookReview->page().route == "notebook", "Review's notebook return label matches the actual route");
    dispatch(*notebookReview, "v-back");
    check(notebookReview->page().route == "search" && notebookReview->page().typed == "appoint",
          "Notebook-review round trip preserves the earlier search frame and input");
    dispatch(*notebookReview, "v-back");
    check(notebookReview->page().route == "home", "Notebook-review round trip ends at its original home");

    auto advanceFixture = [&](const VJ &s) {
        const auto &session = s["session"];
        if (str(session, "phase") == "learn") return advanceLearn(s);
        if (!field(session, "feedback").is_null())
            return reduce(s, VJ{{"type", "continue"}, {"taskId", session["feedback"]["uid"]}});
        const auto current = engine.current(s);
        const auto kind = str(current, "kind");
        const auto word = catalog.get(str(current, "id"));
        const auto answer = kind == "meaning" ? word["meaning"] : kind == "gap"
            ? engine.gap(current)["answer"] : word["word"];
        return reduce(s, VJ{{"type", "answer"}, {"taskId", current["uid"]}, {"value", answer}});
    };
    auto spelling = learning;
    int fixtureSteps = 0;
    while (str(spelling["session"], "phase") != "tasks" ||
           str(engine.current(spelling), "kind") != "gap" || !field(spelling["session"], "feedback").is_null()) {
        check(++fixtureSteps < 100, "Prepare real gap-round navigation fixture");
        spelling = advanceFixture(spelling);
    }
    auto fullSpelling = spelling;
    int fullSteps = 0;
    while (str(engine.current(fullSpelling), "kind") != "spelling" ||
           !field(fullSpelling["session"], "feedback").is_null()) {
        check(++fullSteps < 100, "Prepare actual full-spelling question");
        fullSpelling = advanceFixture(fullSpelling);
    }
    const auto fullTask = engine.current(fullSpelling);
    const auto fullWord = catalog.get(str(fullTask, "id"));
    const auto fullAnswer = str(fullWord, "word");
    auto spellingControls = open("spelling-controls", fullSpelling);
    const auto spellingControlsDir = out / "learning-web-layout-spelling-controls";
    dispatch(*spellingControls, "v-start", "intent", "new");
    check(spellingControls->type("dr"), "Keep a partial full-spelling draft");
    auto compactPage = spellingControls->page();
    render(compactPage, "layout-spelling-compact");
    const auto pronunciation = button("听发音"), unknown = button("想不起来"), submit = button("检查");
    check(pronunciation.rect.x == 24 && pronunciation.rect.y == 650 && pronunciation.rect.w == 312 &&
              unknown.rect.x == 348 && unknown.rect.y == 650 && unknown.rect.w == 312,
          "Pronunciation and unknown share one row immediately above unchanged keyboard");
    check(pronunciation.event.action == Action::DictionaryRecent && unknown.event.action == Action::RecallSkip &&
              submit.rect.x == 518 && submit.rect.y == 1092 && submit.rect.w == 164 && submit.rect.h == 84,
          "Compact spelling retains the production audio, unknown and bottom submit events");
    check(std::none_of(compactPage.actions.begin(), compactPage.actions.end(), [](const std::string &raw) {
        const auto action = VJ::parse(raw);
        return str(action, "action") == "v-do" && str(action, "type") == "hint";
    }), "New spelling has no visible reveal-and-copy action");
    const auto beforeUnknown = state(spellingControlsDir);
    check(spellingControls->dispatch(VJ{{"action", "w-unknown"}}.dump()), "Submit unknown directly");
    const auto afterUnknown = state(spellingControlsDir);
    const auto unknownFeedback = afterUnknown["session"]["feedback"];
    check(!spellingControls->page().keyboard && flag(unknownFeedback, "unknown") &&
              !flag(unknownFeedback, "correct") && unknownFeedback["uid"] == fullTask["uid"] &&
              num(afterUnknown["session"], "answered") == num(beforeUnknown["session"], "answered") + 1 &&
              num(afterUnknown["session"], "correct") == num(beforeUnknown["session"], "correct"),
          "One unknown action records one failed attempt and opens feedback without copying the answer");
    check(num(field(field(afterUnknown["cards"], str(fullTask, "id")), "spelling"), "successes") ==
              num(field(field(beforeUnknown["cards"], str(fullTask, "id")), "spelling"), "successes"),
          "Unknown spelling does not increase successful recall");
    render(spellingControls->page(), "layout-spelling-unknown-feedback");
    auto longFeedback = spellingControls->page();
    check(std::any_of(longFeedback.blocks.begin(), longFeedback.blocks.end(), [&](const WebBlock &b) {
        return b.text == str(fullWord, "example") && b.role == "detail";
    }), "Feedback moves the repeated example into optional detail");
    for (auto &block : longFeedback.blocks) if (block.role == "detail")
        for (int i = 0; i < 20; ++i) block.text += " Optional example remains in the explanation.";
    render(longFeedback, "layout-feedback-long-explanation");
    check(ui.answer_pages() == 1 && button("说明").enabled,
          "Long optional feedback explanations do not add an acknowledgement page");

    auto revealedState = reduce(fullSpelling,
        VJ{{"type", "hint"}, {"taskId", fullTask["uid"]}});
    auto revealed = open("saved-reveal", revealedState);
    dispatch(*revealed, "v-start", "intent", "new");
    auto revealedPage = revealed->page();
    check(std::any_of(revealedPage.blocks.begin(), revealedPage.blocks.end(), [&](const WebBlock &b) {
        return b.role == "word" && b.text == fullAnswer;
    }), "An already persisted explicit reveal remains visible after upgrade");
    render(revealedPage, "layout-spelling-persisted-reveal");
    const auto revealedSession = state(out / "learning-web-layout-saved-reveal")["session"];
    revealed.reset();
    revealed = std::make_unique<NativeVocabulary>((out / "learning-web-layout-saved-reveal").generic_string());
    check(revealed->enter(VJ(), now), "Reopen persisted revealed draft");
    dispatch(*revealed, "v-start", "intent", "new");
    check(state(out / "learning-web-layout-saved-reveal")["session"] == revealedSession,
          "Reopening preserves the old revealed task, assistance and draft exactly");

    auto revealPixels = [&](WebPage p, const std::string &answer, const std::string &name) {
        for (auto &block : p.blocks) if (block.role == "meaning") {
            const auto definition = block.text;
            for (int i = 0; i < 40; ++i) block.text += " " + definition;
            p.prompt = block.text;
        }
        std::vector<uint8_t> expectedPixels(Canvas::bytes);
        Canvas expected(expectedPixels.data(), font);
        expected.clear();
        check(expected.text_width_px(answer, 48) < 636, "Pixel fixture answer fits a single reserved line");
        expected.text_px(24, 490, answer, 48);
        auto containsAnswerPixels = [&] {
            Canvas actual(pixels.data(), font);
            int ink = 0;
            bool matched = true;
            for (int y = 490; y < 554; ++y)
                for (int x = 24; x < 660; ++x)
                    if (expected.shade(x, y) < 8) {
                        ++ink;
                        matched = matched && actual.shade(x, y) == expected.shade(x, y);
                    }
            check(ink > 20, "Expected answer has measurable rendered glyph pixels");
            return matched;
        };
        render(p, "layout-long-meaning-revealed-" + name);
        check(containsAnswerPixels(), "Long definition leaves the saved answer visibly intact in its reserved area");
        p.blocks.erase(std::remove_if(p.blocks.begin(), p.blocks.end(), [](const WebBlock &block) {
            return block.role == "word";
        }), p.blocks.end());
        render(p, "layout-long-meaning-hidden-" + name);
        check(!containsAnswerPixels(), "An unrevealed long-definition projection contains no answer glyphs in that area");
    };
    revealPixels(revealedPage, fullAnswer, "full");
    const auto gapTaskForReveal = engine.current(spelling);
    auto revealedGap = open("saved-gap-reveal", reduce(spelling,
        VJ{{"type", "hint"}, {"taskId", gapTaskForReveal["uid"]}}));
    dispatch(*revealedGap, "v-start", "intent", "new");
    revealPixels(revealedGap->page(), catalog.entry(str(gapTaskForReveal, "id"))->word, "gap");
    auto requestAudio = [&](NativeVocabulary &native) {
        dispatch(native, "v-audio");
        std::string path; int serial = 0; bool stop = false;
        check(native.take_audio(path, serial, stop, nullptr) && !stop && !path.empty(),
              "Native playback request is available");
        return serial;
    };
    auto stoppedAudio = [&](NativeVocabulary &native, int oldSerial, const std::string &reason) {
        std::string path; int serial = 0; bool stop = false;
        check(native.take_audio(path, serial, stop, nullptr) && stop && serial != oldSerial,
              reason + " emits stop for the old audio context");
        check(!native.playback(oldSerial, true), reason + " rejects the late old completion");
    };
    auto audioLearning = open("audio-learning", learning);
    dispatch(*audioLearning, "v-start", "intent", "new");
    const auto learningAudioSerial = requestAudio(*audioLearning);
    dispatch(*audioLearning, "v-do", "type", "learnNext");
    stoppedAudio(*audioLearning, learningAudioSerial, "Moving to the next word");

    auto audioSpelling = open("audio-spelling", fullSpelling);
    const auto audioSpellingDir = out / "learning-web-layout-audio-spelling";
    dispatch(*audioSpelling, "v-start", "intent", "new");
    const auto spellingAudioSerial = requestAudio(*audioSpelling);
    check(audioSpelling->type("au"), "Typing while pronunciation is active");
    std::string pendingPath; int pendingSerial = 0; bool pendingStop = false;
    check(!audioSpelling->take_audio(pendingPath, pendingSerial, pendingStop, nullptr),
          "Saving a draft does not stop active pronunciation");
    const auto audioState = state(audioSpellingDir);
    check(flag(engine.current(audioState), "audioHint") &&
              !flag(engine.current(audioState), "answerRevealed"),
          "Audio marks assistance while keeping full spelling hidden");
    check(audioSpelling->dispatch(VJ{{"action", "w-unknown"}}.dump()), "Unknown while pronunciation is active");
    stoppedAudio(*audioSpelling, spellingAudioSerial, "Submitting an answer");
    const auto feedbackAudioSerial = requestAudio(*audioSpelling);
    dispatch(*audioSpelling, "v-do", "type", "continue");
    stoppedAudio(*audioSpelling, feedbackAudioSerial, "Acknowledging feedback");

    auto failedAudio = open("audio-save-failure", fullSpelling);
    const auto failedAudioDir = out / "learning-web-layout-audio-save-failure";
    dispatch(*failedAudio, "v-start", "intent", "new");
    const auto failedAudioSerial = requestAudio(*failedAudio);
    const auto audioBeforeFailure = state(failedAudioDir);
    fs::rename(failedAudioDir / "vocabulary", failedAudioDir / "vocabulary-held");
    check(failedAudio->dispatch(VJ{{"action", "w-unknown"}}.dump()), "Attempt answer with unavailable journal");
    check(!str(VJ::parse(failedAudio->status()), "courseError").empty() &&
              !failedAudio->take_audio(pendingPath, pendingSerial, pendingStop, nullptr),
          "Failed answer save shows an error without canceling current playback");
    check(failedAudio->playback(failedAudioSerial, true),
          "Original playback completion remains valid after a failed save");
    fs::rename(failedAudioDir / "vocabulary-held", failedAudioDir / "vocabulary");
    check(state(failedAudioDir) == audioBeforeFailure, "Failed answer save preserves the whole prior state");
    dispatch(*failedAudio, "v-check-recovery");

    auto lookupExposure = open("lookup-exposure", fullSpelling);
    const auto lookupExposureDir = out / "learning-web-layout-lookup-exposure";
    dispatch(*lookupExposure, "v-start", "intent", "new");
    check(lookupExposure->type("kept"), "Save spelling draft before dictionary detour");
    dispatch(*lookupExposure, "lookup");
    const auto beforeUnrelated = state(lookupExposureDir);
    check(lookupExposure->type("zzzz-unrelated"), "Type unrelated dictionary prefix");
    lookupExposure->page();
    check(state(lookupExposureDir) == beforeUnrelated,
          "Unrelated live dictionary suggestions do not write learning progress");
    check(lookupExposure->type(fullAnswer), "Publish current-word dictionary suggestion");
    const auto exposed = state(lookupExposureDir);
    const auto candidatePage = lookupExposure->page();
    check(flag(engine.current(exposed), "lookupHint") &&
              std::any_of(candidatePage.blocks.begin(), candidatePage.blocks.end(), [&](const WebBlock &b) {
                  return b.kind == "button" && b.text == fullAnswer;
              }), "Seeing the current word among live suggestions records assistance before opening detail");
    render(candidatePage, "layout-lookup-exposed-suggestion");
    check(lookupExposure->type(fullAnswer) && state(lookupExposureDir) == exposed,
          "Redisplaying the same exposed candidate does not append duplicate exposure");
    dispatch(*lookupExposure, "v-back");
    check(lookupExposure->page().typed == "kept" &&
              state(lookupExposureDir)["session"]["current"] == beforeUnrelated["session"]["current"],
          "Assisted lookup returns to the exact task and draft");
    check(lookupExposure->type(fullAnswer) && lookupExposure->input("submit", ""),
          "Answer after viewing a dictionary suggestion");
    check(flag(state(lookupExposureDir)["session"]["feedback"], "assisted"),
          "Suggestion-assisted answer cannot become unassisted recall");

    auto failedLookup = open("lookup-save-failure", fullSpelling);
    const auto failedLookupDir = out / "learning-web-layout-lookup-save-failure";
    dispatch(*failedLookup, "v-start", "intent", "new");
    dispatch(*failedLookup, "lookup");
    check(failedLookup->type("zzzz-unrelated"), "Keep harmless query before failed exposure");
    const auto lookupBeforeFailure = state(failedLookupDir);
    fs::rename(failedLookupDir / "vocabulary", failedLookupDir / "vocabulary-held");
    check(!failedLookup->type(fullAnswer), "Candidate exposure waits for a durable save");
    const auto failedCandidatePage = failedLookup->page();
    check(std::none_of(failedCandidatePage.blocks.begin(), failedCandidatePage.blocks.end(), [&](const WebBlock &b) {
        return b.text == fullAnswer || b.detail == str(fullWord, "meaning");
    }), "Failed exposure save never publishes the answer suggestion");
    render(failedCandidatePage, "layout-lookup-exposure-save-error");
    fs::rename(failedLookupDir / "vocabulary-held", failedLookupDir / "vocabulary");
    check(state(failedLookupDir) == lookupBeforeFailure, "Failed exposure leaves original task and memory intact");
    dispatch(*failedLookup, "v-check-recovery");
    check(failedLookup->page().typed == "zzzz-unrelated", "Retry preserves the last safely displayed query");
    check(failedLookup->type(fullAnswer) &&
              flag(engine.current(state(failedLookupDir)), "lookupHint"),
          "Retry durably records assistance before publishing the candidate");

    const auto bridgeFixtureState = reduce(fullSpelling,
        VJ{{"type", "draft"}, {"taskId", fullTask["uid"]}, {"value", "bridge"}});
    auto prepareRetainedQuery = [&](const std::string &name) {
        auto retainedState = learning;
        retainedState["notebook"] = VJ::array({newWords[9]});
        retainedState["seen"][str(newWords[9])] = now;
        auto native = open(name, retainedState);
        const auto profile = out / ("learning-web-layout-" + name);
        dispatch(*native, "v-start", "intent", "new");
        dispatch(*native, "lookup");
        check(native->type(fullAnswer), "Save a future task's dictionary query during presentation");
        const auto beforeLeavingSearch = state(profile);
        check(str(beforeLeavingSearch["session"], "phase") == "learn",
              "Retained query begins before an answerable task exists");
        check(native->dispatch(VJ{{"action", "u-notebook"}}.dump()), "Leave search through its actual notebook control");
        dispatch(*native, "v-go", "value", "review");
        dispatch(*native, "v-go", "value", "home");
        dispatch(*native, "v-start", "intent", "new");
        int steps = 0;
        for (;;) {
            const auto currentState = state(profile);
            const auto currentTask = engine.current(currentState);
            const auto &session = currentState["session"];
            if (str(session, "phase") == "tasks" && field(session, "feedback").is_null() &&
                field(currentTask, "uid") == fullTask["uid"]) break;
            check(++steps < 100, "Retained-query fixture reaches the original full-spelling task");
            if (str(session, "phase") == "learn") dispatch(*native, "v-do", "type", "learnNext");
            else if (!field(session, "feedback").is_null()) dispatch(*native, "v-do", "type", "continue");
            else {
                const auto word = catalog.get(str(currentTask, "id"));
                if (str(currentTask, "kind") == "meaning")
                    dispatch(*native, "v-answer", "value", str(word, "meaning"));
                else {
                    const auto answer = str(currentTask, "kind") == "gap"
                        ? str(engine.gap(currentTask), "answer") : str(word, "word");
                    check(native->type(answer) && native->input("submit", ""),
                          "Advance retained-query fixture through actual typed answers");
                }
            }
        }
        check(!flag(engine.current(state(profile)), "lookupHint") && native->back() &&
                  native->page().route == "home",
              "Advancing tasks does not retroactively mark an old dictionary query as assistance");
        return native;
    };
    auto retainedQuery = prepareRetainedQuery("retained-query");
    const auto retainedDir = out / "learning-web-layout-retained-query";
    dispatch(*retainedQuery, "v-go", "value", "search");
    const auto retainedExposed = state(retainedDir);
    check(retainedQuery->page().typed == fullAnswer &&
              flag(engine.current(retainedExposed), "lookupHint"),
          "Reentering the dictionary exposes its retained current-word query before showing suggestions");
    retainedQuery->page();
    retainedQuery->page();
    check(state(retainedDir) == retainedExposed, "Reading the exposed search page performs no additional writes");
    render(retainedQuery->page(), "layout-retained-query-exposed");

    auto retainedFailure = prepareRetainedQuery("retained-query-failure");
    const auto retainedFailureDir = out / "learning-web-layout-retained-query-failure";
    const auto retainedBeforeFailure = state(retainedFailureDir);
    std::string retainedSearchAction;
    for (const auto &raw : retainedFailure->page().actions) {
        const auto action = VJ::parse(raw);
        if (str(action, "action") == "v-go" && str(action, "value") == "search") {
            retainedSearchAction = raw;
            break;
        }
    }
    check(!retainedSearchAction.empty(), "Home has an actual dictionary destination");
    fs::rename(retainedFailureDir / "vocabulary", retainedFailureDir / "vocabulary-held");
    check(!retainedFailure->dispatch(retainedSearchAction), "Reject retained-query entry with unavailable journal");
    const auto blockedSearch = retainedFailure->page();
    check(blockedSearch.route == "recovery" && str(VJ::parse(retainedFailure->status()), "route") == "home" && !blockedSearch.keyboard &&
              !str(VJ::parse(retainedFailure->status()), "courseError").empty() &&
              std::none_of(blockedSearch.blocks.begin(), blockedSearch.blocks.end(), [&](const WebBlock &b) {
                  return b.text == fullAnswer || b.detail == str(fullWord, "meaning");
              }), "Failed retained-query exposure stays on the original route without showing answer candidates");
    fs::rename(retainedFailureDir / "vocabulary-held", retainedFailureDir / "vocabulary");
    check(state(retainedFailureDir) == retainedBeforeFailure,
          "Failed retained-query entry keeps the current task, memory and exposure records intact");
    dispatch(*retainedFailure, "v-check-recovery");
    dispatch(*retainedFailure, "v-go", "value", "search");
    check(retainedFailure->page().typed == fullAnswer &&
              flag(engine.current(state(retainedFailureDir)), "lookupHint"),
          "Retained-query entry can retry safely after storage returns");
    auto bridgeFixture = open("full-dictionary-exposure", bridgeFixtureState);
    bridgeFixture.reset();
    const auto bridgeDir = out / "learning-web-layout-full-dictionary-exposure";
    LearningWeb bridge;
    check(bridge.init(bridgeDir.generic_string()) && bridge.home(), "Initialize full application facade");
    WebPage bridgePage;
    check(bridge.page(bridgePage) && bridge.native_active(),
          "First facade render activates the native learning owner before commands");
    std::string bridgeStatus;
    check(bridge.status(bridgeStatus) &&
              str(VJ::parse(bridgeStatus), "engine") == "native-vocabulary-fsrs6",
          "Full dictionary exposure fixture uses the vocabulary engine");
    check(bridge.command("v-start") && bridge.page(bridgePage) && bridgePage.keyboard &&
              bridgePage.typed == "bridge" && engine.current(state(bridgeDir))["uid"] == fullTask["uid"],
          "Open the exact persisted spelling task and draft through the application facade");
    auto aliasWord = fullAnswer;
    for (char &c : aliasWord)
        if ((unsigned char)c < 128) c = char(std::toupper((unsigned char)c));
    const auto bridgeBefore = state(bridgeDir);
    check(bridge.command("v-lookup-exposure", " " + aliasWord + " "),
          "Full dictionary exposure command accepts a normalized alias spelling");
    const auto bridgeAfter = state(bridgeDir);
    check(flag(engine.current(bridgeAfter), "lookupHint") &&
              bridgeAfter["session"]["current"] == bridgeBefore["session"]["current"] &&
              bridgeAfter["session"]["draft"] == bridgeBefore["session"]["draft"] &&
              !flag(engine.current(bridgeAfter), "answerRevealed"),
          "Full-dictionary command marks the current spelling without revealing or changing its position");
    const auto reviewWord = str(newWords[9]);
    spelling["seen"][reviewWord] = now - 86400000;
    spelling["cards"][reviewWord]["spelling"] = VocabularyEngine::empty_card(now);
    auto switching = open("intent-switch", spelling);
    const auto switchDir = out / "learning-web-layout-intent-switch";
    dispatch(*switching, "v-start", "intent", "new");
    check(switching->type("ab"), "Save partial new-word input");
    const auto savedNew = state(switchDir)["session"];
    const auto savedCards = state(switchDir)["cards"];
    const auto beforeNestedLookup = state(switchDir);
    dispatch(*switching, "lookup");
    check(switching->type("appointment"), "Type a query while learning is paused behind lookup");
    render(switching->page(), "layout-lookup-from-learning");
    check(button("返回学习").event.action == Action::Back, "Learning lookup labels its exact return context");
    check(switching->dispatch(VJ{{"action", "u-notebook"}}.dump()), "Open notebook inside learning lookup");
    dispatch(*switching, "v-back");
    check(switching->page().route == "search" && switching->page().typed == "appointment",
          "Nested notebook restores the lookup draft");
    dispatch(*switching, "v-back");
    check(switching->page().route == "word-batch" && switching->page().typed == "ab" &&
              state(switchDir) == beforeNestedLookup,
          "Lookup/notebook round trip restores exact learning draft without committing navigation");
    render(switching->page(), "layout-new-draft");
    dispatch(*switching, "v-back");
    check(switching->page().route == "home", "Pausing new learning returns home");
    dispatch(*switching, "v-go", "value", "review");
    dispatch(*switching, "v-start", "intent", "review");
    check(str(state(switchDir)["session"], "source") == "review" && switching->page().keyboard,
          "Explicit review opens its due spelling task instead of resuming new learning");
    check(switching->type("re"), "Save partial review input");
    const auto savedReview = state(switchDir)["session"];
    render(switching->page(), "layout-review-draft");
    dispatch(*switching, "v-back");
    check(switching->page().route == "review", "Pausing review returns to review hub");
    dispatch(*switching, "v-go", "value", "home");
    dispatch(*switching, "v-start", "intent", "new");
    auto switched = state(switchDir);
    check(switched["session"] == savedNew && switching->page().typed == "ab" &&
              switched["paused"].size() == 1 && switched["paused"][0] == savedReview,
          "Home restores exact new task/draft and parks exact review task/draft");
    dispatch(*switching, "v-back");
    dispatch(*switching, "v-go", "value", "review");
    render(switching->page(), "layout-review-resume");
    check(button("继续复习").enabled, "Review can resume even while its cards are reserved by the parked group");
    dispatch(*switching, "v-start", "intent", "review");
    switched = state(switchDir);
    check(switched["session"] == savedReview && switching->page().typed == "re" &&
              switched["paused"].size() == 1 && switched["paused"][0] == savedNew && switched["cards"] == savedCards,
          "Review restores its exact draft without changing memory or losing the new group");

    auto almostSummary = [&](VJ s) {
        int steps = 0;
        while (str(s["session"], "phase") != "summary") {
            check(++steps < 150, "Synthetic summary fixture terminates");
            auto before = s;
            s = advanceFixture(s);
            if (str(s["session"], "phase") == "summary") return before;
        }
        throw std::runtime_error("Summary fixture must start unfinished");
    };
    auto summaryBase = chosen;
    // These independent profiles share the fixture engine at a fixed clock.
    // Keep their session IDs disjoint, as they would be with separate live engines,
    // so one profile cannot reuse another profile's cached meaning options.
    summaryBase["serial"] = 100;
    summaryBase["settings"] = VJ{{"mode", "meaning"}, {"dailyNew", 5}};
    auto newFinal = almostSummary(reduce(summaryBase, VJ{{"type", "begin"}, {"intent", "new"}}));
    newFinal["seen"][reviewWord] = now - 86400000;
    newFinal["cards"][reviewWord]["meaning"] = VocabularyEngine::empty_card(now);
    auto newSummary = open("summary-new", newFinal);
    const auto newSummaryDir = out / "learning-web-layout-summary-new";
    dispatch(*newSummary, "v-start", "intent", "new");
    dispatch(*newSummary, "v-do", "type", "continue");
    render(newSummary->page(), "layout-summary-new");
    check(newSummary->page().route == "word-result" && button("再学一组").primary,
          "New-word summary offers an explicit next group after reaching the goal");
    const auto priorNewGroup = state(newSummaryDir)["session"]["id"];
    dispatch(*newSummary, "v-start", "intent", "new");
    check(str(state(newSummaryDir)["session"], "source") == "new" &&
              state(newSummaryDir)["session"]["id"] != priorNewGroup,
          "New-word summary starts new learning despite outstanding due review");

    auto reviewBase = summaryBase;
    reviewBase["serial"] = 200;
    int reviewCount = 0;
    for (const auto &entry : catalog.entries) {
        if (!entry.eligible || entry.stage != "foundation") continue;
        reviewBase["seen"][entry.id] = now - 86400000;
        reviewBase["cards"][entry.id]["meaning"] = VocabularyEngine::empty_card(now);
        if (++reviewCount == 11) break;
    }
    auto reviewFinal = almostSummary(reduce(reviewBase, VJ{{"type", "begin"}, {"intent", "review"}}));
    auto reviewSummary = open("summary-review", reviewFinal);
    const auto reviewSummaryDir = out / "learning-web-layout-summary-review";
    dispatch(*reviewSummary, "v-go", "value", "review");
    dispatch(*reviewSummary, "v-start", "intent", "review");
    dispatch(*reviewSummary, "v-do", "type", "continue");
    render(reviewSummary->page(), "layout-summary-review");
    check(reviewSummary->page().route == "word-result" && button("继续复习").primary,
          "Review summary offers only the remaining due review group");
    const auto reviewSummaryReturn = VJ::parse(reviewSummary->page().actions.at(size_t(footerButton("返回复习").event.value)));
    check(!footerButton("返回复习").primary && str(reviewSummaryReturn, "action") == "v-return" &&
              str(reviewSummaryReturn, "value") == "review",
          "Review summary's secondary return names review explicitly while more review remains");
    dispatch(*reviewSummary, "v-start", "intent", "review");
    check(str(state(reviewSummaryDir)["session"], "source") == "review" &&
              state(reviewSummaryDir)["session"]["ids"].size() == 1,
          "Review summary stays in review instead of adding new words");

    auto notebookBase = summaryBase;
    notebookBase["serial"] = 300;
    for (size_t i = 0; i < 5; ++i) notebookBase["notebook"].push_back(newWords[i]);
    auto notebookFinal = almostSummary(reduce(notebookBase, VJ{{"type", "learnNotebook"}}));
    auto notebookSummary = open("summary-notebook", notebookFinal);
    dispatch(*notebookSummary, "v-go", "value", "review");
    dispatch(*notebookSummary, "v-go", "value", "notebook");
    dispatch(*notebookSummary, "v-do", "type", "learnNotebook");
    dispatch(*notebookSummary, "v-do", "type", "continue");
    render(notebookSummary->page(), "layout-summary-notebook");
    check(notebookSummary->page().route == "word-result" && button("返回生词本").primary,
          "Notebook summary returns to its own notebook");
    dispatch(*notebookSummary, "v-return", "value", "notebook");
    check(notebookSummary->page().route == "notebook", "Notebook summary explicitly returns to notebook");
    dispatch(*notebookSummary, "v-back");
    check(notebookSummary->page().route == "review", "Returning to an existing notebook frame preserves its review parent");
    dispatch(*notebookSummary, "v-back");
    check(notebookSummary->page().route == "home", "Notebook summary's retained parent stack still returns home");

    auto pausedReviewBase = reviewBase;
    pausedReviewBase["serial"] = 400;
    const auto extraReviewCard = engine.due(pausedReviewBase, now).back();
    pausedReviewBase["cards"].erase(str(extraReviewCard, "id"));
    auto pausedReviewFinal = almostSummary(reduce(pausedReviewBase,
        VJ{{"type", "begin"}, {"intent", "review"}}));
    const auto pausedReviewSession = pausedReviewFinal["session"];
    pausedReviewFinal["paused"].push_back(pausedReviewSession);
    pausedReviewFinal["session"] = nullptr;
    auto pausedReviewSummary = open("summary-paused-review", pausedReviewFinal);
    const auto pausedReviewDir = out / "learning-web-layout-summary-paused-review";
    dispatch(*pausedReviewSummary, "v-go", "value", "settings");
    dispatch(*pausedReviewSummary, "v-go", "value", "paused");
    dispatch(*pausedReviewSummary, "v-do", "id", str(pausedReviewSession, "id"));
    dispatch(*pausedReviewSummary, "v-do", "type", "continue");
    render(pausedReviewSummary->page(), "layout-summary-paused-review");
    check(pausedReviewSummary->page().route == "word-result" && footerButton("返回复习").primary &&
              std::count_if(ui.buttons().begin(), ui.buttons().end(), [](const Button &b) {
                  return b.label == "返回复习" && b.rect.y >= 700;
              }) == 1, "Finished review has one footer return plus its explicit top Back");
    const auto finishedReviewState = state(pausedReviewDir);
    check(pausedReviewSummary->back(), "Physical Back from completed paused review");
    check(pausedReviewSummary->page().route == "review" && state(pausedReviewDir) == finishedReviewState,
          "Review resumed from settings/paused returns to review without writing learning progress");
    render(pausedReviewSummary->page(), "layout-review-after-paused-summary");
    check(button("回首页").event.action == Action::Back,
          "Missing review ancestor creates a clean review root instead of keeping paused/settings history");
    dispatch(*pausedReviewSummary, "v-back");
    check(pausedReviewSummary->page().route == "home", "Review completion does not send back to the paused-session list");

    auto pausedNotebookBase = notebookBase;
    pausedNotebookBase["serial"] = 500;
    auto pausedNotebookFinal = almostSummary(reduce(pausedNotebookBase,
        VJ{{"type", "learnNotebook"}, {"explicit", true}}));
    const auto pausedNotebookSession = pausedNotebookFinal["session"];
    pausedNotebookFinal["paused"].push_back(pausedNotebookSession);
    pausedNotebookFinal["session"] = nullptr;
    auto pausedNotebookSummary = open("summary-paused-notebook", pausedNotebookFinal);
    const auto pausedNotebookDir = out / "learning-web-layout-summary-paused-notebook";
    dispatch(*pausedNotebookSummary, "v-go", "value", "settings");
    dispatch(*pausedNotebookSummary, "v-go", "value", "paused");
    dispatch(*pausedNotebookSummary, "v-do", "id", str(pausedNotebookSession, "id"));
    dispatch(*pausedNotebookSummary, "v-do", "type", "continue");
    render(pausedNotebookSummary->page(), "layout-summary-paused-notebook");
    check(pausedNotebookSummary->page().route == "word-result" && button("返回生词本").primary,
          "Notebook resumed from paused list retains its notebook completion destination");
    const auto finishedNotebookState = state(pausedNotebookDir);
    check(pausedNotebookSummary->back(), "Physical Back from completed paused notebook");
    check(pausedNotebookSummary->page().route == "notebook" && state(pausedNotebookDir) == finishedNotebookState,
          "Notebook summary returns to notebook rather than the paused list, without committing navigation");
    dispatch(*pausedNotebookSummary, "v-back");
    check(pausedNotebookSummary->page().route == "home", "Notebook completion drops unrelated settings/paused ancestry");

    auto summaryState = [&](const VJ &before) {
        return reduce(before, VJ{{"type", "continue"}, {"taskId", before["session"]["feedback"]["uid"]}});
    };
    auto checkSummaryBack = [&](const std::string &name, const VJ &completedState,
                                const std::string &destination, const std::string &label) {
        auto native = open(name, completedState);
        check(native->dispatch(VJ{{"action", "v-go"}, {"value", "word-batch"}}.dump()),
              "Open completed summary fixture");
        const auto p = native->page();
        render(p, "layout-" + name);
        check(p.route == "word-result" && p.top.size() == 1 && p.top[0].text == label &&
                  str(VJ::parse(p.actions[size_t(p.top[0].action)]), "value") == destination,
              "Completed summary exposes one source-specific top return");
        check(std::none_of(p.actions.begin(), p.actions.end(), [](const std::string &raw) {
            return str(VJ::parse(raw), "action") == "lookup";
        }) && std::none_of(ui.buttons().begin(), ui.buttons().end(), [](const Button &b) {
            return b.label == "暂停" || b.label == "查词";
        }), "Completed summary has no pause or lookup tool");
        check(button(label).event.action == Action::Back, "Summary top return uses physical Back");
        const auto beforeBack = state(out / ("learning-web-layout-" + name));
        check(native->back() && native->page().route == destination &&
                  state(out / ("learning-web-layout-" + name)) == beforeBack,
              "Summary Back reaches its source without writing learning state");
    };
    const auto completedNew = summaryState(newFinal);
    checkSummaryBack("summary-new-back", completedNew, "home", "回首页");
    checkSummaryBack("summary-review-back", summaryState(reviewFinal), "review", "返回复习");
    checkSummaryBack("summary-notebook-back", summaryState(notebookFinal), "notebook", "返回生词");

    auto notebookSeven = summaryBase;
    notebookSeven["serial"] = 2100;
    for (size_t i = 0; i < 7; ++i) notebookSeven["notebook"].push_back(newWords[i]);
    const auto sevenBeforeSummary = almostSummary(reduce(notebookSeven,
        VJ{{"type", "learnNotebook"}, {"explicit", true}}));
    auto nextNotebook = open("notebook-next-group", sevenBeforeSummary);
    const auto nextNotebookDir = out / "learning-web-layout-notebook-next-group";
    dispatch(*nextNotebook, "v-go", "value", "review");
    dispatch(*nextNotebook, "v-go", "value", "notebook");
    dispatch(*nextNotebook, "v-do", "type", "learnNotebook");
    dispatch(*nextNotebook, "v-do", "type", "continue");
    render(nextNotebook->page(), "layout-notebook-next-group");
    const auto notebookNext = footerButton("下一组生词");
    check(notebookNext.primary && notebookNext.rect.y == 1080 &&
              !footerButton("返回生词本").primary, "Notebook summary offers one direct next-group primary");
    const auto priorNotebook = state(nextNotebookDir);
    const auto remainingNotebook = engine.available_notebook(priorNotebook);
    check(remainingNotebook.size() == 2, "Seven-word notebook has two genuinely available remaining words");
    dispatch(*nextNotebook, "v-do", "type", "learnNotebook");
    const auto continuedNotebook = state(nextNotebookDir);
    check(nextNotebook->page().route == "word-batch" &&
              str(continuedNotebook["session"], "source") == "notebook" &&
              continuedNotebook["session"]["ids"] == remainingNotebook &&
              continuedNotebook["session"]["id"] != priorNotebook["session"]["id"] &&
              continuedNotebook["cards"] == priorNotebook["cards"],
          "One summary action starts the next notebook group without a hub or changing prior memory");

    auto reservedNotebookBase = chosen;
    reservedNotebookBase["serial"] = 2200;
    for (size_t i = 5; i < 10; ++i) reservedNotebookBase["notebook"].push_back(newWords[i]);
    auto reservedNotebook = reduce(reservedNotebookBase,
        VJ{{"type", "learnNotebook"}, {"explicit", true}});
    const auto reservedNotebookSession = reservedNotebook["session"];
    for (const auto &entry : catalog.entries)
        if (entry.eligible && entry.stage == "foundation" && !has(reservedNotebookSession["ids"], entry.id))
            reservedNotebook["seen"][entry.id] = now;
    auto blockedActive = home("layout-home-active-notebook-reserved", reservedNotebook, "返回生词本",
                              "剩余新词在生词本词组中，可回去继续");
    dispatch(*blockedActive, "v-go", "value", "notebook");
    check(blockedActive->page().route == "notebook", "Active notebook reservation returns to its visible notebook entry");
    dispatch(*blockedActive, "v-do", "type", "learnNotebook");
    check(state(out / "learning-web-layout-layout-home-active-notebook-reserved")["session"] ==
              reservedNotebookSession, "Active notebook continuation preserves the exact group");

    auto reservedPaused = reservedNotebook;
    reservedPaused["paused"].push_back(reservedNotebookSession);
    reservedPaused["session"] = completedNew["session"];
    auto blockedPaused = home("layout-home-paused-reserved", reservedPaused, "查看暂停词组",
                              "剩余新词在暂停词组中，可继续原词组");
    dispatch(*blockedPaused, "v-go", "value", "paused");
    check(blockedPaused->page().route == "paused" &&
              state(out / "learning-web-layout-layout-home-paused-reserved") == reservedPaused,
          "Parked reservation opens the paused list without an impossible start");
    auto reservedSummary = open("summary-reserved", reservedPaused);
    check(reservedSummary->dispatch(VJ{{"action", "v-go"}, {"value", "word-batch"}}.dump()),
          "Open summary with only reserved unseen words");
    render(reservedSummary->page(), "layout-summary-reserved");
    check(footerButton("查看暂停词组").primary, "Reserved summary advertises its existing paused group");
    dispatch(*reservedSummary, "v-go", "value", "paused");
    check(reservedSummary->page().route == "paused" &&
              state(out / "learning-web-layout-summary-reserved") == reservedPaused,
          "Reserved summary does not attempt to create an empty new group");
    auto exhaustedSummaryState = completedNew;
    for (const auto &entry : catalog.entries)
        if (entry.eligible && entry.stage == "foundation") exhaustedSummaryState["seen"][entry.id] = now;
    auto exhaustedSummary = open("summary-exhausted", exhaustedSummaryState);
    check(exhaustedSummary->dispatch(VJ{{"action", "v-go"}, {"value", "word-batch"}}.dump()),
          "Open exhausted summary");
    render(exhaustedSummary->page(), "layout-summary-exhausted");
    check(footerButton("选择下一词库").primary, "Truly exhausted summary offers level selection");
    dispatch(*exhaustedSummary, "v-go", "value", "levels");
    check(exhaustedSummary->page().route == "levels" &&
              state(out / "learning-web-layout-summary-exhausted") == exhaustedSummaryState,
          "Exhausted summary opens selection without rewriting completion");

    // Capture real engine boundary states, then exercise each visible footer.
    auto boundaryFlow = chosen;
    boundaryFlow["serial"] = 2300;
    boundaryFlow["settings"]["mode"] = "staged";
    boundaryFlow = reduce(boundaryFlow, VJ{{"type", "begin"}, {"intent", "new"}, {"allowExtra", true}});
    VJ boundaryFixtures = VJ::object();
    int boundarySteps = 0;
    while (str(boundaryFlow["session"], "phase") != "summary") {
        check(++boundarySteps < 150, "Boundary fixture completes");
        auto next = advanceFixture(boundaryFlow);
        if (!field(boundaryFlow["session"], "feedback").is_null()) {
            const auto from = engine.current(boundaryFlow), to = engine.current(next);
            std::string label;
            if (str(next["session"], "phase") == "summary") label = "查看小结";
            else if (str(from, "kind") == str(to, "kind")) label = "下一题";
            else if (str(to, "kind") == "gap") label = "开始补字母";
            else if (str(to, "kind") == "spelling") label = "开始完整拼写";
            if (!label.empty() && !boundaryFixtures.contains(label)) boundaryFixtures[label] = boundaryFlow;
        }
        boundaryFlow = std::move(next);
    }
    int boundaryIndex = 0;
    for (const auto *label : {"下一题", "开始补字母", "开始完整拼写", "查看小结"}) {
        check(boundaryFixtures.contains(label), std::string("Actual flow reaches boundary ") + label);
        const auto name = "boundary-" + std::to_string(++boundaryIndex);
        auto boundaryModel = open(name, boundaryFixtures[label]);
        check(boundaryModel->dispatch(VJ{{"action", "v-go"}, {"value", "word-batch"}}.dump()),
              "Open persisted boundary feedback");
        const auto boundaryPage = boundaryModel->page();
        render(boundaryPage, "layout-" + name);
        const auto nextButton = footerButton(label);
        check(nextButton.primary && nextButton.rect.y == 1080 && ui.answer_pages() == 1,
              "Boundary continuation names the next action at the bottom without an interstitial");
        const auto before = state(out / ("learning-web-layout-" + name));
        const auto preview = engine.continue_preview(before, now);
        check(boundaryModel->dispatch(boundaryPage.actions.at(size_t(nextButton.event.value))),
              "Press actual boundary footer");
        const auto after = state(out / ("learning-web-layout-" + name));
        const auto nextTask = engine.current(after);
        check(str(after["session"], "phase") == str(preview, "phase") &&
                  str(nextTask, "kind") == str(preview, "kind") &&
                  flag(nextTask, "followup") == flag(preview, "followup"),
              "Visible boundary action reaches the previewed phase and round");
    }
    auto correction = chosen;
    correction["serial"] = 2400;
    correction["settings"]["mode"] = "meaning";
    correction["seen"][wordId] = now - 86400000;
    correction["cards"][wordId]["meaning"] = VocabularyEngine::empty_card(now);
    correction = reduce(correction, VJ{{"type", "begin"}, {"intent", "review"}});
    correction = reduce(correction,
        VJ{{"type", "answer"}, {"taskId", engine.current(correction)["uid"]}, {"unknown", true}});
    auto delayedCorrection = open("boundary-correction", correction);
    check(delayedCorrection->dispatch(VJ{{"action", "v-go"}, {"value", "word-batch"}}.dump()),
          "Open persisted correction feedback");
    delayedCorrection->tick(now + 900000);
    render(delayedCorrection->page(), "layout-boundary-correction");
    check(footerButton("开始巩固").primary, "Eligible delayed correction is named explicitly");
    dispatch(*delayedCorrection, "v-do", "type", "continue");
    const auto correctionAfter = state(out / "learning-web-layout-boundary-correction");
    check(flag(engine.current(correctionAfter), "followup") &&
              str(correctionAfter["session"], "phase") == "tasks",
          "Correction footer opens the existing spaced follow-up task");

    auto longNotebookState = fullSpelling;
    longNotebookState["notebook"] = newWords;
    auto longNotebook = open("long-notebook-return", longNotebookState);
    const auto longNotebookDir = out / "learning-web-layout-long-notebook-return";
    dispatch(*longNotebook, "v-start", "intent", "new");
    check(longNotebook->type("position"), "Keep exact draft beneath long notebook navigation");
    const auto originalStudyPage = longNotebook->page();
    const auto originalStudy = state(longNotebookDir);
    dispatch(*longNotebook, "lookup");
    check(longNotebook->type("zzzz-unrelated"), "Keep search query beneath long notebook");
    const auto originalSearchPage = longNotebook->page();
    check(longNotebook->dispatch(VJ{{"action", "u-notebook"}}.dump()), "Open long notebook from lookup");
    dispatch(*longNotebook, "v-go", "value", "notebook-words");
    const auto originalNotebookPage = longNotebook->page();
    render(originalNotebookPage, "layout-long-notebook-first");
    const int notebookPageIndex = ui.answer_pages() - 1;
    check(notebookPageIndex > 0, "Notebook fixture produces actual multiple Canvas pages");
    render(originalNotebookPage, "layout-long-notebook-last", notebookPageIndex);
    const auto notebookPixels = pixels;
    std::string visibleDetailAction;
    for (const auto &control : ui.buttons()) if (control.event.action == Action::WebAction) {
        const auto &raw = originalNotebookPage.actions.at(size_t(control.event.value));
        if (str(VJ::parse(raw), "action") == "v-open") { visibleDetailAction = raw; break; }
    }
    check(!visibleDetailAction.empty() && longNotebook->dispatch(visibleDetailAction),
          "Open an actually visible word on the last notebook page");
    check(longNotebook->page().route == "word" && longNotebook->back(), "Return from notebook word detail");
    const auto restoredNotebookPage = longNotebook->page();
    check(restoredNotebookPage.key == originalNotebookPage.key,
          "Notebook page key survives word detail so the app can restore its saved page index");
    render(restoredNotebookPage, "layout-long-notebook-restored", notebookPageIndex);
    check(pixels == notebookPixels && ui.answer_pages() > notebookPageIndex,
          "Saved notebook page index restores the same visible list and controls");
    check(longNotebook->back() && longNotebook->page().route == "notebook", "List returns to notebook overview");
    check(longNotebook->back() && longNotebook->page().key == originalSearchPage.key &&
              longNotebook->page().typed == originalSearchPage.typed,
          "Nested long notebook restores the exact search query and identity");
    check(longNotebook->back() && longNotebook->page().key == originalStudyPage.key &&
              longNotebook->page().typed == "position" &&
              state(longNotebookDir)["session"]["current"] == originalStudy["session"]["current"] &&
              state(longNotebookDir)["session"]["draft"] == originalStudy["session"]["draft"],
          "Returning through dictionary restores the original learning task and draft");
    auto settingsModel = open("settings", paused);
    const auto settingsState = state(out / "learning-web-layout-settings");
    dispatch(*settingsModel, "v-go", "value", "settings");
    const auto settingsPage = settingsModel->page();
    render(settingsPage, "layout-settings");
    const int settingsPages = ui.answer_pages();
    for (int i = 1; i < settingsPages; ++i)
        render(settingsPage, "layout-settings-" + std::to_string(i + 1), i);
    check(state(out / "learning-web-layout-settings") == settingsState,
          "Viewing all settings pages preserves paused round and progress");
}
int main(int argc, char **argv) {
    try {
        check(argc == 4, "resources, font, fresh output required");
        const fs::path resources = argv[1], out = argv[3], dir = out / "learning-web";
        check(!fs::exists(out), "Fresh QA directory required");
        fs::create_directories(dir);
        fs::create_directories(out / "vocabulary");
        fs::create_directories(out / "frames");
        for (const auto *name : {"catalog.json", "catalog.ndjson", "catalog.tsv"})
            fs::copy_file(resources / name, out / "vocabulary" / name);
        auto m = std::make_unique<LearningWeb>();
        check(m->init(dir.generic_string()), m->error());
        check(m->home(), m->error());
        WebPage page;
        check(m->page(page), m->error());
        auto scripts = m->script_calls();
        check(m->native_active(), "Native entry");
        std::string stat;
        m->status(stat);
        check(str(VJ::parse(stat), "engine") == "native-vocabulary-fsrs6",
              "Native vocabulary selected");
        const auto oldCourse = read(dir / "course-v2.json"),
                   oldProfile = read(dir / "profile-v2.json");
        auto fontData = read(argv[2]);
        Font font;
        check(font.init((const uint8_t *)fontData.data(), fontData.size()), "font");
        layout_checks(resources, out, font);
        std::vector<uint8_t> fb(Canvas::bytes);
        UI ui;
        View view;
        view.screen = Screen::WebLearning;
        VJ timings = VJ::array();
        auto paint = [&](const std::string &name) {
            check(m->page(page), m->error());
            view.web = page;
            view.web_page = 0;
            Canvas canvas(fb.data(), font);
            ui.render(canvas, view);
            std::ofstream p(out / "frames" / (name + ".pgm"), std::ios::binary);
            p << "P5\n684 1216\n255\n";
            for (int y = 0; y < 1216; ++y)
                for (int x = 0; x < 684; ++x) {
                    char c = char(canvas.shade(x, y) * 17);
                    p.write(&c, 1);
                }
            VJ buttons = VJ::array();
            for (const auto &b : ui.buttons()) {
                check(b.rect.x >= 0 && b.rect.y >= 0 && b.rect.x + b.rect.w <= 684 &&
                          b.rect.y + b.rect.h <= 1216,
                      "touch rect bounds " + name);
                buttons.push_back(VJ{{"label", b.label},
                                     {"x", b.rect.x},
                                     {"y", b.rect.y},
                                     {"w", b.rect.w},
                                     {"h", b.rect.h}});
            }
            std::ofstream(out / "frames" / (name + ".json")) << VJ{
                {"title", page.title},
                {"route", page.route},
                {"pages", ui.answer_pages()},
                {"buttons", buttons}}.dump(2);
        };
        auto act = [&](const std::string &name, const std::string &key = "",
                       const std::string &value = "") {
            check(m->page(page), m->error());
            for (size_t i = 0; i < page.actions.size(); ++i) {
                auto a = VJ::parse(page.actions[i]);
                if (str(a, "action") == name && (key.empty() || str(a, key) == value)) {
                    auto start = std::chrono::steady_clock::now();
                    check(m->action(int(i)), m->error());
                    check(m->page(page), m->error());
                    timings.push_back(VJ{{"action", name},
                                         {"ms", std::chrono::duration<double, std::milli>(
                                                    std::chrono::steady_clock::now() - start)
                                                    .count()}});
                    return;
                }
            }
            throw std::runtime_error("Missing visible action " + name + " " + key + " " + value +
                                     " on " + page.route);
        };
        paint("01-home");
        act("v-start");
        paint("02-levels");
        act("v-stage", "value", "foundation");
        act("v-use-stage", "value", "foundation");
        act("v-start");
        paint("03-learn");
        auto initial = state(dir);
        check(m->command("next"), "Ignore stale legacy next on vocabulary boundary");
        check(m->script_calls() == scripts, "Stale legacy next cannot enter JS/listening");
        check(state(dir) == initial, "Stale legacy action leaves progress unchanged");
        act("lookup");
        check(m->type("appointment"), "Type lookup");
        check(m->key("submit"), "Lookup submit");
        paint("04-dictionary");
        check(page.route == "word", "Dictionary detail");
        act("v-audio");
        std::string audio;
        int serial;
        bool stop;
        check(m->take_audio(audio, serial, stop) && !stop &&
                  audio.rfind("vocabulary/audio/", 0) == 0,
              "SD audio mapping");
        check(!m->audio_text(audio).empty(), "TTS fallback text");
        check(m->playback(serial, true), "Audio callback");
        act("v-do", "type", "notebook");
        m->back();
        m->back();
        m->page(page);
        check(page.route == "word-batch", "Lookup returns to learning");
        auto afterLookup = state(dir);
        for (const auto *k : {"ids", "phase", "learnAt", "tasks", "current", "feedback"})
            check(afterLookup["session"][k] == initial["session"][k],
                  "Lookup keeps exact task position");
        VocabularyCatalog catalog;
        check(catalog.load(resources.generic_string()), "Catalog");
        VocabularyEngine engine(catalog);
        bool gapSeen = false, fullSeen = false, wrong = false, restarted = false;
        int steps = 0;
        while (steps++ < 100) {
            auto s = state(dir);
            const auto &ss = s["session"];
            if (str(ss, "phase") == "summary") {
                paint("08-summary");
                break;
            }
            if (str(ss, "phase") == "learn") {
                act("v-do", "type", "learnNext");
                continue;
            }
            if (!field(ss, "feedback").is_null()) {
                act("v-do", "type", "continue");
                continue;
            }
            auto t = engine.current(s);
            auto w = catalog.get(str(t, "id"));
            const auto kind = str(t, "kind");
            if (kind == "meaning") {
                if (!wrong) {
                    paint("05-meaning");
                    check(m->command("w-unknown"), "Unknown");
                    wrong = true;
                } else
                    act("v-answer", "value", str(w, "meaning"));
            } else {
                const auto answer = kind == "gap" ? str(engine.gap(t), "answer") : str(w, "word");
                check(m->type(answer.substr(0, 1)), "First typed letter");
                m->page(page);
                if (kind == "gap" && !gapSeen) {
                    check(page.gap_mask.find('_') != std::string::npos &&
                              page.gap_mask.find("＿") == std::string::npos,
                          "Integrated ASCII gap projection");
                    paint("06-gap");
                    auto beforePixels = fb;
                    auto beforeAudio = state(dir);
                    auto mask = page.gap_mask, draft = page.typed;
                    act("v-audio");
                    check(m->take_audio(audio, serial, stop) && !stop, "Gap pronunciation requested");
                    check(m->playback(serial, true), "Gap pronunciation finished");
                    m->page(page);
                    auto afterAudio = state(dir);
                    auto hintTask = engine.current(afterAudio);
                    check(flag(hintTask, "audioHint") && flag(hintTask, "hint") &&
                              !flag(hintTask, "answerRevealed"), "Audio assistance does not reveal answer");
                    check(page.route == "word-batch" && page.gap_mask == mask && page.typed == draft &&
                              afterAudio["session"]["current"] == beforeAudio["session"]["current"],
                          "Playback keeps question, mask, draft and round");
                    for (const auto &b : page.blocks)
                        check(b.text != str(w, "word") && b.text != "答案：" + str(w, "word"),
                              "No full answer rendered after audio");
                    paint("06b-gap-audio");
                    Canvas beforeCanvas(beforePixels.data(), font), afterCanvas(fb.data(), font);
                    bool samePrompt = true;
                    for (int y = 284; y < 392; ++y)
                        for (int x = 24; x < 660; ++x)
                            samePrompt = samePrompt && beforeCanvas.shade(x,y) == afterCanvas.shade(x,y);
                    check(samePrompt, "Audio status never covers the spelling definition");
                    gapSeen = true;
                }
                if (kind == "spelling" && !fullSeen) {
                    paint("07-spelling");
                    check(std::none_of(page.actions.begin(), page.actions.end(), [](const std::string &raw) {
                        const auto action = VJ::parse(raw);
                        return str(action, "action") == "v-do" && str(action, "type") == "hint";
                    }), "New full-spelling UI has no reveal-and-copy action");
                    fullSeen = true;
                }
                if (!restarted) {
                    auto before = state(dir);
                    m.reset();
                    m = std::make_unique<LearningWeb>();
                    check(m->init(dir.generic_string()), "Reboot init");
                    m->page(page);
                    act("v-start");
                    check(state(dir)["session"] == before["session"],
                          "Reboot exact task and draft");
                    scripts = m->script_calls();
                    restarted = true;
                }
                check(m->type(answer), "Complete input");
                check(m->key("submit"), "Submit spelling");
            }
            check(m->script_calls() == scripts, "No JS in vocabulary interaction");
        }
        check(steps < 100 && gapSeen && fullSeen && wrong && restarted,
              "Full staged flow coverage");
        check(read(dir / "course-v2.json") == oldCourse &&
                  read(dir / "profile-v2.json") == oldProfile,
              "Original files byte-preserved");
        act("v-return", "value", "home");
        act("v-go", "value", "review");
        paint("09-review");
        act("v-go", "value", "notebook");
        paint("10-notebook");
        act("v-go", "value", "notebook-words");
        act("v-open");
        m->back();
        m->page(page);
        check(page.route == "notebook-words", "Notebook detail returns to list");
        m->back(); m->page(page);
        check(page.route == "notebook", "List returns to notebook");
        auto before = state(dir);
        fs::rename(dir / "vocabulary", dir / "vocabulary-held");
        act("v-do", "type", "learnNotebook");
        m->status(stat);
        check(!str(VJ::parse(stat), "courseError").empty(), "Storage failure surfaced");
        fs::rename(dir / "vocabulary-held", dir / "vocabulary");
        check(state(dir) == before, "Failed append cannot change persisted progress");
        act("v-check-recovery");
        act("v-do", "type", "learnNotebook");
        check(str(state(dir)["session"], "source") == "notebook", "Retry works");
        auto saved = state(dir);
        m.reset();
        m = std::make_unique<LearningWeb>();
        check(m->init(dir.generic_string()), "Reopen after journal");
        m->page(page);
        check(state(dir) == saved, "All committed events recover");
        m.reset();
        const auto committedLog = read(dir / "vocabulary/events.ndjson");
        { std::ofstream f(dir / "vocabulary/events.ndjson", std::ios::binary | std::ios::app); f << "{\"data\":\"interrupted"; }
        m = std::make_unique<LearningWeb>();
        check(m->init(dir.generic_string()), "Torn-write reopen");
        m->page(page);
        check(state(dir) == saved, "Torn tail keeps all committed progress");
        check(read(dir / "vocabulary/events.ndjson") == committedLog, "Only uncommitted tail removed");
        bool tailKept = false;
        for (const auto &f : fs::directory_iterator(dir / "vocabulary"))
            if (f.path().extension() == ".tail") tailKept = read(f.path()) == "{\"data\":\"interrupted";
        check(tailKept, "Interrupted bytes retained for recovery");
        m.reset();
        const auto checkpoint = read(dir / "vocabulary/state.json");
        { std::ofstream f(dir / "vocabulary/state.json", std::ios::binary); f << "{\"checksum\":0,\"data\":\"{}\"}"; }
        m = std::make_unique<LearningWeb>();
        check(m->init(dir.generic_string()), "Corrupt checkpoint has a visible error page");
        m->page(page); m->status(stat);
        check(!str(VJ::parse(stat), "courseError").empty(), "Corrupt checkpoint is blocked");
        check(read(dir / "vocabulary/events.ndjson") == committedLog, "Corruption never resets journal");
        m.reset();
        { std::ofstream f(dir / "vocabulary/state.json", std::ios::binary); f << checkpoint; }
        std::ofstream(out / "checks.json")
            << VJ{{"passed", checks},           {"timings", timings},
                  {"oldCourseUnchanged", true}, {"oldProfileUnchanged", true},
                  {"nativeNoJs", true},         {"realDevice", false}}
                   .dump(2);
        std::cout << "Vocabulary UI/storage checks passed " << checks << std::endl;
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
