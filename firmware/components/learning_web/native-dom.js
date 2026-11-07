/* Minimal synchronous DOM adapter for the pinned learning UI. No web browser,
   network, HTML execution, or external scripts; only the embedded release runs. */
globalThis.window=globalThis;globalThis.innerHeight=1300;
globalThis.nativeRevision=1;
let nativeDirect=null;const nativeDirectStats={pages:0};
const nativeSelectors=new Map();
function nativeSelector(s){let c=nativeSelectors.get(s);if(c)return c;const parts=s.trim().match(/(?:\[[^\]]*\]|[^\s])+/g)||[];c=parts.map(part=>{const attrs=[...part.matchAll(/\[([^=\]]+)(?:=["']?([^\]"']*)["']?)?\]/g)].map(m=>[m[1],m[2]]);const plain=part.replace(/\[[^\]]+\]/g,''),id=plain.match(/#([\w-]+)/),tag=plain.match(/^[\w-]+/);return {attrs,id:id?.[1],tag:tag?.[0],classes:[...plain.matchAll(/\.([\w-]+)/g)].map(m=>m[1])};});if(nativeSelectors.size>256)nativeSelectors.clear();nativeSelectors.set(s,c);return c;}
function nativeSimple(n,p){if(n.tag==='#text')return false;if(p.tag&&n.tag!==p.tag||p.id&&n.attrs.id!==p.id)return false;for(const k of p.classes)if(!n._classes.has(k))return false;for(const[k,v]of p.attrs)if(!(k in n.attrs)||v!==undefined&&n.attrs[k]!==v)return false;return true;}
function nativeMatches(n,parts){if(!nativeSimple(n,parts.at(-1)))return false;let p=n.parent;for(let i=parts.length-2;i>=0;i--){while(p&&!nativeSimple(p,parts[i]))p=p.parent;if(!p)return false;p=p.parent;}return true;}
globalThis.setTimeout=()=>1;globalThis.clearTimeout=()=>{};
globalThis.requestAnimationFrame=f=>f();globalThis.addEventListener=()=>{};
if(typeof Intl==='undefined')globalThis.Intl={DateTimeFormat:class{constructor(l,o={}){this.o=o;}format(t){const d=new Date(+new Date(t)+28800000);return (d.getUTCMonth()+1)+'/'+d.getUTCDate()+(this.o.hour?' '+String(d.getUTCHours()).padStart(2,'0')+':'+String(d.getUTCMinutes()).padStart(2,'0'):'');}}};
const decodeHTML=s=>String(s).replace(/&(#x[0-9a-f]+|#\d+|amp|lt|gt|quot|apos|nbsp|#39);/gi,(_,v)=>v[0]==='#'?String.fromCodePoint(v[1].toLowerCase()==='x'?parseInt(v.slice(2),16):+v.slice(1)):({amp:'&',lt:'<',gt:'>',quot:'"',apos:"'",nbsp:' '})[v]);
class NativeNode{
 constructor(tag='',attrs={},parent=null,text=''){this.tag=tag;this.parent=parent;this.children=[];this._text=text;if(tag==='#text')return;this.attrs=attrs;this.listeners={};this.dataset={};for(const[k,v]of Object.entries(attrs))if(k.startsWith('data-'))this.dataset[k.slice(5).replace(/-([a-z])/g,(_,c)=>c.toUpperCase())]=v;this.style={setProperty(){}};this.disabled='disabled'in attrs;this.checked='checked'in attrs;this.value=attrs.value||'';this.selectionStart=this.selectionEnd=this.value.length;this.scrollTop=0;this.clientHeight=this.scrollHeight=900;this.clientWidth=740;this._classes=new Set((attrs.class||'').split(/\s+/));this.classList={contains:c=>this._classes.has(c),add:c=>{if(!this._classes.has(c)){this._classes.add(c);this.attrs.class=(this.attrs.class||'')+' '+c;}}};}
 get textContent(){if(this.tag==='#text')return this._text;if(this._textRevision===nativeRevision)return this._textCache;this._textCache=this.children.map(c=>c.textContent).join(['button','div','li','label'].includes(this.tag)?' ':'');this._textRevision=nativeRevision;return this._textCache;}
 invalidateText(){for(let n=this;n;n=n.parent)n._textRevision=-1;}
 set textContent(s){this.children=[new NativeNode('#text',{},this,String(s))];this.invalidateText();}
 get innerHTML(){return this._html||'';}set innerHTML(s){this._html=s;this.children=[];this.children=(this===nativeApp?nativePreparedChildren(s):null)||parseHTML(s,this);nativeRevision++;}
 getAttribute(k){return this.attrs[k]??null;}hasAttribute(k){return k in this.attrs;}setAttribute(k,v){this.attrs[k]=String(v);}
 addEventListener(k,f){(this.listeners[k]??=[]).push(f);}focus(){}setSelectionRange(a,b){this.selectionStart=a;this.selectionEnd=b;}
 getBoundingClientRect(){return {height:45,width:684};}insertAdjacentHTML(where,s){const nodes=parseHTML(s,this);if(where==='afterbegin')this.children.unshift(...nodes);else this.children.push(...nodes);this.invalidateText();}
 matches(sel){return nativeMatches(this,nativeSelector(sel));}
 closest(s){for(let n=this;n;n=n.parent)if(n.matches(s))return n;return null;}
 querySelectorAll(s){const parts=nativeSelector(s),out=[];function walk(n){for(const c of n.children){if(nativeMatches(c,parts))out.push(c);walk(c);}}walk(this);return out;}
 querySelector(s){const parts=nativeSelector(s);function walk(n){for(const c of n.children){if(nativeMatches(c,parts))return c;const r=walk(c);if(r)return r;}return null;}return walk(this);}click(){if(this.tag==='a'&&String(this.href||'').startsWith('sd-export:')){const raw=nativeBlobs.get(this.href)||'';const ok=nativeExport(this.download,raw);nativeBlobs.delete(this.href);if(globalThis.nativeExportNotice)nativeExportNotice(ok?'已导出到 SD learning-web 目录。':'SD 导出失败，原记录保留，请重试。');return;}nativeClickNode(this);}
}
// Cache only immutable parsed opening tags, never live nodes/listeners or learner
// state. Exact source is the generation key; each use owns mutable attributes.
// Bounds cover both item count and source length, including imported long text.
const nativeTagCache=new Map();let nativeTagBytes=0;
function nativeTag(t){let hit=nativeTagCache.get(t);if(hit)return hit;const m=t.match(/^<([\w-]+)/);if(!m)return null;const a={};for(const x of t.slice(m[0].length).matchAll(/([\w:-]+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))?/g))a[x[1]]=decodeHTML(x[2]??x[3]??x[4]??'');hit={tag:m[1],attrs:a,leaf:['input','br','hr','img','meta','link'].includes(m[1])||t.endsWith('/>')};if(t.length<=2048){if(nativeTagCache.size>=128||nativeTagBytes+t.length>16384){nativeTagCache.clear();nativeTagBytes=0;}nativeTagCache.set(t,hit);nativeTagBytes+=t.length;}return hit;}
function parseHTML(s,parent){const root=new NativeNode('root',{},parent),stack=[root],tokens=s.match(/<!--[\s\S]*?-->|<[^>]*>|[^<]+/g)||[];for(const t of tokens){if(t.startsWith('<!--'))continue;if(t.startsWith('</')){const tag=t.slice(2,-1).trim();while(stack.length>1){const n=stack.pop();if(n.tag===tag)break;}continue;}if(t[0]==='<'){const spec=nativeTag(t);if(!spec)continue;const n=new NativeNode(spec.tag,{...spec.attrs},stack.at(-1));stack.at(-1).children.push(n);if(!spec.leaf)stack.push(n);}else stack.at(-1).children.push(new NativeNode('#text',{},stack.at(-1),decodeHTML(t)));}return root.children;}
const nativeApp=new NativeNode('div',{id:'app'}),nativeDocNodes={};
globalThis.document={querySelector:s=>s==='#app'?nativeApp:nativeApp.querySelector(s)||(nativeDocNodes[s]??=new NativeNode('div')),getElementById:id=>nativeApp.querySelector('#'+id)||null,addEventListener(){},documentElement:{style:{setProperty(){}}},createElement:()=>new NativeNode('a')};
globalThis.localStorage={getItem:k=>nativeRead(k),setItem:(k,v)=>{if(!nativeWrite(k,String(v)))throw Error('SD 保存失败，进度未前进，请重试。');},removeItem:()=>{throw Error('不支持删除学习记录');}};
const nativeBlobs=new Map();let nativeBlobSerial=0;
globalThis.Blob=class{constructor(parts){this.text=parts.join('');}};
globalThis.URL={createObjectURL:b=>{nativeBlobs.clear();const id='sd-export:'+(++nativeBlobSerial);nativeBlobs.set(id,b.text);return id;},revokeObjectURL:id=>nativeBlobs.delete(id)};
let nativeSound=null,nativeSoundSerial=0;globalThis.nativePending=null;
globalThis.Audio=class{constructor(path){this.src=path;this.playbackRate=1;}play(){this.serial=++nativeSoundSerial;nativeSound=this;nativePending={path:this.src,rate:this.playbackRate,serial:this.serial};return {catch(){}};}pause(){if(nativeSound===this){nativeSound=null;nativePending={stop:true,serial:++nativeSoundSerial};}}};
function nativeAudioEnded(serial,ok){if(nativeSound&&nativeSound.serial===serial){nativeRevision++;const a=nativeSound;nativeSound=null;const f=ok?a.onended:a.onerror;if(f)f();}}
function nativeClickNode(n){if(n.disabled)return;nativeRevision++;const e={target:n,preventDefault(){}};for(const f of nativeApp.listeners.click||[])f(e);}
function nativeClearToast(){const n=document.querySelector('#toast');n.textContent='';n.style.display='none';}
let nativeHomeReturn='',nativeOtherBooks=false,nativeHomeOptions=false;const nativeOrigins=new Map();
function nativeAction(data){nativeProjectionReady=null;const source=nativePrepSource();nativePreparedArmed=!!source&&!!nativePrepared&&nativePrepared.source===source&&nativePrepared.action===data.action;try{nativeActionImpl(data);}finally{nativePreparedArmed=false;const carry=['choose','check','read-page','audio'].includes(data.action)&&source&&source===nativePrepSource();if(!carry){nativePrepared=null;nativePreparationJob=null;}}}
function nativeActionImpl(data){nativeClearToast();if(data.action==='native-home-options'){nativeHomeOptions=true;nativeDebug.render();return;}if(data.action==='w-day-home')nativeHomeOptions=false;if(data.action==='native-other-books'||data.action==='native-books'){nativeOtherBooks=data.action==='native-other-books';nativeDebug.render();return;}if(data.homeReturn)nativeHomeReturn=data.homeReturn;else if(['go','u-notebook','word-list','p-resume','p-edit','w-day-home'].includes(data.action))nativeHomeReturn='';if(data.action==='go'&&['home','books','review','dictionary'].includes(data.route)){nativeOtherBooks=false;nativeHomeOptions=false;nativeOrigins.clear();}if(data.action==='native-units'){nativeDebug.go('units');return;}const before=nativeInspect().route,n=new NativeNode('button',Object.fromEntries(Object.entries(data).map(([k,v])=>['data-'+k,String(v)])));nativeClickNode(n);const after=nativeInspect().route;if(['word-batch','quiz','path-practice','path-test'].includes(after))nativeHomeOptions=false;if(data.nativeFrom&&after!==before&&!['word-batch','quiz','path-practice','path-test'].includes(after)&&!nativeDebug.model.state.lookup)nativeOrigins.set(after,data.nativeFrom);}
function nativeBack(){const lookup=nativeApp.querySelector('[data-action=lookup-return]'),back=nativeApp.querySelector('.screen-head [data-action=go]'),route=nativeInspect().route;if(lookup)nativeClickNode(lookup);else if(route==='home'&&nativeHomeOptions){nativeHomeOptions=false;nativeDebug.render();}else if(route==='books'&&nativeOtherBooks){nativeOtherBooks=false;nativeDebug.render();}else if(route===nativeHomeReturn){nativeHomeReturn='';nativeDebug.go('home');}else if(nativeOrigins.has(route)){const target=nativeOrigins.get(route);nativeOrigins.delete(route);nativeDebug.go(target);}else if(route==='word-batch')nativeAction({action:'w-pause'});else if(route==='units')nativeAction({action:'go',route:'books'});else if(back)nativeClickNode(back);else nativeAction({action:'w-day-home'});}
function nativeType(value){nativeRevision++;nativeClearToast();const n=document.getElementById('spelling');if(n){n.value=value;n.selectionStart=n.selectionEnd=value.length;for(const f of n.listeners.input||[])f({target:n});}else for(const ch of value)nativeAction({action:'key',value:ch});}
function nativeKey(kind,value=''){const s=globalThis.nativeInspect(),prefix=s.route==='word-batch'?'w-kbd-':'';nativeAction({action:prefix+(kind==='submit'?'search-submit':kind==='backspace'?'erase':kind),value});}
function nativeEdit(keys){
 const input=document.getElementById('spelling'),before=nativeInspect(),baseRevision=nativeRevision;
 if(!input||before.route!=='word-batch'){for(const key of keys)nativeKey(key==='\b'?'backspace':'key',key==='\b'?'':key);return {stable:false};}
 let value=input.value,start=input.selectionStart??value.length,end=input.selectionEnd??start;
 for(const key of keys){if(key==='\b'){const at=start===end?Math.max(0,start-1):start;value=value.slice(0,at)+value.slice(end);start=end=at;}else{value=value.slice(0,start)+key+value.slice(end);start+=key.length;end=start;}}
 nativeType(value);const current=document.getElementById('spelling');
 if(current&&current.value===value)current.setSelectionRange(start,end);
 const submit=nativeApp.querySelector('[data-action="w-kbd-search-submit"]');
 return {stable:current===input&&before.key===nativeInspect().key,baseRevision,typed:current?.value||'',canSubmit:!!submit&&!submit.disabled,toast:document.querySelector('#toast').textContent,revision:nativeRevision};
}
function nativeAudioLabel(n,full){if(n.dataset.rate)return '慢速 / 停止';if(n.dataset.action==='w-audio')return (full.includes('例句')?'例句':'单词')+'录音 / 停止';return '录音 / 停止';}
function nativeWords(n){return n.textContent.replace(/＿/g,'_').replace(/\s+/g,' ').trim();}
function nativeView(){
 if(nativeDirect&&nativeDirect.revision===nativeRevision)return {...nativeDirect.view,toast:document.querySelector('#toast').textContent};
 const prepared=nativePreparedView();if(prepared)return prepared;
 const modal=nativeApp.querySelector('.modal-cover')||nativeApp.querySelector('.modal'),scope=modal||nativeApp;
 const header=scope.querySelector('.screen-head'),body=modal||scope.querySelector('.screen-body'),footer=scope.querySelector('.screen-foot'),nav=modal?null:scope.querySelector('.tabs');
 const state=nativeInspect(),actions=[],blocks=[];
 function role(n){if(n.classList.contains('word'))return 'word';if(n.classList.contains('ipa'))return 'ipa';if(n.classList.contains('meaning')||n.classList.contains('large'))return 'meaning';if(n.classList.contains('question'))return 'question';if(n.classList.contains('passage')||n.classList.contains('path-probe-text'))return 'passage';if(n.classList.contains('example')||n.classList.contains('sentence'))return 'example';if(n.classList.contains('fine')||n.tag==='small')return 'detail';if(n.classList.contains('notice')&&nativeWords(n)==='完整听完一次后再作答；可以重复听。')return 'detail';if(n.classList.contains('notice')&&state.route==='word-batch')return 'detail';return '';}
 function action(n){const d={...n.dataset};if(!d.action)return -1;actions.push(d);return actions.length-1;}
 function emit(n,area='body'){
  if(n.tag==='#text'){if(nativeWords(n))blocks.push({kind:'text',text:nativeWords(n),area});return;}
  if(['svg','path','style','script'].includes(n.tag)||n.classList.contains('kbd'))return;
  if(n.tag==='button'){if(n.classList.contains('token'))return;const title=n.querySelector('strong'),full=nativeWords(n);blocks.push({kind:'button',text:n.dataset.action?.includes('audio')?nativeAudioLabel(n,full):n.dataset.action==='read-page'?(+n.dataset.delta<0?'上段原文':'下段原文'):n.dataset.action==='skip'?'稍后听':full==='不知道，看看解释'?'不会，看解释':title?nativeWords(title):full||n.attrs['aria-label']||'播放',detail:title?full.replace(nativeWords(title),'').trim():'',role:n.classList.contains('row')?'row':n.parent?.classList.contains('options')?'option':n.dataset.action?.includes('audio')||n.dataset.action==='skip'?'audio':n.dataset.action==='read-page'?'read-nav':'',action:action(n),enabled:!n.disabled,selected:n.classList.contains('chosen')||n.attrs['aria-checked']==='true'||n.classList.contains('active'),primary:n.classList.contains('primary'),area});return;}
  if(n.tag==='a'){if(n.attrs.href==='curriculum.html'){blocks.push({kind:'button',text:nativeWords(n),action:action(new NativeNode('button',{'data-action':'native-units'})),enabled:true,area});}else blocks.push({kind:'text',text:nativeWords(n)+' · 资料地址：'+(n.attrs.href||''),area:'body'});return;}
  if(n.tag==='input'){if(n.attrs.type==='radio'){const label=n.parent;blocks.push({kind:'button',text:nativeWords(label.querySelector('strong')||label),detail:nativeWords(label.querySelector('small')||new NativeNode()),role:'mode',action:action(new NativeNode('button',{'data-action':'w-mode','data-value':n.value})),selected:n.checked,enabled:true,area});}return;}
  if(n.classList.contains('page-controls')){const count=n.querySelector('.count');if(count)blocks.push({kind:'text',role:'article-position',text:'原文 '+nativeWords(count)+' 段',area});for(const b of n.querySelectorAll('button'))emit(b,area);return;}
  if(n.tag==='label'&&n.querySelector('input[type=radio]')){emit(n.querySelector('input'),area);return;}
  if(n.tag==='li'&&n.parent?.classList.contains('daily-sequence')){const label=n.querySelector('strong'),amount=n.querySelector('span');blocks.push({kind:'text',role:'progress',text:nativeWords(label),detail:nativeWords(amount),area});return;}
  if(['h1','h2','h3','p','small','dt','dd'].includes(n.tag)||role(n)||n.tag==='li'){
    if(nativeWords(n))blocks.push({kind:['h1','h2','h3'].includes(n.tag)?'heading':'text',role:role(n),text:nativeWords(n),area});return;}
  for(const c of n.children)emit(c,area);
 }
 if(body)emit(body);else if(modal)emit(modal);
 if(footer)for(const n of footer.children)emit(n,'foot');
 const top=[];if(header)for(const n of header.querySelectorAll('button'))top.push({text:nativeWords(n),action:action(n),enabled:!n.disabled});
 const tabs=[];if(nav)for(const n of nav.querySelectorAll('button'))tabs.push({text:nativeWords(n),action:action(n),selected:n.classList.contains('active')});
 const input=document.getElementById('spelling'),keyboard=!!scope.querySelector('.kbd');
 let prompt='';if(input){const p=scope.querySelector('.spell-prompt');prompt=p?nativeWords(p):'';}
 const toast=document.querySelector('#toast').textContent;
 const pattern=scope.querySelector('.letter-pattern');
 const view={title:header?.querySelector('h1')?.textContent|| (modal?'请确认':'学习'),blocks,top,tabs,actions,keyboard,typed:input?input.value:(state.searchQuery||''),shift:state.shift,symbols:state.symbols,route:state.route,key:state.key,prompt,toast,gapMask:pattern?nativeWords(pattern):'',corrupt:state.corrupt};
 if(keyboard){const keys=scope.querySelectorAll('.kbd button');view.submit=keys.find(n=>(n.dataset.action||'').endsWith('search-submit'))?.dataset||{action:'search-submit'};view.canSubmit=!keys.find(n=>(n.dataset.action||'').endsWith('search-submit'))?.disabled;}
 for(const tab of view.tabs){if(tab.text==='学习')tab.text='首页';if(tab.text==='教材')tab.text='课程';}
 if(modal)return view;
 view.key+='|'+Number(nativeOtherBooks)+'|'+Number(nativeHomeOptions);
 return state.route==='home'?nativeHome(view):nativeSections(view);
}
