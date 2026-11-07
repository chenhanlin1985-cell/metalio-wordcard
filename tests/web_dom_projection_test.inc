// Compare the compact native projection with the original web keyboard. Both
// execute in QuickJS; the original pinned scripts remain unchanged on disk.
{fs::create_directories(work/"dom-projection");LearningWeb m;req(m.init((work/"dom-projection").string()),m.error());run(m,setup+plan);
 run(m,"t.m.batchBegin(t.ids,{type:'course',id:'u1a'});t.m.batchSetMode('staged');t.m.batchStart();t.go('word-batch');");
 auto equivalent=[&]{yes(m,"(()=>{const before=t.m.export();globalThis.nativeCompactKeyboard=false;t.render();const original=JSON.stringify(nativeView());globalThis.nativeCompactKeyboard=true;t.render();return original===JSON.stringify(nativeView())&&before===t.m.export();})()");};
 equivalent();click(m,"lookup");equivalent();
 for(const char c:std::string("city")){req(m.key("key",std::string(1,c)),m.error());equivalent();}
 req(m.key("shift"),m.error());equivalent();req(m.key("symbols"),m.error());equivalent();req(m.key("symbols"),m.error());req(m.key("shift"),m.error());
 click(m,"lookup-return");for(int i=0;i<4;i++)run(m,"nativeAction({action:'w-learn',delta:1})");
 for(int i=0;i<4;i++)answer(m);equivalent();
 req(m.key("key","x"),m.error());equivalent();req(m.key("backspace"),m.error());equivalent();
 for(int i=0;i<4;i++)answer(m);equivalent();
 req(m.key("key","r"),m.error());equivalent();click(m,"lookup");equivalent();click(m,"lookup-return");req(page(m).typed=="r","lookup preserves typed input with compact projection");equivalent();
 yes(m,"(()=>{const a=new NativeNode('div'),b=new NativeNode('div');a.innerHTML='<p class=\"same\" data-value=\"a&amp;b\">one</p>';a.children[0].classList.add('changed');a.children[0].setAttribute('data-value','changed');b.innerHTML='<p class=\"same\" data-value=\"a&amp;b\">one</p>';return b.children[0].attrs.class==='same'&&b.children[0].dataset.value==='a&b'&&!b.querySelector('.changed');})()");
 yes(m,"(()=>{const n=new NativeNode('div');n.innerHTML='<p>A &amp; B <strong>中文</strong></p>';return n.querySelectorAll('p').length===1&&n.querySelectorAll('strong').length===1&&n.querySelector('p').textContent==='A & B 中文';})()");
 yes(m,"(()=>{for(let i=0;i<400;i++)nativeTag('<button data-value=\"'+i+'\">');const n=nativeTagCache.size,b=nativeTagBytes;nativeTag('<p data-value=\"'+'x'.repeat(3000)+'\">');return n<=128&&b<=16384&&nativeTagCache.size===n&&nativeTagBytes===b;})()");
 yes(m,"(()=>{const C=t.B.catalog,queries=['',' ','CITY',' FiRmWaRe ','中文','😀',...C.flatMap(w=>[w.word,w.word.slice(0,1),w.word.slice(0,3),w.word.slice(-2),w.word.slice(1,4)])];return queries.every(q=>JSON.stringify(nativeLookupMatches(q))===JSON.stringify(q.trim()?C.filter(w=>w.word.toLowerCase().includes(q.trim().toLowerCase())).slice(0,3):[]));})()");
 req(m.memory_bytes()<3*1024*1024,"bounded parsed-tag cache under host memory budget");
}
