'use strict';
const {observe}=require('./story-ledger.cjs');
function start(){
  return {lesson:0,stage:'read',queue:[],pending:[],later:[],recent:[],first:{},second:{},retries:{},slots:{},turn:0};
}
function quiz(b,p){
  p.queue=[];
  for(let i=0;i<4;i++){
    p.queue.push([4*p.lesson+i,'first']);
    if(p.lesson)p.queue.push([4*(p.lesson-1)+i,'second']);
  }
  if(p.lesson===b.chapters.length-1)for(let i=0;i<4;i++)p.queue.push([4*p.lesson+i,'second']);
  next(b,p);
}
function next(b,p){
  p.picked=null;
  // Three distinct other answers separate any two appearances of a word.
  const eligible=id=>!p.recent.includes(id);
  const retry=p.pending.find(eligible);
  if(retry!==undefined){
    p.pending.splice(p.pending.indexOf(retry),1);
    p.task=[retry,'retry'];
  }else if(p.queue.length&&eligible(p.queue[0][0])){
    p.task=p.queue.shift();
  }else if(!p.queue.length && (p.lesson<b.chapters.length-1 || !p.pending.length)){
    delete p.task;
    if(p.lesson===b.chapters.length-1)p.stage='done';
    else {p.lesson++;p.stage='read';}
    return;
  }else{
    const at=p.queue.findIndex(([id])=>eligible(id));
    if(at>=0)p.task=p.queue.splice(at,1)[0];
    else{
      const id=Object.keys(p.first).map(Number).find(eligible);
      if(id===undefined)throw Error('缺少可间隔的已学词');
      p.task=[id,'mix'];
    }
  }
  const [id,kind]=p.task;
  p.stage=(kind==='first'?id%2===0:kind==='second'?id%2===1:kind==='retry')?'think':'choose';
  const variant=kind==='first'?0:kind==='second'?2:1+(p.turn%3);
  p.slot=(((Math.floor(id/4)+[1,3,0,2][id%4])%4)+variant)%4;
  if(p.slot===p.slots[id])p.slot=(p.slot+1)%4;
  p.slots[id]=p.slot;
}
function options(b,p){
  const row=b.wordsByIndex[p.task[0]];
  return Array.from({length:4},(_,i)=>[row.gloss,...row.distractors][(i+4-p.slot)%4]);
}
function act(b,s,u,cmd,now,admit,play){
  if(!s.segments){if(cmd==='segments')s.segments=start();return;}
  const p=s.segments;
  if(p.stage==='read'){
    if(cmd==='segZh')u.zh=!u.zh;
    if(cmd==='segQuiz'){u.zh=false;quiz(b,p);}
    return;
  }
  if(p.stage==='done'){
    if(cmd==='segLater'&&p.later.length){p.pending=p.later.splice(0);next(b,p);}
    return;
  }
  const [id,kind]=p.task,word=b.wordsByIndex[id].term;
  if(p.stage==='feedback'){
    if(cmd==='audio'&&b.terms[word].audio)play(b.terms[word].audio,word);
    if(cmd==='segNext'){
      next(b,p);
      if(p.stage==='done')admit(b,s,now);
    }
    return;
  }
  if(p.stage==='think'&&cmd==='segOptions'){p.stage='choose';return;}
  if(cmd!=='segUnknown'&&!(p.stage==='choose'&&/^segPick\/[0-3]$/.test(cmd)))return;
  p.picked=cmd==='segUnknown'?-1:Number(cmd.slice(-1));
  const correct=p.picked===p.slot;
  observe(s,b.terms[word].id,'meaning',correct,false,kind==='retry');
  const answer={correct,at:now,selected:p.picked<0?'':options(b,p)[p.picked]};
  if(kind==='first'||kind==='second')p[kind][id]=answer;
  if(kind==='retry')p.retries[id]={...answer,count:(p.retries[id]?.count||0)+1};
  if(correct&&p.later.includes(id))p.later.splice(p.later.indexOf(id),1);
  if(!correct&&!p.pending.includes(id)&&!p.later.includes(id)){
    if(kind==='retry')p.later.push(id);
    else p.pending.push(id);
  }
  p.recent=[...p.recent,id].slice(-3);p.turn++;p.stage='feedback';
}
function view(b,s,u,page,text,button){
  const p=s.segments;
  if(!p){
    text('每段读一个小场景，接着练四个词。下一段还会问到刚才的词。');
    text(b.intro);button('开始分段练','segments',true);return;
  }
  if(p.stage==='done'){
    page.title='全书练完了';
    for(const [k,label] of [['first','第一次'],['second','隔段再问']]){
      const answers=Object.values(p[k]);text(label+'选对 '+answers.filter(x=>x.correct).length+' / '+answers.length+' 词');
    }
    const corrected=Object.values(p.retries).filter(x=>x.correct).length;
    text('订正后再答对 '+corrected+' 词','source');
    if(p.later.length)button('再练错词 '+p.later.length,'segLater',true,'body');
    button('去复习','deckReview',true);return;
  }
  if(p.stage==='read'){
    const {title,en,zh,words}=b.chapters[p.lesson];
    page.title='第 '+(p.lesson+1)+' 段 · '+title;
    text(u.zh?zh:en.replace(/\[\[|\]\]/g,''));
    for(const word of words)text(word.term+'   '+word.gloss,'source');
    page.top.push({text:u.zh?'英文':'中文',act:'segZh'});
    button('练这四词','segQuiz',true);return;
  }
  const [id,kind]=p.task,row=b.wordsByIndex[id],word=row.term,labels=options(b,p);
  page.title={first:'本段四词',second:'隔段再问',retry:'错词回练',mix:'穿插复习'}[kind];
  if(kind==='second'&&Math.floor(id/4)===p.lesson)page.title='再问一次';
  if(p.stage==='feedback'){
    page.title=p.picked===p.slot?'选对了':'订正词义';
    text(word,'card-word');if(b.terms[word]?.ipa)text(b.terms[word].ipa,'ipa');text(row.gloss,'meaning');
    if(p.picked>=0&&p.picked!==p.slot)text('你刚才选了「'+labels[p.picked]+'」。','source');
    text(row.evidence,'card-example');
    if(b.terms[word].audio)button('听单词','audio',false,'foot','audio');
    button('继续','segNext',true);return;
  }
  text(word,'word');
  if(p.stage==='think'){
    text('先自己说出这个词在故事里的意思。','source');
    button('想好了','segOptions',true);
  }else{
    text('它在这段故事里是什么意思？','source');
    labels.forEach((label,i)=>button('ABCD'[i]+'  '+label,'segPick/'+i,false,'body'));
  }
  button('想不起来','segUnknown');
}
module.exports={act,view};
