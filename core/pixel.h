#ifndef PIXEL_DOODLE_H
#define PIXEL_DOODLE_H
#include <stdint.h>
#include <stdbool.h>
#define PD_MAX_SIDE 512
#define PD_MAX_PIXELS (512u*512u)
#define PD_SIDE 128
#define PD_PIXELS (PD_SIDE * PD_SIDE)
#define PD_LAYERS 8
#define PD_HISTORY 32
#define PD_HISTORY_BYTES (512u*1024u)
enum { PD_LAYER_LOCK=1, PD_LAYER_ALPHA_LOCK=2 };
/* Canonical straight-alpha 0xAARRGGBB. Layer 0 is bottommost. */
typedef struct { uint32_t pixels[PD_PIXELS]; uint32_t *large; uint8_t visible, opacity, blend, flags; } PdLayer;
typedef struct { unsigned offset,bytes,tiles; uint32_t group; uint8_t group_active,layer, visibility, opacity, enabled, blend, flags, order[PD_LAYERS]; char name[64]; uint64_t before, after; } PdEdit;
typedef struct {
    PdLayer layer[PD_LAYERS];
    unsigned width,height,stride,rows,capacity,profile;
    void *allocation;
    uint32_t *large_before,*large_composite;
    uint8_t *large_history;
    unsigned history_capacity;
    char names[PD_LAYERS][64];
    PdEdit history[PD_HISTORY];
    uint8_t history_data[PD_HISTORY_BYTES];
    unsigned history_used;
    uint32_t next_history_group,history_group;
    uint8_t history_group_active;
    uint32_t before[PD_PIXELS], composite[PD_PIXELS];
    unsigned active, count, cursor;
    uint8_t before_visible, before_opacity;
    uint8_t enabled,before_enabled;
    char before_name[64];
    uint8_t order[PD_LAYERS],before_order[PD_LAYERS],before_blend,before_flags;
    bool editing;
    uint64_t revision, serial, saved_revision;
    uint32_t save_sequence;
    uint32_t palette[256],saved_palette[256];
    unsigned palette_count,saved_palette_count;
} PdDocument;
/* Profiles: 0/1 NTR, 2 TWL, 3 3DS, 4 New 3DS with expanded application memory. Small backing remains allocation-free. */
static inline uint32_t *pd_pixels(const PdLayer *l){return l->large?l->large:(uint32_t *)l->pixels;}
static inline uint32_t *pd_before(const PdDocument *d){return d->large_before?d->large_before:(uint32_t *)d->before;}
static inline uint32_t *pd_image(const PdDocument *d){return d->large_composite?d->large_composite:(uint32_t *)d->composite;}
static inline unsigned pd_plane(const PdDocument *d){return d->stride*d->rows;}
static inline unsigned pd_bytes(const PdDocument *d){return pd_plane(d)*4;}
void pd_init(PdDocument *d);
void pd_dispose(PdDocument *d);
unsigned pd_capacity(unsigned width,unsigned height,unsigned profile);
bool pd_create(PdDocument *d,unsigned width,unsigned height,unsigned profile);
#ifdef PD_TEST
void pd_test_fail_allocation(bool fail);
unsigned pd_test_composites(void);
#endif

bool pd_add_layer(PdDocument *d);
bool pd_delete_layer(PdDocument *d);
bool pd_rename_layer(PdDocument *d,const char *name);
bool pd_duplicate_layer(PdDocument *d);
bool pd_merge_down(PdDocument *d);
bool pd_move_layer(PdDocument *d,unsigned position);
bool pd_begin(PdDocument *d);
bool pd_commit(PdDocument *d);
void pd_cancel(PdDocument *d);
/* Guides: bit 0 mirror X, bit 1 mirror Y, bit 2 wrap X, bit 3 wrap Y. */
typedef struct {int ax,ay,bx,by;unsigned points,saved_count;unsigned index[4];uint32_t old[4];} PdPerfectStroke;
void pd_perfect_line(PdDocument *d,PdPerfectStroke *state,int x0,int y0,int x1,int y1,uint32_t color,unsigned guides,uint32_t *damage,const uint32_t *mask);
/* Canvas-anchored ordered dither: 0 solid, 1/2/3 = 25/50/75%; gaps preserve destination. */
void pd_pattern_line(PdDocument *d,int x0,int y0,int x1,int y1,uint32_t color,unsigned size,bool round,unsigned guides,uint32_t *damage,const uint32_t *mask,unsigned pattern);
void pd_masked_line(PdDocument *d,int x0,int y0,int x1,int y1,uint32_t color,unsigned size,bool round,unsigned guides,uint32_t *damage,const uint32_t *mask);
void pd_guided_line_damage(PdDocument *d,int x0,int y0,int x1,int y1,uint32_t color,unsigned size,bool round,unsigned guides,uint32_t *damage);
void pd_guided_line(PdDocument *d,int x0,int y0,int x1,int y1,uint32_t color,unsigned size,bool round,unsigned guides);
void pd_brush_line(PdDocument *d,int x0,int y0,int x1,int y1,uint32_t color,unsigned size,bool round);
void pd_line(PdDocument *d, int x0, int y0, int x1, int y1, uint32_t color);
/* Exact RGBA, four-connected active-layer fill. Owns one undo transaction. */
/* Shape kind: 0 line, 1 rectangle, 2 ellipse. Caller owns transaction. */
void pd_shape(PdDocument *d,int x0,int y0,int x1,int y1,unsigned kind,bool filled,uint32_t color,unsigned size,bool round,unsigned guides,uint32_t *damage,const uint32_t *mask);
bool pd_duplicate_selection(PdDocument *d,const uint32_t *mask);
bool pd_scale_selection(PdDocument *d,const uint32_t *mask,int factor,uint32_t *result);
bool pd_transform_selection(PdDocument *d,const uint32_t *mask,unsigned operation,uint32_t *result);
bool pd_fill_options(PdDocument *d,int x,int y,uint32_t color,unsigned guides,const uint32_t *mask,bool global,unsigned tolerance,bool visible);
bool pd_fill_mask(PdDocument *d,int x,int y,uint32_t color,unsigned guides,const uint32_t *mask);
bool pd_fill(PdDocument *d,int x,int y,uint32_t color,unsigned guides);
typedef struct {uint32_t pixels[PD_PIXELS],mask[PD_PIXELS/32];unsigned width,height;} PdClipboard;
bool pd_mask_bounds_stride(const uint32_t *mask,unsigned width,unsigned height,unsigned stride,int *x,int *y,int *w,int *h);
void pd_mask_rectangle_stride(uint32_t *mask,unsigned stride,unsigned rows,int x0,int y0,int x1,int y1);
void pd_mask_polygon_stride(uint32_t *mask,const int16_t points[][2],unsigned count,unsigned width,unsigned height,unsigned stride);
bool pd_mask_bounds(const uint32_t *mask,unsigned width,unsigned height,int *x,int *y,int *w,int *h);
bool pd_copy_selection(const PdDocument *d,const uint32_t *mask,PdClipboard *clip);
bool pd_paste_selection(PdDocument *d,const PdClipboard *clip,int x,int y);
/* Preview move/delete within an already-open transaction, always from before. */
bool pd_move_selection(PdDocument *d,const uint32_t *mask,int dx,int dy,bool erase);
void pd_mask_rectangle(uint32_t *mask,int x0,int y0,int x1,int y1);
void pd_mask_polygon(uint32_t *mask,const int16_t points[][2],unsigned count,unsigned width,unsigned height);
bool pd_undo(PdDocument *d);
bool pd_redo(PdDocument *d);
/* Scope: 0 selection on active layer, 1 active layer, 2 all visible layers. */
uint32_t pd_adjust_color(uint32_t color,int hue_shift,int saturation_shift,int brightness_shift);
bool pd_adjust_layers(PdDocument *d,unsigned scope,const uint32_t *mask,int hue_shift,int saturation_shift,int brightness_shift);
bool pd_adjust_preview(PdDocument *d,unsigned scope,const uint32_t *mask,int hue_shift,int saturation_shift,int brightness_shift);
uint32_t pd_sample_layers(const PdLayer layers[PD_LAYERS],uint8_t enabled,const uint8_t order[PD_LAYERS],unsigned index);
void pd_composite(PdDocument *d);
void pd_composite_rect(PdDocument *d,int x0,int y0,int x1,int y1);
bool pd_dirty(const PdDocument *d);
/* Directory must exist. Returns 1=loaded, 0=no files, -1=invalid/I/O. */
int pd_load(PdDocument *d, const char *directory);
bool pd_save(PdDocument *d, const char *directory);
#endif
