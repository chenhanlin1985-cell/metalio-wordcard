{fs::create_directories(work/"direct-quiz");LearningWeb m;req(m.init((work/"direct-quiz").string()),m.error());run(m,setup+plan);
 auto compare=[&](const std::string&reason){
  run(m,"globalThis.nativeDirectDisabled=true;t.render();");const auto reference=page(m);
  std::string before,after;req(m.eval("JSON.stringify([t.m.state,t.p.state])",&before),m.error());
  run(m,"globalThis.nativeDirectDisabled=false;t.render();");const auto direct=page(m);
  if(!WebFrameCache::equal(reference,direct)){std::string actual; m.eval("nativeView()",&actual);std::cerr<<reason<<" "<<actual<<"\n";}
  req(WebFrameCache::equal(reference,direct),"direct quiz all data/actions vs HTML: "+reason);
  req(m.eval("JSON.stringify([t.m.state,t.p.state])",&after),m.error());req(before==after,"pure direct projection: "+reason);
 };
 std::string count;req(m.eval("globalThis.directCases=t.M.data.lessons.flatMap(l=>t.M.tasks(l.id).filter(q=>['read','listen'].includes(q.kind)).map(q=>({lesson:l.id,id:q.id,kind:q.kind})));directCases.length",&count),m.error());
 for(int i=0;i<std::stoi(count);++i){
  const auto prefix="globalThis.directCase=directCases["+std::to_string(i)+"];t.m.abandon();t.m.dailyFocus(directCase.lesson,directCase.kind,[directCase.id]);t.go('quiz');";
  run(m,prefix);compare("unanswered "+std::to_string(i));
  run(m,"nativeAction({action:'read-page',delta:1});");compare("pagination "+std::to_string(i));
  run(m,"t.m.error='';if(directCase.kind==='listen')t.m.heard(0);t.m.choose(t.M.task(t.m.state.session).answer);t.render();");compare("heard-selected "+std::to_string(i));
  run(m,"t.m.check();t.render();");compare("correct feedback "+std::to_string(i));
  run(m,prefix+"if(directCase.kind==='listen')t.m.heard(0);t.m.choose((t.M.task(t.m.state.session).answer+1)%t.M.task(t.m.state.session).options.length);t.m.check();t.render();");compare("wrong feedback "+std::to_string(i));
  run(m,prefix+"if(directCase.kind==='listen'){t.m.skip();t.render();}");compare("skip "+std::to_string(i));
 }
 run(m,"t.m.abandon();t.m.dailyFocus('u1a','listen',['listen0']);t.go('quiz');nativeAction({action:'audio',text:t.M.task(t.m.state.session).text,step:'0'});");compare("audio playing");
 std::string audioPath;int audioSerial=0;bool audioStop=false;req(m.take_audio(audioPath,audioSerial,audioStop)&&!audioStop,"direct audio starts");req(m.playback(audioSerial,false),"direct audio failure callback");compare("audio failure message");
 run(m,"t.m.error='';t.m.heard(0);t.m.choose(t.M.task(t.m.state.session).answer);t.m.state.session.lookupAssistedAt=0;t.m.check();t.render();");compare("lookup-assisted feedback");
 // All-course oracle above accumulates a deliberately large synthetic history;
 // use a fresh synthetic state for the bounded one-page preparation check.
 run(m,"t.m.state=t.M.fresh();t.m.error='';nativeClearToast();t.m.dailyFocus('u1a','read',['read0','read1']);t.go('quiz');");page(m);
 WebFrameCache frames;WebPage projected;frames.configure(font);m.prepare_renderer([&](const WebPage&p,const std::function<bool()>&cancel){projected=p;req(frames.build(p,cancel),"direct prepared frame");});
 m.prepare_next();run(m,"nativeAction({action:'choose',n:t.M.task(t.m.state.session).answer});nativeAction({action:'check'});nativeAction({action:'next'});");
 View v;v.screen=Screen::WebLearning;v.web=page(m);std::vector<uint8_t> hot(Canvas::bytes),cold(Canvas::bytes);UI a,b;
 if(!WebFrameCache::equal(projected,v.web)){std::string details;m.eval("({prepared:nativePrepared,source:nativePrepSource(),current:nativeView(),stats:nativePrepStats})",&details);std::cerr<<"DIRECT CACHE "<<frames.built()<<" "<<projected.key<<" vs "<<v.web.key<<" "<<details<<"\n";}
 req(frames.take(v,hot.data(),a),"direct native page consumes matching full raster without DOM");Canvas c(cold.data(),font);b.render(c,v);req(hot==cold,"direct cached pixels exact");
 const auto beforeLookup=page(m);
 run(m,"nativeAction({action:'lookup'});nativeAction({action:'lookup-return'});");
 req(WebFrameCache::equal(beforeLookup,page(m)),"direct lookup returns identical current page");
 yes(m,"t.m.state.session.at===1&&t.m.state.session.page===0&&t.m.state.session.lookupAssistedAt===1&&!t.m.state.lookup");
 run(m,"t.m.error='test save failed';t.render();");yes(m,"nativeDirect===null&&nativeView().blocks.some(b=>b.text.includes('test save failed'))");
 yes(m,"(()=>{const p=new NativeNode('div');p.innerHTML='<p>one <span>two</span></p>';const a=p.textContent,b=p.textContent;p.querySelector('span').textContent='three';if(p.textContent!=='one three')return false;p.querySelector('p').insertAdjacentHTML('beforeend',' four');return a===b&&p.textContent==='one three four';})()");
}
