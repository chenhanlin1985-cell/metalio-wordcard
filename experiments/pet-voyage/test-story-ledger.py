"""Timed, rendered-device regression through the production QuickJS and native store."""
import argparse
import json
import runpy
import shutil
import time
from pathlib import Path
from tempfile import TemporaryDirectory

shared = runpy.run_path(str(Path(__file__).with_name('test-shared-vocabulary.py')))
Device, prepare, restore = (shared[k] for k in ('Device', 'prepare', 'restore'))


def status(device):
    device.proc.stdin.write('status\n')
    device.proc.stdin.flush()
    return json.loads(device.proc.stdout.readline())


def test(args, folder):
    prepare(folder, args)
    epoch = int(time.time() * 1000) + 60000
    d = Device(args.exe, folder)

    def pick(gloss):
        d.tap(next(k for k in d.buttons if k[3:] == gloss))

    def story():
        d.tap('内容库')
        d.tap('口袋里的收据')

    def latest():
        return next(iter(restore(folder)['storyRuns'].values()))

    try:
        d.command(f'clock {epoch}')
        d.tap(next(k for k in d.buttons if k in ('简单模式','普通模式','困难模式')))
        d.tap('简单');d.tap('使用简单模式')
        story()
        d.tap('开始分段练')
        d.tap('练这四词')
        assert not restore(folder)['cards']
        d.tap('想好了')
        pick('演讲')
        assert d.page['title'] == '订正词义', d.page
        projection = status(d)['memory']['meaning']
        sense = projection['senseId']
        state = restore(folder)
        card = state['cards'][sense]['meaning']
        assert card['lastReviewAt'] == epoch and card['due'] == epoch + 900000
        assert card['lapses'] == card['reps'] == 1 and card['pendingRelearn']
        assert card['due'] == projection['due'] and sense in state['seen']
        cursor = latest()['run']
        assert not {'cards','recent','practised'}.intersection(cursor)
        assert cursor['segments']['stage'] == 'feedback'
        d.command('home')
        d.tap('复习')
        assert '开始复习' not in d.buttons, (d.page,restore(folder)['settings'])
        d.command('home')
        d.command(f'clock {card["due"]+1}')
        d.tap('复习')
        d.tap('开始复习')
        session = restore(folder)['session']
        assert session['source'] == 'review' and session['ids'] == [sense]
        d.tap('音乐会')
        assert d.page['title'] == '答对了'
        after = restore(folder)['cards'][sense]['meaning']
        assert after['lastReviewAt'] == card['due']+1 and after['reps'] == 2
        assert not after['pendingRelearn'] and after['due'] == after['fsrsDue']
        d.command('home')
        story()
        assert latest()['run'] == cursor
        assert status(d)['memory']['meaning']['due'] == after['due']
        d.tap('继续'); pick('音乐'); d.tap('继续'); d.tap('想好了'); pick('顾客')
        d.tap('继续'); pick('袖子'); d.tap('继续')
        assert d.page['title'] == '错词回练'
        d.tap('想好了'); pick('音乐会')
        assert restore(folder)['cards'][sense]['meaning'] == after
        assert not list((folder/'learning-web').glob('pack-*.json'))
        print(f'PASS concert story -> PET -> story: {sense}, due {card["due"]} -> {after["due"]}, reps 1 -> 2, stale correction left card intact', flush=True)
        # Complete the remaining real scenes, including a word whose deck entry mixes POS.
        while latest()['run']['segments']['stage'] != 'done':
            segment = latest()['run']['segments']
            if segment['stage'] == 'feedback': d.tap('继续')
            elif segment['stage'] == 'read': d.tap('练这四词')
            elif segment['stage'] == 'think': d.tap('想好了')
            elif segment['stage'] == 'choose':
                letter = 'ABCD'[segment['slot']]
                d.tap(next(k for k in d.buttons if k.startswith(letter+'  ')))
            else: raise AssertionError(segment)
        state = restore(folder)
        separate = [(i,w) for i,w in state['deckLexicon'].items() if w['word']=='cost' and i in state['cards']]
        assert len(separate)==1 and separate[0][0].startswith('story:') and separate[0][1]['pos']=='v.',separate
        assert len(latest()['stamps']) == 1 and len(latest()['stamps'][0]['ids']) == 20
        # Reopen after process exit and compare the canonical journal and the reader.
        d.close()
        d=Device(args.exe,folder)
        d.command(f'clock {card["due"]+2}')
        story()
        assert d.page['title']=='全书练完了' and restore(folder)['cards'][sense]['meaning']==after
        print('PASS 20 real senses, distinct story cost, 20 stamp links, restart and durable due', flush=True)
    finally:
        d.close()


def adventure(args,folder,name):
    prepare(folder,args)
    root=folder/'packs'/name/'1.1.0'
    root.mkdir(parents=True)
    for filename in ('manifest.json','content.json'):
        shutil.copyfile(args.library/name/'1.1.0'/filename,root/filename)
    title=json.loads((root/'manifest.json').read_bytes())['title']
    d=Device(args.exe,folder)
    try:
        d.tap('内容库');d.tap(title);d.tap('接下这件委托')
        while '读下一个词' in d.buttons:d.tap('读下一个词')
        d.tap('去现场试试')
        d.tap('想不起来')
        s=restore(folder)
        memory=status(d)['memory']['meaning']
        card=s['cards'][memory['senseId']]['meaning']
        assert card['lapses']==1 and card['due']==memory['due']
        assert not list((folder/'learning-web').glob('pack-*.json'))
        print('PASS adventure',name,'graded its first wrong word on the native card')
    finally:d.close()


if __name__=='__main__':
    p=argparse.ArgumentParser()
    for key in ('exe','library','catalog'):p.add_argument('--'+key,type=Path,required=True)
    args=p.parse_args()
    with TemporaryDirectory(dir=shared['ROOT']/'work') as tmp:
        test(args,Path(tmp)/'receipt')
        for name in ('rainy-lost-found','last-book-ferry'):adventure(args,Path(tmp)/name,name)

