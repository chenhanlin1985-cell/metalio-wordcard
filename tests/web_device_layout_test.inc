{fs::create_directories(work/"disclosure");LearningWeb m;req(m.init((work/"disclosure").string()),m.error());run(m,setup+plan);
 const auto home=page(m),detail=web_disclosure(home);req(!detail.keyboard&&detail.tabs.empty(),"detail is a read-only secondary surface");
 for(const auto&b:detail.blocks)req(b.role=="detail"&&b.kind!="button","only disclosed text in generic details");
 for(size_t i=0;i<home.actions.size();++i)req(!web_prompt_action_allowed(home,int(i)),"home details cannot start or navigate tasks");
 run(m,"t.m.batchBegin(t.ids,{type:'course',id:'u1a'});t.m.batchSetMode('full');t.m.batchStart();for(let i=0;i<4;i++)t.m.batchLearn();while(t.B.current(t.m.state.batch).kind==='meaning'){const s=t.m.state.batch,q=t.B.current(s);t.m.batchChoose(s.choices[q.id].answer);t.m.batchCheck();t.m.batchNext();}t.go('word-batch');");req(m.type("co"),m.error());
 const auto input=page(m);const auto full=web_disclosure(input);req(input.keyboard&&!full.keyboard,"long prompt has no overlaid keyboard");req(full.typed=="co","disclosure preserves typed value");
 for(const auto&b:full.blocks)if(b.kind=="button")req(web_prompt_action_allowed(input,b.action),"prompt only permits audio and hint");
 for(size_t i=0;i<input.actions.size();++i)if(input.actions[i].find("w-unknown")!=std::string::npos||input.actions[i].find("w-next")!=std::string::npos)req(!web_prompt_action_allowed(input,int(i)),"cannot score or advance in disclosure");
 frame(m,frames,"30-spelling-details",Screen::WebPrompt);
 req(m.home(),m.error());frame(m,frames,"31-home-details",Screen::WebPrompt);
 run(m,"t.m.commit(x=>{x.batch=null;});t.m.dailyFocus('u1a','read',['read0']);t.go('quiz')");const auto reading=page(m);
 req(std::any_of(reading.blocks.begin(),reading.blocks.end(),[](const auto&b){return b.role=="article-position"&&b.text.find("/")!=std::string::npos;}),"article fraction survives projection");
}
{std::vector<uint8_t>a(Canvas::bytes,255),b=a;req(!web_binary_selection_safe(a.data(),b.data(),a.size()),"no change is not a refresh");a[0]=0xf0;req(web_binary_selection_safe(a.data(),b.data(),a.size()),"one binary transition safe");a[1]=0xef;req(!web_binary_selection_safe(a.data(),b.data(),a.size()),"any gray edge rejects DU");std::fill(a.begin(),a.end(),0);req(!web_binary_selection_safe(a.data(),b.data(),a.size()),"large binary replacement rejected");
 fs::create_directories(work/"selection");LearningWeb m;req(m.init((work/"selection").string()),m.error());run(m,setup+plan);run(m,"t.m.dailyReview('meaning',4)");
 run(m,"t.m.commit(x=>{x.batch=null;x.batchMemory=Object.fromEntries(t.ids.map(id=>[id,{meaning:{stage:0,due:Date.now()-1,last:0},spelling:{stage:0,due:Date.now()-1,last:0}}]));});t.m.dailyReview('meaning',4);t.m.batchStart();t.go('word-batch');");
 View v;v.screen=Screen::WebLearning;v.web=page(m);const auto key=v.web.key;Canvas ca(a.data(),font),cb(b.data(),font);ui.render(ca,v);run(m,"nativeAction({action:'w-choose',n:0})");v.web=page(m);ui.render(cb,v);req(key==v.web.key,"selection stays same page identity");req(web_binary_selection_safe(a.data(),b.data(),a.size()),"actual rendered selection has only small binary differences");run(m,"nativeAction({action:'w-check'})");v.web=page(m);ui.render(ca,v);req(key!=v.web.key,"answer feedback is a new refresh identity");req(!web_binary_selection_safe(a.data(),b.data(),a.size()),"answer text never uses binary selection shortcut");
}
