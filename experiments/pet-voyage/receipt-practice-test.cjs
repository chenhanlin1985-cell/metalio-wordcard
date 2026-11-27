'use strict';
const fs=require('node:fs'), path=require('node:path'), vm=require('node:vm');
const root=path.resolve(__dirname,'../..'),lib=process.env.WORDCARD_LIBRARY||path.join(root,'work/wordcard-content/library');
const data={};
for(const id of ['pet-2024','pocket-receipt','wrong-platform','stone-soup']){
  const row=data[id+'@1.0.0']={};
  for(const name of ['manifest.json','content.json'])row[name]=JSON.parse(fs.readFileSync(path.join(lib,id,'1.0.0',name),'utf8'));
}
function run(mode,oldRun){
  const book='pocket-receipt@1.0.0',deck='pet-2024@1.0.0',key='wordcard-library/1/'+deck;
  const words=data[book]['content.json'].chapters[0].recallOrder;
  const termData=data[book]['content.json'].terms,receiptId=termData.receipt.id;
  const keep={memory:{stability:4,difficulty:5},last:123,due:4102444800000,target:4102444800000,attempts:5};
  const record={schema:1,rev:1,deck:data[deck]['manifest.json'].files['content.json'],memory:{cards:{[receiptId]:{meaning:keep}},recent:[],practised:{}},runs:{other:{unchanged:1}}};
  if(oldRun)record.runs[book]={phase:'done',p:5,chapter:0,q:0,wordAt:20,choice:null,draft:'',complete:true,mode,roundMode:mode,skills:{},stamps:[{key:'story',at:123}]};
  let tick=10000000;
  const beforeNow=Date.now;
  Date.now=()=>++tick;
  const storage={[key]:JSON.stringify(record)};
  globalThis.bookKeys=()=>Object.keys(data);
  globalThis.bookRead=(id,file)=>data[id][file];
  globalThis.nativeRead=id=>storage[id];
  let fail=false;
  globalThis.nativeWrite=(id,v)=>{if(fail&&id===key)return false;storage[id]=v;return true;};
  const state=()=>JSON.parse(storage[key]).runs[book];
  const check=(ok,msg)=>{if(!ok)throw Error(mode+' '+msg+' '+JSON.stringify(Books.status()));};
  const buttons=()=>[...Books.view().top,...Books.view().blocks].map(b=>b.act).filter(Boolean);
  const click=cmd=>{check(buttons().includes(cmd),'invisible '+cmd);Books.run(cmd);check(!Books.status().notice,'failed '+cmd);};
  const resume=()=>{click('back');click('open/'+book);click('legacy');};
  Books.shelf();click('open/'+book);
  click('legacy');
  if(oldRun)click('tryRecall');
  else{click('mode');click('mode/'+mode);click('startRead');for(let i=1;i<6;i++)click('nextPage');click('recall');}
  const initial=[],retry=[],oral=[0,0,0,0];
  let rounds=0;
  while(Books.status().route!=='done'){
    check(++rounds<45,'loop');
    if(['group','ready'].includes(Books.status().route))click('startGroup');
    let term=Books.status().word;
    const again=!!state().practice.retry,open=buttons().includes('revealMeaning');
    if(again){
      const at=initial.indexOf(term);
      check(at!==-1&&new Set(initial.slice(at+1).concat(retry).filter(w=>w!==term)).size>=3,'retry too early');
      retry.push(term);
    }else{initial.push(term);if(open)oral[Math.floor((initial.length-1)/5)]++;}
    if(open){
      check(Books.view().blocks.some(b=>b.text===term)&&!Books.view().blocks.some(b=>b.text===termData[term].meaning||b.text===termData[term].cue||b.act?.startsWith('pick/')),'prompt exposes answer');
      if(!again&&term==='receipt'){
        fail=true;Books.run('peek');fail=false;
        check(Books.status().route==='recall'&&!state().practice.hinted,'failed hint write revealed the book');
        click('peek');click('back');click('revealMeaning');
        check(state().practice.hinted,'peek forgotten');resume();
        fail=true;Books.run('selfYes');fail=false;
        check(!state().practice.first.receipt&&state().practice.revealed,'failed write not rolled back');
        click('selfYes');
      }else if(!again&&term==='sleeve')click('unknown');
      else {click('revealMeaning');click(again&&term==='sleeve'?'selfNo':'selfYes');}
    }else if(term==='coat'){
      if(mode==='easy')click('unknown');
      else{Books.run('type:xx');Books.run('submit');}
    }
    else if(mode==='easy'){
      if(term==='cash'){click(buttons().find(a=>a.startsWith('pick/')&&a!=='pick/'+term));check(state().practice.first.cash.selected,'wrong option');}
      else click('pick/'+term);
    }else{
      const pg=Books.view(),value=mode==='hard'?term:[...pg.mask].flatMap((ch,i)=>ch==='_'?[term[i]]:[]).join('');
      check(pg.keyboard,'spelling mode lost');Books.run('type:'+value);Books.run('submit');
    }
    check(typeof Books.status().choice==='boolean','missing feedback');click('advance');
  }
  check(initial.join('|')===words.join('|')&&oral.every(x=>x===2),'first-pass order / distribution');
  check(state().practice.first.receipt.assisted&&state().practice.retries.receipt.correct,'first result overwritten');
  check(!state().practice.first.sleeve.correct&&!state().practice.retries.sleeve.correct,'unresolved retry');
  check(!state().practice.retries.coat,'tail immediately repeated');resume();click('retryLater');check(Books.status().route==='retryWait','gap lost at end');click('back');
  tick+=900001;click('retryLater');check(Books.status().word==='coat','wrong deferred term');
  click('revealMeaning');resume();click('selfYes');click('advance');
  const end=JSON.parse(storage[key]);
  check(end.runs[book].practice.pending.length===0&&end.runs[book].stamps.length===1,'retries or stamps duplicated');
  check(JSON.stringify(end.memory.cards[receiptId].meaning)===JSON.stringify(keep)&&end.runs.other.unchanged===1,'collateral write');
  check(Object.values(end.memory.cards).flatMap(x=>Object.values(x)).every(x=>x.attempts===0||JSON.stringify(x)===JSON.stringify(keep)),'fake FSRS grade');
  click('deckReview');check(Books.status().book===deck,'PET review entry');
  for(const id of ['stone-soup','wrong-platform']){
    Books.shelf();click('open/'+id+'@1.0.0');click('startRead');for(let i=1;i<6;i++)click('nextPage');click('recall');click('startGroup');
    check(!buttons().includes('revealMeaning')&&buttons().some(a=>a.startsWith('pick/')),'other book changed');
  }
  Date.now=beforeNow;
}
const bundle=require('./simulation-bundle.cjs');
const suite='const data='+JSON.stringify(data)+';for(const mode of ["easy","normal","hard"])('+run.toString()+')(mode,mode==="hard");';
if(process.argv[2])fs.writeFileSync(process.argv[2],bundle+'\n'+suite);
vm.runInNewContext(bundle+'\n'+suite,{});
console.log('PASS receipt pilot in all modes, spacing, restore, previous completion, old books and FSRS preservation');
