// The agenda reconciles stable card IDs. It never merges homographs by lowercase text.
void App::recall_import() {
    auto &r=guided_.review;r.bind_clock(day_,rtc_);
    for(int c=0;c<guided_course_count;++c)for(int i=0;i<12;++i){
        const auto&item=guided_courses()[c].items[i];
        auto&record=r.register_card(item.card,item.meaning);
        if(!r.migrated && (guided_.state.completed[c]&(1u<<(i/3)))){
            // Completion proves prior exposure, not a fabricated score or date.
            if(!record.memory[0].attempts){record.memory[0].pending=true;record.memory[0].undated=true;}
            if(!(guided_.state.listened[c]&(1u<<(i/3)))&&!record.memory[1].attempts)record.memory[1].pending=true;
        }
    }
    auto import=[&](const std::vector<Card>&cards,bool personal){
        for(const auto&c:cards){
            if(personal)r.register_card(c,"",true);
            for(size_t k=0;k<3;++k){auto it=learning_.state.memory[k].find(c.id);
                if(it!=learning_.state.memory[k].end())r.import_legacy(c,it->second,StudyMode(k),personal);
            }
        }
    };
    if(!recall_catalog_ready_){
        // Text-only wordbook metadata is loaded on first opening Today, never WAVs/models.
        for(const auto&b:books_){
            std::vector<Card> cards;std::string error;bool loaded=false;
            if(sd_){std::ifstream in(std::string(ROOT)+"/decks/"+b.filename);if(in)loaded=wordcard::load_deck(in,cards,error);}
            if(!loaded&&b.builtin_data){std::istringstream in(b.builtin_data);loaded=wordcard::load_deck(in,cards,error);}
            if(loaded)import(cards,b.filename=="custom-my-vocabulary.csv");
        }
        recall_catalog_ready_=true;
    }
    import(cards_,books_[deck_].filename=="custom-my-vocabulary.csv");
    import(personal_cards_,true);
    r.migrated=true;
}
void App::recall_view() {
    auto&r=guided_.review;r.bind_clock(day_,rtc_);
    view_.recall_counts=r.counts();view_.recall=r.session;view_.recall_card=r.current();
    view_.recall_heard=r.heard;view_.recall_ready=r.ready();
    view_.recall_typing=r.typing();view_.recall_listening=r.listening();
}
bool App::recall_dispatch(Event e) {
    auto&r=guided_.review;r.bind_clock(day_,rtc_);
    const bool lesson=view_.screen==Screen::RecallLesson;
    if(recall_input()) {
        if(e.action==Action::DictionaryKey){if(r.session.typed.size()<80&&e.value>=32&&e.value<=126)r.session.typed+=char(e.value);return true;}
        if(e.action==Action::DictionaryErase){if(!r.session.typed.empty())r.session.typed.pop_back();return true;}
        if(e.action==Action::DictionaryShift){view_.dictionary_shift=!view_.dictionary_shift;return true;}
        if(e.action==Action::DictionarySymbols){view_.dictionary_symbols=!view_.dictionary_symbols;return true;}
        if(e.action==Action::DictionarySearch)e.action=Action::RecallCheck;
        if(e.action==Action::Vocabulary)e.action=r.listening()?Action::Speak:Action::RecallHint;
    }
    switch(e.action){
    case Action::Reload:recall_catalog_ready_=false;return false;
    case Action::RecallHome:
        leave_player();update_day();recall_import();save();view_.screen=Screen::RecallHome;break;
    case Action::RecallBegin:
        if(!guided_valid_){notice_="复习记录损坏，已保留文件，请先导出检查。";break;}
        if(e.value<0||e.value>8)break;
        leave_player();update_day();recall_import();
        if(r.current()){view_.screen=Screen::RecallLesson;notice_="先继续尚未完成的复习。";break;}
        {
            const auto d=learning_.state.days.find(day_);
            size_t used=d==learning_.state.days.end()?0:d->second.fresh;
            for(const auto&entry:r.records)if(entry.second.personal&&entry.second.first_answer&&
                entry.second.first_day==(rtc_?day_:0)&&!entry.second.legacy_seen[0])++used;
            const unsigned fresh=used<learning_.state.new_limit?unsigned(learning_.state.new_limit-used):0;
            if(r.begin(RecallFilter(e.value),learning_.state.lesson_size,fresh))view_.screen=Screen::RecallLesson;
            else notice_=e.value==0?"当前没有待复习内容，可继续课程或添加生词。":"当前没有这类练习；先学习几个词。";
            save();
        }break;
    case Action::RecallResume:
        if(r.current()){leave_player();view_.screen=Screen::RecallLesson;}break;
    case Action::RecallChoose:
        if(lesson&&!r.session.checked&&e.value>=0&&e.value<int(r.session.options.size()))r.session.selected=e.value;
        break;
    case Action::RecallHint:
        if(lesson&&!r.session.checked&&r.current()){
            r.session.hint=true;notice_="首字母："+r.current()->card.front.substr(0,1)+" · 本题使用了提示";save();
        }break;
    case Action::RecallCheck:
        if(lesson&&r.check()){leave_player();save();}break;
    case Action::RecallNext:
        if(lesson&&r.next()){
            leave_player();
            for(int c=0;c<guided_course_count;++c)for(int n=0;n<4;++n)if(guided_.state.completed[c]&(1u<<n)){
                bool complete=true;
                for(int i=n*3;i<n*3+3;++i){auto it=r.records.find(guided_courses()[c].items[i].card.id);
                    complete&=it!=r.records.end()&&it->second.memory[1].attempts&&!it->second.memory[1].pending;
                }
                if(complete)guided_.state.listened[c]|=1u<<n;
            }
            if(!r.current())view_.screen=Screen::RecallDone;
            save();
        }break;
    case Action::RecallSkip:
        if(lesson&&r.skip()){leave_player();if(!r.current())view_.screen=Screen::RecallDone;save();}break;
    case Action::Speak:
        if(!lesson)return false;
        pending_play_=true;notice_="正在准备发音，可返回暂停。";break;
    case Action::Back:
        if(lesson){leave_player();save();view_.screen=Screen::RecallHome;break;}
        if(view_.screen==Screen::RecallHome||view_.screen==Screen::RecallDone){leave_player();view_.screen=Screen::Home;break;}
        return false;
    case Action::Home:
        if(lesson){leave_player();save();}return false;
    case Action::GuidedAdd:
        if(view_.screen==Screen::GuidedLesson&&guided_.current()&&view_.writable&&view_.personal_readable){
            const auto&item=*guided_.current();bool already=false;std::string error;
            if(add_personal_word(personal_path_,item.card,already,error)){
                if(!personal_words_.count(item.card.id))personal_cards_.push_back(item.card);
                personal_words_.insert(item.card.id);r.register_card(item.card,item.meaning,true);
                auto book=std::find_if(books_.begin(),books_.end(),[](const BookSource&b){return b.filename=="custom-my-vocabulary.csv";});
                if(book==books_.end())books_.push_back({"custom-my-vocabulary.csv","我的生词本","课程与查词加入",nullptr,personal_words_.size()});
                else book->total=personal_words_.size();
                view_.books.clear();for(const auto&b:books_)view_.books.push_back({b.name,b.description,b.total});
                personal_deck_dirty_=true;save();notice_=already?"已在生词本中":"已加入生词本，也会进入今日复习";
            }else notice_="未能保存："+error;
        }break;
    default:return false;
    }
    return true;
}
