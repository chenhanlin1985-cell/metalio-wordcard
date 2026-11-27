"""Checks shared by the story pack authoring and installation tools."""
import re

TYPES = ('vocabulary', 'comprehension', 'adventure')
MARK = re.compile(r'\[\[([^\[\]]+)\]\]')


def check_reader(kind, book, frames, text, need):
    terms = book.get('terms', {})
    identifiers = set()
    for word, term in terms.items():
        need(re.fullmatch(r"[a-z]+(?:[ '-][a-z]+)*", word) and text(term.get('meaning')), '随文词义错误 '+word)
        identifier = term.get('id')
        if identifier is not None:
            need(isinstance(identifier, str) and re.fullmatch(r'[A-Za-z0-9:_-]{1,128}', identifier)
                 and identifier not in identifiers, '词条编号重复或无效')
            identifiers.add(identifier)
    seen = set()
    def marked(line):
        words = [m.lower() for m in MARK.findall(line)]
        need(line.count('[[') == line.count(']]') == len(words) and all(w in terms for w in words), '随文词义标注无效')
        seen.update(words)
        return words

    def page(p):
        need(text(p.get('en')) and text(p.get('zh')), '正文或译文缺失')
        if 'image' in p:
            need(type(p['image']) is int and 0 <= p['image'] < frames, '配图超出范围')
        marked(p['en'])

    targets = set()
    for c in book['chapters']:
        need(text(c.get('title')), '章节需要标题')
        if kind == 'vocabulary':
            need(text(c.get('en')) and text(c.get('zh')) and len(c['words']) == 4, '每段需要双语短文和四个词')
            marked_words = set(marked(c['en']))
            chapter_terms = [w['term'] for w in c['words']]
            need(len(set(chapter_terms)) == 4 and set(chapter_terms) == marked_words, '目标词必须出现在本段标注中')
            for w in c['words']:
                word = w['term']
                need(word not in targets and terms[word].get('id'), '目标重复或缺少词库编号 '+word)
                labels = [w['gloss'], *w['distractors']]
                need(len(labels) == len(set(labels)) == 4 and all(text(v) and re.search('[一-龥]',v) for v in labels),
                     '需要四个不同的中文词义选项 '+word)
                targets.add(word)
        else:
            need(1 <= len(c['pages']) <= 100 and 1 <= len(c['questions']) <= 20, '每幕需要正文和理解题')
            for p in c['pages']:
                page(p)
            for q in c['questions']:
                opts = q['options']
                need(all(text(q.get(k)) for k in ('en','zh','reason')) and 2 <= len(opts) <= 4
                     and all(map(text,opts)) and len(set(opts)) == len(opts) and type(q['answer']) is int
                     and 0 <= q['answer'] < len(opts) and type(q['evidence']) is int
                     and 0 <= q['evidence'] < len(c['pages']), '理解题的选项、依据页或解析无效')
    if 'album' in book:
        need(kind=='vocabulary' and 1 <= len(book['album']) <= 100, '原绘本页超出范围')
        for p in book['album']:
            page(p)
    need(seen == set(terms), '随文词义表有未出现的词')
    if kind=='vocabulary':
        need(targets == {k for k,t in terms.items() if t.get('id')}, '目标词与分段练习不一致')
