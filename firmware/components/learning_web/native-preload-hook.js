// Staged, disposable projection. Prepare the likely correct-answer successor
// while reading/listening. Never publish predicted learner progress.
globalThis.nativePrepareStep=()=>{
 if(globalThis.nativePreloadDisabled)return false;
 const source=nativePrepSource(),b=model.state.batch,s=model.state.session;
 const action=route==='quiz'&&s?'next':route==='word-batch'&&b?.checked?'w-next':null;
 if(!source||!action||playing||modal||model.state.lookup||model.error||pathModel.error||nativeSound||document.querySelector('#toast').textContent){nativePreparationJob=null;return false;}
 if(nativePreparationJob&&nativePreparationJob.source!==source)nativePreparationJob=null;
 if(nativePrepared?.source===source)return false;
 const job=nativePreparationJob;
 const md=Object.getOwnPropertyDescriptors(model),pd=Object.getOwnPropertyDescriptors(pathModel);
 const old={route,readPage,lastLesson,legacySummary,audioNote,children:app.children,html:app._html,projection:globalThis.nativeProjectionModel,direct:nativeDirect};
 nativePreparationRestore=()=>{
  for(const k of Object.keys(model))if(!(k in md))delete model[k];Object.defineProperties(model,md);
  for(const k of Object.keys(pathModel))if(!(k in pd))delete pathModel[k];Object.defineProperties(pathModel,pd);
  ({route,readPage,lastLesson,legacySummary,audioNote}=old);app.children=old.children;app._html=old.html;
  globalThis.nativeProjectionModel=old.projection;
  nativeDirect=old.direct;
 };
 try{
  nativeDirect=null;
  if(!job){
   const raw=JSON.stringify(model.state),profile=JSON.stringify(pathModel.state);
   if(raw.length>49152||profile.length>24576)return false;
   nativePreparationJob={source,revision:nativeRevision,action,stage:action==='next'&&!s.checked?'check':'advance',course:JSON.parse(raw),profile:JSON.parse(profile),route,readPage,lastLesson,legacySummary};return true;
  }
  model.state=job.course;pathModel.state=job.profile;
  ({route,readPage,lastLesson,legacySummary}=job);
  const scratch={getItem:()=>null,setItem(){}};model.storage=scratch;pathModel.storage=scratch;
  if(job.stage==='check'){
   const current=model.state.session,t=M.task(current);if(!t||t.kind==='learn')return false;
   current.selected=t.answer;if(t.kind==='listen')current.heard={...current.heard,[current.at]:true};
   globalThis.nativeProjectionModel=model;
   const checked=model.check();globalThis.nativeProjectionModel=old.projection;if(!checked)return false;
   job.course=model.state;job.readPage=0;job.stage='advance';return true;
  }
  if(job.stage==='advance'){
   if(action==='next'){
    const current=model.state.session;legacySummary=!!current.legacy;const focused=current.purpose==='focus';lastLesson=current.lesson;
    if(!model.advance())return false;route=model.state.session?'quiz':focused?'focus-summary':'summary';
    if(route==='quiz')readPage=model.state.session.page||0;
   }else{if(!model.batchNext())return false;route=model.state.batch?'word-batch':'word-result';}
   Object.assign(job,{course:model.state,profile:pathModel.state,route,readPage,lastLesson,legacySummary,stage:'html'});return true;
  }
  ({route,readPage,lastLesson,legacySummary}=job);audioNote='';
  if(job.stage==='html'){
   const direct=nativeQuizView();
   if(direct){nativePrepared={revision:nativeRevision,source:job.source,action:job.action,html:null,children:null,view:direct,identity:nativePrepIdentity(),token:++nativePrepStats.built};nativePreparationJob=null;return false;}
   const html=nativePageContent();if(html.length>32768)return false;job.html=html;job.stage='parse';return true;
  }
  const html=job.html,children=parseHTML(html,app);app.children=children;app._html=html;
  if(globalThis.nativePreparationFault)throw Error('injected preparation failure');
  const view=nativeView(),identity=nativePrepIdentity();
  nativePrepared={revision:nativeRevision,source:job.source,action:job.action,html,children,view,identity,token:++nativePrepStats.built};nativePreparationJob=null;return false;
 }finally{const restore=nativePreparationRestore;if(restore)restore();nativePreparationRestore=null;}
};
