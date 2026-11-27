static const std::pair<const char*,const char*> pet_senses[]={
    {"pet-0005","vocab:37ecf521c8f469c92ce4:sense-1"},
    {"pet-0006","vocab:72f7c894ad6c4ce2ef54:sense-1"},
    {"pet-0010","vocab:d531950f9771bdc4a0ef:sense-1"},
    {"pet-0014","vocab:5a324108508a69447776:sense-1"},
    {"pet-0016","vocab:6f595eba3f46ceaa9fd0:sense-1"},
    {"pet-0018","vocab:4509beb0ab401d71fa4a:sense-1"},
    {"pet-0019","vocab:18e1dd392168f601d0e5:sense-1"},
    {"pet-0020","vocab:4566a3eca61559aff973:sense-1"},
    {"pet-0021","vocab:f11e2c668e8e1dcb1331:sense-1"},
    {"pet-0028","vocab:b6958aab121d91a7aeff:sense-1"},
    {"pet-0038","vocab:5c4d3352b8ac943f6e31:sense-1"},
    {"pet-0039","vocab:5c5f9b87778cfb103aa0:sense-1"},
    {"pet-0041","vocab:a52514252adf86066966:sense-1"},
    {"pet-0043","vocab:8218f3d6780abf45a5a4:sense-1"},
    {"pet-0044","vocab:1b56e72e957c16786a0c:sense-1"},
    {"pet-0047","vocab:8a46854926e5416bae46:sense-1"},
    {"pet-0052","vocab:f2c819b6790de306847f:sense-1"},
    {"pet-0054","vocab:5b612edb571c479d1a33:sense-1"},
    {"pet-0058","vocab:53368bd169100f64249b:sense-1"},
    {"pet-0061","vocab:20d1c99b819b2019da6e:sense-1"},
    {"pet-0062","vocab:18159f6fa757e618ab6e:sense-1"},
    {"pet-0070","vocab:731f46507cc5394513e3:sense-1"},
};
// The catalog owns word content and membership. Session and memory live in VocabularyEngine.
static bool matches_deck_digest(const std::string &bytes,const std::string &expected){
    unsigned char digest[32]{};
    const auto *raw=reinterpret_cast<const unsigned char*>(bytes.data());
#ifdef ESP_PLATFORM
    size_t length=0;
    if(psa_crypto_init()!=PSA_SUCCESS||psa_hash_compute(PSA_ALG_SHA_256,raw,bytes.size(),digest,sizeof(digest),&length)!=PSA_SUCCESS||length!=sizeof(digest))return false;
#elif defined(_WIN32)
    if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,const_cast<PUCHAR>(raw),ULONG(bytes.size()),digest,sizeof(digest))<0)return false;
#else
    if(!SHA256(raw,bytes.size(),digest))return false;
#endif
    const char digits[]="0123456789abcdef";std::string hex;hex.reserve(64);
    for(auto c:digest){hex+=digits[c>>4];hex+=digits[c&15];}
    return hex==expected;
}
static VJ deck_json(const std::string &path,const std::string &digest={}) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f || f.tellg()<=0 || f.tellg()>2*1024*1024)return nullptr;
    const auto size=static_cast<size_t>(f.tellg());f.seekg(0);
    if(digest.empty())return VJ::parse(f,nullptr,false);
    std::string bytes(size,'\0');
    if(!f.read(bytes.data(),size)||!matches_deck_digest(bytes,digest))return nullptr;
    return VJ::parse(bytes,nullptr,false);
}
VJ VocabularyCatalog::deck_word(const std::string &id) const {
    const auto i=deck_words_.find(id);
    return i==deck_words_.end()?VJ(nullptr):VJ::parse(i->second);
}
bool VocabularyCatalog::contains_word(const std::string &stage,const std::string &id) const {
    auto it=decks.find(stage);
    if(it!=decks.end())return it->second.members.count(id)!=0;
    const auto *e=entry(id);
    return e&&e->stage==stage;
}
void VocabularyCatalog::restore_deck_word(const VJ &w) {
    const auto id=str(w,"id");
    if(entry(id))return;
    index_[id]=entries.size();
    entries.push_back({id,str(w,"word"),str(w,"meaning"),str(w,"pos"),str(w,"stage"),true,0,0});
    deck_words_.emplace(id,w.dump());
}
void VocabularyCatalog::load_decks(const std::string &root) {
    decks.clear();deck_words_.clear();
    for(const auto &name:book_directories(root))for(const auto &ver:book_directories(root+"/"+name)){
        const auto key=name+"@"+ver,path=book_path(key);if(path.empty())continue;
        const auto dir=root+"/"+path+"/";
        const auto m=deck_json(dir+"manifest.json");
        if(str(m,"type")!="deck"||str(m,"format")!="wordcard-pack"||num(m,"schema")!=1||str(m,"id")!=name||str(m,"version")!=ver)continue;
        const auto sig=str(field(m,"files"),"content.json");
        if(sig.size()!=64||sig.find_first_not_of("0123456789abcdef")!=std::string::npos)continue;
        auto data=deck_json(dir+"content.json",sig);
        const auto &items=field(data,"words");
        if(!items.is_object()||items.empty()||items.size()>12000)continue;
        bool valid=true;
        for(const auto &item:items.items())if(item.key().empty()||item.key().size()>128||str(item.value(),"word").empty()||str(item.value(),"meaning").empty()){valid=false;break;}
        if(!valid)continue;
        const bool knownGre=key=="gre-trial@1.0.0"&&sig=="1918b4647d7c16bb73afc1b8e87e142a9697186588ea2334c08be7e2336bbfa4";
        bool greMirror=knownGre&&items.size()==140;
        if(greMirror)for(const auto &item:items.items()){
            const auto *w=entry(item.key());
            if(!w||w->stage!="gre-trial"||w->word!=str(item.value(),"word")||w->pos+" "+w->meaning!=str(item.value(),"meaning")){greMirror=false;break;}
        }
        auto &d=decks[key];d.stage=greMirror?"gre-trial":key;d.title=str(m,"title");d.signature=sig;
        for(const auto &item:items.items()){
            const auto &src=item.value();const auto word=str(src,"word"),gloss=str(src,"meaning");
            const auto declared=str(src,"senseId");
            const auto canonical=declared.empty()?item.key():declared;
            const auto *same=(!declared.empty()||greMirror)?entry(canonical):nullptr;
            const auto published="sense:"+std::to_string(declared.size())+"/"+declared+"/"+std::to_string(word.size())+"/"+word+"/"+gloss;
            if(!declared.empty()&&(!same||same->word!=word||(same->pos.empty()?same->meaning:same->pos+" "+same->meaning)!=gloss))same=entry(published);
            std::string id;
            if(same&&same->word==word&&(same->pos.empty()?same->meaning:same->pos+" "+same->meaning)==gloss)id=same->id;
            if(id.empty()&&key=="pet-2024@1.0.0"){
                for(const auto &link:pet_senses)if(item.key()==link.first){
                    const auto *meaning=entry(link.second);
                    if(meaning&&meaning->word==word&&meaning->pos+" "+meaning->meaning==gloss)id=meaning->id;
                    break;
                }
            }
            if(id.empty()){
                id=!same&&!declared.empty()&&declared.size()<=128?published:"deck:"+key+"/"+sig+"/"+item.key();
                const auto space=gloss.find(' ');
                const bool pos=space>0&&space<14&&gloss[space-1]=='.';
                VJ w={{"id",id},{"stage",d.stage},{"word",word},{"meaning",pos?gloss.substr(space+1):gloss},{"pos",pos?gloss.substr(0,space):""},{"ipa",str(src,"ipa")},{"example",str(src,"example")},{"exampleZh",str(src,"exampleZh")},{"topic",str(src,"topic")}};
                if(field(src,"morphology").is_object())w["morphology"]=src.at("morphology");
                auto audio=str(src,"audio");
                if(audio.rfind("audio/",0)==0&&audio.find("..")==std::string::npos&&audio.find_first_of("\\:")==std::string::npos&&field(m,"files").contains(audio))
                    w["audio"]={{"path","packs/"+path+"/"+audio},{"text",word}};
                restore_deck_word(w);
            }
            d.senses.emplace(item.key(),id);
            if(d.members.insert(id).second)d.ids.push_back(id);
        }
        if(!greMirror)metadata["stages"].push_back({{"id",key},{"title",d.title},{"ready",d.ids.size()}});
    }
}
