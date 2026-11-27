# 英语构词数据

词库、教材和故事共用 `word-formation.tsv`。每一行是一个词形和按阅读顺序列出的部件，`=` 表示在教学中保留整词。两端的短横线标明词缀的位置，`~` 标出不能独立成词的词根。原卡并列写法或带缩写时，`>` 指向卡片上实际讲解的词形。两个可以独立成词的部件合成一个词时，词卡逐个标为“组成词”。含空格和连字符的项目按完整词条查表。

`word-parts.tsv` 给出每个部件的中文释义。`@` 后的标识区分同形部件的不同用法，例如否定的 im- 和表示向内的 im-。固件只保存实际用到的部件、顺序和释义。

派生关系的初始候选来自 Khuyagbaatar Batsuren、Gábor Bella 和 Fausto Giunchiglia 的 MorphyNet 英语 v1。本项目对照词库词义筛选、补订了构词式，加入复合词、常见词根、部件中文义和整词决定。构词表、释义表及其编译索引采用 [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)。[MorphyNet 数据和论文](https://github.com/kbatsuren/MorphyNet)

部分词基中文释义依据本项目已有词库及其 ECDICT 资料整理。ECDICT 的许可见同目录的 [ECDICT 许可](word-parts-ECDICT-LICENSE.txt)。

修改两张表后运行 `python tools/build_word_formation.py`。运行 `python tools/build_word_formation.py --check` 可比较固件索引与源表。用 `python tools/audit_word_formation.py --library <内容包目录> --decks <CSV词库目录> --inventory <inventory.json> --csv <逐词清单.csv>` 可从学习系统的实际词条、故事依赖与候选词表核对覆盖范围。此清单按拼写去重，每个来源使用同一份构词内容。学习日期和练习结果仍由各自的词义记录管理。
