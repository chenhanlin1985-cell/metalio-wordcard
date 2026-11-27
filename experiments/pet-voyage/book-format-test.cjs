'use strict';
// Run the same actions through Node and the device's QuickJS interpreter.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const dir=process.env.WORDCARD_LIBRARY, data={};
const fixtureEditions={'pet-2024':'1.0.0','gre-trial':'1.0.0','after-twenty-years':'1.1.0',
 'stone-soup':'1.1.0','pocket-receipt':'1.1.0','wrong-platform':'1.1.0','last-book-ferry':'1.1.0','rainy-lost-found':'1.1.0'};
for(const name of fs.readdirSync(dir)){
  for(const version of fs.readdirSync(path.join(dir,name))){
    if(fixtureEditions[name]!==version)continue;
    const folder=path.join(dir,name,version),manifest=path.join(folder,'manifest.json');
    if(!fs.existsSync(manifest))continue;
    data[name+'@'+version]={'manifest.json':JSON.parse(fs.readFileSync(manifest)),
      'content.json':JSON.parse(fs.readFileSync(path.join(folder,'content.json')))};
  }
}
function exercise(){
  const store={};
  globalThis.bookKeys=()=>Object.keys(books);
  globalThis.bookRead=(key,file)=>books[key][file];
  globalThis.nativeRead=key=>store[key]||null;
  globalThis.nativeWrite=(key,data)=>(store[key]=data,true);
  const expect=(ok,msg)=>{if(!ok)throw Error(msg);};
  const tap=act=>{
    const p=Books.view();
    expect([...p.blocks,...p.top,...p.tabs].some(x=>x.act===act),'missing '+act+' on '+p.title);
    Books.run(act);expect(!Books.status().notice,Books.status().notice);
  };
  const deckKey='pet-2024@1.0.0';
  const getRun=key=>JSON.parse(store['wordcard-library/1/'+deckKey]).runs[key];
  const base='stone-soup@1.1.0';
  // An additional one-chapter book with a new ID needs no player change.
  const tiny=JSON.parse(JSON.stringify(books[base])),m=tiny['manifest.json'],tinyBook=tiny['content.json'];
  m.id='one-scene';m.title='一段故事';tinyBook.chapters.length=1;delete tinyBook.album;delete tinyBook.art;
  const keys=tinyBook.chapters[0].words.map(row=>row.term);
  tinyBook.terms=Object.fromEntries(keys.map(k=>[k,tinyBook.terms[k]]));delete m.files['pages.wpix'];
  const tinyKey=m.id+'@'+m.version;books[tinyKey]=tiny;
  const old=JSON.parse(JSON.stringify(books[base]));
  old['manifest.json'].version='1.0.0';old['manifest.json'].schema=2;delete old['manifest.json'].storyType;
  books['stone-soup@1.0.0']=old;
  Books.shelf();
  const rows=Books.view().blocks.filter(x=>x.role==='library-row');
  expect(rows.length===7&&new Set(rows.map(x=>x.text)).size===7,'duplicate edition');
  expect(rows.filter(x=>x.detail==='分段学词').length===4&&rows.some(x=>x.detail==='阅读理解')&&rows.filter(x=>x.detail==='任务冒险').length===2,'unlabelled story');
  Books.run('resume/stone-soup@1.0.0');expect(Books.status().book===base,'old bookmark did not open the current edition');
  for(const [key,record] of Object.entries(books)){
    if(record['manifest.json'].storyType!=='vocabulary')continue;
    const book=record['content.json'],words=book.chapters.flatMap(c=>c.words);
    Books.shelf();tap('open/'+key);
    if(book.album){
      tap('album');tap('zh');tap('nextPage');tap('words');
      const entry=Books.view().blocks.find(x=>x.act?.startsWith('gloss/'));
      tap(entry.act);tap('back');tap('back');tap('back');
      expect(Books.status().route==='segments-cover','album changed activity');
    }
    tap('segments');
    let turns=0,firstError=false,retried=false;
    const answers=[],first=[],independent=[];
    while(Books.status().route!=='segments-done'){
      expect(++turns<400,'unbounded practice');
      const p=getRun(key).segments;
      if(p.stage==='read'){tap('segQuiz');continue;}
      if(p.stage==='think'){if(p.task[1]==='first')independent.push(p.task[0]);tap('segOptions');continue;}
      if(p.stage==='feedback'){tap('segNext');continue;}
      expect(p.stage==='choose','missing choices');
      const [id,kind]=p.task,row=words[id];
      const options=Books.view().blocks.filter(x=>x.act?.startsWith('segPick/'));
      expect(options.length===4&&options[p.slot].text.slice(3)===row.gloss,'wrong gloss');
      expect(!answers.slice(-3).includes(id),'repeat too soon');
      answers.push(id);
      if(kind==='first')first.push(id);
      if(kind==='retry')retried=true;
      tap('segPick/'+(!firstError?(p.slot+1)%4:p.slot));firstError=true;
      expect(Books.view().blocks.some(x=>x.text===row.gloss),'feedback lost sense');
      if(answers.length===1){
        const cursor=JSON.stringify(getRun(key));Books.shelf();tap('open/'+key);
        expect(JSON.stringify(getRun(key))===cursor,'wrong-answer bookmark changed');
      }
    }
    const run=getRun(key);
    expect(retried&&new Set(first).size===words.length&&Object.keys(run.segments.second).length===words.length,'incomplete practice');
    expect(independent.length===book.chapters.length*2,'each section needs two independent recalls');
    expect(Books.summary().done===book.chapters.length&&run.stamps.length===1,'completion');
  }
}
const bundle=require('./simulation-bundle.cjs');
const script=bundle+'\nconst books='+JSON.stringify(data)+';\n('+exercise.toString()+')();';
if(process.argv[2])fs.writeFileSync(process.argv[2],script);
vm.runInNewContext(script,{});
console.log('PASS labelled shelf, edition resolution, original illustrated pages and all vocabulary books including a new single-section book');
