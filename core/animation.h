#ifndef PD_ANIMATION_H
#define PD_ANIMATION_H
#include "pixel.h"
#include <stdio.h>
#define PD_FRAMES 256
typedef struct { bool active; unsigned origin; uint32_t deadline; } PdPlayback;
bool pd_playback_start(PdDocument *d,PdPlayback *p,uint32_t now);
bool pd_playback_tick(PdDocument *d,PdPlayback *p,uint32_t now);
bool pd_playback_seek(PdDocument *d,PdPlayback *p,unsigned frame);
void pd_playback_stop(PdDocument *d,PdPlayback *p);
unsigned pd_frame_count(const PdDocument *d);
unsigned pd_frame_index(const PdDocument *d);
unsigned pd_frame_duration(const PdDocument *d);
size_t pd_frame_memory(const PdDocument *d);
size_t pd_frame_budget(const PdDocument *d);
bool pd_frame_switch(PdDocument *d,unsigned index);
bool pd_frame_add(PdDocument *d,bool duplicate);
bool pd_frame_delete(PdDocument *d);
bool pd_frame_set_duration(PdDocument *d,unsigned milliseconds);
bool pd_frame_snapshot(PdDocument *d);
bool pd_frame_slot_unused(const PdDocument *d,unsigned layer);
bool pd_frame_has_cel(const PdDocument *d,unsigned frame,unsigned layer);
bool pd_frame_duplicate_layer_cels(PdDocument *d,unsigned source,unsigned target);
bool pd_frames_dirty(const PdDocument *d);
void pd_frames_saved(PdDocument *d);
bool pd_frame_alpha(const PdDocument *d,unsigned frame,unsigned char *out);
uint64_t pd_frames_visual_generation(const PdDocument *d);
uint64_t pd_frames_generation(const PdDocument *d);
void pd_frames_dispose(PdDocument *d);
/* Streaming PDDA/version 10; the caller owns atomic replacement and fsync. */
bool pd_frames_write(PdDocument *d,FILE *f);
bool pd_frames_read(PdDocument *d,FILE *f);
bool pd_frames_verify(FILE *f);
#ifdef PD_TEST
void pd_frames_test_fail_after(int allocations);
bool pd_frames_test_budget(PdDocument *d,size_t bytes);
#endif
#endif
