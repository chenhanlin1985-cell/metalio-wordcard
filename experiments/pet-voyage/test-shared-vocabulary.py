"""Exercise the shipping shelf, device button geometry, keyboard, and shared learning modes."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[2]
PET = 'pet-2024@1.0.0'


def restore(directory):
    store = directory / 'learning-web/vocabulary-v2'
    checkpoint = json.loads(json.loads((store/'state.json').read_bytes())['data'])
    state = checkpoint['state']
    with (store/'events.ndjson').open('rb') as journal:
        journal.seek(checkpoint['journalOffset'])
        for line in journal:
            for patch in json.loads(json.loads(line)['data'])['patch']:
                path = [p.replace('~1', '/').replace('~0', '~') for p in patch['path'][1:].split('/')]
                node = state
                for part in path[:-1]:
                    node = node[int(part) if isinstance(node, list) else part]
                key = path[-1]
                if isinstance(node, list):
                    key = len(node) if key == '-' else int(key)
                if patch['op'] == 'remove':
                    del node[key]
                elif patch['op'] == 'add' and isinstance(node, list):
                    node.insert(key, patch['value'])
                else:
                    assert patch['op'] in ('add','replace'), patch
                    node[key] = patch['value']
    return state


class Device:
    def __init__(self, exe, folder):
        self.proc = subprocess.Popen([exe, (folder/'learning-web').as_posix(), ROOT/'assets/font.fontpack',
                                      folder/'frame.pgm', '100', ROOT/'assets/learning-kaishu-v3.fontpack'],
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, encoding='utf8')
        self.read()

    def read(self):
        self.page = json.loads(self.proc.stdout.readline())
        assert not self.page['note'], self.page
        self.buttons = {b['label']: b for b in self.page['buttons'] if b['enabled']}

    def command(self, command):
        self.proc.stdin.write(command+'\n')
        self.proc.stdin.flush()
        self.read()

    def tap(self, label):
        if label not in self.buttons and '下一屏' in self.buttons:
            self.tap('下一屏')
        assert label in self.buttons, (label, self.page['title'], list(self.buttons))
        b = self.buttons[label]
        self.command(f"tap {b['x']+b['w']//2} {b['y']+b['h']//2}")

    def close(self):
        self.proc.stdin.close()
        assert self.proc.wait(timeout=20) == 0


def prepare(folder, args):
    shutil.copytree(args.catalog, folder/'vocabulary')
    for name in ('gre-trial','pet-2024','pocket-receipt'):
        version = '1.1.0' if name=='pocket-receipt' else '1.0.0'
        dest = folder/'packs'/name/version
        dest.mkdir(parents=True)
        for f in ('content.json','manifest.json'):
            shutil.copyfile(args.library/name/version/f, dest/f)


def test_mode(args, folder, title, mode, kind):
    prepare(folder,args)
    words = {r[0]: (r[1],r[2]) for row in (args.catalog/'catalog.tsv').read_text('utf8').splitlines()
             if len(r := row.split('\t')) >= 5}
    device = Device(args.exe,folder)
    try:
        device.tap('内容库'); device.tap('词库')
        device.tap(title)
        original = restore(folder)['session']
        device.tap('词根卡')
        assert device.page['title'] == '词根卡'
        device.tap('返回词卡')
        assert restore(folder)['session'] == original
        for m in ('简单',mode):
            device.tap('模式'); device.tap(m); device.tap('使用'+m+'模式')
        for _ in range(4):
            device.tap('下一个词')
        device.tap('开始测试')
        counts = Counter()
        while True:
            state = restore(folder)
            session = state['session']
            if session['phase'] == 'summary':
                break
            assert sum(counts.values()) < 16
            task = next(t for t in session['tasks'] if t['uid'] == session['current'])
            k, wid = task['kind'],task['id']
            if wid in words:
                word,meaning = words[wid]
            else:
                w = state['deckLexicon'][wid]
                word,meaning = w['word'],w['meaning']
            if k == 'meaning':
                assert not device.page['keyboard']
                device.tap(meaning)
            else:
                assert k == kind and device.page['keyboard'], (k,device.page)
                letters = [c for c in word if c.isascii() and c.isalpha()]
                partial = (letters[1:-1] if len(letters)>3 else letters)[:min(2,len(letters)-1)]
                for c in (word if k == 'recall' else partial):
                    device.command('key '+str(ord(c)))
                device.command('key 13')
            assert device.page['title'] == '答对了', (word,device.page)
            counts[k] += 1
            label = next(b for b in device.buttons if b in ('下一题','开始补字母','开始想英文','查看小结','开始完整拼写'))
            device.tap(label)
        assert counts == {'meaning':5,kind:5},counts
        assert all(set(state['cards'][i]) == {'meaning',kind} for i in session['ids'])
        print('PASS', title, mode, dict(counts), 'real keyboard submissions; root card returns to same word')
    finally:
        device.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--catalog',type=Path,required=True)
    parser.add_argument('--library',type=Path,required=True)
    args = parser.parse_args()
    with TemporaryDirectory(dir=ROOT/'work') as tmp:
        test_mode(args,Path(tmp)/'gre','GRE 难词试学','困难','recall')
        test_mode(args,Path(tmp)/'pet','PET 主题词库','普通','partial')


