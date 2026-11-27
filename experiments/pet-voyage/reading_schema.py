"""Validate a book whose reading passages precede its comprehension questions."""
import re


def check_reading(m, book, text, need, frames=0):
    files={'content.json', book['art']} if frames else {'content.json'}
    need(book.get('style') == 'read-first' and set(m['files']) == files and bool(book.get('art')) == bool(frames), '伴读包内容不完整')
    for k in ('author', 'englishTitle', 'attribution', 'intro', 'source'):
        need(text(book.get(k)), '伴读包缺少 ' + k)
    need(book['source'].startswith('https://'), '原著链接需要使用 HTTPS')
    terms = book['terms']
    used, ids = set(), set()
    for key, t in terms.items():
        need(re.fullmatch(r"[a-z]+(?:[ '-][a-z]+)*", key) and text(t.get('meaning')), '随文释义错误')
        if 'id' in t:
            need(re.fullmatch(r'[A-Za-z0-9:_-]{1,128}', t['id']) and t['id'] not in ids, '复习词编号重复或无效')
            need(text(t.get('example')) and key in t['example'].lower(), '复习例句缺词 ' + key)
            opts = t['distractors']
            need(len(opts) == 2 and all(map(text, opts)) and len(set([t['meaning'], *opts])) == 3, '复习选项不完整')
            ids.add(t['id'])
    chapters = book['chapters']
    need(1 <= len(chapters) <= 500, '幕数错误')
    for c in chapters:
        need(text(c.get('title')) and 1 <= len(c['pages']) <= 100 and (0 if frames else 1) <= len(c['questions']) <= 20, '幕不完整')
        if frames:
            order=c['recallOrder']
            need(len(order)==len(set(order)) and all(k in terms and terms[k].get('id') and text(terms[k].get('cue')) for k in order),'绘本回想顺序有误')
            need(set(order)=={k.lower() for p in c['pages'] for k in re.findall(r'\[\[([^\[\]]+)\]\]',p['en']) if terms.get(k.lower(),{}).get('id')}, '绘本主练词和正文不符')
        for p in c['pages']:
            if frames:
                need(type(p.get('image')) is int and 0 <= p['image'] < frames,'绘本缺图')
            need(text(p.get('en')) and text(p.get('zh')), '缺少正文或译文')
            tags = [k.lower() for k in re.findall(r'\[\[([^\[\]]+)\]\]', p['en'])]
            need(p['en'].count('[[') == p['en'].count(']]') == len(tags) and all(t in terms for t in tags), '正文词义标注无效')
            used.update(tags)
        for q in c['questions']:
            opts = q['options']
            need(all(text(q.get(k)) for k in ('en','zh','reason')) and 2 <= len(opts) <= 4 and
                 all(map(text, opts)) and len(set(opts)) == len(opts) and type(q['answer']) is int and
                 0 <= q['answer'] < len(opts) and type(q['evidence']) is int and 0 <= q['evidence'] < len(c['pages']), '幕末题或原文位置错误')
    need(used == set(terms) and ids, '存在未使用的释义或缺少复习词')
