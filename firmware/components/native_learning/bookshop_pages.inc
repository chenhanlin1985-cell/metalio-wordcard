const auto &shop=field(state_,"bookshop");
const int shopGroup=int(num(shop,"groups")),scene=shop_scene(state_);
const auto shopAction=[](const std::string &act,const std::string &value=""){return VJ{{"action",act},{"value",value}};};
auto shopTabs=[&](const std::string &selected){
    v.tabs(selected);
    p.tabs[1].action=v.action(shopAction("v-go","shop-library"));
    p.tabs[2].action=v.action(shopAction("v-go","shop-review"));
};
auto shopArt=[&](int tile,bool thumb=false){
    WebBlock art;art.kind="text";art.area="body";art.role=thumb?"shop-thumb":"shop-scene";
    art.picture=tile+(thumb?3:0);art.picture_source=catalog_.directory+"/bookshop.wpix";
    art.picture_width=thumb?192:384;art.picture_height=thumb?96:192;p.blocks.push_back(std::move(art));
};
if(route_=="shop-prompt"&&!task.is_null()){p.route="word-result";p.title="完整题干";v.top("返回",shopAction("v-back"));v.text(shop_cue(catalog_,str(task,"id"),str(ss,"shopRole")=="visit"?scene:flag(task,"followup")?1:0),"question");v.button("回原题",shopAction("v-back"),true,"foot");return p;}
if(route_=="bookshop"){
    p.route="home";p.title=shop_title(scene);p.key+=":"+n(shopGroup)+":"+n(scene);
    const bool ongoing=!engine_.resumable(state_,"bookshop").is_null();
    const auto waiting=engine_.shop_due(state_,now_).size();
    const bool visit=shop_visit(state_,now_);
    v.top("设置",shopAction("v-go","settings"));shopTabs("home");
    v.button(mode_name(str(state_["settings"],"mode"))+"模式",shopAction("v-go","modes"));p.blocks.back().role="vocab-home-mode";
    v.text(p.title,"vocab-home-stage",true);
    v.text(ongoing?"接着上次的委托":shopGroup>=2&&!scene?"两件委托已完成，可以布置书店。":visit?"昨天的客人带着便笺回来了。":waiting?"先温习柜台上的旧词。":shopGroup<2?"今日委托 · "+std::string(shop_name(shopGroup)):"今天已收店，明天再会。","vocab-home-next");
    p.home_new_target=10;
    for(const auto &id:field(today,"new")){
        const auto *entry=catalog_.entry(str(id));
        if(entry&&entry->stage=="gre-trial")++p.home_new_words;
    }
    v.text("待复习 "+n(waiting)+" 项","vocab-home-history");shopArt(scene,true);
    const auto stamps=album_stamps();v.button("我的收集册 · "+n(stamps.size())+" 枚",shopAction("v-go","collection"));p.blocks.back().role="vocab-home-collection";
    const size_t start=stamps.empty()?0:(stamps.size()-1)/6*6;
    for(size_t i=start;i<start+6;i++){v.text(n(i+1),"vocab-home-stamp");p.blocks.back().selected=i<stamps.size();}
    v.text("GRE 难词试学 · 每件委托 5 词","vocab-home-stamp-note");
    v.button(ongoing?"继续上次的委托":shopGroup>=2&&!scene?"布置我的书店":visit?"接待回访客人":waiting?"开始到期复习":shopGroup<2?"开始今天的委托":"看看我的书店",shopAction("v-shop-start"),true,"foot");return p;
}
if(route_=="shop-library"||route_=="shop-review"||route_=="shop-first"||route_=="shop-second"){
    p.route="word-result";v.top("返回",shopAction("v-back"));
    if(route_=="shop-library"){
        p.title="本章词库";v.text("GRE 难词试学","",true);
        v.text("每件委托 5 个词，先认词，再用店里的情境回忆。");
        v.button(shop_name(0),shopAction("v-go","shop-first"));
        v.button(shop_name(1),shopAction("v-go","shop-second"));
        shopTabs("levels");
    }else if(route_=="shop-review"){
        p.title="书店复习";const auto dueCount=engine_.shop_due(state_,now_).size();
        const bool active=!engine_.resumable(state_,"bookshop").is_null();
        v.text("GRE 难词试学","",true);v.text("当前模式到期 "+n(dueCount)+" 项。","meaning");
        if(active||dueCount)v.button(active?"继续上次的委托":"开始到期复习",shopAction("v-shop-start"),true,"foot");
        shopTabs("review");
    }else{
        const int index=route_=="shop-first"?0:1;p.title=shop_name(index);
        v.text(index==0?"帮老读者找回一本地图册。":"帮邻居救回雨水打湿的旧书。","meaning");
        v.text(std::string("5 个词 · ")+(index<shopGroup?"已完成":index==shopGroup?"当前委托":"完成上一件委托后开启"),"source");
    }
    v.button("回到书店",shopAction("v-shop-home"),false,"foot");return p;
}
if(route_=="shop-choice"||route_=="shop-counter"||route_=="shop-visit"||route_=="shop-demo"||route_=="shop-postcard"){
    p.route="word-result";v.top("返回",shopAction("v-back"));
    if(route_=="shop-choice"||route_=="shop-demo"){
        const bool demo=route_=="shop-demo";p.title=demo?"预演隔日回访":"布置书店";
        shopArt(scene);v.text(demo?"在独立预演中接待一位客人，今天的复习日期保持原样。":"窗边空出一角。铺上软垫，可以和读者聊书。摆上装订线，邻居就会送旧书来修。");
        for(const auto &branch:{"reading","repair"})v.button(shop_title(branch==std::string("reading")?1:2),shopAction(demo?"v-shop-demo":"v-shop-decor",branch),false,"foot");
    }else if(route_=="shop-counter"){
        p.title=shop_title(scene);shopArt(scene);v.text(scene==1?"猫在软垫上团成一圈。读者留了张便笺，约好下次再聊。":"猫用爪子压住线头。邻居说下次还要带一本旧书来。");
        v.button("预演隔日回访",shopAction("v-go","shop-demo"),false,"foot");
        v.button("回到书店",shopAction("v-shop-home"),true,"foot");
    }else if(route_=="shop-visit"){
        p.title=scene==1?"窗边的老读者":"送书来修的邻居";shopArt(scene);
        v.text(scene==1?"熟客又坐回窗边，翻开夹在书里的纸条。":"邻居带来一本线松了的旧书，放在工作台上。");
        v.text(engine_.shop_due(state_,now_).empty()?"暂时没有到期词，读一张便笺热身。":"换个情境，看看还能想起哪些到期的词。","source");
        v.button("读客人的便笺",shopAction("v-shop-visit"),true,"foot");
    }else{
        const int variant=shopDemoScene_?shopDemoScene_:scene;
        p.title=shopDemoScene_?"回访预演 · 一张便笺":"老客人的便笺";
        const auto *entry=catalog_.entry(shopPostcardWord_);
        if(!entry){v.button("回到书店",shopAction("v-shop-home"),true,"foot");return p;}
        if(shopPostcardAnswer_.empty()){
            v.text(shop_cue(catalog_,entry->id,variant),"question");
            VJ sample{{"id","postcard-"+n(variant)},{"shopRole","postcard"}};
            for(const auto &opt:engine_.choices(VJ{{"id",entry->id},{"uid","postcard-"+entry->id}},sample))v.button(str(opt),shopAction("v-shop-pick",str(opt)),false,"foot");
            v.button("想不起来",shopAction("v-shop-pick","?"),false,"foot");
        }else{
            v.text(shopPostcardAnswer_==entry->word?"答对了":"再认一次","",true);
            v.text(entry->word,"card-word");v.text(entry->meaning,"meaning");
            v.text("这张热身便笺不计入 FSRS。","source");
            v.button(shopDemoScene_?"返回今天":"回到书店",shopAction("v-shop-home"),true,"foot");
        }
    }
    return p;
}
