#include "amy_stream.h"
#include <math.h>
#include <string.h>
static int reuse_small=1;
int amy_small_reuse_enabled(void){return reuse_small;}
void amy_small_set_reuse(int enabled){reuse_small=!!enabled;}
static int supported(const snt_piperlite_model *m,int frames,int chunk) {
    return m && m->in_ch==192 &&
        ((m->c0==192 && m->c1==96 && m->c2==48 && m->c3==24) ||
         (m->c0==160 && m->c1==80 && m->c2==40 && m->c3==20)) &&
        m->pf_channels==0 && m->pf_layers==0 && m->n_tensors==46 &&
        frames>0 && frames<=AMY_STREAM_MAX_FRAMES && chunk>=1 && chunk<=64;
}
static int window_size(int frames,int chunk) {
    const int max_window=chunk+2*AMY_STREAM_HALO;
    return frames<max_window?frames:max_window;
}
size_t amy_stream_scratch_floats(const snt_piperlite_model *m,int frames,int chunk) {
    if(!supported(m,frames,chunk))return 0;
    const int window=window_size(frames,chunk);
    return (size_t)(m->in_ch+SNT_PIPERLITE_HOP)*window+snt_piperlite_arena_floats(m,window);
}
int amy_stream_decode(const snt_piperlite_model *m,const float *latent,
    int frames,int chunk,float *scratch,size_t scratch_floats,
    amy_stream_sink sink,amy_stream_cancel cancel,void *user) {
    const size_t need=amy_stream_scratch_floats(m,frames,chunk);
    if(!need || !latent || !scratch || !sink || !cancel)return -1;
    if(scratch_floats<need)return -2;
    const int max_window=window_size(frames,chunk);
    float *z=scratch,*audio=z+(size_t)m->in_ch*max_window;
    float *arena=audio+(size_t)SNT_PIPERLITE_HOP*max_window;
    const size_t arena_floats=need-(size_t)(m->in_ch+SNT_PIPERLITE_HOP)*max_window;
    int start;
    for(start=0;start<frames;start+=chunk) {
        if(cancel(user))return 1;
        const int count=frames-start<chunk?frames-start:chunk;
        const int left=start>AMY_STREAM_HALO?start-AMY_STREAM_HALO:0;
        const int right=start+count+AMY_STREAM_HALO<frames?start+count+AMY_STREAM_HALO:frames;
        const int window=right-left;
        int ch,i;
        for(ch=0;ch<m->in_ch;++ch) {
            const float *row=latent+(size_t)ch*frames+left;
            for(i=0;i<window;++i)if(!isfinite(row[i]))return -4;
            memcpy(z+(size_t)ch*window,row,(size_t)window*sizeof(float));
        }
        if(snt_piperlite_synthesize(m,z,window,audio,arena,arena_floats)!=0)return -3;
        float *valid=audio+(size_t)(start-left)*SNT_PIPERLITE_HOP;
        const int samples=count*SNT_PIPERLITE_HOP;
        for(i=0;i<samples;++i)if(!isfinite(valid[i]))return -4;
        if(cancel(user) || sink(valid,samples,user))return 1;
    }
    return 0;
}
