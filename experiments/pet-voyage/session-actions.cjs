'use strict';
const {modes,spell}=require('./game.cjs');
const {page}=require('./pages.cjs');
function performStudy(cmd,pack,g,s,u,now){
 const [a,v]=cmd.split('/'),q=s.session,t=q?.tasks[0];let store=true;
 function reset(){Object.assign(u,{overlay:null,stack:[],sheet:0,index:0,page:0,shift:false,symbols:false,query:''});}
 function home(){reset();s.scene='home';}
 function back(){if(u.stack.length)Object.assign(u,u.stack.pop());else home();}
 function open(overlay,extra={}){u.stack.push({overlay:u.overlay,sheet:u.sheet,index:u.index,page:u.page,query:u.query});Object.assign(u,{overlay,sheet:0,page:0,index:0},extra);}
 if(a==='pageNext'||a==='pagePrev'){u.page=Math.max(0,u.page+(a==='pageNext'?1:-1));store=false;}
 else if(a==='back'){back();}
 else if(a==='home'){home();}
 else if(a==='reviewStart'||a==='start'){reset();if(s.scene==='result')s.scene='home';g.start(s,now,a==='reviewStart');}
 else if(a==='study'&&s.scene==='brief'){g.study(s,s.chapter,pack.chapters[s.chapter].core,'main');reset();}
 else if(a==='learn'||a==='learnPrev'||a==='startQuiz'){
  if(u.overlay||q?.phase!=='learn')return false;
  const last=q.at+1===q.ids.length;
  if((a==='learn'&&last)||(a==='startQuiz'&&!last)||(a==='learnPrev'&&q.at===0))return false;
  g.studyNext(s,now,a==='learnPrev'?-1:1);u.page=0;
 }
 else if(a==='advance'){g.advance(s,now);u.page=0;}
 else if(a==='resultNext'){const side=s.result?.type!=='main';home();if(!side)g.start(s,now);}
 else if(a==='fork'){g.choose(s,v);u.page=0;}
 else if(a==='evidence'){g.showEvidence(s,Number(v));u.page=0;}
 else if(a==='again'&&s.scene==='inspect'){s.scene='gate';u.page=0;}
 else if(a==='side'&&u.overlay==='station'){const i=u.index;reset();g.side(s,i);}
 else if(['map','review','journal'].includes(a)){if(u.overlay===a)return false;open(a,{sheet:a==='map'?Math.floor(Math.min(s.chapter,pack.chapters.length-1)/5):0});}
 else if(a==='item'||a==='station'){open(a,{index:Number(v)});}
 else if(a==='more'||a==='prev'){u.sheet+=a==='more'?1:-1;u.page=0;}
 else if(a==='mode'&&v&&modes[v]){s.mode=v;back();}
 else if(a==='mode'||a==='settings'){open('mode');}
 else if(a==='lookup'&&q?.phase==='ask'){t.assisted=true;open('lookup',{query:''});}
 else if(a==='entry'&&u.overlay==='lookup'&&pack.words[v]){open('entry',{word:v});}
 else if(u.overlay==='lookup'&&['key','erase','submit','shift','symbols','bookmarks'].includes(a)){
  if(a==='key')u.query=(u.query+v).slice(0,50);
  if(a==='erase')u.query=u.query.slice(0,-1);
  if(a==='shift')u.shift=!u.shift;if(a==='symbols')u.symbols=!u.symbols;
  if(a==='submit'){const found=page(g,pack,s,u,now).blocks.find(b=>b.act?.startsWith('entry/'));if(found)open('entry',{word:found.act.slice(6)});}
 }
 else if(['translation','detail','prompt'].includes(a)&&q?.phase==='ask'){t.assisted=true;open('prompt');}
 else if(q?.phase==='ask'){
  if(a==='key'){const lim=t.k==='partial'?spell(pack.words[t.id].w).answer.length:80;q.draft=(q.draft+v).slice(0,lim);}
  else if(a==='erase')q.draft=q.draft.slice(0,-1);
  else if(a==='symbols')u.symbols=!u.symbols;
  else if(a==='shift')u.shift=!u.shift;
  else if(a==='pick'||a==='unknown'||a==='submit'){
   if(a==='submit'&&!q.draft.trim())return false;
   g.grade(s,a==='unknown'?'':a==='pick'?pack.words[v].w:q.draft,now);u.page=0;
  }else return false;
 }else return false;
 return store;
}
module.exports={performStudy};
