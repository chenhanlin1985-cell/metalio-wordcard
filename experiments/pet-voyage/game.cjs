'use strict';
const {observe}=require('./story-ledger.cjs');
const GAP=900000;
const {award}=require('./stamps.cjs');
const modes={easy:'简单',normal:'普通',hard:'困难'};
const norm=s=>s.trim().toLowerCase().replace(/\s+/g,' ');
function hash(s){let n=2166136261;for(const ch of s)n=Math.imul(n^ch.charCodeAt(0),16777619)>>>0;return n;}
function spell(w){
 const pos=Array.from(w).map((ch,i)=>/[a-z]/i.test(ch)?i:-1).filter(i=>i>=0);
 const mask=pos.length>=4?pos.slice(1,3):pos.slice(0,Math.min(2,pos.length-1));
 return {mask:Array.from(w).map((c,i)=>mask.includes(i)?'_':c).join(''),answer:mask.map(i=>w[i]).join('')};
}
function createGame(pack){
 const {words,chapters,authored}=pack,all=Object.keys(words);
 const gates=pack.gates||[], forks=pack.forks||[], total=chapters.length;
 const pendingGate=s=>gates[s.gates]?.after<=s.chapter;
 const pendingFork=s=>forks.find(f=>f.after<=s.chapter&&!s.forks[f.key]);
 const actAt=i=>Math.max(0,pack.acts.findLastIndex(a=>a.start<=i));
 const topics=chapters.map(c=>all.filter(id=>words[id].topic===c.topic));
 function fresh(){return {version:1,deck:pack.deckSha256,chapter:0,mode:'easy',forks:{},gates:0,items:[],practised:{},cards:{},recent:[],session:null,scene:'home',result:null};}
 function kind(id,mode){if(mode==='normal'&&spell(words[id].w).answer)return 'partial';if(mode==='hard'&&/^[A-Za-z]+(?:[ '-][A-Za-z]+)*$/.test(words[id].w))return 'recall';return 'meaning';}
 function eligible(s,id,now){const i=s.recent.findLastIndex(e=>e[0]===id);return i<0||now-s.recent[i][1]>=GAP||new Set(s.recent.slice(i+1).map(e=>words[e[0]].w).filter(w=>w!==words[id].w)).size>=3;}
 function touch(s,id,now){s.recent.push([id,now]);if(s.recent.length>60)s.recent.shift();}
 function due(s,now){const out=[];for(const [id,skills] of Object.entries(s.cards))for(const [k,c] of Object.entries(skills)){
  if((k==='meaning'||k===kind(id,s.mode))&&c.due<=now&&eligible(s,id,now))out.push({id,k,due:c.due});
 }return out.sort((a,b)=>a.due-b.due);}
 function session(s,type,ids,tasks,chapter){s.result=null;s.session={type,chapter,ids,tasks,mode:s.mode,at:0,draft:'',phase:ids.length?'learn':'ask',right:0,attempts:0,deferred:0};s.scene=s.session.phase;}
 function ret(s,i){const choices=chapters[i].returns;return choices.both||choices[s.forks[chapters[i].branchKey]];}
 function reviewRows(s,now){
  const learning=s.session?.type==='review'?s.parkedStudy?.session:s.session;
  const reserved=new Set((learning?.tasks||[]).map(t=>t.id));
  return due(s,now).filter(t=>!reserved.has(t.id));
 }
 function start(s,now,forceReview=false){
  if(forceReview){
   if(s.session?.type==='review'){s.scene=s.session.phase;return;}
   const rows=reviewRows(s,now).slice(0,5);
   if(!s.parkedReview&&!rows.length)return;
   if(!s.parkedStudy)s.parkedStudy={session:s.session,scene:s.scene,result:s.result};
   if(s.parkedReview){s.session=s.parkedReview;delete s.parkedReview;s.scene=s.session.phase;s.result=null;}
   else session(s,'review',[],rows.map(({id,k})=>({id,k})),Math.min(s.chapter,total-1));
   return;
  }
  if(s.session?.type==='review'){s.parkedReview=s.session;s.session=null;}
  if(s.parkedStudy){Object.assign(s,s.parkedStudy);delete s.parkedStudy;if(!s.session&&['inspect','crossing','fork','gate','ending','brief'].includes(s.scene))return;}
  if(s.session){s.scene=s.session.phase;return;}
  if(s.scene==='result')return;
  if(pendingGate(s)){s.scene='gate';return;}
  if(pendingFork(s)){s.scene='fork';return;}
  if(pack.plain){
   const i=topics.findIndex(ids=>ids.some(id=>!s.practised[id]));
   if(i===-1)s.scene='completed';else side(s,i);
   return;
  }
  if(s.chapter===total){s.scene='ending';award(pack,s,'story',[],[],now);return;}
  s.scene='brief';
 }
 function study(s,i,ids,type){
  if(s.session||!ids.length)return;
  const tasks=ids.map(id=>({id,k:kind(id,s.mode),...(type==='main'?{cue:chapters[i].prompts[id]}:{})})).sort((a,b)=>hash(a.id+':'+i)-hash(b.id+':'+i));
  if(type==='main'&&Object.keys(chapters[i].returns).length){const [id,en,zh]=ret(s,i);tasks.splice(2,0,{id,k:kind(id,s.mode),cue:[en,zh],callback:true});}
  session(s,type,ids,tasks,i);
 }
 function side(s,i){
  if(i<0||i>=total||(!pack.plain&&i>s.chapter)||s.session)return;
  const used=new Set();const ids=topics[i].filter(id=>{
   if(chapters[i].core.includes(id)||s.practised[id]||used.has(norm(words[id].w)))return false;
   used.add(norm(words[id].w));return true;
  }).slice(0,5);study(s,i,ids,'side');
 }
 function current(s){return s.session?.tasks[0];}
 function spaceNext(s,now){
  const tasks=s.session.tasks;
  const i=tasks.findIndex(t=>eligible(s,t.id,now));
  if(i>0)tasks.unshift(tasks.splice(i,1)[0]);
 }
 function studyNext(s,now,step=1){
  const q=s.session;
  if(q?.phase!=='learn'||(step<0&&q.at===0))return;
  if(typeof NATIVE_STORY!=='undefined'&&NATIVE_STORY)require('./story-ledger.cjs').expose(s,q.ids[q.at]);
  touch(s,q.ids[q.at],now);
  q.at+=step;
  if(q.at===q.ids.length){spaceNext(s,now);s.scene=q.phase='ask';}
 }
 function cue(t){return t.cue||authored[t.id]||null;}
 function options(s){
  const t=current(s),w=words[t.id],same=topics.find(ids=>ids.includes(t.id))||all;const seed=t.id+':'+s.session.attempts;
  const seen=new Set([norm(w.w)]),list=[t.id];
  const authoredGroup=authored[t.id]?chapters.find(c=>c.core.includes(t.id)).core:[];
  const ranked=ids=>[...ids].sort((a,b)=>hash(seed+a)-hash(seed+b));
  for(const id of [...ranked(authoredGroup),...ranked(same),...(same.length<4?ranked(all):[])]){
   const value=norm(words[id].w);if(seen.has(value)||words[id].zh===w.zh)continue;seen.add(value);list.push(id);if(list.length===3)break;
  }
  return list.sort((a,b)=>hash(a+seed)-hash(b+seed));
 }
 function grade(s,value,now){
  const q=s.session;if(q?.phase!=='ask')return;
  const t=current(s),w=words[t.id],expected=t.k==='partial'?spell(w.w).answer:w.w;
  const ok=norm(value)===norm(expected);
  if(typeof NATIVE_STORY!=='undefined'&&NATIVE_STORY)observe(s,t.id,t.k,ok,!!t.assisted,!!t.retry);
  else require('./simulation-memory.cjs')(s,t,ok,now);
  touch(s,t.id,now);s.practised[t.id]=true;q.attempts++;if(ok)q.right++;
  if(!ok&&!t.retry)q.tasks.push({...t,retry:true,assisted:false});
  if(!ok&&t.retry)q.deferred++;
  q.feedback={ok,value};q.phase=s.scene='feedback';q.draft='';
 }
 function advance(s,now){
  const q=s.session;if(q?.phase!=='feedback')return;
  q.tasks.shift();delete q.feedback;
  spaceNext(s,now);
  while(q.tasks.length&&q.tasks[0].retry&&!eligible(s,q.tasks[0].id,now)){q.tasks.shift();q.deferred++;}
  if(q.tasks.length){q.phase=s.scene='ask';return;}
  s.result={type:q.type,chapter:q.chapter,right:q.right,attempts:q.attempts,deferred:q.deferred};
  if(pack.plain&&q.type==='side'&&q.attempts>0){
   award(pack,s,'group',q.ids,q.ids.map(id=>words[id].w),now);
  }
  if(q.type==='main'&&q.chapter===s.chapter){s.items.push(chapters[q.chapter].token);s.chapter++;}
  s.session=null;s.scene='result';
 }
 function choose(s,value){if(s.scene!=='fork'||!['A','B'].includes(value))return;
  const key=pendingFork(s)?.key;if(!key)return;
  s.forks[key]=value;s.scene='home';
 }
 function ending(s){return pack.endings[forks.map(f=>s.forks[f.key]).join('')||'default'];}
 function showEvidence(s,index){
  if(s.scene!=='gate'||!pendingGate(s))return;
  const g=gates[s.gates].test;if(!g[3].includes(index))return;
  if(index===g[2]){s.gates++;s.scene='crossing';}
  else s.scene='inspect';
 }
 return {fresh,actAt,pendingGate,pendingFork,kind,eligible,due,reviewRows,start,study,side,studyNext,current,cue,options,grade,advance,choose,ending,showEvidence,topics};
}
module.exports={createGame,modes,spell};
