{fs::create_directories(work/"home-nav");LearningWeb m;req(m.init((work/"home-nav").string()),m.error());run(m,setup+plan);
 run(m,"t.m.batchBegin(t.ids,{type:'course',id:'u1a'});t.m.batchSetMode('staged');t.m.batchStart();t.m.batchLearn();t.m.batchPause();t.go('home');");
 auto press=[&](const std::string&label){View v;v.screen=Screen::WebLearning;v.web=page(m);Canvas c(fb.data(),font);ui.render(c,v);for(const auto&b:ui.buttons())if(b.label==label){const auto e=ui.hit(b.rect.x+b.rect.w/2,b.rect.y+b.rect.h/2);req(e.action==Action::WebAction,"home action uses real projected route: "+label);req(m.action(e.value),m.error());return;}throw std::runtime_error("missing home control "+label);};
 frame(m,frames,"32-home-resume");const auto h=page(m);req(h.tabs.size()==4,"four global destinations");
 const auto settings=std::find_if(ui.buttons().begin(),ui.buttons().end(),[](const auto&b){return b.label=="设置";});req(settings!=ui.buttons().end(),"home settings target exists");req(settings->rect.h>=88,"home top target is finger sized");req(ui.hit(settings->rect.x+settings->rect.w/2,settings->rect.y-6).action==Action::DeviceMenu,"six pixel finger roll still hits top settings");req(ui.hit(settings->rect.x+settings->rect.w/2,settings->rect.y-20).action==Action::None,"touch tolerance stays bounded");req(h.tabs[0].text=="首页"&&h.tabs[1].text=="课程"&&h.tabs[2].text=="复习"&&h.tabs[3].text=="词典","stable navigation vocabulary");
 yes(m,"nativeView().blocks.some(b=>b.role==='home-next'&&b.text.includes('第 2 / 4'))");
 std::string before,after;req(m.eval("JSON.stringify([t.m.state,t.p.state])",&before),"capture original isolated records");
 for(const auto&it:std::vector<std::pair<std::string,std::string>>{{"课程","books"},{"复习","review"},{"词典","dictionary"}}){press(it.first);req(page(m).route==it.second,"navigation enters correct destination");press("首页");req(page(m).route=="home","root nav returns home");}
 frame(m,frames,"32-home-resume");req(std::count_if(ui.buttons().begin(),ui.buttons().end(),[](const Button&b){return b.enabled;})==7,"simple home has settings, primary, arrangements and four tabs only");
 press("学习安排");req(page(m).route=="home-options","secondary management page");frame(m,frames,"55-home-arrangements");
 press("课内生词本 · 0");req(page(m).route=="word-list","arrangements notebook opens notebook");req(m.back(),m.error());req(page(m).route=="home-options","notebook returns to arrangements");
 press("学习计划");req(page(m).route=="path-plan","opens actual saved plan");req(m.back(),m.error());req(page(m).route=="home-options","plan returns arrangements");
 press("起点摸底");req(page(m).route=="path-brief","opens test briefing, does not answer");req(m.back(),m.error());req(page(m).route=="home-options","brief returns arrangements");
 press("今日分项进度");req(page(m).route=="path-today","existing daily details preserved");req(m.back(),m.error());req(page(m).route=="home-options","details return arrangements");
 req(m.back(),m.error());req(page(m).route=="home","secondary back returns simple home");
 for(int i=0;i<5;++i)page(m);req(m.eval("JSON.stringify([t.m.state,t.p.state])",&after),"capture records after navigation");req(before==after,"home browsing preserves course/plan records and paused word position");
 press("继续学习");req(page(m).route=="word-batch","resume enters existing word batch");yes(m,"t.m.state.batch.learnAt===1&&t.m.state.batch.phase==='learn'");
 // Rest days/provisional plans cannot masquerade as completed work.
 run(m,"t.p.commit(x=>{x.plan=null;});t.go('home');");yes(m,"nativeView().blocks.some(b=>b.role==='home-meta'&&b.text.includes('试学安排'))");
 frame(m,frames,"33-home-provisional");
}
{fs::create_directories(work/"home-sources");LearningWeb m;req(m.init((work/"home-sources").string()),m.error());run(m,setup+plan);
 yes(m,"t.p.trainBegin('complex-tech','read',2)");yes(m,"t.m.begin('u2a')");yes(m,"t.m.batchBegin(t.ids,{type:'course',id:'u1a'})");yes(m,"t.m.batchSetMode('staged')");yes(m,"t.m.batchStart()");yes(m,"t.m.batchLearn()");run(m,"t.saved=JSON.parse(JSON.stringify(t.m.state.batch));t.go('home');");
 yes(m,"t.D.snapshot(t.m,t.p).active.type==='batch'");yes(m,"nativeView().blocks.find(b=>b.role==='home-course').detail.startsWith('1A')");frame(m,frames,"42-home-coexisting-batch");click(m,"w-day-next");yes(m,"nativeInspect().route==='word-batch'&&t.m.state.batch.source.id==='u1a'&&t.m.state.batch.learnAt===1");
 run(m,"t.m.commit(x=>{x.batch=null;x.dailyPausedBatch={...t.saved,paused:true};});t.go('home');");yes(m,"t.D.snapshot(t.m,t.p).active.type==='saved-batch'");yes(m,"nativeView().blocks.find(b=>b.role==='home-course').detail.startsWith('1A')");click(m,"w-day-next");yes(m,"t.m.state.batch.source.id==='u1a'&&t.m.state.batch.learnAt===1&&t.p.state.training.pack==='complex-tech'");
 run(m,"t.m.commit(x=>{x.batch=null;delete x.dailyPausedBatch;});t.go('home');");yes(m,"t.D.snapshot(t.m,t.p).active.type==='course'");yes(m,"nativeView().blocks.find(b=>b.role==='home-course').detail.startsWith('2A')");frame(m,frames,"43-home-coexisting-course");click(m,"w-day-next");yes(m,"t.m.state.session.lesson==='u2a'&&t.m.state.batch.source.id==='u2a'&&t.p.state.training.pack==='complex-tech'");
 run(m,"t.m.commit(x=>{x.batch=null;x.session=null;delete x.dailyPausedBatch;});t.go('home');");yes(m,"t.D.snapshot(t.m,t.p).active.type==='practice'");yes(m,"nativeView().blocks.find(b=>b.role==='home-course').detail===t.P.practice.pack('complex-tech').title");frame(m,frames,"44-home-bridge-practice");click(m,"w-day-next");yes(m,"nativeInspect().route==='path-practice'&&t.p.state.training.pack==='complex-tech'");
}
