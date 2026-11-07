{fs::create_directories(work/"preload");LearningWeb m;req(m.init((work/"preload").string()),m.error());run(m,setup+plan+"globalThis.nativeDirectDisabled=true;");
 WebFrameCache frames;frames.configure(font);
 m.prepare_renderer([&](const WebPage&p,const std::function<bool()>&cancel){req(frames.build(p,cancel),"complete prepared raster");});
 auto check_frame=[&]{
  View v;v.screen=Screen::WebLearning;v.web=page(m);
  std::vector<uint8_t> hot(Canvas::bytes,0x57),cold(Canvas::bytes);UI hu,cu;
  req(frames.take(v,hot.data(),hu),"actual committed page matches prepared raster");
  Canvas c(cold.data(),font);cu.render(c,v);req(hot==cold,"pre-render pixels equal ordinary render");
  req(hu.answer_pages()==cu.answer_pages()&&hu.buttons().size()==cu.buttons().size(),"pre-render page count and controls");
  for(size_t i=0;i<cu.buttons().size();++i){const auto&a=hu.buttons()[i];const auto&b=cu.buttons()[i];req(a.rect.x==b.rect.x&&a.rect.y==b.rect.y&&a.rect.w==b.rect.w&&a.rect.h==b.rect.h&&a.label==b.label&&a.enabled==b.enabled&&a.dark_fill==b.dark_fill&&a.event.action==b.event.action&&a.event.value==b.event.value,"prepared button geometry, semantics and enabled state");}
  for(int y=0;y<Canvas::height;y+=37)for(int x=0;x<Canvas::width;x+=31){auto a=hu.hit(x,y),b=cu.hit(x,y);req(a.action==b.action&&a.value==b.value,"prepared hit map including outside buttons");}
  req(!frames.take(v,hot.data(),hu),"raster consumed once");
  req(frames.build(v.web,[]{return false;}),"build mismatch test");
  const auto baseline=hot;v.web.actions.push_back("different action");req(!frames.take(v,hot.data(),hu)&&hot==baseline,"action mismatch never installs pixels");v.web=page(m);v.web_page=1;req(!frames.take(v,hot.data(),hu),"different native pagination cannot hit");v.web_page=0;v.notice="保存失败";req(!frames.take(v,hot.data(),hu),"error notice cannot be hidden by cache");v.notice.clear();
  int polls=0;req(!frames.build(v.web,[&]{return ++polls>12;}),"cancel during raster");req(!frames.take(v,hot.data(),hu),"partial frame never published");
 };
 // Base-transition oracle retains the old isolated JSON/model round trip.
 yes(m,R"JS((()=>{const M=WORDCARD_MODEL;function scenario(fail){const pair=[true,false].map(ref=>{let writes=0,raw=null;const store={getItem:()=>null,setItem(k,v){if(++writes===fail)throw Error('test save failed');raw=v;}};return {ref,model:new M.Model(store,()=>Date.now()),stored:()=>[writes,raw]};});function step(f){const results=pair.map(p=>{globalThis.nativeCoreReference=p.ref;const ok=f(p.model);return JSON.stringify([ok,p.model.error,p.model.state,p.stored()]);});if(results[0]!==results[1])throw Error('core oracle mismatch');return pair[0].model;}
 for(const id of ['u1a','u2a','u3b']){let model=step(m=>m.begin(id));if(!model.state.session)continue;step(m=>m.commit(x=>{if(x.session.unified)x.session.wordDone=[...x.session.wordIds];}));let i=0;while(model.state.session&&i++<80){const s=model.state.session,q=M.task(s);if(q.kind==='listen')step(m=>m.heard(s.at));if(q.kind!=='learn'){step(m=>m.choose(q.answer));step(m=>m.check(i===1));if(model.error)break;}step(m=>m.advance());if(model.error)break;}if(model.state.session)step(m=>m.abandon());}
 const model=step(m=>m.dailyFocus('u1a','read',['read0','read1']));if(model.state.session){step(m=>m.advance());step(m=>m.choose(M.task(m.state.session).answer));step(m=>m.check());step(m=>m.advance());}}
 for(const fail of [0,1,3,8,16])scenario(fail);globalThis.nativeCoreReference=false;return true;})())JS");
 run(m,"t.m.dailyFocus('u1a','read',['read0','read1']);t.go('quiz');nativeAction({action:'choose',n:t.M.task(t.m.state.session).answer});nativeAction({action:'check'});");page(m);
 std::string before;req(m.eval("JSON.stringify([t.m.state,t.p.state,nativeView(),nativeRevision])",&before),m.error());
 m.prepare_next();std::string after;req(m.eval("JSON.stringify([t.m.state,t.p.state,nativeView(),nativeRevision])",&after),m.error());req(before==after,"preparation leaves learner state, current DOM and revision unchanged");
 yes(m,"nativePrepStats.built===1&&nativePrepared!==null");
 click(m,"next");page(m);yes(m,"nativePrepStats.domHits===1&&nativePrepStats.viewHits===1");check_frame();
 yes(m,"(()=>{const hot=JSON.stringify(nativeView());t.render();return hot===JSON.stringify(nativeView());})()");
 run(m,"nativeAction({action:'choose',n:t.M.task(t.m.state.session).answer});nativeAction({action:'check'});");page(m);m.prepare_next();
 click(m,"next");page(m);yes(m,"nativePrepStats.domHits===2&&nativePrepStats.viewHits===2&&nativeInspect().route==='focus-summary'");check_frame();
 yes(m,"(()=>{const hot=JSON.stringify(nativeView());t.render();return hot===JSON.stringify(nativeView());})()");
 // Saving still happens on the real tap; a ready page cannot advance on failure.
 run(m,"t.m.dailyFocus('u1c','listen',['listen0']);t.m.heard(0);t.m.choose(t.M.task(t.m.state.session).answer);t.m.check();t.go('quiz');");page(m);m.prepare_next();
 run(m,"globalThis.preSave=JSON.stringify(t.m.state);globalThis.realStore=t.m.storage;t.m.storage={getItem:()=>null,setItem(){throw Error('test write failure');}};");click(m,"next");
 yes(m,"JSON.stringify(t.m.state)===preSave&&!!t.m.error");run(m,"t.m.storage=realStore;t.m.error='';t.render();");page(m);
 // An interrupted/faulted speculative stage must restore every live field.
 run(m,"globalThis.preFault=JSON.stringify([t.m.state,t.p.state,nativeView(),nativeRevision]);nativePrepareStep();nativePreparationFault=true;try{nativePrepareStep();}catch{}nativeAbortPreparation();nativePreparationFault=false;");
 yes(m,"preFault===JSON.stringify([t.m.state,t.p.state,nativeView(),nativeRevision])");
 // Real QuickJS deadline interruption, not just a normal JavaScript exception.
 run(m,"globalThis.preInterrupt=JSON.stringify([t.m.state,t.p.state,nativeView(),nativeRevision]);globalThis.savedParser=parseHTML;parseHTML=()=>{for(;;){}};");
 m.prepare_next();run(m,"parseHTML=savedParser;");
 yes(m,"nativePreparationRestore===null&&nativePreparationJob===null&&preInterrupt===JSON.stringify([t.m.state,t.p.state,nativeView(),nativeRevision])");
 m.prepare_next();run(m,"nativeAction({action:'lookup'});");yes(m,"nativePrepared===null&&nativePreparationJob===null");click(m,"lookup-return");page(m);
 m.prepare_next();run(m,"globalThis.hitsBefore=nativePrepStats.domHits;globalThis.originalFormatter=Intl.DateTimeFormat;nativePrepared.html+=' ';nativeAction({action:'next'});");page(m);
 yes(m,"nativePrepStats.domHits===hitsBefore&&nativePrepStats.misses>0&&nativeInspect().route==='focus-summary'");
 // Exercise the prepared word-meaning, two-letter, full-spelling and result
 // projections too. Compare exact native structures against a cold render.
 for(const auto*mode:{"staged","full","meaning"}){
  run(m,std::string("t.m.batchBegin(t.ids.slice(0,2),{type:'course',id:'u1a'});t.m.batchSetMode('")+mode+"');t.m.batchStart();for(let i=0;i<8&&t.m.state.batch?.phase==='learn';i++)t.m.batchLearn();t.go('word-batch');");
  for(int i=0;i<24;++i){std::string active;req(m.eval("!!t.m.state.batch",&active),m.error());if(active!="true")break;
   run(m,"{const s=t.m.state.batch,q=t.B.current(s);if(q.kind==='meaning')nativeAction({action:'w-choose',n:s.choices[q.id].answer});else nativeType(q.kind==='gap'?s.gaps[q.id].answer:t.B.get(q.id).word);}");
   if(page(m).keyboard)req(m.key("submit"),m.error());else click(m,"w-check");page(m);m.prepare_next();
   run(m,"globalThis.wordHits=nativePrepStats.domHits;");click(m,"w-next");page(m);
   yes(m,"nativePrepStats.domHits===wordHits+1");
   check_frame();
   yes(m,"(()=>{const hot=JSON.stringify(nativeView());t.render();return hot===JSON.stringify(nativeView());})()");
  }
  yes(m,"!t.m.state.batch");
 }
 req(m.memory_bytes()<3*1024*1024,"one prepared page has bounded retained memory");
}
