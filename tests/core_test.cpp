#include "wordcard.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace wordcard;
namespace fs = std::filesystem;
static int checks = 0;
static void require(bool condition, const std::string &what) {
    ++checks;
    if (!condition)
        throw std::runtime_error(what);
}
static void put(const fs::path &path, const std::string &data) {
    std::ofstream f(path, std::ios::binary);
    f << data;
}
int main(int argc, char **argv) {
    try {
        require(argc == 4, "arguments");
        const fs::path work = argv[1];
        std::string error;
        std::vector<Card> ket, pet;
        std::ifstream k(work / "KET_2025_OFFICIAL.csv"), p(work / "PET_2024.csv");
        require(load_deck(k, ket, error) && ket.size() == 1998, "KET: " + error);
        require(load_deck(p, pet, error) && pet.size() == 1871, "PET: " + error);
        std::istringstream special("\xEF\xBB\xBF#comment\r\nid,front,back,tags\r\na,\"say "
                                   "\"\"hi\"\"\",\"你好\n世界\",KET|课堂\r\n");
        std::vector<Card> cards;
        require(load_deck(special, cards, error), "quoted CSV");
        require(cards[0].front == "say \"hi\"" && cards[0].back == "你好\n世界" &&
                    cards[0].group == "课堂",
                "CSV values");
        std::vector<Card> sample = {{"new", "apple", "苹果", "", "", "旅行"},
                                    {"due", "meeting", "会议", "", "", "工作"},
                                    {"future", "plan", "计划", "", "", "工作"}};
        ProgressMap sample_progress;
        sample_progress["due"].seen = 2;
        sample_progress["due"].due_day = 100;
        sample_progress["future"].seen = 1;
        sample_progress["future"].due_day = 101;
        const auto all_counts = study_counts(sample, sample_progress, 100);
        require(all_counts.total == 3 && all_counts.learned == 2 && all_counts.unseen == 1 &&
                    all_counts.due == 1,
                "Home counts distinguish new, learned and due cards");
        const auto work_counts = study_counts(sample, sample_progress, 100, "工作");
        require(work_counts.total == 2 && work_counts.unseen == 0 && work_counts.due == 1,
                "Home counts respect the selected topic");
        require(study_counts(sample, sample_progress, 101).due == 2,
                "Home due count advances with study day");
        for (auto bad :
             {"front,back\nx,\"bad", "id,front,back\na,x,y\na,z,q", "front,back\n\"a\"oops,b"}) {
            std::istringstream in(bad);
            error.clear();
            require(!load_deck(in, cards, error), "reject invalid CSV");
            require(cards.size() == 1 && cards[0].id == "a", "transactional deck parse");
        }
        Progress state;
        review(state, Rating::Good, 100);
        require(state.interval == 1 && state.due_day == 101, "first interval");
        review(state, Rating::Good, 101);
        require(state.interval == 3, "second interval");
        review(state, Rating::Again, 104);
        require(state.interval == 0 && state.due_day == 104 && state.again == 1, "again interval");
        review(state, Rating::Hard, 104);
        require(state.interval == 1 && state.hard == 1, "hard interval");
        for (int i = 0; i < 100; ++i)
            review(state, Rating::Good, 100);
        require(state.interval == 365 && state.ease == 300 && state.box == 7, "scheduler caps");
        ProgressMap progress;
        Session session;
        session.begin(&cards, progress, 100, "课堂");
        require(session.size() == 1 && !session.rate(Rating::Good, progress, 100) &&
                    progress.empty(),
                "reveal gate");
        session.reveal();
        require(session.rate(Rating::Again, progress, 100) && session.size() == 2 &&
                    !session.revealed(),
                "again queue");
        session.reveal();
        require(session.rate(Rating::Again, progress, 100) && session.finished() &&
                    session.size() == 2,
                "bounded again queue");
        progress["a"].due_day = 200;
        session.begin(&cards, progress, 100);
        require(session.finished(), "future cards excluded");
        session.begin(&cards, progress, 200, "other");
        require(session.finished(), "group filter");
        auto path = (work / "progress-test.csv").string();
        std::remove(path.c_str());
        std::remove((path + ".bak").c_str());
        require(save_progress(path, progress, error), "save: " + error);
        ProgressMap restored;
        require(load_progress(path, restored, error) && restored.at("a").again == 2,
                "progress roundtrip");
        require(save_progress(path, progress, error) && fs::exists(path + ".bak"),
                "last committed version retained");
        fs::remove(path);
        put(path + ".tmp", "interrupted");
        require(load_progress(path, restored, error) && fs::exists(path),
                "interrupted rename recovery");
        put(path, "#wordcard-progress-v1,1,1\nid,box\na,1\n");
        error.clear();
        require(!load_progress(path, restored, error) && restored.at("a").again == 2,
                "corruption preserved in memory");
        require(safe_relative_path("audio/apple.wav"), "valid audio path");
        for (auto bad : {"../a.wav", "audio/../../a.wav", "C:/x.wav", "/audio/a.wav",
                         "audio\\a.wav", "audio//x.wav"})
            require(!safe_relative_path(bad), "path traversal rejected");
        std::map<std::string, AudioEntry> index;
        std::ifstream ai(argv[3]);
        require(load_audio_index(ai, index, error) && index.size() == 3868,
                "real audio index: " + error);
        size_t covered = 0;
        for (const auto &x : index)
            if (!x.second.path.empty())
                ++covered;
        require(covered == 3829, "audio coverage");
        WaveReader wav;
        size_t count = 0;
        uint64_t samples = 0;
        int16_t block[997];
        for (const auto &f : fs::directory_iterator(argv[2]))
            if (f.path().extension() == ".wav") {
                require(wav.open(f.path().string(), error), f.path().string() + ": " + error);
                uint32_t read = 0;
                while (auto n = wav.read(block, 997))
                    read += uint32_t(n);
                require(read == wav.samples() && wav.remaining() == 0, "WAV streaming length");
                samples += read;
                ++count;
            }
        require(count == 2285, "all WAV files tested");
        auto badwav = work / "truncated.wav";
        put(badwav, "RIFF\xff\xff\xff\xffWAVE");
        require(!wav.open(badwav.string(), error), "truncated WAV rejected");
        std::cout << "PASS " << checks << " checks; KET=1998 PET=1871; streamed " << count
                  << " WAV files, " << samples << " samples\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
