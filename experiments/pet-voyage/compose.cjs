'use strict';
function composeContent(r,d,key,deckKey,audioRoot,consume=false){
  const plain=r.m.type==='deck';
  // The device owns this freshly parsed table. Release each original row as it is projected.
  const w=consume?d.data.words:{};
  for(const id in d.data.words){const term=d.data.words[id];
   const item={id,w:term.word,zh:term.meaning,ipa:term.ipa,topic:term.topic};if(term.audio)item.audio=audioRoot+term.audio;
   w[id]=item;
  }
  const st={firstAction:'开始故事',acts:[{start:0,title:r.m.title}],gates:[],forks:[],authored:{},endings:{}};
  if(plain)st.chapters=Object.entries(d.data.topics).map(([topic,title])=>({topic,title,intro:'按本主题选取一组词，每组最多五条。',core:[],returns:{}}));
  else{
   Object.assign(st,r.data);if(r.data.ending)st.endings={default:r.data.ending};
   st.chapters=r.data.chapters.map(({words,...c})=>{
    const prompts={};
    for(const term of words){
     prompts[term.id]=[term.en,term.zh];st.authored[term.id]=prompts[term.id];
     if(term.meaning)w[term.id].sceneZh=term.meaning;
    }
    return {returns:{},...c,core:words.map(t=>t.id),prompts};
   });
  }
  return {...st,key,deckKey,plain,title:r.m.title,deckTitle:d.m.title,deckSha256:d.m.files['content.json'],words:w};
}
module.exports={composeContent};
