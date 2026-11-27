'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const root=path.resolve(__dirname,'../..'),lib=process.env.WORDCARD_LIBRARY||path.join(root,'work/wordcard-content/library');
const book='pocket-receipt@1.0.0',deck='pet-2024@1.0.0',data={};
for(const key of [book,deck]){
  data[key]={};
  for(const name of ['content.json','manifest.json'])data[key][name]=JSON.parse(fs.readFileSync(path.join(lib,...key.split('@'),name),'utf8'));
}
function suite(data,book,deck){
  const expected=['音乐会','音乐','顾客','袖子','大衣','羊毛','衣物标签','尺码','店员','收据','包裹','生日','日期','价钱为','现金','找回的零钱','小钱包','信用卡','归还','乐器'];
  const terms=['concert','music','customer','sleeve','coat','wool','label','size','shop assistant','receipt','parcel','birthday','date','cost','cash','change','purse','credit card','return','instrument'];
  const key='wordcard-library/1/'+deck;
  function check(v,msg){if(!v)throw Error(msg+' '+JSON.stringify(Books.status()));}
  const buttons=()=>[...Books.view().top,...Books.view().blocks].filter(b=>b.act);
  const click=cmd=>{check(buttons().some(b=>b.act===cmd),'not visible '+cmd);Books.run(cmd);check(!Books.status().notice,'save '+cmd);};
  globalThis.bookKeys=()=>[book,deck];
  globalThis.bookRead=(id,name)=>data[id][name];
  for(const scenario of ['perfect','mistakes','allUnknown']){
    const issued={},firstSlots=[],secondSlots=[],seen=[],lessons=[],retries=[],oral=[];
    const old={phase:'recall',chapter:0,p:5,q:0,wordAt:4,choice:false,draft:'',complete:scenario==='perfect',mode:'normal',roundMode:'normal',skills:{},practice:{first:{sleeve:{correct:false}},pending:[]},stamps:[{key:'story',at:10}]};
    const card={memory:{difficulty:3,stability:4},attempts:8,due:777,last:1,target:777};
    const id=data[book]['content.json'].terms.receipt.id;
    const record={schema:1,deck:data[deck]['manifest.json'].files['content.json'],rev:3,memory:{cards:{[id]:{meaning:card}},practised:{},recent:[]},runs:{[book]:old,other:{preserved:1}}};
    const storage={[key]:JSON.stringify(record)};
    let fail=false,reopened=false,afterEnd=false;
    globalThis.nativeRead=n=>storage[n];globalThis.nativeWrite=(n,v)=>{if(fail&&n===key)return false;storage[n]=v;return true;};
    const state=()=>JSON.parse(storage[key]).runs[book].segments;
    Books.shelf();click('open/'+book);check(storage[key]===JSON.stringify(record),'opening modified progress');
    click('segments');let guard=0;
    while(++guard<280){
      const p=state();
      if(p.stage==='done'){
        if(scenario==='allUnknown'&&!afterEnd){check(p.later.length===20,'unresolved tail');afterEnd=true;click('segLater');continue;}
        break;
      }
      if(p.stage==='read'){
        check(p.lesson===lessons.length,'scene order');lessons.push(p.lesson);
        const copy=Books.view().blocks[0].text.toLowerCase();
        check(terms.slice(p.lesson*4,p.lesson*4+4).every(w=>copy.includes(w)),'missing contextual target');
        click('segZh');click('segZh');click('segQuiz');continue;
      }
      const [wid,kind]=p.task,term=terms[wid];
      check(Books.status().word===term,'diagnostic term');
      if(kind==='first')check(Math.floor(wid/4)===lessons.length-1,'delayed first practice');
      check(!seen.slice(-3).includes(wid),'too few intervening terms');
      if(kind==='retry')retries.push(wid);
      if(p.stage==='think'){
        check(!Books.view().blocks.some(b=>b.text.includes(expected[wid])),'meaning leaked');
        if(kind==='first')oral.push(wid);click('segOptions');
      }
      const choices=buttons().filter(b=>b.act.startsWith('segPick/'));
      check(choices.length===4&&new Set(choices.map(b=>b.text.slice(3))).size===4,'four distinct Chinese options');
      const correct=choices.find(b=>b.text.slice(3)===expected[wid]);
      check(correct,'missing answer');const slot=Number(correct.act.slice(-1));
      if(kind==='first')firstSlots.push(slot);
      if(kind==='second')secondSlots.push(slot);
      if(!reopened&&kind==='first'){
        const before=JSON.stringify(Books.view());click('back');click('open/'+book);
        check(JSON.stringify(Books.view())===before,'changed after reopen');
        fail=true;Books.run(correct.act);fail=false;check(Books.status().notice&&Books.status().choice===null,'failed write accepted');
        reopened=true;
      }
      // Audit the grade for every visible alternative from identical stored state.
      const snap=storage[key];
      for(const opt of choices){
        Books.run(opt.act);
        check(Books.status().choice===(opt.act===correct.act),'wrong grading '+opt.text);
        storage[key]=snap;Books.shelf();Books.run('open/'+book);
      }
      const miss=scenario==='allUnknown'&&!afterEnd||scenario==='mistakes'&&((kind==='first'&&wid===0)||(kind==='second'&&wid===19));
      const selected=miss?choices.find(b=>b!==correct):correct;
      click(scenario==='allUnknown'&&!afterEnd?'segUnknown':selected.act);
      const feedback=Books.view();
      check(feedback.blocks.some(b=>b.text===expected[wid])&&feedback.blocks.some(b=>b.text.toLowerCase().includes(term)&&b.text!==term),'feedback clue');
      if(miss&&scenario==='mistakes')check(feedback.blocks.some(b=>b.text.includes(selected.text.slice(3))),'selected error lost');
      check(Books.status().choice===!miss,'wrong feedback');
      (issued[kind]??=[]).push(wid);seen.push(wid);click('segNext');
    }
    check(guard<280&&lessons.length===5&&oral.length===10&&new Set(oral).size===10,'sequence or count');
    check(new Set(issued.first).size===20&&issued.first.length===20&&new Set(issued.second).size===20&&issued.second.length===20,'every word twice');
    if(scenario==='perfect')check(seen.length===40&&[0,1,2,3].every(i=>firstSlots.filter(x=>x===i).length===5&&secondSlots.filter(x=>x===i).length===5),'answer positions');
    if(scenario==='mistakes')check(retries.join(',')==='0,19'&&!state().first[0].correct&&state().retries[0].correct,'retry tail');
    const final=JSON.parse(storage[key]);
    check(final.runs[book].practice.first.sleeve.correct===false&&final.runs[book].stamps.length===1&&final.runs.other.preserved===1,'legacy state');
    check(JSON.stringify(final.memory.cards[id].meaning)===JSON.stringify(card)&&Object.values(final.memory.cards).flatMap(Object.values).every(c=>c.attempts===0||c.attempts===8),'FSRS discrepancy');
    click('deckReview');check(Books.status().book===deck,'PET review');
  }
}
const bundle=require('./simulation-bundle.cjs');
const script='('+suite.toString()+')('+JSON.stringify(data)+','+JSON.stringify(book)+','+JSON.stringify(deck)+');';
if(process.argv[2])fs.writeFileSync(process.argv[2],bundle+'\n'+script);
vm.runInNewContext(bundle+'\n'+script,{});
console.log('PASS five scenes, 20 words twice, individual options, balanced keys, legacy records, spaced retry and all-unknown recovery');
