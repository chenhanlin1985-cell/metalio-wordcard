// Separate executable linked ONLY against unchanged upstream float kernels.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <string>
extern "C" int snt_voice_synthesize(const uint8_t *,const uint8_t *,const int32_t *,int,float,float *,int);
static std::vector<char> read(const std::string &p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){if(argc!=3)return 2;auto front=read(std::string(argv[1])+"/front_f32.bin"),dec=read(std::string(argv[1])+"/dec_f32.bin");
for(int i=1;i<=6;++i){const std::string path=std::string(argv[2])+"/"+std::to_string(i)+"-amy";auto ids=read(path+"-ids.bin");std::vector<float> samples(22050*12);
int n=snt_voice_synthesize((const uint8_t *)front.data(),(const uint8_t *)dec.data(),(const int32_t *)ids.data(),int(ids.size()/4),1.08f,samples.data(),int(samples.size()));if(n<=0)return 3;
std::ofstream f(path+"-raw.wav",std::ios::binary);auto u32=[&](uint32_t x){for(int b=0;b<4;++b)f.put(char(x>>(b*8)));};auto u16=[&](uint16_t x){f.put(char(x));f.put(char(x>>8));};
f.write("RIFF",4);u32(36+n*2);f.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(22050);u32(44100);u16(2);u16(16);f.write("data",4);u32(n*2);
for(int j=0;j<n;++j){if(!std::isfinite(samples[j]))return 4;u16(uint16_t(int16_t(std::lround(std::clamp(samples[j],-1.f,1.f)*32767))));}if(!f)return 5;
}return 0;}
