// Included inside App::dispatch; also exercised by the host flow harness.
    case Action::Dictionary:
        view_.vocabulary_review_pending=false;
        if(screen==Screen::WebLearning||screen==Screen::WebTools) {
            dictionary_origin_return_=screen;
            view_.dictionary_native_lookup=screen==Screen::WebLearning && view_.web.key.rfind("vocabulary:",0)==0;
            if(view_.dictionary_native_lookup) {
                view_.dictionary_query=view_.web.typed;
                view_.dictionary_matches.clear();
                view_.dictionary_recent.clear();
            }
        } else if(screen!=Screen::Dictionary&&screen!=Screen::DictionaryEntry&&screen!=Screen::DictionaryResults&&screen!=Screen::Vocabulary) {
            dictionary_origin_return_=Screen::Home;
            view_.dictionary_native_lookup=false;
        }
        if (screen == Screen::Study) { learning_.pause(); save(); }
        leave_player(); view_.screen = Screen::Dictionary;
        break;
    case Action::DictionaryKey:
        if (screen == Screen::Dictionary && e.value >= 32 && e.value <= 126) {
            if (view_.dictionary_query.size() < 48) view_.dictionary_query += char(e.value);
            else notice_ = "最多输入 48 个英文字符";
        }
        break;
    case Action::DictionaryErase:
        if (screen == Screen::Dictionary && !view_.dictionary_query.empty()) view_.dictionary_query.pop_back();
        break;
    case Action::DictionaryClear:
        if (screen == Screen::Dictionary) view_.dictionary_query.clear();
        break;
    case Action::DictionaryShift:
        view_.dictionary_shift = !view_.dictionary_shift;
        break;
    case Action::DictionarySymbols:
        view_.dictionary_symbols = !view_.dictionary_symbols;
        break;
    case Action::DictionarySearch:
        if (screen == Screen::Dictionary) {
            std::string error;
            view_.dictionary_query = dictionary_query_text(view_.dictionary_query);
            if(view_.dictionary_query.empty()){notice_="请先输入要查询的单词";break;}
            std::vector<DictionaryMatch> matches;
            if (dictionary_.search(view_.dictionary_query,matches,error)) {
                const int exact = dictionary_exact_match(view_.dictionary_query,matches);
                if (exact >= 0) {
                    // Exact search opens only that entry; other matches stay hidden.
                    auto previous = std::move(view_.dictionary_matches);
                    view_.dictionary_matches = std::move(matches);
                    if (!open_dictionary_entry(size_t(exact),Screen::Dictionary)) {
                        view_.dictionary_matches = std::move(previous);
                        break;
                    }
                } else {
                    // dictionary_pages renders the first six rows, with no result
                    // pagination. Do not count any undisplayed matches as exposure.
                    bool exposed = true;
                    for (size_t i=0;i<matches.size() && i<6;++i)
                        if (!dictionary_expose(matches[i].word)) { exposed = false; break; }
                    if (!exposed) break;
                    view_.dictionary_matches = std::move(matches);
                    view_.screen = Screen::DictionaryResults;
                }
                auto &recent = view_.dictionary_recent;
                recent.erase(std::remove(recent.begin(),recent.end(),view_.dictionary_query),recent.end());
                recent.insert(recent.begin(),view_.dictionary_query);
                if (recent.size()>2) recent.resize(2);
            }
            else notice_ = error;
        }
        break;
    case Action::DictionarySelect:
        if (screen == Screen::DictionaryResults && e.value >= 0 && e.value < int(view_.dictionary_matches.size())) {
            open_dictionary_entry(size_t(e.value),Screen::DictionaryResults);
        }
        break;
    case Action::DictionaryPage:
        if (screen == Screen::DictionaryEntry)
            view_.answer_page = std::clamp(view_.answer_page+e.value,0,ui_.answer_pages()-1);
        break;
    case Action::DictionaryAdd:
        if (screen == Screen::DictionaryEntry && view_.writable && view_.personal_readable && !dictionary_card_.id.empty()) {
            audio_.stop(); pending_play_ = false;
            std::string error; bool already = false;
            mkdir((std::string(ROOT)+"/decks").c_str(),0775);
            if (add_personal_word(personal_path_,dictionary_card_,already,error)) {
                if (!already) {view_.vocabulary.page=0;view_.vocabulary_filter=VocabularyFilter::All;}
                if (!personal_words_.count(dictionary_card_.id)) personal_cards_.push_back(dictionary_card_);
                personal_words_.insert(dictionary_card_.id);
                auto it = std::find_if(books_.begin(),books_.end(),[](const BookSource &b){return b.filename=="custom-my-vocabulary.csv";});
                if (it == books_.end()) books_.push_back({"custom-my-vocabulary.csv","我的生词本","查词加入 · 按计划复习",nullptr,personal_words_.size()});
                else it->total = personal_words_.size();
                view_.books.clear();
                for (const auto &b : books_) view_.books.push_back({b.name,b.description,b.total});
                notice_ = already ? "这个词已在生词本中" : "已保存到生词本，可以开始学习";
                if (books_[deck_].filename == "custom-my-vocabulary.csv") {
                    personal_deck_dirty_ = true;
                    sync_personal_deck();
                }
            } else notice_ = "未能保存生词："+error;
        }
        break;
    case Action::Vocabulary: {
        view_.vocabulary_review_pending=false;
        if (screen == Screen::Study) { learning_.pause(); save(); }
        if(screen==Screen::DictionaryEntry && dictionary_return_!=Screen::Vocabulary) {
            vocabulary_return_=Screen::DictionaryEntry;
            vocabulary_origin_card_=dictionary_card_;vocabulary_origin_return_=dictionary_return_;
            vocabulary_origin_page_=view_.answer_page;
        } else if(screen!=Screen::Vocabulary && !(screen==Screen::DictionaryEntry && dictionary_return_==Screen::Vocabulary)) {
            vocabulary_return_=screen==Screen::Dictionary || screen==Screen::DictionaryResults ? screen : Screen::Home;
        }
        leave_player(); update_day(); view_.screen = Screen::Vocabulary;
        refresh_vocabulary();
        break;
    }
    case Action::VocabularyFilter:
        if (screen == Screen::Vocabulary && e.value>=0 && e.value<=2) {
            view_.vocabulary_filter = VocabularyFilter(e.value); view_.vocabulary.page=0;
        }
        break;
    case Action::VocabularyPage:
        if (screen == Screen::Vocabulary)
            view_.vocabulary.page = std::clamp(view_.vocabulary.page+e.value,0,int(view_.vocabulary.pages)-1);
        break;
    case Action::VocabularySelect:
        if (screen == Screen::Vocabulary && e.value>=0 && size_t(e.value)<personal_cards_.size()) {
            leave_player(); dictionary_card_=personal_cards_[e.value]; dictionary_return_=Screen::Vocabulary;
            view_.entry_from_vocabulary=true; view_.audio_status.clear(); view_.answer_page=0;
            view_.screen=Screen::DictionaryEntry;
        }
        break;
    case Action::DictionaryRecent:
        if (screen == Screen::Dictionary && e.value>=0 && e.value<int(view_.dictionary_recent.size())) {
            view_.dictionary_query=view_.dictionary_recent[e.value]; dispatch({Action::DictionarySearch});
        }
        break;
    case Action::SpeechStop:
        if (screen == Screen::DictionaryEntry) {
            audio_.stop(); pending_play_=false; view_.audio_status="已停止发音";
        }
        break;
    case Action::VocabularyReview: {
        if (screen != Screen::Vocabulary || !view_.personal_readable) break;
        if (learning_.state.lesson.active()) {
            if (learning_.state.lesson.book=="custom-my-vocabulary.csv") dispatch({Action::Resume});
            else { learning_.pause();save();view_.vocabulary_review_pending=true;view_.screen=Screen::Pause; }
            break;
        }
        auto it = std::find_if(books_.begin(),books_.end(),[](const BookSource &b){return b.filename=="custom-my-vocabulary.csv";});
        if (it == books_.end() || !it->total) break;
        leave_player(); update_day();
        if (deck_ != int(it-books_.begin())) {deck_=int(it-books_.begin());load_deck();}
        if (!sync_personal_deck()) break;
        group_.clear();
        std::set<std::string> selection;
        for (const auto &card:cards_)
            if (vocabulary_matches(card,progress_,day_,view_.vocabulary_filter)) selection.insert(card.id);
        if (learning_.begin(it->filename,"",day_,StudyMode::Visual,false,nullptr,false,&selection)) {
            view_.answer_page=0;view_.screen=Screen::Study;save();
            if (nvs_ok_) { nvs_set_str(nvs_,"book",books_[deck_].filename.c_str());nvs_commit(nvs_); }
        } else {
            view_.screen=Screen::Vocabulary;
            notice_=view_.vocabulary.fresh && !view_.personal_planned_new ? "今日新词已达上限，可调整学习计划" : "当前没有待学词，可以继续查词";
        }
        break;
    }
