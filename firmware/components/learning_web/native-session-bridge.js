// Cold-screen boundary only. NativeStudy owns actions, state transactions and
// semantic pages while a C++ study route is active. No per-tap callback to JS.
globalThis.nativeTakeStudy=()=>({course:model.state,profile:pathModel.state,now:Date.now(),
 ui:{route,readPage,lastLesson,legacySummary,searchQuery,entryId,returnTo,shift,symbols,
 pickedUnit,unitPage,listMode,listPage,listLessonId,nativeOtherBooks,nativeHomeOptions},
 soundSerial:nativeSoundSerial,playing,modal:!!modal,corrupt:model.corrupt||pathModel.corrupt,
 error:model.error||pathModel.error,audioNote,toast:document.querySelector('#toast').textContent});
globalThis.nativeRestoreStudy=(c,deferPaint=false)=>{
 cancelAudio();model.state=c.course;pathModel.state=c.profile;
 ({route,readPage,lastLesson,legacySummary,searchQuery,entryId,returnTo,shift,symbols,
 pickedUnit,unitPage,listMode,listPage,listLessonId,nativeOtherBooks,nativeHomeOptions}=c.ui);
 nativeSoundSerial=c.soundSerial;nativeSound=null;nativePending=c.pendingAudio||null;
 model.error=c.error||'';audioNote=c.audioNote||'';
 nativeClearToast();if(c.toast)toast(c.toast);
 nativeDirect=null;nativePrepared=null;nativePreparationJob=null;nativeProjectionReady=null;
 // Only the two read-only root navigations may skip this superseded page.
 // The caller immediately renders the target in the same synchronous command.
 nativeRevision++;if(!deferPaint)render();return true;
};
