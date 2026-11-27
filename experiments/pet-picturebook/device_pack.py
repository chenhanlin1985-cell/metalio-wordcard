"""Encode the six approved illustration panels and bind the story to its PET deck."""
from pathlib import Path
import argparse
import json
import re
import struct
import sys
import zipfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'experiments/pet-voyage'))
from packages import pack_folder, install
HERE = Path(__file__).parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--story', type=Path, default=HERE/'stone-soup.json')
args=parser.parse_args()
source = json.loads(args.story.read_text(encoding='utf8'))
book_id=source.get('id','stone-soup')
assert re.fullmatch(r'[a-z][a-z0-9-]{0,63}',book_id)
first_book=book_id=='stone-soup'
stage = ROOT/'work'/('picturebook-device' if first_book else book_id)/'pack-source'
stage.mkdir(parents=True, exist_ok=True)
deck = json.loads((ROOT/'work/wordcard-content/library/pet-2024/1.0.0/content.json').read_text(encoding='utf8'))
order = source.get('recallOrder') if not first_book else json.loads(re.search(r"const order=(\[[^;]+\]);", (HERE/'reader.html').read_text(encoding='utf8'))[1].replace("'",'"'))
terms = {}
for number, word, page, meaning, cue, *choices in source['terms']:
    key='pet-'+number
    assert deck['words'][key]['word']==word
    line=next(s.strip('“” ') for s in re.split(r'(?<=[.!?])\s+',source['scenes'][page-1]['en']) if re.search(r'\b'+word+r'\b',s,re.I))
    terms[word]=dict(id=key,meaning=meaning,cue=cue,example=source.get('examples',{}).get(word,line),distractors=choices)
for key, meaning in source.get('returnWords',{}).items():
    entry=deck['words'][key]
    assert entry['word'] not in terms
    # A recurring gloss can be heard without creating another review card.
    terms[entry['word']]=dict(relatedId=key,meaning=meaning,ipa=entry['ipa'],audio='packs/pet-2024/1.0.0/'+entry['audio'])
pattern=re.compile(r"\b("+"|".join(map(re.escape,terms))+r")\b",re.I)
pages=[dict(en=pattern.sub(lambda m:'[['+m[0]+']]',p['en']),zh=p['zh'],image=i) for i,p in enumerate(source['scenes'])]
def write(name,obj):
    (stage/name).write_text(json.dumps(obj,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
write('content.json',dict(style='read-first', art='pages.wpix', terms=terms, englishTitle=source['en'],
    author=source.get('author','欧洲民间故事'),attribution=source.get('attribution','据石头汤民间故事独立改写'),intro=source.get('intro','口袋里只有一块石头，能给一村人做顿晚饭吗？'),
    source=source.get('source','https://www.cm-almeirim.pt/conhecer-almeirim/gastronomia/a-lenda-da-sopa-da-pedra'),
    chapters=[dict(title=source['title'],pages=pages,recallOrder=order,questions=[])]))
write('manifest.json',dict(format='wordcard-pack',schema=2,type='story',id=book_id,version='1.0.0',title=source['title'],requires='pet-2024@1.0.0'))
# WPIX stores black as a set bit. Sample each cell at the reader's native display size.
atlas=Image.open(args.story.parent/(book_id+'.png')).convert('L')
w,h=atlas.size
header=bytearray(struct.pack('<8sHHHH',b'WPIX0002',6,16,0,0)); payload=bytearray()
for i in range(6):
    x,y=i%2,i//2
    box=source['panelBounds'][i] if 'panelBounds' in source else (round(x*w/2),round(y*h/3),round((x+1)*w/2),round((y+1)*h/3))
    assert 0<=box[0]<box[2]<=w and 0<=box[1]<box[3]<=h
    panel=atlas.crop(tuple(box))
    frame=panel.resize((576,384),Image.Resampling.LANCZOS)
    bits=frame.point(lambda shade:255 if shade<160 else 0,mode='1').tobytes()
    check=2166136261
    for v in bits:check=((check^v)*16777619)&0xffffffff
    header.extend(struct.pack('<IIHHI',16+6*16+len(payload),len(bits),576,384,check))
    payload.extend(bits)
(stage/'pages.wpix').write_bytes(header+payload)
out=ROOT/'outputs'/('pet-picturebook-device' if first_book else book_id)/(book_id+'-1.0.0.zip')
if out.exists():
    with zipfile.ZipFile(out) as archive:
        manifest=json.loads(archive.read('manifest.json'))
        files=manifest.pop('files')
        assert manifest==json.loads((stage/'manifest.json').read_bytes())
        assert set(files)=={'content.json','pages.wpix'}
        if any(archive.read(name)!=(stage/name).read_bytes() for name in files):
            raise ValueError('该版本的内容已发布；修改绘本时请提高版本号。')
else:
    pack_folder(stage,out)
print(install(out,ROOT/'work/wordcard-content/library'))

