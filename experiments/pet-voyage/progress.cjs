'use strict';
const shared=['cards','practised','recent'];
const storeKey=deck=>'wordcard-library/1/'+deck;
function split(state){
 const run={...state},memory={};for(const k of shared){memory[k]=run[k];delete run[k];}
 return {run,memory};
}
function read(store,pack,fresh){
 const key=storeKey(pack.deckKey),saved=store.getItem(key);
 let record;
 if(saved){
  record=JSON.parse(saved);
  if(record.schema!==1||record.deck!==pack.deckSha256||!record.runs||!record.memory)throw Error('存档与词库版本不匹配');
 }else{
  record={schema:1,deck:pack.deckSha256,rev:0,runs:{},memory:{cards:{},recent:[],practised:{}}};
 }
 const state={...(record.runs[pack.key]||split(fresh()).run),...record.memory};
 return {record,state};
}
function write(store,pack,fresh,state,rev){
 const {record}=read(store,pack,fresh);
 if(record.rev!==rev)throw Error('其他页面已更新进度，请刷新后继续。');
 const {run,memory}=split(state);
 const changed=(run.stamps?.length||0)!==(record.runs[pack.key]?.stamps?.length||0);
 record.stampSchema=1;record.runs[pack.key]=run;record.memory=memory;record.rev++;
 store.setItem(storeKey(pack.deckKey),JSON.stringify(record),changed);return record.rev;
}
module.exports={read,write,storeKey};
