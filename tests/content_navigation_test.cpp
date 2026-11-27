#include "learning_web.hpp"
#include "native_json.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace wordcard;

int main(int argc,char **argv) try {
    if(argc!=2)throw std::runtime_error("Pass an isolated learning-web directory with catalog and sample packages");
    LearningWeb web;
    auto require=[&](bool ok,const std::string &message){if(!ok)throw std::runtime_error(message+" "+web.error());};
    require(web.init(std::filesystem::path(argv[1]).generic_string()),"init");
    auto page=[&]{WebPage p;require(web.page(p),"page");return p;};
    auto click=[&](const std::string &label){
        auto p=page();int id=-1;
        for(auto &b:p.blocks)if(b.text==label&&b.action>=0)id=b.action;
        for(auto &b:p.tabs)if(b.text==label)id=b.action;
        for(auto &b:p.top)if(b.text==label)id=b.action;
        require(id>=0&&web.action(id),"click "+label+" from "+p.title);page();
    };
    auto status=[&]{std::string out;require(web.status(out),"status");return StudyJson::parse(out);};
    click("选择学习内容");require(page().route=="content-library","first-use opens unified catalog");click("首页");
    click("内容库");click("雨夜失物处");click("接下这件委托");click("读下一个词");
    auto word=str(status(),"word");
    require(web.back(),"back");click("首页");
    require(str(status()["currentContent"],"title")=="雨夜失物处","active story");
    click("复习");auto review=status();
    require(str(review,"selectedStage")=="pet-2024@1.0.0"&&str(review,"route")=="review","story review uses the shared scheduler for its declared deck");
    click("首页");click("继续学习");require(str(status(),"word")==word,"study position after review");
    require(web.back(),"back home");click("简单模式");click("普通 · 补字母");
    require(page().route=="home","changing mode returns to invoking home");
    click("内容库");click("词库");click("PET 主题词库");
    require(str(status(),"selectedStage")=="pet-2024@1.0.0"&&str(status()["batch"],"phase")=="learn","word list opens at first word");
    std::cout<<"PASS native bridge catalog, scope, home, resume and mode"<<std::endl;
    return 0;
} catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}
