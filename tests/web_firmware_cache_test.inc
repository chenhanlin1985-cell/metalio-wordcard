{fs::create_directories(work/"cache-revision");LearningWeb m;bool ok=m.init((work/"cache-revision").string());req(ok,m.error());run(m,setup+plan);auto first=page(m);const auto builds=m.page_builds();for(int i=0;i<20;++i){auto p=page(m);req(p.key==first.key&&p.blocks.size()==first.blocks.size(),"cached view remains identical");}req(m.page_builds()==builds&&m.page_hits()>=20,"unchanged reads use cache");
 for(const auto name:{"WORDCARD_MODEL","WORDCARD_LEGACY_MODEL"}){
  const std::string n=name;
  yes(m,n+".data.lessons.every(l=>JSON.stringify("+n+".tasks(l.id))===JSON.stringify("+n+".nativeReferenceTasks(l.id)))");
  yes(m,n+".data.lessons.every(l=>"+n+".tasks(l.id)!=="+n+".tasks(l.id))");
  yes(m,n+".data.lessons.every(l=>"+n+".tasks(l.id).every(q=>Object.isFrozen(q)&&(!q.options||Object.isFrozen(q.options))))");
  yes(m,n+".tasks('toString').length===0&&"+n+".tasks('missing').length===0");
 }
 yes(m,"(()=>{const B=WORDCARD_BATCH,strings=['','ASCII','词义 Français 한국어','\\u0000','\\ud800','\\udc00','\\ud834\\udf06','x\\ud800z','a😀中'];for(let i=0;i<512;i++)strings.push(String.fromCharCode(i*127%65536,i*7919%65536,i*31%65536));return strings.every(s=>B.nativeCachedHash(s)===B.nativeReferenceHash(s));})()");
 yes(m,"(()=>{const B=WORDCARD_BATCH,pools=[[],['one'],['b','a'],B.catalog.map(w=>w.id),B.catalog.map(w=>w.meaning),['same','same','a','😀','中','\\ud800']];for(const ids of pools)for(let seed=0;seed<24;seed++)for(const avoid of [undefined,ids[0],ids.at(-1)])if(JSON.stringify(B.nativeOrder(ids,seed,avoid))!==JSON.stringify(B.nativeReferenceOrder(ids,seed,avoid)))return false;return true;})()");
 yes(m,"(()=>{const B=WORDCARD_BATCH;for(const w of B.catalog)for(const seed of ['1','999'])for(const ids of [[w.id],B.catalog.slice(0,8).map(x=>x.id)]){if(JSON.stringify(B.nativeOptions(w,ids,seed))!==JSON.stringify(B.nativeReferenceOptions(w,ids,seed)))return false;}return B.catalog.every(w=>B.get(w.id)===w)&&B.get('toString')===undefined;})()");
 yes(m,"(()=>{const B=WORDCARD_BATCH,w=B.catalog[0],ids=[w.id],a=B.nativeOptions(w,ids,'77'),expected=JSON.stringify(a);a.options[0]='changed';return JSON.stringify(B.nativeOptions(w,ids,'77'))===expected;})()");
 run(m,"t.m.batchBegin(t.ids,{type:'course',id:'u1a'});t.m.batchSetMode('full');t.m.batchStart();for(let i=0;i<4;i++)t.m.batchLearn();while(t.B.current(t.m.state.batch).kind==='meaning'){const s=t.m.state.batch,q=t.B.current(s);t.m.batchChoose(s.choices[q.id].answer);t.m.batchCheck();t.m.batchNext();}t.go('word-batch');");auto empty=page(m);req(empty.keyboard&&!empty.can_submit,"empty input initially disabled");req(m.key("key","c"),m.error());auto typed=page(m);req(typed.typed=="c"&&typed.can_submit,"non-rendered input edit invalidates cache");req(m.key("backspace"),m.error());req(page(m).typed.empty()&&!page(m).can_submit,"erase invalidates submit cache");
 req(empty.key==typed.key,"individual input characters do not count as page turns");
 req(m.command("lookup"),m.error());req(page(m).key!=empty.key,"lookup route changes refresh identity");req(m.type("co"),m.error());req(web_keyboard_controls(page(m)).size()==3,"cached selectors preserve three candidates");req(m.key("submit"),m.error());req(!page(m).toast.empty(),"toast update invalidates view");req(m.command("lookup-return"),m.error());req(page(m).typed.empty(),"lookup return input cache accurate");
 yes(m,"nativeApp.querySelectorAll('[data-action]').length>0");yes(m,"!!nativeApp.querySelector('.screen-head h1')");yes(m,"nativeApp.querySelectorAll('input[id=spelling]').length===1");
 run(m,"t.m.commit(x=>{x.batch=null;x.session=null;});t.m.begin('u1a');t.m.commit(x=>{x.session.at=x.session.queue.indexOf('read0');});t.go('quiz');");const auto reading=page(m);
 run(m,"nativeAction({action:'read-page',delta:1})");req(page(m).key!=reading.key,"reading pagination changes refresh identity");
 for(const auto field:{"learnAt","page","stage","attemptPage"}){
  const std::string f=field;
  yes(m,"(()=>{const oldB=t.m.state.batch,oldT=t.p.state.training,oldA=t.p.state.attempt;t.m.state.batch={learnAt:0};t.p.state.training={page:0,stage:'learn'};t.p.state.attempt={page:0};const before=nativeInspect().key;const f='"+f+"';if(f==='learnAt')t.m.state.batch.learnAt=1;else if(f==='attemptPage')t.p.state.attempt.page=1;else t.p.state.training[f]=f==='stage'?'answer':1;const different=before!==nativeInspect().key;t.m.state.batch=oldB;t.p.state.training=oldT;t.p.state.attempt=oldA;return different;})()");
 }
}
