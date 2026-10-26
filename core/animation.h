#ifndef PD_ANIMATION_H
#define PD_ANIMATION_H
#include "pixel.h"
#include <stdio.h>
#define PD_FRAMES 256
#define PD_TAGS 32
#define PD_TAG_NAME_MAX 32
/* Aseprite-style tag: an inclusive frame range with a loop direction (0 forward, 1 reverse,
 * 2 ping-pong, 3 ping-pong reverse), repeat count (0 = forever, kept for interchange) and RGB colour. */
typedef struct { char name[PD_TAG_NAME_MAX]; unsigned from,to,direction,repeat; uint32_t color; } PdTag;
/* Playback loops the innermost tag containing the starting frame, else every frame. */
typedef struct { bool active,backward; unsigned origin,from,to,direction; uint32_t deadline; } PdPlayback;
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
/* Inserts a frame after the current one whose cels are all linked to the current frame's cels. */
bool pd_frame_add_linked(PdDocument *d);
/* Linked cels: cels of one layer with the same nonzero group share one image. Packing the
 * current frame copies each linked layer to the rest of its group. */
unsigned pd_cel_link(const PdDocument *d,unsigned frame,unsigned layer);
/* Links the current cel of a layer with the previous frame's cel; the current cel takes that
 * image. Returns false if nothing changed. *cleared reports that undo history had to be reset
 * because the current cel's pixels were replaced. */
bool pd_cel_link_previous(PdDocument *d,unsigned layer,bool *cleared);
bool pd_cel_unlink(PdDocument *d,unsigned layer);
/* Joins frame's cel on layer to the source frame's linked-cel group (creating one when needed).
 * The caller guarantees both cels hold the same image; used by linked-cel import. */
bool pd_cel_set_link(PdDocument *d,unsigned frame,unsigned layer,unsigned source);
/* Moves one cel's image to another layer and/or frame. Links on both cels are cleared and
 * paint undo history is cleared because the change is outside it. */
bool pd_cel_move(PdDocument *d,unsigned source_frame,unsigned source_layer,unsigned dest_frame,unsigned dest_layer);
/* Copies one frame/layer into out (a pd_plane stride plane); the working frame is read directly. */
bool pd_frame_layer_copy(const PdDocument *d,unsigned frame,unsigned layer,uint32_t *out);
/* Selects a frame already present in the packed store without snapshotting working pixels (import). */
bool pd_frame_select_packed(PdDocument *d,unsigned index);
void pd_frame_clear_links(PdDocument *d,unsigned layer);
unsigned pd_tag_count(const PdDocument *d);
const PdTag *pd_tag(const PdDocument *d,unsigned index);
/* Innermost (shortest) tag containing frame, or -1. */
int pd_tag_at(const PdDocument *d,unsigned frame);
/* Adds or replaces a tag after validating it; returns its index or -1. */
int pd_tag_add(PdDocument *d,const PdTag *tag);
bool pd_tag_set(PdDocument *d,unsigned index,const PdTag *tag);
bool pd_tag_delete(PdDocument *d,unsigned index);
bool pd_frame_delete(PdDocument *d);
bool pd_frame_move(PdDocument *d,unsigned target);
/* Moves every marked frame in one block so the block starts at final index start.
 * new_index (optional) receives each old frame index's new position. Frame order changes;
 * paint undo is remapped so it keeps targeting the same art, and no allocation is performed. */
bool pd_frames_move(PdDocument *d,const uint8_t *marked,unsigned start,uint16_t *new_index);
bool pd_frame_set_duration(PdDocument *d,unsigned milliseconds);
bool pd_frame_set_all_durations(PdDocument *d,unsigned milliseconds);
bool pd_frame_snapshot(PdDocument *d);
bool pd_frame_slot_unused(const PdDocument *d,unsigned layer);
bool pd_frame_has_cel(const PdDocument *d,unsigned frame,unsigned layer);
bool pd_frame_duplicate_layer_cels(PdDocument *d,unsigned source,unsigned target);
bool pd_frames_dirty(const PdDocument *d);
void pd_frames_saved(PdDocument *d);
bool pd_frame_alpha(const PdDocument *d,unsigned frame,unsigned char *out);
/* Read-only previews: Normal-blend composite of visible layers with opacity (blend modes ignored).
 * pd_frame_rgba fills a full stride plane; pd_frame_sample a w x h nearest-neighbour thumbnail. */
bool pd_frame_rgba(const PdDocument *d,unsigned frame,uint32_t *out);
bool pd_frame_sample(const PdDocument *d,unsigned frame,uint32_t *out,unsigned w,unsigned h);
void pd_rgba_over(uint32_t *dst,uint32_t src,unsigned opacity);
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
