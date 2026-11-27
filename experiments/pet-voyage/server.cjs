'use strict';
const http=require('node:http'),fs=require('node:fs'),path=require('node:path');
const {pipeline}=require('node:stream/promises'),{Transform}=require('node:stream');
const {execFile}=require('node:child_process'),{promisify}=require('node:util');
const project=path.resolve(__dirname,'../..'),root=path.join(project,'outputs/pet-voyage');
const directory=process.env.WORDCARD_LIBRARY||path.join(project,'work/wordcard-content/library');
const port=Number(process.env.PET_VOYAGE_PORT||8798);
fs.mkdirSync(directory,{recursive:true});
const library=require('./library.cjs').openLibrary(directory),renderer=require('../bookshop/device/frame-service.cjs');
const archiveDir=path.join(project,'outputs/wordcard-packs');
const types={'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.css':'text/css; charset=utf-8','.json':'application/json','.wav':'audio/wav','.zip':'application/zip'};
function file(req,res,f){
 if(!types[path.extname(f)]||!fs.statSync(f).isFile())throw Error('找不到文件');
 res.writeHead(200,{'Content-Type':types[path.extname(f)],'Content-Length':fs.statSync(f).size,'Cache-Control':'no-cache','X-Content-Type-Options':'nosniff'});
 if(req.method==='HEAD')res.end();else fs.createReadStream(f).on('error',()=>res.destroy()).pipe(res);
}
function json(res,code,obj){res.writeHead(code,{'Content-Type':'application/json; charset=utf-8','Cache-Control':'no-store'});res.end(JSON.stringify(obj));}
async function install(req,res){
 if(req.method!=='POST'||req.headers.origin!=='http://'+req.headers.host||req.headers['content-type']!=='application/zip')return json(res,403,{error:'请从本机内容库选择 ZIP 文件'});
 const tmp=await fs.promises.mkdtemp(path.join(directory,'.upload-')),archive=path.join(tmp,'package.zip');
 try{
  let size=0;
  await pipeline(req,new Transform({transform(chunk,_,cb){size+=chunk.length;cb(size>256*1024**2?Error('包超过 256 MB'):null,chunk);}}),fs.createWriteStream(archive,{flags:'wx'}));
  const py=process.env.WORDCARD_PYTHON||(process.platform==='win32'?path.join(process.env.LOCALAPPDATA,'Programs/Python/Python310/python.exe'):'python3');
  try{
   const {stdout}=await promisify(execFile)(py,[path.join(__dirname,'packages.py'),archive,directory],{env:{...process.env,PYTHONUTF8:'1',PYTHONIOENCODING:'utf-8'},windowsHide:true});
   library.refresh();json(res,200,JSON.parse(stdout));
  }catch(e){let msg='无法安装这个包';try{msg=JSON.parse(e.stdout).error||msg;}catch{}json(res,400,{error:msg});}
 }finally{await fs.promises.rm(archive,{force:true});await fs.promises.rmdir(tmp);}
}
http.createServer(async(req,res)=>{
 try{
  if(!['127.0.0.1:'+port,'localhost:'+port].includes(req.headers.host))return json(res,403,{error:'仅限本机'});
  const url=new URL(req.url,'http://127.0.0.1'),n=decodeURIComponent(url.pathname),key=url.searchParams.get('key');
  if(n==='/api/install')return await install(req,res);
  if((n==='/'||n==='/index.html')&&library.get(key)?.m.schema===2){
   res.writeHead(302,{Location:'/reader.html?key='+encodeURIComponent(key),'Cache-Control':'no-store'});res.end();return;
  }
  if(n==='/device-frame'){
   const r=library.get(key);if(key&&!r)return json(res,404,{error:'包尚未安装'});
   return renderer.handle(req,res,r?.data.art?path.join(r.dir,r.data.art):path.join(root,'river.wpix'));
  }
  if(!['GET','HEAD'].includes(req.method))return json(res,405,{error:'请求方式错误'});
  if(n==='/api/library')return json(res,200,{installed:library.list(),downloads:fs.existsSync(archiveDir)?fs.readdirSync(archiveDir).filter(f=>f.endsWith('.zip')):[]});
  if(n==='/api/open'){try{return json(res,200,library.compose(key));}catch(e){return json(res,409,{error:e.message});}}
  if(n.startsWith('/packs/')){
   const [, ,id,...parts]=n.split('/'),r=library.get(id),rel=parts.join('/');
   if(!r||!Object.hasOwn(r.m.files,rel))throw Error('资源不存在');
   return file(req,res,path.join(r.dir,rel));
  }
  if(n.startsWith('/downloads/')){
   const name=n.slice(11);if(!/^[a-z0-9.-]+\.zip$/.test(name))throw Error('无效文件');
   return file(req,res,path.join(archiveDir,name));
  }
  if(!['/','/index.html','/library.html','/library.js','/shop.css','/voyage.js','/reader.html','/reader.js','/reader.css'].includes(n))throw Error('找不到页面');
  const f=path.join(root,n==='/'?'index.html':n.slice(1));
  return file(req,res,f);
 }catch(e){if(!res.headersSent)json(res,404,{error:e.message});else res.end();}
}).listen(port,'127.0.0.1',()=>console.log('Wordcard content library http://127.0.0.1:'+port));
