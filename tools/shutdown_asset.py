"""Encode original artwork for the panel; never redraw or edit the source PNG.

RGB -> the panel's 16 gray levels, lossless RLE at original dimensions. Screen
fitting is performed by the native Canvas renderer, keeping aspect ratio.
"""
from pathlib import Path
import runpy
ROOT = Path(__file__).resolve().parents[1]
runpy.run_path(str(ROOT / "tools/build_preflight.py"))
from PIL import Image
import hashlib,json
from itertools import groupby
source=ROOT/'assets/shutdown/recharge-original.png'
im=Image.open(source).convert('RGB')
width,height=im.size
rgb=im.get_flattened_data() if hasattr(im,'get_flattened_data') else im.getdata()
pixels=[((299*r+587*g+114*b+500)//1000+8)//17 for r,g,b in rgb]
runs=[]
for shade,group in groupby(pixels):
    count=sum(1 for _ in group)
    while count:
        n=min(count,4096);runs.append(((n-1)<<4)|shade);count-=n
decoded=[run&15 for run in runs for _ in range((run>>4)+1)]
assert decoded==pixels and max(pixels)<=15 and len(decoded)==width*height
dest=ROOT/'firmware/main/generated/shutdown_art.hpp'
text='#pragma once\n#include <cstdint>\nnamespace wordcard::shutdown_art {\n'
text+=f'inline constexpr int width={width},height={height};\ninline constexpr uint16_t runs[]={{\n'
text+='\n'.join(','.join(f'0x{x:04x}' for x in runs[i:i+16])+',' for i in range(0,len(runs),16))
text+='\n};\n}\n'
dest.write_text(text,encoding='utf-8')
record=dict(source='recharge-original.png',source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    source_dimensions=[width,height],gray_levels=16,encoded_bytes=len(runs)*2,lossless_rle_roundtrip=True,
    rendering='Native Canvas fits original aspect ratio, no crop or stretch; white margins',
    generated_with='OpenAI image_gen',pronunciation_source='https://dictionary.cambridge.org/pronunciation/english/recharge',
    usage_sentence='Time to rest and recharge.',
    generated_header_sha256=hashlib.sha256(dest.read_bytes()).hexdigest())
(source.parent/'provenance.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('PASS: original artwork encoded for 16-level e-ink, RLE roundtrip exact;',record['encoded_bytes'],'bytes')
