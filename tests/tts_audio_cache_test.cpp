#include "tts_audio_cache.hpp"
#include "tts_test_model.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace wordcard;
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    int checks=0;auto check=[&](bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);};
    const auto folder=std::filesystem::path(argv[1])/"tts-cache-fixture";
    // A unique fixture each invocation: never delete user/cache files.
    const auto run=folder/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(run);
    TtsAudioCache cache;cache.open(run.string());CachedSpeech read;
    const auto a=tts_audio_key("API","sano"),b=tts_audio_key("api","sano"),engine=tts_audio_key("API","espeak");
    check(a!=b && a!=engine,"text case and engine identities distinct");
    const auto pico=tts_audio_key("API","test-3");
    check(pico!=tts_audio_key("API","test-2") && pico!=a && pico.find("tts-pico-v1/")==0,"Pico has isolated model and processing identity");
    std::vector<int16_t> pcm{0,1,-1,32767,-32768,4000,-2000};
    check(!cache.load(a,read),"missing cache is a miss, no scan or invented audio");
    check(cache.store(a,pcm.data(),pcm.size(),100000,1),"atomic source PCM save");
    const auto validated=cache.validations();
    check(cache.load(a,read) && read.pcm==pcm && read.gain==100000 && read.actual==1,"exact PCM and playback metadata");
    check(cache.validations()==validated,"same session no repeated CRC validation");
    check(cache.store(pico,pcm.data(),pcm.size(),65536,5) && cache.load(pico,read) && read.actual==5 && read.pcm==pcm,"Pico engine identity survives exact PCM cache round trip");
    const auto amy=tts_audio_key("API","amy");
    check(amy!=a && amy!=pico && cache.store(amy,pcm.data(),pcm.size(),65536,6) && cache.load(amy,read) && read.actual==6 && read.pcm==pcm,"Amy identity is isolated and persists");
    TtsAudioCache reboot;reboot.open(run.string());
    const auto amy16=tts_audio_key("API","amy16"),amymix=tts_audio_key("API","amymix");
    check(amy16!=amy && amymix!=amy && amy16!=amymix && tts_audio_key("API","test-5")!=tts_audio_key("API","test-6"),"integer precisions and diagnostic identities isolated");
    check(cache.store(amy16,pcm.data(),pcm.size(),65536,7) && cache.load(amy16,read) && read.actual==7,"INT16 cache metadata survives");
    check(cache.store(amymix,pcm.data(),pcm.size(),65536,8) && cache.load(amymix,read) && read.actual==8,"mixed cache metadata survives");
    TtsAudioCache integer_reboot;integer_reboot.open(run.string());
    check(integer_reboot.load(amy16,read) && read.pcm==pcm && read.actual==7 && integer_reboot.load(amymix,read) && read.actual==8,"integer modes persist across reopen");
    const auto small=tts_audio_key("API","amysmall");
    check(small!=amy && small!=amy16 && small!=amymix && tts_audio_key("API","test-7")!=tts_audio_key("API","test-4"),"Small model cache identity separate from original and integer voices");
    check(cache.store(small,pcm.data(),pcm.size(),65536,9),"Small cache write accepts new actual engine");
    TtsAudioCache small_reboot;small_reboot.open(run.string());
    check(small_reboot.load(small,read) && read.pcm==pcm && read.actual==9,"Small voice PCM persists across reopen");
    const auto packed=tts_audio_key("API","amysmall16");
    check(packed!=small && tts_audio_key("API","test-8")!=tts_audio_key("API","test-7"),"prepacked precision has independent identity");
    check(cache.store(packed,pcm.data(),pcm.size(),65536,10) && cache.load(packed,read) && read.actual==10,"prepacked voice cache roundtrip");
    check(!cache.store(small,pcm.data(),pcm.size(),65536,11),"unknown engine is rejected");
    check(reboot.load(a,read) && read.pcm==pcm && reboot.validations()==0,"reboot unchanged metadata reuses persisted validation");
    check(!reboot.load(engine,read),"wrong engine does not reuse a voice");
    check(!reboot.load("new-runtime/"+a,read),"processing version invalidates identity");
    std::string collision;
    for(int i=0;i<10000;++i) {auto k=tts_audio_key("other-"+std::to_string(i),"sano");if(cache.path(k)==cache.path(a)){collision=k;break;}}
    check(!collision.empty() && cache.store(collision,pcm.data(),pcm.size(),65536,2),"bounded-slot collision fixture");
    check(!cache.load(a,read) && cache.load(collision,read),"collision never returns wrong word");
    check(cache.store(a,pcm.data(),pcm.size(),65536,1),"replace old slot safely");
    {std::fstream f(cache.path(a),std::ios::binary|std::ios::in|std::ios::out);f.seekp(-1,std::ios::end);char x=0;f.write(&x,1);}
    auto old_time=std::filesystem::last_write_time(cache.path(a));
    std::filesystem::last_write_time(cache.path(a),old_time+std::chrono::seconds(3));
    TtsAudioCache edited;edited.open(run.string());
    check(!edited.load(a,read) && edited.validations()==1,"changed corrupted file rejected selectively");
    check(!cache.store(a,pcm.data(),0,65536,1) && !cache.store(a,pcm.data(),TtsAudioCache::max_samples+1,65536,1),"no partial empty or oversized cache");
    TtsAudioCache readonly;readonly.open((run/"absent-parent"/"cache").string());
    check(!readonly.store(a,pcm.data(),pcm.size(),65536,1),"unwritable cache fails independently of synthesis");
    TtsTestSession s;check(s.start(false,true) && s.reuse_audio,"listen may reuse cached sound");s.cancel();
    check(s.start(false) && !s.reuse_audio,"explicit test always recomputes");s.cancel();
    check(s.start(true,true) && !s.reuse_audio,"batch performance comparison bypasses cache");
    std::cout<<"PASS: "<<checks<<" TTS audio cache checks; replay, restart, exact identity, collision, corruption and failure isolation\n";
}
