#ifndef PD_APP_H
#define PD_APP_H
#include "pixel.h"
#include "files.h"
#include "animation.h"
#include "community.h"
#include "dropdown.h"
enum { PD_TOOLS=1,PD_PAN=2,PD_SAMPLE=4,PD_ZOOM=8,PD_ERASE=16,PD_SAVE=32,PD_MENU=64,PD_SHOULDER_L=128,PD_SHOULDER_R=256,PD_FRAME_PREV=512,PD_FRAME_NEXT=1024 };
/* Screen pixels. NDS builds store native 15-bit color with the display bit set, halving both screen
 * buffers; there a one-field struct makes any unconverted 32-bit write a compile error. */
#ifdef PD_FB16
typedef struct { uint16_t v; } PdFb;
static inline PdFb pd_fb(uint32_t c){PdFb p={(uint16_t)(0x8000u|((c>>19)&31u)|((c>>6)&992u)|((c<<7)&31744u))};return p;}
#else
typedef uint32_t PdFb;
static inline PdFb pd_fb(uint32_t c){return c;}
#endif
typedef struct PdApp PdApp;
struct PdApp {
    PdDocument doc;
    int width,height,zoom,ox,oy,page,confirm;
    int lastx,lasty,panx,pany,subx,suby;
    bool pan_frame;
    bool last_erase,touching,blocked,stroke,pan,redraw,exit_requested;
    /* Half-open world damage and screen presentation bounds. */
    int dx0,dy0,dx1,dy1,px0,py0,px1,py1;
    bool partial,present_top;
    unsigned input_us,render_us,present_us,gap_us,samples;
    /* Diagnostics: platforms supply a free-running tick counter and the largest allocation worth probing. */
    uint32_t (*clock_ticks)(void);
    uint32_t clock_hz;
    size_t memory_limit;
    unsigned free_kb,largest_kb,card_write_kbs,card_read_kbs;
    bool memory_measured,overview_pending;
    unsigned input_contacts,ink_starts;
    uint32_t overview_at;
    unsigned timeline_first,timeline_layer_first;
    PdPlayback playback;
    bool onion_enabled;
    unsigned char *onion;
    size_t onion_plane;
    uint64_t onion_key;
    uint64_t recovery_frames;
    char diag_status[48];
    uint32_t color,background,recent[12];
    unsigned recent_count,numeric_mode;
    char numeric[10];
    bool numeric_replace;
    unsigned text_cursor,text_mode; /* text_mode: 0 letters, 2 symbols, 3 hex */
    unsigned text_shift; /* 0 lowercase, 1 one capital, 2 caps lock */
    bool text_repeat;
    uint32_t text_repeat_at;
    unsigned hue,saturation,value,color_drag;
    unsigned color_tab,painted_hue;
    int painted_sx,painted_sy;
    bool picker_update,picker_field;
    uint32_t swatches[256];
    unsigned swatch_count,swatch_selected;
    int swatch_hold,swatch_target;
    uint32_t swatch_hold_ms,swatch_nav_ms;
    bool swatch_drag,palette_dirty;
    int palette_scroll[2],swatch_start_y,swatch_start_scroll;
    bool swatch_scrolling,palette_edit;
    unsigned palette_edit_index;
    uint32_t palette_edit_before;
    unsigned palette_density,palette_target;
    bool swatch_mini;
    bool palette_picker_active;
    uint32_t palette_picker_color,palette_picker_background;
    unsigned palette_picker_hue,palette_picker_saturation,palette_picker_value;
    char directory[256],notice[64];
    char file_source[PD_PATH];
    char folder[PD_PATH],project[PD_PATH],pending[PD_PATH],filename[64];
    uint32_t file_preview[64*64];
    bool preview_ready,preview_attempted,recent_files_unsaved;
    char preview_name[64],preview_error[64];
    char recent_projects[8][PD_PATH];
    unsigned recent_project_count;
    int recent_project_selected;
    PdListing listing;
    PdCommunityPage community_page;
    PdDropdown community_dropdown,layer_blend_dropdown,file_type_dropdown;
    char upload_title[121],upload_description[2001],upload_tags[257];
    char upload_snapshot[PD_PATH],upload_request_id[37];
    unsigned upload_size,upload_stage;
    bool upload_snapshot_ready,upload_busy,share_animation;
    unsigned community_mode;
    bool community_loading,community_loaded;
    bool community_action_busy;
    char community_status[64];
    uint32_t (*gallery_thumbs)[32*32]; /* six thumbnails, allocated on first gallery use */
    unsigned gallery_page,gallery_count;
    bool gallery_ready[6];
    unsigned file_mode,file_page,name_action,file_filter;
    int file_scroll,file_start_scroll,file_start_y,file_hold;
    bool file_scrolling;
    unsigned preview_palette_count;
    /* Shown beside the upper-screen file preview; zero when unknown. */
    unsigned preview_width,preview_height,preview_layers,preview_frames;
    long preview_bytes;
    unsigned new_background,new_layers,new_width,new_height,size_axis;
    bool new_current_palette;
    uint32_t recovery_at,idle_at,recovery_palette;
    uint64_t recovery_revision;
    bool recovery_clock,recovery_available,recovery_pending,recovery_enabled;
    int selected;
    bool project_native,zoom_overlay,toolbox,tool_options_open;
    unsigned tool_panel;
    unsigned help_tab;
    unsigned bindings,draft_bindings; /* four paired controls, two bits per action */
    PdClipboard *clipboard; /* allocated on first copy or cut */
    /* Masks hold mask_words words each, sized to the largest canvas the profile allows. */
    uint32_t *selection,*selection_base,*selection_draft;
    unsigned mask_words;
    int16_t lasso[512][2];
    unsigned lasso_count,selection_shape,selection_mode,selection_layer;
    uint64_t selection_revision;
    bool selection_valid,selection_drag,selection_move,lasso_overflow;
    int selection_x,selection_y,selection_dx,selection_dy,selection_offset_x,selection_offset_y;
    unsigned guides;
    uint32_t *guide_damage;
    PdPerfectStroke perfect_stroke;
    bool pixel_perfect;
    unsigned brush_size[2],brush_round[2],brush_edit;
    unsigned brush_pattern,brush_preset;
    unsigned char brush_presets[4][4]; /* size, round, pixel perfect, dither */
    bool pixel_grid,brush_drag,brush_dirty,presets_dirty;
    bool fill_global,fill_visible;
    unsigned fill_tolerance;
    unsigned fill_pattern,shape_pattern; /* 0 solid, 1/2/3 = 25/50/75% dither */
    int adjust_hue,adjust_saturation,adjust_brightness;
    unsigned adjust_scope,adjust_drag;
    bool adjust_preview_ready,adjust_preview_stale;
    bool shape_drag,shape_filled;
    int shape_x,shape_y,shape_lastx,shape_lasty;
    unsigned tool; /* 0 pencil, 1 eraser, 2 sampler, 3 fill, 4 selection, 5 line, 6 rectangle, 7 ellipse */
    unsigned handed,zoom_side,draft_handed,draft_zoom_side;
    uint32_t now_ms,layer_hold_ms;
    int layer_hold,layer_drag_y,layer_scroll,layer_start_scroll,layer_start_y;
    unsigned layer_previous;
    uint32_t layer_scroll_at;
    bool layer_scrolling;
    bool layer_popup,layer_delete,layer_drag,opacity_drag;
    const char *platform;
    void (*online_begin)(PdApp *app);
    void (*online_list)(PdApp *app,unsigned mode,unsigned offset);
    void (*online_favorite)(PdApp *app,unsigned item,bool favorite);
    bool (*online_submit)(PdApp *app,const char *path,const char *request_id,
                          const char *title,const char *description,
                          const char *tags,unsigned size);
    void (*online_tick)(PdApp *app);
#ifdef PD_PRIVATE_BUILD
    void (*share_current)(PdApp *app,bool visible);
    void (*share_tick)(PdApp *app);
#endif
    unsigned pairing_state;
    uint32_t pairing_poll_at;
    char pairing_user_code[10], pairing_message[64];
};
void app_init(PdApp *a,int w,int h,const char *directory,const char *platform);
void app_input(PdApp *a,bool touch,int x,int y,unsigned down,unsigned held);
unsigned app_bindings(const PdApp *a,unsigned actions);
void app_text_result(PdApp *a,const char *text,bool accepted);
void app_pan_pixels(PdApp *a,int dx,int dy);
void app_pan(PdApp *a,int dx,int dy);
void app_render(PdApp *a,PdFb *bottom,PdFb *top,int topw,int toph);
/* Sets the device profile and resizes masks to it; DS mode (profile 1) needs only 128 x 128. */
bool app_set_profile(PdApp *a,unsigned profile);
#ifdef PD_TEST
/* Centre of the Menu button for an action index, so tests follow the layout table. */
bool app_test_menu_point(const PdApp *a,int action,int *x,int *y);
#endif
bool app_reserve_gallery(PdApp *a);
void app_dispose(PdApp *a);
void pd_pairing_init(PdApp *a);
void pd_pairing_exit(void);
void pd_pairing_begin(PdApp *a);
void pd_pairing_tick(PdApp *a);
const char *pd_pairing_access_token(void);
#endif
