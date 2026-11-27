'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const root=path.resolve(__dirname,'../..'),base=process.env.WORDCARD_LIBRARY||path.join(root,'work/wordcard-content/library');
const story='after-twenty-years@1.0.0',deck='pet-2024@1.0.0',data={};
for(const k of [story,deck])data[k]=Object.fromEntries(['manifest.json','content.json'].map(f=>[f,JSON.parse(fs.readFileSync(path.join(base,...k.split('@'),f),'utf8'))]));
function test(mode){
 const story='after-twenty-years@1.0.0',deck='pet-2024@1.0.0',name='wordcard-library/1/'+deck,book=data[story]['content.json'];
 const keep={memory:{stability:4,difficulty:5},last:100,due:4102444800000,target:4102444800000,attempts:5};
 const storage={[name]:JSON.stringify({schema:1,rev:3,deck:data[deck]['manifest.json'].files['content.json'],memory:{cards:{'pet-0080':{meaning:keep}},practised:{},recent:[]},runs:{'some-other-story':{go:1}}})};
 const assert=(cond,msg)=>{if(!cond)throw Error(mode+' '+msg+' '+JSON.stringify(Books.status()));};
 globalThis.bookKeys=()=>Object.keys(data);globalThis.bookRead=(k,f)=>data[k][f];globalThis.nativeRead=k=>storage[k];globalThis.nativeWrite=(k,v)=>(storage[k]=v,true);
 const click=a=>{const v=Books.view();assert([...v.top,...v.blocks,...v.tabs].some(b=>b.act===a),a+' unavailable');Books.run(a);assert(!Books.status().notice,'notice');};
 Books.shelf();click('open/'+story);click('mode');click('mode/'+mode);click('startRead');
 let total=0,qs=0;
 for(let i=0;i<book.chapters.length;i++){
  const c=book.chapters[i];assert(Books.status().chapter===i,'chapter');click('zh');click('zh');
  for(let j=1;j<c.pages.length;j++)click('nextPage');
  click('recall');
  while(Books.status().route==='recall'){
   const s=Books.status(),word=s.word,pg=Books.view();
   if(mode==='easy')click('pick/'+word);
   else{
    const answer=mode==='hard'?word:Array.from(pg.mask).flatMap((ch,n)=>ch==='_'?[word[n]]:[]).join('');
    Books.run('type:'+answer.slice(0,1));click('peek');click('back');assert(Books.view().draft===answer.slice(0,1),'peek draft');
    click('back');click('open/'+story);assert(Books.view().draft===answer.slice(0,1),'resume draft');
    Books.run('type:'+answer);Books.run('submit');
   }
   assert(Books.status().choice===true,'real recall');click('advance');total++;
  }
  assert(Books.status().route==='actDone','recall before plot');
  if(mode==='normal'||(i===0&&mode==='hard')){
   click('plot');
   for(let j=0;j<c.questions.length;j++){
    const q=c.questions[j],blocks=Books.view().blocks;
    q.options.forEach((label,n)=>{
     const letter=String.fromCharCode(65+n);
     assert(blocks.some(x=>x.kind==='text'&&x.text===letter+'   '+label),'missing full choice');
     assert(blocks.some(x=>x.text===letter&&x.role==='answer-letter'&&x.act==='answer/'+n),'short choice');
    });
    click('peek');click('back');assert(Books.status().question===j,'original question');
    click('answer/'+q.answer);click('peek');assert(Books.status().peek===q.evidence,'evidence');click('back');
    click('back');click('open/'+story);assert(Books.status().choice===q.answer,'selected answer persisted');
    click('plotNext');qs++;
   }
  }
  assert(Object.keys(JSON.parse(storage[name]).memory.cards).length===1,'premature FSRS');click('nextAct');
 }
 assert(total===8&&(mode!=='normal'||qs===7)&&Books.status().complete,'full book');
 const record=JSON.parse(storage[name]);assert(JSON.stringify(record.memory.cards['pet-0080'].meaning)===JSON.stringify(keep),'future card changed');
 assert(record.runs[story].stamps.length===1&&record.runs[story].stamps[0].kind==='story','one completion stamp');
 assert(record.runs['some-other-story'].go===1,'other book changed');
 for(const t of Object.values(book.terms))if(t.id){
  const card=record.memory.cards[t.id];assert(card.meaning,'meaning not admitted');
  if(t.id!=='pet-0080')assert(card.meaning.attempts===0,'fake grade');
  if(mode!=='easy')assert(card[mode==='hard'?'recall':'partial'].attempts===0,'spelling channel');
 }
 click('deckReview');assert(Books.status().book===deck&&Books.status().route==='review','PET queue');
 click('back');assert(Books.status().complete,'return from PET');
 assert(JSON.parse(storage[name]).runs[story].stamps.length===1,'reading or review return duplicated the award');
 Books.run('reviewOf/'+story);click('reviewStart');assert(Books.status().book===deck&&Books.status().route==='ask','normal PET review');
}
const bundle=require('./simulation-bundle.cjs');
const suite='const data='+JSON.stringify(data)+'; for(const mode of ["easy","normal","hard"])('+test.toString()+')(mode);';
if(process.argv[2])fs.writeFileSync(process.argv[2],bundle+'\n'+suite);
vm.runInNewContext(bundle+'\n'+suite,{});
console.log('PASS 3 recall modes, 5 acts, 8 words, optional questions, return, drafts, admission and PET review');
