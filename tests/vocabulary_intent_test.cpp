#include "vocabulary.hpp"
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <set>
using namespace wordcard;
namespace {
int checks = 0;
void check(bool ok, const char *why) {
    ++checks;
    require(ok, why);
}
}
int main(int argc, char **argv) {
    try {
        require(argc == 2, "Usage: vocabulary_intent <catalog directory>");
        VocabularyCatalog catalog;
        require(catalog.load(argv[1]), "Catalog unavailable");
        VocabularyEngine engine(catalog);
        int64_t now = 1793750400000LL;
        auto act = [&](const VJ &state, const VJ &action) {
            VJ events;
            now += 1000;
            return engine.reduce(state, action, now, events);
        };
        const VJ startNew{{"type", "begin"}, {"intent", "new"}};
        const VJ startReview{{"type", "begin"}, {"intent", "review"}};
        auto fails = [&](const VJ &state, const VJ &action, const char *why) {
            const auto before = state;
            bool rejected = false;
            try { (void)act(state, action); }
            catch (const std::exception &) { rejected = true; }
            check(rejected && state == before, why);
        };
        auto chosen = act(engine.create(now), VJ{{"type", "chooseStage"}, {"stage", "foundation"}});
        std::vector<std::string> foundation;
        for (const auto &entry : catalog.entries)
            if (entry.eligible && entry.stage == "foundation") foundation.push_back(entry.id);
        require(foundation.size() >= 30, "Fixture needs thirty foundation words");
        auto answer = [&](const VJ &state) {
            auto task = engine.current(state);
            const auto kind = str(task, "kind");
            const auto word = catalog.get(str(task, "id"));
            const auto value = kind == "meaning" ? word["meaning"] : kind == "gap"
                ? engine.gap(task)["answer"] : word["word"];
            return act(state, VJ{{"type", "answer"}, {"taskId", task["uid"]}, {"value", value}});
        };
        auto step = [&](const VJ &state) {
            const auto &session = state["session"];
            if (str(session, "phase") == "learn")
                return act(state, VJ{{"type", "learnNext"}, {"sessionId", session["id"]},
                                     {"learnAt", session["learnAt"]}});
            if (!field(session, "feedback").is_null())
                return act(state, VJ{{"type", "continue"}, {"taskId", session["feedback"]["uid"]}});
            return answer(state);
        };
        auto finish = [&](VJ state) {
            int steps = 0;
            while (str(state["session"], "phase") != "summary") {
                require(++steps < 150, "Session failed to terminate");
                state = step(state);
            }
            return state;
        };
        auto parked = [](const VJ &state, const VJ &id) {
            for (const auto &session : state["paused"])
                if (session["id"] == id) return session;
            return VJ();
        };
        fails(chosen, VJ{{"type", "importNotebook"}, {"id", "dict:personal:0000000000000000"},
                          {"word", VJ{{"id", "dict:personal:0000000000000000"}, {"word", "example"}}}},
              "Notebook import rejects unregistered external words atomically");
        fails(chosen, VJ{{"type", "importNotebook"}, {"id", foundation[0]},
                          {"word", catalog.get(foundation[0])}},
              "Built-in entries must use normal notebook action, not external import");
        auto withDue = chosen;
        withDue["seen"][foundation[20]] = now - 86400000;
        withDue["cards"][foundation[20]]["meaning"] = VocabularyEngine::empty_card(now - 1000);
        auto learning = act(withDue, startNew);
        check(str(learning["session"], "source") == "new" &&
              str(learning["session"], "phase") == "learn", "New entry never redirects to due review");
        check(learning["cards"] == withDue["cards"], "Starting new leaves FSRS cards unchanged");
        check(str(act(withDue, VJ{{"type", "begin"}})["session"], "source") == "review",
              "Unqualified begin keeps legacy due-first behavior");
        int steps = 0;
        while (str(engine.current(learning), "kind") != "gap" ||
               !field(learning["session"], "feedback").is_null() ||
               str(learning["session"], "phase") != "tasks") {
            require(++steps < 80, "Gap fixture failed");
            learning = step(learning);
        }
        learning = act(learning, VJ{{"type", "draft"}, {"taskId", engine.current(learning)["uid"]},
                                    {"value", "ab"}});
        const auto savedLearning = learning["session"];
        auto review = act(learning, startReview);
        check(str(review["session"], "source") == "review", "Review entry switches purpose");
        check(parked(review, savedLearning["id"]) == savedLearning,
              "Switch preserves every learning field including exact draft and task");
        check(review["session"]["tasks"].size() == 1 &&
              review["session"]["tasks"][0]["id"] == foundation[20], "Review uses only due skills");
        review = act(review, VJ{{"type", "answer"}, {"taskId", engine.current(review)["uid"]},
                               {"unknown", true}});
        const auto savedReview = review["session"];
        auto resumed = act(review, startNew);
        check(resumed["session"] == savedLearning && parked(resumed, savedReview["id"]) == savedReview,
              "Returning to new restores draft and parks exact review feedback/retry queue");
        auto reviewAgain = act(resumed, startReview);
        check(reviewAgain["session"] == savedReview, "Review restores feedback before advancing");
        auto repeatedReview = act(reviewAgain, startReview);
        check(repeatedReview["session"] == savedReview &&
              repeatedReview["paused"] == reviewAgain["paused"], "Repeated start cannot duplicate a parked session");
        auto autoResume = act(reviewAgain, VJ{{"type", "begin"}});
        check(autoResume["session"] == savedReview, "Legacy begin still resumes whichever session is active");
        check(engine.resumable(reviewAgain, "new") == savedLearning, "UI query finds parked new group");
        check(engine.resumable(reviewAgain, "review") == savedReview, "UI query finds active review");

        const auto beforeQueries = reviewAgain;
        (void)engine.resumable(reviewAgain, "notebook");
        (void)engine.available_due(reviewAgain, now);
        check(reviewAgain == beforeQueries, "UI availability queries never mutate progress");

        auto reserved = act(chosen, startNew);
        const auto reservedGroup = reserved["session"];
        const auto reservedId = str(reservedGroup["ids"][0]);
        reserved["cards"][reservedId]["meaning"] = VocabularyEngine::empty_card(now - 1000);
        reserved["cards"][foundation[20]]["meaning"] = VocabularyEngine::empty_card(now - 1000);
        check(engine.due(reserved, now).size() == 2 && engine.available_due(reserved, now).size() == 1,
              "Due count remains compatible while actionable review excludes unfinished same-skill cards");
        auto reservedReview = act(reserved, startReview);
        check(reservedReview["session"]["ids"] == VJ::array({foundation[20]}) &&
              parked(reservedReview, reservedGroup["id"]) == reservedGroup,
              "Switching cannot schedule a second copy of a parked learning card");
        check(engine.available_due(reservedReview, now).empty(),
              "Both parked learning and active review reserve their pending cards");
        auto onlyReserved = reserved;
        onlyReserved["cards"].erase(foundation[20]);
        fails(onlyReserved, startReview, "No actionable review rejects atomically instead of opening new or replacing a session");
        fails(chosen, startReview, "Empty review never starts learning");
        fails(chosen, VJ{{"type", "begin"}, {"intent", "other"}}, "Unknown entry purpose is rejected");

        auto stage = act(chosen, startNew);
        const auto foundationGroup = stage["session"];
        stage = act(stage, VJ{{"type", "chooseStage"}, {"stage", "everyday"}});
        check(engine.resumable(stage, "new").is_null(), "Changing stage cannot silently restore another-stage group");
        stage = act(stage, startNew);
        const auto everydayGroup = stage["session"];
        for (const auto &id : everydayGroup["ids"])
            check(catalog.entry(str(id))->stage == "everyday", "New group belongs to selected stage");
        check(parked(stage, foundationGroup["id"]) == foundationGroup, "Other-stage pause retained");
        stage = act(stage, VJ{{"type", "chooseStage"}, {"stage", "foundation"}});
        stage = act(stage, startNew);
        check(stage["session"] == foundationGroup && parked(stage, everydayGroup["id"]) == everydayGroup,
              "Returning to original stage resumes its exact group");
        auto legacySwitch = learning;
        legacySwitch["session"]["source"] = "legacy";
        const auto legacyDraft = legacySwitch["session"];
        legacySwitch = act(legacySwitch, startReview);
        check(parked(legacySwitch, legacyDraft["id"]) == legacyDraft &&
              legacySwitch["nativeResumeStages"][str(legacyDraft, "id")] == "foundation",
              "Explicit switch parks legacy byte-for-byte and records its current stage separately");
        check(engine.resumable(legacySwitch, "new") == legacyDraft,
              "Home discovers a legacy group parked by an explicit review switch");
        legacySwitch = act(legacySwitch, startNew);
        check(legacySwitch["session"] == legacyDraft &&
              legacySwitch["session"]["draft"]["value"] == "ab",
              "Legacy new-review-new round trip restores its exact question and draft");
        legacySwitch = act(legacySwitch, VJ{{"type", "chooseStage"}, {"stage", "everyday"}});
        check(engine.resumable(legacySwitch, "new").is_null(),
              "A recorded legacy origin cannot restore into a different selected stage");

        auto legacy = act(chosen, startNew);
        legacy["session"]["source"] = "legacy";
        const auto legacyGroup = legacy["session"];
        check(act(legacy, startNew)["session"] == legacyGroup, "Active migrated lesson remains continuable from home");
        legacy = act(legacy, VJ{{"type", "chooseStage"}, {"stage", "everyday"}});
        check(engine.resumable(legacy, "new").is_null(), "Parked legacy has no inferred stage ownership");
        legacy = act(legacy, startNew);
        check(str(legacy["session"], "source") == "new" &&
              parked(legacy, legacyGroup["id"]) == legacyGroup, "Selecting a stage preserves parked legacy without hijacking new entry");
        legacy = act(legacy, VJ{{"type", "resumePaused"}, {"id", legacyGroup["id"]}});
        check(legacy["session"] == legacyGroup, "Explicit legacy resume remains available");

        auto notebook = chosen;
        for (int i = 0; i < 5; ++i) notebook["notebook"].push_back(foundation[size_t(i)]);
        notebook = act(notebook, VJ{{"type", "learnNotebook"}, {"explicit", true}});
        const auto notebookGroup = notebook["session"];
        check(engine.resumable(notebook, "new").is_null(), "Notebook never matches home new intent");
        notebook = act(notebook, startNew);
        const auto notebookAdjacentNew = notebook["session"];
        for (const auto &id : notebookAdjacentNew["ids"])
            check(!has(notebookGroup["ids"], id), "Unseen notebook words are reserved against duplicate new groups");
        check(parked(notebook, notebookGroup["id"]) == notebookGroup, "Home parks notebook intact");
        notebook = act(notebook, VJ{{"type", "learnNotebook"}, {"explicit", true}});
        check(notebook["session"] == notebookGroup &&
              parked(notebook, notebookAdjacentNew["id"]) == notebookAdjacentNew,
              "Explicit notebook entry restores only notebook and preserves new group");
        const auto notebookPaused = notebook["paused"];
        notebook = act(notebook, VJ{{"type", "learnNotebook"}, {"explicit", true}});
        check(notebook["session"] == notebookGroup && notebook["paused"] == notebookPaused,
              "Repeated notebook entry is idempotent");

        auto almostGoal = chosen;
        almostGoal["serial"] = 600;
        almostGoal["settings"] = VJ{{"mode", "staged"}, {"dailyNew", 5}};
        almostGoal["days"][VocabularyEngine::date(now)] = VJ{
            {"new", VJ::array({foundation[0], foundation[1], foundation[2]})},
            {"reviews", VJ::array()}, {"answers", 0}};
        for (int i = 0; i < 3; ++i) almostGoal["seen"][foundation[size_t(i)]] = now;
        auto softGroup = act(almostGoal, VJ{{"type", "begin"}, {"intent", "new"}, {"allowExtra", true}});
        check(softGroup["session"]["ids"].size() == 5,
              "Explicit soft goal keeps five words when only two remain toward daily target");
        while (str(softGroup["session"], "phase") == "learn") softGroup = step(softGroup);
        check(str(softGroup["session"], "phase") == "tasks" &&
              str(engine.current(softGroup), "kind") == "meaning" &&
              num(softGroup["session"], "answered") == 0,
              "Five-word soft-goal group reaches an actual meaning question instead of deferring every task");
        check(softGroup["days"][VocabularyEngine::date(now)]["new"].size() == 8,
              "Explicit group may cross the daily target while preserving accurate daily count");
        check(act(almostGoal, startNew)["session"]["ids"].size() == 2,
              "Explicit entry without allowExtra still respects remaining target allowance");
        check(act(almostGoal, VJ{{"type", "begin"}})["session"]["ids"].size() == 2,
              "Unqualified begin retains the original web-parity quota semantics");

        auto goal = chosen;
        goal["settings"] = VJ{{"mode", "meaning"}, {"dailyNew", 5}};
        goal = finish(act(goal, startNew));
        check(goal["days"][VocabularyEngine::date(now)]["new"].size() == 5, "Goal fixture completes five new words");
        fails(goal, startNew, "Extra group requires explicit request after reaching daily goal");
        fails(goal, VJ{{"type", "begin"}}, "Legacy automatic entry retains daily limit");
        std::set<std::string> learned;
        for (const auto &id : goal["session"]["ids"]) learned.insert(str(id));
        for (int group = 0; group < 2; ++group) {
            const auto beforeCards = goal["cards"];
            goal = act(goal, VJ{{"type", "begin"}, {"intent", "new"}, {"allowExtra", true}});
            check(str(goal["session"], "source") == "new" && goal["session"]["ids"].size() == 5,
                  "Explicit extra starts at most five new words after goal");
            check(goal["cards"] == beforeCards, "Extra group does not reset earlier memory or due dates");
            for (const auto &id : goal["session"]["ids"])
                check(learned.insert(str(id)).second, "Next group never repeats completed words");
            goal = finish(goal);
        }
        check(goal["days"][VocabularyEngine::date(now)]["new"].size() == 15,
              "Daily goal is soft across successive explicit extra groups");
        auto exhausted = chosen;
        for (const auto &id : foundation) exhausted["seen"][id] = now;
        fails(exhausted, VJ{{"type", "begin"}, {"intent", "new"}, {"allowExtra", true}},
              "Exhausted stage does not fall through to review");
        check(engine.resumable(goal, "new").is_null(), "Completed summary is never resumed as an unfinished group");
        // Availability must describe precisely what the explicit action can start.
        VJ foundationIds = VJ::array();
        for (const auto &id : foundation) foundationIds.push_back(id);
        check(engine.available_new(chosen) == foundationIds,
              "New availability preserves curriculum order without a daily-goal limit");
        auto availability = reserved;
        availability["seen"][foundation[6]] = now;
        auto parkedReservation = reservedGroup;
        parkedReservation["id"] = "availability-parked";
        parkedReservation["ids"] = VJ::array({foundation[5]});
        availability["paused"].push_back(parkedReservation);
        availability["notebook"] = VJ::array({foundation[8], foundation[5], foundation[0],
                                             foundation[7], foundation[6]});
        const auto availabilityBefore = availability;
        const auto availableNew = engine.available_new(availability);
        check(!availableNew.empty() && availableNew[0] == foundation[7] &&
              !has(availableNew, foundation[0]) && !has(availableNew, foundation[5]) &&
              !has(availableNew, foundation[6]),
              "New availability excludes active, parked and already seen words");
        check(engine.available_notebook(availability) == VJ::array({foundation[8], foundation[7]}),
              "Notebook availability preserves notebook order across reservations");
        check(availability == availabilityBefore, "Availability does not mutate sessions or memory");

        auto allReserved = chosen;
        allReserved["session"] = notebookGroup;
        for (const auto &id : foundation)
            if (!has(notebookGroup["ids"], id)) allReserved["seen"][id] = now;
        check(engine.available_new(allReserved).empty() &&
              engine.resumable(allReserved, "new").is_null(),
              "Only reserved unseen notebook words cannot advertise an available new group");
        fails(allReserved, VJ{{"type", "begin"}, {"intent", "new"}, {"allowExtra", true}},
              "Explicit new rejects only-reserved words without replacing the notebook");
        allReserved["session"]["source"] = "new";
        allReserved["notebook"] = notebookGroup["ids"];
        check(engine.available_notebook(allReserved).empty(),
              "Only reserved notebook words cannot advertise a fresh notebook group");
        fails(allReserved, VJ{{"type", "learnNotebook"}, {"explicit", true}},
              "Explicit notebook rejects only-reserved words without replacing new learning");

        auto notebookMore = chosen;
        notebookMore["serial"] = 900;
        notebookMore["settings"]["mode"] = "meaning";
        for (size_t i = 0; i < 7; ++i) notebookMore["notebook"].push_back(foundation[i]);
        notebookMore = finish(act(notebookMore, VJ{{"type", "learnNotebook"}, {"explicit", true}}));
        const auto nextNotebookIds = engine.available_notebook(notebookMore);
        check(nextNotebookIds == VJ::array({foundation[5], foundation[6]}),
              "Completed notebook group exposes its remaining unseen words directly");
        const auto notebookCards = notebookMore["cards"];
        notebookMore = act(notebookMore, VJ{{"type", "learnNotebook"}, {"explicit", true}});
        check(notebookMore["session"]["ids"] == nextNotebookIds &&
              str(notebookMore["session"], "source") == "notebook" &&
              notebookMore["cards"] == notebookCards,
              "Next explicit notebook group uses advertised words without changing prior memory");

        // Preview uses the real transition, including round skipping and deferral.
        check(engine.continue_preview(chosen, now).is_null() &&
              engine.continue_preview(learning, now).is_null(),
              "Continue preview is absent before a persisted answer feedback");
        bool sawMeaningToGap = false, sawGapToSpelling = false, sawMeaningToSpelling = false;
        bool sawSameRound = false, sawSummary = false;
        int previewSerial = 1000;
        for (const auto *mode : {"staged", "full", "meaning"}) {
            auto flow = chosen;
            flow["serial"] = previewSerial++;
            flow["settings"]["mode"] = mode;
            flow = act(flow, VJ{{"type", "begin"}, {"intent", "new"}, {"allowExtra", true}});
            bool firstWrong = false;
            int previewSteps = 0;
            while (str(flow["session"], "phase") != "summary") {
                require(++previewSteps < 150, "Preview fixture terminates");
                if (str(flow["session"], "phase") == "learn") {
                    flow = step(flow);
                } else if (!field(flow["session"], "feedback").is_null()) {
                    const auto before = flow;
                    const auto previous = engine.current(flow);
                    const auto preview = engine.continue_preview(flow, now + 1000);
                    check(flow == before, "Preview leaves complete persisted state unchanged");
                    flow = step(flow);
                    const auto next = engine.current(flow);
                    check(preview == VJ{{"phase", flow["session"]["phase"]},
                                        {"kind", str(next, "kind")},
                                        {"followup", flag(next, "followup")},
                                        {"source", flow["session"]["source"]}},
                          "Preview matches actual continuation across all training modes");
                    const auto fromKind = str(previous, "kind"), toKind = str(next, "kind");
                    sawMeaningToGap |= fromKind == "meaning" && toKind == "gap";
                    sawGapToSpelling |= fromKind == "gap" && toKind == "spelling";
                    sawMeaningToSpelling |= fromKind == "meaning" && toKind == "spelling";
                    sawSameRound |= !next.is_null() && fromKind == toKind &&
                                    flag(previous, "followup") == flag(next, "followup");
                    sawSummary |= str(preview, "phase") == "summary";
                } else if (!firstWrong) {
                    const auto task = engine.current(flow);
                    flow = act(flow, VJ{{"type", "answer"}, {"taskId", task["uid"]}, {"unknown", true}});
                    firstWrong = true;
                } else {
                    flow = answer(flow);
                }
            }
        }
        check(sawMeaningToGap && sawGapToSpelling && sawMeaningToSpelling &&
              sawSameRound && sawSummary,
              "Preview covers each normal round boundary, next question and summary");

        auto deferred = review;
        const auto deferredBefore = deferred;
        const auto deferredAt = num(deferred, "lastClock") + 1000;
        const auto deferredPreview = engine.continue_preview(deferred, deferredAt);
        VJ deferredEvents;
        const auto deferredResult = engine.reduce(deferred,
            VJ{{"type", "continue"}, {"taskId", deferred["session"]["feedback"]["uid"]}},
            deferredAt, deferredEvents);
        check(str(deferredPreview, "phase") == "summary" &&
              str(deferredResult["session"], "phase") == "summary" &&
              !deferredResult["session"]["deferrals"].empty() && deferred == deferredBefore,
              "Preview skips a spacing-blocked correction without persisting its deferral");
        const auto laterPreview = engine.continue_preview(deferred, deferredAt + 900000);
        check(str(laterPreview, "phase") == "tasks" && flag(laterPreview, "followup") &&
              str(laterPreview, "kind") == "meaning" && deferred == deferredBefore,
              "Preview discovers a later eligible correction without changing exposure timing");

        auto externalPayload = [](const std::string &word) {
            uint64_t hash = 14695981039346656037ull;
            for (unsigned char c : word) { hash ^= c; hash *= 1099511628211ull; }
            std::ostringstream id;
            id << "dict:personal:" << std::hex << std::setfill('0') << std::setw(16) << hash;
            return VJ{{"id", id.str()}, {"word", word}, {"meaning", "Alias exposure fixture"},
                      {"ipa", ""}, {"pos", ""}};
        };
        const auto spellingTask = engine.current(learning);
        const auto currentSpelling = catalog.entry(str(spellingTask, "id"))->word;
        auto aliasSpelling = currentSpelling;
        for (char &c : aliasSpelling)
            if ((unsigned char)c < 128) c = char(std::toupper((unsigned char)c));
        auto alias = externalPayload(aliasSpelling);
        catalog.install_external(alias);
        const auto aliasId = str(alias, "id");
        const auto aliasCount = catalog.entries.size();
        check(aliasId != str(spellingTask, "id") && !catalog.entry(aliasId)->eligible,
              "Exposure alias is an independent personal dictionary record");
        check(engine.current_word_matches(learning, " \t" + aliasSpelling + "\r\n") &&
              !engine.current_word_matches(learning, aliasSpelling + "-different") &&
              catalog.entries.size() == aliasCount,
              "Current-word comparison uses answer normalization without merging catalog IDs");
        auto personalNotebook = chosen;
        personalNotebook["notebook"] = VJ::array({aliasId});
        check(engine.available_notebook(personalNotebook) == VJ::array({aliasId}) &&
              !has(engine.available_new(personalNotebook), aliasId),
              "Personal dictionary words remain notebook-eligible, outside the curriculum");
        personalNotebook = act(personalNotebook, VJ{{"type", "learnNotebook"}, {"explicit", true}});
        check(personalNotebook["session"]["ids"] == VJ::array({aliasId}),
              "Explicit notebook accepts advertised personal dictionary words");

        VJ aliasEvents;
        now += 1000;
        const auto audioAlias = engine.reduce(learning,
            VJ{{"type", "audio"}, {"id", aliasId}}, now, aliasEvents);
        const auto audioTask = engine.current(audioAlias);
        check(flag(audioTask, "hint") && flag(audioTask, "audioHint") &&
              !flag(audioTask, "answerRevealed") && !flag(audioTask, "lookupHint"),
              "Same-spelling alias audio marks assistance without displaying the answer");
        check(audioAlias["cards"] == learning["cards"] &&
              audioAlias["session"]["current"] == learning["session"]["current"] &&
              audioAlias["session"]["draft"] == learning["session"]["draft"],
              "Alias audio preserves the exact task, input and FSRS memory");
        check(num(audioAlias["exposures"][str(spellingTask, "id")], "at") == now &&
              num(audioAlias["exposures"][aliasId], "at") == now && aliasEvents.size() == 2 &&
              str(aliasEvents.back(), "viaWordId") == aliasId,
              "Alias audio records original and current-word exposure using existing spacing");
        const auto lookedAlias = act(learning, VJ{{"type", "lookup"}, {"id", aliasId}});
        check(flag(engine.current(lookedAlias), "lookupHint") &&
              flag(engine.current(lookedAlias), "hint") &&
              !flag(engine.current(lookedAlias), "answerRevealed"),
              "Same-spelling alias lookup marks visible assistance without forcing reveal UI");
        const auto answeredAlias = answer(lookedAlias);
        check(flag(answeredAlias["session"]["feedback"], "assisted") &&
              !engine.current_word_matches(answeredAlias, currentSpelling),
              "Alias lookup cannot receive unassisted credit and feedback cannot be hinted again");

        auto meaningExposure = chosen;
        meaningExposure["serial"] = 1100;
        meaningExposure = act(meaningExposure, VJ{{"type", "begin"}, {"intent", "new"}, {"allowExtra", true}});
        while (str(meaningExposure["session"], "phase") == "learn") meaningExposure = step(meaningExposure);
        const auto meaningTask = engine.current(meaningExposure);
        const auto meaningWord = catalog.entry(str(meaningTask, "id"))->word;
        const auto meaningAlias = externalPayload(meaningWord);
        catalog.install_external(meaningAlias);
        const auto meaningAudio = act(meaningExposure, VJ{{"type", "audio"}, {"id", meaningAlias["id"]}});
        check(!flag(engine.current(meaningAudio), "hint") &&
              !flag(engine.current(meaningAudio), "audioHint"),
              "Alias pronunciation in a meaning question retains existing unassisted semantics");
        const auto meaningLookup = act(meaningExposure, VJ{{"type", "lookup"}, {"id", meaningAlias["id"]}});
        check(flag(engine.current(meaningLookup), "lookupHint"),
              "Alias definition lookup marks assistance in a meaning question");

        const auto apostropheWord = externalPayload("study-flow's-probe");
        catalog.install_external(apostropheWord);
        auto apostropheState = learning;
        for (auto &task : apostropheState["session"]["tasks"])
            if (task["uid"] == apostropheState["session"]["current"]) task["id"] = apostropheWord["id"];
        check(engine.current_word_matches(apostropheState, " STUDY-FLOW’S-PROBE "),
              "Exposure matching shares curly-apostrophe and case normalization with answers");

        std::cout << "Vocabulary intent checks passed: " << checks << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
