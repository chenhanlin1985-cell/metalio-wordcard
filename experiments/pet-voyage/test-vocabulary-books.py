"""Finish the other two vocabulary books with the production touch controls and store."""
import argparse
import json
from pathlib import Path
import runpy
import shutil
from tempfile import TemporaryDirectory

shared=runpy.run_path(str(Path(__file__).with_name('test-shared-vocabulary.py')))


def test(args,folder,name):
    shared['prepare'](folder,args)
    source=args.library/name/'1.1.0'
    dest=folder/'packs'/name/'1.1.0'
    shutil.copytree(source,dest)
    title=json.loads((source/'manifest.json').read_bytes())['title']
    chapters=json.loads((source/'content.json').read_bytes())['chapters']
    words=[w for c in chapters for w in c['words']]
    d=shared['Device'](args.exe,folder)
    def run():
        state=shared['restore'](folder)
        return next(iter(state['storyRuns'].values()))['run']
    try:
        d.tap('内容库');d.tap(title);d.tap('看绘本');d.tap('下一页');d.tap('中文');d.tap('回到学习')
        d.tap('开始分段练')
        answers=[]
        for _ in range(360):
            p=run()['segments'];stage=p['stage']
            if stage=='done':break
            if stage=='read':d.tap('练这四词');continue
            if stage=='think':d.tap('想好了');continue
            if stage=='feedback':d.tap('继续');continue
            assert stage=='choose',p
            wid,kind=p['task'];row=words[wid]
            assert wid not in answers[-3:]
            opts=[k for k in d.buttons if k[:1] in 'ABCD' and k[1:3]=='  ']
            assert {k[3:] for k in opts}=={row['gloss'],*row['distractors']},(row,opts)
            gloss=row['distractors'][0] if not answers else row['gloss']
            d.tap(next(k for k in opts if k[3:]==gloss))
            if not answers:
                assert d.page['title']=='订正词义'
                bookmark=run();d.close();d=shared['Device'](args.exe,folder)
                d.tap('继续学习');assert run()==bookmark and d.page['title']=='订正词义'
            answers.append(wid)
        else:raise AssertionError('unbounded practice')
        result=run()
        assert result['complete'] and len(result['segments']['first'])==len(result['segments']['second'])==len(words)
        assert result['segments']['retries']['0']['correct']
        assert len(shared['restore'](folder)['cards'])==len(words)
        print('PASS',title,len(chapters),'sections',len(words),'native word cards, illustrated pages, cold restart, spaced correction')
    finally:d.close()


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    for key in ('exe','library','catalog'):parser.add_argument('--'+key,type=Path,required=True)
    args=parser.parse_args()
    with TemporaryDirectory(dir=shared['ROOT']/'work') as temp:
        for name in ('stone-soup','wrong-platform'):test(args,Path(temp)/name,name)
