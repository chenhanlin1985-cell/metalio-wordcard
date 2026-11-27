'use strict';
const {composeContent}=require('./compose.cjs');
const {createGame}=require('./game.cjs');
const {page}=require('./pages.cjs');
const {performStudy}=require('./session-actions.cjs');
const progress=typeof NATIVE_STORY!=='undefined'&&NATIVE_STORY?require('./story-ledger.cjs'):require('./progress.cjs');

const reader=require('./book-reader.cjs');
const isReader=b=>b?.storyType==='vocabulary'||b?.storyType==='comprehension';
let reviewReturn=null;
let rows=[],builtins=[],pack,game,state,rev,ui,notice='',audio,serial=200000;
let category='story',offsets={story:0,deck:0},origin='shelf',rootReview=false;
const latestKey='content-selection';
let homeMode=false;
const nav=selected=>[['home','首页'],['library','内容库'],['review','复习'],['search','词典']].map(([id,text])=>({act:'root/'+id,text,selected:selected===id}));
function summary(){
 if(!pack)return null;
 if(isReader(pack)){
  const total=pack.chapters.length,segment=state.segments;
  const index=pack.storyType==='vocabulary'?segment?.lesson||0:state.chapter;
  return {key:pack.key,deckKey:pack.deckKey,title:pack.title,type:'story',storyType:pack.storyType,
    done:state.complete?total:index,total,detail:state.complete?'全书完成':pack.chapters[index].title};
 }
 const name=pack.title,done=pack.plain?Object.keys(state.practised).length:state.chapter;
 const total=pack.plain?Object.keys(pack.words).length:pack.chapters.length;
 const session=state.session?.type==='review'?state.parkedStudy?.session:state.session;
 const detail=session?(session.phase==='learn'?'认识单词 '+(session.at+1)+' / '+session.ids.length:'接着上次的题目'):pack.plain?'按主题分组背词':pack.chapters[Math.min(state.chapter,total-1)].title;
 return {key:pack.key,deckKey:pack.deckKey,title:name,storyType:pack.storyType,type:pack.plain?'deck':'story',done,total,detail,mode:state.mode};
}
function installBuiltinRows(list){builtins=list;}
function editionOrder(a,b){
 if(a.m.schema!==b.m.schema)return b.m.schema-a.m.schema;
 const av=a.m.version.split('.').map(Number),bv=b.m.version.split('.').map(Number);
 return bv[0]-av[0]||bv[1]-av[1]||bv[2]-av[2];
}
function preferred(id){return rows.find(x=>x.m.type==='story'&&x.m.id===id);}
function catalog(){
 const visible=rows.filter(x=>x.m.type===category);
 const choices=category==='story'?visible.filter(x=>preferred(x.m.id)===x):visible;
 if(category==='story'){
  const order=Object.keys(reader.titles);
  choices.sort((a,b)=>order.indexOf(a.m.storyType)-order.indexOf(b.m.storyType));
 }
 const items=choices.map(({m,key})=>({
  label:m.title,act:'open/'+key,id:m.id,key,version:m.version,requires:m.requires,
  detail:m.type==='story'?(reader.titles[m.storyType]||'需要新版故事包'):''
 }));
 for(const x of items){
  if(items.some(y=>y!==x&&y.id===x.id))x.label+=' · '+x.version;
  if(x.requires&&!rows.some(d=>d.key===x.requires&&d.m.type==='deck'))x.detail='需要词库 '+x.requires;
 }
 if(category==='deck'){
  for(const x of builtins){
   if(x.packKey){const row=items.find(v=>v.key===x.packKey);if(row){row.detail='统一背词';continue;}}
   items.push({act:'builtin/'+x.id,label:x.title,detail:x.detail});
  }
 }
 return items;
}
const readStore={getItem:n=>nativeRead(n),setItem:(n,v,stampsChanged=false)=>{if(!(stampsChanged?nativeWrite(n,v,true):nativeWrite(n,v)))throw Error('SD 写入失败，请重试或返回内容库');}};
function clear(){pack=null;game=null;state=null;audio=null;homeMode=false;ui={overlay:null,stack:[],sheet:0,index:0,page:0};}
function shelf(location){
 if(location){const [tab,pg]=location.split('/');if(tab==='story'||tab==='deck'){category=tab;offsets[tab]=Math.max(0,Number(pg)||0);}}
 clear();reviewReturn=null;notice='';rows=[];rootReview=false;
 for(const key of bookKeys()){
  try{
   const m=bookRead(key,'manifest.json');
   if(m.format!=='wordcard-pack'||!(m.schema===1||([2,3].includes(m.schema)&&m.type==='story'))||key!==m.id+'@'+m.version||!['story','deck'].includes(m.type))continue;
   rows.push({key,m:{schema:m.schema,storyType:m.schema===3?m.storyType:null,id:m.id,type:m.type,title:m.title,version:m.version,requires:m.requires,files:{'content.json':m.files['content.json']}}});
  }catch{notice='有内容包无法读取，请重新安装。';}
 }
 rows.sort((a,b)=>(a.m.type===b.m.type?0:a.m.type==='story'?-1:1)||a.m.id.localeCompare(b.m.id)||editionOrder(a,b));
}
function restore(){
 const value=progress.read(readStore,pack,game.fresh);state=value.state;rev=value.record.rev;
}
function open(key){
 clear();notice='';
 if(typeof nativeDeckOwner!=='undefined'&&rows.some(x=>x.key===key&&x.m.type==='deck'))throw Error('词库索引无法加载，请检查 SD 卡中的词库文件。');
 let r=rows.find(x=>x.key===key);
 if(!r||(r.m.type==='story'&&r.m.schema!==3)){
  const replacement=preferred(key.split('@')[0]);
  if(replacement?.m.schema===3){r=replacement;key=r.key;}
 }
 if(r?.m.type==='story'&&(r.m.schema!==3||!Object.hasOwn(reader.titles,r.m.storyType)))throw Error('请安装标准格式的故事包，再打开这本书。');
 const d=r?.m.type==='deck'?r:rows.find(x=>x.key===r?.m.requires&&x.m.type==='deck');
 if(!r||!d)throw Error('需要安装词库 '+(r?.m.requires||''));
 const deck={m:d.m,data:bookRead(d.key,'content.json')};
 const story=d===r?deck:{m:r.m,data:bookRead(key,'content.json')};
 if(r.m.type==='story'&&r.m.storyType!=='adventure'){
  pack=reader.compose(story,deck,key,d.key,'packs/'+d.m.id+'/'+d.m.version+'/');
  deck.data=null;pack.digest=r.m.files['content.json'];pack.deckTitle=d.m.title;game={fresh:reader.fresh};restore();
  return;
 }
 if(d!==r)for(const c of story.data.chapters){
  if(!Object.hasOwn(deck.data.topics,c.topic)||c.words.some(w=>deck.data.words[w.id]?.topic!==c.topic))throw Error('章节词条与指定词库不符');
 }
 pack=composeContent(story,deck,key,d.key,'packs/'+d.m.id+'/'+d.m.version+'/',typeof nativeDeckOwner!=='undefined');
 deck.data=null;
 pack.storyType=r.m.storyType;pack.digest=r.m.files['content.json'];game=createGame(pack);restore();
}
function run(cmd){
 notice='';
 try{
  if(cmd==='shelf'||cmd==='library'||cmd==='root/library'){shelf();return;}
  if(cmd.startsWith('category/')){const t=cmd.slice(9);if(t==='story'||t==='deck'){category=t;offsets[t]=0;}return;}
  if(cmd==='moreBooks'||cmd==='prevBooks'){offsets[category]=Math.max(0,offsets[category]+(cmd==='moreBooks'?1:-1));return;}
  if(cmd.startsWith('open/')||cmd.startsWith('resume/')||cmd.startsWith('reviewOf/')||cmd.startsWith('modeOf/')){
   const key=cmd.slice(cmd.indexOf('/')+1);open(key);origin=cmd.startsWith('open/')?'shelf':'home';rootReview=cmd.startsWith('reviewOf/');
   homeMode=cmd.startsWith('modeOf/');
   reviewReturn=null;
   if(rootReview&&isReader(pack)){open(pack.deckKey);}
   if(rootReview||homeMode){if(!isReader(pack))ui.overlay=rootReview?'review':'mode';return;}
   if(isReader(pack)){readStore.setItem(latestKey,JSON.stringify(summary()));return;}
   const pausedStory=['inspect','crossing','gate','fork','ending','brief'].includes(state.scene);
   if(!pausedStory&&!(state.scene==='result'&&state.result?.type!=='review'))game.start(state,Date.now());
   rev=progress.write(readStore,pack,game.fresh,state,rev);
   readStore.setItem(latestKey,JSON.stringify(summary()));return;
  }
  if(!pack)return;
  if(cmd==='back'&&reviewReturn&&ui.overlay==='review'&&!ui.stack.length){
   const target=reviewReturn;reviewReturn=null;open(target.key);origin=target.origin;rootReview=false;return;
  }
  if(cmd==='back'&&!ui.stack.length&&!ui.overlay){
   if(rootReview){ui.overlay='review';return;}
   if(origin==='shelf'){readStore.setItem(latestKey,JSON.stringify(summary()));shelf();return;}
  }
  if(isReader(pack)){
   if(cmd==='deckReview'&&state.complete){
    const route={key:pack.key,origin},mode=state.mode;open(pack.deckKey);state.mode=mode;reviewReturn=route;ui.overlay='review';rootReview=true;return;
   }
   const before=JSON.stringify(state),previousUI=JSON.stringify(ui);
   reader.act(pack,state,ui,cmd,Date.now(),(path,text)=>{audio={path,text,serial:++serial};});
   if(before!==JSON.stringify(state)){
    try{rev=progress.write(readStore,pack,game.fresh,state,rev);}
    catch(error){ui=JSON.parse(previousUI);throw error;}
   }
   if(cmd.startsWith('mode/'))readStore.setItem(latestKey,JSON.stringify(summary()));
   return;
  }
  if(cmd==='reviewStart'&&!state.parkedReview&&state.session?.type!=='review'&&!game.reviewRows(state,Date.now()).length)return;
  if(cmd==='resultNext'&&state.result?.type==='review'){
   state.scene='home';state.result=null;ui.overlay='review';rev=progress.write(readStore,pack,game.fresh,state,rev);return;
  }
  if(cmd==='resultNext'&&state.result?.type==='side'){
   state.result=null;state.scene='home';game.start(state,Date.now());rev=progress.write(readStore,pack,game.fresh,state,rev);return;
  }

  if(cmd==='audio'){
   const q=state.session;
   const id=ui.overlay==='entry'?ui.word:q?.phase==='learn'?q.ids[q.at]:q?.phase==='feedback'?q.tasks[0].id:null;
   if(id&&pack.words[id].audio)audio={path:pack.words[id].audio,text:pack.words[id].w,serial:++serial};
   return;
  }
  if(cmd.startsWith('edit:')||cmd.startsWith('type:')){
   const p=view();if(!p.keyboard)return;
   const input=cmd.slice(5);
   let draft=cmd.startsWith('type:')?'':p.draft||'';
   for(const ch of input)draft=ch==='\b'?draft.slice(0,-1):draft+ch;
   if(ui.overlay==='lookup'){ui.query=draft.slice(0,50);return;}
   const q=state.session,t=q.tasks[0];q.draft=draft.slice(0,t.k==='partial'?require('./game.cjs').spell(pack.words[t.id].w).answer.length:80);
  }else if(!performStudy(cmd,pack,game,state,ui,Date.now()))return;
  rev=progress.write(readStore,pack,game.fresh,state,rev);
 }catch(e){notice=e.message;if(/^(open|resume|reviewOf|modeOf)\//.test(cmd))clear();else if(pack){try{restore();}catch{clear();}}}
}
function view(){
 if(isReader(pack)){
  const p=reader.view(pack,state,ui,origin);
  if(notice)p.blocks.unshift({kind:'text',text:notice,role:'source'});
  p.key=['vocabulary:packs',pack.key,reader.scene(state,ui),state.chapter,state.p,state.q,String(state.choice),ui.index,ui.term,ui.zh].join(':');
  if(reader.segmented(pack,ui)){const x=state.segments;p.key=['vocabulary:packs',pack.key,'segments',x?.stage,x?.turn,x?.lesson,x?.task,!!ui.zh].join(':');}
  const term=reader.segmented(pack,ui)&&state.segments?.stage==='feedback'?reader.current(pack,state):
    ui.overlay==='gloss'?ui.term:null;
  if(term)for(const [skill,card] of Object.entries(state.cards[pack.terms[term]?.id]||{})){
    const when=new Date(card.due+8*3600000).toISOString().slice(0,16).replace('T',' ');
    p.blocks.push({kind:'text',role:'source',text:({meaning:'词义',partial:'补字母',recall:'想英文'}[skill]||'拼写')+'复习 '+when});
  }
  return p;
 }
 const p=pack?page(game,pack,state,ui,Date.now()):{title:'内容库',route:'content-library',blocks:[],top:[],tabs:nav('library')};
 if(!pack){
  const items=catalog(),size=8;
  const n=Math.max(1,Math.ceil(items.length/size));
  const sheet=offsets[category]=Math.min(offsets[category],n-1);
  for(const [id,text] of [['story','故事书'],['deck','词库']])p.blocks.push({kind:'button',text,act:'category/'+id,role:'library-filter',selected:id===category});
  for(const item of items.slice(sheet*size,(sheet+1)*size))p.blocks.push({kind:'button',text:item.label,detail:item.detail||'',act:item.act,role:'library-row'});
  if(!items.length)p.blocks.push({kind:'text',text:category==='story'?'还没有故事书，可从 SD 卡添加。':'还没有词库，可从 SD 卡添加。',role:'library-message'});
  if(sheet)p.blocks.push({kind:'button',text:'上一页',act:'prevBooks',role:'library-prev'});
  if(sheet+1<n)p.blocks.push({kind:'button',text:'下一页',act:'moreBooks',role:'library-more'});
  if(n>1)p.blocks.push({kind:'text',text:(sheet+1)+' / '+n,role:'library-count'});
 }else{
  p.tabs=ui.overlay==='review'?nav('review'):[];
  if(!ui.overlay&&state.scene==='brief'){
   p.blocks.unshift({kind:'heading',role:'story-heading',text:pack.chapters[state.chapter].title,detail:'第 '+(state.chapter+1)+' 站'});
  }
  if(state.scene==='result'&&!ui.overlay){
   const next=p.blocks.find(b=>b.act==='resultNext');
   if(next&&state.result?.type==='side')next.text='继续学习';
   if(next&&state.result?.type==='review')next.text='回到复习';
  }
  p.top=p.top.filter(b=>b.act!=='library');
  if(!ui.overlay&&!ui.stack.length&&!rootReview){
   const back=p.top.find(b=>b.act==='back');if(back)back.text=origin==='shelf'?'返回内容库':'回首页';
  }
  if(!ui.overlay&&(state.scene==='brief'||state.scene==='learn'||state.scene==='ending'))p.top.push({text:pack.plain?'主题':'章节',act:'map'});
  if(ui.overlay==='map'&&!pack.plain)p.top.push({text:'手记',act:'journal'});
  if(!ui.overlay&&state.scene==='ending')p.blocks=p.blocks.filter(b=>b.act!=='journal'||b.primary);
 }
 if(notice)p.blocks.unshift({kind:'text',text:notice,role:pack?'meaning':'library-message'});
 const scene=ui.overlay||state?.scene||'shelf';
 p.key='vocabulary:packs:'+(pack?.key||category)+':'+scene+':'+(state?.chapter??offsets[category])+':'+(state?.session?.at??0)+':'+(state?.session?.tasks[0]?.id||'');
 if(pack?.art)p.art=pack.art;
 p.source=pack?.key||'';
 return p;
}
function status(){
 const q=state?.session,t=q?.tasks[0];
 const seg=reader.segmented(pack,ui),x=seg&&state.segments,word=x?.task&&reader.current(pack,state);
 const local=seg&&word?pack.terms[word].id:isReader(pack)?pack.terms[reader.current(pack,state)]?.id:t?.id;
 return {memory:local?state.cards[local]||null:null,engine:'content-packs',category,sheet:offsets[category],origin,route:seg?'segments-'+(x?.stage||'cover'):ui.overlay||state?.phase||state?.scene||'shelf',title:view().title,book:pack?.key,deck:pack?.deckKey,chapter:state?.chapter,word:seg?word:isReader(pack)?reader.current(pack,state):q?.phase==='learn'?q.ids[q.at]:t?.id,query:ui.query,draft:q?.draft??state?.draft,page:state?.p,question:state?.q,wordAt:state?.wordAt,choice:seg?(x?.stage==='feedback'?x.picked===x.slot:null):state?.choice,peek:ui.peek,complete:state?.complete,revision:rev||0,notice,installed:rows.length,art:pack?.art||''};
}
clear();
function bookmark(){return {key:pack.key,category,offsets,origin,rootReview,ui,reviewReturn};}
function reopen(json){
 const saved=JSON.parse(json);
 try{open(saved.key);({category,offsets,origin,rootReview,ui,reviewReturn}=saved);}
 catch(e){clear();notice=e.message;}
}
globalThis.Books={bookmark,reopen,shelf,run,view,status,summary,homeMode:()=>homeMode,backRoot:()=>!!pack&&!ui.stack.length&&(rootReview&&!reviewReturn&&ui.overlay==='review'||!rootReview&&origin==='home'&&(!ui.overlay||ui.overlay==='mode')),location:()=>category+'/'+offsets[category],origin:()=>origin,installBuiltinRows:s=>installBuiltinRows(JSON.parse(s)),takeAudio:()=>{const a=audio;audio=null;return a;},audioEnd:ok=>{if(!ok)notice='这段录音未能播放，可再听一次。';}};
