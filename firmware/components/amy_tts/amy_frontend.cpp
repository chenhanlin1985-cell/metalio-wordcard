#include "amy_frontend.hpp"
#include "speech_policy.hpp"
#include "cp_id_tables_multi.h"
#include <espeak-ng/speak_lib.h>
#include <cstring>
#include <iterator>
namespace {
int lookup(unsigned cp){size_t lo=0,hi=std::size(CP_ID_AMY);while(lo<hi){size_t mid=(lo+hi)/2;if(CP_ID_AMY[mid].cp<cp)lo=mid+1;else hi=mid;}return lo<std::size(CP_ID_AMY) && CP_ID_AMY[lo].cp==cp?CP_ID_AMY[lo].id:-1;}
bool next(const char *&p,unsigned &cp){unsigned char c=*p++;if(c<128){cp=c;return true;}int n=c>=0xf0&&c<=0xf4?3:c>=0xe0?2:c>=0xc2?1:0;if(!n)return false;cp=c&((1u<<(6-n))-1);for(int i=0;i<n;++i){unsigned char b=*p;if(!b||(b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&63);++p;}return cp<=0x10ffff && !(cp>=0xd800&&cp<=0xdfff);}
}
bool amy_phoneme_ids(OfflineTts &espeak,const std::string &folder,const std::string &text,std::vector<int32_t> &ids,std::string &error){
 ids.clear();std::string spoken;if(!wordcard::speech_text(text,spoken)){error="此文本暂不支持 Amy";return false;}
 if(!espeak.prepare(folder,error))return false;
 ids={1,0};bool sounded=false;
 auto append=[&](unsigned cp){const int id=lookup(cp);if(id<0)return true;if(ids.size()+3>265){error="Amy 文本过长，请分成短句";return false;}ids.push_back(id);ids.push_back(0);return true;};
 const void *cursor=spoken.c_str();int guard=0;
 while(cursor && *(const char *)cursor){
  if(++guard>128){error="Amy 音素分句超出限制";ids.clear();return false;}
  const char *begin=(const char *)cursor;
  const char *ph=espeak_TextToPhonemes(&cursor,espeakCHARS_UTF8,espeakPHONEMES_IPA);
  const char *end=cursor?(const char *)cursor:spoken.c_str()+spoken.size();
  if(end<=begin || end>spoken.c_str()+spoken.size()){error="Amy 音素转换未前进";ids.clear();return false;}
  if(ph){bool paren=false;while(*ph){unsigned cp;if(!next(ph,cp)){error="Amy 音素编码错误";ids.clear();return false;}if(cp=='('){paren=true;continue;}if(cp==')'){paren=false;continue;}if(paren)continue;if(!append(cp)){ids.clear();return false;}sounded|=cp!=' ' && lookup(cp)>=14;}}
  char punctuation=0;for(const char *q=end;q>begin;){char c=*--q;if(c=='.'||c==','||c=='?'||c=='!'||c==':'||c==';'){punctuation=c;break;}if(c!=' '&&c!='\t'&&c!='\n'&&c!='\r'&&c!='-')break;}
  if(punctuation && (!append(punctuation) || ((punctuation==','||punctuation==':'||punctuation==';') && !append(' ')))){ids.clear();return false;}
 }
 if(!sounded){error="Amy 没有可发音内容";ids.clear();return false;}ids.push_back(2);return true;
}
