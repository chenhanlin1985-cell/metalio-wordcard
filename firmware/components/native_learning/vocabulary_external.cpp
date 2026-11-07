#include "vocabulary.hpp"
#include <iomanip>
#include <fstream>
#include <set>
#include "dictionary.hpp"
#include <sstream>
namespace wordcard {
namespace {
std::string external_hash(const std::string &word) {
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : word) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}
std::string text(const VJ &value, const char *key, size_t limit, bool required,
                 bool multiline = false) {
    const auto &v = field(value, key);
    require(v.is_string() || (!required && v.is_null()), "词典字段格式无效");
    const auto result = str(v);
    require(result.size() <= limit, "词典词条过长，未加入生词本");
    require(!required || result.find_first_not_of(" \t\r\n") != std::string::npos,
            "词典单词或释义为空");
    for (unsigned char c : result)
        require(c >= 32 || (multiline && (c == '\n' || c == '\r' || c == '\t')),
                "词典内容含无效控制字符");
    require(result.find(char(127)) == std::string::npos, "词典内容含无效控制字符");
    // The JSON serializer rejects malformed UTF-8 without replacing text.
    (void)VJ(result).dump();
    return result;
}
std::string short_meaning(const std::string &definition) {
    std::string sense;
    size_t start = 0;
    while (start < definition.size()) {
        const auto ascii = definition.find_first_of("\r\n;", start);
        const auto chinese = definition.find("；", start);
        const auto end = std::min(ascii, chinese);
        const auto segment = definition.substr(start, end == std::string::npos ? end : end - start);
        const auto first = segment.find_first_not_of(" \t"), last = segment.find_last_not_of(" \t");
        if (first != std::string::npos) {
            sense = segment.substr(first, last - first + 1);
            break;
        }
        if (end == std::string::npos) break;
        start = end + (end == chinese ? std::string("；").size() : 1);
    }
    require(!sense.empty(), "词典没有可显示的释义");
    if (sense.size() <= 80) return sense;
    size_t end = 77; // Leave three UTF-8 bytes for the ellipsis.
    while (end && (static_cast<unsigned char>(sense[end]) & 0xc0) == 0x80) --end;
    auto word_character = [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '\'' || c == '-';
    };
    // Back up to a whole English token if the byte boundary falls inside it.
    if (end && word_character(sense[end]) && word_character(sense[end - 1]))
        while (end && word_character(sense[end - 1])) --end;
    while (end && (sense[end - 1] == ' ' || sense[end - 1] == '\t')) --end;
    return (end ? sense.substr(0, end) : "查看完整释义") + "…";
}
VJ external_word(const VJ &input) {
    require(input.is_object(), "词典词条格式无效");
    auto id = text(input, "id", 30, true);
    auto word = text(input, "word", 100, true);
    // Registration after reboot receives the canonical record, while a fresh
    // dictionary payload carries the unabridged definition in meaning.
    auto definition = text(input, input.contains("dictionaryDefinition")
                                      ? "dictionaryDefinition" : "meaning", 4000, true, true);
    auto meaning = short_meaning(definition);
    auto ipa = text(input, "ipa", 256, false);
    auto pos = text(input, "pos", 80, false);
    require(word.front() != ' ' && word.back() != ' ', "词典单词首尾含空格");
    const auto hash = external_hash(word);
    require(id == "dict:personal:" + hash, "词典词条ID无效或与单词不符");
    return VJ{{"id", id}, {"word", word}, {"meaning", meaning},
              {"dictionaryDefinition", definition}, {"ipa", ipa},
              {"pos", pos}, {"stage", "personal"}, {"lessonEligible", false},
              {"source", "full-offline-dictionary"}, {"humanVerified", false},
              {"example", ""}, {"exampleZh", ""},
              {"audio", VJ{{"kind", "generated"}, {"text", word},
                            {"path", "audio/external-" + hash + ".wav"}}}};
}
} // namespace
void VocabularyCatalog::install_external(const VJ &value) {
    const auto word = external_word(value);
    const auto id = str(word, "id");
    require(external.is_object(), "生词词典索引损坏");
    const auto *existing = entry(id);
    if (existing) {
        require(external.contains(id) && external.at(id) == word,
                "同一生词ID的词典内容已改变，原记录保留");
        return;
    }
    require(!external.contains(id), "生词词典索引不一致");
    Entry row{id, str(word, "word"), str(word, "meaning"), str(word, "pos"),
              "personal", false, 0, 0};
    // Imported definitions are memory records, never offsets into catalog.ndjson.
    const auto before = entries.size();
    try {
        entries.push_back(std::move(row));
        index_.emplace(id, before);
        external[id] = word;
    } catch (...) {
        external.erase(id);
        index_.erase(id);
        if (entries.size() > before) entries.pop_back();
        throw;
    }
}
void VocabularyCatalog::remove_external(const std::string &id) {
    if (!external.contains(id)) return;
    external.erase(id);
    cache_.erase(id);
    auto found = index_.find(id);
    if (found == index_.end()) return;
    const auto at = found->second;
    index_.erase(found);
    entries.erase(entries.begin() + at);
    for (size_t i = at; i < entries.size(); ++i) index_.at(entries[i].id) = i;
}
bool NativeVocabulary::import_word(const VJ &input) {
    lastImportedWord_.clear();
    lastImportedId_.clear();
    std::string installed;
    try {
        require(loaded_, "背词记录未加载；请检查SD卡");
        const auto word = external_word(input);
        const auto id = str(word, "id"), spelling = str(word, "word");
        const auto &saved = field(state_, "externalWords");
        require(saved.is_null() || saved.is_object(), "生词词典记录损坏");
        const bool hadSaved = saved.is_object() && saved.contains(id);
        if (hadSaved)
            require(saved.at(id) == word, "同一生词ID的词典内容已改变，原记录保留");
        // Match exact spelling only: IT and it are distinct dictionary words.
        std::string curated;
        for (const auto &entry : catalog_.entries)
            if (entry.word == spelling && !catalog_.external.contains(entry.id)) {
                curated = entry.id;
                break;
            }
        bool ok;
        if (!curated.empty() && !hadSaved) {
            ok = commit(VJ{{"type", "notebook"}, {"id", curated}});
        } else {
            const bool wasInstalled = catalog_.external.contains(id);
            catalog_.install_external(word);
            if (!wasInstalled) installed = id;
            ok = commit(VJ{{"type", "importNotebook"}, {"id", id}, {"word", word}});
        }
        if (!ok) {
            if (!installed.empty()) catalog_.remove_external(installed);
            return false;
        }
        installed.clear(); // The durable state now owns this catalog definition.
        lastImportedWord_ = spelling;
        lastImportedId_ = curated.empty() || hadSaved ? id : curated;
        note_ = "已加入生词本";
        return true;
    } catch (const std::exception &e) {
        if (!installed.empty()) catalog_.remove_external(installed);
        capture_error(e);
        return false;
    }
}

// Explicit user action only. No startup scan or background dictionary walk.
bool NativeVocabulary::prepare_word_list() {
    wordListError_.clear();wordList_=VJ::array();wordListIssues_=VJ::array();wordListExisting_=0;
    wordListReady_=false;++wordListToken_;++viewSerial_;
    try {
        require(loaded_, "背词记录未加载");
        const auto root=catalog_.directory.substr(0,catalog_.directory.find_last_of('/'));
        std::ifstream file(root+"/my-words.txt",std::ios::binary|std::ios::ate);
        require(bool(file), "没有找到 my-words.txt，请按本页说明保存到 SD 卡");
        const auto length=file.tellg();
        require(length>=0 && length<=32768,"单词表超过 32 KB，请分成每次不超过 200 行");
        file.seekg(0);std::string text(size_t(length),'\0');
        if(length>0)file.read(&text[0],length);
        require(bool(file),"单词表读取失败，请重新复制文件");
        if(text.rfind("\xef\xbb\xbf",0)==0)text.erase(0,3);
        Dictionary dictionary;bool opened=false;std::set<std::string> requested,ids;
        std::istringstream lines(text);std::string line;size_t number=0,count=0;
        while(std::getline(lines,line)) {
            ++number;if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.find_first_not_of(" \t")==std::string::npos)continue;
            require(++count<=200,"一次最多添加 200 行，请分成多个文件");
            auto issue=[&](const std::string &why){wordListIssues_.push_back(VJ{{"line",number},{"reason",why}});};
            bool letters=false,valid=line.size()<=200;
            for(unsigned char c:line) {
                const bool alpha=(c>='a'&&c<='z')||(c>='A'&&c<='Z');letters|=alpha;
                if(!alpha && !(c>='0'&&c<='9') && std::string(" '-.+#&/").find(char(c))==std::string::npos)valid=false;
            }
            if(!valid||!letters){issue("只保留英文单词或短语，每行一项");continue;}
            const auto word=dictionary_query_text(line);
            if(word.size()>96){issue("这一行过长，请检查是否混入了例句");continue;}
            if(!requested.insert(word).second)continue;
            auto find=[&](const std::string &spelling)->std::string {
                // Retain an already saved dictionary sense before curated aliases.
                for(const auto &id:field(state_,"notebook")) {
                    const auto *e=catalog_.entry(str(id));if(e&&e->word==spelling)return e->id;
                }
                for(const auto &e:catalog_.entries)if(e.word==spelling)return e.id;
                return {};
            };
            std::string id=find(word);VJ definition=nullptr;
            if(id.empty()) {
                std::string error;
                if(!opened){if(!dictionary.open(root+"/dictionary",error))throw std::runtime_error(error);opened=true;}
                std::vector<DictionaryMatch> matches;
                if(!dictionary.search(word,matches,error))throw std::runtime_error(error);
                const auto exact=dictionary_exact_match(word,matches);
                if(exact<0){issue(word+"：未找到唯一词条，请在词典中确认拼写");continue;}
                DictionaryEntry entry;if(!dictionary.read(matches[size_t(exact)].index,entry,error))throw std::runtime_error(error);
                if(entry.translation.empty()){issue(word+"：词典暂无中文释义，请在词典中确认");continue;}
                id=find(entry.word);
                if(id.empty()) {
                    const auto card=dictionary_card(entry);
                    try {
                        definition=external_word(VJ{{"id","dict:"+card.id},{"word",card.front},
                                                   {"meaning",card.back},{"ipa",card.ipa},{"pos",""}});
                        id=str(definition,"id");
                    } catch(const std::exception &) {issue(word+"：词典内容暂不能用于背词，请在词典中查看");continue;}
                }
            }
            if(!ids.insert(id).second)continue;
            if(has(state_["notebook"],id)){++wordListExisting_;continue;}
            wordList_.push_back(VJ{{"id",id},{"definition",definition}});
        }
        require(count>0,"单词表为空，请每行填入一个英文单词或短语");
        wordListReady_=true;note_.clear();++viewSerial_;return true;
    } catch(const std::exception &e) {
        wordList_=VJ::array();wordListIssues_=VJ::array();wordListExisting_=0;
        wordListError_=e.what();return false;
    }
}
bool NativeVocabulary::accept_word_list(uint64_t token) {
    if(!wordListReady_||token!=wordListToken_||wordList_.empty())return true;
    std::vector<std::string> installed;
    try {
        VJ ids=VJ::array(),definitions=VJ::object();
        for(const auto &item:wordList_) {
            const auto id=str(item,"id");const auto &definition=field(item,"definition");
            if(!definition.is_null()) {
                const bool had=catalog_.external.contains(id);
                catalog_.install_external(definition);
                if(!had)installed.push_back(id);
                definitions[id]=catalog_.external.at(id);
            }
            ids.push_back(id);
        }
        if(!commit(VJ{{"type","importWordList"},{"ids",ids},{"definitions",definitions}})) {
            for(const auto &id:installed)catalog_.remove_external(id);
            return false;
        }
        const auto count=wordList_.size();wordList_=VJ::array();wordListReady_=false;
        // Keep the notebook's original return frame, including a paused spelling draft.
        dispatch(VJ{{"action","v-return"},{"value","notebook"}}.dump());note_="已加入 "+std::to_string(count)+" 个词";return true;
    } catch(const std::exception &e) {
        for(const auto &id:installed)catalog_.remove_external(id);
        capture_error(e);return false;
    }
}

} // namespace wordcard
