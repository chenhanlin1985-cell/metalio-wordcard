// One disposable next-page projection. It owns no learner state or EPD history.
let nativePrepared=null,nativePreparedArmed=false,nativeProjectionReady=null;
let nativePreparationJob=null,nativePreparationRestore=null;
const nativePrepStats={built:0,domHits:0,viewHits:0,misses:0,cancelled:0};
function nativePrepIdentity(){const s=nativeInspect();return JSON.stringify([s.route,s.key,s.searchQuery,s.shift,s.symbols,s.corrupt,nativeOtherBooks,nativeHomeOptions,document.querySelector('#toast').textContent]);}
function nativePrepSource(){const n=nativeDebug,m=n.model.state,s=m.session,b=m.batch,r=nativeInspect().route;if(m.lookup)return '';return r==='quiz'&&s?JSON.stringify([r,s.lesson,s.at,s.purpose,s.queue]):r==='word-batch'&&b?.checked?JSON.stringify([r,b.serial,b.phase,b.at,b.learnAt]):'';}
function nativeAbortPreparation(keep=false){if(nativePreparationRestore){const restore=nativePreparationRestore;restore();nativePreparationRestore=null;}if(!keep)nativePreparationJob=null;nativePrepared=null;nativePreparedArmed=false;nativeProjectionReady=null;nativePrepStats.cancelled++;}
function nativePreparedChildren(html){
 if(!nativePreparedArmed||!nativePrepared)return null;
 const p=nativePrepared;nativePrepared=null;
 if(p.html!==html||p.identity!==nativePrepIdentity()){nativePrepStats.misses++;return null;}
 nativeProjectionReady={view:p.view,identity:p.identity,revision:nativeRevision+1};nativePrepStats.domHits++;return p.children;
}
function nativePreparedView(){const p=nativeProjectionReady;nativeProjectionReady=null;if(!p)return null;if(p.revision===nativeRevision&&p.identity===nativePrepIdentity()){nativePrepStats.viewHits++;return p.view;}nativePrepStats.misses++;return null;}
