#pragma once
#include "resource_cache_io.hpp"
#include <cstring>
#ifdef ESP_PLATFORM
#include <esp_log.h>
#endif
namespace wordcard {
inline ResourceCheck read_binary_asset(const std::string &path,size_t bytes,uint32_t expected,
                                      void *destination,bool check_crc,std::string &error) {
    errno=0;FILE *file=std::fopen(path.c_str(),"rb");
    if(!file) {
        error=errno==ENOENT?"合成资源不存在："+path.substr(path.find_last_of('/')+1):"SD 资源打开失败，请重试";
        return errno==ENOENT?ResourceCheck::Invalid:ResourceCheck::Retry;
    }
    uint32_t crc=0xffffffff;size_t total=0,n;unsigned char block[4096];
    while((n=std::fread(block,1,sizeof(block),file))!=0) {
        if(total+n>bytes){total+=n;break;}
        if(check_crc)for(size_t i=0;i<n;++i){crc^=block[i];for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));}
        if(destination)std::memcpy(static_cast<unsigned char *>(destination)+total,block,n);
        total+=n;
    }
    const int io=std::ferror(file),reason=errno;std::fclose(file);
    const bool valid=!io && total==bytes && (!check_crc || ~crc==expected);
#ifdef ESP_PLATFORM
    if(!valid)ESP_LOGE("wordcard.assets","read path=%s bytes=%u/%u crc=%08lx/%08lx ferror=%d errno=%d",
        path.c_str(),unsigned(total),unsigned(bytes),(unsigned long)~crc,(unsigned long)expected,io,reason);
#else
    (void)reason;
#endif
    if(valid)return ResourceCheck::Valid;
    error=io?"SD 资源读取失败，请重试":"合成资源大小或校验不符："+path.substr(path.find_last_of('/')+1);
    return io?ResourceCheck::Retry:ResourceCheck::Invalid;
}
inline bool cached_binary_asset(ResourceCache &cache,const std::string &path,size_t bytes,uint32_t crc,
                                void *destination,std::string &error) {
    const auto key=path+":"+std::to_string(bytes)+":"+std::to_string(crc);bool copied=false;
    const auto result=cache.get(key,[&](ResourceStamp &s) {
        const bool ok=resource_stamp(path,s);if(!ok)error="SD 资源信息读取失败，请重试";return ok;
    },[&] {
        const auto result=read_binary_asset(path,bytes,crc,destination,true,error);
        copied=destination && result==ResourceCheck::Valid;return result;
    });
    if(result!=ResourceCheck::Valid) {
        if(error.empty())error="合成资源缺失或校验未通过："+path.substr(path.find_last_of('/')+1);
        return false;
    }
    // An unchanged model still needs loading after reboot. Skip only its CRC,
    // not the read/error/length checks needed to put actual bytes in memory.
    if(destination && !copied && read_binary_asset(path,bytes,crc,destination,false,error)!=ResourceCheck::Valid) {
        cache.invalidate(key);return false;
    }
    return true;
}
}
