#include "resource_cache.hpp"
#include <iostream>
#include <stdexcept>
using namespace wordcard;
int main() {
    int checks=0;auto check=[&](bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);};
    ResourceCache cache;ResourceStamp a{100,10,true},b{200,20,true};int reads=0,stats=0;
    auto statA=[&](ResourceStamp &s){++stats;s=a;return true;};
    auto statB=[&](ResourceStamp &s){++stats;s=b;return true;};
    auto verify=[&]{++reads;return ResourceCheck::Valid;};
    check(cache.get("a:v1",statA,verify)==ResourceCheck::Valid,"first use validates");
    for(int i=0;i<100;++i)cache.get("a:v1",statA,verify);
    check(reads==1 && stats==1,"unchanged session does no filesystem checks");
    cache.get("b:v1",statB,verify);const auto encoded=cache.encode();
    ResourceCache restored;check(restored.restore(encoded),"restore persisted cache");
    check(reads==2 && stats==2,"restore does not scan resources");
    restored.get("a:v1",statA,verify);check(reads==2 && stats==3,"reboot unchanged first use compares metadata only");
    ++b.modified;restored.get("b:v1",statB,verify);
    check(reads==3 && stats==4,"only changed file content revalidated");
    restored.get("a:v1",statA,verify);check(reads==3 && stats==4,"other file remains warm");
    restored.invalidate("a:v1");restored.get("a:v1",statA,verify);
    check(reads==4 && stats==5,"read failure invalidates only its entry");
    restored.invalidate("a:v1");
    check(restored.get("a:v1",statA,[]{return ResourceCheck::Retry;})==ResourceCheck::Retry,"transient I/O error");
    check(!restored.verified("a:v1") && restored.candidate("a:v1"),"I/O failure not cached as corrupt");
    restored.get("a:v1",statA,verify);check(restored.verified("a:v1"),"retry can recover");
    ResourceCache missing;ResourceStamp gone{};
    check(missing.get("gone",[&](ResourceStamp&s){s=gone;return true;},verify)==ResourceCheck::Invalid,"missing file");
    ResourceCache added;check(added.restore(missing.encode()),"persist absence");
    check(added.get("gone",statA,verify)==ResourceCheck::Valid,"added resource invalidates prior absence");
    ResourceCache damaged;auto corrupt=encoded;corrupt.back()='x';
    check(!damaged.restore(corrupt) && damaged.size()==0,"truncated or damaged cache rejected");
    ResourceCache failedStat;
    check(failedStat.get("a",[](ResourceStamp &){return false;},verify)==ResourceCheck::Retry,"stat I/O retry");
    check(failedStat.size()==0,"failed metadata not persisted");
    restored.get("a:v2",statA,verify);check(restored.verified("a:v2"),"validator version gets separate cache");
    std::cout<<"PASS: "<<checks<<" resource cache checks; no startup scan, reuse, selective invalidation and I/O recovery\n";
}
