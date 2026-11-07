(function(root){'use strict';
const node=typeof module==='object'&&module.exports,T=node?require('./word-training.js'):root.WORDCARD_TRAINING,clone=x=>JSON.parse(JSON.stringify(x));
const OTHER=3,kindName={meaning:'词义',spelling:'完整拼写'},intervals=[1,2,4,7];
const key=q=>q.id+'|'+q.kind,actual=v=>v&&v.correct&&!v.assisted;
function pool(s,known=[]){s.retryPool=[...new Set([...s.ids,...known])].filter(T.get);return s;}
function make(...args){const s=T.make(...args);s.reviewVersion=1;s.targetIds=[...s.ids];s.corrections=[];s.retryScheduled=[];s.deferredReviews=[];return pool(s);}
function makeReview(ids,source,serial,now,targets,known){const s=make(ids,source,serial,now,'review');s.reviewKind=targets.every(q=>q.kind==='meaning')?'meaning':targets.every(q=>q.kind==='spelling')?'spelling':'all';s.practiceMode=s.reviewKind==='meaning'?'meaning':'full';s.queue=targets.map(q=>({id:q.id,kind:q.kind,filler:false}));s.targets=targets.map(q=>({id:q.id,kind:q.kind}));delete s.deferredKind;return pool(s,known);}
function addKnown(s,id){if(s.ids.includes(id))return true;if(s.ids.length>=8)return false;s.ids.push(id);s.choices[id]=T.make([id],s.source,s.serial,s.started).choices[id];s.gaps[id]=T.gap(id,s.serial);if(s.phase==='check'&&s.mode==='learn')s.learned.push(id);return true;}
function schedule(s,q){
  if(q.kind==='gap'||q.filler||q.followup||s.retryScheduled.includes(key(q)))return;
  s.retryScheduled.push(key(q));let others=new Set(),insert=-1;
  for(let i=s.at+1;i<s.queue.length;i++){const p=s.queue[i];if(p.id===q.id)others.clear();else others.add(p.id);if(others.size>=OTHER){insert=i+1;break;}}
  if(insert<0){
    const candidates=s.retryPool.filter(id=>id!==q.id&&!others.has(id));
    const needed=OTHER-others.size,available=candidates.filter(id=>s.ids.includes(id)).concat(candidates.filter(id=>!s.ids.includes(id)).slice(0,8-s.ids.length));
    if(available.length<needed){s.deferredReviews.push({id:q.id,kind:q.kind});return;}
    for(const id of available.slice(0,needed)){addKnown(s,id);s.queue.push({id,kind:q.kind,filler:true});others.add(id);}
    insert=s.queue.length;
  }
  s.queue.splice(insert,0,{id:q.id,kind:q.kind,filler:false,followup:true});
}
function check(s,unknown=false){
  if(s.reviewVersion!==1)return T.check(s,unknown);
  const q=T.current(s),saved=clone(s.results),r=T.check(s,unknown);
  if(q.followup){s.results=saved;s.corrections.push({id:q.id,kind:q.kind,...clone(r),afterFeedback:true});}
  else if(!actual(r))schedule(s,q);
  return r;
}
function next(s){
  if(s.reviewVersion!==1)return T.next(s);
  if(s.phase!=='check'||!s.checked)throw Error('先检查本题');
  if(s.at+1>=s.queue.length)return true;
  s.at++;s.selected=null;s.typed='';s.hint=false;s.checked=null;
  const q=T.current(s);if(!q.followup)return false;
  const last=s.queue.slice(0,s.at).map(p=>p.id).lastIndexOf(q.id),seen=new Set(s.queue.slice(last+1,s.at).filter(p=>p.id!==q.id).map(p=>p.id));
  if(seen.size>=OTHER)return false;
  // Other failing dimensions can insert a later exposure after the retry was planned.
  // Recheck spacing at dequeue time, against the latest actual exposure to this word.
  let at=-1;
  for(let i=s.at+1;i<s.queue.length;i++){if(s.queue[i].id===q.id)seen.clear();else seen.add(s.queue[i].id);if(seen.size>=OTHER){at=i;break;}}
  if(at>=0){s.queue.splice(s.at,1);s.queue.splice(at,0,q);return false;}
  const available=s.retryPool.filter(id=>id!==q.id&&!seen.has(id));
  const fillers=available.slice(0,OTHER-seen.size).filter(id=>addKnown(s,id));
  if(fillers.length+seen.size<OTHER){s.queue.splice(s.at,1);s.deferredReviews.push({id:q.id,kind:q.kind});return s.at>=s.queue.length;}
  s.queue.splice(s.at,1);s.queue.push(...fillers.map(id=>({id,kind:q.kind,filler:true})),q);return false;
}
function summarize(s,now){const r=T.summarize(s,now);if(s.reviewVersion!==1)return r;
  return {...r,reviewVersion:1,ids:s.targetIds,reviewKind:s.reviewKind,targets:s.reviewKind?s.targets:r.targets,corrections:s.corrections,deferredReviews:s.deferredReviews};
}
function updateMemory(memory,r,now){
  if(r.reviewVersion!==1)return T.updateMemory(memory,r,now);
  for(const id of r.ids)for(const kind of ['meaning','spelling']){
    const v=r.results[id]?.[kind];if(!v)continue;memory[id]=memory[id]||{};
    const old=memory[id][kind],correction=r.corrections.find(q=>q.id===id&&q.kind===kind),early=old&&now<old.due;
    if(!actual(v)||r.mode==='revisit'){
      memory[id][kind]={...(old||{}),stage:0,due:now+(actual(correction)?T.DAY:T.GAP),last:now,assisted:!!v.assisted,afterFeedback:true,reviewPolicy:1,independent:0,errors:(old?.errors||0)+(!v.correct?1:0),corrections:(old?.corrections||0)+(actual(correction)?1:0),needsCorrection:!actual(correction)};
    }else if(early){memory[id][kind]={...old,last:now};}
    else{const stage=Math.max(old?.stage||0,Math.min(3,old?.independent||0));memory[id][kind]={...(old||{}),stage,due:now+intervals[stage]*T.DAY,last:now,lastIndependent:now,assisted:false,afterFeedback:false,pending:false,needsCorrection:false,reviewPolicy:1,independent:(old?.independent||0)+1,errors:old?.errors||0,corrections:old?.corrections||0};}
  }
}
function effectiveDue(state,id,kind,row){let due=row.due;
  if(row.last&&row.due<=row.last&&(row.assisted||row.afterFeedback))due=Math.max(due,row.last+T.GAP);
  const last=state.batchLast,v=last?.results[id]?.[kind];if(v&&!actual(v)&&!last.corrections?.some(q=>q.id===id&&q.kind===kind&&actual(q)))due=Math.max(due,last.time+T.GAP);
  if(last?.ids.length===1&&last.ids[0]===id&&new Set([...Object.keys(state.memory||{}),...Object.keys(state.batchMemory||{})]).size===1)due=Math.max(due,last.time+T.GAP);
  return due;
}
function memoryRow(state,id,kind){return state.batchMemory?.[id]?.[kind]||(kind==='meaning'&&state.memory[id]?{...state.memory[id],last:0,legacy:true}:null);}
function reviewItems(state,now,kind='all',mode='due',ids=null){
  const known=[...new Set([...Object.keys(state.memory||{}),...Object.keys(state.batchMemory||{})])].filter(T.get),chosen=ids||(mode==='notebook'?[...(state.notebook||[]),...(state.extraNotebook||[])]:known),rows=[];
  for(const id of [...new Set(chosen)].filter(T.get))for(const k of ['meaning','spelling']){
    if(kind!=='all'&&kind!==k)continue;const m=memoryRow(state,id,k);
    if(mode==='due'&&!m)continue;const due=m?effectiveDue(state,id,k,m):now;
    rows.push({id,kind:k,due,ready:mode==='due'?due<=now:!(m&&due>now&&due<=Math.max(m.last||0,state.batchLast?.time||0)+T.GAP),memory:m,untested:!m});
  }
  return rows.sort((a,b)=>a.due-b.due||a.id.localeCompare(b.id)||a.kind.localeCompare(b.kind));
}
function stateLabel(m){if(!m)return '尚未检查';if(m.needsCorrection)return '还需重练';if(m.afterFeedback)return '纠正后待复习';if(m.reviewPolicy!==1)return '原记录保留';return m.independent<2?'首次记住，待巩固':m.independent<4?'已间隔回忆'+m.independent+'次':'已多次间隔回忆';}
function rowValid(v,kind){return v&&typeof v.correct==='boolean'&&typeof v.assisted==='boolean'&&typeof v.unknown==='boolean'&&(kind==='meaning'?v.typed===null:typeof v.typed==='string'&&v.typed.length<=120);}
function extensionsValid(s,result=false){
  if(s.reviewVersion!==undefined&&s.reviewVersion!==1)return false;
  if(s.reviewVersion!==1)return true;
  if(!Array.isArray(s.corrections)||s.corrections.length>16||s.corrections.some(q=>!s.ids.includes(q.id)||!kindName[q.kind]||!rowValid(q,q.kind)||q.afterFeedback!==true||actual(s.results[q.id]?.[q.kind])))return false;
  if(new Set(s.corrections.map(key)).size!==s.corrections.length)return false;
  if(!Array.isArray(s.deferredReviews)||s.deferredReviews.some(q=>!s.ids.includes(q.id)||!kindName[q.kind]))return false;
  if(s.reviewKind&&!['all','meaning','spelling'].includes(s.reviewKind))return false;
  if(result){if(s.reviewKind&&(!Array.isArray(s.targets)||!s.targets.length||s.targets.some(q=>!s.ids.includes(q.id)||!kindName[q.kind]||!s.results[q.id]?.[q.kind])))return false;return true;}
  return Array.isArray(s.targetIds)&&s.targetIds.length>0&&new Set(s.targetIds).size===s.targetIds.length&&s.targetIds.every(id=>s.ids.includes(id))&&Array.isArray(s.retryPool)&&s.retryPool.length<=T.catalog.length&&s.retryPool.every(T.get)&&Array.isArray(s.retryScheduled)&&s.retryScheduled.length<=16&&s.queue.length<=128&&s.queue.every(q=>q.followup===undefined||q.followup===true&&!q.filler);
}
function validate(s){try{if(!extensionsValid(s))return false;let t=s;if(s?.reviewVersion===1&&s.queue.length>48){t=clone(s);const from=Math.max(0,s.at-23);t.queue=t.queue.slice(from,from+48);t.at-=from;}return T.validate(t);}catch{return false;}}
function resultValid(r){if(r?.reviewVersion!==1)return T.resultValid(r);return extensionsValid(r,true)&&T.resultValid(r.reviewKind?{...r,mode:'revisit'}:r);}
function fieldsValid(x){try{if(x.batch&&!validate(x.batch)||x.batchLast&&!resultValid(x.batchLast)||x.batchLog?.some(r=>!resultValid(r)))return false;
  for(const row of Object.values(x.batchMemory||{}))for(const v of Object.values(row))if(v.reviewPolicy!==undefined&&(v.reviewPolicy!==1||!Number.isInteger(v.independent)||v.independent<0||!Number.isInteger(v.errors)||v.errors<0||!Number.isInteger(v.corrections)||v.corrections<0||typeof v.needsCorrection!=='boolean'))return false;
  const t=clone(x);delete t.batch;delete t.batchLast;delete t.batchLog;return T.fieldsValid(t);
}catch{return false;}}
const api={...T,OTHER,kindName,intervals,pool,make,makeReview,check,next,summarize,updateMemory,reviewItems,memoryRow,stateLabel,validate,resultValid,fieldsValid};if(node)module.exports=api;else root.WORDCARD_TRAINING=api;
})(typeof window==='object'?window:globalThis);
