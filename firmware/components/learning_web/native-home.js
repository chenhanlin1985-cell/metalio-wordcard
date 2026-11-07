// Native information architecture only. The pinned learning rules and storage
// stay in the original web scripts; each entry uses their existing action.
function nativeHome(view){
 const s=globalThis.nativeDailySnapshot;if(!s||view.corrupt)return view;
 const {model,pathModel:profile,M,P}=nativeDebug,B=M.wordBatch;
 const batch=model.state.batch||model.state.dailyPausedBatch;
 const nextStage=s.stages.find(t=>t.kind===s.next.kind);
 // Follow the scheduler's active task, not whichever old state happens to
 // exist. Batch, saved batch, course and bridge practice can all coexist.
 const active=s.active?.type;
 const source=['batch','saved-batch','review'].includes(active)?batch.source:
  active==='course'?{type:'course',id:model.state.session.lesson}:
  active==='practice'?{type:'bridge',id:profile.state.training.pack}:
  active==='assessment'?null:nextStage?.target||{type:'course',id:s.plan.lesson};
 const lesson=source?.type==='course'?M.getLesson(source.id):null;
 const pack=source?.type==='bridge'?P.practice.pack(source.id):null;
 const current=active==='assessment'?'继续已保存的起点摸底':pack?pack.title:lesson?lesson.code+' · '+lesson.title:source?.type==='review'?B.sourceTitle(source):'当前学习安排';
 const track=active==='assessment'?'词汇、听力与阅读':pack?'日常与科技 · 听读练习':lesson?'Empower A2 · 第 '+lesson.unit+' 单元':'自主复习';
 const primary=view.blocks.find(b=>b.kind==='button'&&b.primary);
 const add=(text,data,role,detail='')=>{const action=view.actions.length;view.actions.push(data);return {kind:'button',text,detail,area:'body',role,action,enabled:true};};
 let next='';
 if(['review','active-review'].includes(s.next.type))next='先巩固到期词义与拼写，再学新内容';
 else if(batch&&['batch','saved-batch'].includes(s.next.type))next=batch.phase==='learn'?'认识新词 · 第 '+(batch.learnAt+1)+' / '+batch.ids.length+' 个':batch.phase==='overview'?'已准备好一组词，选择训练方式':'检查词义与拼写 · 第 '+(batch.at+1)+' / '+batch.queue.length+' 项';
 else if(s.active)next=(s.active.type==='assessment'?'起点摸底':'听读练习')+' · 第 '+(s.active.answered+1)+' / '+s.active.total+' 项';
 else if(s.next.type==='done')next=s.study?'今日安排已完成，明天接着学':'今天休息，可按需回顾';
 else if(s.next.type==='available-end')next='当前可用材料已练完，可以回顾已学内容';
 else if(s.next.type==='waiting')next=s.deferredTargets.length?'还有暂缓听力，可以接着补听':'错项稍后再练，当前进度已保存';
 else next='按今天的安排，先完成一小组';
 const details=view.blocks.filter(b=>b.role==='detail');
 const stages=view.blocks.filter(b=>b.role==='progress').map((b,i)=>({...b,detail:i<2&&s.stages[i].total===0?'无到期':b.detail}));
 const remaining=s.stages.reduce((n,t)=>n+t.remaining,0);
 const summary=remaining?'今日还剩 '+remaining+' 项':s.cooling?'稍后再练 '+s.cooling+' 项':!s.study?'休息日，按需回顾':s.next.type==='available-end'?'当前材料已练完':'今日安排已完成';
 if(nativeHomeOptions){
  view.title='学习安排';view.route='home-options';view.top=[];
  view.blocks=[{kind:'text',role:'section-heading',text:summary,area:'body'},
   {kind:'text',role:'section-note',text:s.provisional?'试学安排，可调整方向与时间':P.goals[s.plan.goal]+' · 每天 '+s.plan.minutes+' 分钟',area:'body'},
   add('今日分项进度',{action:'p-today',homeReturn:'path-today'},'section-row','查看词义、拼写、新词与听读'),
   add('学习计划',profile.state.plan?{action:'go',route:'path-plan',homeReturn:'path-plan'}:{action:'p-edit'},'section-row','调整学习方向与时间'),
   add(profile.state.attempt?'继续摸底':profile.state.report?'摸底报告':'起点摸底',profile.state.attempt?{action:'p-resume'}:{action:'go',route:profile.state.report?'path-report':profile.state.goal?'path-brief':'path-goal',homeReturn:profile.state.report?'path-report':profile.state.goal?'path-brief':'path-goal'},'section-row','判断起点，查看课程建议'),
   add('课内生词本 · '+model.notebookIds().length,{action:'u-notebook',homeReturn:'word-list'},'section-row','查看收藏的课程词语')];
  return view;
 }
 view.title='首页';view.top=[];
 view.blocks=[
  {kind:'text',role:'home-meta',area:'body',text:s.date.slice(5).replace('-','月')+'日 · '+(s.study?(s.provisional?'试学安排':P.goals[s.plan.goal])+' · '+s.plan.minutes+'分钟':'休息日 · 按需回顾')},
  {kind:'text',role:'home-course',area:'body',text:track,detail:current},
  {kind:'text',role:'home-next',area:'body',text:next},
  ...(primary?[{...primary,text:['batch','saved-batch','course','practice'].includes(s.next.type)?'继续学习':primary.text,role:'home-primary'}]:[]),
  {kind:'text',role:'home-summary',area:'body',text:summary},
  add('学习安排',{action:'native-home-options'},'home-today'),
  ...details
 ];
 return view;
}
