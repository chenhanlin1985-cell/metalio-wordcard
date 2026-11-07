// Independent test persistence. Never writes learning, dictionary or guided progress.
void App::assessment_load() {
    assessment_storage_ok_ = sd_ && view_.writable;
    if (sd_ && (exists(assessment_path_) || exists(assessment_path_ + ".bak"))) {
        std::string error;
        assessment_valid_ = assessment_.load(assessment_path_, error);
        if (!assessment_valid_) {
            assessment_storage_ok_ = false;
            ESP_LOGE(TAG, "Assessment: %s", error.c_str());
        } else if (assessment_.state.paused)
            assessment_write();
    }
    assessment_interest_ = assessment_.state.attempt ? assessment_.state.interest
                                                     : (guided_.profile.interest == 1 ? 1 : 0);
}
bool App::assessment_write() {
    if (!assessment_valid_ || !sd_) {
        assessment_storage_ok_ = false;
        notice_ = "测试无法保存；请检查 SD 卡。";
        return false;
    }
    std::string error;
    assessment_storage_ok_ = assessment_.save(assessment_path_, error);
    if (!assessment_storage_ok_) {
        notice_ = "测试未保存，请重试保存后继续。";
        ESP_LOGE(TAG, "Assessment save: %s", error.c_str());
    }
    return assessment_storage_ok_;
}
void App::assessment_view() {
    auto &a = view_.assessment;
    a.state = assessment_.state;
    a.item = assessment_.item();
    a.storage_ok = assessment_valid_ && assessment_storage_ok_ && sd_;
    a.record_valid = assessment_valid_;
    a.page = assessment_page_;
    a.report_skill = assessment_skill_;
    a.busy = audio_.busy();
    a.can_submit = a.storage_ok && assessment_.can_submit();
    a.can_play = a.storage_ok && audio_.ready() && !audio_.busy() && !assessment_.state.inflight &&
                 (assessment_.state.stage == AssessmentStage::Sound ||
                  (assessment_.state.stage == AssessmentStage::Listening &&
                   assessment_.state.responses[assessment_.state.position].plays < 2));
    for (int s = 0; s < 2; ++s)
        a.reports[s] = assessment_.report(s, assessment_interest_);
    // Interest is an exploration filter; changing it never rewrites a learning profile.
    a.state.interest = assessment_interest_;
    if (!assessment_valid_ && view_.screen >= Screen::AssessmentHome)
        notice_ = "测试记录损坏，请先导出检查。原文件保留。";
}
void App::assessment_screen() {
    assessment_page_ = 0;
    view_.screen = assessment_.state.paused                            ? Screen::AssessmentPause
                   : assessment_.state.stage == AssessmentStage::Done  ? Screen::AssessmentReport
                   : assessment_.state.stage == AssessmentStage::Sound ? Screen::AssessmentSound
                                                                       : Screen::AssessmentQuestion;
}
bool App::assessment_result(int kind, const std::string &path) {
    // The existing worker sends a path for errors, but empty paths for success/cancel.
    // There is one worker; old results are drained before a new request while it is idle.
    if (!assessment_audio_pending_ || (!path.empty() && path != assessment_audio_path_) ||
        kind < 1 || kind > 3)
        return false;
    assessment_audio_pending_ = false;
    assessment_.finish_audio(kind == 1, kind == 3);
    const bool saved = assessment_write();
    if (saved)
        notice_ = kind == 1   ? "播放完成。"
                  : kind == 3 ? "播放已中断，不占完整播放次数。"
                              : "播放失败，不计错；可重试或暂不测听力。";
    dirty_ = true;
    return true;
}
bool App::assessment_dispatch(Event e) {
    const auto screen = view_.screen;
    const bool active_screen =
        screen == Screen::AssessmentQuestion || screen == Screen::AssessmentSound;
    auto stop = [&]() {
        audio_.stop();
        assessment_audio_pending_ = false;
        assessment_.finish_audio(false, true);
    };
    auto commit = [&](const Assessment &before) {
        if (assessment_write())
            return true;
        assessment_ = before;
        return false;
    };
    switch (e.action) {
    case Action::AssessmentHome:
        if (active_screen)
            return assessment_dispatch({Action::AssessmentPause});
        if (screen == Screen::GuidedLesson || screen == Screen::RecallLesson ||
            screen == Screen::Study)
            return true;
        leave_player();
        view_.screen = Screen::AssessmentHome;
        break;
    case Action::AssessmentBegin:
        if (screen != Screen::AssessmentHome || !assessment_valid_ || !assessment_storage_ok_)
            break;
        if (assessment_.state.stage == AssessmentStage::Done) {
            view_.screen = Screen::AssessmentRestart;
            break;
        }
        if (assessment_.state.stage != AssessmentStage::Ready) {
            const auto before = assessment_;
            assessment_.resume();
            if (commit(before))
                assessment_screen();
            break;
        }
        {
            const auto before = assessment_;
            if (assessment_.begin(assessment_interest_) && commit(before))
                assessment_screen();
        }
        break;
    case Action::AssessmentRestart:
        if (screen == Screen::AssessmentRestart && assessment_valid_ && assessment_storage_ok_) {
            std::string error;
            // Completed attempts remain available for later export. Never overwrite an earlier
            // attempt.
            if (!assessment_.save(assessment_path_ + ".attempt-" +
                                      std::to_string(assessment_.state.attempt),
                                  error)) {
                notice_ = "旧报告备份失败，尚未开始新测试。";
                break;
            }
            const auto before = assessment_;
            if (assessment_.begin(assessment_interest_) && commit(before))
                assessment_screen();
        }
        break;
    case Action::AssessmentChoose:
        if (screen == Screen::AssessmentQuestion && assessment_storage_ok_)
            assessment_.choose(e.value);
        break;
    case Action::AssessmentSubmit:
    case Action::AssessmentUnknown:
        if (screen == Screen::AssessmentQuestion && assessment_storage_ok_) {
            const auto before = assessment_;
            if (assessment_.submit(e.action == Action::AssessmentUnknown) && commit(before))
                assessment_screen();
        }
        break;
    case Action::AssessmentPage:
        if (screen == Screen::AssessmentQuestion &&
            assessment_.state.stage == AssessmentStage::Reading && (e.value == -1 || e.value == 1))
            assessment_page_ = std::clamp(assessment_page_ + e.value, 0, ui_.answer_pages() - 1);
        break;
    case Action::AssessmentPlay:
        if (active_screen && assessment_storage_ok_ && audio_.ready() && !audio_.busy()) {
            std::string stale_text, stale_path;
            int stale_kind;
            while (audio_.result(stale_text, stale_kind, stale_path)) {
            } // Worker reports before releasing busy.
            const auto &a = assessment_.state.stage == AssessmentStage::Sound
                                ? assessment_soundcheck()
                                : assessment_.item()->audio;
            const auto path = std::string(ROOT) + "/assessment/pilot-1/" + a.file;
            // Validate this immutable recording on demand. USB exits reboot the app, clearing this
            // cache.
            if (!assessment_verified_.count(path) && !assessment_audio_valid(path, a)) {
                if (assessment_.begin_audio())
                    assessment_.finish_audio(false);
                if (assessment_write())
                    notice_ = "测试录音缺失或损坏；请补资源或暂不测听力。";
                break;
            }
            assessment_verified_.insert(path);
            const auto before = assessment_;
            if (!assessment_.begin_audio() || !commit(before))
                break;
            assessment_audio_path_ = path;
            assessment_audio_pending_ = true;
            // Empty synthesis text is intentional: assessment MUST NOT fall back to another voice.
            if (!audio_.play(path, "")) {
                assessment_audio_pending_ = false;
                assessment_.finish_audio(false);
                assessment_write();
                notice_ = "暂时无法播放，请重试。";
            } else
                notice_ = "正在播放固定录音。";
        }
        break;
    case Action::AssessmentSoundConfirm:
        if (screen == Screen::AssessmentSound && assessment_storage_ok_) {
            const auto before = assessment_;
            if (assessment_.sound_confirm() && commit(before))
                assessment_screen();
        }
        break;
    case Action::AssessmentPause:
        if (active_screen) {
            stop();
            assessment_.pause();
            assessment_write();
            view_.screen = Screen::AssessmentPause;
        }
        break;
    case Action::AssessmentResume:
        if (screen == Screen::AssessmentPause && assessment_storage_ok_) {
            const auto before = assessment_;
            assessment_.resume();
            if (commit(before))
                assessment_screen();
        }
        break;
    case Action::AssessmentSave:
        if (screen >= Screen::AssessmentHome && assessment_write())
            notice_ = "测试记录已保存，可以继续。";
        break;
    case Action::AssessmentSkip:
        if ((screen == Screen::AssessmentSound || screen == Screen::AssessmentQuestion) &&
            assessment_.state.position >= 12) {
            stop();
            assessment_write();
            view_.screen = Screen::AssessmentSkip;
        }
        break;
    case Action::AssessmentSkipConfirm:
        if (screen == Screen::AssessmentSkip && assessment_storage_ok_) {
            const auto before = assessment_;
            if (assessment_.omit_listening() && commit(before))
                assessment_screen();
        }
        break;
    case Action::AssessmentReport:
        if (screen == Screen::AssessmentHome && assessment_.state.stage == AssessmentStage::Done)
            view_.screen = Screen::AssessmentReport;
        break;
    case Action::AssessmentSkill:
        if (screen == Screen::AssessmentReport && (e.value == 0 || e.value == 1))
            assessment_skill_ = e.value;
        break;
    case Action::AssessmentInterest:
        if ((screen == Screen::AssessmentHome || screen == Screen::AssessmentReport) &&
            (e.value == 0 || e.value == 1))
            assessment_interest_ = e.value;
        break;
    case Action::AssessmentRecommend:
        if (screen == Screen::AssessmentReport) {
            const auto r = assessment_.report(assessment_skill_, assessment_interest_);
            for (int i = 0; i < guided_course_count; ++i)
                if (guided_courses()[i].id == r.course_id) {
                    guided_dispatch({Action::GuidedCourse, i});
                    notice_ = "建议先试学；原来未完成的小节仍可继续。";
                    break;
                }
        }
        break;
    case Action::Back:
        if (active_screen)
            return assessment_dispatch({Action::AssessmentPause});
        if (screen == Screen::AssessmentSkip) {
            assessment_screen();
            break;
        }
        if (screen == Screen::AssessmentRestart || screen == Screen::AssessmentReport) {
            view_.screen = Screen::AssessmentHome;
            break;
        }
        if (screen == Screen::AssessmentHome || screen == Screen::AssessmentPause) {
            view_.screen = Screen::Home;
            break;
        }
        return false;
    default:
        // Cover-key navigation and injected global actions must not expose lessons or answers
        // mid-test.
        if (active_screen || screen == Screen::AssessmentSkip) {
            if (e.action == Action::Volume || e.action == Action::Brightness ||
                e.action == Action::Light || e.action == Action::Refresh)
                return false;
            stop();
            assessment_.pause();
            assessment_write();
            view_.screen = Screen::AssessmentPause;
            return true;
        }
        return false;
    }
    return true;
}
