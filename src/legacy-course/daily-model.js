(function(root){'use strict';const node=typeof module==='object'&&module.exports,U=node?require('./unified-model.js'):root.WORDCARD_MODEL,D=node?require('./daily-engine.js'):root.WORDCARD_DAILY,B=U.wordBatch;
function valid(x){return D.logValid(x.dailyLog)&&D.logValid(x.dailySkip)&&(!x.dailyPausedBatch||B.validate(x.dailyPausedBatch));}
class Model extends U.Model{
 constructor(storage,clock){super(storage,clock);if(!this.corrupt&&!valid(this.state)){this.corrupt=true;this.error='每日记录未能读取，原始记录保留，请先导出。';this.state=U.fresh();}}
 commit(fn){const before=this.state;return super.commit(x=>{fn(x);if(this.dailyEvent)D.recordCourse(before,x,this.clock());if(!valid(x))throw Error('每日学习记录校验未通过');});}
 advance(){this.dailyEvent=true;try{return super.advance();}finally{this.dailyEvent=false;}}
 batchNext(){this.dailyEvent=true;try{return super.batchNext();}finally{this.dailyEvent=false;}}
 dailyFocus(id,kind,ids){if(this.state.session){this.error='原来的听读位置已保存，请先继续。';return false;}return this.commit(x=>{const tasks=U.tasks(id);if(!['read','listen'].includes(kind)||!Array.isArray(ids)||!ids.length||ids.length>8||new Set(ids).size!==ids.length||ids.some(key=>!tasks.some(t=>t.id===key&&t.kind===kind)))throw Error('今日听读任务无效');x.session={lesson:id,legacy:false,purpose:'focus',mode:'focus',focus:kind,focusCatchup:false,baseLength:ids.length,queue:[...ids],at:0,page:0,selected:null,checked:null,heard:{},skipped:[],first:{},errors:[]};});}
 dailyReview(kind,limit=6){if(this.state.batch?.source.type==='review')return this.batchResume();this.reviewNotice=null;const rows=this.reviewItems(kind).filter(q=>q.ready),ids=[...new Set(rows.map(q=>q.id))].slice(0,limit),targets=rows.filter(q=>ids.includes(q.id));if(!targets.length){this.reviewNotice={kind,due:null,message:'当前没有这项到期复习。'};return false;}return this.commit(x=>{if(x.batch){if(x.dailyPausedBatch)throw Error('已保留一组背词，请先继续当前任务。');x.dailyPausedBatch=x.batch;}x.batchSerial=(x.batchSerial||0)+1;x.batch=B.makeReview(ids,{type:'review',id:'due'},x.batchSerial,this.clock(),targets,this.batchKnown(x));});}
 dailyResume(){if(this.state.batch)return this.batchResume();if(!this.state.dailyPausedBatch)return true;return this.commit(x=>{x.batch=x.dailyPausedBatch;x.batch.paused=false;delete x.dailyPausedBatch;});}
 import(raw){try{const x=JSON.parse(raw);if(!valid(x))throw Error();return super.import(raw);}catch{this.error='导入失败，原记录保留，请检查每日记录。';return false;}}
}
const api={...U,Model,daily:D,validate:x=>U.validate(x)&&valid(x)};if(node)module.exports=api;else root.WORDCARD_MODEL=api;
})(typeof window==='object'?window:globalThis);
