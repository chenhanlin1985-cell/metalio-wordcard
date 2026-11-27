'use strict';

// The four rows under each scene follow the words in the passage.
// The first Chinese gloss is the in-context sense, followed by three distractors.
const scenes = [
  ['谁抓住了袖子？',
   'The concert was over. As the music faded, a customer reached for his coat. May’s neighbour was holding the same sleeve.',
   '音乐会散场，音乐声渐渐消失。一位顾客伸手去拿大衣，梅的邻居正抓着同一只袖子。',
   [['concert','音乐会','排练','乐队','演讲'],['music','音乐','歌词','乐谱','掌声'],['customer','顾客','店主','送货员','售票员'],['sleeve','袖子','口袋','领口','下摆']]],
  ['两件一模一样的大衣',
   'May found a second coat. Both were made of wool. She checked the label inside each one. Even the size was the same.',
   '梅又找到一件大衣。两件都是羊毛做的。她翻看衣服里面的标签，连尺码也一样。',
   [['coat','大衣','围巾','衬衫','背心'],['wool','羊毛','棉花','丝绸','皮革'],['label','衣物标签','拉链','衣架','纽扣'],['size','尺码','款式','颜色','重量']]],
  ['口袋里藏着什么？',
   'The shop assistant pointed to the pockets. May found a receipt in each. The man hugged his parcel. Inside was a present for his daughter’s birthday.',
   '店员指了指衣袋。梅从两个口袋里各翻出一张收据。男人抱紧包裹，里面是给女儿的生日礼物。',
   [['shop assistant','店员','顾客','保安','送货员'],['receipt','收据','购物清单','价目表','保修卡'],['parcel','包裹','衣袋','柜台','货架'],['birthday','生日','纪念日','假期','周末']]],
  ['十二英镑能看出什么？',
   'The date on both receipts was the same. Both gifts cost twelve pounds. The man paid in cash. He gave twenty pounds and got eight pounds in change.',
   '两张收据上的日期相同。两件礼物都花了十二英镑。男人用现金付了二十英镑，找回八英镑。',
   [['date','日期','地点','金额','编号'],['cost','价钱为','重达','节省','赚得'],['cash','现金','支票','代金券','积分'],['change','找回的零钱','付的小费','打折省的钱','交的押金']]],
  ['是谁的大衣？',
   'May’s neighbour opened her purse and showed her credit card. Now May could return the other coat to the man. His parcel went BOOM. His daughter knew which instrument was inside!',
   '邻居打开小钱包，亮出信用卡。梅终于能把另一件大衣还给男人了。包裹咚地响了一声，女儿猜到了里面是哪种乐器！',
   [['purse','小钱包','背包','手套','文件夹'],['credit card','信用卡','会员卡','礼品卡','交通卡'],['return','归还','借走','试穿','折叠'],['instrument','乐器','曲目','鼓点','节拍']]],
];
const words=scenes.flatMap(s=>s[3]);
const positions=[1,3,0,2, 2,0,3,1, 3,1,2,0, 1,2,0,3, 0,3,1,2];
function start(){
  return {lesson:0,stage:'read',queue:[],pending:[],later:[],recent:[],first:{},second:{},retries:{},slots:{},turn:0};
}
function quiz(p){
  p.queue=[];
  for(let i=0;i<4;i++){
    p.queue.push([4*p.lesson+i,'first']);
    if(p.lesson)p.queue.push([4*(p.lesson-1)+i,'second']);
  }
  if(p.lesson===scenes.length-1)for(let i=0;i<4;i++)p.queue.push([4*p.lesson+i,'second']);
  next(p);
}
function next(p){
  p.picked=null;
  // Three distinct other answers separate any two appearances of a word.
  const eligible=id=>!p.recent.includes(id);
  const retry=p.pending.find(eligible);
  if(retry!==undefined){
    p.pending.splice(p.pending.indexOf(retry),1);
    p.task=[retry,'retry'];
  }else if(p.queue.length&&eligible(p.queue[0][0])){
    p.task=p.queue.shift();
  }else if(!p.queue.length && (p.lesson<scenes.length-1 || !p.pending.length)){
    delete p.task;
    if(p.lesson===scenes.length-1)p.stage='done';
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
  p.slot=(positions[id]+variant)%4;
  if(p.slot===p.slots[id])p.slot=(p.slot+1)%4;
  p.slots[id]=p.slot;
}
function options(p){
  const row=words[p.task[0]];
  return Array.from({length:4},(_,i)=>row[1+(i+4-p.slot)%4]);
}
function act(b,s,u,cmd,now,admit,play){
  if(cmd==='legacy'){u.legacy=true;return;}
  if(!s.segments){if(cmd==='segments')s.segments=start();return;}
  const p=s.segments;
  if(p.stage==='read'){
    if(cmd==='segZh')u.zh=!u.zh;
    if(cmd==='segQuiz'){u.zh=false;quiz(p);}
    return;
  }
  if(p.stage==='done'){
    if(cmd==='segLater'&&p.later.length){p.pending=p.later.splice(0);next(p);}
    return;
  }
  const [id,kind]=p.task,word=words[id][0];
  if(p.stage==='feedback'){
    if(cmd==='audio'&&b.terms[word].audio)play(b.terms[word].audio,word);
    if(cmd==='segNext'){
      next(p);
      if(p.stage==='done')admit(b,s,now);
    }
    return;
  }
  if(p.stage==='think'&&cmd==='segOptions'){p.stage='choose';return;}
  if(cmd!=='segUnknown'&&!(p.stage==='choose'&&/^segPick\/[0-3]$/.test(cmd)))return;
  p.picked=cmd==='segUnknown'?-1:Number(cmd.slice(-1));
  const correct=p.picked===p.slot;
  require('./story-ledger.cjs').observe(s,b.terms[word].id,'meaning',correct,false,kind==='retry');
  const answer={correct,at:now,selected:p.picked<0?'':options(p)[p.picked]};
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
    button('开始分段练','segments',true);button('原版绘本','legacy');return;
  }
  if(p.stage==='done'){
    page.title='五段练完了';
    for(const [k,label] of [['first','第一次'],['second','隔段再问']]){
      const answers=Object.values(p[k]);text(label+'选对 '+answers.filter(x=>x.correct).length+' / '+answers.length+' 词');
    }
    const corrected=Object.values(p.retries).filter(x=>x.correct).length;
    text('订正后再答对 '+corrected+' 词','source');
    if(p.later.length)button('再练错词 '+p.later.length,'segLater',true,'body');
    button('去复习','deckReview',true);button('原版绘本','legacy');return;
  }
  if(p.stage==='read'){
    const [title,en,zh,rows]=scenes[p.lesson];
    page.title='第 '+(p.lesson+1)+' 段 · '+title;
    text(u.zh?zh:en);
    for(const row of rows)text(row[0]+'   '+row[1],'source');
    page.top.push({text:u.zh?'英文':'中文',act:'segZh'},{text:'原版绘本',act:'legacy'});
    button('练这四词','segQuiz',true);return;
  }
  const [id,kind]=p.task,row=words[id],word=row[0],labels=options(p);
  page.title={first:'本段四词',second:'隔段再问',retry:'错词回练',mix:'穿插复习'}[kind];
  if(kind==='second'&&Math.floor(id/4)===p.lesson)page.title='再问一次';
  if(p.stage==='feedback'){
    page.title=p.picked===p.slot?'选对了':'订正词义';
    text(word,'card-word');text(b.terms[word].ipa,'ipa');text(row[1],'meaning');
    if(p.picked>=0&&p.picked!==p.slot)text('你刚才选了「'+labels[p.picked]+'」。','source');
    const en=scenes[Math.floor(id/4)][1];
    const sentence=en.match(/[^.!?]+[.!?]+/g).find(line=>new RegExp('\\b'+word+'\\b','i').test(line));
    text(sentence.trim(),'card-example');
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
module.exports={act,view,word:id=>words[id][0]};
