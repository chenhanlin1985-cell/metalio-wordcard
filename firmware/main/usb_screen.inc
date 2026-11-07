void UI::usb_page(Canvas &c,const View &v) {
    c.text(40,54,"U 盘导入资源");c.text(41,54,"U 盘导入资源");
    c.fill({40,132,604,2});
    const char *title=v.usb_phase==0?"连接电脑，复制资源":v.usb_phase==1?"正在准备 U 盘模式":
        v.usb_phase==2?"U 盘连接模式":"未能开启 U 盘";
    c.text(40,220,title);
    auto paragraph=[&](int y,const std::string &s) {
        auto lines=c.wrap(s,604);
        for(size_t i=0;i<lines.size();++i)c.text(40,y+int(i)*46,lines[i]);
    };
    if(v.usb_phase==3) {
        paragraph(320,v.usb_error);
        paragraph(485,"已有文件保持原样。返回学习后可重新进入 U 盘模式。");
        paragraph(650,v.usb_error.find("ESP_ERR_NO_MEM")!=std::string::npos?
            "内存分配失败，请返回后重试。":"若仍失败，请关机后检查 SD 卡和连接。");
        button(c,{40,860,604,100},Action::UsbExit,"返回学习",0,true);
    } else {
        paragraph(330,"将资源包中的 metalio 文件夹合并到 U 盘根目录，保留已有个人文件。");
        paragraph(505,"传输期间暂停学习与发音。复制结束后，在电脑上安全弹出 U 盘。");
        paragraph(680,"安全弹出后点击返回学习，重新载入课程、词典和发音资源。");
        if(v.usb_phase==0) {
            button(c,{40,860,604,100},Action::UsbStart,"进入 U 盘模式",0,true);
            button(c,{40,990,604,88},Action::Back,"返回");
            paragraph(1120,"进入前保存进度，切换时会重启一次。");
        } else if(v.usb_phase==1) {
            button(c,{40,860,604,100},Action::None,"正在保存并切换…",0,false,false);
            paragraph(1035,"正在停止音频，请稍候。");
        } else {
            button(c,{40,860,604,100},Action::UsbExit,"返回学习");
            paragraph(1010,"电脑识别后即可复制文件。本页不会因等待连接而自动退出。");
        }
    }
}
