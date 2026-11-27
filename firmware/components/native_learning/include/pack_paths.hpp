#pragma once
#ifdef _WIN32
#include <filesystem>
#else
#include <dirent.h>
#endif
#include <vector>
#include <string>
static std::string book_path(const std::string &key) {
    const auto at=key.find('@');
    if(at==0||at==std::string::npos||at>64||key.size()>87)return {};
    for(size_t i=0;i<at;++i){
        const char c=key[i];
        if(!(c>='a'&&c<='z')&&!(c>='0'&&c<='9')&&(c!='-'||i==0))return {};
    }
    int dots=0,digits=0;
    for(size_t i=at+1;i<key.size();++i){
        const char c=key[i];
        if(c=='.'&&digits>0){++dots;digits=0;}
        else if(c>='0'&&c<='9'&&digits<6)++digits;
        else return {};
    }
    if(dots!=2||!digits)return {};
    return key.substr(0,at)+"/"+key.substr(at+1);
}
static std::vector<std::string> book_directories(const std::string &path){
    std::vector<std::string> names;
#ifdef _WIN32
    std::error_code error;
    for(const auto &child:std::filesystem::directory_iterator(path,error)){
        if(child.is_directory(error))names.push_back(child.path().filename().string());
    }
#else
    if(auto *dir=opendir(path.c_str())){
        while(const auto *entry=readdir(dir)){
            if(entry->d_name[0]!='.')names.emplace_back(entry->d_name);
        }
        closedir(dir);
    }
#endif
    return names;
}
