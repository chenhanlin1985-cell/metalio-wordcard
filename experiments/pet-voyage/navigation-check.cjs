'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const root=path.resolve(__dirname,'../..');
const shelf=process.env.WORDCARD_LIBRARY||path.join(root,'work/wordcard-content/library');
const packages={};
const fixtureEditions={'pet-2024':'1.0.0','gre-trial':'1.0.0','after-twenty-years':'1.1.0',
 'stone-soup':'1.1.0','pocket-receipt':'1.1.0','wrong-platform':'1.1.0','last-book-ferry':'1.1.0','rainy-lost-found':'1.1.0'};
for(const id of fs.readdirSync(shelf)){
 for(const version of fs.readdirSync(path.join(shelf,id))){
  if(fixtureEditions[id]!==version)continue;
  const dir=path.join(shelf,id,version);
  if(!fs.existsSync(path.join(dir,'manifest.json')))continue;
  packages[id+'@'+version]=Object.fromEntries(['manifest.json','content.json'].map(file=>[file,JSON.parse(fs.readFileSync(path.join(dir,file),'utf8'))]));
 }
}
function exercise(){
 const store={legacy:'protected'};
 globalThis.bookKeys=()=>Object.keys(data);
 globalThis.bookRead=(k,f)=>data[k][f];
 globalThis.nativeRead=k=>store[k]||null;
 globalThis.nativeWrite=(k,v)=>(store[k]=v,true);
 const req=(ok,why)=>{if(!ok)throw Error(why+' '+JSON.stringify(Books.status()));};
 const click=act=>{const p=Books.view();req([...p.blocks,...p.top,...p.tabs].some(x=>x.act===act),act+' is not visible');Books.run(act);};
 const story='rainy-lost-found@1.1.0',deck='pet-2024@1.0.0',gre='gre-trial@1.0.0';
 const rows=()=>Books.view().blocks.filter(b=>b.role==='library-row');
 Books.installBuiltinRows(JSON.stringify([{id:'everyday',title:'日常核心'},{id:'notebook',title:'我的单词本'},{id:'gre-trial',title:'GRE',packKey:gre}]));
 Books.shelf();req(Books.status().route==='shelf','shelf');
 req(Books.view().tabs.map(x=>x.text).join('/')==='首页/内容库/复习/词典','roots');
 req(rows().length===Object.values(data).filter(x=>x['manifest.json'].type==='story').length&&rows().every(x=>data[x.act.slice(5)]['manifest.json'].type==='story'),'story category');
 click('open/'+story);req(Books.status().route==='brief','direct story');
 click('study');click('learn');const word=Books.status().word;
 click('back');req(Books.status().route==='shelf'&&Books.status().category==='story','back to source');
 click('category/deck');req(rows().some(x=>x.act==='builtin/notebook')&&rows().filter(x=>x.act.includes('gre-trial')).length===1,'merged decks');
 req(JSON.parse(store['content-selection']).key===story,'browsing preserves selection');
 Books.run('resume/'+story);req(Books.status().word===word&&Books.backRoot(),'resume exact word');
 Books.run('reviewOf/'+story);req(Books.status().route==='review'&&!Books.view().blocks.some(b=>b.act==='reviewStart'),'empty review');
 Books.run('resume/'+story);req(Books.status().word===word,'review preserves new task');
 Books.run('root/library');click('category/deck');click('open/'+gre);req(Books.status().route==='learn','deck opens at word');
 click('back');click('open/'+deck);req(Books.status().route==='learn','PET deck');
 const latest=store['content-selection'];Books.shelf('story/0');
 const saved=data[deck];delete data[deck];Books.shelf('story/0');click('open/'+story);
 req(Books.status().route==='shelf'&&Books.status().notice.includes(deck)&&store['content-selection']===latest,'missing dependency');
 data[deck]=saved;Books.shelf();
 const storeName='wordcard-library/1/'+deck,record=JSON.parse(store[storeName]);
 const id=Object.keys(saved['content.json'].words).at(-1);
 record.memory.cards[id]={meaning:{due:1}};store[storeName]=JSON.stringify(record);
 Books.run('reviewOf/'+story);click('reviewStart');
 req(Books.status().word===id&&!Books.backRoot(),'back from active review is local');
 click('back');req(Books.status().route==='review'&&Books.backRoot(),'back from review overview is global');
 req(store.legacy==='protected','legacy');
 const extras=Array.from({length:7},(_,i)=>({id:'extra-'+i,title:'额外词库 '+i}));
 Books.installBuiltinRows(JSON.stringify(extras.slice(0,6)));
 Books.shelf('deck/0');req(rows().length===8&&!Books.view().blocks.some(b=>b.act==='moreBooks'),'eight entries on one page');
 Books.installBuiltinRows(JSON.stringify(extras));Books.shelf('deck/0');
 req(rows().length===8,'overflow page capacity');click('moreBooks');
 req(rows().length===1&&rows()[0].act==='builtin/extra-6','ninth item reachable');click('prevBooks');
 req(rows().length===8,'page back');
 Books.shelf('story/0');click('open/'+story);
 req(Books.status().route!=='shelf','valid story still opens');
 Books.shelf('deck/0');click('open/'+gre);
 const deckRecord=()=>JSON.parse(store['wordcard-library/1/'+gre]);
 const group=()=>deckRecord().runs[gre].session;
 const earned=()=>deckRecord().runs[gre].stamps||[];
 const ids=group().ids,initialTasks=JSON.stringify(group().tasks),initialCards=JSON.stringify(deckRecord().memory.cards);
 const position=i=>req(Books.status().word===ids[i]&&Books.status().route==='learn'&&Books.view().title.includes((i+1)+' / '+ids.length),'wrong card');
 position(0);Books.run('learnPrev');position(0);Books.run('startQuiz');position(0);
 for(let i=1;i<ids.length;i++){click('learn');position(i);}
 Books.run('learn');position(ids.length-1);
 for(let i=ids.length-2;i>=0;i--){
  click('learnPrev');position(i);
  if(i===2){click('back');click('open/'+gre);position(i);}
 }
 for(let i=1;i<ids.length;i++){click('learn');position(i);}
 req(JSON.stringify(group().tasks)===initialTasks&&JSON.stringify(deckRecord().memory.cards)===initialCards&&!group().attempts&&!earned().length,'browsing graded or rebuilt the group');
 for(let group=1;group<=2;group++){
  while(Books.status().route==='learn')click(Books.view().blocks.some(b=>b.act==='startQuiz')?'startQuiz':'learn');
  while(Books.status().route==='ask'){
   click('pick/'+Books.status().word);
   req(earned().length===group-1,'unfinished group got a stamp');
   click('advance');
  }
  req(Books.status().route==='result'&&earned().length===group&&earned()[group-1].words.length===5,'completed deck group');
  Books.run('resume/'+gre);req(earned().length===group,'reopened result issued another stamp');
  click('resultNext');
 }
}
const bundle=require('./simulation-bundle.cjs');
const test='const data='+JSON.stringify(packages)+'; ('+exercise.toString()+')();';
if(process.argv[2])fs.writeFileSync(process.argv[2],bundle+'\n'+test);
vm.runInNewContext(bundle+'\n'+test,{});
console.log('PASS catalog, direct launch, exact return, review separation, missing dependency');
