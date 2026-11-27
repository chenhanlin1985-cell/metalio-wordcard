'use strict';
const fs=require('node:fs'),path=require('node:path');
const keyOf=m=>m.id+'@'+m.version;
function openLibrary(directory){
 const records=new Map();
 function refresh(){
  const next=new Map();
  for(const group of fs.readdirSync(directory,{withFileTypes:true})){
   if(!group.isDirectory()||group.name.startsWith('.'))continue;
   for(const v of fs.readdirSync(path.join(directory,group.name),{withFileTypes:true})){
    if(!v.isDirectory())continue;
    const dir=path.join(directory,group.name,v.name),key=group.name+'@'+v.name;
    if(records.has(key)){next.set(key,records.get(key));continue;}
    const m=JSON.parse(fs.readFileSync(path.join(dir,'manifest.json'),'utf8'));
    if(keyOf(m)!==key)throw Error('包目录和清单不一致');
    next.set(key,{m,dir,data:JSON.parse(fs.readFileSync(path.join(dir,'content.json'),'utf8'))});
   }
  }
  records.clear();for(const [k,r] of next)records.set(k,r);
 }
 function problem(r){
  if(r.m.type==='deck')return '';
  const dep=r.m.requires,deck=records.get(dep);
  if(!deck||deck.m.type!=='deck')return '需要安装词库 '+dep;
  if(r.m.schema===2){
   for(const [term,t] of Object.entries(r.data.terms))
    if(t.id&&deck.data.words[t.id]?.word!==term)return '剧情词和词库编号不符 '+term;
   return '';
  }
  for(const c of r.data.chapters){
   if(!Object.hasOwn(deck.data.topics,c.topic))return '缺少词库主题 '+c.topic;
   for(const word of c.words)if(deck.data.words[word.id]?.topic!==c.topic)return '章节缺少对应词条 '+word.id;
  }
  return '';
 }
 function compose(key){
  const r=records.get(key);if(!r)throw Error('尚未安装这个包');
  const why=problem(r);if(why)throw Error(why);
  const plain=r.m.type==='deck',d=plain?r:records.get(r.m.requires),deckKey=keyOf(d.m);
  if(r.m.schema===2){
   const terms={};
   for(const [term,t] of Object.entries(r.data.terms)){
    const w=t.id&&d.data.words[t.id];
    terms[term]=w?{...t,ipa:w.ipa,audio:w.audio?'/packs/'+deckKey+'/'+w.audio:null}:t;
   }
   return {...r.data,key,title:r.m.title,deckKey,deckSha256:d.m.files['content.json'],terms};
  }
  return require('./compose.cjs').composeContent(r,d,key,deckKey,'packs/'+deckKey+'/');
 }
 function list(){return Array.from(records.values(),r=>({key:keyOf(r.m),title:r.m.title,type:r.m.type,version:r.m.version,needs:r.m.requires||null,problem:problem(r),count:r.m.type==='deck'?Object.keys(r.data.words).length:r.data.chapters.length}));}
 refresh();return {refresh,list,compose,get:key=>records.get(key)};
}
module.exports={openLibrary};
