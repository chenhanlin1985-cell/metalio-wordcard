#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define WORDCARD_QUALITY_REGIONS_MAX 16
typedef struct {int x,y,width,height;} WordcardQualityRect;
/* Configure between scans. Coordinates are native framebuffer pixels.
 * The quality and text tables must have the same phase count. */
bool wordcard_quality_set(const WordcardQualityRect*,unsigned,const uint8_t*,unsigned,int,int);
void wordcard_quality_clear(void);
bool wordcard_quality_changed(WordcardQualityRect,const uint8_t*,const uint8_t*,int,int);
void wordcard_quality_mark(bool*,uint8_t*,int,int);
/* Call while feeders are idle; prepares the immutable SRAM LUT for a phase. */
void wordcard_quality_prepare_phase(int phase);
void wordcard_quality_patch(int row,int phase,const uint8_t *difference,uint8_t *drive);
typedef void (*WordcardLineLookup)(const uint32_t*,uint8_t*,const uint8_t*,uint32_t);
/* A row with quality regions is converted once, selecting a prebuilt LUT per
 * span. False leaves the output untouched for the normal converter. */
bool wordcard_quality_render(int row,int phase,const uint8_t *difference,uint8_t *drive,
                            const uint8_t *normal_lut,WordcardLineLookup lookup);
#ifdef __cplusplus
}
#endif
