'use strict';
const {FSRS6Model,FSRS6_DEFAULT_WEIGHTS}=require('ts-fsrs/models/fsrs-6');
const model=FSRS6Model.create({config:{weights:FSRS6_DEFAULT_WEIGHTS,enableShortTerm:true,numRelearningSteps:1}});
const DAY=86400000,GAP=900000;
module.exports=function(s,t,ok,now){
  const prev=s.cards[t.id]?.[t.k];
  // Story callbacks can occur before a card is due. An early success never inflates its FSRS interval.
  if(!t.retry&&(!ok||!prev||now>=prev.due)){
   const elapsed=prev?Math.max(0,Math.floor((now-prev.last)/DAY)):0;
   const memory=model.step({memoryState:prev?.memory||null,rating:ok?3:1,elapsedDays:elapsed});
   const target=now+DAY*model.nextInterval(memory,.9);
   (s.cards[t.id]??={})[t.k]={memory,last:now,due:ok?target:now+GAP,target,attempts:(prev?.attempts||0)+1};
  }else if(t.retry&&prev&&ok)prev.due=Math.max(now+GAP,prev.target);
};
