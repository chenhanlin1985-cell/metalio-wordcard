'use strict';

// This pilot binds to the published order and meanings of this edition.
const bookKey = 'pocket-receipt@1.0.0';
const contrasts = {
  receipt: '付款后拿到收据，购物清单是买之前列的。',
  sleeve: '袖子套住手臂，口袋用来装东西。',
  parcel: '爸爸抱着的是包好的东西，也就是包裹。',
  wool: '这里说的是做大衣的材料：羊毛。',
  change: '这里指付钱后找回的零钱。',
  label: '缝在衣服里的标签，上面写着尺码。',
  cost: '这里说一件东西要花多少钱。',
  purse: '她从小钱包里拿出了卡。',
};
const firstKind = (term, modeKind) => Object.hasOwn(contrasts, term) ? 'selfMeaning' : modeKind;
const fresh = () => ({first:{}, retries:{}, pending:[], retry:null, revealed:false, hinted:false});
function current(book,state) {
  return state.practice?.retry || book.chapters[state.chapter].focus[state.wordAt];
}
function record(state,term,kind,correct,now,wrongWord) {
  const p=state.practice, result={kind,correct,assisted:p.hinted,at:now};
  if(wrongWord)result.selected=wrongWord;
  if(p.retry)p.retries[term]=result;
  else {
    p.first[term]=result;
    if(!correct||p.hinted)p.pending.push({term,at:now,others:[]});
  }
  state.choice=correct;
}
function touchPage(state,page,now) {
  // Re-reading a queued word restarts its gap, without counting page flips as answers.
  const terms=new Set(Array.from(page.en.matchAll(/\[\[([^\]]+)\]\]/g),m=>m[1].toLowerCase()));
  for(const row of state.practice.pending)if(terms.has(row.term)){row.at=now;row.others=[];}
}
function afterFeedback(state,term,now) {
  const p=state.practice;
  for(const row of p.pending){
    if(row.term===term){row.at=now;row.others=[];}
    else if(row.others.length<3&&!row.others.includes(term))row.others.push(term);
  }
  p.revealed=false;p.hinted=false;p.retry=null;
}
function takeRetry(book,state,now,fromBookEnd=false) {
  const p=state.practice;
  if(!p)return false;
  const i=p.pending.findIndex(row=>{
    if(!fromBookEnd)return row.others.length>=3;
    const id=book.terms[row.term].id, others=new Set(row.others);
    let last=row.at;
    for(const [seen,at] of state.recent||[])if(at>row.at){
      if(seen===id){others.clear();last=at;}else others.add(seen);
    }
    return others.size>=3 || now-last>=15*60*1000;
  });
  if(i<0)return false;
  p.retry=p.pending.splice(i,1)[0].term;state.phase='recall';return true;
}
module.exports={bookKey,contrasts,firstKind,fresh,current,record,touchPage,afterFeedback,takeRetry};
