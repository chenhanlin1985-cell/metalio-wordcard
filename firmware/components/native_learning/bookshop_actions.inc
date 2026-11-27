if(name.rfind("v-shop-",0)==0){
    const auto &book=field(state_,"bookshop");const auto group=num(book,"groups");
    if(!catalog_.shopCues.is_object()){note_="请补齐书店 SD 资源";return true;}
    if(name=="v-shop-home"){shopDemoScene_=0;backStack_.clear();go("bookshop");return true;}
    if(name=="v-shop-decor"){
        if(route_=="shop-choice"&&commit(VJ{{"type","shopDecor"},{"value",value}})){backStack_.clear();go("shop-counter");}
        return true;
    }
    if(name=="v-shop-pick"){
        if(route_!="shop-postcard"||!shopPostcardAnswer_.empty())return true;
        if(!shopDemoScene_&&!commit(VJ{{"type","shopPostcard"}}))return true;
        shopPostcardAnswer_=value;++viewSerial_;return true;
    }
    if(name=="v-shop-demo"||name=="v-shop-visit"){
        if(name=="v-shop-demo"&&(route_!="shop-demo"||(value!="reading"&&value!="repair")))return true;
        if(name=="v-shop-visit"&&(route_!="shop-visit"||!shop_visit(state_,now_)))return true;
        if(name=="v-shop-visit"&&!engine_.shop_due(state_,now_).empty()){
            if(commit(VJ{{"type","shopBegin"}})) navigate("word-batch");
            return true;
        }
        const auto &ids=field(book,"ids");
        if(!ids.is_array()||ids.empty()){go("bookshop");return true;}
        shopDemoScene_=name=="v-shop-demo"?(value=="reading"?1:2):0;
        const int variant=shopDemoScene_?shopDemoScene_:shop_scene(state_);
        shopPostcardWord_=str(ids[size_t(variant-1)%ids.size()]);
        shopPostcardAnswer_.clear();navigate("shop-postcard");return true;
    }
    if(name=="v-shop-start"){
        if(engine_.resumable(state_,"bookshop").is_null()){
            if(group==2&&!shop_scene(state_)){navigate("shop-choice");return true;}
            if(shop_visit(state_,now_)){navigate("shop-visit");return true;}
            if(group==2&&engine_.shop_due(state_,now_).empty()){navigate("shop-counter");return true;}
        }
        if(commit(VJ{{"type","shopBegin"}})) navigate("word-batch");
        return true;
    }
    return true;
}
