'use strict';
const {createGame}=require('./game.cjs');
const {performStudy}=require('./session-actions.cjs');
const {page}=require('./pages.cjs');
require('../bookshop/device/size.cjs');
const progress=require('./progress.cjs');
const ACTIVE='wordcard-library/selected';
const panel=document.getElementById('panel'),img=document.getElementById('frame'),hits=document.getElementById('hits'),msg=document.getElementById('message');
let pack,g,s,u={overlay:null,stack:[],sheet:0,index:0,page:0},busy=true,sound,rev=0,pendingClick=false,paintSeq=0;
function restore(){
 const v=progress.read(localStorage,pack,g.fresh);s=v.state;rev=v.record.rev;
}
function save(){rev=progress.write(localStorage,pack,g.fresh,s,rev);}
function reset(){u={overlay:null,stack:[],sheet:0,index:0,page:0};}

function alertUser(text){msg.hidden=false;msg.textContent=text;}
async function render(){
 const seq=++paintSeq;
 busy=true;panel.setAttribute('aria-busy','true');
 try{
  const p=page(g,pack,s,u,Date.now());
  const res=await fetch('/device-frame?key='+encodeURIComponent(pack.key),{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(p)});
  if(!res.ok)throw Error();const f=await res.json();const uri='data:image/png;base64,'+f.image;
  const preload=new Image();preload.src=uri;await preload.decode();if(seq!==paintSeq)return;img.src=uri;img.alt=p.title;
  hits.replaceChildren(...f.hits.map(h=>{
   const b=document.createElement('button');b.type='button';b.disabled=!h.enabled;b.dataset.act=h.act;b.setAttribute('aria-label',h.label||h.act);
   Object.assign(b.style,{left:h.x/6.84+'%',top:h.y/12.16+'%',width:h.w/6.84+'%',height:h.h/12.16+'%'});return b;
  }));panel.dataset.view=u.overlay||s.scene;panel.dataset.title=p.title;panel.dataset.pages=f.pages;
  msg.hidden=true;
 }catch{alertUser('画面暂时无法读取。请刷新此页，已保存的委托可以继续。');}
 finally{if(seq===paintSeq){busy=false;panel.setAttribute('aria-busy','false');}}
}
function play(){
 const q=s.session;if(u.overlay!=='entry'&&(!q||!['learn','feedback'].includes(q.phase)))return;
 const id=u.overlay==='entry'?u.word:q.phase==='learn'?q.ids[q.at]:q.tasks[0].id;
 if(sound)sound.pause();sound=new Audio('/'+pack.words[id].audio);sound.play().catch(()=>alertUser('发音暂时无法播放，可再次点听单词。'));
}
function perform(cmd){
 if(busy||!s)return;
 const a=cmd.split('/')[0];const now=Date.now();
 if(a==='library'){location.assign('/library.html');return;}
 if(a==='audio'){play();return;}
 if(sound)sound.pause();
 const store=performStudy(cmd,pack,g,s,u,now);
 if(store)try{save();}catch(e){restore();alertUser(e.message||'浏览器无法保存进度，请保留这个窗口。');return;}
 void render();
}
async function dispatch(cmd){
 if(busy||pendingClick||!s)return;
 pendingClick=true;panel.setAttribute('aria-busy','true');const expected=rev;
 try{
  const action=()=>{
   const latest=progress.read(localStorage,pack,g.fresh);
   if(latest.record.rev!==expected||rev!==expected){restore();reset();void render();return;}
   perform(cmd);
  };
  if(navigator.locks)await navigator.locks.request(progress.storeKey(pack.deckKey),action);else action();
 }catch(e){alertUser(e.message);}finally{pendingClick=false;if(!busy)panel.setAttribute('aria-busy','false');}
}
hits.addEventListener('click',e=>{const b=e.target.closest('button[data-act]');if(b&&!b.disabled)dispatch(b.dataset.act);});
document.getElementById('coverHome').onclick=()=>dispatch('home');document.getElementById('coverBack').onclick=()=>dispatch('back');
window.addEventListener('storage',e=>{if(pack&&e.key===progress.storeKey(pack.deckKey)&&e.newValue){restore();void render();}});
(async()=>{
 try{
  const key=new URLSearchParams(location.search).get('key')||localStorage.getItem(ACTIVE);
  if(!key){location.replace('/library.html');return;}
  const res=await fetch('/api/open?key='+encodeURIComponent(key));const data=await res.json();
  if(!res.ok)throw Error(data.error||'这个内容包暂时无法打开');
  if(data.style==='read-first'){location.replace('/reader.html?key='+encodeURIComponent(key));return;}
  pack=data;g=createGame(pack);restore();document.title=pack.title+' · 小小单词卡';localStorage.setItem(ACTIVE,key);await render();
 }catch(e){alertUser(e.message+'。');const a=document.createElement('a');a.href='/library.html';a.textContent='打开内容库';msg.append(a);}
})();
