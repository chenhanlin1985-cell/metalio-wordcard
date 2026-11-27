'use strict';
// This receipt travels in the same atomic progress write as the completed activity.
function award(pack,state,kind,ids,words,now){
 const key=kind==='story'?'story':'group/'+ids.join(',');
 const stamps=state.stamps??=[];
 if(stamps.some(s=>s.key===key))return;
 stamps.push({key,title:pack.title,kind,at:now,words,sourceIds:ids});
}
module.exports={award};
