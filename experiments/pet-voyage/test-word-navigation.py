"""Walk both five-word learning paths through the device renderer and on-disk resume."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[2]


def include_long_definition(catalog):
    # Give the second card a multi-screen definition in the disposable catalog.
    index = catalog/'catalog.tsv'
    rows = index.read_text('utf8').splitlines()
    at = next(i for i, row in enumerate(rows) if row.split('\t')[1] == 'system' and row.split('\t')[4] == 'everyday')
    fields = rows[at].split('\t')
    with (catalog/'catalog.ndjson').open('r+b') as data:
        data.seek(int(fields[-2]))
        word = json.loads(data.read(int(fields[-1])))
        word['meaning'] = '系统，指多个部分按一定的关系组成的整体。' * 18
        encoded = json.dumps(word, ensure_ascii=False).encode('utf8')
        data.seek(0, 2)
        fields[-2:] = [str(data.tell()), str(len(encoded))]
        data.write(encoded + b'\n')
    rows[at] = '\t'.join(fields)
    index.write_bytes(('\n'.join(rows)+'\n').encode('utf8'))


def exercise(exe, catalog, library, folder, packaged):
    shutil.copytree(catalog, folder/'vocabulary')
    if packaged:
        target = folder/'packs/pet-2024/1.0.0'
        target.mkdir(parents=True)
        for name in ('manifest.json', 'content.json'):
            shutil.copyfile(library/'pet-2024/1.0.0'/name, target/name)
    else:
        include_long_definition(folder/'vocabulary')
    work = folder/'learning-web'
    proc = None
    frame = folder/'card.pgm'

    def read():
        page = json.loads(proc.stdout.readline())
        assert not page['note'], page['note']
        return page

    def launch():
        nonlocal proc
        proc = subprocess.Popen([exe, work.as_posix(), ROOT/'assets/font.fontpack', frame,
                                 '100', ROOT/'assets/learning-kaishu-v3.fontpack'],
                                stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, encoding='utf8')
        return read()

    def close():
        proc.stdin.close()
        assert proc.wait(timeout=15) == 0

    def buttons():
        return {b['label']: b for b in page['buttons'] if b['enabled']}

    def tap(label):
        nonlocal page
        b = buttons().get(label)
        assert b, (page['title'], label, list(buttons()))
        proc.stdin.write(f"tap {b['x'] + b['w']//2} {b['y'] + b['h']//2}\n")
        proc.stdin.flush()
        page = read()

    deck = 'PET 主题词库' if packaged else '日常核心'
    multi_screen = False

    def position(n):
        nonlocal multi_screen
        assert page['title'].endswith(f'{n} / 5'), (n, page)
        controls = buttons()
        assert ('上一个词' in controls) == (n > 1)
        assert ('开始测试' in controls) == (n == 5)
        forward = controls['开始测试' if n == 5 else '下一个词']
        if n > 1:
            previous = controls['上一个词']
            assert previous['y'] == forward['y'] and previous['x']+previous['w'] < forward['x']
        if '下一屏' in controls:
            multi_screen = True
            tap('下一屏')
            assert page['title'].endswith(f'{n} / 5') and '上一屏' in buttons()
            assert ('上一个词' in buttons()) == (n > 1)

    page = launch()
    try:
        tap('内容库'); tap('词库'); tap(deck)
        position(1)
        for n in range(2, 6):
            tap('下一个词'); position(n)
        for n in (4, 3):
            tap('上一个词'); position(n)
        close()
        page = launch()
        tap('继续学习')
        position(3)
        for n in (2, 1):
            tap('上一个词'); position(n)
        for n in range(2, 6):
            tap('下一个词'); position(n)
        tap('开始测试')
        assert '上一个词' not in buttons() and '开始测试' not in buttons()
        if packaged:
            assert not (work/'pack-pet-2024@1.0.0.json').exists()
        else:
            assert multi_screen, 'exercise a native card with separate text pagination'
        frames = [json.loads(json.loads(line)['data']) for line in
                  (work/'vocabulary-v2/events.ndjson').read_bytes().splitlines()]
        flips = [f for f in frames if f['action']['type'] in ('learnNext', 'learnPrev')]
        assert len(flips)==13 and sum(f['action']['type']=='learnPrev' for f in flips)==4
        assert sum(p['path']=='/session/newCount' for f in flips for p in f['patch'])==5
        assert all(not p['path'].startswith('/cards/') for f in flips for p in f['patch'])
        assert all(e['type']=='presentation' for f in flips for e in f['events'])
        assert '认识单词' not in page['title'] and page['route']=='word-batch'
        print(f'PASS {deck}: 1→5→3, restart at 3, 3→1→5, explicit test; paginated={multi_screen}')
    finally:
        if proc.poll() is None:
            close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--library', type=Path, required=True)
    args = parser.parse_args()
    with TemporaryDirectory(dir=ROOT/'work') as tmp:
        for packaged in (False, True):
            exercise(args.exe, args.catalog, args.library, Path(tmp)/('pet' if packaged else 'graded'), packaged)

