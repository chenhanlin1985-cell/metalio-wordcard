#pragma once
#include <cstdint>
#include <functional>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
namespace wordcard {
struct ResourceStamp {
    uint64_t bytes=0;
    int64_t modified=0;
    bool present=false;
    bool operator==(const ResourceStamp &b)const {
        return bytes==b.bytes && modified==b.modified && present==b.present;
    }
};
enum class ResourceCheck { Valid, Invalid, Retry };
// One owner per cache. External writes are only allowed across a USB restart.
// A restored entry gets one metadata comparison at first use, never a scan.
class ResourceCache {
    struct Entry {ResourceStamp stamp;ResourceCheck result;bool current=false;};
    std::map<std::string,Entry> entries_;
    bool dirty_=false;
    static constexpr size_t limit=4096;
    static uint32_t checksum(const std::string &s) {
        uint32_t h=2166136261u;for(unsigned char c:s){h^=c;h*=16777619u;}return h;
    }
  public:
    uint32_t hits=0,metadata_checks=0,validations=0;
    ResourceCheck get(const std::string &key,const std::function<bool(ResourceStamp &)> &stat,
                      const std::function<ResourceCheck()> &verify) {
        auto it=entries_.find(key);
        if(it!=entries_.end() && it->second.current){++hits;return it->second.result;}
        ResourceStamp stamp;++metadata_checks;
        if(!stat(stamp)) {invalidate(key);return ResourceCheck::Retry;}
        if(it!=entries_.end() && it->second.stamp==stamp) {
            it->second.current=true;++hits;return it->second.result;
        }
        ++validations;
        const auto result=stamp.present?verify():ResourceCheck::Invalid;
        if(result==ResourceCheck::Retry) {invalidate(key);return result;}
        if(entries_.size()>=limit && it==entries_.end())entries_.erase(entries_.begin());
        entries_[key]={stamp,result,true};dirty_=true;return result;
    }
    // Unknown entries remain candidates until actually used; never label them verified.
    bool candidate(const std::string &key)const {
        const auto it=entries_.find(key);
        return it==entries_.end() || !it->second.current || it->second.result==ResourceCheck::Valid;
    }
    bool verified(const std::string &key)const {
        const auto it=entries_.find(key);
        return it!=entries_.end() && it->second.current && it->second.result==ResourceCheck::Valid;
    }
    void invalidate(const std::string &key) {if(entries_.erase(key))dirty_=true;}
    void clear() {entries_.clear();dirty_=true;}
    void recheck_metadata() {for(auto &p:entries_)p.second.current=false;}
    bool dirty()const{return dirty_;}
    void saved(){dirty_=false;}
    size_t size()const{return entries_.size();}
    std::string encode()const {
        std::ostringstream body;
        for(const auto &p:entries_)body<<std::quoted(p.first)<<' '<<p.second.stamp.bytes<<' '
            <<p.second.stamp.modified<<' '<<p.second.stamp.present<<' '
            <<(p.second.result==ResourceCheck::Valid)<<'\n';
        const auto data=body.str();return "WCRC2 "+std::to_string(checksum(data))+"\n"+data;
    }
    bool restore(const std::string &data) {
        if(data.size()>2*1024*1024)return false;
        const auto nl=data.find('\n');if(nl==std::string::npos)return false;
        std::istringstream header(data.substr(0,nl));std::string magic,extra;uint32_t crc=0;
        if(!(header>>magic>>crc) || magic!="WCRC2" || (header>>extra) || checksum(data.substr(nl+1))!=crc)return false;
        std::map<std::string,Entry> loaded;
        std::istringstream rows(data.substr(nl+1));std::string line;
        while(std::getline(rows,line)) {
            std::istringstream row(line);std::string key;ResourceStamp stamp;int present,valid;
            if(!(row>>std::quoted(key)>>stamp.bytes>>stamp.modified>>present>>valid) || (row>>extra) ||
               key.empty() || key.size()>512 || key.find_first_of("\r\n")!=std::string::npos ||
               (present!=0 && present!=1) || (valid!=0 && valid!=1) || (!present && valid) ||
               loaded.size()>=limit || loaded.count(key))return false;
            stamp.present=present;loaded.emplace(key,Entry{stamp,valid?ResourceCheck::Valid:ResourceCheck::Invalid,false});
        }
        entries_=std::move(loaded);dirty_=false;return true;
    }
};
}
