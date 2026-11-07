#include "speech_policy.hpp"
namespace wordcard {
bool speech_text(const std::string &input, std::string &output) {
    output.clear();
    if (input.empty() || input.size() > 360) return false;
    bool content = false;
    for (size_t i=0; i<input.size();) {
        const unsigned char ch=input[i];
        if (ch >= 32 && ch <= 126) {
            output += char(ch); ++i;
            content |= (ch>='A' && ch<='Z') || (ch>='a' && ch<='z') || (ch>='0' && ch<='9');
        } else if (ch=='\n' || ch=='\r' || ch=='\t') { output += ' '; ++i; }
        else if (input.compare(i,3,"’")==0 || input.compare(i,3,"‘")==0) { output+='\''; i+=3; }
        else if (input.compare(i,3,"“")==0 || input.compare(i,3,"”")==0) { output+='"'; i+=3; }
        else if (input.compare(i,3,"–")==0 || input.compare(i,3,"—")==0) { output+='-'; i+=3; }
        else if (ch>=0xc2 && ch<=0xca && i+1<input.size() && (static_cast<unsigned char>(input[i+1])&0xc0)==0x80) {
            const unsigned cp=((ch&31u)<<6)|(static_cast<unsigned char>(input[i+1])&63u);
            if(cp==0xa0) output+=' ';
            else if(cp>=0xc0 && cp<=0x2af && cp!=0xd7 && cp!=0xf7) {output.append(input,i,2);content=true;}
            else return false;
            i+=2;
        }
        else return false;
    }
    if (output=="@") { output="at"; content=true; }
    else if (output=="IT" || output=="API" || output=="USB" || output=="CPU" || output=="GPU") {
        std::string letters;
        for (char ch:output) { if(!letters.empty()) letters+=' '; letters+=ch; }
        output=letters;
    } else if (output=="C++") output="C plus plus";
    else if (output=="C#") output="C sharp";
    else if (output=="a/an") output="a or an";
    return content;
}
SpeechSource speech_source(bool sd_valid, bool tts_ready, const std::string &text) {
    if(sd_valid) return SpeechSource::Sd;
    std::string normalized;
    return tts_ready && speech_text(text,normalized) ? SpeechSource::Synthesis : SpeechSource::None;
}
bool SpeechResampler::push(const int16_t *data,size_t count,const std::function<bool(const int16_t *,size_t)> &sink) {
    if (rate_<8000 || rate_>48000) return false;
    int16_t buffer[512];size_t used=0;
    for(size_t i=0;i<count;++i,++index_) {
        while(next_<=index_*16000) {
            int64_t value=data[i];
            if(index_) {
                const auto fraction=next_-(index_-1)*16000;
                value=(int64_t(previous_)*int64_t(16000-fraction)+int64_t(data[i])*int64_t(fraction))/16000;
            }
            buffer[used++]=int16_t(value);++emitted_;next_+=rate_;
            if(used==512) { if(!sink(buffer,used)) return false;used=0; }
        }
        previous_=data[i];
    }
    return !used || sink(buffer,used);
}
} // namespace wordcard
