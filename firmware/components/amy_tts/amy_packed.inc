/* Validated packed model pointers. No FP32 decoder weights are resident in the
 * prepacked mode. Float w pointers are opaque identities intercepted below. */
static const unsigned char *packed_model;
static size_t packed_bytes;
static uint32_t aq_u32(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
void amy_packed_unbind(void){packed_model=NULL;packed_bytes=0;}
int amy_packed_init(snt_piperlite_model *m,const void *blob,size_t bytes){
 amy_packed_unbind();if(!m || !blob || ((uintptr_t)blob&15) || bytes<1536)return -1;
 const unsigned char *p=blob;
 const uint32_t head[]={0x31505141,1,192,160,80,40,20,23,46};
 for(unsigned i=0;i<9;++i)if(aq_u32(p+i*4)!=head[i])return -2;
 if(aq_u32(p+36)!=bytes || aq_u32(p+40)!=16)return -3;
 memset(m,0,sizeof(*m));m->in_ch=192;m->c0=160;m->c1=80;m->c2=40;m->c3=20;m->n_tensors=46;
 for(unsigned i=0;i<23;++i){
  const unsigned char *d=p+64+i*64;unsigned slot=aq_u32(d),ic=aq_u32(d+4),oc=aq_u32(d+8),k=aq_u32(d+12),s=aq_u32(d+16),pad=aq_u32(d+20),dil=aq_u32(d+24),n=aq_u32(d+28);
  unsigned wi,wo,wk,ws=0,wp=0,wd=1;
  if(slot!=i*2)return -4;
  if(!i){wi=192;wo=160;wk=7;}
  else if(i==22){wi=20;wo=1;wk=7;}
  else {unsigned stage=(slot-2)/14,within=(slot-2)%14;const unsigned ci[]={160,80,40},co[]={80,40,20};
   if(!within){wi=ci[stage];wo=co[stage];ws=stage==2?4:8;wp=ws/2;wk=ws*2;wd=0;}
   else {unsigned br=(within-2)/4;const unsigned ks[]={3,5,7},d1[]={1,2,3},d2[]={2,6,12};wi=wo=co[stage];wk=ks[br];wd=((within-2)%4)?d2[br]:d1[br];}
  }
  if(ic!=wi || oc!=wo || k!=wk || s!=ws || pad!=wp || dil!=wd || n!=((ic*(s?k/s:k)+15)&~15u))return -5;
  const unsigned qoff=aq_u32(d+32),soff=aq_u32(d+36),boff=aq_u32(d+40),qw=aq_u32(d+44),sb=aq_u32(d+48),bb=aq_u32(d+52);
  if(qw!=(s?s:1)*((oc+7)&~7u)*n*2 || sb!=oc*4 || bb!=oc*4)return -6;
  if(qoff<1536 || soff<1536 || boff<1536 || (qoff&15) || (soff&15) || (boff&15) ||
     qoff>bytes || qw>bytes-qoff || soff>bytes || sb>bytes-soff || boff>bytes || bb>bytes-boff)return -7;
  for(unsigned o=0;o<oc;++o){const float scale=((const float *)(p+soff))[o],bias=((const float *)(p+boff))[o];int e;
   if(!isfinite(scale) || scale<=0 || frexpf(scale,&e)!=0.5f || !isfinite(bias))return -8;}
  m->w[slot]=(const float *)(p+qoff);m->w[slot+1]=(const float *)(p+boff);
 }
 packed_model=p;packed_bytes=bytes;return 0;
}
static const unsigned char *amy_packed_find(const float *w,int ic,int oc,int k,int stride,int pad,int dil){
 if(!packed_model || !packed_bytes)return NULL;
 for(unsigned i=0;i<23;++i){const unsigned char *d=packed_model+64+i*64;
  if((const float *)(packed_model+aq_u32(d+32))!=w)continue;
  if(aq_u32(d+4)!=(unsigned)ic || aq_u32(d+8)!=(unsigned)oc || aq_u32(d+12)!=(unsigned)k ||
     aq_u32(d+16)!=(unsigned)stride || aq_u32(d+20)!=(unsigned)pad || aq_u32(d+24)!=(unsigned)dil)return NULL;
  return d;
 }
 return NULL;
}
