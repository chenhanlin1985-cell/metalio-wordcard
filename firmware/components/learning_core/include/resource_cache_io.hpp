#pragma once
#include "resource_cache.hpp"
#include "wordcard.hpp"
#include <cerrno>
#include <cstdio>
#include <sys/stat.h>
namespace wordcard {
inline bool resource_stamp(const std::string &path,ResourceStamp &stamp) {
    struct stat info{};errno=0;
    if(stat(path.c_str(),&info)!=0) {
        if(errno==ENOENT || errno==ENOTDIR){stamp={};return true;}
        return false;
    }
#ifdef _WIN32
    const bool regular=(info.st_mode&_S_IFMT)==_S_IFREG;
#else
    const bool regular=S_ISREG(info.st_mode)!=0;
#endif
    stamp={uint64_t(info.st_size),int64_t(info.st_mtime),regular};return true;
}
inline bool load_resource_cache(const std::string &path,ResourceCache &cache) {
    FILE *f=std::fopen(path.c_str(),"rb");if(!f)return false;
    std::string data;char block[1024];size_t n;
    while((n=std::fread(block,1,sizeof(block),f)) && data.size()+n<=2*1024*1024)data.append(block,n);
    const bool good=!std::ferror(f) && !n;std::fclose(f);
    return good && cache.restore(data);
}
inline bool save_resource_cache(const std::string &path,ResourceCache &cache) {
    if(!cache.dirty())return true;
    std::string error;
    if(!atomic_write(path,cache.encode(),error))return false;
    cache.saved();return true;
}
}
