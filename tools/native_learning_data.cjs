// Build-time data export only. No executable JavaScript is embedded here.
const fs=require('fs'),path=require('path'),crypto=require('crypto');
const root=path.resolve(__dirname,'..'),src=path.join(root,'src/legacy-course');
const M=require(path.join(src,'model.js')),L=require(path.join(src,'legacy-model.js')),B=require(path.join(src,'review-engine.js')),P=require(path.join(src,'path-practice.js'));
const audio=fs.readFileSync(path.join(src,'audio-map.js'),'utf8');
const vm=require('vm'),scope={};scope.window=scope;vm.runInNewContext(audio,scope);
const data={version:M.data.version,catalog:B.catalog,lessons:M.data.lessons,units:M.data.units,
 tasks:Object.fromEntries(M.data.lessons.map(l=>[l.id,M.tasks(l.id)])),
 legacyTasks:Object.fromEntries(L.data.lessons.map(l=>[l.id,L.tasks(l.id)])),packs:P.packs,
 bridgeTasks:Object.fromEntries(P.packs.map(p=>[p.id,Object.fromEntries(['words','read','listen'].map(k=>[k,P.tasks(p.id,k)]))])),audio:scope.WORDCARD_AUDIO};
if(!data.audio||Object.keys(data.audio).length<100)throw Error('Audio data export failed');
const raw=JSON.stringify(data),dir=path.join(root,'firmware/components/native_learning/generated');fs.mkdirSync(dir,{recursive:true});
// MSVC limits each UTF-8 literal by its encoded byte length, not JS UTF-16 length.
const chunks=[];for(let i=0;i<raw.length;i+=3000)chunks.push(raw.slice(i,i+3000));
if(raw.includes(')NATIVE_DATA"'))throw Error('Data delimiter collision');
fs.writeFileSync(path.join(dir,'learning_data.cpp'),'#include <string>\nnamespace wordcard {\n'+chunks.map((s,i)=>`static const char chunk${i}[]=R"NATIVE_DATA(${s})NATIVE_DATA";`).join('\n')+'\nstd::string native_learning_json(){std::string result;result.reserve('+Buffer.byteLength(raw)+');'+chunks.map((_,i)=>`result+=chunk${i};`).join('')+'return result;}\n}\n');
fs.writeFileSync(path.join(dir,'provenance.json'),JSON.stringify({source:'src/legacy-course',dataOnly:true,sha256:crypto.createHash('sha256').update(raw).digest('hex'),bytes:Buffer.byteLength(raw),words:data.catalog.length,lessons:data.lessons.length},null,2));
console.log('Native immutable data: '+Buffer.byteLength(raw)+' bytes, '+data.catalog.length+' words');
