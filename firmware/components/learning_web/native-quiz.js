// Pure semantic projection of the pinned read/listen quiz. The original HTML
// renderer remains the oracle and the fallback for other routes/error overlays.
function nativeQuizView(){
 if(typeof nativeInspect!=='function')return null;
 const s=model.state.session,t=s&&M.task(s);
 if(globalThis.nativeDirectDisabled||route!=='quiz'||!s||!['read','listen'].includes(t?.kind)||modal||pendingReturn||model.error||pathModel.error||model.corrupt||pathModel.corrupt)return null;
 const l=M.getLesson(s.lesson),total=s.unified?s.courseTaskCount:s.purpose==='focus'?s.baseLength:M.taskList(s).length,retry=s.at>=total;
 const blocks=[],actions=[],top=[];
 const words=x=>String(x).replace(/＿/g,'_').replace(/\s+/g,' ').trim();
 const action=data=>{actions.push(Object.fromEntries(Object.entries(data).map(([k,v])=>[k,String(v)])));return actions.length-1;};
 const text=(value,kind='text',role='')=>{const valueText=words(value);if(valueText)blocks.push({kind,role,text:valueText,area:'body'});};
 const button=(label,data,{area='body',role='',primary=false,enabled=true,selected=false}={})=>blocks.push({kind:'button',text:label,detail:'',role,action:action(data),enabled,selected,primary,area});
 const audio=step=>button('录音 / 停止',{action:'audio',text:t.text,step:step===null?'':step},{role:'audio'});
 if(s.checked){
  const c=s.checked;
  text(c.skip?'听力已暂缓':c.correct?c.assisted?'查词后答对':'答对了':'先弄懂，再练一次','heading');
  text(c.skip?'这题不会计错，但也不会记为听力完成。课后仍可补听。':c.correct?'你找到了这段信息的关键。':'这题会在课末重练；当前回答已保存。');
  if(!c.skip){text(t.options[t.answer]);text(t.why);if(t.kind==='listen'){audio(null);text(t.text);}}
  button(s.at+1===s.queue.length&&s.errors.length===0?s.purpose==='focus'?'完成本组':'完成本课':'继续',{action:'next'},{area:'foot',primary:true});
 }else{
  text(t.title,'heading');
  if(t.kind==='read'){
   const parts=pages(t.text,l.unit===2?24:32);readPage=Math.min(readPage,parts.length-1);
   text(parts[readPage],'text','passage');
   if(parts.length>1){text('原文 '+(readPage+1)+'/'+parts.length+' 段','text','article-position');button('上段原文',{action:'read-page',delta:-1},{role:'read-nav',enabled:readPage!==0});button('下段原文',{action:'read-page',delta:1},{role:'read-nav',enabled:readPage!==parts.length-1});}
  }else{
   audio(s.at);button('慢速 / 停止',{action:'audio',text:t.text,step:s.at,rate:.85},{role:'audio'});button('稍后听',{action:'skip'},{role:'audio'});
   const note=audioNote||'完整听完一次后再作答；可以重复听。';text(note,'text',note==='完整听完一次后再作答；可以重复听。'?'detail':'');
  }
  text(t.q,'text','question');
  t.options.forEach((o,i)=>button(words(String.fromCharCode(65+i)+' '+o),{action:'choose',n:i},{role:'option',selected:s.selected===i}));
  const heard=t.kind!=='listen'||s.heard[s.at];
  button(s.selected===null?heard?'选择答案后检查':'先完整播放录音':'检查答案',{action:'check'},{area:'foot',primary:true,enabled:s.selected!==null&&!!heard&&!playing});
  button('不会，看看解释',{action:'unknown'},{area:'foot',enabled:!!heard&&!playing});
 }
 top.push({text:'查词',action:action({action:'lookup'}),enabled:true},{text:'暂停',action:action({action:'go',route:'pause'}),enabled:true});
 const state=nativeInspect();
 return {title:`${l.code}${s.legacy?' 旧版':''}${s.purpose==='focus'?' '+({words:'词汇',read:'阅读',listen:'听力'}[s.focus])+'专项':''} · ${s.mode==='catchup'?'补听力':retry?'课末巩固':`${s.at+1} / ${total} 步`}`,blocks,top,tabs:[],actions,keyboard:false,typed:state.searchQuery||'',shift:state.shift,symbols:state.symbols,route:state.route,key:state.key+'|'+Number(nativeOtherBooks)+'|'+Number(nativeHomeOptions),prompt:'',toast:document.querySelector('#toast').textContent,gapMask:'',corrupt:state.corrupt};
}
function nativeRenderQuiz(){
 nativeDirect=null;
 const view=nativeQuizView();if(!view)return false;
 // Quiz routes own no editable DOM input. Remove old nodes so a lookup/back
 // cannot accidentally inspect a previous spelling field or stale button.
 app.children=[];app._html='';nativeRevision++;
 nativeDirect={view,revision:nativeRevision};nativeDirectStats.pages++;return true;
}
