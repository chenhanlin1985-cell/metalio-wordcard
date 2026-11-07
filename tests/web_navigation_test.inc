{fs::create_directories(work/"navigation");LearningWeb m;req(m.init((work/"navigation").string()),m.error());run(m,setup+plan);
 auto press=[&](const std::string&label){const auto p=page(m);for(const auto&b:p.blocks)if(b.text==label&&b.enabled){req(m.action(b.action),m.error());return;}for(const auto&b:p.tabs)if(b.text==label){req(m.action(b.action),m.error());return;}throw std::runtime_error("missing native navigation "+label);};
 std::string before,after;req(m.eval("JSON.stringify([t.m.state,t.p.state])",&before),m.error());
 press("课程");frame(m,frames,"45-course-root");req(ui.answer_pages()==1,"courses root fits single screen");
 press("第 1 单元 · 认识与交流");frame(m,frames,"46-course-unit");req(m.back(),m.error());req(page(m).route=="books","unit returns to its actual course root");
 press("其他教材与课程说明");frame(m,frames,"47-other-books");req(m.back(),m.error());yes(m,"!nativeOtherBooks");
 press("整册目录");req(m.back(),m.error());req(page(m).route=="books","full directory back returns courses");
 press("复习");frame(m,frames,"48-review-empty");req(ui.answer_pages()==1,"review root fits one screen");
 press("课内生词本 · 0");req(m.back(),m.error());req(page(m).route=="review","review notebook back");
 press("词典");frame(m,frames,"49-dictionary-root");req(ui.answer_pages()==1,"dictionary root fits one screen");
 press("课内生词本 · 0");req(m.back(),m.error());req(page(m).route=="dictionary","dictionary notebook back");
 press("课内词语与表达");req(page(m).keyboard,"dictionary opens preserved keyboard");req(m.back(),m.error());req(page(m).route=="dictionary","input returns dictionary root");
 req(m.eval("JSON.stringify([t.m.state,t.p.state])",&after),m.error());req(before==after,"root browsing leaves learner data byte-equivalent");
 const auto steady=m.memory_bytes();for(int i=0;i<12;++i){press("课程");press("复习");press("词典");press("首页");}req(m.memory_bytes()<steady+128*1024,"repeated root navigation has bounded retained memory");
 // Continue a real paused group, lookup, and return without moving the word.
 run(m,"t.m.batchBegin(t.ids,{type:'course',id:'u1a'});");run(m,"t.m.batchSetMode('staged');");run(m,"t.m.batchStart();");run(m,"t.m.batchLearn();");run(m,"t.m.batchPause();t.go('home');");
 press("继续学习");yes(m,"t.m.state.batch.learnAt===1");click(m,"lookup");req(m.type("city"),m.error());req(m.key("submit"),m.error());frame(m,frames,"50-study-lookup");
 click(m,"audio");std::string path;int token=0;bool stop=false;req(m.take_audio(path,token,stop),"lookup audio exists");req(m.playback(token,true),m.error());
 click(m,"add");yes(m,"t.m.notebookIds().includes(t.ids[1])");req(m.back(),m.error());yes(m,"nativeInspect().route==='word-batch'&&t.m.state.batch.learnAt===1&&!t.m.state.lookup");frame(m,frames,"51-study-return");req(m.back(),m.error());req(page(m).route=="home","pause returns home");
 // Separate due lanes, real controls, and future review after one answer.
 run(m,"t.m.commit(x=>{x.batch=null;x.session=null;delete x.dailyPausedBatch;});");run(m,"t.m.batchBegin(t.ids,{type:'course',id:'u1a'});");run(m,"t.m.batchSetMode('meaning');");run(m,"t.m.batchStart();");for(int i=0;i<4;++i)run(m,"t.m.batchLearn();");run(m,"t.go('word-batch');");finish(m);
 run(m,"Date.now=()=>Date.parse('2026-11-03T10:00:00+08:00');t.go('review');");frame(m,frames,"52-review-due");press("复习词义");frame(m,frames,"53-review-setup");click(m,"w-begin");frame(m,frames,"54-review-question");req(m.back(),m.error());req(page(m).route=="home","review pause returns home with queue saved");yes(m,"!!t.m.state.batch&&t.m.state.batch.paused");
}
