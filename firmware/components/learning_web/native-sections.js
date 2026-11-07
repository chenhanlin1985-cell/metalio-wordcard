// Device-sized information architecture. Learning state and rules remain in
// the pinned engines; these views only expose existing actions and counts.
function nativeSections(view){
 if(view.corrupt)return view;
 const {model:m,M}=nativeDebug,route=view.route;
 const add=(text,detail,data,primary=false,enabled=true,role='section-row')=>{
  const action=view.actions.length;view.actions.push({...data,nativeFrom:route});
  return {kind:'button',text,detail,action,primary,enabled,role,area:'body'};
 };
 const text=(s,role='section-note')=>({kind:'text',text:s,role,area:'body'});
 const roots=['home','books','review','dictionary'];
 const owner=route==='books'||['book','units','unit','lesson','course-plan','planned'].includes(route)?'books':
  route==='review'||['word-review','review-schedule','unit-recall'].includes(route)?'review':
  route==='dictionary'?'dictionary':null;
 if(owner&&!view.tabs.length)view.tabs=roots.map((r,i)=>{
  const action=view.actions.length;view.actions.push({action:'go',route:r});
  return {text:['首页','课程','复习','词典'][i],action,selected:r===owner};
 });
 if(route==='books'&&!nativeOtherBooks){
  view.title='课程';view.top=[];
  const unit=n=>{
   const lessons=M.data.lessons.filter(l=>l.unit===n&&l.role==='lesson');
   const count=lessons.reduce((sum,l)=>sum+l.words.length,0);
   return add('第 '+n+' 单元 · '+(n===1?'认识与交流':'工作与生活'),count+' 个核心表达 · '+m.unitCount(n)+'/4 课完成',{action:'unit',number:n});
  };
  const tech=M.getLesson('u3b');
  view.blocks=[text('Empower A2','section-heading'),text('按课连续学，也可先选科技英语。'),
   unit(1),unit(2),...(tech?[add('科技英语 · '+tech.code,tech.title,{action:'lesson',id:tech.id})]:[]),
   add('整册目录','查看全部单元与开放情况',{action:'go',route:'units'}),
   add('其他教材与课程说明','',{action:'native-other-books'},false,true,'section-link'),
   text('当前可学：前两单元与 3B 原创试学。','detail')];
 }else if(route==='books'){
  view.title='其他教材与课程说明';view.tabs=[];
  view.top=[{text:'返回',action:view.actions.push({action:'native-books'})-1,enabled:true}];
 }else if(route==='review'){
  view.title='复习';view.top=[];
  const old=view.blocks,find=a=>old.find(b=>b.action>=0&&view.actions[b.action]?.action===a);
  const meaning=m.reviewItems('meaning').filter(q=>q.ready).length;
  const spelling=m.reviewItems('spelling').filter(q=>q.ready).length;
  const notice=m.reviewNotice;
  view.blocks=[text(meaning+spelling?'先复习到期内容':'今天暂无到期词汇','section-heading'),
   text('词义 '+meaning+' 项 · 拼写 '+spelling+' 项'),
   ...(find('w-resume')?[{...find('w-resume'),text:'继续未完成的背词',detail:'',role:'section-link'}]:[]),
   add('复习词义',meaning+' 项到期',{action:'w-r-start',kind:'meaning',mode:'due'},true,meaning>0),
   add('复习完整拼写',spelling+' 项到期',{action:'w-r-start',kind:'spelling',mode:'due'},false,spelling>0),
   add('听读回顾',m.dueUnits().length+' 个单元到期',{action:'go',route:'review-schedule'}),
   add('错词与复习日程','查看分项状态，也可主动加练',{action:'go',route:'word-review'}),
   add('课内生词本 · '+m.notebookIds().length,'',{action:'u-notebook'},false,true,'section-link'),
   text(notice?notice.message:'错后隔词再练；一次答对不算掌握。','detail')];
 }else if(route==='dictionary'){
  view.title='词典';view.top=[];
  view.blocks=[text('查词，留作自己的词汇','section-heading'),text('输入英文，查看词义、音标并听发音。'),
   add('输入查询','日常、科技与完整离线词典',{action:'native-device-dictionary'},true,true,'device-dictionary'),
   add('课内词语与表达',M.wordBatch.catalog.length+' 项 · 含课程例句',{action:'u-dictionary-input'}),
   add('课内生词本 · '+m.notebookIds().length,'进入词义与拼写复习',{action:'u-notebook'}),
   add('我的查词生词本','查看从离线词典加入的词',{action:'native-device-notebook'},false,true,'device-notebook'),
   text('学习中点“查词”，返回时仍在原位置。','detail')];
 }
 // Notebook/schedule back follows its actual invoking root, without storing
 // navigation state in learner files or replacing the saved lookup point.
 for(const b of view.blocks)if(b.action>=0&&['u-notebook'].includes(view.actions[b.action]?.action))view.actions[b.action].nativeFrom=route;
 return view;
}
