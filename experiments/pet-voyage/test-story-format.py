"""Build the shipping story fixtures through the public pack writer and importer."""
from pathlib import Path
from tempfile import TemporaryDirectory
import argparse
import copy
import json
import shutil

from packages import content, install, pack_folder

ROOT=Path(__file__).resolve().parents[2]


def prepare(library, decks):
    library.mkdir(parents=True,exist_ok=True)
    sources=sorted((ROOT/'resources/stories').glob('*/*/manifest.json'))
    manifests={p:json.loads(p.read_bytes()) for p in sources}
    required={m['requires'] for m in manifests.values()} | {'gre-trial@1.0.0','pet-2024@1.0.0'}
    dependencies={}
    for key in sorted(required):
        name,version=key.split('@')
        source=decks/name/version
        target=library/name/version
        target.mkdir(parents=True,exist_ok=True)
        for file in ('manifest.json','content.json'):
            shutil.copyfile(source/file,target/file)
        dependencies[key]=json.loads((source/'content.json').read_bytes())
    counts={}
    with TemporaryDirectory(dir=ROOT/'work') as temp:
        for source,m in manifests.items():
            deck=dependencies[m['requires']]
            book=json.loads((source.parent/'content.json').read_bytes())
            kind=m['storyType']
            counts[kind]=counts.get(kind,0)+1
            archive=Path(temp)/(m['id']+'-'+m['version']+'.zip')
            _,built=pack_folder(source.parent,archive)
            assert built==m,m['id']
            install(archive,library)
            if kind=='adventure':
                for chapter in book['chapters']:
                    assert all(deck['words'][w['id']]['topic']==chapter['topic'] for w in chapter['words'])
            else:
                assert all(deck['words'][t['id']]['word']==k for k,t in book['terms'].items() if t.get('id'))
            # Each kind rejects content that could silently degrade its learning activity.
            bad=copy.deepcopy(book)
            if kind=='vocabulary':bad['chapters'][0]['words'][0]['distractors'][0]=bad['chapters'][0]['words'][0]['gloss']
            elif kind=='comprehension':bad['chapters'][0]['questions'][0]['evidence']=999
            else:bad['chapters'][0]['words'][0]['en']='missing blank'
            try:content(m,bad,source.parent)
            except ValueError:pass
            else:raise AssertionError(m['id']+' accepted broken exercise')
    assert all(counts.get(k,0)>=n for k,n in {'vocabulary':3,'comprehension':1,'adventure':2}.items()),counts
    print('PASS shipping packages, installed manifests, media, dependency references and invalid author inputs',counts)


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--library',type=Path,required=True)
    parser.add_argument('--decks',type=Path,required=True)
    args=parser.parse_args()
    prepare(args.library,args.decks)
