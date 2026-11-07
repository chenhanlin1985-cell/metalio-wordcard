#include "vocabulary.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
using namespace wordcard;
namespace fs = std::filesystem;
namespace {
int checks = 0;
constexpr int64_t Now = 1793750400000LL;
void check(bool ok, const char *why) {
    ++checks;
    require(ok, why);
}
std::string read(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    require(bool(in), "Cannot read synthetic fixture");
    return std::string(std::istreambuf_iterator<char>(in), {});
}
VJ state(const fs::path &profile) {
    auto frame = VJ::parse(str(VJ::parse(read(profile / "vocabulary/state.json")), "data"));
    auto result = frame["state"];
    std::ifstream in(profile / "vocabulary/events.ndjson", std::ios::binary);
    if (in) {
        in.seekg(num(frame, "journalOffset"));
        std::string line;
        while (std::getline(in, line))
            result = result.patch(VJ::parse(str(VJ::parse(line), "data"))["patch"]);
    }
    return result;
}
VJ payload(const std::string &word, const std::string &meaning = "测试释义；仅用于合成测试",
           const std::string &ipa = "") {
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : word) { hash ^= c; hash *= 1099511628211ull; }
    std::ostringstream id;
    id << "dict:personal:" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return VJ{{"id", id.str()}, {"word", word}, {"meaning", meaning}, {"ipa", ipa},
              {"pos", ""}, {"example", ""}, {"exampleZh", ""}};
}
VJ status(const NativeVocabulary &model) { return VJ::parse(model.status()); }
void dispatch(NativeVocabulary &model, const VJ &action) {
    check(model.dispatch(action.dump()), "Native action handled");
    check(str(status(model), "courseError").empty(), "Native action succeeds");
}
void checkpoint(const fs::path &profile, const VJ &data) {
    fs::create_directories(profile / "vocabulary");
    const auto bytes = VJ{{"state", data}, {"journalOffset", 0}}.dump();
    uint32_t hash = 2166136261u;
    for (unsigned char c : bytes) hash = (hash ^ c) * 16777619u;
    std::ofstream(profile / "vocabulary/state.json", std::ios::binary)
        << VJ{{"data", bytes}, {"checksum", hash}}.dump() << '\n';
}
}
int main(int argc, char **argv) {
    try {
        require(argc == 3, "Usage: vocabulary_external <catalog directory> <fresh output>");
        const fs::path resources = argv[1], out = argv[2];
        require(!fs::exists(out), "Output must be new; never overwrite a profile");
        fs::create_directories(out / "vocabulary");
        for (const auto *name : {"catalog.json", "catalog.tsv", "catalog.ndjson"})
            fs::copy_file(resources / name, out / "vocabulary" / name);
        VocabularyCatalog catalog;
        check(catalog.load(resources.generic_string()), "Production catalog loads");
        const auto curatedCount = catalog.entries.size();
        auto external = payload("quizzacious", "测试词义\n保留中文、换行和 IPA", "kwɪˈzeɪʃəs");
        const auto id = str(external, "id");
        check(std::none_of(catalog.entries.begin(), catalog.entries.end(), [&](const auto &entry) {
            return entry.word == str(external, "word");
        }), "External fixture is not curated");
        catalog.install_external(external);
        const auto full = catalog.get(id);
        check(catalog.entries.size() == curatedCount + 1 && catalog.entry(id) != nullptr,
              "External word is indexed");
        check(!catalog.entry(id)->eligible && catalog.entry(id)->stage == "personal" &&
              !flag(full, "lessonEligible") && !flag(full, "humanVerified"),
              "Dictionary import does not claim curated lesson eligibility");
        check(str(full, "dictionaryDefinition") == str(external, "meaning") &&
              str(full, "meaning") == "测试词义" &&
              str(full, "ipa") == str(external, "ipa"), "Definition and real IPA are preserved");
        check(str(full["audio"], "kind") == "generated" &&
              str(full["audio"], "text") == str(external, "word") &&
              str(full["audio"], "path") == "audio/external-" + id.substr(14) + ".wav",
              "Stable generated audio path and text use normal fallback");
        catalog.install_external(external);
        check(catalog.entries.size() == curatedCount + 1, "Repeated install is unique");
        auto rejects = [&](const VJ &value, const char *why) {
            const auto count = catalog.entries.size();
            const auto definitions = catalog.external;
            bool failed = false;
            try { catalog.install_external(value); } catch (const std::exception &) { failed = true; }
            check(failed && catalog.entries.size() == count && catalog.external == definitions, why);
        };
        auto bad = external; bad["id"] = "dict:personal:../../state.json";
        rejects(bad, "Unsafe external ID is rejected without mutation");
        bad = external; bad["word"] = "another-word";
        rejects(bad, "ID must match exact case-sensitive word hash");
        bad = external; bad["meaning"] = "changed definition";
        rejects(bad, "Installed definitions cannot be overwritten");
        bad = payload("bounded-external"); bad["meaning"] = std::string(4001, 'x');
        rejects(bad, "Oversize meaning is rejected without truncation");
        bad = payload(std::string(101, 'x'));
        rejects(bad, "Oversize word is rejected");
        bad = payload("invalid-ipa"); bad["ipa"] = std::string(257, 'x');
        rejects(bad, "Oversize IPA is rejected");
        bad = payload("invalid-text"); bad["meaning"] = std::string("bad\0value", 9);
        rejects(bad, "NUL content is rejected");
        bad = payload("invalid-type"); bad["meaning"] = 123;
        rejects(bad, "Numeric definition is not coerced");
        auto missingIpa = payload("no-ipa-external"); missingIpa.erase("ipa");
        catalog.install_external(missingIpa);
        check(str(catalog.get(str(missingIpa, "id")), "ipa").empty(), "Missing IPA stays absent");
        std::string longDefinition = "\n；  ";
        for (int i = 0; i < 900; ++i) longDefinition += "释义";
        // Stay inside the real input budget while exercising a multibyte cut.
        longDefinition.resize(6 + 660 * 6);
        longDefinition += "；第二义必须完整保留";
        const auto lengthy = payload("lengthy-definition", longDefinition);
        catalog.install_external(lengthy);
        const auto lengthyCard = catalog.get(str(lengthy, "id"));
        check(str(lengthyCard, "dictionaryDefinition") == longDefinition &&
              str(lengthyCard, "meaning").size() <= 80 &&
              str(lengthyCard, "meaning").find("…") != std::string::npos,
              "Long dictionary text is retained with a bounded training sense");
        check(VJ::parse(lengthyCard.dump()) == lengthyCard, "Short sense never splits a UTF-8 code point");
        catalog.install_external(lengthyCard);
        check(catalog.get(str(lengthy, "id")) == lengthyCard,
              "Canonical long definition registers identically after persistence");
        const auto english = payload("english-definition", std::string(70, 'x') + " extraordinarily long remainder");
        catalog.install_external(english);
        check(str(catalog.get(str(english, "id")), "meaning") == std::string(70, 'x') + "…",
              "Training sense truncation retains whole English words");
        catalog.remove_external(id);
        check(!catalog.entry(id) && catalog.entry(str(missingIpa, "id")) != nullptr,
              "Rollback preserves subsequent indices");
        catalog.install_external(external);

        auto open = [&](const char *name) {
            const auto profile = out / (std::string("learning-web-") + name);
            fs::create_directories(profile);
            auto model = std::make_unique<NativeVocabulary>(profile.generic_string());
            check(model->enter(VJ(), Now), "Native synthetic profile opens");
            check(str(status(*model), "courseError").empty(), "Native profile loads without error");
            return model;
        };
        const auto profile = out / "learning-web-external";
        auto model = open("external");
        const auto before = state(profile);
        check(!before.contains("externalWords"), "Fresh schema keeps externalWords optional");
        check(model->import_word(external), "External definition imports through durable bridge");
        auto imported = state(profile);
        check(imported["externalWords"][id] == full && imported["notebook"] == VJ::array({id}),
              "Journal persists definition and unique notebook membership together");
        for (const auto *key : {"cards", "seen", "session", "paused", "days", "settings"})
            check(imported[key] == before[key], "Import does not mutate learning progress");
        check(str(status(*model), "lastImportedWord") == str(external, "word") &&
              str(status(*model), "lastImportedId") == id, "Success receipt identifies imported word");
        check(model->import_word(external), "Repeated dictionary add succeeds");
        check(state(profile)["notebook"] == VJ::array({id}), "Duplicate add has one notebook entry");
        bad = external; bad["meaning"] = "replacement";
        const auto committed = state(profile);
        const auto journal = read(profile / "vocabulary/events.ndjson");
        check(!model->import_word(bad), "Changed existing definition fails");
        check(state(profile) == committed && read(profile / "vocabulary/events.ndjson") == journal,
              "Rejected import preserves every state and log byte");
        check(!status(*model).contains("lastImportedId"), "Failed import clears success receipt");
        check(model->import_word(external), "Valid retry clears prior import error");
        model.reset();
        model = open("external");
        check(state(profile)["externalWords"][id] == full, "Reboot replays external word from WAL");
        check(!status(*model).contains("lastImportedId"), "Receipt is not persisted");
        dispatch(*model, VJ{{"action", "v-open"}, {"value", id}});
        bool showsWord = false, showsMeaning = false;
        for (const auto &block : model->page().blocks) {
            showsWord |= block.text == str(external, "word");
            showsMeaning |= block.text.find(str(external, "meaning")) != std::string::npos;
        }
        check(showsWord && showsMeaning, "Imported notebook detail renders persisted definition");
        dispatch(*model, VJ{{"action", "v-audio"}, {"id", id}, {"example", false}});
        std::string audioPath; int serial = 0; bool stop = false; double rate = 0;
        check(model->take_audio(audioPath, serial, stop, &rate) && !stop &&
              model->audio_text(audioPath) == str(external, "word"), "Native playback passes fallback text");
        check(!fs::exists(out / audioPath), "Import does not generate an SD audio file");
        model->stop_audio();
        dispatch(*model, VJ{{"action", "v-do"}, {"type", "learnNotebook"}});
        check(str(status(*model)["batch"], "source") == "notebook", "Notebook has explicit session source");
        auto learning = state(profile);
        dispatch(*model, VJ{{"action", "v-do"}, {"type", "learnNext"},
                           {"sessionId", learning["session"]["id"]}, {"learnAt", 0}});
        const auto learningSaved = state(profile);
        check(learningSaved["seen"].contains(id), "External notebook word enters native learning");
        model.reset(); model = open("external");
        check(state(profile) == learningSaved, "Reboot keeps imported active notebook state exact");

        VocabularyEngine engine(catalog);
        auto reviewed = engine.create(Now);
        reviewed["externalWords"][id] = full;
        reviewed["notebook"] = VJ::array({id});
        reviewed["seen"][id] = Now - 86400000;
        reviewed["cards"][id]["meaning"] = VocabularyEngine::empty_card(Now - 1000);
        reviewed["cards"][id]["spelling"] = VocabularyEngine::empty_card(Now - 1000);
        checkpoint(out / "learning-web-reviewed", reviewed);
        auto restored = open("reviewed");
        check(state(out / "learning-web-reviewed") == reviewed,
              "External definitions install before saved card IDs are validated");
        dispatch(*restored, VJ{{"action", "v-start"}, {"intent", "review"}});
        check(str(status(*restored)["batch"], "source") == "review", "Imported cards use native due review");

        auto curated = open("curated");
        const auto existing = catalog.entries.front();
        check(curated->import_word(payload(existing.word)), "Exact curated spelling imports");
        const auto curatedState = state(out / "learning-web-curated");
        check(curatedState["notebook"] == VJ::array({existing.id}) &&
              !curatedState.contains("externalWords") && str(status(*curated), "lastImportedId") == existing.id,
              "Exact catalog match preserves original ID without external schema");
        std::string upper;
        for (const auto &entry : catalog.entries) {
            if (!entry.eligible || entry.word.size() < 2) continue;
            auto candidate = entry.word;
            for (auto &c : candidate) if ((unsigned char)c < 128) c = char(std::toupper((unsigned char)c));
            if (candidate == entry.word) continue;
            bool exists = false;
            for (const auto &other : catalog.entries) exists |= other.word == candidate;
            if (!exists) { upper = candidate; break; }
        }
        require(!upper.empty(), "Needs case-sensitive fixture");
        const auto uppercase = payload(upper);
        check(curated->import_word(uppercase) &&
              str(status(*curated), "lastImportedId") == str(uppercase, "id"),
              "Different capitalization never merges with curated lowercase word");

        auto failed = open("write-failure");
        const auto failProfile = out / "learning-web-write-failure";
        const auto originalSnapshot = read(failProfile / "vocabulary/state.json");
        fs::create_directory(failProfile / "vocabulary/events.ndjson");
        check(!failed->import_word(external), "Journal write failure rejects import");
        check(read(failProfile / "vocabulary/state.json") == originalSnapshot &&
              num(status(*failed), "revision") == 0 && !status(*failed).contains("lastImportedId"),
              "Failed persistent write leaves progress and receipt unchanged");
        fs::remove(failProfile / "vocabulary/events.ndjson");
        auto corrected = external; corrected["meaning"] = "重试后正确释义";
        dispatch(*failed, VJ{{"action", "v-check-recovery"}});
        check(num(status(*failed), "revision") == 0, "Checking restored storage does not replay the failed import");
        dispatch(*failed, VJ{{"action", "v-import"}, {"value", corrected.dump()}});
        check(str(state(failProfile)["externalWords"][id], "dictionaryDefinition") == "重试后正确释义",
              "Failed import rolls memory registration back and allows corrected retry");
        std::cout << checks << " external dictionary checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "External dictionary check failed: " << e.what() << '\n';
        return 1;
    }
}
