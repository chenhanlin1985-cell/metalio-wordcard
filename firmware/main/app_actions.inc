// Included after App's declaration; hardware effects stay outside the portable learning core.
void App::date_editor() {
    time_t now = std::time(nullptr);
    tm t{};
    localtime_r(&now, &t);
    if (rtc_)
        view_.date_fields = {t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min};
    else
        view_.date_fields = {2026, 1, 1, 12, 0};
    view_.date_field = 0;
    view_.screen = Screen::Date;
}
void App::leave_player() {
    web_audio_pending_=false;web_audio_playing_=false;
    web_audio_note_.clear();
    if(web_.ready())web_.cancel_audio();
    guided_audio_pending_=false;
    recall_audio_pending_=false;
    if (view_.screen == Screen::Player && !playlist_.empty()) {
        learning_.state.player_book = books_[deck_].filename;
        learning_.state.player_group = group_;
        learning_.state.player_id = cards_[playlist_[player_pos_]].id;
        save();
    }
    audio_.stop();
    pending_play_ = false;
    pending_record_ = false;
    player_running_ = false;
    player_next_ = 0;
}
void App::start_player(bool repeat, bool sentences_only) {
    leave_player();
    refresh_speech();
    playlist_.clear();
    for (size_t i = 0; i < cards_.size(); ++i)
        if ((group_.empty() || cards_[i].group == group_) && available_audio_.count(cards_[i].id) &&
            (!sentences_only || is_sentence(cards_[i])))
            playlist_.push_back(i);
    player_pos_ = 0;
    if (learning_.state.player_book == books_[deck_].filename &&
        learning_.state.player_group == group_)
        for (size_t i = 0; i < playlist_.size(); ++i)
            if (cards_[playlist_[i]].id == learning_.state.player_id)
                player_pos_ = i;
    view_.screen = Screen::Player;
    player_sentences_ = sentences_only;
    view_.repeat_mode = repeat;
    view_.answer_page = 0;
    audio_.clear_recording();
    player_running_ = !repeat && !playlist_.empty();
    pending_play_ = !playlist_.empty();
}
void App::speak() {
    const Card *card = view_.screen == Screen::RecallLesson ? (guided_.review.current()?&guided_.review.current()->card:nullptr)
                      : view_.screen == Screen::GuidedLesson ? (guided_.current()?&guided_.current()->card:nullptr)
                      : view_.screen == Screen::GuidedListen ? &guided_courses()[guided_browse_].items[6+guided_sentence_].card
                      : view_.screen == Screen::DictionaryEntry ? &dictionary_card_
                      : view_.screen == Screen::Player
                           ? (playlist_.empty() ? nullptr : &cards_[playlist_[player_pos_]])
                           : learning_.current();
    if (!card || !audio_.ready()) {
        pending_play_ = false;
        notice_ = "没有可用发音，可继续看词学习";
        return;
    }
    if (audio_.busy()) {
        audio_.stop();
        pending_play_ = true;
        return;
    }
    auto path=sd_speech_path(*card);
    if(!validate_sd_speech(*card))path.clear();
    if(wordcard::speech_source(!path.empty(),audio_.tts_ready(),card->front)==SpeechSource::None) {
        pending_play_=false;notice_="请补齐 SD 音频或离线合成资源";return;
    }
    pending_play_ = !audio_.play(path,card->front);
    if(!pending_play_) {
        if(view_.screen==Screen::GuidedLesson){guided_audio_pending_=true;guided_.played(false);}
        if(view_.screen==Screen::RecallLesson){recall_audio_pending_=true;guided_.review.heard=false;}
        last_spoken_id_=card->id;
        view_.audio_status=path.empty()?"正在准备合成发音；可停止或返回":"正在播放 SD 发音";
        dirty_=true;
    }
    player_next_ = 0;
}
void App::playback_tick() {
    if (pending_record_ && !audio_.busy() && !dirty_) {
        pending_record_ = false;
        if (!audio_.record()) {
            notice_ = "录音未能开始，请重试";
            dirty_ = true;
        }
    }
    if (pending_play_ && !audio_.busy() && !dirty_) {
        pending_play_ = false;
        speak();
    }
    if (view_.screen == Screen::Player && player_running_ && !pending_play_ && !audio_.busy() &&
        player_next_ && uint64_t(esp_timer_get_time()) >= player_next_) {
        player_next_ = 0;
        if (player_pos_ + 1 < playlist_.size()) {
            ++player_pos_;
            view_.answer_page = 0;
            pending_play_ = true;
            dirty_ = true;
        } else {
            player_running_ = false;
            notice_ = "这一组听完了，可以休息一下";
            dirty_ = true;
            learning_.state.player_book = books_[deck_].filename;
            learning_.state.player_group = group_;
            learning_.state.player_id = cards_[playlist_[player_pos_]].id;
            save();
        }
    }
}
void App::dispatch(Event e) {
    if(usb_pending_)return;
    if (e.action == Action::None)
        return;
    notice_.clear();
    if (personal_deck_dirty_ && !sync_personal_deck() &&
        (e.action == Action::Start || e.action == Action::ContinueCourse)) {
        dirty_ = true;
        return;
    }
    if (learning_.state.haptics && e.action != Action::Vibe && !keyboard_dispatch_ && !touch_dispatch_acknowledged_)
        app_vibe_pulse();
    const auto screen = view_.screen;
    if(!web_dispatch(e) && !assessment_dispatch(e) && !recall_dispatch(e) && !guided_dispatch(e)) switch (e.action) {
    case Action::UsbDisk:
        leave_player();tts_test_.cancel();
        view_.screen=Screen::UsbDisk;view_.usb_phase=0;view_.usb_error.clear();
        break;
    case Action::UsbStart:
        leave_player();tts_test_.cancel();
        view_.screen=Screen::UsbDisk;view_.usb_phase=1;view_.usb_error.clear();
        usb_pending_=true;usb_wait_since_=esp_timer_get_time();
        break;
    case Action::UsbExit:
        view_.screen=Screen::Resources;
        break;
    case Action::Home:
        view_.legacy_home=false;
        view_.vocabulary_review_pending=false;
        if (screen == Screen::Study) {
            learning_.pause();
            save();
        }
        leave_player();
        view_.screen = Screen::Home;
        update_day();
        break;
    case Action::Back:
        if(dictionary_back())break;
        if(screen==Screen::UsbDisk)view_.screen=Screen::Resources;
        else if(screen==Screen::TtsTest)view_.screen=Screen::Settings;
        else if (screen == Screen::Study) {
            audio_.stop();
            pending_play_ = false;
            learning_.pause();
            save();
            view_.screen = Screen::Pause;
        } else if (screen == Screen::Player) {
            leave_player();
            view_.screen = Screen::Practice;
        } else if (screen == Screen::Plan)
            view_.screen = learning_.state.onboarded ? previous_ : Screen::Welcome;
        else if (screen == Screen::Date)
            view_.screen = learning_.state.onboarded ? Screen::Profile : Screen::Plan;
        else if (screen == Screen::SelfTest)
            view_.screen = Screen::Settings;
        else if (screen == Screen::Settings || screen == Screen::Stats ||
                 screen == Screen::Resources)
            view_.screen = Screen::Profile;
        else if (screen != Screen::Welcome)
            view_.screen = Screen::Home;
        break;
    case Action::Library:
        view_.legacy_home=true;
        view_.screen = Screen::Library;
        view_.page = deck_ / 4;
        break;
    case Action::Deck:
        view_.legacy_home=true;
        if (e.value < 0 || e.value >= int(books_.size()))
            break;
        if (e.value != deck_ && learning_.state.lesson.active()) {
            view_.screen = Screen::Pause;
            notice_ = "请先继续或结束当前小节，再换词书";
            break;
        }
        if (e.value != deck_) {
            deck_ = e.value;
            load_deck();
            if (nvs_ok_) {
                nvs_set_str(nvs_, "book", books_[deck_].filename.c_str());
                nvs_commit(nvs_);
            }
        }
        view_.screen = Screen::Home;
        break;
    case Action::Groups:
        view_.screen = Screen::Groups;
        view_.page = 0;
        break;
    case Action::Group:
        view_.legacy_home=true;
        if (learning_.state.lesson.active()) {
            view_.screen = Screen::Pause;
            notice_ = "结束当前小节后可以更换主题";
            break;
        }
        if (e.value >= 0 && e.value < int(view_.groups.size()))
            group_ = e.value ? view_.groups[e.value] : "";
        player_sentences_ = false;
        view_.screen = Screen::Home;
        break;
    case Action::Page:
        if (screen == Screen::Library)
            view_.page = std::clamp(view_.page + e.value, 0, int((books_.size() - 1) / 4));
        else if (screen == Screen::Groups)
            view_.page =
                std::clamp(view_.page + e.value, 0, std::max(0, int(view_.groups.size() - 1) / 6));
        break;
    case Action::Start:
    case Action::Difficult:
    case Action::Favorites: {
        if (learning_.state.lesson.active()) {
            learning_.pause();
            save();
            view_.screen = Screen::Pause;
            notice_ = "有一节尚未完成，可以继续或结束";
            break;
        }
        update_day();
        audio_.stop();
        const auto mode =
            e.action == Action::Start ? StudyMode(std::clamp(e.value, 0, 2)) : StudyMode::Visual;
        const bool difficult = e.action == Action::Difficult ||
                               (screen == Screen::Done && learning_.state.lesson.difficult);
        const bool favorite = e.action == Action::Favorites ||
                              (screen == Screen::Done && learning_.state.lesson.favorites);
        if (mode == StudyMode::Listening && (!audio_.ready() || !available_audio_.size())) {
            view_.screen = Screen::Practice;
            notice_ = "听音练习需要可用的发音资源";
            break;
        }
        if (!learning_.begin(books_[deck_].filename, group_, day_, mode, difficult,
                             &available_audio_, favorite)) {
            view_.screen = screen == Screen::Home ? Screen::Home : Screen::Practice;
            notice_ = "当前没有符合条件的词，可以换个练习";
            break;
        }
        view_.answer_page = 0;
        view_.screen = Screen::Study;
        save();
        pending_play_ = mode == StudyMode::Listening;
        break;
    }
    case Action::ContinueCourse:
        if (screen == Screen::Done && !group_.empty() &&
            learning_.begin_remaining(books_[deck_].filename,group_,day_)) {
            audio_.stop(); pending_play_ = false; view_.answer_page = 0;
            view_.screen = Screen::Study; save();
        }
        break;
    case Action::Resume: {
        view_.vocabulary_review_pending=false;
        auto &lesson = learning_.state.lesson;
        if (!lesson.active()) {
            view_.screen = Screen::Home;
            break;
        }
        auto it = std::find_if(books_.begin(), books_.end(),
                               [&](const auto &b) { return b.filename == lesson.book; });
        if (it == books_.end()) {
            view_.screen = Screen::Pause;
            notice_ = "该词书未安装，请恢复词书或结束这节";
            break;
        }
        if (deck_ != int(it - books_.begin())) {
            deck_ = int(it - books_.begin());
            load_deck();
        }
        if (!learning_.validate_lesson(books_[deck_].filename)) {
            view_.screen = Screen::Pause;
            notice_ = "词书内容已变更，请结束这节后重开";
            break;
        }
        if (lesson.mode == StudyMode::Listening) {
            refresh_speech();
            auto available = available_audio_;
            if (!audio_.ready() || !learning_.validate_lesson_audio(available)) {
                learning_.pause();
                view_.screen = Screen::Pause;
                notice_ = "发音不可用，请关机补齐资源后重启，或结束这节";
                break;
            }
        }
        group_ = lesson.group;
        learning_.resume();
        view_.screen = Screen::Study;
        view_.answer_page = 0;
        save();
        pending_play_ = lesson.mode == StudyMode::Listening && !lesson.revealed;
        break;
    }
    case Action::EndLesson:
        end_lesson();
        break;
    case Action::Reveal:
        if (screen == Screen::Study && learning_.reveal()) {
            view_.answer_page = 0;
            audio_.stop();
            pending_play_ = false;
            save();
        }
        break;
    case Action::Rate:
        if (screen != Screen::Study || e.value < 0 || e.value > 2)
            break;
        audio_.stop();
        pending_play_ = false;
        update_day();
        if (learning_.rate(Rating(e.value), day_, rtc_, learning_.state.lesson.position)) {
            save();
            view_.answer_page = 0;
            if (!learning_.state.lesson.active())
                view_.screen = Screen::Done;
            else
                pending_play_ = learning_.state.lesson.mode == StudyMode::Listening;
        }
        break;
    case Action::Undo:
        if ((screen == Screen::Study || screen == Screen::Done) && learning_.undo()) {
            audio_.stop();
            pending_play_ = false;
            save();
            view_.screen = Screen::Study;
            view_.answer_page = 0;
        }
        break;
    case Action::Favorite:
        if (screen == Screen::Study && learning_.current()) {
            const auto id = learning_.current()->id;
            auto &favorites = learning_.state.favorites;
            if (favorites.count(id))
                favorites.erase(id);
            else
                favorites.insert(id);
            save();
        }
        break;
    case Action::Speak:
        if (screen == Screen::DictionaryEntry) {
            // Paint a stoppable preparation state before starting the audio worker.
            // E-paper DMA is intentionally deferred during synthesis/playback.
            pending_play_=true;
            view_.audio_status=view_.audio_source==SpeechSource::Sd?"正在准备 SD 发音":"正在准备合成发音；可停止或返回";
        } else if (screen == Screen::Study || screen == Screen::Player)
            speak();
        break;
    case Action::Plan:
        previous_ = screen;
        view_.new_limit = learning_.state.new_limit;
        view_.lesson_size = learning_.state.lesson_size;
        view_.screen = Screen::Plan;
        break;
    case Action::NewLimit:
        if (screen == Screen::Plan)
            view_.new_limit = unsigned(std::clamp(int(view_.new_limit) + e.value, 0, 50));
        break;
    case Action::LessonSize:
        if (screen == Screen::Plan)
            view_.lesson_size = unsigned(std::clamp(int(view_.lesson_size) + e.value, 5, 12));
        break;
    case Action::SavePlan: {
        if (screen != Screen::Plan)
            break;
        learning_.state.new_limit = view_.new_limit;
        learning_.state.lesson_size = view_.lesson_size;
        const bool saved = save();
        if (!learning_.state.onboarded)
            date_editor();
        else {
            view_.screen = Screen::Home;
            notice_ = saved ? "计划已保存，从下一小节生效" : "计划已应用，仅本次有效，无法保存";
        }
        break;
    }
    case Action::Date:
        date_editor();
        break;
    case Action::DateField:
        if (screen == Screen::Date)
            view_.date_field = std::clamp(e.value, 0, 4);
        break;
    case Action::DateStep:
        if (screen == Screen::Date) {
            const int low[] = {2024, 1, 1, 0, 0}, high[] = {2099, 12, 31, 23, 59};
            const int i = view_.date_field;
            view_.date_fields[i] = std::clamp(view_.date_fields[i] + e.value, low[i], high[i]);
        }
        break;
    case Action::SaveDate: {
        if (screen != Screen::Date)
            break;
        auto f = view_.date_fields;
        if (!valid_date(f[0], f[1], f[2])) {
            notice_ = "这个日期不存在，请调整月份或日期";
            break;
        }
        tm t{};
        t.tm_year = f[0] - 1900;
        t.tm_mon = f[1] - 1;
        t.tm_mday = f[2];
        t.tm_hour = f[3];
        t.tm_min = f[4];
        if (Pcf8563::GetInstance().SetTime(t) && Pcf8563::GetInstance().ApplyRtcToSystem()) {
            rtc_ = true;
            update_day();
            if(menu_stack_.empty())learning_.state.onboarded = true;
            const bool saved = menu_stack_.empty()?save():true;
            view_.screen = Screen::Home;
            notice_ = saved ? "日期与时间已保存" : "日期已设置；学习记录仅本次有效";
        } else
            notice_ = "日期写入失败，请稍后重试";
        break;
    }
    case Action::SkipDate:
        if (screen == Screen::Date && !learning_.state.onboarded) {
            learning_.state.onboarded = true;
            save();
            view_.screen = Screen::Home;
            notice_ = rtc_ ? "沿用设备当前日期" : "日期未设，学习记录暂不显示日历日期";
        }
        break;
    case Action::Practice:
        leave_player();
        view_.screen = Screen::Practice;
        break;
    case Action::Profile:
        leave_player();
        view_.screen = Screen::Profile;
        break;
    case Action::Stats:
        update_day();
        view_.screen = Screen::Stats;
        break;
    case Action::Haptics:
        learning_.state.haptics = !learning_.state.haptics;
        save();
        break;
    case Action::Resources:
        view_.screen = Screen::Resources;
        break;
    case Action::Reload: {
        if (learning_.state.lesson.active()) {
            view_.screen = Screen::Pause;
            notice_ = "结束当前小节后可以重新载入词书";
            break;
        }
        if (!sd_) {
            notice_ = "未挂载 SD，请关机插卡后重启";
            break;
        }
        const std::string current = books_[deck_].filename;
        audio_.reload_tts();
        audio_.invalidate_cache();
        checked_audio_paths_.recheck_metadata();
        audio_index_.clear();
        std::ifstream in(std::string(ROOT) + "/audio_index.csv");
        std::string error;
        if (in && !load_audio_index(in, audio_index_, error))
            notice_ = "主发音索引读取失败";
        catalog();
        load_dictionary();
        deck_ = 0;
        for (size_t i = 0; i < books_.size(); ++i)
            if (books_[i].filename == current)
                deck_ = int(i);
        load_deck();
        view_.screen = Screen::Resources;
        if (notice_.empty())
            notice_ = "词书与发音资源已重新载入";
        break;
    }
    case Action::Player:
        start_player(false);
        break;
    case Action::ListenCourse:
        if (group_.empty()) { view_.screen = Screen::Groups; view_.page = 0; }
        else start_player(false,true);
        break;
    case Action::Repeat:
        start_player(true,player_sentences_);
        break;
    case Action::PlayerToggle:
        if (screen != Screen::Player || playlist_.empty())
            break;
        player_running_ = !player_running_;
        if (player_running_)
            pending_play_ = true;
        else {
            audio_.stop();
            pending_play_ = false;
            player_next_ = 0;
            learning_.state.player_book = books_[deck_].filename;
            learning_.state.player_group = group_;
            learning_.state.player_id = cards_[playlist_[player_pos_]].id;
            save();
        }
        break;
    case Action::PlayerStep:
        if (screen != Screen::Player || playlist_.empty() || audio_.recording())
            break;
        audio_.stop();
        audio_.clear_recording();
        player_pos_ = size_t(std::clamp(int(player_pos_) + e.value, 0, int(playlist_.size() - 1)));
        view_.answer_page = 0;
        pending_play_ = true;
        player_next_ = 0;
        break;
    case Action::Record:
        if (screen == Screen::Player && view_.repeat_mode) {
            pending_play_ = false;
            if (audio_.recording())
                audio_.finish_recording();
            else if (pending_record_)
                pending_record_ = false;
            else if (!audio_.busy())
                pending_record_ = true;
            else
                notice_ = "请等原音播放结束再开始跟读";
        }
        break;
    case Action::Replay:
        if (screen == Screen::Player && view_.repeat_mode && !audio_.busy())
            audio_.replay();
        break;
    case Action::Settings:
        view_.screen = Screen::Settings;
        break;
    case Action::TtsTest:
        if(screen==Screen::TtsTest && tts_test_.active())break;
        if(screen==Screen::Study){learning_.pause();save();}
        leave_player();view_.screen=Screen::TtsTest;
        break;
    case Action::TtsMode:
        if(screen==Screen::TtsTest)tts_test_.select_mode(e.value);
        break;
    case Action::TtsPhrase:
        if(screen==Screen::TtsTest)tts_test_.select_phrase(tts_test_.phrase+e.value);
        break;
    case Action::TtsRun:
        if(screen==Screen::TtsTest && audio_.ready())tts_test_.start(e.value==1);
        else notice_="音频驱动未就绪，请查看硬件自检";
        break;
    case Action::TtsListen:
        if(screen==Screen::TtsTest && audio_.ready())tts_test_.start(false,true);
        break;
    case Action::TtsStop:
        if(screen==Screen::TtsTest){tts_test_.cancel();audio_.stop();}
        break;
    case Action::SelfTest:
        audio_.stop();
        view_.screen = Screen::SelfTest;
        full_ = true;
        break;
    case Action::Light:
        Frontlight::GetInstance().ToggleCct();
        break;
    case Action::Brightness:
        Frontlight::GetInstance().SetBrightness(
            uint8_t(
                std::clamp(int(Frontlight::GetInstance().target_brightness()) + e.value, 0, 100)),
            true);
        if (Frontlight::GetInstance().cct() == FrontlightCct::kOff)
            Frontlight::GetInstance().SetCct(FrontlightCct::kWarm);
        break;
    case Action::Volume:
        audio_.volume(audio_.volume() + e.value);
        if (nvs_ok_) {
            nvs_set_u8(nvs_, "volume", uint8_t(audio_.volume()));
            nvs_commit(nvs_);
        }
        break;
    case Action::Vibe:
        app_vibe_pulse();
        notice_ = "短震指令已发出";
        break;
    case Action::Tone:
        audio_.tone();
        break;
    case Action::Mic:
        audio_.mic_test();
        break;
    case Action::Refresh:
        full_ = true;
        break;
#include "app_dictionary_actions.inc"
    case Action::AnswerPage:
        view_.answer_page = (view_.answer_page + 1) % ui_.answer_pages();
        break;
    default:
        break;
    }
    menu_finish(e,screen);
    // Every route (including serial start/lesson resume) tears down a departing
    // test. Do not maintain a partial whitelist of navigation actions.
    if(screen==Screen::TtsTest && tts_test_.leave_page(view_.screen==Screen::TtsTest))audio_.stop();
    if(screen==Screen::GuidedLesson && view_.screen!=Screen::GuidedLesson){leave_player();save();}
    if(keyboard_page() && dictionary_keyboard_edit(e.action)) {
        const auto now=uint64_t(esp_timer_get_time());
        if(!keyboard_dirty_since_)keyboard_dirty_since_=now;
        keyboard_redraw_at_=now;
    } else {
        keyboard_dirty_since_=keyboard_redraw_at_=0;
        if(view_.screen==Screen::UsbDisk||view_.screen==Screen::PowerOff)keyboard_poweroff();
        keyboard_deactivate();
    }
    dirty_ = true;
}
