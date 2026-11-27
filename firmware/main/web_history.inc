// Keep each five-word history page and its filters within one panel.
if(vocabulary&&w.route=="history"){
    answer_pages_=1;sectionHeader();
    for(const auto &b:w.top)if(b.text=="查找")folioButton({342,12,154,86},Action::WebAction,b.text,b.action,false,true,36);
    int y=328;
    for(const auto &b:w.blocks){
        if(b.role=="history-scope")folio_line(c,42,222,fit_px(c,b.text,600,32),32);
        else if(b.role=="history-count")folio_line(c,342,1113,b.text,32,Canvas::Align::Center);
        else if(b.role=="history-entry"){
            buttons_.push_back({{24,y,636,100},{Action::WebAction,b.action},b.text,true,false});
            folio_line(c,42,y+27,fit_px(c,b.text,565,40),40);
            folio_line(c,42,y+74,fit_px(c,b.detail,550,28),28);
            folio_arrow(c,636,y+48,8);c.fill({42,y+99,600,1});y+=105;
        }else if(b.role=="history-practice")folioButton({42,905,600,90},Action::WebAction,b.text,b.action,true,true,36);
        else if(b.kind=="button"){
            if(b.area=="foot")
                folioButton({b.text=="上一页"?24:450,1068,210,90},Action::WebAction,b.text,b.action,false,true,36);
            else if(b.text=="清除搜索"||b.text=="按词组找"){
                folioButton({346,252,308,66},Action::WebAction,b.text,b.action,false,true,28);
            }else folioButton({24,252,314,66},Action::WebAction,b.text,b.action,false,true,28);
        }else if(b.role=="meaning")folio_line(c,42,y+43,fit_px(c,b.text,600,32),32);
    }
    return;
}

