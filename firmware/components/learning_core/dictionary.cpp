#include "dictionary.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace wordcard {
// Small, explicit editorial supplement. The SD ECDICT files remain untouched.
// IT/it meanings and IPA reuse the reviewed course entries. Technical definitions
// and pronunciation conventions are documented in docs/2026-10-27-词典与生词本.md.
static const DictionaryEntry supplement[] = {
    {"IT","aɪ tiː","n. 信息技术（information technology）","","",0,true},
    {"it","ɪt","pron. 它；（指天气、时间、距离等）","","",0,true},
    {"API","ˌeɪ piː ˈaɪ","n. 应用程序接口\n供软件之间调用功能的接口。","Application Programming Interface.","",0,true},
    {"USB","ˌjuː es ˈbiː","n. 通用串行总线；用于连接设备并传输数据、供电的接口标准。","Universal Serial Bus.","",0,true},
    {"C++","ˌsiː plʌs ˈplʌs","n. C++ 编程语言。","Pronounced C plus plus.","",0,true},
    {"C#","ˌsiː ˈʃɑːrp","n. C# 编程语言；常用于 .NET 平台。","Pronounced C sharp.","",0,true}
};
constexpr uint32_t supplement_base=0x80000000u;
constexpr size_t supplement_count=sizeof(supplement)/sizeof(supplement[0]);
struct CourseNote { const char *word,*ipa,*meaning; };
#include "dictionary_course_notes.inc"
static const CourseNote *course_note(const std::string &word) {
    for (const auto &note:course_notes) if (word==note.word) return &note;
    return nullptr;
}
static uint32_t little32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}
std::string dictionary_query_text(const std::string &word) {
    std::string key;
    bool space = false;
    for (unsigned char c : word) {
        if (c <= 32) { space = !key.empty(); continue; }
        if (space) key += ' ';
        space = false;
        key += char(c);
    }
    return key;
}
std::string dictionary_key(const std::string &word) {
    auto key = dictionary_query_text(word);
    for (char &c : key) if (c >= 'A' && c <= 'Z') c += 32;
    return key;
}
int dictionary_exact_match(const std::string &query, const std::vector<DictionaryMatch> &matches) {
    const auto text = dictionary_query_text(query), key = dictionary_key(query);
    int found = -1, count = 0;
    for (size_t i = 0; i < matches.size(); ++i) {
        if (matches[i].word == text) return int(i);
        if (dictionary_key(matches[i].word) == key) { found = int(i); ++count; }
    }
    return count == 1 ? found : -1; // Ambiguous acronym: let the learner choose.
}
Dictionary::~Dictionary() { close(); }
void Dictionary::close() {
    if (index_) std::fclose(index_);
    if (data_) std::fclose(data_);
    index_ = data_ = nullptr; count_ = data_size_ = 0;
}
bool Dictionary::open(const std::string &folder, std::string &error) {
    close(); error.clear();
    index_ = std::fopen((folder+"/index.bin").c_str(),"rb");
    data_ = std::fopen((folder+"/entries.bin").c_str(),"rb");
    uint8_t header[16];
    if (!index_ || !data_ || std::fread(header,1,16,index_) != 16 || std::memcmp(header,"WCDICT1\0",8)) {
        error = "词典未安装或索引损坏，请复制词典 SD 包"; close(); return false;
    }
    count_ = little32(header+8); data_size_ = little32(header+12);
    std::fseek(index_,0,SEEK_END); const auto index_size = std::ftell(index_);
    std::fseek(data_,0,SEEK_END); const auto data_size = std::ftell(data_);
    if (!count_ || count_ > 5000000 || data_size_ > 0x7fffffffu ||
        index_size != 16 + int64_t(count_)*8 || data_size != data_size_) {
        error = "词典文件不完整，请重新复制词典 SD 包"; close(); return false;
    }
    return true;
}
bool Dictionary::row(uint32_t index, std::vector<std::string> &fields, std::string &error) {
    if (!ready() || index >= count_) { error = "词典条目不可用"; return false; }
    uint8_t record[8];
    if (std::fseek(index_,long(16+uint64_t(index)*8),SEEK_SET) || std::fread(record,1,8,index_) != 8) {
        error = "词典索引读取失败"; return false;
    }
    uint32_t offset = little32(record), length = little32(record+4);
    if (!length || length > 65536 || uint64_t(offset)+length > data_size_) {
        error = "词典条目边界无效"; return false;
    }
    std::string raw(length,'\0');
    if (std::fseek(data_,long(offset),SEEK_SET) || std::fread(&raw[0],1,length,data_) != length) {
        error = "词典内容读取失败"; return false;
    }
    fields.assign(1,"");
    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (c == '\t') { fields.push_back(""); continue; }
        if (c == '\\' && i+1 < raw.size()) {
            c = raw[++i];
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
        }
        fields.back() += c;
    }
    if (fields.size() != 6 || fields[0].empty() || fields[1].empty()) {
        error = "词典条目格式无效"; return false;
    }
    return true;
}
bool Dictionary::read(uint32_t index, DictionaryEntry &entry, std::string &error) {
    error.clear();
    if (ready() && index>=supplement_base && index-supplement_base<supplement_count) {
        entry=supplement[index-supplement_base];entry.index=index;return true;
    }
    std::vector<std::string> fields;
    if (!row(index,fields,error)) return false;
    entry = {fields[1],fields[2],fields[3],fields[4],fields[5],index};
    if (const auto *note=course_note(entry.word)) {
        entry.phonetic=note->ipa;entry.course_note=true;
        entry.translation=std::string(note->meaning)+"\n其他词典释义：\n"+entry.translation;
    }
    return true;
}
bool Dictionary::search(const std::string &query, std::vector<DictionaryMatch> &matches, std::string &error) {
    matches.clear(); error.clear();
    const auto key = dictionary_key(query);
    if (!ready()) { error = "词典未安装，请复制词典 SD 包"; return false; }
    if (key.empty() || key.size() > 96) { error = "请输入英文单词或短语"; return false; }
    for (size_t i=0;i<supplement_count;++i)
        if (dictionary_key(supplement[i].word).compare(0,key.size(),key)==0)
            matches.push_back({supplement[i].word,supplement_base+uint32_t(i),supplement[i].translation});
    uint32_t lo = 0, hi = count_;
    std::vector<std::string> fields;
    while (lo < hi) {
        uint32_t mid = lo + (hi-lo)/2;
        if (!row(mid,fields,error)) return false;
        if (fields[0] < key) lo = mid + 1; else hi = mid;
    }
    for (uint32_t i = lo; i < count_ && matches.size() < 6; ++i) {
        if (!row(i,fields,error)) return false;
        if (fields[0].compare(0,key.size(),key) != 0) break;
        if (std::any_of(std::begin(supplement),std::end(supplement),[&](const auto &s){return dictionary_key(s.word)==fields[0];})) continue;
        auto meaning = fields[3].empty() ? fields[4] : fields[3];
        if (const auto *note=course_note(fields[1])) meaning=note->meaning;
        meaning = meaning.substr(0,meaning.find('\n'));
        matches.push_back({fields[1],i,meaning});
    }
    auto exact = std::find_if(matches.begin(),matches.end(),[&](const auto &m){return m.word == dictionary_query_text(query);});
    if (exact != matches.end()) std::rotate(matches.begin(),exact,exact+1);
    return true;
}
std::string personal_word_id(const std::string &word) {
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : word) { hash ^= c; hash *= 1099511628211ull; }
    std::ostringstream out; out << "personal:" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}
Card dictionary_card(const DictionaryEntry &entry) {
    Card c; c.id = personal_word_id(entry.word); c.front = entry.word; c.tags = "personal|word"; c.group = "随手查词";
    if (entry.supplemental) c.hint = "项目补充词条";
    if (entry.course_note) c.hint = "课程常用义 + ECDICT";
    c.back = entry.translation.empty() ? "暂无中文释义" : entry.translation;
    if (!entry.definition.empty()) c.back += "\n英文释义：\n" + entry.definition;
    if (!entry.exchange.empty()) {
        std::istringstream stream(entry.exchange); std::string part;
        const std::map<std::string,std::string> labels = {{"p","过去式"},{"d","过去分词"},
            {"i","现在分词"},{"3","第三人称单数"},{"s","复数"},{"r","比较级"},{"t","最高级"},{"0","原形"}};
        while (std::getline(stream,part,'/')) {
            const auto colon = part.find(':');
            if (colon == std::string::npos) continue;
            const auto label = labels.find(part.substr(0,colon));
            if (label != labels.end()) c.back += "\n" + label->second + "：" + part.substr(colon+1);
        }
    }
    if (!entry.phonetic.empty()) c.ipa = std::string(entry.supplemental || entry.course_note?"US /":"词典 /") + entry.phonetic + "/";
    return c;
}
bool add_personal_word(const std::string &path, const Card &card, bool &already, std::string &error) {
    already = false;
    if (!recover_backup(path,error)) return false;
    std::vector<Card> cards;
    if (exists(path)) {
        std::ifstream in(path);
        if (!load_deck(in,cards,error)) return false; // Never replace a damaged personal file.
    }
    for (const auto &c : cards) {
        if (c.id != card.id) continue;
        if (c.front != card.front) { error = "生词 ID 冲突，原文件已保留"; return false; }
        already = true; return true;
    }
    if (cards.size() >= 10000) { error = "生词本已达 10000 项，请先整理"; return false; }
    cards.push_back(card);
    std::ostringstream text; text << "id,front,back,hint,tags,group,ipa\n";
    for (const auto &c : cards)
        text << csv_escape(c.id) << ',' << csv_escape(c.front) << ',' << csv_escape(c.back) << ','
             << csv_escape(c.hint) << ',' << csv_escape(c.tags) << ',' << csv_escape(c.group) << ','
             << csv_escape(c.ipa) << '\n';
    return atomic_write(path,text.str(),error);
}
bool reload_personal_words(const std::string &path, std::vector<Card> &cards, std::string &error) {
    std::ifstream input(path);
    std::vector<Card> loaded;
    if (!input || !load_deck(input, loaded, error)) {
        if (error.empty()) error = "生词本读取失败";
        return false;
    }
    // A successful append must retain every prior card in order; never silently invalidate a queue.
    if (loaded.size() < cards.size()) { error = "生词本内容已变更，请重新加载资源"; return false; }
    for (size_t i = 0; i < cards.size(); ++i)
        if (loaded[i].id != cards[i].id || loaded[i].front != cards[i].front) {
            error = "生词本内容已变更，请重新加载资源"; return false;
        }
    cards.swap(loaded);
    return true;
}
bool vocabulary_matches(const Card &card, const ProgressMap &memory, uint32_t day, VocabularyFilter filter) {
    const auto it = memory.find(card.id);
    const bool fresh = it == memory.end() || !it->second.seen;
    return filter == VocabularyFilter::All || (filter == VocabularyFilter::New && fresh) ||
           (filter == VocabularyFilter::Due && !fresh && it->second.due_day <= day);
}
VocabularyPage vocabulary_page(const std::vector<Card> &cards, const ProgressMap &memory,
                               uint32_t day, VocabularyFilter filter, int page) {
    VocabularyPage result; result.total = cards.size();
    std::vector<size_t> selected;
    // Recently added first. Due view uses the oldest due date first.
    for (size_t i = cards.size(); i > 0; --i) {
        const auto &card = cards[i-1]; const auto it = memory.find(card.id);
        if (it == memory.end() || !it->second.seen) ++result.fresh;
        else if (it->second.due_day <= day) ++result.due;
        else if (!result.next_day || it->second.due_day < result.next_day) result.next_day = it->second.due_day;
        if (vocabulary_matches(card,memory,day,filter)) selected.push_back(i-1);
    }
    if (filter == VocabularyFilter::Due)
        std::stable_sort(selected.begin(),selected.end(),[&](size_t a,size_t b){return memory.at(cards[a].id).due_day < memory.at(cards[b].id).due_day;});
    result.matching = selected.size(); result.pages = std::max<size_t>(1,(selected.size()+3)/4);
    result.page = std::clamp(page,0,int(result.pages)-1);
    for (size_t p = size_t(result.page)*4; p < selected.size() && result.items.size()<4; ++p) {
        const auto i = selected[p]; const auto &card = cards[i]; const auto it = memory.find(card.id);
        std::string status = "新词";
        if (it != memory.end() && it->second.seen)
            status = it->second.due_day <= day ? "待复习" : std::to_string(it->second.due_day-day)+"天后复习";
        result.items.push_back({i,card.front,card.back.substr(0,card.back.find('\n')),status});
    }
    return result;
}
} // namespace wordcard
