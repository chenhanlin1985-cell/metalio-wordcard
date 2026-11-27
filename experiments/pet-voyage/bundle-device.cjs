'use strict';
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'../..');
const esbuild=require('esbuild');
const code=esbuild.buildSync({entryPoints:[path.join(__dirname,'device.cjs')],define:{NATIVE_STORY:'true'},bundle:true,format:'iife',platform:'neutral',mainFields:['main'],target:'es2022',charset:'utf8',minify:true,write:false}).outputFiles[0].text;
if(code.includes(')wordbooks"'))throw Error('Raw string delimiter collision');
const deflated=require('node:zlib').deflateRawSync(Buffer.from(code),{level:9});
const packed=Array.from(deflated,byte=>String(byte)).join(',').match(/.{1,180}(?:,|$)/g);
fs.writeFileSync(path.join(root,'firmware/components/learning_web/generated/content_player_deflate.inc'),
  'static constexpr size_t book_length='+Buffer.byteLength(code)+';\nstatic const unsigned char book_deflate[]={\n'+packed.join('\n')+'\n};\n');
const chunks=Array.from(code).join('').match(/[\s\S]{1,4000}/gu);
fs.writeFileSync(path.join(root,'firmware/components/learning_web/generated/content_player.inc'),chunks.map(s=>'R"wordbooks('+s+')wordbooks"').join('\n')+'\n');
