"""Install a ZIP as an immutable directory. Only content and media can enter the library."""
from pathlib import Path
from hashlib import sha256
import argparse, json, os, re, stat, struct, tempfile, zipfile, shutil, sys, wave, io
ID=r'[a-z0-9][a-z0-9-]{0,63}'
REV=r'(?:0|[1-9][0-9]{0,5})(?:\.(?:0|[1-9][0-9]{0,5})){2}'
def need(v,msg):
    if not v:raise ValueError(msg)
def object_from(b):
    def unique(pairs):
        obj={}
        for k,v in pairs:
            need(k not in obj and k not in ('__proto__','constructor','prototype'),'JSON 字段重复或无效')
            obj[k]=v
        return obj
    return json.loads(b,object_pairs_hook=unique)
def text(v):return isinstance(v,str) and 0<len(v.strip())<=2048
def media_path(kind,name):
    pattern=r'audio/[A-Za-z0-9_-][A-Za-z0-9._-]*\.wav' if kind=='deck' else r'[a-z0-9_-]+\.wpix'
    return isinstance(name,str) and len(name)<=160 and re.fullmatch(pattern,name)
def image_count(b, dimensions=(384,192)):
    need(len(b)>=16 and b[:8]==b'WPIX0002','插图格式不符')
    n,stride=struct.unpack_from('<HH',b,8);need(0<n<=128 and stride==16 and len(b)>=16+16*n,'插图索引不完整')
    for i in range(n):
        off,size,w,h,check=struct.unpack_from('<IIHHI',b,16+16*i)
        need((w,h)==dimensions and size==w*h//8 and 16+16*n<=off<=len(b)-size,'场景图尺寸或位置错误')
        crc=2166136261
        for x in b[off:off+size]:crc=((crc^x)*16777619)&0xffffffff
        need(crc==check,'场景图校验失败')
    return n
def content(m,data,folder):
    need((folder/'content.json').stat().st_size<=2*1024*1024,'正文超过设备读取上限')
    media=set(m['files'])-{'content.json'}
    if m['type']=='deck':
        words=data['words'];topics=data['topics']
        need(isinstance(words,dict) and 1<=len(words)<=30000 and isinstance(topics,dict) and topics and all(text(v) for v in topics.values()),'词条或主题表不完整')
        audio=set()
        for i,w in words.items():
            need(re.fullmatch(r'[a-zA-Z0-9:_-]{1,128}',i) and w['topic'] in topics and all(text(w[k]) for k in ('word','ipa','meaning')),'词条格式错误 '+i)
            if w.get('audio'):audio.add(w['audio'])
        need(audio==media,'词条发音和包内文件不一致')
        for n in audio:
            with wave.open(str(folder/n)) as f:need(f.getnchannels()==1 and f.getnframes()>0 and f.getsampwidth()==2 and f.getcomptype()=='NONE','需要单声道 PCM WAV '+n)
        return
    dep=m['requires']
    need(isinstance(dep,str) and re.fullmatch(ID+'@'+REV,dep),'依赖词库信息错误')
    from story_schema import TYPES, check_reader
    kind=m.get('storyType')
    need(m['schema']==3 and kind in TYPES, '故事需要 schema 3 和明确的学习类型')
    need(text(data.get('intro')) and 1<=len(data['chapters'])<=500, '故事缺少简介或章节')
    if kind!='adventure':
        art=data.get('art')
        need(media==({art} if art else set()),'故事配图清单不一致')
        frames=image_count((folder/art).read_bytes(), (576,384)) if art else 0
        check_reader(kind,data,frames,text,need)
        return
    art=data.get('art');need(media==({art} if art else set()),'故事插图清单不一致')
    images=image_count((folder/art).read_bytes()) if art else 0
    chapters=data['chapters'];n=len(chapters)
    need(1<=n<=500 and text(data['intro']) and text(data.get('firstAction','开始故事')),'故事缺少开场或章节')
    acts=data.get('acts',[{'start':0,'title':m['title']}]);need(acts and [a['start'] for a in acts]==sorted({a['start'] for a in acts}) and acts[0]['start']==0,'幕次起点错误')
    for a in acts:need(0<=a['start']<n and text(a['title']) and ('image' not in a or 0<=a['image']<images),'幕次或配图引用错误')
    forks=data.get('forks',[]);need(len(forks)<=8,'分支过多')
    for x in forks:need(isinstance(x['after'],int) and 1<=x['after']<=n and re.fullmatch(ID,x['key']) and text(x['title']) and text(x['body']) and len(x['options'])==2 and all(text(z) for z in x['options']),'故事分支无效')
    for rows in [forks,data.get('gates',[])]:
        need([x['after'] for x in rows]==sorted({x['after'] for x in rows}),'章节节点顺序错误')
    need(len({x['key'] for x in forks})==len(forks),'分支标识重复')
    combos=['']
    for x in forks:combos=[a+b for a in combos for b in ('A','B')]
    endings=data.get('endings',{}) if forks else {'default':data.get('ending',[])}
    need(all(key in endings for key in (combos if forks else ['default'])) and all(len(v)==2 and all(text(z) for z in v) for v in endings.values()),'缺少结局或结局内容')
    for x in data.get('gates',[]):
        title,prompt,correct,opts,win,lose=x['test']
        need(1<=x['after']<=n and all(text(z) for z in (title,prompt,win,lose)) and correct in opts and len(set(opts))==len(opts) and 2<=len(opts)<=4 and all(isinstance(z,int) and 0<=z<x['after'] for z in opts),'证物关引用错误')
    known=set()
    def line(pair):return isinstance(pair,list) and len(pair)==2 and all(text(z) for z in pair) and pair[0].count('[ ]')==1
    for idx,c in enumerate(chapters):
        ids=[w['id'] for w in c['words']];need(all(text(c[k]) for k in ('title','intro','token','outro','topic')) and 1<=len(set(ids))==len(ids)<=30,'章节内容不完整')
        need(all(line([w['en'],w['zh']]) and ('meaning' not in w or text(w['meaning'])) for w in c['words']),'缺少剧情词的双语题目')
        cond=c.get('branchKey')
        if cond:need(any(x['key']==cond and x['after']<=idx for x in forks),'章节引用尚未开放的分支')
        if 'branchIntro' in c:need(cond and set(c['branchIntro'])=={'A','B'} and all(text(z) for z in c['branchIntro'].values()),'分支引言无效')
        returns=c.get('returns',{})
        need(not returns or set(returns)=={'both'} or (cond and set(returns)=={'A','B'}),'回访分支不完整')
        for i,en,zh in returns.values():need(i in known and line([en,zh]),'回访词应来自前面的章节')
        known.update(ids)
def install(archive,library):
    library.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(archive) as z,tempfile.TemporaryDirectory(dir=library,prefix='.staging-') as tmp:
        tmp=Path(tmp);folder=tmp/'content';folder.mkdir()
        rows=z.infolist();names=[r.filename for r in rows]
        need(2<=len(rows)<=32000 and len(set(n.lower() for n in names))==len(names) and sum(r.file_size for r in rows)<=384*1024**2,'文件数量、大小或重名不合要求')
        need('manifest.json' in names and z.getinfo('manifest.json').file_size<=4*1024**2,'缺少有效清单')
        m=object_from(z.read('manifest.json'));need(m['format']=='wordcard-pack' and ((m['schema']==1 and m['type']=='deck') or (m['schema']==3 and m['type']=='story')) and m['type'] in ('deck','story') and re.fullmatch(ID,m['id']) and re.fullmatch(REV,m['version']) and text(m['title']),'无法识别的包格式或版本')
        reserved=r'(?i)(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\..*)?'
        need(not re.fullmatch(reserved,m['id']),'包标识不可作目录名')
        need(set(names)=={'manifest.json',*m['files']} and 'content.json' in m['files'] and 'manifest.json' not in m['files'],'文件清单不完整')
        for row in rows:
            n=row.filename;need(not stat.S_ISLNK(row.external_attr>>16) and row.file_size<=48*1024**2,'无效的包文件')
            if n=='manifest.json':continue
            need(n=='content.json' or media_path(m['type'],n),'不支持的资源路径 '+n)
            need(not re.fullmatch(reserved,n.split('/')[-1]) and len(n)<=160,'无效的文件名')
            b=z.read(row);need(sha256(b).hexdigest()==m['files'][n],'文件校验失败 '+n)
            dst=folder/n;dst.parent.mkdir(exist_ok=True);dst.write_bytes(b)
        content(m,object_from((folder/'content.json').read_bytes()),folder)
        (folder/'manifest.json').write_bytes(z.read('manifest.json'))
        target=library/m['id']/m['version'];target.parent.mkdir(exist_ok=True)
        try:folder.rename(target)
        except OSError:
            if not target.is_dir():raise
            need(object_from((target/'manifest.json').read_bytes())==m,'该编号和版本已用于不同内容，请更换版本号')
            return '已安装相同版本',m
        return '安装完成',m
def pack_folder(source,destination):
    source=source.resolve(strict=True)
    need(source.is_dir(),'请选择内容包源目录')
    need(destination.suffix.lower()=='.zip','输出文件需要使用 .zip 扩展名')
    need(not destination.exists(),'输出文件已存在，请选择新文件名')
    def read(name):
        file=(source/name).resolve(strict=True)
        need(file.is_relative_to(source) and file.is_file(),'资源必须位于源目录内 '+name)
        need(file.stat().st_size<=48*1024**2,'单个资源文件过大 '+name)
        return file.read_bytes()
    manifest=object_from(read('manifest.json'))
    manifest.update(format='wordcard-pack',schema=manifest.get('schema',1),files={})
    kind=manifest['type']
    need(kind in ('story','deck'),'包类型只能是 deck 或 story')
    body=read('content.json')
    data=object_from(body)
    if kind=='deck':
        media={w['audio'] for w in data['words'].values() if w.get('audio')}
    else:
        media={data['art']} if data.get('art') else set()
    need(all(media_path(kind,n) for n in media),'内容中引用了不支持的资源路径')
    destination.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent,prefix='.pack-') as tmp:
        folder=Path(tmp)
        archive=folder/'content.zip'
        with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=3) as z:
            for name in ['content.json',*sorted(media)]:
                payload=body if name=='content.json' else read(name)
                manifest['files'][name]=sha256(payload).hexdigest()
                z.writestr(name,payload)
            z.writestr('manifest.json',json.dumps(manifest,ensure_ascii=False,indent=2).encode('utf-8'))
        install(archive,folder/'check')
        # Publish a fully validated archive without replacing an existing file.
        if os.name=='nt':
            archive.rename(destination)
        else:
            os.link(archive,destination)
    return '打包完成',manifest

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--pack',action='store_true');ap.add_argument('source',type=Path);ap.add_argument('destination',type=Path)
    args=ap.parse_args()
    try:
        operation=pack_folder if args.pack else install
        note,m=operation(args.source,args.destination);print(json.dumps({'message':note,'id':m['id'],'version':m['version']},ensure_ascii=False))
    except Exception as e:
        print(json.dumps({'error':str(e) or '内容包无效'},ensure_ascii=False));sys.exit(1)
