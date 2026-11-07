{fs::create_directories(work/"early-preload");LearningWeb m;req(m.init((work/"early-preload").string()),m.error());run(m,setup+plan);
 yes(m,R"JS((()=>{const M=WORDCARD_MODEL,store={getItem:()=>null,setItem(){}};let count=0;for(const lesson of M.data.lessons){if(lesson.role!=='lesson')continue;for(const q of M.tasks(lesson.id).filter(t=>['read','listen'].includes(t.kind)))for(const assisted of [false,true]){
 const models=[new M.Model(store,()=>123456789),new M.Model(store,()=>123456789)];
 for(const x of models){if(!x.dailyFocus(lesson.id,q.kind,[q.id]))throw Error(x.error);if(q.kind==='listen')x.heard(0);x.choose(q.answer);if(assisted)x.state.session.lookupAssistedAt=0;x.state.session.errors=[q.id];}
 const normal=models[0].check();globalThis.nativeProjectionModel=models[1];const projected=models[1].check();globalThis.nativeProjectionModel=null;
 if(normal!==projected||JSON.stringify(models[0].state)!==JSON.stringify(models[1].state))throw Error('private check projection mismatch '+q.id);count++;
 } }return count>=40;})())JS");
 // Prediction may never count a future answer or a future audio completion.
 for(const auto*kind:{"read","listen"})for(bool correct:{true,false}){
  run(m,std::string("t.m.abandon();t.m.dailyFocus('u1a','")+kind+"',['"+kind+"0']);t.go('quiz');");page(m);
  std::string before,after;req(m.eval("JSON.stringify([t.m.state,t.p.state,nativeView(),nativeRevision])",&before),m.error());
  m.prepare_next();req(m.eval("JSON.stringify([t.m.state,t.p.state,nativeView(),nativeRevision])",&after),m.error());req(before==after,"early prediction preserves unanswered/unheard live state");
  yes(m,"globalThis.nativeProjectionModel!==t.m");
  yes(m,"!!nativePrepared&&!t.m.state.session.checked");
  if(std::string(kind)=="listen"){
   run(m,"nativeAction({action:'choose',n:t.M.task(t.m.state.session).answer});nativeAction({action:'check'});");
   yes(m,"!t.m.state.session.checked&&!t.m.state.session.heard[t.m.state.session.at]");
   run(m,"t.m.error='';t.m.heard(t.m.state.session.at);t.render();");
  }
  run(m,std::string("nativeAction({action:'choose',n:")+(correct?"t.M.task(t.m.state.session).answer":"(t.M.task(t.m.state.session).answer+1)%t.M.task(t.m.state.session).options.length")+"});nativeAction({action:'check'});");
  yes(m,"!!nativePrepared");click(m,"next");page(m);
  yes(m,"(()=>{const actual=JSON.stringify(nativeView());t.render();return actual===JSON.stringify(nativeView());})()");
  yes(m,correct?"!t.m.state.session":"!!t.m.state.session");
 }
 // Completed snapshot stages survive same-question pagination, but not exit.
 run(m,"t.m.abandon();t.m.dailyFocus('u1a','read',['read0','read1']);t.go('quiz');nativePrepareStep();globalThis.oldJob=nativePreparationJob;nativeAction({action:'read-page',delta:1});");
 yes(m,"nativePreparationJob===oldJob");m.prepare_next();yes(m,"!!nativePrepared");
 run(m,"nativeAction({action:'lookup'});");yes(m,"nativePrepared===null&&nativePreparationJob===null");
}
