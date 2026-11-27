'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {createGame,spell}=require('./game.cjs'),{page}=require('./pages.cjs');
const library=require('./library.cjs').openLibrary(process.env.WORDCARD_LIBRARY||path.resolve(__dirname,'../../work/wordcard-content/library'));
const pack=library.compose('last-book-ferry@1.0.0'),gates=pack.gates.map(x=>x.test);
const g=createGame(pack),W=pack.words,cs=pack.chapters,now=1800000000000,ends=new Set();
assert.equal(Object.keys(W).length,1871);
function solve(s,wrong=false){
 const q=s.session,items=[];
 while(q.phase==='learn')g.studyNext(s,now);
 for(let n=0;s.session&&n<50;n++){
  const t=g.current(s);assert(W[t.id]);const sol=t.k==='partial'?spell(W[t.id].w).answer:W[t.id].w;
  if(t.k==='meaning'){const opts=g.options(s);assert.equal(opts.length,3);assert(opts.includes(t.id));assert.equal(new Set(opts.map(i=>W[i].w)).size,3);}
  if(t.retry){const at=items.lastIndexOf(t.id);assert(at>=0&&new Set(items.slice(at+1)).size>=3);}
  items.push(t.id);g.grade(s,wrong&&n===0?'':sol,now);assert(s.session.feedback.ok===(n!==0||!wrong));
  const attempts=s.session.attempts;g.grade(s,sol,now);assert.equal(s.session.attempts,attempts);g.advance(s,now);
 }
 assert.equal(s.session,null);assert.equal(s.scene,'result');
}
for(const route of ['A','B'])for(const legacy of ['A','B']){
 const s=g.fresh();s.mode=route==='B'?(legacy==='B'?'hard':'normal'):'easy';
 for(let i=0;i<20;i++){
  s.scene='home';g.start(s,now);assert.equal(s.scene,'brief');g.study(s,i,cs[i].core,'main');
  if(i===5)assert.equal(W[s.session.tasks.find(t=>t.callback).id].w,route==='A'?'backpack':'parcel');
  if(i===15)assert.equal(W[s.session.tasks.find(t=>t.callback).id].w,legacy==='A'?'repair':'message');
  if(i===19)assert.equal(W[s.session.tasks.find(t=>t.callback).id].w,legacy==='A'?'shelf':'audience');
  solve(s,i===2);assert.equal(s.items.length,i+1);
  if((i+1)%5===0){
   s.scene='home';g.start(s,now);assert.equal(s.scene,'gate');
   const k=s.gates,x=gates[k],old=JSON.stringify(s.cards);g.showEvidence(s,x[3].find(i=>i!==x[2]));assert.equal(s.scene,'inspect');
   s.scene='gate';g.showEvidence(s,x[2]);assert.equal(s.gates,k+1);assert.equal(JSON.stringify(s.cards),old);
   g.start(s,now);if(i===4||i===14){assert.equal(s.scene,'fork');g.choose(s,i===4?route:legacy);}
  }
 }
 assert.equal(s.scene,'ending');ends.add(g.ending(s)[0]);assert.equal(Object.keys(s.practised).length,80);
 if(route==='A'&&legacy==='A'){
  let batches=0;s.mode='easy';
  for(let i=0;i<20;i++){for(;;){g.side(s,i);if(!s.session)break;solve(s);batches++;assert(batches<=500);}}
  assert.equal(Object.keys(s.practised).length,1871);
  s.scene='home';g.start(s,now+31*864e5);assert.equal(s.scene,'ending');
  g.start(s,now+31*864e5,true);assert.equal(s.session.type,'review');assert(s.session.tasks.every(t=>W[t.id]));
  console.log('Full deck',batches,'side commissions; all 1871 IDs exercised');
 }
}
assert.equal(ends.size,4);
{
 const s=g.fresh();s.mode='normal';g.start(s,now);g.study(s,0,cs[0].core,'main');
 while(s.scene==='learn')g.studyNext(s,now);
 s.session.draft='q';const original=JSON.stringify(s.session);
 const id=cs[1].core[0];s.cards[id]={meaning:{due:now-1}};
 g.start(s,now,true);assert.equal(s.session.type,'review');assert.equal(g.current(s).id,id);
 s.session.draft='r';const review=JSON.stringify(s.session);
 g.start(s,now);assert.equal(JSON.stringify(s.session),original);
 g.start(s,now,true);assert.equal(JSON.stringify(s.session),review);
}
console.log('PASS: four endings, three modes, twenty linked chapters, four evidence gates, opposing branches, independent review/study drafts, FSRS and full-deck coverage.');
