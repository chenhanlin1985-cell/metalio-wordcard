(function(root){'use strict';const node=typeof module==='object'&&module.exports,P=node?require('./adaptive-model.js'):root.WORDCARD_PATH,D=node?require('./daily-engine.js'):root.WORDCARD_DAILY;
class Model extends P.Model{
 constructor(storage,clock){super(storage,clock);if(!this.corrupt&&(!D.logValid(this.state.dailyLog)||!D.logValid(this.state.dailySkip))){this.corrupt=true;this.error='每日听读记录未能读取，原始计划保留，请先导出。';this.state=P.fresh();}}
 commit(fn){const before=this.state;return super.commit(x=>{fn(x);if(this.dailyEvent)D.recordProfile(before,x,this.clock());if(!D.logValid(x.dailyLog)||!D.logValid(x.dailySkip))throw Error('每日听读记录校验未通过');});}
 trainNext(){this.dailyEvent=true;try{return super.trainNext();}finally{this.dailyEvent=false;}}
 trainSkip(){this.dailyEvent=true;try{return super.trainSkip();}finally{this.dailyEvent=false;}}
 import(raw){try{const x=JSON.parse(raw);if(!D.logValid(x.dailyLog)||!D.logValid(x.dailySkip))throw Error();return super.import(raw);}catch{this.error='导入失败，原摸底与计划保留。';return false;}}
}
const api={...P,Model,validate:x=>P.validate(x)&&D.logValid(x.dailyLog)};if(node)module.exports=api;else root.WORDCARD_PATH=api;
})(typeof window==='object'?window:globalThis);
