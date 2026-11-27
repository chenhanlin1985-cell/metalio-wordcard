'use strict';
const {spell,modes}=require('./game.cjs');
const {observe}=require('./story-ledger.cjs');
const {award}=require('./stamps.cjs');
const pilot=require('./receipt-practice.cjs');
const segments=require('./receipt-segments.cjs');
const segmented=(b,u)=>b?.key===pilot.bookKey&&!u.legacy&&!u.overlay;
const matches=en=>Array.from(en.matchAll(/\[\[([^\]]+)\]\]/g),m=>m[1].toLowerCase());
function compose(story,deck,key,deckKey,audioRoot){
 const b=story.data,terms={},used=new Set();
 if(b.style!=='read-first'||!b.chapters?.length)throw Error('无法识别故事正文');
 for(const [term,t] of Object.entries(b.terms)){
  const w=t.id&&deck.data.words[t.id];
  if(t.id&&w?.word!==term)throw Error('词库缺少对应词条 '+term);
  terms[term]=w?{...t,ipa:w.ipa,audio:w.audio?audioRoot+w.audio:''}:t;
 }
 const chapters=b.chapters.map(c=>{
  const focus=[];
  for(const p of c.pages)for(const term of matches(p.en)){
   if(!terms[term])throw Error('缺少随文词义 '+term);
   if(terms[term].id&&!used.has(term)){used.add(term);focus.push(term);}
  }
  if(b.art && (!Array.isArray(c.recallOrder)||c.recallOrder.length!==focus.length||
     new Set(c.recallOrder).size!==focus.length||c.recallOrder.some(k=>!focus.includes(k))))throw Error('绘本回想词表与正文不符');
  return {...c,focus:b.art?c.recallOrder:focus};
 });
 return {...b,title:story.m.title,key,deckKey,deckSha256:deck.m.files['content.json'],terms,chapters,targets:[...used]};
}
function fresh(){return {phase:'cover',chapter:0,p:0,q:0,wordAt:0,choice:null,draft:'',mode:'easy',roundMode:'easy',skills:{},complete:false,cards:{},practised:{},recent:[]};}
function scene(s,u){return u.overlay||s.phase;}
function pop(u){Object.assign(u,u.stack.pop()||{overlay:null});}
function push(u,overlay,props={}){
 const {stack,...prev}=u;stack.push(prev);Object.assign(u,{overlay},props);
}
function alternatives(b,term){
 const words=b.targets,at=words.indexOf(term),picks=[term,words[(at+1)%words.length],words[(at+3)%words.length]];
 return [...new Set(picks.slice(at%3).concat(picks.slice(0,at%3)))];
}
function admit(b,s,now){
 if(typeof NATIVE_STORY==='undefined'||!NATIVE_STORY)for(const term of b.targets){
  const id=b.terms[term].id,skills=(s.cards[id]??={});
  for(const k of new Set(['meaning',...(s.skills[id]||[])]))
   if(!skills[k])skills[k]={memory:null,last:now,due:now,target:now,attempts:0};
 }
 s.phase='done';s.complete=true;
 award(b,s,'story',b.targets.map(term=>b.terms[term].id),b.targets,now);
}
function act(b,s,u,cmd,now,play){
 if(segmented(b,u)&&cmd!=='settings'&&cmd!=='mode'){segments.act(b,s,u,cmd,now,admit,play);return;}
 const c=b.chapters[s.chapter],term=pilot.current(b,s),v=scene(s,u);
 const [a,param]=cmd.split('/');
 if(a==='back'&&u.overlay){pop(u);return;}
 if(a==='mode'&&param&&modes[param]&&u.overlay==='mode'){s.mode=param;pop(u);return;}
 if(a==='settings'||(a==='mode'&&!param)){push(u,'mode');return;}
 if(v==='done'&&b.key===pilot.bookKey){
  if(a==='tryRecall'&&!s.practice){
   s.practice=pilot.fresh();s.wordAt=0;s.choice=null;s.draft='';s.phase='ready';return;
  }
  if(a==='retryLater'&&s.practice?.pending.length){
   if(!pilot.takeRetry(b,s,now,true))push(u,'retryWait');
   return;
  }
 }
 if(a==='audio'){
  const t=b.terms[v==='gloss'?u.term:v==='recall'&&s.choice!==null?term:''];
  if(t?.audio)play(t.audio,v==='gloss'?u.term:term);return;
 }
 if(v==='glossary'&&a==='gloss'&&b.terms[param]){push(u,'gloss',{term:param});return;}
 if(a==='lookup'||a==='words'){
  const pi=u.overlay==='peek'?u.peek:s.p;
  push(u,'glossary',{terms:matches(c.pages[pi].en)});return;
 }
 if(a==='peek'&&(v==='plot'||v==='recall'||(b.art&&v==='done'))){
  const at=b.art&&v==='recall'?c.pages.findIndex(pg=>matches(pg.en).includes(term)):0;
  if(v==='recall'&&s.choice===null)s.hinted=true;
  if(s.practice){
   if(v==='recall'&&s.choice===null&&!s.practice.revealed)s.practice.hinted=true;
   pilot.touchPage(s,c.pages[Math.max(0,at)],now);
  }
  push(u,'peek',{peek:v==='plot'&&s.choice!==null?c.questions[s.q].evidence:Math.max(0,at),zh:false});return;
 }
 if(a==='zh'&&['read','peek','plot'].includes(v)){u.zh=!u.zh;return;}
 if(v==='peek'){
  if(a==='nextPage'&&u.peek+1<c.pages.length)u.peek++;
  if(a==='prevPage'&&u.peek>0)u.peek--;
  if(s.practice)pilot.touchPage(s,c.pages[u.peek],now);
  u.zh=false;return;
 }
 if(v==='cover'&&a==='startRead'){s.phase='read';return;}
 if(v==='read'){
  if(a==='prevPage'&&s.p>0)s.p--;
  if(a==='nextPage'&&s.p+1<c.pages.length)s.p++;
  if(a==='recall'&&s.p+1===c.pages.length){s.phase=b.art?'ready':c.focus.length?'recall':'actDone';s.roundMode=s.mode;s.wordAt=0;s.choice=null;s.draft='';}
  u.zh=false;return;
 }
 if((v==='ready'||v==='group')&&a==='startGroup'){
  s.phase='recall';s.roundMode=s.mode;
  if(b.key===pilot.bookKey){s.practice??=pilot.fresh();pilot.takeRetry(b,s,now);}
  return;
 }
 if(v==='recall'){
  if(s.choice!==null){
   if(a==='advance'){
    const retry=!!s.practice?.retry;
    if(s.practice)pilot.afterFeedback(s,term,now);
    s.choice=null;s.draft='';if(!retry)s.wordAt++;
    if(s.wordAt>=c.focus.length){
     if(s.practice&&pilot.takeRetry(b,s,now))return;
     if(b.art&&s.chapter+1===b.chapters.length)admit(b,s,now);else s.phase='actDone';
    }else if(!retry&&b.art&&s.wordAt%5===0)s.phase='group';
    else if(s.practice)pilot.takeRetry(b,s,now);
   }return;
  }
  const mode=s.roundMode,partial=spell(term),kind=mode==='hard'?'recall':mode==='normal'&&partial.answer?'partial':'meaning';
  if(s.practice&&(s.practice.retry||pilot.firstKind(term,kind)==='selfMeaning')){
   if(!s.practice.revealed&&a==='revealMeaning'){s.practice.revealed=true;return;}
   if((!s.practice.revealed&&a==='unknown')||
      (s.practice.revealed&&(a==='selfYes'||a==='selfNo'))){
    observe(s,b.terms[term].id,'meaning',a==='selfYes',!!s.hinted||!!s.practice.hinted,!!s.practice.retry);
    s.hinted=false;
    pilot.record(s,term,'selfMeaning',a==='selfYes',now);
   }
   return;
  }
  if(a==='shift'||a==='symbols'){u[a]=!u[a];return;}
  if(a==='erase'||a==='key'||cmd.startsWith('edit:')||cmd.startsWith('type:')){
   if(kind==='meaning')return;
   const input=a==='erase'?'\b':a==='key'?param:cmd.slice(5);
   let val=cmd.startsWith('type:')?'':s.draft;
   for(const ch of input)val=ch==='\b'?val.slice(0,-1):val+ch;
   s.draft=val.slice(0,kind==='partial'?partial.answer.length:80);return;
  }
  if(a==='unknown'||(a==='submit'&&s.draft.trim()&&kind!=='meaning')||(a==='pick'&&kind==='meaning'&&alternatives(b,term).includes(param))){
   const value=a==='unknown'?'':a==='pick'?param:s.draft.trim().toLowerCase();
   s.choice=value===(kind==='partial'?partial.answer:term).toLowerCase();
   observe(s,b.terms[term].id,kind,s.choice,!!s.hinted||!!s.practice?.hinted,false);
   s.hinted=false;
   if(s.practice)pilot.record(s,term,kind,s.choice,now,a==='pick'&&!s.choice?param:null);
   const skills=(s.skills[b.terms[term].id]??=[]);if(!skills.includes(kind))skills.push(kind);
  }return;
 }
 if(v==='actDone'&&a==='plot'){s.phase='plot';s.q=0;s.choice=null;return;}
 if(v==='plot'){
  if(a==='answer'&&s.choice===null&&c.questions[s.q].options.some((_,i)=>String(i)===param))s.choice=Number(param);
  if(a==='plotNext'&&s.choice!==null){s.choice=null;if(++s.q>=c.questions.length)s.phase='actDone';}
 }
 if((v==='actDone'||v==='plot')&&a==='nextAct'){
  if(s.chapter+1===b.chapters.length)admit(b,s,now);
  else{s.chapter++;s.phase='read';s.p=0;s.q=0;s.wordAt=0;s.choice=null;s.draft='';}u.zh=false;
 }
}
function view(b,s,u,origin){
 const v=scene(s,u),c=b.chapters[s.chapter],term=pilot.current(b,s),t=b.terms[term];
 const p={title:b.title,route:'story-reader',blocks:[],top:[{text:u.overlay?'返回':origin==='home'?'回首页':'返回内容库',act:'back'}],tabs:[]};
 const text=(str,role='reader-copy')=>p.blocks.push({kind:'text',role,text:str});
 const button=(text,act,primary=false,area='foot',role='')=>p.blocks.push({kind:'button',text,act,primary,area,role});
 const top=(text,act)=>p.top.push({text,act});
 if(segmented(b,u)){segments.view(b,s,u,p,text,button);return p;}
 const end=s.chapter+1===b.chapters.length,cont=end?'读完全书':'读下一幕';
 if(v==='cover'){
  text(b.englishTitle);text(b.intro);text(b.art?'先读完故事，再按五词一组回想。':'全书 '+b.targets.length+' 个剧情词。读完一幕，回想本幕的词。','source');
  top(modes[s.mode]+'模式','mode');button('翻开第一页','startRead',true);
 }else if(v==='read'||v==='peek'){
  const pi=v==='peek'?u.peek:s.p,pg=c.pages[pi],last=pi+1===c.pages.length;
  if(b.art){
   p.title=b.title+' · '+(pi+1)+'/'+c.pages.length;p.art=b.art;p.source=b.key;
   p.blocks.push({kind:'text',role:'picturebook-art',image:pg.image});
  }else{p.title='第 '+(s.chapter+1)+' 幕 · '+c.title;text((pi+1)+' / '+c.pages.length+' 页','source');}
  top(u.zh?'英文':'中文','zh');top('词义','words');
  for(const para of (u.zh?pg.zh:pg.en.replace(/\[\[|\]\]/g,'')).split(/\n+/))text(para);
  if(pi)button('上一页','prevPage',false,'foot',b.art?'picture-nav':'audio');
  if(!last)button('下一页','nextPage',v==='read','foot',b.art?'picture-nav':v==='peek'?'audio':'');
  if(v==='peek')button(s.phase==='done'?'回到书末':'回到原题','back',true,'foot',b.art?'picture-nav':'');
  else if(last)button(b.art?'读完':'回想本幕词','recall',true,'foot',b.art?'picture-nav':'');
 }else if(v==='glossary'){
  p.title='本页词义';for(const k of [...new Set(u.terms)])button(k,'gloss/'+k,false,'body');
  if(!u.terms.length)text('这页没有标注词。');button('回到原文','back',true);
 }else if(v==='gloss'){
  p.title='随文词义';text(u.term,'card-word');text(b.terms[u.term].ipa,'ipa');text(b.terms[u.term].meaning,'meaning');
  if(b.terms[u.term].audio)button('听单词','audio');button('返回词义','back',true);
 }else if(v==='ready'||v==='group'){
  p.title=v==='ready'?'故事读完了':'这一组完成了';text('已练 '+s.wordAt+' / '+c.focus.length+' 词。','meaning');
  if(b.key===pilot.bookKey)text('每组两个词，先自己说出意思，再翻面核对。','source');
  top(modes[s.mode]+'模式','mode');button(v==='ready'?'练五词':'下一组','startGroup',true);
 }else if(v==='recall'){
  p.title=(b.art?'看情境想词 ':'回想本幕词 ')+(s.wordAt+1)+' / '+c.focus.length;
  const partial=spell(term),type=s.roundMode==='hard'?'recall':s.roundMode==='normal'&&partial.answer?'partial':'meaning';
  const ownMeaning=s.practice&&(s.practice.retry||pilot.firstKind(term,type)==='selfMeaning');
  if(ownMeaning&&s.choice===null){
   p.title=s.practice.retry?'错词再练':'回忆词义 '+(s.wordAt+1)+' / '+c.focus.length;
   text(term,'word');
   if(s.practice.revealed){
    text(t.meaning,'meaning');text('刚才想到的意思对吗？','source');
    button('意思对','selfYes',true);button('没答对','selfNo');
   }else{
    text('这个词是什么意思？','source');
    top('回看原文','peek');button('核对词义','revealMeaning',true);button('想不起来','unknown');
   }
   return p;
  }
  if(s.choice===null){
   text(b.art?t.cue:t.meaning,b.art?'reader-copy':'meaning');
   const passage=c.pages.find(pg=>matches(pg.en).includes(term));
   const sentence=passage?.en.split(/(?<=[.!?])\s+|\n+/).find(line=>line.includes('[['+term+']]'));
   if(!b.art&&sentence)text(sentence.replaceAll('[['+term+']]','____').replace(/\[\[|\]\]/g,''));
   if(type==='meaning'){
    top('回看原文','peek');for(const k of alternatives(b,term))button(k,'pick/'+k,false,'foot','option');
    button('想不起来','unknown');
   }else{
    p.route='word-batch';p.keyboard=true;p.draft=s.draft;p.mask=type==='partial'?partial.mask:'';
    p.ready=type==='partial'?s.draft.length===partial.answer.length:!!s.draft.trim();p.shift=!!u.shift;p.symbols=!!u.symbols;
    button('回看原文','peek',false,'body');
   }
  }else{
   p.title=s.practice?.hinted?'看提示后核对':s.choice?'想起来了':'再记一次';text(term,'card-word');text(t.ipa,'ipa');text(t.meaning,'meaning');
   const result=s.practice?.first[term];
   if(s.practice&&!s.choice&&!s.practice.retry&&result?.selected){
    const picked=result.selected;text('刚才选的 '+picked+'：'+b.terms[picked].meaning,'source');
   }
   if(s.practice&&(!s.choice||s.practice.hinted)&&pilot.contrasts[term])text(pilot.contrasts[term]);
   if(t.example)text(t.example,'card-example');
   top('回看原文','peek');if(t.audio)button('听单词','audio',false,'foot','audio');
   button('继续','advance',true);
  }
 }else if(v==='actDone'){
  p.title='第 '+(s.chapter+1)+' 幕 · '+c.title;text('本幕已回想 '+c.focus.length+' 个词。','meaning');
  text(c.focus.join('  ·  '));if(s.q<c.questions.length)button('选做剧情题','plot');
  button(cont,'nextAct',true);
 }else if(v==='plot'){
  const q=c.questions[s.q];p.title='剧情题 '+(s.q+1)+' / '+c.questions.length;
  text(u.zh?q.zh:q.en,'question');top(u.zh?'英文':'中文','zh');top('原文','peek');
  if(s.choice===null)q.options.forEach((label,i)=>{
   const letter=String.fromCharCode(65+i);
   text(letter+'   '+label);
   button(letter,'answer/'+i,false,'foot','answer-letter');
  });
  else{text(q.options[q.answer]);text((s.choice===q.answer?'答对了。':'线索在这里。')+q.reason);}
  if(s.choice===null)button(cont,'nextAct');else button(s.q+1<c.questions.length?'下一题':'本幕小结','plotNext',true);
 }else if(v==='done'){
  p.title='全书读毕';text(b.title,'meaning');text('已答的故事词可在复习和已学词中查看。');text('完读印章已收入收集册','source');
  if(s.practice){
   const first=Object.values(s.practice.first),oral=first.filter(r=>r.kind==='selfMeaning');
   const retries=Object.values(s.practice.retries);
   text('首次独立回忆（自评） '+oral.filter(r=>r.correct&&!r.assisted).length+' / '+oral.length,'source');
   text('订正后想起 '+retries.filter(r=>r.correct&&!r.assisted).length+' 词 · 待回练 '+s.practice.pending.length+' 词','source');
   const unresolved=retries.filter(r=>!r.correct||r.assisted).length;
   if(unresolved)text('回练仍需复习 '+unresolved+' 词','source');
   if(s.practice.pending.length)button('错词回练','retryLater',false,'body');
  }else{
   if(b.key===pilot.bookKey)button('试练本书','tryRecall',true,'body');
   text(b.targets.join('  ·  '),'source');
  }
  if(b.art)button('重读','peek');
  button('去复习','deckReview',true);
 }else if(v==='retryWait'){
  p.title='稍后回练';text('先到 PET 复习三个不同的其他词，或隔十五分钟再来。');
  button('回到书末','back',true);
 }else if(v==='mode'){
  p.title='背词方式';text('从下一次本幕回想开始生效。','source');
  for(const [k,label] of Object.entries(modes))button(label+' · '+({easy:'选英文',normal:'补字母',hard:'拼全词'}[k]),'mode/'+k,false,'body');
 }
 return p;
}
module.exports={compose,fresh,view,act,scene,segmented,segmentWord:segments.word,current:pilot.current};
