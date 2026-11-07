void App::web_load(){
 if(!sd_){ESP_LOGW(TAG,"LEARNING_WEB no SD; legacy recovery retained");return;}
 std::string dir=std::string(ROOT)+"/learning-web";
 // Explicit SD-only commissioning profile. Never replace the user's files.
 std::ifstream diagnostic(std::string(ROOT)+"/.device-qa");std::string token;
 if(diagnostic&&std::getline(diagnostic,token)&&token=="daily-flow-20261101"){
  web_qa_=true;
  dir+="/device-test-20261101";
  ESP_LOGW(TAG,"LEARNING_WEB_DIAGNOSTIC isolated profile; personal learning files untouched");
 }
 if(mkdir(dir.c_str(),0775)&&errno!=EEXIST){notice_="无法创建学习进度目录";return;}
 const auto began=esp_timer_get_time();
 if(!web_.init(dir)){notice_="新版学习未能启动，原有学习仍可用";ESP_LOGE(TAG,"LEARNING_WEB %s",web_.error().c_str());return;}
 web_.prepare_renderer([this](const WebPage&p,const std::function<bool()>&cancel){if(!cancel())web_frame_.prepare(p);});
 web_.page(view_.web);view_.screen=Screen::WebLearning;
 ESP_LOGI(TAG,"BOOT_STAGE learning_ready elapsed_ms=%lld engine_bytes=%u",(long long)(esp_timer_get_time()-began)/1000,unsigned(web_.memory_bytes()));
}
void App::web_view(){
 if((view_.screen!=Screen::WebLearning&&view_.screen!=Screen::WebPrompt)||!web_.ready())return;
 const auto old=view_.web.key;
 if(!web_.page(view_.web)){view_.web.error="学习界面错误，请返回设备菜单";ESP_LOGE(TAG,"WEB_VIEW %s",web_.error().c_str());}
 view_.web.speech_note=web_audio_note_;
 if(old!=view_.web.key){web_positions_[old]=view_.web_page;view_.web_page=web_positions_[view_.web.key];web_audio_note_.clear();view_.web.speech_note.clear();}
 view_.dictionary_shift=view_.web.shift;view_.dictionary_symbols=view_.web.symbols;
 view_.dictionary_ready=view_.web.can_submit;
 // Two actual candidate/help rows share the stable keyboard touch capture path.
 view_.dictionary_recent.clear();for(const auto&b:view_.web.blocks)if(b.kind=="button"&&b.area!="foot"&&view_.dictionary_recent.size()<2)view_.dictionary_recent.push_back(b.text);
}
bool App::web_dispatch(Event e){
 if(menu_dispatch(e))return true;
 if(e.action==Action::StorageCheck && view_.screen==Screen::DeviceSettings) {
  if(!sd_ || !progress_valid_) {notice_="SD 或原记录尚不可用；保留文件后检查存储";return true;}
  std::string error;
  const auto probe=std::string(ROOT)+"/.write-test";
  if(!atomic_write(probe,"wordcard\n",error)) {notice_="SD 仍不能写入，请检查空间和连接";ESP_LOGE(TAG,"STORAGE_CHECK %s",error.c_str());return true;}
  std::remove(probe.c_str());view_.writable=true;
  if(!storage_warning_.empty() && !save())return true;
  if(web_.ready())web_.command("v-check-recovery");
  notice_="SD 写入检查通过；返回学习查看记录状态";return true;
 }
 if(dictionary_native_dispatch(e))return true;
 if(e.action==Action::WebLearning || (e.action==Action::Home&&web_.ready())){
  leave_player();if(e.action==Action::Home)web_.home();view_.screen=Screen::WebLearning;return true;
 }
 if(e.action==Action::DeviceMenu){leave_player();view_.screen=Screen::WebTools;return true;}
 if(e.action==Action::WebImport){if(e.value==0)view_.screen=Screen::WebImportConfirm;else{
  leave_player();if(web_.import_progress()){view_.screen=Screen::WebLearning;notice_="已导入新版进度，原文件保留备份";}else notice_=web_.error();
 }return true;}
 if(view_.screen==Screen::WebPrompt&&e.action==Action::Back){view_.screen=Screen::WebLearning;view_.web_page=web_prompt_return_page_;return true;}
 if((view_.screen!=Screen::WebLearning&&view_.screen!=Screen::WebPrompt)||!web_.ready())return false;
 if(view_.screen==Screen::WebPrompt&&e.action==Action::WebAction&&!web_prompt_action_allowed(view_.web,e.value))return true;
 bool ok=true;const auto command_began=esp_timer_get_time();
 switch(e.action){
 case Action::WebPage:if(e.value==0){web_prompt_return_page_=view_.web_page;view_.screen=Screen::WebPrompt;view_.web_page=0;}else view_.web_page=std::max(0,std::min(view_.web_page+e.value,ui_.answer_pages()-1));return true;
 case Action::WebAction:
  if(e.value>=0 && size_t(e.value)<view_.web.actions.size()) {
   const auto &action=view_.web.actions[size_t(e.value)];
   if(action=="{\"action\":\"v-device-date\"}") {menu_push(Screen::Date);date_editor();return true;}
   if(action=="{\"action\":\"v-check-recovery\"}" && !storage_warning_.empty()) save();
  }
  ok=web_.action(e.value);break;
 case Action::Back:ok=web_.back();break;
 case Action::DictionaryKey:ok=web_.key("key",std::string(1,char(e.value)));break;
 case Action::DictionaryErase:ok=web_.key("backspace");break;
 case Action::DictionaryShift:ok=web_.key("shift");break;
 case Action::DictionarySymbols:ok=web_.key("symbols");break;
 case Action::DictionarySearch:ok=web_.key("submit");break;
 case Action::RecallSkip:ok=web_.command("w-unknown");break;
 case Action::DictionaryRecent:{
  const auto keys=web_keyboard_state(view_.web);
  if(e.value<0||e.value>=keys.count||!(keys.enabled&(1u<<e.value)))break;
  if(e.value==1&&web_long_prompt(view_.web)){web_prompt_return_page_=view_.web_page;view_.screen=Screen::WebPrompt;view_.web_page=0;break;}
  const auto choices=web_keyboard_controls(view_.web);ok=web_.action(choices[e.value]->action);break;}
 case Action::Vocabulary:if(view_.web.route=="word-batch")ok=web_.command("lookup");else if(view_.web.keyboard)ok=web_.command("u-notebook");else{leave_player();vocabulary_return_=Screen::WebLearning;update_day();view_.screen=Screen::Vocabulary;refresh_vocabulary();}break;
 default:return false;
 }
 if(!ok){notice_="这次操作未完成，原进度保留";ESP_LOGE(TAG,"WEB_ACTION %s",web_.error().c_str());}
 ESP_LOGI(TAG,"WEB_INPUT action=%d elapsed_us=%lld ok=%d write_us=%llu gc_us=%llu",int(e.action),(long long)(esp_timer_get_time()-command_began),ok,(unsigned long long)web_.last_write_us(),(unsigned long long)web_.last_gc_us());
 return true;
}
void App::web_audio_tick(){
 if(!web_.ready())return;
 std::string path;int serial=0;bool stop=false;double rate=1;
 if(web_.take_audio(path,serial,stop,&rate)){
  if(stop){audio_.stop();web_audio_playing_=false;web_audio_pending_=false;}
  else{audio_.stop();web_audio_playing_=false;web_audio_path_=path;web_audio_serial_=serial;web_audio_pending_=true;web_audio_slow_=rate<0.99;
   const auto normal=std::string(ROOT)+(path.rfind("vocabulary/audio/",0)==0?"/":"/web-course-v1/")+path;
   web_audio_note_=exists(normal)?(web_audio_slow_?"SD 慢速录音 · 保持音高":"SD 录音 · 原速播放"):"离线合成发音 · 优先使用缓存";dirty_=true;}
 }
 // A resident SD recording can provide immediate audible feedback. The old
 // path waited for an unrelated full-panel status paint before starting it.
 // Synthesis still paints its source/waiting status before doing long work.
 const bool vocabulary_audio=web_audio_path_.rfind("vocabulary/audio/",0)==0;
 const bool immediate_sd=web_audio_pending_&&exists(std::string(ROOT)+(vocabulary_audio?"/":"/web-course-v1/")+web_audio_path_);
 if(web_audio_pending_&&!audio_.busy()&&(!dirty_||immediate_sd)){
  web_audio_pending_=false;
  // Only the embedded map can supply paths; never expose arbitrary SD files.
  if((web_audio_path_.rfind("audio/",0)!=0&&!vocabulary_audio)||!safe_relative_path(web_audio_path_)){web_.playback(web_audio_serial_,false);dirty_=true;return;}
  const auto text=web_.audio_text(web_audio_path_);
  auto real=std::string(ROOT)+(vocabulary_audio?"/":"/web-course-v1/")+web_audio_path_;
  if(web_audio_slow_&&real.size()>4){const auto slow=real.substr(0,real.size()-4)+"-slow.wav";if(exists(slow))real=slow;else web_audio_note_="慢速录音未安装，改为原速播放";}
  web_audio_playing_=true;
  const bool sd_audio=exists(real);
  if(!sd_audio&&view_.web.route=="path-test"){web_audio_playing_=false;web_.playback(web_audio_serial_,false);dirty_=true;return;}
  notice_=sd_audio?"":"SD 录音缺失，使用离线合成发音";
  if(!audio_.play(sd_audio?real:"",sd_audio?"":text,uint32_t(web_audio_serial_))){web_audio_playing_=false;web_.playback(web_audio_serial_,false);dirty_=true;}
 }
}
void App::boot_message(const std::string &stage){
 view_.screen=Screen::Booting;view_.boot_stage=stage;
 if(!MetalioEpd_Ready()||!font_.ready())return;
 Canvas canvas(epd_hl_get_framebuffer(MetalioEpd_Hl()),font_);ui_.render(canvas,view_);
 // The panel retains the previous session's ink; the new RAM history starts
 // white. Re-establish the same physical baseline used by the stable renderer.
 epd_poweron();epd_hl_waveform(MetalioEpd_Hl(),&MY_WAVEFORM);epd_clear();
 const auto err=epd_hl_update_screen_from_white(MetalioEpd_Hl(),MODE_GC16,25);epd_poweroff();
 ESP_LOGI(TAG,"BOOT_STAGE first_frame elapsed_ms=%lld err=%d",(long long)esp_timer_get_time()/1000,int(err));
 full_=err!=EPD_DRAW_SUCCESS;dirty_=true;
 refresh_budget_.completed(true,err==EPD_DRAW_SUCCESS,uint64_t(esp_timer_get_time())/1000);
}
