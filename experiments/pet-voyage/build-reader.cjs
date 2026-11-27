'use strict';
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'../..'),out=path.join(root,'outputs'),reader=path.join(out,'pet-after-twenty-years');
const esbuild=require('esbuild');
const key='after-twenty-years@1.0.0';
const library=require('./library.cjs').openLibrary(process.env.WORDCARD_LIBRARY||path.join(root,'work/wordcard-content/library'));
const book=library.compose(key),deck=library.get(book.deckKey);
for(const t of Object.values(book.terms)){
 if(!t.id)continue;
 const id=deck.data.words[t.id].audio;
 const wav=fs.readFileSync(path.join(deck.dir,id));
 if(wav.toString('ascii',0,4)!=='RIFF'||wav.toString('ascii',8,12)!=='WAVE')throw Error('无效发音 '+id);
 t.audio='data:audio/wav;base64,'+wav.toString('base64');
}
const bundle=name=>esbuild.buildSync({entryPoints:[path.join(__dirname,name+'.cjs')],bundle:true,platform:'browser',target:'es2022',minify:true,write:false}).outputFiles[0].text;
const js=bundle('reader'),html=fs.readFileSync(path.join(__dirname,'reader.html'),'utf8'),css=fs.readFileSync(path.join(__dirname,'reader.css'),'utf8');
const site=path.join(out,'pet-voyage');
for(const [name,body] of Object.entries({'reader.html':html,'reader.js':js,'reader.css':css,'voyage.js':bundle('player')}))fs.writeFileSync(path.join(site,name),body);
fs.mkdirSync(reader,{recursive:true});
const licenses=['LICENSE-ts-fsrs.txt','LICENSE-srs-kit.txt'].map(n=>fs.readFileSync(path.join(out,'bookshop',n),'utf8')).join('\n');
const embedded=html.replace('<link rel="stylesheet" href="reader.css">',()=>'<style>'+css+'</style>')
 .replace('<script defer src="reader.js"></script>',()=>'<script type="application/json" id="book-data">'+JSON.stringify(book).replace(/</g,'\\u003c')+'</script><script type="text/plain" id="licenses">'+licenses+'</script><script>'+js.replace(/<\/script/gi,'<\\/script')+'</script>');
fs.writeFileSync(path.join(reader,'index.html'),embedded);console.log('Offline pages, glossary, audio and scheduler written.');
