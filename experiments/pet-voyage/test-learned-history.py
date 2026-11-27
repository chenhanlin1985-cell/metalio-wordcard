"""Use the physical-device renderer and an isolated profile with native learning records."""
import argparse
import json
from pathlib import Path
import runpy
from tempfile import TemporaryDirectory

support=runpy.run_path(str(Path(__file__).with_name('test-shared-vocabulary.py')))
Device, prepare, restore, ROOT=(support[k] for k in ('Device','prepare','restore','ROOT'))


def exercise(args,folder):
    prepare(folder,args)
    device=Device(args.exe,folder)

    def tap(label):
        for _ in range(8):
            if label in device.buttons:
                device.tap(label);return
            if '下一屏' not in device.buttons:break
            device.tap('下一屏')
        raise AssertionError((label,device.page['title'],list(device.buttons)))

    def search(term):
        tap('查找')
        for letter in term:device.command('key '+str(ord(letter)))
        device.command('key 13')
        assert device.page['title']=='已学单词',device.page

    try:
        tap('内容库');tap('词库');tap('PET 主题词库')
        tap('模式');tap('简单');tap('使用简单模式')
        for _ in range(4):tap('下一个词')
        tap('开始测试')
        while restore(folder)['session']['phase']!='summary':
            state=restore(folder);group=state['session']
            task=next(t for t in group['tasks'] if t['uid']==group['current'])
            word=state['deckLexicon'].get(task['id'])
            if word:gloss=word['meaning']
            else:gloss=next(r.split('\t')[2] for r in (args.catalog/'catalog.tsv').read_text('utf8').splitlines() if r.startswith(task['id']+'\t'))
            tap(gloss)
            tap('查看小结' if '查看小结' in device.buttons else '下一题')
        device.command('home')
        tap('已学单词');tap('按词组找')
        label=next(k for k in device.buttons if ' · ' in k)
        tap(label);tap('重练这组词');assert '/5' in device.page['title']
        tap('返回词卡');tap('返回词组');tap('返回已学单词');tap('回首页')
        tap('内容库');tap('词库');tap('GRE 难词试学')
        tap('模式');tap('困难');tap('使用困难模式')
        for _ in range(4):tap('下一个词')
        tap('开始测试')
        vocabulary=json.loads((folder/'packs/gre-trial/1.0.0/content.json').read_bytes())['words']
        while not device.page['keyboard']:
            group=restore(folder)['session']
            question=next(q for q in group['tasks'] if q['uid']==group['current'])
            meaning=vocabulary[question['id']]['meaning'].split(' ',1)[1]
            tap(meaning);tap('开始想英文' if '开始想英文' in device.buttons else '下一题')
        device.command('key 97');device.command('key 98')
        pending_title=device.page['title']
        device.command('home')
        before=restore(folder)
        assert before['session']['draft']['value']=='ab'
        tap('已学单词');tap('下一页');tap('上一页')
        tap('只看当前词库')
        search('ABST')
        tap('abstruse');tap('词根卡');tap('返回词卡')
        tap('重练这个词');tap('深奥难懂的')
        assert device.page['title']=='答对了',device.page
        tap('开始想英文')
        assert device.page['keyboard']
        for c in 'abstruse':device.command('key '+str(ord(c)))
        device.command('key 13')
        assert device.page['title']=='答对了'
        tap('查看小结')
        assert device.page['title']=='重练完成'
        tap('返回词卡');tap('返回已学单词');tap('清除搜索')
        tap('重练本页 5 词')
        assert '/5' in device.page['title'],device.page
        tap('不认识')
        assert device.page['title']=='再记一遍'
        tap('返回已学单词')
        assert restore(folder)==before, 'history or practice changed the saved learning state'
        device.command('home');tap('继续学习')
        assert device.page['title']==pending_title
        device.command('home')
        tap(next(k for k in device.buttons if k.startswith('我的收集册')))
        tap(next(k for k in device.buttons if '完成印章' in k))
        tap('backpack');tap('下一个词');assert device.page['title']=='重学单词 2 / 5'
        tap('上一个词');tap('返回印章');tap('重练这组词')
        assert '/5' in device.page['title'],device.page
        device.command('home');tap('复习');tap('已学单词')
        search('zzzz');tap('清除搜索')
        print('PASS learned history from home and review, native PET group stamp with exact word IDs, search, pages, saved FSRS and group, full and five-word practice')
    finally:
        device.close()
    device=Device(args.exe,folder)
    try:
        tap('已学单词');search('abstr');tap('abstruse')
        assert device.page['title']=='重学单词 1 / 1'
        print('PASS historical words without stamps survive restart')
    finally:device.close()


if __name__=='__main__':
    p=argparse.ArgumentParser()
    for key in ('exe','library','catalog'):p.add_argument('--'+key,type=Path,required=True)
    args=p.parse_args()
    with TemporaryDirectory(dir=ROOT/'work') as tmp:exercise(args,Path(tmp))

