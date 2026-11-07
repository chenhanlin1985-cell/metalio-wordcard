#include "wordcard_ui.hpp"
#include "dictionary_keyboard.hpp"
#include <algorithm>
namespace wordcard {
// Row previews are deliberately one line; disclose anything omitted. Trim at
// UTF-8 boundaries, using the same pixel size as the rendered word/meaning.
static std::string dictionary_row_preview(Canvas &c,const std::string &s,int width,int pixels) {
    const auto lines=c.wrap_px(s,width,pixels);
    if(lines.empty())return {};
    if(lines.size()==1)return lines.front();
    std::string out=lines.front();
    while(!out.empty()&&c.text_width_px(out+"…",pixels)>width){
        size_t end=out.size()-1;
        while(end>0&&(static_cast<unsigned char>(out[end])&0xC0)==0x80)--end;
        out.resize(end);
    }
    return out+"…";
}
void UI::dictionary_pages(Canvas &c,const View &v) {
    const bool recall_input=v.screen==Screen::RecallLesson&&v.recall_typing&&!v.recall.checked;
    if (v.screen == Screen::Dictionary||recall_input) {
        button(c,{24,12,120,96},Action::Back,"返回");
        c.text_px(180,34,recall_input?(v.recall_listening?"听写":"练拼写"):v.dictionary_native_lookup?"完整词典":"查词",36);
        button(c,{476,12,184,96},Action::Vocabulary,recall_input?(v.recall_listening?"播放题目":"首字母提示"):"生词本");
        c.border({12,124,660,132},3);
        std::string visible = recall_input?v.recall.typed:v.dictionary_query;
        bool shortened = false;
        while (c.text_width((shortened?"…":"")+visible,2)>620 && !visible.empty()) {visible.erase(0,1);shortened=true;}
        if(visible.empty())c.text_px(32,172,recall_input?"输入你的答案":"输入英文单词或短语",36);else c.text(32,146,(shortened?"…":"")+visible,2);
        button(c,{518,1092,164,84},Action::DictionarySearch,recall_input?"检查":"查询",0,true,recall_input?v.recall_ready:v.dictionary_ready);
        c.text_px(24,284,dictionary_row_preview(c,!v.notice.empty()?v.notice:recall_input?(v.recall_listening&&!v.recall_heard?"先点右上角播放题目":"输入完成后，点右下角检查"):!v.dictionary_ready?"请先复制词典 SD 资源包":"长按退格连续删除",636,36),36);
        if(recall_input&&v.recall_card){
            const auto prompt=c.wrap(v.recall_listening?"听发音，写出单词或短语。\n可以反复播放；返回会保留本题。":v.recall_card->meaning,636);
            for(size_t i=0;i<prompt.size()&&i<5;++i)c.text(24,398+int(i)*48,prompt[i]);
            button(c,{24,650,636,88},Action::RecallSkip,v.recall_listening?"暂时不方便听，保留待补":"暂缓本题，稍后再练");
            c.text_px(24,750,"已练对 "+std::to_string(v.recall.solved)+" / "+std::to_string(v.recall.total)+" 项",36);
        } else if(!v.dictionary_recent.empty()) {
            c.text(24,344,"最近查过");
            for(size_t i=0;i<v.dictionary_recent.size()&&i<2;++i)
                button(c,{24,398+int(i)*108,636,88},Action::DictionaryRecent,v.dictionary_recent[i],int(i));
        } else {
            const auto tips=c.wrap("输入完整单词，或输入开头几个字母。\n日常用语、英文短语和科技缩写都可以查。",636);
            for(size_t i=0;i<tips.size();++i)c.text(24,398+int(i)*48,tips[i]);
        }
        // Only the editable strip is monochrome. Its old/new pixels can use
        // the board's complete DU waveform without changing gray UI text.
        for(int y=124;y<354;++y)for(int x=12;x<672;++x)c.pixel(x,y,c.shade(x,y)<8);
        for(int slot=0;slot<30;++slot) {
            const auto key=dictionary_keyboard_key(slot,v.dictionary_shift,v.dictionary_symbols);
            const auto &r=key.rect;const auto &e=key.event;
            if(e.action==Action::DictionaryKey && e.value!=' ') {
                const std::string label(1,char(e.value));
                button(c,r,e.action,"",e.value);
                buttons_.back().label=label;
                c.text(r.x+(r.w-c.text_width(label))/2,r.y+(r.h-38)/2,label);
            } else if(e.action==Action::DictionaryShift||e.action==Action::DictionaryErase) {
                button(c,r,e.action,"",0,e.action==Action::DictionaryShift&&v.dictionary_shift);
                buttons_.back().label=e.action==Action::DictionaryShift?"大写":"退格";
                const int cx=r.x+r.w/2,cy=r.y+r.h/2;
                if(e.action==Action::DictionaryShift) {
                    for(int i=0;i<18;++i){c.fill({cx-i,cy-20+i,3,3});c.fill({cx+i-2,cy-20+i,3,3});}
                    c.fill({cx-17,cy-3,11,3});c.fill({cx+7,cy-3,11,3});
                    c.fill({cx-8,cy-2,3,23});c.fill({cx+6,cy-2,3,23});c.fill({cx-8,cy+19,17,3});
                } else {
                    for(int i=0;i<15;++i){c.fill({cx-22+i,cy-i,3,3});c.fill({cx-22+i,cy+i,3,3});}
                    c.fill({cx-7,cy-14,27,3});c.fill({cx-7,cy+14,27,3});c.fill({cx+18,cy-14,3,31});
                    for(int i=0;i<13;++i){c.fill({cx-2+i,cy-6+i,2,2});c.fill({cx+10-i,cy-6+i,2,2});}
                }
            } else {
                const char *label=e.action==Action::DictionaryShift?(v.dictionary_shift?"小写":"大写"):
                    e.action==Action::DictionarySymbols?(v.dictionary_symbols?"ABC":"123"):
                    e.action==Action::DictionaryErase?"退格":"空格";
                button(c,r,e.action,label,e.value,e.action==Action::DictionaryShift&&v.dictionary_shift);
            }
        }
        return;
    }
    c.text_px(40,32,v.screen==Screen::Vocabulary || (v.entry_from_vocabulary && v.screen==Screen::DictionaryEntry) ? "我的生词本" : "离线词典",44); c.fill({40,98,604,2});
    button(c,{40,120,140,88},Action::Back,"返回");
    if (v.screen == Screen::DictionaryResults) {
        c.text(40,240,"查词结果",2);
        c.text(40,310,c.ellipsize(v.dictionary_query,604));
        for (size_t i=0;i<v.dictionary_matches.size() && i<6;++i) {
            const auto &match=v.dictionary_matches[i];const int y=370+int(i)*96;
            button(c,{40,y,604,88},Action::DictionarySelect,"",int(i));
            c.text_px(56,y+2,dictionary_row_preview(c,match.word,572,40),40);
            c.text_px(56,y+47,dictionary_row_preview(c,match.meaning,572,36),36);
        }
        if (v.dictionary_matches.empty()) {
            c.text(40,420,"没有找到匹配词条。");
            c.text(40,480,"可试单词原形或更短的前缀。");
        }
        if (!v.notice.empty()) c.text(40,990,c.ellipsize(v.notice,604));
        else if (v.dictionary_matches.size()==6) c.text(40,990,"显示前 6 条；可补全输入缩小范围。");
        button(c,{40,v.dictionary_native_lookup?1080:1060,604,v.dictionary_native_lookup?100:88},Action::Dictionary,"修改输入",0,v.dictionary_native_lookup);
    } else if (v.screen == Screen::DictionaryEntry && v.card) {
        int px=78;auto word=c.wrap_px(v.card->front,604,px);
        if(word.size()>2){px=48;word=c.wrap_px(v.card->front,604,px);}
        for(size_t i=0;i<std::min<size_t>(3,word.size());++i)c.text_px(40,230+int(i)*(px+12),word[i],px);
        const auto text=c.wrap_px((v.card->ipa.empty() ? "音标暂缺\n" : v.card->ipa+"\n")+v.card->back,604,40);
        answer_pages_=std::max(1,int((text.size()+5)/6));
        const size_t start=size_t(std::clamp(v.answer_page,0,answer_pages_-1))*6;
        for(size_t i=start;i<text.size()&&i<start+6;++i)c.text_px(40,452+int(i-start)*52,text[i],40);
        c.text(450,140,std::to_string(v.answer_page+1)+" / "+std::to_string(answer_pages_));
        button(c,{40,800,280,88},Action::DictionaryPage,"上一页",-1,false,v.answer_page>0);
        button(c,{364,800,280,88},Action::DictionaryPage,"下一页",1,false,v.answer_page+1<answer_pages_);
        const bool speaking=v.audio_busy || v.audio_pending;
        button(c,{40,v.dictionary_native_lookup?968:920,280,88},speaking?Action::SpeechStop:Action::Speak,speaking ? "停止发音" : v.audio_available ? (v.audio_source==SpeechSource::Synthesis ? "合成发音 / 再听" : "SD 发音 / 再听") : "暂无可用发音",0,false,v.audio_available || speaking);
        if(v.dictionary_native_lookup) {
            button(c,{364,968,280,88},Action::Vocabulary,"生词本");
            button(c,{40,1080,604,100},Action::DictionaryAdd,v.dictionary_added?"已加入生词本":"加入生词本",0,true,v.writable && !v.dictionary_added);
            const auto message=!v.notice.empty()?v.notice:!v.audio_status.empty()?v.audio_status:
                !v.writable?"SD 不可写，暂不能保存生词。":"完整词典释义";
            c.text_px(40,904,dictionary_row_preview(c,message,604,36),36);
            return;
        }
        button(c,{364,920,280,88},Action::DictionaryAdd,v.dictionary_added ? "已加入生词本" : "加入生词本",0,true,v.writable && v.personal_readable && !v.dictionary_added);
        std::string message=!v.notice.empty()?v.notice:!v.personal_readable?v.personal_error:
            !v.audio_status.empty()?v.audio_status:!v.writable?"SD 不可写，暂不能保存生词。":
            v.card->hint=="项目补充词条"?"项目补充词条；合成发音会保存供再听。":
            v.card->hint=="课程常用义 + ECDICT"?"课程常用义 + ECDICT · 发音可缓存":"ECDICT 释义；合成发音会保存供再听。";
        const auto status=c.wrap(message,604);
        for (size_t i=0;i<status.size() && i<2;++i)c.text(40,1024+int(i)*36,status[i]);
        button(c,{40,1100,604,88},Action::Vocabulary,v.dictionary_added?"去生词本学习":"查看我的生词本");
    } else if (v.screen == Screen::Vocabulary) {
        c.text(40,240,"我的生词本",2);
        button(c,{364,120,280,88},Action::Dictionary,"查词加生词");
        const auto &p=v.vocabulary;
        if (!v.personal_readable || !p.total) {
            const auto lines=c.wrap(!v.personal_readable?v.personal_error:"还没有添加生词。\n查到不认识的词，点加入生词本。\n之后可以背词，并按计划复习。",604);
            for (size_t i=0;i<lines.size() && i<6;++i)c.text(40,410+int(i)*56,lines[i]);
            if (!v.notice.empty()) c.text(40,780,c.ellipsize(v.notice,604));
            button(c,{40,900,604,108},v.personal_readable?Action::Dictionary:Action::Resources,v.personal_readable?"去查一个词":"查看资源与保存状态",0,true);
            return;
        }
        c.text(40,320,"共 "+std::to_string(p.total)+" 词 · 待复习 "+std::to_string(p.due)+" · 新词 "+std::to_string(p.fresh));
        const char *filters[]={"全部","待复习","新词"};
        for (int i=0;i<3;++i)button(c,{40+i*208,362,188,88},Action::VocabularyFilter,filters[i],i,int(v.vocabulary_filter)==i);
        for (size_t i=0;i<p.items.size();++i) {
            const auto &item=p.items[i];const int y=458+int(i)*100;
            button(c,{40,y,604,92},Action::VocabularySelect,"",int(item.index));
            const int status_width=c.text_width_px(item.status,36);
            c.text_px(56,y+4,dictionary_row_preview(c,item.word,544-status_width,40),40);
            c.text_px(628-status_width,y+8,item.status,36);
            c.text_px(56,y+50,dictionary_row_preview(c,item.meaning,572,36),36);
        }
        if (p.items.empty()) {
            c.text(40,510,v.vocabulary_filter==VocabularyFilter::Due?"目前没有到期词。":"这一页没有新词了。");
            c.text(40,568,"可切换到全部，查看或听已存的词。");
        }
        button(c,{40,872,184,88},Action::VocabularyPage,"上一页",-1,false,p.page>0);
        c.text_px(270,897,std::to_string(p.page+1)+" / "+std::to_string(p.pages),36);
        button(c,{460,872,184,88},Action::VocabularyPage,"下一页",1,false,size_t(p.page+1)<p.pages);
        const size_t due=v.vocabulary_filter==VocabularyFilter::New?0:p.due;
        const size_t fresh=v.vocabulary_filter==VocabularyFilter::Due?0:v.personal_planned_new;
        std::string message=!v.notice.empty()?v.notice:!v.date_valid?"日期未设置，请先在“我的”校准。":
            due+fresh?"先复习到期词，再学新词。":
            v.vocabulary_filter==VocabularyFilter::Due?"暂无到期词，可切换到全部或新词。":
            p.fresh?"今日新词额度已用完，可调整学习计划。":"本轮已学完，到期后再复习。";
        const auto lines=c.wrap(message,604);
        for(size_t i=0;i<lines.size() && i<2;++i)c.text(40,976+int(i)*36,lines[i]);
        button(c,{40,1070,604,96},v.guided_enabled?Action::RecallBegin:Action::VocabularyReview,v.personal_resume?"继续这一节":
            v.vocabulary_filter==VocabularyFilter::Due?"开始复习 · "+std::to_string(due)+" 词":
            "开始学习 · "+std::to_string(due+fresh)+" 词待学",v.guided_enabled?(v.vocabulary_filter==VocabularyFilter::New?7:v.vocabulary_filter==VocabularyFilter::Due?8:6):0,true,v.personal_resume || due+fresh>0);
    }
}
} // namespace wordcard
