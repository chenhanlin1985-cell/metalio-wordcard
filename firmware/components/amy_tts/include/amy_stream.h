#ifndef WORDCARD_AMY_STREAM_H
#define WORDCARD_AMY_STREAM_H
#include "snt_piperlite.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Experimental exact-context decoder wrapper for the pinned Amy architecture.
 * All memory belongs to the caller. No allocation, resampling or quantization.
 * 12 neighbouring latent frames cover the graph's 11-frame dependency radius.
 * The full-utterance front end is intentionally retained to preserve prosody. */
#define AMY_STREAM_HALO 12
#define AMY_STREAM_MAX_FRAMES 1033
typedef int (*amy_stream_sink)(const float *pcm,int count,void *user);
typedef int (*amy_stream_cancel)(void *user);
size_t amy_reuse_scratch_floats(const snt_piperlite_model *,int frames,int chunk);
int amy_reuse_decode(const snt_piperlite_model *,const float *,int,int,float *,size_t,amy_stream_sink,amy_stream_cancel,void *);
int amy_small_reuse_enabled(void);
void amy_small_set_reuse(int enabled);
size_t amy_stream_scratch_floats(const snt_piperlite_model *model,int frames,int chunk_frames);
/* Returns 0 after all samples; 1 when cancelled/rejected by sink; negative on
 * invalid arguments, insufficient scratch, decoder error or nonfinite output.
 * Partial output must never be committed as a completed cache entry. */
int amy_stream_decode(const snt_piperlite_model *model,const float *latent,
    int frames,int chunk_frames,float *scratch,size_t scratch_floats,
    amy_stream_sink sink,amy_stream_cancel cancel,void *user);
#ifdef __cplusplus
}
#endif
#endif
