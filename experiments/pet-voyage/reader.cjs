'use strict';
const progress=require('./progress.cjs');
const {createGame}=require('./game.cjs');
const $=id=>document.getElementById(id);
const page=$('page'), actions=$('actions');
let book, game, reviewTerms, state, rev, overlay=null, translated=false, busy=false, audio=null;
const activeStore=localStorage, embedded=document.getElementById('book-data');
const fresh=()=>({phase:'cover',chapter:0,p:0,q:0,choice:null,answers:[],complete:false,round:[],index:0,
 cards:{},recent:[],practised:{}});
const read=()=>progress.read(activeStore,book,fresh);
function restore(){const result=read();state=result.state;rev=result.record.rev;}
function msg(s){$('status').textContent=s;$('status').hidden=!s;}
function resetView(){overlay=null;translated=false;if(audio){audio.pause();audio=null;}}
function save(fn){
 if(busy)return;busy=true;$('leaf').setAttribute('aria-busy','true');
 const expected=rev;
 const act=()=>{
  restore();
  if(rev!==expected){resetView();draw();msg('另一页已更新书签，这里已回到最新位置。');return;}
  fn(state);rev=progress.write(activeStore,book,fresh,state,rev);resetView();draw();
 };
 const attempt=async()=>{
  try{if(navigator.locks)await navigator.locks.request(progress.storeKey(book.deckKey),act);else act();}
  catch(e){try{restore();resetView();draw();}catch{}msg('书签保存失败，请保留此页并重试。');}
  finally{busy=false;$('leaf').setAttribute('aria-busy','false');}
 };void attempt();
}
function node(tag,cls,txt){
 const el=document.createElement(tag);if(cls)el.className=cls;if(txt!==undefined)el.textContent=txt;return el;
}
function put(tag,cls,txt){const el=node(tag,cls,txt);page.append(el);return el;}
function button(parent,label,fn,cls){
 const b=node('button',cls,label);b.type='button';b.onclick=fn;parent.append(b);return b;
}
const primary=(label,fn)=>button(actions,label,fn,'action primary');
const secondary=(label,fn)=>button(actions,label,fn,'action');
const date=()=>Date.now();
const due=(s,now)=>reviewTerms.filter(k=>(s.cards[book.terms[k].id]?.meaning?.due??Infinity)<=now);
function addReview(s){
 for(const term of reviewTerms){
  const id=book.terms[term].id;
  const skills=(s.cards[id]??={});
  // Admission alone is not a successful vocabulary attempt.
  if(!skills.meaning){const now=date();skills.meaning={memory:null,last:now,due:now,target:now,attempts:0};}
 }
 s.complete=true;s.phase='done';
}
function advancePlot(s){
 if(s.choice===null)return;
 const ch=book.chapters[s.chapter];s.choice=null;
 if(++s.q<ch.questions.length)return;
 if(s.chapter+1===book.chapters.length){addReview(s);return;}
 s.chapter++;s.p=0;s.q=0;s.phase='read';
}
function clock(){
 const ns='http://www.w3.org/2000/svg';
 const svg=document.createElementNS(ns,'svg');svg.setAttribute('viewBox','0 0 120 120');svg.setAttribute('class','clock');svg.setAttribute('aria-hidden','true');
 function shape(tag,props){const s=document.createElementNS(ns,tag);for(const [k,v] of Object.entries(props))s.setAttribute(k,v);svg.append(s);}
 shape('circle',{cx:60,cy:60,r:53,fill:'none',stroke:'currentColor','stroke-width':1.6});
 for(let i=0;i<60;i++)shape('line',{x1:60,y1:10,x2:60,y2:i%5===0?17:12,stroke:'currentColor','stroke-width':i%5===0?1.7:.65,transform:'rotate('+i*6+' 60 60)'});
 for(const [deg,len,width] of [[298.5,27,3.3],[342,39,2]])shape('line',{x1:60,y1:60,x2:60,y2:60-len,stroke:'currentColor','stroke-width':width,transform:'rotate('+deg+' 60 60)'});
 shape('circle',{cx:60,cy:60,r:2.6,fill:'currentColor'});return svg;
}
function cover(){
 $('running').textContent='PET 故事书';$('counter').textContent='全书 '+book.chapters.length+' 幕';
 const v=put('div','cover');v.append(node('p','kicker','欧·亨利　/　O. Henry'),node('h1','',book.title),node('p','english-title',book.englishTitle),clock(),node('p','blurb',book.intro));
 const cite=node('div','source',book.attribution+'　');const a=node('a','','原作');a.href=book.source;a.target='_blank';a.rel='noopener noreferrer';cite.append(a);v.append(cite);
 const label=state.phase==='cover'?'翻开第一页':state.complete&&state.phase==='done'?'回到读后页':'接着上次读';
 primary(label,()=>{if(state.phase==='cover')save(s=>{s.phase='read';});else{resetView();draw();}});
}
function paragraph(text){
 const p=node('p');
 for(const part of text.split(/(\[\[[^\]]+\]\])/g)){
  if(part.startsWith('[[')){
   const term=part.slice(2,-2);
   button(p,term,()=>{overlay={type:'gloss',term,back:overlay,translated,scroll:page.scrollTop};translated=false;draw();},'term');
  }else p.append(document.createTextNode(part));
 }return p;
}
function reading(ci,pi,peek){
 const ch=book.chapters[ci],curr=ch.pages[pi],last=pi+1===ch.pages.length;
 $('counter').textContent=(pi+1)+' / '+ch.pages.length+' 页';
 put('p','kicker','第 '+(ci+1)+' 幕');put('h2','',ch.title);
 const story=put('div','reading');story.lang='en';for(const line of curr.en.split(/\n+/))story.append(paragraph(line));
 if(translated)put('p','translation',curr.zh);
 if(pi>0)secondary('上一页',()=>peek?movePeek(-1):save(s=>{s.p--;}));
 secondary(translated?'收起译文':'看中文',()=>{translated=!translated;draw(false);if(translated)page.querySelector('.translation').scrollIntoView({block:'nearest'});else page.scrollTop=0;});
 if(peek){
  if(!last)secondary('下一页',()=>movePeek(1));
  primary(overlay.reread?(last?'读完本幕':'回读后页'):'回到原题',()=>{
   if(overlay.reread&&last&&ci+1<book.chapters.length){overlay.c++;overlay.p=0;}
   else overlay=null;
   translated=false;draw();
  });
 }else{
  const next=primary(last?'读完本幕，答题':'下一页',()=>save(s=>{if(last){s.phase='quiz';s.q=0;s.choice=null;}else s.p++;}));
  const checkEnd=()=>{next.disabled=story.getBoundingClientRect().bottom>page.getBoundingClientRect().bottom+3;};
  page.onscroll=checkEnd;requestAnimationFrame(checkEnd);
 }
}
function movePeek(n){overlay.p+=n;translated=false;draw();}
function gloss(){
 const term=overlay.term,t=book.terms[term];put('p','kicker','随文词义');put('h2','word',term);
 if(t.ipa)put('p','ipa',t.ipa);put('p','small-copy',t.meaning);
 if(t.audio)secondary('听单词',()=>{if(audio)audio.pause();audio=new Audio(t.audio);audio.play().catch(()=>msg('这次没有播出，可再点一次。'));});
 primary('回到原文',()=>{if(audio)audio.pause();const pos=overlay.scroll;translated=overlay.translated;overlay=overlay.back;draw();page.scrollTop=pos;});
}
function choices(opts,selected,correct,select){
 const group=put('div','choices');
 opts.forEach((label,i)=>{
  const b=button(group,label,()=>select(i),'choice');b.disabled=selected!==null;
  if(selected!==null){if(i===correct)b.classList.add('correct');else if(i===selected)b.classList.add('wrong');}
 });
}
function quiz(){
 const ch=book.chapters[state.chapter],q=ch.questions[state.q];
 $('counter').textContent=(state.q+1)+' / '+ch.questions.length+' 题';
 put('p','kicker','第 '+(state.chapter+1)+' 幕 · '+ch.title);put('p','question',q.en).lang='en';
 if(translated)put('p','translation',q.zh);
 choices(q.options,state.choice,q.answer,i=>save(s=>{if(s.choice!==null)return;s.choice=i;s.answers.push([s.chapter,s.q,i]);}));
 if(state.choice!==null)put('p','explanation',(state.choice===q.answer?'答对了。':'再看这处细节。')+q.reason);
 if(state.choice===null)secondary(translated?'收起中文':'题意中文',()=>{translated=!translated;draw(false);});
 secondary('回看原文',()=>{overlay={type:'peek',c:state.chapter,p:state.choice===null?0:q.evidence};translated=false;draw();});
 if(state.choice!==null)primary(state.q+1<ch.questions.length?'下一题':state.chapter+1<book.chapters.length?'读下一幕':'完成全书',()=>save(advancePlot));
}
function done(){
 const pending=due(state,date());put('p','kicker','全书读毕');put('h2','',book.title);
 put('p','small-copy','这本书的 '+reviewTerms.length+' 个剧情词已接入 PET 复习。');
 put('p','explanation',pending.length?'现在可练 '+pending.length+' 个词。':'今天的这组词已经练完。');
 if(!pending.length){
  const next=Math.min(...reviewTerms.map(k=>state.cards[book.terms[k].id].meaning.due));
  put('p','caption','最近一次到期　'+new Date(next).toLocaleString('zh-CN',{month:'short',day:'numeric',hour:'2-digit',minute:'2-digit'}));
 }
 const reread=()=>{overlay={type:'peek',reread:true,c:0,p:0};translated=false;draw();};
 if(pending.length){secondary('重读故事',reread);primary('复习剧情词',()=>save(s=>{s.round=due(s,date());s.index=0;s.choice=null;s.phase='review';}));}
 else primary('重读故事',reread);
}
function review(){
 const term=state.round[state.index],t=book.terms[term],slot=reviewTerms.indexOf(term)%3,opts=[...t.distractors];
 opts.splice(slot,0,t.meaning.replace(/^(?:n\.|adj\.|短语)\s*/,''));
 $('counter').textContent=(state.index+1)+' / '+state.round.length+' 词';
 put('h2','',term);put('p','example',t.example).lang='en';put('p','caption','选出它在句中的意思');
 choices(opts,state.choice,slot,i=>save(s=>{
  if(s.choice!==null)return;s.choice=i;
  const task={phase:'ask',tasks:[{id:t.id,k:'meaning'}],attempts:0,right:0};
  game.grade({session:task,cards:s.cards,recent:s.recent,practised:s.practised},i===slot?term:'',date());
 }));
 if(state.choice!==null)put('p','explanation',(state.choice===slot?'答对了。':'这个词会提前复习。')+t.meaning);
 secondary('回读后页',()=>save(s=>{s.phase='done';s.choice=null;}));
 if(state.choice!==null)primary(state.index+1<state.round.length?'下一个词':'完成复习',()=>save(s=>{s.choice=null;if(++s.index===s.round.length)s.phase='done';}));
}
function draw(focus=true){
 const oldScroll=page.scrollTop;page.onscroll=null;page.replaceChildren();actions.replaceChildren();msg('');
 $('counter').textContent='';$('running').textContent=book.title;
 const view=overlay?.type||state.phase,home=$('home');home.hidden=view==='cover'&&!!embedded;
 home.textContent=view==='cover'?'内容库':'扉页';
 home.onclick=()=>{if(view==='cover'){location.assign('/library.html');return;}resetView();overlay={type:'cover'};draw();};
 if(view==='cover')cover();
 if(view==='read')reading(state.chapter,state.p,false);
 if(view==='peek')reading(overlay.c,overlay.p,true);
 if(view==='gloss')gloss();
 if(view==='quiz')quiz();
 if(view==='done')done();
 if(view==='review')review();
 $('leaf').dataset.view=view;page.scrollTop=focus?0:oldScroll;if(focus)page.focus({preventScroll:true});
}
(async()=>{
 try{
  if(embedded)book=JSON.parse(embedded.textContent);
  else{const key=new URLSearchParams(location.search).get('key');const response=await fetch('/api/open?key='+encodeURIComponent(key));const result=await response.json();if(!response.ok)throw Error(result.error);book=result;}
  if(book.style!=='read-first')throw Error('这本书需要从内容库打开。');
  reviewTerms=Object.keys(book.terms).filter(k=>book.terms[k].id);
  const words=Object.fromEntries(reviewTerms.map(k=>[book.terms[k].id,{w:k}]));
  game=createGame({words,chapters:[],authored:{}});
  restore();if(!embedded)localStorage.setItem('wordcard-library/selected',book.key);
  document.title=book.title+' · PET 故事书';draw();
  window.addEventListener('storage',e=>{if(e.key===progress.storeKey(book.deckKey)){try{restore();resetView();draw();}catch{msg('无法读取书签，请重新打开。');}}});
  document.addEventListener('visibilitychange',()=>{if(!document.hidden&&state.phase==='done'&&!overlay)draw(false);});
 }catch(e){page.textContent='暂时无法打开故事。'+e.message;}
})();
