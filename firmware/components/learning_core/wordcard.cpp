#include "wordcard.hpp"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace wordcard {
StudyCounts study_counts(const std::vector<Card> &cards, const ProgressMap &progress, uint32_t day,
                         const std::string &group) {
    StudyCounts counts;
    for (const auto &card : cards) {
        if (!group.empty() && card.group != group)
            continue;
        ++counts.total;
        const auto found = progress.find(card.id);
        if (found == progress.end() || found->second.seen == 0)
            ++counts.unseen;
        else {
            ++counts.learned;
            if (found->second.due_day <= day)
                ++counts.due;
        }
    }
    return counts;
}
static std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    return first == std::string::npos
               ? ""
               : value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
bool read_csv(std::istream &input, const std::function<bool(const Row &)> &consume,
              std::string &error) {
    Row row;
    std::string field;
    bool quoted = false, closed_quote = false, at_start = true;
    size_t bytes = 0;
    auto emit = [&]() {
        row.push_back(trim(field));
        field.clear();
        closed_quote = false;
        bool ok = true;
        if (!(row.size() == 1 && (row[0].empty() || row[0][0] == '#')))
            ok = consume(row);
        row.clear();
        return ok;
    };
    char ch;
    while (input.get(ch)) {
        if (++bytes > 16 * 1024 * 1024) {
            error = "CSV exceeds 16 MiB";
            return false;
        }
        if (at_start) {
            at_start = false;
            if (static_cast<unsigned char>(ch) == 0xEF) {
                char b = 0, c = 0;
                input.get(b);
                input.get(c);
                if (static_cast<unsigned char>(b) != 0xBB ||
                    static_cast<unsigned char>(c) != 0xBF) {
                    error = "Invalid UTF-8 BOM";
                    return false;
                }
                continue;
            }
        }
        if (ch == '\0') {
            error = "NUL in CSV";
            return false;
        }
        if (quoted) {
            if (ch == '"') {
                if (input.peek() == '"') {
                    input.get(ch);
                    field += '"';
                } else {
                    quoted = false;
                    closed_quote = true;
                }
            } else
                field += ch;
        } else if (ch == '"') {
            if (!trim(field).empty() || closed_quote) {
                error = "Unexpected CSV quote";
                return false;
            }
            field.clear();
            quoted = true;
        } else if (ch == ',') {
            row.push_back(trim(field));
            field.clear();
            closed_quote = false;
            if (row.size() > 32) {
                error = "Too many CSV columns";
                return false;
            }
        } else if (ch == '\n' || ch == '\r') {
            if (ch == '\r' && input.peek() == '\n')
                input.get(ch);
            if (!emit())
                return false;
        } else {
            if (closed_quote && ch != ' ' && ch != '\t') {
                error = "Text after CSV quote";
                return false;
            }
            if (!closed_quote)
                field += ch;
        }
        if (field.size() > 32768) {
            error = "CSV field too long";
            return false;
        }
    }
    if (quoted) {
        error = "Unclosed CSV quote";
        return false;
    }
    if (input.bad()) {
        error = "CSV read failed";
        return false;
    }
    return (field.empty() && row.empty() && !closed_quote) || emit();
}
std::string csv_escape(const std::string &text) {
    if (text.find_first_of(",\"\r\n") == std::string::npos)
        return text;
    std::string out = "\"";
    for (char c : text) {
        if (c == '"')
            out += '"';
        out += c;
    }
    return out + '"';
}
static std::string cell(const Row &row, const std::map<std::string, size_t> &cols,
                        const char *key) {
    const auto found = cols.find(key);
    return found == cols.end() || found->second >= row.size() ? "" : row[found->second];
}
std::string reverse_prompt(const std::string &meaning) {
    std::string out;
    bool english = false;
    for (unsigned char c : meaning) {
        const bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if (letter) {
            if (!english)
                out += "____";
            english = true;
        } else {
            out += char(c);
            english = false;
        }
    }
    return out;
}
bool load_deck(std::istream &input, std::vector<Card> &output, std::string &error) {
    std::vector<Card> next;
    std::set<std::string> ids;
    std::map<std::string, size_t> cols;
    bool header = false;
    const bool ok = read_csv(
        input,
        [&](const Row &row) {
            if (!header) {
                for (size_t i = 0; i < row.size(); ++i)
                    cols[row[i]] = i;
                if (!cols.count("front") || !cols.count("back")) {
                    error = "Deck requires front/back headers";
                    return false;
                }
                header = true;
                return true;
            }
            Card card{cell(row, cols, "id"),   cell(row, cols, "front"), cell(row, cols, "back"),
                      cell(row, cols, "hint"), cell(row, cols, "tags"),  cell(row, cols, "group"),
                      cell(row, cols, "ipa")};
            if (card.front.empty() || card.back.empty()) {
                error = "Card missing front/back";
                return false;
            }
            if (card.id.empty())
                card.id = card.front;
            if (!ids.insert(card.id).second) {
                error = "Duplicate card ID: " + card.id;
                return false;
            }
            if (card.group.empty()) {
                const auto separator = card.tags.find('|');
                card.group = trim(separator == std::string::npos ? card.tags
                                                                 : card.tags.substr(separator + 1));
            }
            if (next.size() >= 10000) {
                error = "Deck exceeds 10000 cards";
                return false;
            }
            next.push_back(std::move(card));
            return true;
        },
        error);
    if (!ok || next.empty()) {
        if (error.empty())
            error = "Deck is empty";
        return false;
    }
    output.swap(next);
    return true;
}
void review(Progress &p, Rating rating, uint32_t today) {
    if (p.seen < UINT32_MAX)
        ++p.seen;
    if (rating == Rating::Again) {
        if (p.again < UINT32_MAX)
            ++p.again;
        p.ease = static_cast<uint16_t>(std::max(130, int(p.ease) - 20));
        p.interval = 0;
        p.box = p.box > 1 ? p.box - 2 : 0;
    } else if (rating == Rating::Hard) {
        if (p.hard < UINT32_MAX)
            ++p.hard;
        p.ease = static_cast<uint16_t>(std::max(130, int(p.ease) - 15));
        p.interval = std::max<uint16_t>(1, p.interval);
        p.box = std::max<uint8_t>(1, p.box);
    } else {
        if (p.good < UINT32_MAX)
            ++p.good;
        if (p.interval == 0)
            p.interval = 1;
        else if (p.interval == 1)
            p.interval = 3;
        else
            p.interval = static_cast<uint16_t>(std::min<uint32_t>(
                365,
                std::max<uint32_t>(p.interval + 1, (uint32_t(p.interval) * p.ease + 50) / 100)));
        p.ease = std::min<uint16_t>(300, p.ease + 5);
        p.box = std::min<uint8_t>(7, p.box + 1);
    }
    p.due_day = today > UINT32_MAX - p.interval ? UINT32_MAX : today + p.interval;
}
bool exists(const std::string &path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0;
}
bool recover_backup(const std::string &path, std::string &error) {
    if (!exists(path) && exists(path + ".bak") &&
        std::rename((path + ".bak").c_str(), path.c_str()) != 0) {
        error = "Cannot recover backup: " + path;
        return false;
    }
    return true;
}
bool atomic_write(const std::string &path, const std::string &bytes, std::string &error) {
    if (!recover_backup(path, error))
        return false;
    const auto temp = path + ".tmp", backup = path + ".bak";
    if (exists(temp) && std::remove(temp.c_str()) != 0) {
        error = "Cannot remove temporary file";
        return false;
    }
    FILE *file = std::fopen(temp.c_str(), "wb");
    if (!file) {
        error = "Cannot create " + temp;
        return false;
    }
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    ok = std::fflush(file) == 0 && ok;
#ifdef _WIN32
    ok = _commit(_fileno(file)) == 0 && ok;
#else
    ok = fsync(fileno(file)) == 0 && ok;
#endif
    ok = std::fclose(file) == 0 && ok;
    if (!ok) {
        std::remove(temp.c_str());
        error = "Write/sync failed: " + path;
        return false;
    }
    const bool original = exists(path);
    if (exists(backup) && std::remove(backup.c_str()) != 0) {
        std::remove(temp.c_str());
        error = "Backup is not writable";
        return false;
    }
    if (original && std::rename(path.c_str(), backup.c_str()) != 0) {
        std::remove(temp.c_str());
        error = "Cannot back up " + path;
        return false;
    }
    if (std::rename(temp.c_str(), path.c_str()) != 0) {
        if (original)
            std::rename(backup.c_str(), path.c_str());
        error = "Cannot commit " + path;
        return false;
    }
    // Keep the preceding committed version for manual recovery after media damage.
    return true;
}
static uint32_t checksum(const std::string &text) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : text) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}
static bool number(const std::string &text, uint32_t &out, uint32_t maximum = UINT32_MAX) {
    if (text.empty())
        return false;
    uint64_t value = 0;
    for (unsigned char c : text) {
        if (c < '0' || c > '9')
            return false;
        value = value * 10 + c - '0';
        if (value > maximum)
            return false;
    }
    out = static_cast<uint32_t>(value);
    return true;
}
bool save_progress(const std::string &path, const ProgressMap &progress, std::string &error) {
    std::ostringstream body;
    body << "id,box,dueDay,seen,again,hard,good,ease,intervalDays\n";
    for (const auto &item : progress) {
        const auto &p = item.second;
        body << csv_escape(item.first) << ',' << unsigned(p.box) << ',' << p.due_day << ','
             << p.seen << ',' << p.again << ',' << p.hard << ',' << p.good << ',' << p.ease << ','
             << p.interval << '\n';
    }
    const auto payload = body.str();
    return atomic_write(path,
                        "#wordcard-progress-v1," + std::to_string(progress.size()) + ',' +
                            std::to_string(checksum(payload)) + '\n' + payload,
                        error);
}
bool load_progress(const std::string &path, ProgressMap &output, std::string &error) {
    if (!recover_backup(path, error))
        return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "Cannot read progress: " + path;
        return false;
    }
    std::string marker;
    std::getline(file, marker);
    std::ostringstream rest;
    rest << file.rdbuf();
    const auto payload = rest.str();
    // Marker starts with '#', so parse its numeric suffix separately.
    const auto first = marker.find(','),
               second = marker.find(',', first == std::string::npos ? first : first + 1);
    uint32_t count = 0, hash = 0;
    if (marker.substr(0, first) != "#wordcard-progress-v1" || second == std::string::npos ||
        !number(marker.substr(first + 1, second - first - 1), count) ||
        !number(marker.substr(second + 1), hash) || checksum(payload) != hash) {
        error = "Progress checksum/version mismatch";
        return false;
    }
    ProgressMap next;
    bool header = false;
    std::istringstream input(payload);
    const bool ok = read_csv(
        input,
        [&](const Row &row) {
            if (!header) {
                header = true;
                return row == Row{"id",   "box",  "dueDay", "seen",        "again",
                                  "hard", "good", "ease",   "intervalDays"};
            }
            if (row.size() != 9 || row[0].empty() || next.count(row[0])) {
                error = "Invalid progress row";
                return false;
            }
            uint32_t n[8]{};
            const uint32_t maximum[] = {7,          UINT32_MAX, UINT32_MAX, UINT32_MAX,
                                        UINT32_MAX, UINT32_MAX, 300,        365};
            for (size_t i = 0; i < 8; ++i)
                if (!number(row[i + 1], n[i], maximum[i])) {
                    error = "Invalid progress number";
                    return false;
                }
            if (n[6] < 130) {
                error = "Invalid ease";
                return false;
            }
            Progress p;
            p.box = uint8_t(n[0]);
            p.due_day = n[1];
            p.seen = n[2];
            p.again = n[3];
            p.hard = n[4];
            p.good = n[5];
            p.ease = uint16_t(n[6]);
            p.interval = uint16_t(n[7]);
            next.emplace(row[0], p);
            return true;
        },
        error);
    if (!ok || !header || next.size() != count) {
        if (error.empty())
            error = "Progress row count mismatch";
        return false;
    }
    output.swap(next);
    return true;
}
void Session::begin(const std::vector<Card> *cards, const ProgressMap &progress, uint32_t day,
                    const std::string &group, bool difficult) {
    cards_ = cards;
    queue_.clear();
    position_ = 0;
    revealed_ = false;
    if (!cards)
        return;
    for (size_t i = 0; i < cards->size(); ++i) {
        const auto &card = (*cards)[i];
        auto found = progress.find(card.id);
        const Progress p = found == progress.end() ? Progress{} : found->second;
        if (!group.empty() && card.group != group)
            continue;
        if (difficult && !(p.again > 0 || p.hard >= 2 || p.ease < 220))
            continue;
        if (p.seen == 0 || p.due_day <= day)
            queue_.push_back(i);
    }
    std::stable_sort(queue_.begin(), queue_.end(), [&](size_t a, size_t b) {
        auto pa = progress.find((*cards)[a].id), pb = progress.find((*cards)[b].id);
        const auto due_a = pa == progress.end() ? 0 : pa->second.due_day;
        const auto due_b = pb == progress.end() ? 0 : pb->second.due_day;
        return due_a < due_b;
    });
}
const Card *Session::current() const {
    return !cards_ || finished() ? nullptr : &(*cards_)[queue_[position_]];
}
bool Session::rate(Rating rating, ProgressMap &progress, uint32_t today) {
    const auto *card = current();
    if (!card || !revealed_)
        return false;
    review(progress[card->id], rating, today);
    // Again is requeued once for immediate retrieval practice, not indefinitely.
    if (rating == Rating::Again && std::count(queue_.begin(), queue_.end(), queue_[position_]) < 2)
        queue_.push_back(queue_[position_]);
    ++position_;
    revealed_ = false;
    return true;
}
bool safe_relative_path(const std::string &path) {
    if (path.empty() || path[0] == '/' ||
        path.find_first_of("\\:\r\n\0", 0, 5) != std::string::npos)
        return false;
    std::istringstream parts(path);
    std::string part;
    while (std::getline(parts, part, '/'))
        if (part.empty() || part == "." || part == "..")
            return false;
    return true;
}
bool load_audio_index(std::istream &input, std::map<std::string, AudioEntry> &output,
                      std::string &error) {
    std::map<std::string, AudioEntry> next;
    std::map<std::string, size_t> cols;
    bool header = false;
    const bool ok = read_csv(
        input,
        [&](const Row &row) {
            if (!header) {
                for (size_t i = 0; i < row.size(); ++i)
                    cols[row[i]] = i;
                header = true;
                if (!cols.count("card_id") || !cols.count("audio_path")) {
                    error = "Invalid audio index header";
                    return false;
                }
                return true;
            }
            const auto id = cell(row, cols, "card_id"), path = cell(row, cols, "audio_path");
            if (id.empty() || next.count(id)) {
                error = "Duplicate/empty audio ID";
                return false;
            }
            if (!path.empty() && (!safe_relative_path(path) || path.rfind("audio/", 0) != 0)) {
                error = "Unsafe audio path";
                return false;
            }
            next[id] = {path, cell(row, cols, "audio_source_text"),
                        cell(row, cols, "filename_review_needed") == "yes"};
            return true;
        },
        error);
    if (!ok || !header)
        return false;
    output.swap(next);
    return true;
}
static uint32_t little32(const unsigned char *p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
static uint16_t little16(const unsigned char *p) {
    return uint16_t(p[0]) | uint16_t(uint16_t(p[1]) << 8);
}
void WaveReader::close() {
    if (file_)
        std::fclose(file_);
    file_ = nullptr;
    samples_ = remaining_ = 0;
}
bool WaveReader::open(const std::string &path, std::string &error) {
    close();
    FILE *f = std::fopen(path.c_str(), "rb");
    auto fail = [&](const char *message) {
        error = message;
        if (f)
            std::fclose(f);
        return false;
    };
    if (!f)
        return fail("Audio file is missing");
    if (std::fseek(f, 0, SEEK_END) != 0)
        return fail("Cannot seek WAV");
    const long length = std::ftell(f);
    std::rewind(f);
    unsigned char header[12]{};
    if (length < 12 || std::fread(header, 1, 12, f) != 12 || std::memcmp(header, "RIFF", 4) ||
        std::memcmp(header + 8, "WAVE", 4))
        return fail("Not a RIFF/WAVE file");
    const uint64_t end = uint64_t(little32(header + 4)) + 8;
    if (end > uint64_t(length) || end < 12)
        return fail("Truncated RIFF payload");
    bool format = false;
    long offset = 0;
    uint32_t size = 0;
    while (uint64_t(std::ftell(f)) + 8 <= end) {
        unsigned char chunk[8]{};
        if (std::fread(chunk, 1, 8, f) != 8)
            return fail("Truncated WAV chunk");
        const uint32_t n = little32(chunk + 4);
        const long start = std::ftell(f);
        const uint64_t next = uint64_t(start) + n + (n & 1);
        if (next > end || next > LONG_MAX)
            return fail("Invalid WAV chunk length");
        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            unsigned char fmt[16]{};
            if (n < 16 || std::fread(fmt, 1, 16, f) != 16 || little16(fmt) != 1 ||
                little16(fmt + 2) != 1 || little32(fmt + 4) != 16000 ||
                little32(fmt + 8) != 32000 || little16(fmt + 12) != 2 || little16(fmt + 14) != 16)
                return fail("WAV must be PCM mono 16-bit 16 kHz");
            format = true;
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            if (offset)
                return fail("Multiple WAV data chunks");
            offset = start;
            size = n;
        }
        if (std::fseek(f, static_cast<long>(next), SEEK_SET) != 0)
            return fail("WAV seek failed");
    }
    if (!format || !offset || !size || size % 2 || std::fseek(f, offset, SEEK_SET) != 0)
        return fail("Missing or invalid WAV audio data");
    file_ = f;
    samples_ = remaining_ = size / 2;
    return true;
}
size_t WaveReader::read(int16_t *output, size_t max_samples) {
    if (!file_ || !output)
        return 0;
    const size_t count = std::min<size_t>(max_samples, remaining_);
    const size_t got = std::fread(output, sizeof(int16_t), count, file_);
    remaining_ -= static_cast<uint32_t>(got);
    return got;
}
} // namespace wordcard
