"""Course word assessment and global review share the production word-sense record."""
import argparse
import json
import runpy
import time
from pathlib import Path
from tempfile import TemporaryDirectory

support=runpy.run_path(str(Path(__file__).with_name('test-shared-vocabulary.py')))


def test(args,folder):
    support['prepare'](folder,args)
    device=support['Device'](args.exe,folder)
    state=lambda:support['restore'](folder)
    epoch=int(time.time()*1000)+60000
    try:
        device.command(f'clock {epoch}')
        device.tap(next(k for k in device.buttons if k in ('简单模式','普通模式','困难模式')))
        device.tap('简单');device.tap('使用简单模式')
        device.command('courses')
        for label in ('第 1 单元 · 认识与交流','1A 说明来自哪里与住在哪里','分组背本课词汇 · 含拼写','下一个词','下一个词','学完本组，开始检查'):
            device.tap(label)
        before=state()['courseCursor']['batch']
        local=before['queue'][before['at']]['id']
        sense=json.loads((args.catalog/'catalog.json').read_bytes())['legacyMap'][local]
        device.tap('不会，看解释')
        data=state()
        first=data['cards'][sense]['meaning']
        assert first['reps']==1 and first['lapses']==1 and first['due']==epoch+900000
        cursor=data['courseCursor']
        assert 'memory' not in cursor and 'batchMemory' not in cursor
        device.command('home')
        device.command(f'clock {first["due"]+1}')
        device.tap('复习')
        if '查看全部词库' in device.buttons:device.tap('查看全部词库')
        device.tap('开始复习')
        assert state()['session']['ids']==[sense]
        meaning=next(r.split('\t')[2] for r in (args.catalog/'catalog.tsv').read_text('utf8').splitlines() if r.startswith(sense+'\t'))
        device.tap(meaning)
        after=state()['cards'][sense]['meaning']
        assert after['reps']==2 and after['due']==after['fsrsDue']
        device.command('courses')
        device.proc.stdin.write('course-cursor\n');device.proc.stdin.flush()
        projected=json.loads(device.proc.stdout.readline())
        assert projected['batchMemory'][local]['meaning']['due']==after['due']
        assert projected['memory'][local]['due']==after['due']
        assert projected['batch']==cursor['batch']
        print(f'PASS course {local} -> native all-deck review -> course: same sense, due, paused assessment and two observations')
    finally:device.close()


if __name__=='__main__':
    p=argparse.ArgumentParser()
    for key in ('exe','library','catalog'):p.add_argument('--'+key,type=Path,required=True)
    with TemporaryDirectory(dir=support['ROOT']/'work') as tmp:test(p.parse_args(),Path(tmp))

