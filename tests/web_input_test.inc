// Real persisted model + native key entry; synthetic learner, never real progress.
{fs::create_directories(work/"input");LearningWeb m;req(m.init((work/"input").string()),m.error());run(m,setup);
 run(m,"t.m.batchBegin(t.ids,{type:'course',id:'u1a'});t.m.batchSetMode('staged');t.m.batchStart();for(let i=0;i<4;i++)t.m.batchLearn();while(t.B.current(t.m.state.batch).kind==='meaning'){const s=t.m.state.batch,q=t.B.current(s);t.m.batchChoose(s.choices[q.id].answer);t.m.batchCheck();t.m.batchNext();}t.go('word-batch');");
 auto p=page(m);req(!p.gap_mask.empty()&&std::count(p.gap_mask.begin(),p.gap_mask.end(),'_')==2,"two public gap slots");
 frame(m,frames,"56-gap-empty");
 const auto builds=m.page_builds();
 for(const auto&keys:std::vector<std::string>{"a","n",std::string(1,'\b'),"z"}){
  View v;v.screen=Screen::WebLearning;v.web=page(m);Canvas c(fb.data(),font);ui.render(c,v);const auto previous=fb;
  req(m.edit(keys),m.error());req(m.input_pending(),"GC retained until paint boundary");
  v.web=page(m);req(ui.render_input(c,v),"only input changes, partial paint available");auto patched=fb;
  req(keyboard_fast_refresh_safe(patched.data(),previous.data(),fb.size(),true),"real gap edit remains inside binary field/submit bounds");
  req(keyboard_fast_refresh_safe(fb.data(),std::vector<uint8_t>(Canvas::bytes,255).data(),fb.size(),true)==false,"whole frame never accepted as input-only diff");
  ui.render(c,v);req(patched==fb,"partial paint is pixel-identical to full renderer, including submit state");
  req(m.page_builds()==builds,"letter edits reuse action/page projection");
  m.finish_input();req(!m.input_pending(),"one bounded collection after painting");
 }
 frame(m,frames,"57-gap-filled");req(m.edit("\b"),m.error());frame(m,frames,"58-gap-next-slot");m.finish_input();
 req(m.edit("bc\bde"),m.error());req(page(m).typed=="abde","burst preserves key and erase order");frame(m,frames,"59-gap-overflow");m.finish_input();
 std::string saved;{std::ifstream f(work/"input/course-v2.json");saved.assign(std::istreambuf_iterator<char>(f),{});}req(saved.find("\"typed\":\"abde\"")!=std::string::npos,"committed before visual acknowledgement");
 req(m.edit(std::string(121,'x')),m.error());req(page(m).typed=="abde"&&!page(m).toast.empty(),"invalid burst rolls back visibly");m.finish_input();
 req(m.command("lookup"),m.error());req(m.key("key","f"),m.error());req(page(m).route=="search","stale C++ page never edits wrong route");req(m.command("lookup-return"),m.error());req(page(m).typed=="abde","lookup returns original input");
 req(m.type("a"),m.error());page(m);req(m.edit("\b\b"),m.error());req(page(m).typed.empty(),"erase through empty is safe");m.finish_input();
 // Save failure must keep the old value and expose recovery, never draw unsaved input.
 fs::create_directory(work/"input/course-v2.json.tmp");req(m.edit("c"),m.error());req(page(m).typed.empty()&&!page(m).toast.empty(),"failed SD write leaves original input and feedback");m.finish_input();fs::remove(work/"input/course-v2.json.tmp");
 req(m.edit("a"),m.error());m.finish_input();frame(m,frames,"60-gap-recovered");
 run(m,"t.m.commit(x=>{x.batch.hint=false;});t.render()");auto unhinted=page(m);req(std::none_of(unhinted.blocks.begin(),unhinted.blocks.end(),[](const auto&b){return b.text.find("提示：")!=std::string::npos;}),"cache regression begins without hint");req(m.command("w-hint"),m.error());req(m.edit("\b"),m.error());auto hinted=page(m);req(std::any_of(hinted.blocks.begin(),hinted.blocks.end(),[](const auto&b){return b.text.find("提示：")!=std::string::npos;}),"an intervening same-page action invalidates delta cache");req(m.edit("a"),m.error());m.finish_input();
}
{LearningWeb m;req(m.init((work/"input").string()),m.error());run(m,setup);run(m,"t.go('word-batch')");req(page(m).typed=="a","cold reload retains confirmed input");frame(m,frames,"61-gap-reloaded");}
{Canvas c(fb.data(),font);View v;v.screen=Screen::Dictionary;v.dictionary_ready=true;ui.render(c,v);
 for(const auto&s:std::vector<std::string>{"a","appointment","firmware",""}){v.dictionary_query=s;req(ui.render_input(c,v),"dictionary accepts field-only repaint");auto partial=fb;ui.render(c,v);req(partial==fb,"dictionary partial paint matches original full renderer exactly");}}
{LearningWeb m;req(m.init((work/"input").string()),m.error());run(m,setup);run(m,"t.m.commit(x=>{x.batch.at=x.batch.queue.findIndex(q=>q.kind==='spelling');x.batch.typed='';});t.go('word-batch');");
 View v;v.screen=Screen::WebLearning;v.web=page(m);req(v.web.gap_mask.empty(),"full spelling never exposes the gap/answer pattern");Canvas c(fb.data(),font);ui.render(c,v);
 req(m.key("key","c"),m.error());v.web=page(m);req(ui.render_input(c,v),"full spelling direct field update");const auto partial=fb;ui.render(c,v);req(partial==fb,"full spelling partial/full pixels equal");m.finish_input();frame(m,frames,"62-full-input");
}
