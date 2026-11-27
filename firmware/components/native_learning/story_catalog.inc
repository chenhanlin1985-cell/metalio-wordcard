// Exact triples reviewed against the published examples. A changed gloss needs a new decision.
static bool reviewed_story_gloss(const std::string &word,const std::string &story,const std::string &deck) {
    static const char *const aliases[][3]={
        {"announcement", "n. 广播通知", "n. 通告"},
        {"appointment", "n. 事先约好的会面", "n. 约会"},
        {"boat", "n. 船；小船", "n. 船"},
        {"bridge", "n. 桥；此处指天桥", "n. 桥"},
        {"change", "n. 找零；找回的钱", "n. 找零"},
        {"coat", "n. 大衣；外套", "n. 外套"},
        {"delay", "n. 延误；晚点", "n. 延误"},
        {"drawing", "n. 图画", "n. 画"},
        {"face", "n. 脸，面孔", "n. 脸"},
        {"fare", "n. 车费；票价", "n. 票价"},
        {"fire", "n. 火；炉火", "n. 火"},
        {"label", "n. 衣物标签", "n. 标签"},
        {"light", "n. 光，光亮", "n. 光"},
        {"on time", "短语　准时，按约定时间", "phrase. 准时"},
        {"price", "n. 价格；标价", "n. 价格"},
        {"purse", "n. 女式小钱包", "n. 钱包"},
        {"receipt", "n. 收据；购物小票", "n. 收据"},
        {"restaurant", "n. 餐馆", "n. 餐厅"},
        {"return", "v. 归还", "v. 退还"},
        {"size", "n. 尺码；大小", "n. 尺寸"},
        {"slice", "n. 薄片", "n. 片"},
        {"ticket", "n. 票；车票", "n. 票"},
        {"watch", "n. 表，随身的报时工具", "n. 手表"},
        {"window", "n. 窗；此处指车窗", "n. 窗户"},
    };
    for(const auto &a:aliases)if(word==a[0]&&story==a[1]&&deck==a[2])return true;
    return false;
}
// Loaded on opening a story. An edition and its required deck form one binding.
void VocabularyCatalog::open_story(const std::string &key,const std::string &digest,const std::string &deckKey,const std::string &deckDigest) {
    const auto deck=decks.find(deckKey);
    require(deck!=decks.end()&&deck->second.signature==deckDigest,"故事所需的词库版本不匹配");
    const std::string edition=key+"/"+digest+"/"+deckKey+"/"+deckDigest;
    if(story.edition==edition)return;
    const auto path=book_path(key);
    require(!path.empty()&&digest.size()==64,"故事版本无效");
    const auto root=directory.substr(0,directory.find_last_of("/\\"))+"/packs/"+path+"/";
    const auto m=deck_json(root+"manifest.json");
    require(str(m,"id")+"@"+str(m,"version")==key&&str(m,"type")=="story"&&str(m,"requires")==deckKey&&
            str(field(m,"files"),"content.json")==digest,"故事目录与清单不符");
    const auto book=deck_json(root+"content.json",digest);
    require(book.is_object(),"故事内容校验失败");
    StoryBinding next;
    next.edition=edition;
    auto bind=[&](const std::string &local,const std::string &word,const std::string &meaning){
        auto mapping=deck->second.senses.find(local);
        require(mapping!=deck->second.senses.end(),"故事引用了未知词条");
        const auto *e=entry(mapping->second);
        require(e&&e->word==word&&!meaning.empty(),"故事词条与词库不符");
        std::string id=e->id;
        const auto gloss=e->pos.empty()?e->meaning:e->pos+" "+e->meaning;
        if(gloss!=meaning&&!reviewed_story_gloss(word,meaning,gloss)){
            id="story:"+edition+"/"+local;
            const auto space=meaning.find(' ');
            const bool pos=space>0&&space<14&&meaning[space-1]=='.';
            VJ item={{"id",id},{"word",word},{"meaning",pos?meaning.substr(space+1):meaning},
                     {"pos",pos?meaning.substr(0,space):""},{"stage","story:"+key}};
            restore_deck_word(item);
        }
        next.senses[local]=id;
    };
    const auto type=str(m,"storyType");
    require(num(m,"schema")==3&&(type=="vocabulary"||type=="comprehension"||type=="adventure"),"请安装标准格式的故事包");
    next.grades_words=type!="comprehension";
    if(type!="adventure"){
        for(const auto &t:field(book,"terms").items()){
            const auto local=str(t.value(),"id");
            if(!local.empty())bind(local,t.key(),str(t.value(),"meaning"));
        }
    }else{
        next.uses_deck=true;
        std::map<std::string,std::string> declared;
        for(const auto &chapter:field(book,"chapters"))for(const auto &term:field(chapter,"words")){
            const auto local=str(term,"id"),meaning=str(term,"meaning");
            if(meaning.empty())continue;
            require(!declared.count(local)||declared.at(local)==meaning,"同一故事词条引用了不同词义");
            declared[local]=meaning;
            const auto it=deck->second.senses.find(local);
            require(it!=deck->second.senses.end(),"故事引用了未知词条");
            const std::string word=entry(it->second)->word;
            bind(local,word,meaning);
        }
    }
    story=std::move(next);
}
