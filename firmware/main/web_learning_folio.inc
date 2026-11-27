// Typography and hit regions for the vocabulary engine's learning surfaces.
const bool folioUI=vocabulary && (w.route=="levels" || w.route=="review" || w.route=="word-batch" ||
    w.route=="word-result" || w.route=="collection" || w.route=="gallery" || w.route=="stamp");

auto folioButton=[&](Rect r,Action action,const std::string &label,int value=0,bool primary=false,
                     bool enabled=true,int slot=136){
    const Rect hit=r.x==42 && r.w==600?Rect{24,r.y,636,r.h}:r;
    buttons_.push_back({hit,{action,value},label,enabled,primary});
    const bool wide=r.w>420;
    const int inset=wide?18:0;
    if(!wide&&r.y<112&&folio_line(c,0,0,label,slot,Canvas::Align::Left,false)>r.w-16)slot=28;
    const bool arrow=primary && wide && enabled;
    const int room=r.w-inset*2-(arrow?52:16);
    const bool fits=folio_line(c,0,0,label,slot,Canvas::Align::Left,false)<=room;
    if(!fits)slot=28;
    const auto shown=folio_line(c,0,0,label,slot,Canvas::Align::Left,false)<=room?label:fit_px(c,label,room,28);
    if(wide || r.y>=350)c.border(r,enabled?(primary?3:2):1);
    folio_line(c,wide?r.x+inset:r.x+r.w/2,r.y+r.h/2,shown,slot,
               wide?Canvas::Align::Left:Canvas::Align::Center);
    if(arrow)folio_arrow(c,r.x+r.w-27,r.y+r.h/2,10);
};
auto pageButton=[&](Rect r,Action a,const std::string &label,int value=0,bool primary=false,bool enabled=true){
    if(folioUI)folioButton(r,a,label,value,primary,enabled,primary?44:136);
    else button(c,r,a,label,value,primary,enabled);
};
auto bookLine=[&](int x,int cy,const std::string &name,int centeredRoom=0){
    const bool gre=name.rfind("GRE ",0)==0;
    const auto title=gre?name.substr(4):name;
    if(centeredRoom){
        const int width=folio_line(c,0,0,title,52,Canvas::Align::Left,false)+
                        (gre?13+folio_line(c,0,0,"GRE",40,Canvas::Align::Left,false):0);
        if(width>centeredRoom){
            folio_line(c,x,cy,fit_px(c,name,centeredRoom,28),28,Canvas::Align::Center);return;
        }
        x-=width/2;
    }
    if(gre)x+=folio_line(c,x,cy,"GRE",40)+13;
    folio_line(c,x,cy,title,52);
};
auto sectionHeader=[&]{
    const auto back=w.top.empty()?std::string("回首页"):w.top.front().text;
    folioButton({24,12,184,86},Action::Back,back);
    folioButton({512,12,148,86},Action::DeviceMenu,"设置");
    c.fill({42,110,600,2});
    folio_line(c,42,167,w.title,44);
};
if(folioUI && v.screen!=Screen::WebPrompt && status.empty() && w.error.empty() && w.route=="levels"){
    std::vector<const WebBlock*> rows;
    const WebBlock *apply=nullptr;
    for(const auto &b:w.blocks)if(b.kind=="button"){
        if(b.area=="foot")apply=&b;else rows.push_back(&b);
    }
    // Custom catalogs with more entries keep the regular paginated layout.
    bool titlesFit=true;
    for(const auto *b:rows){
        const auto split=b->text.rfind(" · ");
        if(split==std::string::npos)continue;
        const int bookWidth=c.text_width_px(b->text.substr(0,split),52);
        const int countWidth=folio_line(c,0,0,b->text.substr(split+std::string(" · ").size()),28,Canvas::Align::Left,false);
        if(bookWidth+countWidth+24>580)titlesFit=false;
    }
    if(apply && rows.size()<=7 && titlesFit){
        answer_pages_=1;sectionHeader();int y=219;
        for(const auto *b:rows){
            const Rect hit{24,y,636,100};
            buttons_.push_back({hit,{Action::WebAction,b->action},b->text,b->enabled,b->primary});
            const auto split=b->text.rfind(" · ");
            if(split!=std::string::npos){
                const auto title=b->text.substr(0,split);
                if(b->selected){c.fill({42,y+29,4,42});}
                bookLine(62,y+50,title);
                folio_line(c,642,y+50,b->text.substr(split+std::string(" · ").size()),28,Canvas::Align::Right);
            }else{
                folio_line(c,62,y+51,b->text,136);
                folio_arrow(c,637,y+51,9);
            }
            c.fill({42,y+98,600,1});y+=101;
        }
        folioButton({42,980,600,100},Action::WebAction,apply->text,apply->action,true,apply->enabled,44);
        navBar(w,true);return;
    }
}
if(folioUI && v.screen!=Screen::WebPrompt && status.empty() && w.error.empty() && w.route=="review"){
    answer_pages_=1;sectionHeader();int y=297,fy=0;
    for(const auto &b:w.blocks)if(b.role=="review-history")folioButton({376,123,268,86},Action::WebAction,b.text,b.action,false,true,36);
    for(const auto &b:w.blocks){
        if(b.kind=="heading"){
            if(b.text=="我的单词本" || b.text=="全部已学词库")folio_line(c,42,y,b.text,44);
            else bookLine(42,y,b.text);
            y+=103;
        }else if(b.kind=="button"){
            if(b.area=="foot"){
                folioButton({42,752+fy*114,600,100},Action::WebAction,b.text,b.action,b.primary,b.enabled,b.primary?44:136);++fy;
            }
        }else{
            const int size=b.role=="source"?32:40;
            for(const auto &line:c.wrap_px(b.text,600,size)){c.text_px(42,y-size/2,line,size);y+=size+13;}
            y+=18;
        }
    }
    navBar(w,true);return;
}
