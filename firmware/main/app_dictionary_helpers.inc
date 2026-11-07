#include "../components/native_learning/include/native_json.hpp"

bool App::dictionary_expose(const std::string &word) {
    if (!view_.dictionary_native_lookup) return true;
    // Persist exposure before publishing an entry or a visible candidate.
    // Standalone dictionary browsing never touches native learning state.
    std::string status;
    if (!web_.ready() || !web_.command("v-lookup-exposure", word) || !web_.status(status)) {
        notice_ = "查词提示未能保存，请重试；原进度保留";
        return false;
    }
    const auto result = StudyJson::parse(status, nullptr, false);
    if (!result.is_object() || !field(result, "courseError").is_string() ||
        !str(result, "courseError").empty()) {
        notice_ = result.is_object() && !str(result, "courseError").empty()
            ? str(result, "courseError") : "查词状态未能确认，请重试；原进度保留";
        return false;
    }
    return true;
}

bool App::open_dictionary_entry(size_t match, Screen origin) {
    if (match >= view_.dictionary_matches.size()) return false;
    DictionaryEntry entry; std::string error;
    if (!dictionary_.read(view_.dictionary_matches[match].index,entry,error)) { notice_=error; return false; }
    if (!dictionary_expose(entry.word)) return false;
    leave_player();
    dictionary_card_ = dictionary_card(entry); dictionary_return_ = origin;
    view_.entry_from_vocabulary = false; view_.answer_page = 0;
    view_.audio_status.clear(); view_.screen = Screen::DictionaryEntry;
    return true;
}
void App::refresh_vocabulary() {
    const auto effective=view_.guided_enabled?guided_.review.display_progress(progress_,personal_cards_):progress_;
    view_.vocabulary = vocabulary_page(personal_cards_,effective,day_,view_.vocabulary_filter,view_.vocabulary.page);
    const auto daily = learning_.state.days.find(day_);
    size_t fresh = daily == learning_.state.days.end() ? 0 : daily->second.fresh;
    if(view_.guided_enabled)for(const auto&entry:guided_.review.records)
        if(entry.second.personal&&entry.second.first_answer&&entry.second.first_day==(rtc_?day_:0)&&!entry.second.legacy_seen[0])++fresh;
    const size_t remaining = fresh < learning_.state.new_limit ? learning_.state.new_limit-fresh : 0;
    view_.personal_planned_new = std::min(view_.vocabulary.fresh,remaining);
    view_.personal_resume = view_.guided_enabled?guided_.review.current()!=nullptr:
        learning_.state.lesson.active() && learning_.state.lesson.book == "custom-my-vocabulary.csv";
}

bool App::dictionary_native_dispatch(Event e) {
    const auto screen=view_.screen;
    if(!view_.dictionary_native_lookup ||
       (screen!=Screen::Dictionary && screen!=Screen::DictionaryResults &&
        screen!=Screen::DictionaryEntry && screen!=Screen::Vocabulary))return false;
    if(e.action==Action::Vocabulary) {
        leave_player();
        if(web_.ready() && web_.command("u-notebook")) {
            view_.screen=Screen::WebLearning;
            view_.dictionary_native_lookup=false;
        } else notice_="生词本暂未打开，可返回继续查词";
        return true;
    }
    if(e.action==Action::DictionaryAdd) {
        if(screen!=Screen::DictionaryEntry || dictionary_card_.id.empty())return true;
        if(!view_.writable) {notice_="SD 不可写，暂不能保存生词。";return true;}
        if(view_.dictionary_native_added_id==dictionary_card_.id)return true;
        try {
        const StudyJson word{{"id","dict:"+dictionary_card_.id},{"word",dictionary_card_.front},
                     {"ipa",dictionary_card_.ipa},{"meaning",dictionary_card_.back},
                     {"pos",""},{"example",""},{"exampleZh",""}};
        std::string status;
        const bool sent=web_.ready() && web_.command("v-import",word.dump());
        if(!sent || !web_.status(status)) {
            notice_="生词未能保存，请重试；原记录保留";
            return true;
        }
        const auto result=StudyJson::parse(status,nullptr,false);
        if(result.is_object() && str(result,"courseError").empty() &&
           str(result,"lastImportedWord")==dictionary_card_.front &&
           !str(result,"lastImportedId").empty()) {
            view_.dictionary_native_added_id=dictionary_card_.id;
            notice_="已加入生词本";
        } else notice_=result.is_object() && !str(result,"courseError").empty()
            ? str(result,"courseError") : "生词未能保存，请重试；原记录保留";
        } catch(const std::exception &) {
            notice_="生词未能保存，请重试；原记录保留";
        }
        return true;
    }
    // A lookup is an auxiliary view, never an entry into the old learning queue.
    if(e.action==Action::RecallHome || e.action==Action::RecallBegin ||
       e.action==Action::RecallResume || e.action==Action::VocabularyReview) {
        notice_="请返回背词页面，从复习入口继续";
        return true;
    }
    return false;
}

bool App::dictionary_back() {
    if(view_.screen==Screen::Dictionary&&(dictionary_origin_return_==Screen::WebLearning||dictionary_origin_return_==Screen::WebTools)){
        leave_player();view_.screen=dictionary_origin_return_;view_.dictionary_native_lookup=false;return true;
    }
    if(view_.screen==Screen::DictionaryEntry) {
        audio_.stop();pending_play_=false;view_.screen=dictionary_return_;return true;
    }
    if(view_.screen==Screen::DictionaryResults) {view_.screen=Screen::Dictionary;return true;}
    if(view_.screen==Screen::Pause && view_.vocabulary_review_pending) {
        view_.vocabulary_review_pending=false;view_.screen=Screen::Vocabulary;return true;
    }
    if(view_.screen!=Screen::Vocabulary)return false;
    leave_player();
    if(vocabulary_return_==Screen::DictionaryEntry) {
        dictionary_card_=vocabulary_origin_card_;dictionary_return_=vocabulary_origin_return_;
        view_.answer_page=vocabulary_origin_page_;view_.entry_from_vocabulary=false;
        view_.audio_status.clear();
    }
    view_.screen=vocabulary_return_;return true;
}

void App::end_lesson() {
    if(view_.screen!=Screen::Pause)return;
    const bool vocabulary=view_.vocabulary_review_pending;
    view_.vocabulary_review_pending=false;
    audio_.stop();pending_play_=false;learning_.end();save();
    view_.screen=Screen::Home;
    if(vocabulary) {
        // The user explicitly chose to finish the old section and study words.
        // Re-enter through the same quota, SD and queue checks as normal review.
        dispatch({Action::Vocabulary});dispatch({Action::VocabularyReview});
    }
}
