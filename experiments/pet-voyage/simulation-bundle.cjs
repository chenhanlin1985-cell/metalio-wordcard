'use strict';
const path=require('node:path');
const esbuild=require('esbuild');
module.exports=esbuild.buildSync({entryPoints:[path.join(__dirname,'device.cjs')],bundle:true,platform:'neutral',
  format:'iife',mainFields:['main'],target:'es2022',charset:'utf8',define:{NATIVE_STORY:'false'},write:false}).outputFiles[0].text;
