// Settings are an overlay. Device pages never replace the learning engine.
void App::menu_push(Screen target){
 if(view_.screen==target)return;
 auto found=std::find(menu_stack_.begin(),menu_stack_.end(),target);
 if(target!=Screen::WebLearning&&found!=menu_stack_.end())menu_stack_.erase(found,menu_stack_.end());
 else menu_stack_.push_back(view_.screen);
 view_.screen=target;
}
bool App::menu_return(){
 if(menu_stack_.empty())return false;
 view_.screen=menu_stack_.back();menu_stack_.pop_back();return true;
}
bool App::menu_dispatch(Event e){
 if(!web_.ready())return false;
 const auto screen=view_.screen;
 if(e.action==Action::Home){
  if(!web_.home()){notice_="无法返回背词，进度已保留";return true;}
  leave_player();menu_stack_.clear();menu_training_=false;view_.more_learning_scope=false;view_.legacy_home=false;
  view_.screen=Screen::WebLearning;return true;
 }
 if(e.action==Action::MoreLearning && view_.more_learning_scope){
  if(!web_.resume_main()){notice_="课程进度保存未完成，请重试返回";return true;}
  leave_player();view_.more_learning_scope=false;view_.screen=Screen::MoreLearning;return true;
 }
 if(e.action==Action::DeviceMenu){leave_player();menu_push(Screen::WebTools);return true;}
 if(e.action==Action::DeviceSettings||e.action==Action::Profile){leave_player();menu_push(Screen::DeviceSettings);return true;}
 if(e.action==Action::MoreLearning){menu_push(Screen::MoreLearning);return true;}
 if(e.action==Action::LearningSettings){
  if(!web_.command("v-go","settings")){notice_="背词设置暂时无法打开";return true;}
  menu_training_=true;menu_push(Screen::WebLearning);return true;
 }
 if(e.action==Action::LegacyCourses){
  if(!web_.open_courses()){notice_="课程暂时无法打开，原记录保留";return true;}
  view_.more_learning_scope=true;view_.screen=Screen::WebLearning;return true;
 }
 if(e.action==Action::GuidedCourses)view_.legacy_home=false;
 if(screen==Screen::MoreLearning&&(e.action==Action::GuidedCourses||e.action==Action::Library))view_.more_learning_scope=true;
 if(e.action==Action::Back){
  if(menu_training_&&screen==Screen::WebLearning&&view_.web.route=="settings"){
   if(web_.back()){menu_training_=false;menu_return();}return true;
  }
  if(view_.more_learning_scope&&(screen==Screen::Library||screen==Screen::GuidedCourses||screen==Screen::Home||
       (screen==Screen::WebLearning&&(view_.web.route=="books"||view_.web.route=="home")))){
   return menu_dispatch({Action::MoreLearning});
  }
  // USB exit must first pass through the storage shutdown handler.
  if(screen!=Screen::WebLearning&&screen!=Screen::WebPrompt&&screen!=Screen::UsbDisk &&
     (screen==Screen::WebTools||screen==Screen::DeviceSettings||screen==Screen::MoreLearning||screen==Screen::WebImportConfirm||
      screen==Screen::Settings||screen==Screen::Date||screen==Screen::Resources||screen==Screen::Stats||screen==Screen::SelfTest||screen==Screen::TtsTest)){
   if(menu_return())return true;
  }
 }
 // Track device subpages by their actual parent, including diagnostics cycles.
 Screen target=screen;
 if(e.action==Action::Settings)target=Screen::Settings;
 else if(e.action==Action::Date)target=Screen::Date;
 else if(e.action==Action::Resources)target=Screen::Resources;
 else if(e.action==Action::SelfTest)target=Screen::SelfTest;
 else if(e.action==Action::TtsTest)target=Screen::TtsTest;
 else if(e.action==Action::Stats)target=Screen::Stats;
 else if(e.action==Action::WebImport&&e.value==0)target=Screen::WebImportConfirm;
 if(target!=screen&&!menu_stack_.empty())menu_push(target);
 return false;
}
void App::menu_finish(Event e,Screen before){
 if(!web_.ready())return;
 if(before==Screen::Date&&(e.action==Action::SaveDate||e.action==Action::SkipDate)&&view_.screen==Screen::Home&&!menu_stack_.empty()){menu_return();return;}
 if(before==Screen::WebImportConfirm&&e.action==Action::WebImport&&e.value!=0&&view_.screen==Screen::WebLearning&&!menu_stack_.empty()){menu_return();return;}
 if(view_.more_learning_scope&&e.action==Action::SavePlan&&view_.screen==Screen::Home){view_.screen=previous_;return;}
 if(view_.more_learning_scope&&view_.screen==Screen::Home&&!view_.legacy_home)menu_dispatch({Action::MoreLearning});
}