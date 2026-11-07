const fs=require('fs'),path=require('path'),crypto=require('crypto');
const root=path.resolve(__dirname,'..'),web=path.join(root,'src/legacy-course'),dest=path.join(root,'firmware/components/learning_web');
const scripts=[...fs.readFileSync(path.join(web,'index.html'),'utf8').matchAll(/<script src="([^"]+)"/g)].map(x=>x[1]);
const inspect=`globalThis.nativeInspect=()=>({route,searchQuery,shift,symbols,key:[route,model.state.batch?.serial,model.state.batch?.phase,model.state.batch?.learnAt,model.state.batch?.at,!!model.state.batch?.checked,model.state.session?.lesson,model.state.session?.at,model.state.session?.page,readPage,!!model.state.session?.checked,pathModel.state.training?.serial,pathModel.state.training?.at,pathModel.state.training?.page,pathModel.state.training?.stage,!!pathModel.state.training?.checked,pathModel.state.attempt?.serial,pathModel.state.attempt?.at,pathModel.state.attempt?.page,pickedUnit,unitPage,listPage,entryId,!!modal].join('|'),corrupt:model.corrupt||pathModel.corrupt});globalThis.nativeDebug={model,pathModel,M,P,go,render,play,cancelAudio};globalThis.nativeExportNotice=message=>toast(message);`+fs.readFileSync(path.join(dest,'native-session-bridge.js'),'utf8');
const tables={};
function staticTasks(name,source){
 const model=require(path.join(web,name));
 const table=Object.fromEntries(model.data.lessons.map(l=>[l.id,model.tasks(l.id)]));
 const json=JSON.stringify(table);
 tables[name]={lessons:Object.keys(table).length,tasks:Object.values(table).reduce((n,a)=>n+a.length,0),sha256:crypto.createHash('sha256').update(json).digest('hex')};
 if((source.match(/function tasks\(id\)/g)||[]).length!==1||!source.includes('const api={'))throw Error('Task table hook location '+name);
 // Course data are pinned. Build exactly the same choices once on the host,
 // rather than rebuilding every distractor set during every home-page query.
 // Keep the original implementation as an independent QuickJS test oracle.
 const cached=`const __nativeTasks=Object.assign(Object.create(null),${json});
 function __nativeFreeze(x){if(x&&typeof x==='object'){Object.values(x).forEach(__nativeFreeze);Object.freeze(x);}return x;}
 __nativeFreeze(__nativeTasks);
 function tasks(id){return Object.prototype.hasOwnProperty.call(__nativeTasks,id)?__nativeTasks[id].slice():[];}
 `;
 return source.replace('function tasks(id)','function __nativeReferenceTasks(id)').replace('const api={',cached+'const api={nativeReferenceTasks:__nativeReferenceTasks,');
}
function rankedOrder(source){
 if(!source.includes('function order(ids,seed,avoid)')||!source.includes('const api={'))throw Error('Order hook location');
 const cached=`const __nativeHashes=new Map();function __nativeCachedHash(s){if(__nativeHashes.has(s))return __nativeHashes.get(s);let h=2166136261;for(let i=0;i<s.length;i++){const c=s.charCodeAt(i);h=Math.imul(h^c,16777619)>>>0;if(c>=0xd800&&c<=0xdbff&&i+1<s.length){const next=s.charCodeAt(i+1);if(next>=0xdc00&&next<=0xdfff)i++;}}if(__nativeHashes.size>=512)__nativeHashes.clear();__nativeHashes.set(s,h);return h;}
 function order(ids,seed,avoid){let a=[...ids].map(id=>({id,h:__nativeCachedHash(seed+':'+id)})).sort((x,y)=>x.h-y.h||x.id.localeCompare(y.id)).map(x=>x.id);if(a.length>1&&a.every((x,i)=>x===ids[i]))a.push(a.shift());if(a.length>1&&a[0]===avoid)a.push(a.shift());return a;}
 `;
 const B=require(path.join(web,'word-batch.js'));
 const gloss=Object.fromEntries(B.catalog.map(w=>[w.id,w.meaning.split(/[；;，,（(：:]/)[0].trim().slice(0,2)]));
 const options=`const __nativeGloss=${JSON.stringify(gloss)},__nativeOptionCache=new Map();
 function options(w,ids,seed){const key=JSON.stringify([w.id,ids,seed]);let v=__nativeOptionCache.get(key);if(!v){const pool=[...new Set([...ids.map(get),...catalog].filter(x=>x.meaning!==w.meaning&&__nativeGloss[x.id]!==__nativeGloss[w.id]).map(x=>x.meaning))],right=__nativeCachedHash(seed+':'+w.id)%3,picks=order(pool,seed).slice(0,2);picks.splice(right,0,w.meaning);v={options:picks,answer:right};if(__nativeOptionCache.size>=128)__nativeOptionCache.clear();__nativeOptionCache.set(key,v);}return {options:v.options.slice(),answer:v.answer};}
 `;
 return source.replace('function order(ids,seed,avoid)','function __nativeReferenceOrder(ids,seed,avoid)')
  .replace('function options(w,ids,seed)','function __nativeReferenceOptions(w,ids,seed)')
  .replace('picks=order(pool,seed)','picks=__nativeReferenceOrder(pool,seed)')
  .replace('const get=id=>catalog.find(w=>w.id===id),','const __nativeById=new Map(catalog.map(w=>[w.id,w]));const get=id=>__nativeById.get(id),')
  .replace('const api={',cached+options+'const api={nativeCachedHash:__nativeCachedHash,nativeOptions:options,nativeReferenceOptions:__nativeReferenceOptions,nativeOrder:order,nativeReferenceOrder:__nativeReferenceOrder,nativeReferenceHash:hash,');
}
function nativeKeyboard(source){
 const hook='function keyboard(){';
 if(source.split(hook).length!==2)throw Error('Native keyboard hook location');
 // C++ owns the unchanged keyboard geometry, labels, input queue and hit map.
 // The DOM projection consumes only presence and submit state; keep the full
 // web implementation behind a host-test switch as an independent oracle.
 source=source.replace(hook,hook+`if(globalThis.nativeCompactKeyboard!==false)return '<div class="kbd"><button data-action="search-submit">查询</button></div>';`);
 const search="searchQuery.trim()?M.wordBatch.catalog.filter(w=>w.word.toLowerCase().includes(searchQuery.trim().toLowerCase())).slice(0,3):[]";
 if(source.split(search).length!==2)throw Error('Lookup candidate hook location');
 // Pinned course catalogue only. Query normalization once, stop at the same
 // first three matches; no persisted state or result order changes.
 return source.replace('function search(){',`const nativeLookupIndex=M.wordBatch.catalog.map(w=>[w,w.word.toLowerCase()]);
 globalThis.nativeLookupMatches=query=>{const q=query.trim().toLowerCase(),out=[];if(q)for(const [word,key] of nativeLookupIndex)if(key.includes(q)){out.push(word);if(out.length===3)break;}return out;};
 function search(){`).replace(search,'nativeLookupMatches(searchQuery)');
}
function nativeCore(source){
 const hook='core(method,args,after=()=>{}){';
 if(source.split(hook).length!==2)throw Error('Unified core hook location');
 return source.replace(hook,`core(method,args,after=()=>{}){
  const previewCheck=globalThis.nativeProjectionModel===this&&method==='check'&&['read','listen'].includes(P.task(this.state.session)?.kind)&&!P.task(this.state.session)?.wordId;
  if(globalThis.nativeCoreReference||!['begin','check','advance'].includes(method)||(!previewCheck&&!P.validate(this.state)))return this.nativeReferenceCore(method,args,after);
  // Capture the original base mutation; retain its intermediate validation and
  // the outer full transaction. No throwaway storage/JSON/model round trip.
  const m=Object.create(P.Model.prototype);Object.assign(m,{state:this.state,clock:this.clock,error:'',corrupt:false});let mutate=null;
  m.commit=fn=>{mutate=fn;return true;};
  if(!m[method](...args)){this.error=m.error;return false;}
  // Only a private, disposable read/listen prediction: apply the original
  // mutation without another JSON transaction. The real tap never sets this
  // owner token; real check/advance still validate and persist normally.
  if(previewCheck){if(mutate)mutate(this.state);after(this.state);return true;}
  return this.commit(x=>{if(mutate)mutate(x);if(!P.validate(x))throw Error('学习记录未保存，请重试或导出备份。');after(x);});
 }
 nativeReferenceCore(method,args,after=()=>{}){`);
}
function nativeContent(source){
 // The physical UI owns scrolling and button dispatch. Browser scroll-region
 // bookkeeping and HTML submit types have no native consumer; keep actual
 // spelling/search input capture, listeners and lookup caret restoration.
 const previous=" const previousBody=app.querySelector('.screen-body[data-scroll-key]');if(previousBody)adaptiveScrollPositions.set(previousBody.dataset.scrollKey,previousBody.scrollTop);";
 if(!source.includes(previous))throw Error('Native scroll capture hook location');
 source=source.replace(previous,'');
 const scroll=source.indexOf(" if(route!=='search'){const b=app.querySelector('.screen-body');");
 const attach=source.indexOf(' pathUI.attach?.();',scroll);
 if(scroll<0||attach<scroll)throw Error('Native browser-only scroll hook location');
 source=source.slice(0,scroll)+source.slice(attach);
 const start=source.indexOf(' const views={home,books,book,units,unit,lesson,quiz,');
 const end=source.indexOf('app.innerHTML=view()+modalHtml();',start);
 if(start<0||end<0)throw Error('Pure page content hook location');
 const declarations=source.slice(start,end);
 const out=source.slice(0,start)+' app.innerHTML=nativePageContent();'+source.slice(end+'app.innerHTML=view()+modalHtml();'.length);
 return out.replace('function render(){','function nativePageContent(){'+declarations+'return view()+modalHtml();}\n'+fs.readFileSync(path.join(dest,'native-quiz.js'),'utf8')+'\nfunction render(){if(nativeRenderQuiz())return;');
}
const parts=[...['native-dom.js','native-home.js','native-sections.js','native-preload.js'].map(name=>[name,fs.readFileSync(path.join(dest,name),'utf8')]),...scripts.map(name=>{let s=fs.readFileSync(path.join(web,name),'utf8');if(name==='model.js'||name==='legacy-model.js')s=staticTasks(name,s);if(name==='word-batch.js')s=rankedOrder(s);if(name==='unified-model.js')s=nativeCore(s);if(name==='daily-ui.js'){const hook='function home(){const s=snap();return';if(!s.includes(hook))throw Error('Daily snapshot hook location');s=s.replace(hook,'function home(){const s=snap();globalThis.nativeDailySnapshot=s;return');}if(name==='app.js'){s=nativeContent(nativeKeyboard(s));const end=s.lastIndexOf('})();');if(end<0)throw Error('App hook location');s=s.slice(0,end)+inspect+fs.readFileSync(path.join(dest,'native-preload-hook.js'),'utf8')+s.slice(end);}return[name,s];})];
fs.mkdirSync(path.join(dest,'generated'),{recursive:true});
let out='#include <cstddef>\nnamespace wordcard {\nstruct WebScript {const char *name;const char *data;size_t length;};\n';
parts.forEach(([n,s],i)=>{const bytes=[...Buffer.from(s)];out+=`static const unsigned char script${i}[]={\n`+bytes.map(b=>b.toString()).join(',')+',0};\n';});
out+='extern const WebScript web_scripts[]={\n'+parts.map(([n,s],i)=>`{${JSON.stringify(n)},reinterpret_cast<const char*>(script${i}),sizeof(script${i})-1}`).join(',\n')+'};\nextern const size_t web_script_count=sizeof(web_scripts)/sizeof(web_scripts[0]);\n}\n';
fs.writeFileSync(path.join(dest,'generated/web_bundle.cpp'),out);
const hashes=Object.fromEntries(scripts.map(n=>[n,crypto.createHash('sha256').update(fs.readFileSync(path.join(web,n))).digest('hex')]));
fs.writeFileSync(path.join(dest,'generated/provenance.json'),JSON.stringify({source:'src/legacy-course',scripts:hashes,appHookOnly:false,transforms:['app native inspection hook','direct read/listen native semantic projection with pinned HTML oracle and error fallback','native browser-only scroll housekeeping removed; revision-scoped text extraction cache','single draft base transition with original intermediate and final validation; original retained as oracle','question-owned staged disposable prediction; exact HTML and native page checks; independent raster and hit-map cache','pinned lowercase lookup index; identical first-three substring candidates','native keyboard presence/submit projection; unchanged C++ keys and full web keyboard oracle','reuse rendered daily snapshot for native home information architecture; no extra scheduler call','immutable exact build-time course tasks; original retained for equivalence checks','word-batch ranked sort, indexed get, pinned gloss, bounded512 hash and bounded128 exact options caches; original algorithms retained as oracles'],taskTables:tables,engine:'QuickJS-NG v0.17.0 / 6d46d07d04041b40f4f49eaa7fdebe44c314c699',network:false,localEnginePatch:'context-owned cooperative interrupt interval: foreground 10000, speculative 256; exact vendor also used by host'},null,2));
console.log('Bundled '+scripts.length+' pinned learning scripts; DOM/native adapter; '+Buffer.byteLength(out)+' bytes');
