"""Catalog identity, revision conflicts, and interrupted writes through the firmware bridge."""
import argparse
import hashlib
import json
import runpy
import time
from pathlib import Path
from tempfile import TemporaryDirectory

s=runpy.run_path(str(Path(__file__).with_name('test-shared-vocabulary.py')))


def install(folder,name,kind,data,deck=None):
    root=folder/'packs'/name/'1.0.0'
    root.mkdir(parents=True)
    raw=json.dumps(data,ensure_ascii=False).encode()
    digest=hashlib.sha256(raw).hexdigest()
    manifest=dict(id=name,title=name,type=kind,version='1.0.0',format='wordcard-pack',
                  schema=3 if kind=='story' else 1,files={'content.json':digest})
    if deck:manifest.update(requires=deck,storyType='vocabulary')
    (root/'manifest.json').write_text(json.dumps(manifest),'utf8')
    (root/'content.json').write_bytes(raw)
    return name+'@1.0.0',digest


def test(args,folder):
    s['prepare'](folder,args)
    defs=[]
    for label,gloss,identity in [('alpha','河岸','bank.river'),('beta','河岸','bank.river'),
                                 ('gamma','银行','bank.river'),('delta','河岸',None)]:
        item={'word':'bank','meaning':'n. '+gloss}
        if identity:item['senseId']=identity
        deck,deck_digest=install(folder,label,'deck',{'words':{'local':item}})
        book,digest=install(folder,label+'-story','story',{'terms':{'bank':{'id':'local','meaning':item['meaning']}}},deck)
        defs.append(dict(book=book,digest=digest,deck=deck,deckDigest=deck_digest))
    device=s['Device'](args.exe,folder)

    def call(i,**payload):
        device.proc.stdin.write('story-request '+json.dumps({**defs[i],**payload})+'\n')
        device.proc.stdin.flush()
        return json.loads(device.proc.stdout.readline())

    def save(i,rev,step):
        return call(i,op='save',run={'step':step},rev=rev,
                    answer=dict(local='local',skill='meaning',correct=False,assisted=False,retry=False))

    try:
        device.command(f'clock {int(time.time()*1000)+60000}')
        one=save(0,0,1)
        shared=one['cards']['local']['meaning']
        assert call(1,op='read')['cards']['local']['meaning']==shared
        assert call(2,op='read')['cards']==call(3,op='read')['cards']=={}
        different=save(2,0,1)['cards']['local']['meaning']
        unknown=save(3,0,1)['cards']['local']['meaning']
        assert len({x['senseId'] for x in (shared,different,unknown)})==3
        assert 'error' in save(0,0,2)
        assert call(0,op='read')==one
        journal=folder/'learning-web/vocabulary-v2/events.ndjson'
        held=journal.with_suffix('.held')
        journal.rename(held);journal.mkdir()
        try:
            assert 'error' in save(0,1,2)
        finally:
            journal.rmdir();held.rename(journal)
        device.close();device=s['Device'](args.exe,folder)
        assert call(0,op='read')==one
        next_step=save(0,1,2)
        assert next_step['rev']==2 and next_step['cards']['local']['meaning']['attempts']==2
        print('PASS confirmed cross-deck sharing, conflicting gloss, undeclared sense, stale write, SD write failure, restart, single successful retry')
    finally:device.close()


if __name__=='__main__':
    p=argparse.ArgumentParser()
    for key in ('exe','library','catalog'):p.add_argument('--'+key,type=Path,required=True)
    with TemporaryDirectory(dir=s['ROOT']/'work') as tmp:test(p.parse_args(),Path(tmp))

