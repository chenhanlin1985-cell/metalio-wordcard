'use strict';
const {modes,spell}=require('./game.cjs');
function page(g,pack,s,u,now){
 const cs=pack.chapters,W=pack.words,q=s.session,t=q?.tasks[0],v=u.overlay||s.scene;
 const n=cs.length,ci=q?.chapter??Math.min(s.chapter,n-1),c=cs[ci],act=g.actAt(ci),acts=pack.acts,gates=pack.gates.map(x=>x.test);
 const p={route:'word-result',title:'',blocks:[],top:[],tabs:[],subpage:u.page||0,shift:!!u.shift,symbols:!!u.symbols};
 const body=(text,role='',detail='')=>p.blocks.push({kind:'text',text,role,detail});
 const btn=(text,act,primary=false,area='foot',role='')=>p.blocks.push({text,act,kind:'button',area,primary,role});
 const pic=i=>{if(acts[i]?.image!==undefined)p.blocks.push({kind:'text',text:'',image:acts[i].image});};
 const top=(text,act)=>p.top.push({text,act});
 const back=()=>top('返回','back');
 const tabs=selected=>{p.tabs=[pack.plain?'背词':'故事',pack.plain?'主题':'章节',pack.plain?'内容库':'手记','复习'].map((text,i)=>({text,act:['home','map',pack.plain?'library':'journal','review'][i],selected:i===selected}));};
 const gloss=w=>w.sceneZh||w.zh;
 const full=(id,en)=>en.replace('[ ]',W[id].w);
 const pr=s.chapter===n&&s.gates===gates.length,gatePending=g.pendingGate(s);
 const forkPending=g.pendingFork(s);
 const due=g.due(s,now).length;
 const mode=()=>top(modes[s.mode],'mode');
 switch(v){
 case 'home':
  p.title=pack.title;mode();top('换书','library');
  if(pack.plain){
   const learned=Object.keys(s.practised).length,total=Object.keys(W).length;
   body(pack.title,'word');body('已练 '+learned+' / '+total+' 条','meaning');
   body('到期复习 '+due+' 项','source');
  if(q||learned<total)btn(q?'继续这组词':'开始背词','start',true);
   else btn('已练完 · 查看主题','map',true);
   tabs(0);break;
  }
  pic(act);body(acts[act].title+'  ·  第 '+(act+1)+' 幕','source');
  body(pr?'故事已完成':gatePending?gates[s.gates][0]:forkPending?'轮到你来做决定':c.title,'word');
  body(s.chapter===0&&!q?pack.intro:q?'你离开时的委托还在。':pr?g.ending(s)[0]:'已走过 '+s.chapter+' / '+n+' 站 · 收到 '+s.items.length+' 件证物','meaning');
  btn(q?'接着上次的委托':gatePending?'出示证物':forkPending?'决定下一程':pr?'翻开最后一页':s.chapter?'前往下一站':pack.firstAction, 'start',true);
  tabs(0);break;
 case 'brief':
  p.title='第 '+(ci+1)+' 站 · '+c.title;back();body(c.intro,'meaning');
  if(c.branchIntro)body(c.branchIntro[s.forks[c.branchKey]],'source');
  body('本次会遇到 '+c.core.length+' 个剧情词'+(Object.keys(c.returns).length?'，还有一条从前的线索。':'。'),'source');btn('接下这件委托','study',true);break;
 case 'learn':{
  const w=W[q.ids[q.at]];p.title='认词 '+(q.at+1)+' / '+q.ids.length;back();
  body(w.w,'card-word');body(w.ipa,'ipa');body(gloss(w),'meaning');const clue=c.prompts?.[q.ids[q.at]];if(clue)body(full(q.ids[q.at],clue[0]),'card-example',clue[1]);
  if(w.audio)btn('听单词','audio',false,'foot','audio');
  if(q.at>0)btn('上一个词','learnPrev',false,'foot','word-nav');
  const last=q.at+1===q.ids.length;
  btn(last?(q.type==='main'?'去现场试试':'开始测试'):(pack.plain?'下一个词':'读下一个词'),last?'startQuiz':'learn',true,'foot','word-nav');
  break;}
 case 'ask':{
  const w=W[t.id],clue=g.cue(t),prompt=clue?clue[0].replace('[ ]','____'):gloss(w);
  p.title=t.retry?'错词再练':t.callback?'旧线索派上用场':q.type==='review'?'到期复习':q.type==='side'?(t.k==='meaning'?'词义练习':'拼写练习'):'补全现场记录';back();
  if(t.k==='meaning'){body(prompt,'question');if(clue)btn('看中文情境','translation',false,'body');for(const id of g.options(s))btn(W[id].w,'pick/'+id,false,'foot','option');btn('想不起来','unknown');}
  else{p.route='word-batch';p.keyboard=true;p.draft=q.draft;p.mask=t.k==='partial'?spell(w.w).mask:'';p.ready=t.k==='partial'?q.draft.length===spell(w.w).answer.length:!!q.draft.trim();
   body(prompt,'meaning');btn('完整题干','prompt',false,'body');}break;}
 case 'prompt':case 'translation':
  p.title='题目提示';back();body(g.cue(t)?.[0].replace('[ ]','____')||gloss(W[t.id]),'question');
  if(g.cue(t))body(g.cue(t)[1],'meaning');btn('回到原题','back',true);break;
 case 'lookup':{
  p.route='search';p.title='课内查词';p.keyboard=true;p.draft=u.query||'';p.ready=!!p.draft.trim();back();
  const term=p.draft.trim().toLowerCase(),hidden=W[t?.id]?.w.toLowerCase();
  const found=term?Object.values(W).filter(w=>w.w.toLowerCase()!==hidden&&w.w.toLowerCase().startsWith(term)):[];
  body(term&&!found.length?'没有匹配的其他词条':'输入英文查当前词库','source');
  const seen=new Set();for(const w of found){if(seen.has(w.w))continue;seen.add(w.w);btn(w.w,'entry/'+w.id,false,'body');if(seen.size===2)break;}
  break;}
 case 'entry':{const w=W[u.word];p.title='词条';back();body(w.w,'card-word');body(w.ipa,'ipa');body(gloss(w),'meaning');if(w.audio)btn('听单词','audio',false,'foot','audio');btn('回到查词','back',true);break;}
 case 'feedback':{
  const w=W[t.id],clue=g.cue(t);p.title=q.feedback.ok?'答对了':'再记一次';back();body(w.w,'card-word');body(gloss(w),'meaning');
  if(clue)body(full(t.id,clue[0]),'card-example',clue[1]);
  if(!q.feedback.ok)body('隔一会儿会再练这个词。','source');if(w.audio)btn('听单词','audio',false,'foot','audio');btn('继续','advance',true);break;}
 case 'result':{
  const r=s.result,cc=cs[r.chapter];p.title=r.type==='main'?'获得 · '+cc.token:r.type==='side'?'一组词已练完':'本组复习完成';back();
  if(r.type==='main'){pic(g.actAt(r.chapter));body(cc.outro,'meaning');}
  body('本轮答对 '+r.right+' / '+r.attempts+' 次'+(r.deferred?'，'+r.deferred+' 项稍后复习。':'。'),'source');
  if(pack.plain&&r.type==='side'&&r.attempts>0)body('本组印章已收入收集册','source');
  btn(gatePending?'带上证物':r.type==='main'?'继续故事':'回首页','resultNext',true);break;}
 case 'gate':{
  const x=gates[s.gates];p.title=x[0];back();body(x[1],'question');
  for(const i of x[3])btn(cs[i].token,'evidence/'+i);break;}
 case 'inspect':p.title='再查一件证物';back();body(gates[s.gates][5],'meaning');btn('打开证物袋','again',true);break;
 case 'crossing':p.title='这一幕完成';back();pic(g.actAt(Math.max(0,s.chapter-1)));body(gates[s.gates-1][4],'meaning');btn(forkPending?'决定下一程':pr?'读结局':'继续故事','start',true);break;
 case 'fork':{
  const f=g.pendingFork(s);p.title=f.title;back();body(f.body,'meaning');
  btn(f.options[0],'fork/A');btn(f.options[1],'fork/B');break;}
 case 'completed':p.title='本词库已练完';back();body('已练过全部 '+Object.keys(W).length+' 条，之后可以继续做复习。','meaning');btn('回首页','home',true);break;
 case 'ending':{const [title,epilogue]=g.ending(s);p.title=title;back();pic(act);body(epilogue,'meaning');body(n+' 章完成 · 证物已存入手记','source');btn('翻看故事手记','journal',true);break;}
 case 'journal':{
  p.title='故事手记';back();const start=u.sheet*5;body('证物 '+s.items.length+' / '+n,'source');
  if(!s.items.length)body('完成第一站就可以收到第一件证物。','meaning');
  s.items.slice(start,start+5).forEach((val,j)=>btn(val,'item/'+(start+j),false,'body'));
  if(start>0)btn('前五件','prev');if(start+5<s.items.length)btn('后五件','more');tabs(2);break;}
 case 'item':{const x=cs[u.index];p.title=x.token;back();body(x.outro,'meaning');body('出自第 '+(u.index+1)+' 站 · '+x.title,'source');btn('回到手记','back',true);break;}
 case 'map':{
  p.title=pack.plain?'词库主题':'故事章节';back();const base=u.sheet*5;body(acts[g.actAt(base)].title,'meaning');
  cs.slice(base,base+5).forEach((x,j)=>btn((base+j+1)+'  '+x.title,'station/'+(base+j),false,'body'));
  if(u.sheet>0)btn('上一页','prev');if(base+5<n)btn('下一页','more');tabs(1);break;}
 case 'station':{
  const i=u.index,x=cs[i],ids=g.topics[i],count=ids.filter(id=>s.practised[id]).length;
  p.title=x.title;back();body(x.intro,'meaning');body('主题词条 '+count+' / '+ids.length+' 已练'+(pack.plain?'':' · 剧情词 '+x.core.length+' 条'),'source');
  const available=ids.some(id=>!x.core.includes(id)&&!s.practised[id]);
  if((pack.plain||i<=s.chapter)&&available&&!q)btn('练习本主题','side',true);
  else body(q?'请先完成当前练习。':!pack.plain&&i>s.chapter?'读到本章后开放练习。':'本主题已全部练过。','source');
  btn(pack.plain?'回到主题':'回到章节','back');break;}
 case 'review':
  p.title='到期复习';back();body(g.reviewRows(s,now).length+' 项等待温习','word');body(pack.deckTitle||pack.title,'meaning');
  if(q?.type==='review'||s.parkedReview||g.reviewRows(s,now).length)btn(q?.type==='review'||s.parkedReview?'继续复习':'开始复习','reviewStart',true);
  else body('当前词库暂时没有到期项。','source');
  tabs(3);break;
 case 'mode':
  p.title='背词方式';back();body('下一组词使用新模式。','source');
  for(const [v,label] of Object.entries(modes))btn((s.mode===v?'✓ ':'')+label+' · '+({easy:'选英文',normal:'补字母',hard:'拼全词'})[v],'mode/'+v,false,'body');break;
 }
 return p;
}
module.exports={page};
