// Lexical notes are matched by whole headword. Family membership is not a review identity.
struct RootCard {const char *words,*root,*connection,*family;};
static const RootCard root_cards[]={
    {"parsimonious parsimony parsimoniously","parsimon- · 吝啬、过分节省","把名词 parsimony 和形容词 parsimonious 放在一起记。一个人连请朋友喝杯水都舍不得，就很 parsimonious。","parsimony 吝啬 · parsimoniously 吝啬地"},
    {"inchoate inchoative","词源 · 开始","拉丁语 inchoare 表示开始。一个想法刚有轮廓，还说不清，就是 an inchoate idea。","inchoate 初步的 · inchoative 表示开始的"},
    {"placate placatory placation implacable","plac- · 平息、安抚","想象有人正在生气。你解释并补救，希望能 placate 对方。加上 im- 的 implacable 则形容难以安抚。","placatory 安抚性的 · implacable 难以平息的"},
    {"laconic laconically","地名词源 · 拉科尼亚","古希腊的斯巴达位于拉科尼亚。斯巴达人以说话简短著称。别人长篇解释，他只答一个词，这就是 a laconic reply。","laconic 简洁的 · laconically 简短地"},
    {"proclivity declivity acclivity","cliv- · 坡、倾斜","想象球自然滚向斜坡的一侧。把这个倾斜的方向联系到一个人做某事的倾向。","proclivity 倾向 · declivity 下坡"},
    {"ameliorate amelioration ameliorative","melior- · 更好","病房里太吵，换上安静的设备后，病人的处境得到了改善。ameliorate 常接 conditions 或 a situation。","amelioration 改善 · ameliorative 起改善作用的"},
    {"belligerent belligerence bellicose","bell- · 战争","看到交战双方可以想到 belligerents。用来形容说话的态度时，它表示带着敌意、随时准备吵起来。","bellicose 好战的 · belligerence 敌对态度"},
    {"impecunious pecuniary","pecun- · 钱","pecuniary 表示钱财方面的。im- 带有否定意味，impecunious 形容手头拮据。","pecuniary 金钱上的 · impecunious 囊中羞涩的"},
    {"juxtapose juxtaposition","juxta- · 紧邻","把新街景和同一街口的老照片并排放，方便比较，这个动作就是 juxtapose。","juxtapose 并置 · juxtaposition 并列"},
    {"reticent reticence tacit taciturn","tac / tic · 沉默","别人问起往事，他只点了点头。reticent 形容这种不愿多说的态度。","reticence 寡言 · tacit 默认的"},
    {"abstruse","trud / trus · 推","abs- 有推离的方向感。这个词的拉丁语祖词指藏起来，意思像藏得很深一样难懂。","intrude 闯入 · protrude 突出"},
    {"alacrity alacritous","alacr- · 活泼、热切","拉丁语 alacer 形容精力充沛。有人欣然接下任务，就可以说 with alacrity。","alacritous 欣然的 · alacrity 欣然、敏捷"},
    {"obdurate durable endure endurance","dur- · 硬、持久","用材料的坚硬去记 durable 的耐用；用态度的强硬去记 obdurate 的顽固。","durable 耐用的 · endure 忍受"},
    {"equivocate equivocal unequivocal","equ(i)- · 相等；voc- · 声音","一句话听起来有两种同样说得通的解释。用这个画面记住含糊其词。","equivocal 模棱两可的 · unequivocal 明确的"},
    {"ephemeral ephemera","hemer- · 日","希腊语词源描写只持续一天。想象一朵朝开暮谢的花。","ephemera 短暂存在的事物 · ephemerality 短暂"},
    {"circumspect inspect inspection spectator spectacle perspective retrospective prospect suspect","spec / spect · 看","先找与看的联系。circum- 是四周，做事前环顾四周，就有了谨慎的意思。","inspect 检查 · spectator 观众"},
    {"predict prediction dictate dictation contradict contradiction dictator verdict dictionary","dic / dict · 说","pre- 表示在先。事情发生前先说出来，就是预测；contra- 有相反的方向。","predict 预测 · contradict 反驳"},
    {"transport transportation portable export import porter","port · 搬运、携带","看搬运的方向。im- 表示向内，ex- 表示向外，trans- 表示越过。","import 进口 · export 出口"},
    {"describe description prescribe prescription inscribe inscription manuscript transcript script scribble","scrib / script · 写","同一个词根有两种常见拼法。可以把 prescribe 和 prescription 成对记。","prescribe 开处方 · inscription 铭文"},
    {"construct construction reconstruct obstruct obstruction structure instruction instructor instruct","struct · 建造、搭建","从搭建引申到结构。re- 表示再来一次，reconstruct 就是重建。","construct 建造 · structure 结构"},
    {"attract attraction attractive extract subtract distract distraction retract tractor traction","tract · 拉、牵引","看拉的方向。attract 把注意力拉过来，distract 把注意力拉开。","attract 吸引 · retract 收回"},
    {"introduce introduction produce production reduce reduction conduct conductor induce deduct","duc / duct · 引导、带领","在 conduct 里可以想到带领；conductor 可以指带领乐团演奏的指挥。","conduct 引导 · induce 引起"},
    {"audible inaudible audience audio audition auditorium auditory","aud · 听","audible 表示听得见。加上否定前缀 in-，就成了听不见的。","audience 听众 · audition 试音"},
    {"reject rejection inject injection eject project projector projection object objection","ject · 扔、投","把东西投出去或投进去。inject 向内注入，eject 向外弹出。","inject 注入 · eject 弹出"},
    {"rupture interrupt interruption disrupt disruption erupt eruption bankrupt","rupt · 破、断","想象一条连续的线被截断。interrupt 是打断，disrupt 是扰乱。","interrupt 打断 · erupt 喷发"},
    {"telephone microphone phonetic phonics symphony","phon · 声音","microphone 拾取声音，telephone 把声音传到远处。","microphone 麦克风 · phonetic 语音的"},
    {"geography geographic geology geologist geometry geothermal","geo · 地球、大地","geo- 说明谈的是大地。和研究、描写、测量结合，就形成不同的学科名。","geology 地质学 · geography 地理学"},
    {"photograph photography photographer photographic biography autobiography graphic autograph","graph · 写、记录","photo- 是光。photograph 好比用光记录图像，biography 则记录一个人的生平。","photograph 照片 · biography 传记"},
    {"submit submission transmit transmission permit permission admit admission dismiss missile mission","mit / miss · 送、派","把一份东西从一处送到另一处。transmit 是传送，submit 可以指提交。","transmit 传送 · submit 提交"},
    {"receive reception receipt recipient accept acceptance intercept","cap / cept / ceiv · 拿、收","从接到东西来记。receive 是收到；receipt 在购物场景里指收到款项的凭据。","receive 收到 · reception 接待"},
    {"reverse inverse convert conversion versatile divert diversion","vert / vers · 转","先记转变方向。reverse 是反转，divert 是转向别处。","reverse 反转 · convert 转变"},
    {"pedal pedestrian","ped · 脚","pedal 是脚踏板，pedestrian 是用脚走路的人。","pedal 脚踏板 · pedestrian 行人"},
    {"incredulous credible incredible credibility credulous","cred · 相信","credible 是可信的；credulous 形容轻易相信别人的人。","credible 可信的 · incredulous 怀疑的"},
    {"backpack handbag raincoat sweatshirt","复合构词 · 两个词干","先认出两个熟词，再把意思合起来。backpack 是背在背上的包，raincoat 是挡雨的外套。","hand + bag 手提包 · rain + coat 雨衣"},
};
static VJ word_root(const VJ &entry){
    const auto &note=field(entry,"morphology");
    if(note.is_object()&&!str(note,"root").empty()&&!str(note,"connection").empty()&&!str(note,"family").empty())return note;
    const auto word=str(entry,"word");
    if(word.empty()||word.find(' ')!=std::string::npos)return nullptr;
    for(const auto &r:root_cards){
        std::string_view list(r.words);size_t pos=0;
        while(pos<list.size()){
            auto end=list.find(' ',pos);
            if(list.substr(pos,end==std::string_view::npos?end:end-pos)==word)return VJ{{"root",r.root},{"connection",r.connection},{"family",r.family}};
            if(end==std::string_view::npos)break;
            pos=end+1;
        }
    }
    return nullptr;
}
