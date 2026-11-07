void App::guided_load() {
    view_.guided_enabled = true;
    std::string load_path=guided_path_;
    if(!exists(load_path)&&!exists(load_path+".bak")) {
        const auto version=load_path.find("guided-state-v3.txt");
        if(version!=std::string::npos)load_path.replace(version,19,"guided-state-v2.txt");
    }
    if(!exists(load_path)&&!exists(load_path+".bak")) {
        const auto version=load_path.find("guided-state-v2.txt");
        if(version!=std::string::npos)load_path.replace(version,19,"guided-state-v1.txt");
    }
    if (sd_ && (exists(load_path) || exists(load_path + ".bak"))) {
        std::string error;
        guided_valid_ = guided_.load(load_path, error);
        if (!guided_valid_) {
            notice_ = "课程进度无法读取，已保留原文件；请先导出检查。";
            ESP_LOGE(TAG, "Guided state: %s", error.c_str());
        }
    }
    guided_browse_ = guided_.state.course;
}
void App::guided_view() {
    const bool lesson = view_.screen == Screen::GuidedLesson ||
                        view_.screen == Screen::GuidedPause || view_.screen == Screen::GuidedDone;
    view_.guided = guided_.view(lesson ? -1 : (view_.screen==Screen::Home||view_.screen==Screen::Welcome)?guided_.recommended_course():guided_browse_);
    view_.guided_sentence = guided_sentence_;
    if (view_.screen == Screen::GuidedListen) {
        const auto &item = guided_courses()[guided_browse_].items[6 + guided_sentence_];
        view_.guided.text = item.card.front;
        view_.guided.meaning = item.meaning;
        view_.guided.ipa = item.card.ipa;
    }
    if (!guided_valid_)
        notice_ = "课程进度损坏，暂不能开始；原文件已保留。";
}
bool App::guided_dispatch(Event e) {
    const bool lesson = view_.screen == Screen::GuidedLesson;
    if(e.action==Action::Practice)view_.legacy_practice=false;
    switch (e.action) {
    case Action::LearningStart:
        leave_player();guided_.draft=guided_.profile;view_.screen=Screen::LearningStart;break;
    case Action::LearningInterest:
        if(view_.screen==Screen::LearningStart&&e.value>=0&&e.value<3)guided_.draft.interest=e.value;
        break;
    case Action::LearningLevel:
        if(view_.screen==Screen::LearningStart&&e.value>=0&&e.value<3)guided_.draft.start=e.value;
        break;
    case Action::LearningApply:
        if(view_.screen==Screen::LearningStart&&guided_valid_){
            guided_.profile=guided_.draft;guided_.profile.configured=true;guided_.profile.last_course=-1;
            guided_browse_=guided_.recommended_course();view_.screen=Screen::Home;
            if(save())notice_="已更新学习建议，已有进度保留。";
        }
        break;
    case Action::CurriculumContinue:
        if(guided_.state.active)return guided_dispatch({Action::GuidedResume,0});
        guided_browse_=guided_.recommended_course();
        return guided_dispatch({Action::GuidedBegin,guided_.next_node(guided_browse_)});
    case Action::CurriculumLevel:
        if(e.value>=-1&&e.value<3){guided_.catalog_level=e.value;guided_.catalog_page=0;}
        break;
    case Action::CurriculumPage:
        if(e.value==-1||e.value==1)guided_.catalog_page=std::max(0,std::min(guided_.catalog_page+e.value,std::max(0,(int(guided_.catalog().size())-1)/4)));
        break;
    case Action::LegacyPractice:
        leave_player();view_.legacy_practice=true;view_.screen=Screen::Practice;break;
    case Action::Speak:
        if(lesson || view_.screen==Screen::GuidedListen){
            pending_play_=true;notice_="正在准备发音，可暂停或返回。";break;
        }
        return false;
    case Action::GuidedListen:
        leave_player();
        if(view_.screen==Screen::Home||view_.screen==Screen::Welcome)guided_browse_=guided_.recommended_course();
        guided_sentence_ = 0;
        view_.screen = Screen::GuidedListen;
        view_.audio_status.clear();
        break;
    case Action::GuidedListenStep:
        if (view_.screen == Screen::GuidedListen && guided_sentence_ + e.value >= 0 &&
            guided_sentence_ + e.value < 6) {
            leave_player();
            guided_sentence_ += e.value;
            view_.audio_status.clear();
        }
        break;
    case Action::GuidedCourses:
        view_.legacy_home=false;
        leave_player();
        view_.screen = Screen::GuidedCourses;
        break;
    case Action::GuidedTrack:
        if (e.value >= -1 && e.value < 4){
            guided_.catalog_track=e.value;guided_.catalog_page=0;
        }
        break;
    case Action::GuidedCourse:
        view_.legacy_home=false;
        if (e.value >= 0 && e.value < guided_course_count) {
            guided_browse_ = e.value;
            view_.screen = Screen::CourseDetail;
        }
        break;
    case Action::GuidedBegin:
        if (!guided_valid_) {
            notice_ = "课程进度无法读取，请先导出 SD 资料检查。";
            break;
        }
        if (guided_.state.active) {
            leave_player();
            view_.screen = Screen::GuidedPause;
            notice_ = "还有一节未完成，可以先继续。";
            break;
        }
        if (guided_.begin(guided_browse_, e.value)) {
            leave_player();
            learning_.pause();
            learning_.state.onboarded = true;
            view_.screen = Screen::GuidedLesson;
            view_.audio_status.clear();
            save();
        }
        break;
    case Action::GuidedResume:
        view_.legacy_home=false;
        if (guided_.state.active) {
            leave_player();
            guided_browse_ = guided_.state.course;
            view_.screen = Screen::GuidedLesson;
        }
        break;
    case Action::GuidedPause:
        if (lesson) {
            leave_player();
            view_.screen = Screen::GuidedPause;
            save();
        }
        break;
    case Action::GuidedAbandon:
        if (view_.screen == Screen::GuidedPause) {
            const auto done = guided_.state.completed, listened = guided_.state.listened;
            guided_.state = {};
            guided_.state.completed = done;
            guided_.state.listened = listened;
            leave_player();
            save();
            view_.screen = Screen::GuidedCourses;
        }
        break;
    case Action::GuidedChoose:
        if (lesson)
            guided_.choose(e.value);
        break;
    case Action::GuidedRemove:
        if (lesson)
            guided_.remove(e.value);
        break;
    case Action::GuidedCheck:
        if (lesson && guided_.check()) {
            leave_player();
            save();
        }
        break;
    case Action::GuidedNext:
        if (lesson && guided_.advance()) {
            leave_player();
            view_.audio_status.clear();
            save();
            if (guided_.state.finished) {
                guided_browse_ = guided_.state.course;
                view_.screen = Screen::GuidedDone;
            }
        }
        break;
    case Action::GuidedSkip:
        if (lesson && guided_.skip_listening()) {
            leave_player();
            save();
            notice_ = "已改为看词练习；不会记为听力完成。";
        }
        break;
    case Action::Back:
        if(view_.screen==Screen::LearningStart){view_.screen=Screen::Home;break;}
        if(view_.screen==Screen::CourseDetail){leave_player();view_.screen=Screen::GuidedCourses;break;}
        if (lesson) {
            leave_player();
            save();
            view_.screen = Screen::GuidedPause;
            break;
        }
        if (view_.screen == Screen::GuidedCourses || view_.screen == Screen::GuidedPause ||
            view_.screen == Screen::GuidedDone || view_.screen == Screen::GuidedListen) {
            leave_player();
            view_.screen = Screen::Home;
            break;
        }
        return false;
    default:
        return false;
    }
    return true;
}
