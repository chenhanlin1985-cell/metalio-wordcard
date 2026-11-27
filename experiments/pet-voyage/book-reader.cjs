'use strict';
const vocabulary=require('./vocabulary-story.cjs');
const {award}=require('./stamps.cjs');
const titles={vocabulary:'分段学词',comprehension:'阅读理解',adventure:'任务冒险'};
const tags=text=>Array.from(text.matchAll(/\[\[([^\]]+)\]\]/g),m=>m[1].toLowerCase());
const plain=text=>text.replace(/\[\[|\]\]/g,'');
const segmented=(b,u)=>b?.storyType==='vocabulary'&&!u.overlay;

function compose(story,deck,key,deckKey,audioRoot){
  const b=story.data,storyType=story.m.storyType,terms={};
  if(!b.chapters?.length||!b.terms)throw Error('故事缺少章节或词义表');
  for(const [word,t] of Object.entries(b.terms)){
    const entry=t.id&&deck.data.words[t.id];
    if(t.id&&entry?.word!==word)throw Error('故事引用了其他词条 '+word);
    terms[word]={...t,ipa:entry?.ipa,audio:entry?.audio?audioRoot+entry.audio:''};
  }
  const wordsByIndex=[],used=new Set();
  if(storyType==='vocabulary'){
    for(const chapter of b.chapters){
      if(chapter.words?.length!==4)throw Error('分段学词每段需要四词');
      for(const row of chapter.words){
        const term=row.term;
        const line=chapter.en.match(/[^.!?]+[.!?]*[”’"]?/g)?.find(line=>tags(line).includes(term));
        if(!terms[term]?.id||used.has(term)||!line||row.distractors?.length!==3||
           new Set([row.gloss,...row.distractors]).size!==4)throw Error('段落目标词或选项错误 '+term);
        used.add(term);wordsByIndex.push({...row,evidence:plain(line.trim())});
      }
    }
  }
  return {...b,storyType,title:story.m.title,key,deckKey,deckSha256:deck.m.files['content.json'],terms,wordsByIndex,targets:[...used]};
}
function fresh(){return {phase:'cover',chapter:0,p:0,q:0,choice:null,answers:{},mode:'easy',complete:false};}
function scene(s,u){return u.overlay||s.phase;}
function push(u,overlay,props={}){
  const {stack,...previous}=u;
  stack.push(previous);Object.assign(u,{overlay,zh:false},props);
}
function finish(b,s,now){
  s.phase='done';s.complete=true;
  award(b,s,'story',b.targets.map(k=>b.terms[k].id),b.targets,now);
}
function pageAt(b,s,u){
  const pages=u.overlay==='album'?b.album:b.chapters[s.chapter].pages,index=u.overlay?u.index:s.p;
  return {pages,index,page:pages[index]};
}
function act(b,s,u,cmd,now,play){
  const [a,value]=cmd.split('/');
  if(u.overlay&&a==='back'){Object.assign(u,u.stack.pop()||{overlay:null});return;}
  if(a==='album'&&b.album?.length&&!u.overlay&&(!s.segments||['read','done'].includes(s.segments.stage))){push(u,'album',{index:0});return;}
  if(u.overlay==='gloss'){
    if(a==='audio'&&b.terms[u.term].audio)play(b.terms[u.term].audio,u.term);return;
  }
  if(u.overlay==='glossary'){
    if(a==='gloss'&&u.terms.includes(value))push(u,'gloss',{term:value});return;
  }
  if(['album','peek'].includes(u.overlay)||(!u.overlay&&s.phase==='read'&&b.storyType==='comprehension')){
    const {pages,page,index}=pageAt(b,s,u);
    if(a==='zh'){u.zh=!u.zh;return;}
    if(a==='words'){push(u,'glossary',{terms:[...new Set(tags(page.en))]});return;}
    const offset=a==='prevPage'?-1:a==='nextPage'?1:0;
    if(offset&&index+offset>=0&&index+offset<pages.length){
      if(u.overlay)u.index+=offset;else s.p+=offset;u.zh=false;return;
    }
    if(!u.overlay&&a==='plot'&&index===pages.length-1){s.phase='plot';s.q=0;s.choice=null;u.zh=false;}
    return;
  }
  if(u.overlay)return;
  if(segmented(b,u)){vocabulary.act(b,s,u,cmd,now,finish,play);return;}
  if(s.phase==='cover'&&a==='startRead'){s.phase='read';return;}
  if(s.phase!=='plot')return;
  const chapter=b.chapters[s.chapter],q=chapter.questions[s.q];
  if(a==='zh'){u.zh=!u.zh;return;}
  if(a==='peek'){push(u,'peek',{index:q.evidence});return;}
  if(a==='answer'&&s.choice===null&&q.options.some((_,i)=>String(i)===value)){
    s.choice=Number(value);s.answers[s.chapter+'/'+s.q]=s.choice;return;
  }
  if(a==='plotNext'&&s.choice!==null){
    s.choice=null;u.zh=false;
    if(s.q+1<chapter.questions.length){s.q++;return;}
    if(s.chapter+1===b.chapters.length){finish(b,s,now);return;}
    s.chapter++;s.p=0;s.q=0;s.phase='read';
  }
}
function view(b,s,u,origin){
  const p={title:b.title,route:'story-reader',blocks:[],top:[{text:u.overlay?'返回':origin==='home'?'回首页':'返回内容库',act:'back'}],tabs:[]};
  const text=(str,role='reader-copy')=>p.blocks.push({kind:'text',role,text:str});
  const button=(text,act,primary=false,area='foot',role='')=>p.blocks.push({kind:'button',text,act,primary,area,role});
  const top=(text,act)=>p.top.push({text,act});
  const chapter=b.chapters[s.chapter],v=scene(s,u);
  if(segmented(b,u)){
    if(b.album&&(!s.segments||['read','done'].includes(s.segments.stage)))top('看绘本','album');
    vocabulary.view(b,s,u,p,text,button);return p;
  }
  if(v==='cover'){
    text(titles[b.storyType]+' · '+b.chapters.length+' 幕','source');text(b.intro);
    button('翻开第一页','startRead',true);
  }else if(['read','album','peek'].includes(v)){
    const {pages,page,index}=pageAt(b,s,u);
    p.title=(v==='album'?b.title:chapter.title)+' · '+(index+1)+'/'+pages.length;
    if(page.image!==undefined){p.art=b.art;p.source=b.key;p.blocks.push({kind:'text',role:'picturebook-art',image:page.image});}
    top(u.zh?'英文':'中文','zh');if(tags(page.en).length)top('词义','words');
    for(const para of (u.zh?page.zh:plain(page.en)).split(/\n+/))text(para);
    const role=page.image!==undefined?'picture-nav':'audio';
    if(index)button('上一页','prevPage',false,'foot',role);
    if(index+1<pages.length)button('下一页','nextPage',true,'foot',role);
    if(v==='read'&&index+1===pages.length)button('答理解题','plot',true);
    if(v==='peek'||v==='album')button(v==='peek'?'回到原题':'回到学习','back',true);
  }else if(v==='glossary'){
    p.title='本页词义';for(const k of u.terms)button(k,'gloss/'+k,false,'body');button('回到原文','back',true);
  }else if(v==='gloss'){
    const t=b.terms[u.term];p.title='随文词义';text(u.term,'card-word');if(t.ipa)text(t.ipa,'ipa');text(t.meaning,'meaning');
    if(t.audio)button('听单词','audio');button('返回词义','back',true);
  }else if(v==='plot'){
    const q=chapter.questions[s.q];p.title='理解题 '+(s.q+1)+' / '+chapter.questions.length;
    text(u.zh?q.zh:q.en,'question');top(u.zh?'英文':'中文','zh');top('原文','peek');
    if(s.choice===null){
      q.options.forEach((value,i)=>{const letter=String.fromCharCode(65+i);text(letter+'   '+value);button(letter,'answer/'+i,false,'foot','answer-letter');});
    }else{
      text(s.choice===q.answer?'答对了':'再看这条线索','meaning');text(q.options[q.answer]);text(q.reason);
      button(s.q+1<chapter.questions.length?'下一题':s.chapter+1<b.chapters.length?'读下一幕':'读完全书','plotNext',true);
    }
  }else if(v==='done'){
    p.title='全书读毕';text(b.title,'meaning');let total=0,correct=0;
    b.chapters.forEach((c,i)=>c.questions.forEach((q,j)=>{total++;if(s.answers[i+'/'+j]===q.answer)correct++;}));
    text('理解题答对 '+correct+' / '+total+' 题');
    button(origin==='home'?'回首页':'返回内容库','back',true);
  }
  return p;
}
function current(b,s){const task=s?.segments?.task;return task?b.wordsByIndex[task[0]]?.term:null;}
module.exports={titles,compose,fresh,view,act,scene,segmented,current};
