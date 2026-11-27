"""Exercise the shipped story with the native canvas and the real 684 x 1216 font."""
import argparse
import json
from pathlib import Path
import re
import runpy
import shutil
import subprocess
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[2]


def verify_buttons(view, frame):
    if view['route'] != 'story-reader':
        return
    pixels = frame.read_bytes().split(b'255\n', 1)[1]
    assert len(pixels) == 684 * 1216
    buttons = [b for b in view['buttons'] if b['enabled']]
    for b in buttons:
        x, y, w, h = (b[k] for k in ('x', 'y', 'w', 'h'))
        assert 0 <= x < x + w <= 684 and 0 <= y < y + h <= 1216, b
        # Ignore the outline. A 36 px glyph line fits in a 48 px ink band.
        rows = [row for row in range(y + 8, y + h - 8)
                if min(pixels[row * 684 + x + 8:row * 684 + x + w - 8]) < 100]
        assert rows and max(rows) - min(rows) <= 48, (view['title'], b['label'], rows)
    letters = [b for b in buttons if b['label'] in ('A','B','C','D')]
    if letters:
        assert len({b['y'] for b in letters}) == 1
        assert min(b['w'] for b in letters) >= 145
        assert all(a['x'] + a['w'] < b['x'] for a,b in zip(letters,letters[1:]))


def test(exe, library, catalog, receipt=False):
    source = library/'after-twenty-years/1.1.0/content.json'
    book = json.loads(source.read_text(encoding='utf-8'))
    with TemporaryDirectory(dir=ROOT/'work') as directory:
        folder = Path(directory)
        # Use only the two validated packages. Keep the disposable fixture self-contained.
        for name in ('pet-2024', 'pocket-receipt' if receipt else 'after-twenty-years'):
            version='1.0.0' if name=='pet-2024' else '1.1.0'
            src = library/name/version
            dest = folder/'packs'/name/version
            dest.mkdir(parents=True)
            for filename in ('manifest.json','content.json'):
                (dest/filename).write_bytes((src/filename).read_bytes())
            if (src/'pages.wpix').exists():
                shutil.copyfile(src/'pages.wpix',dest/'pages.wpix')
        (folder/'vocabulary').mkdir()
        for filename in ('catalog.json','catalog.tsv','catalog.ndjson'):
            shutil.copyfile(catalog/filename,folder/'vocabulary'/filename)
        frame = folder/'frame.pgm'
        cmd = [exe,(folder/'learning-web').as_posix(),ROOT/'assets/font.fontpack',frame,
               '100',ROOT/'assets/learning-kaishu-v3.fontpack']
        proc = subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,encoding='utf-8')
        def read():
            page = json.loads(proc.stdout.readline())
            assert not page['note'], page['note']
            verify_buttons(page,frame)
            return page
        try:
            page = read()
            def tap(label):
                nonlocal page
                for _ in range(8):
                    visible = {b['label']:b for b in page['buttons'] if b['enabled']}
                    b = visible.get(label) or visible.get('下一屏')
                    assert b, (label,page['title'],list(visible))
                    proc.stdin.write(f"tap {b['x']+b['w']//2} {b['y']+b['h']//2}\n")
                    proc.stdin.flush()
                    page = read()
                    if b['label'] == label:
                        return
                raise AssertionError('unreachable '+label)
            tap('内容库');tap('故事书')
            if receipt:
                tap('口袋里的收据');tap('开始分段练');tap('练这四词')
                tap('想好了')
                def pick(meaning):
                    choices = [b['label'] for b in page['buttons']
                               if re.match(r'^[ABCD]  ', b['label'])]
                    assert len(choices) == len(set(choices)) == 4, choices
                    tap(next(label for label in choices if label[3:] == meaning))
                pick('演讲');assert page['title']=='订正词义'
                tap('继续');pick('音乐');tap('继续')
                tap('想好了');pick('顾客');tap('继续')
                pick('袖子');tap('继续')
                assert page['title']=='错词回练'
                tap('想好了');pick('音乐会');tap('继续')
                assert page['title'].startswith('第 2 段')
                tap('练这四词');tap('想好了');pick('大衣');tap('继续')
                pick('羊毛');tap('继续')
                assert page['title']=='隔段再问'
                tap('想好了');pick('音乐');tap('继续')
                assert page['title']=='隔段再问'
                pick('音乐会')
                print('PASS native four visible Chinese choices, wrong answer, correction and ordinary second encounter')
                return
            tap('二十年后');tap('翻开第一页')
            for i,c in enumerate(book['chapters']):
                for _ in c['pages'][1:]:
                    tap('下一页')
                tap('答理解题')
                for j,q in enumerate(c['questions']):
                    tap('原文');tap('回到原题');tap(chr(65+((q['answer']+1)%len(q['options']) if i==j==0 else q['answer'])))
                    tap('下一题' if j+1<len(c['questions']) else '读下一幕' if i+1<len(book['chapters']) else '读完全书')
            assert page['title']=='全书读毕'
            restore=runpy.run_path(str(Path(__file__).with_name('test-shared-vocabulary.py')))['restore']
            state=restore(folder)
            run=next(iter(state['storyRuns'].values()))['run']
            assert run['complete'] and len(run['answers'])==7 and not state['cards']
            assert run['answers']['0/0']!=book['chapters'][0]['questions'][0]['answer']
        finally:
            proc.stdin.close()
            proc.wait(timeout=15)
    print('PASS native ink and touch bounds for five acts and seven comprehension questions with evidence return')


if __name__=='__main__':
    args=argparse.ArgumentParser()
    args.add_argument('--exe',type=Path,default=ROOT/'work/native-learning/build/Release/native_preview.exe')
    args.add_argument('--library',type=Path,default=ROOT/'work/wordcard-content/library')
    args.add_argument('--catalog',type=Path,default=ROOT/'resources/vocabulary')
    opts=args.parse_args()
    test(opts.exe,opts.library,opts.catalog)
    test(opts.exe,opts.library,opts.catalog,receipt=True)
