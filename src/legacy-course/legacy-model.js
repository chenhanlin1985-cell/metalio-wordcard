(function(root){
'use strict';
const data=typeof module==='object'&&module.exports?require('./legacy-data.js'):root.WORDCARD_LEGACY_DATA;
const KEY='wordcard-adult-web-v1';
const words=data.lessons.flatMap(x=>x.words);
const getLesson=id=>data.lessons.find(l=>l.id===id);
function tasks(id){
 const l=getLesson(id);if(!l)return[];
 const list=[];
 for(let i=0;i<l.words.length;i+=2)list.push({id:`pair${i/2}`,kind:'learn',title:'在句子里认识表达',wordIds:l.words.slice(i,i+2).map(w=>w.id)});
 l.words.forEach((w,i)=>{const choices=[w.meaning,l.words[(i+1)%l.words.length].meaning,l.words[(i+2)%l.words.length].meaning];const shift=(i+1)%3;const options=choices.slice(shift).concat(choices.slice(0,shift));list.push({id:`word${i}`,kind:'meaning',wordId:w.id,title:`${w.word} 在这里是什么意思？`,text:w.example,options,answer:options.indexOf(w.meaning),why:`${w.word} 在这里表示“${w.meaning}”。`});});
 l.readQuestions.forEach((q,i)=>list.push({id:`read${i}`,kind:'read',title:'读懂这段信息',text:l.reading,q:q[0],options:q[1],answer:q[2],why:q[3]}));
 l.dialogues.forEach((d,i)=>list.push({id:`listen${i}`,kind:'listen',title:'先听，再选择',text:d.text,q:d.q,options:d.options,answer:d.answer,why:d.why}));
 list.push({id:'gap',kind:'gap',title:'把表达放回句子',text:l.gap[0],options:l.gap[1],answer:l.gap[2],why:l.gap[3]});
 list.push({id:'transfer',kind:'transfer',title:'换个情境，仍然读得懂',q:l.transfer[0],options:l.transfer[1],answer:l.transfer[2],why:l.transfer[3]});
 return list;
}
const task=(s)=>tasks(s.lesson).find(t=>t.id===s.queue[s.at]);
function fresh(){return {schema:1,content:data.version,selectedBook:'empower-a2',selectedUnit:1,completed:{},visits:{},session:null,notebook:[],memory:{},events:[]};}
function validate(x){
 if(!x||x.schema!==1||x.content!==data.version||!data.books.some(b=>b.id===x.selectedBook))return false;
 if(!Number.isInteger(x.selectedUnit)||x.selectedUnit<1||x.selectedUnit>12||!x.completed||!x.visits||!x.memory||!Array.isArray(x.notebook)||!Array.isArray(x.events))return false;
 if(x.notebook.some(id=>!words.some(w=>w.id===id))||new Set(x.notebook).size!==x.notebook.length||Object.keys(x.memory).some(id=>!words.some(w=>w.id===id)))return false;
 if(Object.values(x.memory).some(m=>!m||!Number.isFinite(m.due)||!Number.isInteger(m.success)||m.success<0))return false;
 if(Object.entries(x.completed).some(([id,c])=>!getLesson(id)||!c||!Array.isArray(c.skipped)||!Array.isArray(c.heard)||!Number.isFinite(c.time)||c.skipped.some(k=>!tasks(id).some(t=>t.id===k&&t.kind==='listen'))||c.heard.some(k=>!tasks(id).some(t=>t.id===k&&t.kind==='listen'))))return false;
 const s=x.session;
 if(s){
  const known=tasks(s.lesson);const ids=known.map(t=>t.id);
  if(!getLesson(s.lesson)||!Array.isArray(s.queue)||s.queue.length<1||s.queue.length>400||s.queue.some(id=>!ids.includes(id))||!Number.isInteger(s.at)||s.at<0||s.at>=s.queue.length||!Array.isArray(s.skipped)||!s.first||!s.heard||!s.errors||!Array.isArray(s.errors)||s.errors.some(k=>!ids.includes(k))||!Number.isInteger(s.selected)&&s.selected!==null)return false;
  if(Object.entries(s.heard).some(([k,v])=>!Number.isInteger(+k)||+k<0||+k>=s.queue.length||v!==true))return false;
  if(s.selected!==null&&(!task(s).options||s.selected<0||s.selected>=task(s).options.length))return false;
  if(s.checked!==null&&(!s.checked||typeof s.checked.correct!=='boolean'||typeof s.checked.unknown!=='boolean'||task(s).kind==='learn'))return false;
  if(Object.entries(s.first).some(([k,v])=>!ids.includes(k)||typeof v!=='boolean'))return false;
 }
 return true;
}
function newSession(id,only){const all=tasks(id);return {lesson:id,queue:only||all.map(t=>t.id),at:0,selected:null,checked:null,heard:{},skipped:[],first:{},errors:[],mode:only?'catchup':'lesson'};}
class Model{
 constructor(storage,clock=()=>Date.now()){this.storage=storage;this.clock=clock;this.state=fresh();this.error='';this.corrupt=false;try{const raw=storage.getItem(KEY);if(raw){const x=JSON.parse(raw);if(!validate(x))throw Error('invalid');this.state=x;}}catch(e){this.corrupt=true;this.error='浏览器记录无法读取，已保留原数据。请先导出原记录。';}}
 commit(fn){if(this.corrupt)return false;const next=JSON.parse(JSON.stringify(this.state));try{fn(next);if(!validate(next))throw Error('invalid state');this.storage.setItem(KEY,JSON.stringify(next));this.state=next;this.error='';return true;}catch(e){this.error='学习记录未保存，请导出进度或检查浏览器存储后重试。';return false;}}
 selectBook(id){return this.commit(x=>{if(!data.books.some(b=>b.id===id))throw Error();x.selectedBook=id;});}
 selectUnit(n){return this.commit(x=>{if(n<1||n>12)throw Error();x.selectedUnit=n;});}
 begin(id,only){if(this.state.session)return this.state.session.lesson===id;return this.commit(x=>{if(!getLesson(id)||only&&only.some(k=>!tasks(id).some(t=>t.id===k&&t.kind==='listen')))throw Error();x.session=newSession(id,only);});}
 choose(n){return this.commit(x=>{const s=x.session,t=task(s);if(!t.options||s.checked||n<0||n>=t.options.length)throw Error();s.selected=n;});}
 heard(at){return this.commit(x=>{const s=x.session;if(s.at!==at||task(s).kind!=='listen'||s.checked)throw Error();s.heard[at]=true;});}
 check(unknown=false){return this.commit(x=>{const s=x.session,t=task(s);if(s.checked||t.kind==='learn'||!unknown&&s.selected===null||t.kind==='listen'&&!s.heard[s.at])throw Error();const correct=!unknown&&s.selected===t.answer;s.checked={correct,unknown};if(!(t.id in s.first))s.first[t.id]=correct;if(!correct&&!s.errors.includes(t.id))s.errors.push(t.id);if(correct)s.errors=s.errors.filter(id=>id!==t.id);if(t.wordId){const m=x.memory[t.wordId]||{success:0,due:this.clock()};if(correct)m.success++;m.due=this.clock()+(correct?86400000:0);x.memory[t.wordId]=m;}});}
 skip(){return this.commit(x=>{const s=x.session,t=task(s);if(t.kind!=='listen'||s.checked)throw Error();if(!s.skipped.includes(t.id))s.skipped.push(t.id);s.checked={correct:false,unknown:false,skip:true};});}
 advance(){return this.commit(x=>{const s=x.session,t=task(s);if(t.kind!=='learn'&&!s.checked)throw Error();if(t.kind==='learn')t.wordIds.forEach(id=>{if(!x.memory[id])x.memory[id]={success:0,due:this.clock()+86400000};});
 if(s.checked&&!s.checked.correct&&!s.checked.skip)s.queue.push(t.id);
 if(s.checked?.correct&&t.kind==='listen')s.skipped=s.skipped.filter(k=>k!==t.id);
 x.visits[s.lesson]=(x.visits[s.lesson]||0)+1;
 if(s.at+1<s.queue.length){s.at++;s.selected=null;s.checked=null;return;}
 const prev=x.completed[s.lesson];const heard=[...new Set((prev?.heard||[]).concat(s.queue.filter((id,i)=>tasks(s.lesson).find(t=>t.id===id).kind==='listen'&&s.heard[i]&&s.first[id]!==undefined)))];
 const skipped=s.mode==='catchup'?(prev?.skipped||[]).filter(k=>!heard.includes(k)):s.skipped;
 x.completed[s.lesson]={time:this.clock(),first:s.mode==='catchup'?(prev?.first||{}):s.first,heard,skipped};
 x.events.push({lesson:s.lesson,time:this.clock(),kind:s.mode,skipped:skipped.length});x.events=x.events.slice(-200);x.session=null;
 });}
 abandon(){return this.commit(x=>{x.session=null;});}
 add(id){return this.commit(x=>{if(!words.some(w=>w.id===id))throw Error();if(!x.notebook.includes(id))x.notebook.push(id);if(!x.memory[id])x.memory[id]={success:0,due:this.clock()};});}
 remove(id){return this.commit(x=>{x.notebook=x.notebook.filter(k=>k!==id);});}
 review(id,correct){return this.commit(x=>{if(!words.some(w=>w.id===id))throw Error();const m=x.memory[id]||{success:0,due:this.clock()};m.success=correct?m.success+1:0;m.due=this.clock()+(correct?Math.min(7,Math.max(1,m.success))*86400000:0);x.memory[id]=m;});}
 due(){return words.filter(w=>this.state.memory[w.id]&&this.state.memory[w.id].due<=this.clock());}
 complete(id){const c=this.state.completed[id];return !!c&&c.skipped.length===0;}
 unitCount(n){return data.lessons.filter(l=>l.unit===n&&this.complete(l.id)).length;}
 summaryNext(id){const l=getLesson(id),c=this.state.completed[id];if(!l||!c)return null;if(c.skipped.length)return {action:'catchup',id};const next=data.lessons.find(x=>x.unit===l.unit&&!this.complete(x.id));if(next)return {action:'start',id:next.id,code:next.code};return this.unitCount(l.unit)===4?{action:'unit-review',unit:l.unit}:{action:'lesson-review',id};}
 export(){return JSON.stringify(this.state,null,2);}
 import(raw){let x;try{x=JSON.parse(raw);}catch(e){this.error='文件不是有效 JSON。';return false;}if(!validate(x)){this.error='这份文件不属于当前网页试学，或内容不完整。原进度保留。';return false;}try{this.storage.setItem(KEY,JSON.stringify(x));this.state=x;this.corrupt=false;this.error='';return true;}catch(e){this.error='无法保存导入的进度，原记录保留。';return false;}}
}
function reviewChoices(id,seed=0){const w=words.find(x=>x.id===id);if(!w)return null;let h=2166136261;for(const c of id+':'+seed){h=Math.imul(h^c.charCodeAt(0),16777619)>>>0;}const pool=[...new Set(words.filter(x=>x.meaning!==w.meaning).map(x=>x.meaning))],a=h%pool.length,b=(a+1+(h>>>8)%(pool.length-1))%pool.length,right=(h>>>16)%3,options=[pool[a],pool[b]];options.splice(right,0,w.meaning);return {options,right};}
const api={KEY,Model,tasks,task,getLesson,words,validate,fresh,data,reviewChoices};if(typeof module==='object'&&module.exports)module.exports=api;else root.WORDCARD_LEGACY_MODEL=api;
})(typeof window==='object'?window:globalThis);
