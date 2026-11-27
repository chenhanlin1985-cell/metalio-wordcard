'use strict';
// Only navigation and story-specific feedback go into the cursor.
// The native service owns the cards and returns a fresh projection after each save.
const observations=new WeakMap(),exposures=new WeakMap();
function expose(state,local){exposures.set(state,local);}
function observe(state,local,skill,correct,assisted,retry){
 if(typeof NATIVE_STORY!=='undefined'&&NATIVE_STORY)
  observations.set(state,{local,skill,correct,assisted,retry});
}
function request(pack,data){
 return JSON.parse(nativeStory(JSON.stringify({book:pack.key,digest:pack.digest,deck:pack.deckKey,deckDigest:pack.deckSha256,...data})));
}
function read(store,pack,fresh){
 const {run,rev,cards,practised,recent}=request(pack,{op:'read'});
 return {record:{rev},state:{...(run||fresh()),cards,practised,recent}};
}
function write(store,pack,fresh,state,rev){
 const {cards,practised,recent,...run}=state;
 const saved=request(pack,{op:'save',run,rev,answer:observations.get(state),exposed:exposures.get(state)});
 observations.delete(state);exposures.delete(state);
 Object.assign(state,{cards:saved.cards,practised:saved.practised,recent:saved.recent});
 return saved.rev;
}
module.exports={observe,expose,read,write};
