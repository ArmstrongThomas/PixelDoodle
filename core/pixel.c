#include "pixel.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void pd_init(PdDocument *d) {
    memset(d, 0, sizeof(*d));
    d->enabled=1;d->width=d->height=d->stride=d->rows=128;d->capacity=8;d->history_capacity=PD_HISTORY_BYTES;
    for (unsigned i=0;i<PD_LAYERS;i++) { d->order[i]=(uint8_t)i;d->layer[i].visible=1; d->layer[i].opacity=255; snprintf(d->names[i],64,"Layer %u",i+1); }
}
void pd_dispose(PdDocument *d){free(d->allocation);d->allocation=NULL;}
unsigned pd_capacity(unsigned width,unsigned height,unsigned profile){
    if(!width||!height||width>(profile>=2?512u:128u)||height>(profile>=2?512u:128u))return 0;
    if(width<=128&&height<=128)return 8;
    unsigned bytes=((width+7)/8*8)*((height+7)/8*8)*4;
    unsigned cap=profile==2?(2u*1024u*1024u)/bytes:8;if(cap>8)cap=8;return cap;
}
#ifdef PD_TEST
static bool fail_allocation;
static unsigned composite_calls;
unsigned pd_test_composites(void){return composite_calls;}
void pd_test_fail_allocation(bool fail){fail_allocation=fail;}
#endif
bool pd_create(PdDocument *d,unsigned width,unsigned height,unsigned profile){
    unsigned capacity=pd_capacity(width,height,profile);if(!capacity||d->editing)return false;
    unsigned stride=128,rows=128,bytes=0,history=PD_HISTORY_BYTES;uint8_t *block=NULL;
    if(width>128||height>128){stride=(width+7)/8*8;rows=(height+7)/8*8;bytes=stride*rows*4;history=bytes*(profile==4?8u:2u);if(history<PD_HISTORY_BYTES)history=PD_HISTORY_BYTES;
        #ifdef PD_TEST
        if(fail_allocation)return false;
        #endif
        block=calloc(1,(capacity+2)*bytes+history);if(!block&&profile==4){history=bytes*2;if(history<PD_HISTORY_BYTES)history=PD_HISTORY_BYTES;block=calloc(1,(capacity+2)*bytes+history);}if(!block)return false;}
    else if(profile==4){history=2u*1024u*1024u;block=calloc(1,history);if(!block)history=PD_HISTORY_BYTES;}
    pd_dispose(d);pd_init(d);d->profile=profile;d->width=width;d->height=height;d->stride=stride;d->rows=rows;d->capacity=capacity;d->history_capacity=history;d->allocation=block;
    if(block&&bytes){for(unsigned i=0;i<capacity;i++)d->layer[i].large=(uint32_t *)(block+i*bytes);d->large_before=(uint32_t *)(block+capacity*bytes);d->large_composite=(uint32_t *)(block+(capacity+1)*bytes);}if(block)d->large_history=block+(capacity+2)*bytes;return true;
}
static uint8_t *history_data(PdDocument *d){return d->large_history?d->large_history:d->history_data;}
bool pd_dirty(const PdDocument *d) { return d->revision != d->saved_revision || d->palette_count != d->saved_palette_count || memcmp(d->palette,d->saved_palette,d->palette_count*sizeof(uint32_t)); }
bool pd_begin(PdDocument *d) {
    if(d->editing || d->active>=d->capacity) return false;
    memcpy(pd_before(d),pd_pixels(&d->layer[d->active]),pd_bytes(d));
    d->before_visible=d->layer[d->active].visible;
    d->before_opacity=d->layer[d->active].opacity;
    d->before_enabled=d->enabled;memcpy(d->before_name,d->names[d->active],64);
    d->before_flags=d->layer[d->active].flags;d->before_blend=d->layer[d->active].blend;memcpy(d->before_order,d->order,PD_LAYERS);
    d->editing=true; return true;
}
void pd_cancel(PdDocument *d) {
    if(!d->editing) return;
    memcpy(pd_pixels(&d->layer[d->active]),pd_before(d),pd_bytes(d));
    d->layer[d->active].visible=d->before_visible;
    d->layer[d->active].opacity=d->before_opacity;
    d->enabled=d->before_enabled;memcpy(d->names[d->active],d->before_name,64);
    d->layer[d->active].flags=d->before_flags;d->layer[d->active].blend=d->before_blend;memcpy(d->order,d->before_order,PD_LAYERS);
    d->editing=false;
}
bool pd_commit(PdDocument *d) {
    if(!d->editing) return false;
    PdLayer *l=&d->layer[d->active]; d->editing=false;
    if(l->flags==d->before_flags&&l->blend==d->before_blend&&!memcmp(d->order,d->before_order,PD_LAYERS)&&d->enabled==d->before_enabled&&!memcmp(d->before_name,d->names[d->active],64)&&l->visible==d->before_visible && l->opacity==d->before_opacity &&
       !memcmp(pd_pixels(l),pd_before(d),pd_bytes(d))) return false;
    /* Pack only changed 8x8 tiles; dense edits retain the old 64 KiB cost. */
    uint32_t changed[128]={0};unsigned tiles=0,cols=d->stride/8;
    for(unsigned ty=0;ty<d->rows/8;ty++)for(unsigned tx=0;tx<cols;tx++){
        bool different=false;for(unsigned y=0;y<8&&!different;y++)
            different=memcmp(pd_before(d)+(ty*8+y)*d->stride+tx*8,pd_pixels(l)+(ty*8+y)*d->stride+tx*8,8*4)!=0;
        if(different){unsigned id=ty*cols+tx;changed[id/32]|=1u<<(id%32);tiles++;}
    }
    unsigned bytes=tiles*260u;bool dense=bytes>=pd_bytes(d);if(dense)bytes=pd_bytes(d);
    d->count=d->cursor;d->history_used=d->count?d->history[d->count-1].offset+d->history[d->count-1].bytes:0;
    while(d->count&&(d->count==PD_HISTORY||d->history_used+bytes>d->history_capacity)){
        unsigned removed=d->history[0].bytes;
        memmove(history_data(d),history_data(d)+removed,d->history_used-removed);d->history_used-=removed;
        memmove(d->history,d->history+1,(d->count-1)*sizeof(PdEdit));d->count--;d->cursor--;
        for(unsigned i=0;i<d->count;i++)d->history[i].offset-=removed;
    }
    PdEdit *e=&d->history[d->count];e->offset=d->history_used;e->bytes=bytes;e->tiles=dense?UINT32_MAX:tiles;
    unsigned char *out=history_data(d)+e->offset;
    if(dense){for(unsigned i=0;i<pd_plane(d);i++){uint32_t delta=pd_before(d)[i]^pd_pixels(l)[i];memcpy(out+i*4,&delta,4);}}
    else for(unsigned tile=0;tile<cols*d->rows/8;tile++){if(!(changed[tile/32]&(1u<<(tile%32))))continue;uint32_t id=tile;memcpy(out,&id,4);out+=4;
        for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){unsigned i=(tile/(d->stride/8)*8+y)*d->stride+tile%(d->stride/8)*8+x;uint32_t delta=pd_before(d)[i]^pd_pixels(l)[i];memcpy(out,&delta,4);out+=4;}}
    d->history_used+=bytes;
    e->layer=(uint8_t)d->active;
    e->visibility=d->before_visible^l->visible; e->opacity=d->before_opacity^l->opacity;
    e->enabled=d->before_enabled^d->enabled;for(unsigned i=0;i<64;i++)e->name[i]=d->before_name[i]^d->names[d->active][i];
    e->flags=d->before_flags^l->flags;e->blend=d->before_blend^l->blend;for(unsigned i=0;i<PD_LAYERS;i++)e->order[i]=d->order[i]^d->before_order[i];
    e->before=d->revision; e->after=++d->serial; d->revision=e->after;
    d->cursor=++d->count; return true;
}
static void toggle(PdDocument *d,const PdEdit *e) {
    PdLayer *l=&d->layer[e->layer];
    const unsigned char *in=history_data(d)+e->offset;
    if(e->tiles==UINT32_MAX){for(unsigned i=0;i<pd_plane(d);i++){uint32_t delta;memcpy(&delta,in+i*4,4);pd_pixels(l)[i]^=delta;}}
    else for(unsigned t=0;t<e->tiles;t++){uint32_t tile;memcpy(&tile,in,4);in+=4;
        for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){uint32_t delta;memcpy(&delta,in,4);in+=4;pd_pixels(l)[(tile/(d->stride/8)*8+y)*d->stride+tile%(d->stride/8)*8+x]^=delta;}}
    l->visible^=e->visibility; l->opacity^=e->opacity; d->active=e->layer;
    d->enabled^=e->enabled;for(unsigned i=0;i<64;i++)d->names[e->layer][i]^=e->name[i];
    l->flags^=e->flags;l->blend^=e->blend;for(unsigned i=0;i<PD_LAYERS;i++)d->order[i]^=e->order[i];
    if(!(d->enabled&(1u<<d->active)))for(unsigned i=0;i<PD_LAYERS;i++)if(d->enabled&(1u<<i)){d->active=i;break;}
}
/* Stable slots retain undo data; order is a separate bottom-to-top permutation. */
static void move_slot(PdDocument *d,unsigned slot,unsigned position){
    unsigned from=0;while(d->order[from]!=slot)from++;
    if(from<position)memmove(d->order+from,d->order+from+1,position-from);
    else if(from>position)memmove(d->order+position+1,d->order+position,from-position);
    d->order[position]=(uint8_t)slot;
}
bool pd_move_layer(PdDocument *d,unsigned position){
    if(position>=PD_LAYERS||!pd_begin(d))return false;move_slot(d,d->active,position);return pd_commit(d);
}
bool pd_duplicate_layer(PdDocument *d){
    if(d->editing||d->enabled==((1u<<d->capacity)-1))return false;unsigned source=d->active;
    for(unsigned i=0;i<d->capacity;i++)if(!(d->enabled&(1u<<i))){
        d->active=i;if(!pd_begin(d))return false;d->enabled|=(uint8_t)(1u<<i);memcpy(pd_pixels(&d->layer[i]),pd_pixels(&d->layer[source]),pd_bytes(d));d->layer[i].visible=d->layer[source].visible;d->layer[i].opacity=d->layer[source].opacity;d->layer[i].blend=d->layer[source].blend;d->layer[i].flags=d->layer[source].flags;
        char name[64];snprintf(name,sizeof(name),"%.58s COPY",d->names[source]);memset(d->names[i],0,64);memcpy(d->names[i],name,strlen(name)+1);
        move_slot(d,i,PD_LAYERS-1);unsigned pos=0;while(d->order[pos]!=source)pos++;move_slot(d,i,pos+1);return pd_commit(d);
    }return false;
}
bool pd_add_layer(PdDocument *d){
    if(d->editing||d->enabled==((1u<<d->capacity)-1))return false;
    for(unsigned i=0;i<d->capacity;i++)if(!(d->enabled&(1u<<i))){
        d->active=i;if(!pd_begin(d))return false;d->enabled|=(uint8_t)(1u<<i);
        memset(pd_pixels(&d->layer[i]),0,pd_bytes(d));d->layer[i].blend=d->layer[i].flags=0;d->layer[i].visible=1;d->layer[i].opacity=255;
        memset(d->names[i],0,64);snprintf(d->names[i],64,"Layer %u",i+1);move_slot(d,i,PD_LAYERS-1);return pd_commit(d);
    }return false;
}
bool pd_delete_layer(PdDocument *d){
    if(d->editing||(d->layer[d->active].flags&PD_LAYER_LOCK)||!(d->enabled&(1u<<d->active))||!(d->enabled&(d->enabled-1)))return false;
    if(!pd_begin(d))return false;d->enabled&=(uint8_t)~(1u<<d->active);
    memset(pd_pixels(&d->layer[d->active]),0,pd_bytes(d));bool ok=pd_commit(d);
    for(unsigned i=0;i<PD_LAYERS;i++)if(d->enabled&(1u<<i)){d->active=i;break;}return ok;
}
bool pd_rename_layer(PdDocument *d,const char *name){
    if(!name||!name[0]||strlen(name)>=64||!(d->enabled&(1u<<d->active))||!pd_begin(d))return false;
    memset(d->names[d->active],0,64);snprintf(d->names[d->active],64,"%s",name);return pd_commit(d);
}
bool pd_undo(PdDocument *d) {
    if(d->editing || !d->cursor) return false;
    const PdEdit *e=&d->history[--d->cursor]; toggle(d,e); d->revision=e->before; return true;
}
bool pd_redo(PdDocument *d) {
    if(d->editing || d->cursor==d->count) return false;
    const PdEdit *e=&d->history[d->cursor++]; toggle(d,e); d->revision=e->after; return true;
}
void pd_pattern_line(PdDocument *d,int x0,int y0,int x1,int y1,uint32_t color,unsigned size,bool round,unsigned guides,uint32_t *damage,const uint32_t *mask,unsigned pattern) {
    if(pattern>3)return;
    if(size<1||size>16)return;
    if(!d->editing || !(d->enabled&(1u<<d->active)) || !d->layer[d->active].visible || (d->layer[d->active].flags&PD_LAYER_LOCK)) return;
    /* Public API bounds both endpoints, so malformed coordinates cannot cause huge loops. */
    if(x0 < -4096||x0>4096||x1 < -4096||x1>4096||y0 < -4096||y0>4096||y1 < -4096||y1>4096)return;
    if(!(guides&4)&&(x0<0||x0>=(int)d->width||x1<0||x1>=(int)d->width))return;
    if(!(guides&8)&&(y0<0||y0>=(int)d->height||y1<0||y1>=(int)d->height))return;
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    if(!(color>>24)) color=0;
    for(;;) {
        int low=-(int)(size/2);
        for(unsigned by=0;by<size;by++)for(unsigned bx=0;bx<size;bx++){
            int px=x0+low+(int)bx,py=y0+low+(int)by;
            if((guides&4)&&(px<0||px>=(int)d->width))px=(px%(int)d->width+(int)d->width)%(int)d->width;
            if((guides&8)&&(py<0||py>=(int)d->height))py=(py%(int)d->height+(int)d->height)%(int)d->height;
            if(px<0||py<0||px>=(int)d->width||py>=(int)d->height)continue;
            int cx=(int)bx*2-(int)size+1,cy=(int)by*2-(int)size+1;if(round&&cx*cx+cy*cy>(int)(size*size))continue;
            for(unsigned mirror=0;mirror<=(guides&3);mirror++){if(mirror&~guides)continue;
            unsigned targetx=(unsigned)((mirror&1)?(int)d->width-1-px:px),targety=(unsigned)((mirror&2)?(int)d->height-1-py:py);
            if(pattern){unsigned rank=((targetx&1u)<<1)^((targety&1u)*3u);if(rank>=pattern)continue;}
            unsigned index=targety*d->stride+targetx;if(mask&&!(mask[index/32]&(1u<<(index%32))))continue;uint32_t *pixel=&pd_pixels(&d->layer[d->active])[index],old=*pixel;
            if(d->layer[d->active].flags&PD_LAYER_ALPHA_LOCK){unsigned a=color>>24;uint32_t original=pd_before(d)[index];if((original>>24)&&a){uint32_t rgb=0;for(unsigned shift=0;shift<24;shift+=8)rgb|=((((color>>shift)&255)*a+((original>>shift)&255)*(255-a)+127)/255)<<shift;*pixel=(original&0xff000000u)|rgb;}}
            else *pixel=color;
            if(damage&&old!=*pixel)damage[index/32]|=1u<<(index%32);
            }
        }
        if(x0==x1&&y0==y1) break;
        int twice=2*err;
        if(twice>=dy){err+=dy;x0+=sx;}
        if(twice<=dx){err+=dx;y0+=sy;}
    }
}
void pd_masked_line(PdDocument *d,int x0,int y0,int x1,int y1,uint32_t color,unsigned size,bool round,unsigned guides,uint32_t *damage,const uint32_t *mask){pd_pattern_line(d,x0,y0,x1,y1,color,size,round,guides,damage,mask,0);}
void pd_guided_line_damage(PdDocument *d,int x0,int y0,int x1,int y1,uint32_t color,unsigned size,bool round,unsigned guides,uint32_t *damage){pd_masked_line(d,x0,y0,x1,y1,color,size,round,guides,damage,NULL);}
void pd_guided_line(PdDocument *d,int x0,int y0,int x1,int y1,uint32_t color,unsigned size,bool round,unsigned guides){pd_guided_line_damage(d,x0,y0,x1,y1,color,size,round,guides,NULL);}
static bool fill_matches(uint32_t pixel,uint32_t target,unsigned tolerance){
    if(!(pixel>>24))pixel=0;if(!(target>>24))target=0;
    for(unsigned shift=0;shift<32;shift+=8){int difference=(int)((pixel>>shift)&255)-(int)((target>>shift)&255);if(abs(difference)>(int)tolerance)return false;}return true;
}
bool pd_fill_options(PdDocument *d,int x,int y,uint32_t color,unsigned guides,const uint32_t *mask,bool global,unsigned tolerance,bool visible){
    if(tolerance>255)return false;
    if(d->editing||d->active>=d->capacity||!(d->enabled&(1u<<d->active))||!d->layer[d->active].visible||(d->layer[d->active].flags&PD_LAYER_LOCK))return false;
    if(x<0||y<0||x>=(int)d->width||y>=(int)d->height)return false;
    /* Bounded iterative queue; each pixel is queued once, never recursive. */
    uint32_t *work=calloc(1,pd_plane(d)*4+pd_plane(d)/8);if(!work)return false;uint8_t *seen=(uint8_t *)(work+pd_plane(d));
    if(visible)pd_composite(d);if(!pd_begin(d)){free(work);return false;}const uint32_t *source=visible?pd_image(d):pd_before(d);if(!(color>>24))color=0;
    for(unsigned mirror=0;mirror<=(guides&3);mirror++){if(mirror&~guides)continue;
        unsigned seed=(unsigned)(((mirror&2)?(int)d->height-1-y:y)*d->stride+((mirror&1)?(int)d->width-1-x:x));
        if(mask&&!(mask[seed/32]&(1u<<(seed%32))))continue;
        memset(seen,0,pd_plane(d)/8);
        uint32_t target=source[seed];unsigned head=0,tail=0;
        if(global){for(unsigned yy=0;yy<d->height;yy++)for(unsigned xx=0;xx<d->width;xx++){unsigned i=yy*d->stride+xx;if(mask&&!(mask[i/32]&(1u<<(i%32))))continue;if(fill_matches(source[i],target,tolerance))work[tail++]=i;}}
        else{work[tail++]=seed;seen[seed/8]|=(uint8_t)(1u<<(seed%8));}
        while(head<tail){unsigned index=work[head++];uint32_t original=pd_before(d)[index],value=color;
            if(d->layer[d->active].flags&PD_LAYER_ALPHA_LOCK){unsigned alpha=color>>24;value=original;if((original>>24)&&alpha){uint32_t rgb=0;for(unsigned shift=0;shift<24;shift+=8)rgb|=((((color>>shift)&255)*alpha+((original>>shift)&255)*(255-alpha)+127)/255)<<shift;value=(original&0xff000000u)|rgb;}}
            pd_pixels(&d->layer[d->active])[index]=value;
            if(global)continue;
            int px=(int)(index%d->stride),py=(int)(index/d->stride);int nx[4]={px-1,px+1,px,px},ny[4]={py,py,py-1,py+1};
            for(unsigned n=0;n<4;n++){if(nx[n]<0){if(!(guides&4))continue;nx[n]=(int)d->width-1;}if(nx[n]>=(int)d->width){if(!(guides&4))continue;nx[n]=0;}if(ny[n]<0){if(!(guides&8))continue;ny[n]=(int)d->height-1;}if(ny[n]>=(int)d->height){if(!(guides&8))continue;ny[n]=0;}
                unsigned next=(unsigned)(ny[n]*d->stride+nx[n]);if(mask&&!(mask[next/32]&(1u<<(next%32))))continue;if((seen[next/8]&(1u<<(next%8)))||!fill_matches(source[next],target,tolerance))continue;seen[next/8]|=(uint8_t)(1u<<(next%8));work[tail++]=next;
            }
        }
    }
    free(work);return pd_commit(d);
}
bool pd_fill_mask(PdDocument *d,int x,int y,uint32_t color,unsigned guides,const uint32_t *mask){return pd_fill_options(d,x,y,color,guides,mask,false,0,false);}
bool pd_fill(PdDocument *d,int x,int y,uint32_t color,unsigned guides){return pd_fill_mask(d,x,y,color,guides,NULL);}
void pd_brush_line(PdDocument *d,int x0,int y0,int x1,int y1,uint32_t color,unsigned size,bool round){pd_guided_line(d,x0,y0,x1,y1,color,size,round,0);}
void pd_line(PdDocument *d,int x0,int y0,int x1,int y1,uint32_t color){pd_brush_line(d,x0,y0,x1,y1,color,1,false);}
#if defined(__GNUC__)
__attribute__((always_inline))
#endif
static inline uint32_t sample_layers(const PdLayer layers[PD_LAYERS],uint8_t enabled,const uint8_t order[PD_LAYERS],unsigned i){
        unsigned a=0,r=0,g=0,b=0;
        for(unsigned k=0;k<PD_LAYERS;k++) {unsigned j=order[k];
            const PdLayer *l=&layers[j]; if(!(enabled&(1u<<j))||!l->visible)continue;
            uint32_t p=pd_pixels(l)[i]; unsigned sa=((p>>24)*l->opacity+127)/255;
            if(!sa)continue;
            if(l->blend&&a){
                unsigned back[3]={(r+a/2)/a,(g+a/2)/a,(b+a/2)/a},src[3]={(p>>16)&255,(p>>8)&255,p&255};
                for(unsigned c=0;c<3;c++){unsigned cb=back[c],cs=src[c],v=cs;
                    if(l->blend==1)v=(cb*cs+127)/255;
                    if(l->blend==2)v=255-((255-cb)*(255-cs)+127)/255;
                    if(l->blend==3)v=cb<128?(2*cb*cs+127)/255:255-(2*(255-cb)*(255-cs)+127)/255;
                    src[c]=(cs*(255-a)+v*a+127)/255;
                }p=(p&0xff000000u)|(src[0]<<16)|(src[1]<<8)|src[2];
            }
            if(sa==255){r=((p>>16)&255)*255;g=((p>>8)&255)*255;b=(p&255)*255;a=255;continue;}
            /* Premultiplied accumulators scaled 0..65025, converted once at end. */
            r=((p>>16)&255)*sa+(r*(255-sa)+127)/255;
            g=((p>>8)&255)*sa+(g*(255-sa)+127)/255;
            b=(p&255)*sa+(b*(255-sa)+127)/255;
            a=sa+(a*(255-sa)+127)/255;
        }
        if(a==255){return 0xff000000u|(((r+127)/255)<<16)|(((g+127)/255)<<8)|((b+127)/255);}
        return a ? (a<<24)|(((r+a/2)/a)<<16)|(((g+a/2)/a)<<8)|((b+a/2)/a) : 0;
}
uint32_t pd_sample_layers(const PdLayer layers[PD_LAYERS],uint8_t enabled,const uint8_t order[PD_LAYERS],unsigned i){return sample_layers(layers,enabled,order,i);}
void pd_composite_rect(PdDocument *d,int x0,int y0,int x1,int y1) {
    if(x0<0)x0=0;if(y0<0)y0=0;if(x1>(int)d->width)x1=(int)d->width;if(y1>(int)d->height)y1=(int)d->height;
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){unsigned i=(unsigned)(y*d->stride+x);pd_image(d)[i]=sample_layers(d->layer,d->enabled,d->order,i);}
}

void pd_composite(PdDocument *d){
#ifdef PD_TEST
composite_calls++;
#endif
pd_composite_rect(d,0,0,(int)d->width,(int)d->height);}
bool pd_merge_down(PdDocument *d){
    if(d->editing)return false;unsigned source=d->active,target=PD_LAYERS;
    for(unsigned k=0;k<PD_LAYERS;k++){unsigned i=d->order[k];if(i==source)break;if(d->enabled&(1u<<i))target=i;}
    if(target==PD_LAYERS||(d->layer[source].flags&PD_LAYER_LOCK)||(d->layer[target].flags&(PD_LAYER_LOCK|PD_LAYER_ALPHA_LOCK))||!d->layer[source].visible||!d->layer[target].visible||d->layer[source].blend||d->layer[target].blend)return false;
    uint8_t mask=d->enabled;d->enabled=(uint8_t)((1u<<source)|(1u<<target));pd_composite(d);d->enabled=mask;
    d->active=target;if(!pd_begin(d))return false;
    memcpy(pd_pixels(&d->layer[target]),pd_image(d),pd_bytes(d));d->layer[target].opacity=255;
    d->enabled&=(uint8_t)~(1u<<source);return pd_commit(d);
}

static bool mask_has(const uint32_t *mask,unsigned i){return (mask[i/32]&(1u<<(i%32)))!=0;}
static void mask_set(uint32_t *mask,unsigned i){mask[i/32]|=1u<<(i%32);}
bool pd_mask_bounds_stride(const uint32_t *mask,unsigned width,unsigned height,unsigned stride,int *x,int *y,int *w,int *h){
    int right=-1,bottom=-1;*x=(int)width;*y=(int)height;for(unsigned yy=0;yy<height;yy++)for(unsigned xx=0;xx<width;xx++)if(mask_has(mask,yy*stride+xx)){if((int)xx<*x)*x=(int)xx;if((int)yy<*y)*y=(int)yy;if((int)xx>right)right=(int)xx;if((int)yy>bottom)bottom=(int)yy;}*w=right-*x+1;*h=bottom-*y+1;return right>=0;
}
void pd_mask_rectangle_stride(uint32_t *mask,unsigned stride,unsigned rows,int x0,int y0,int x1,int y1){
    if(x0>x1){int t=x0;x0=x1;x1=t;}if(y0>y1){int t=y0;y0=y1;y1=t;}if(x0<0)x0=0;if(y0<0)y0=0;if(x1>=(int)stride)x1=(int)stride-1;if(y1>=(int)rows)y1=(int)rows-1;
    for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)mask_set(mask,(unsigned)(y*stride+x));
}
void pd_mask_polygon_stride(uint32_t *mask,const int16_t points[][2],unsigned count,unsigned width,unsigned height,unsigned stride){
    if(count<3||count>512||!width||!height||width>512||height>512)return;for(unsigned i=0;i<count;i++)if(points[i][0]<0||points[i][1]<0||points[i][0]>=(int)width||points[i][1]>=(int)height)return;
    /* Even-odd coverage at pixel centers. Doubled integer coordinates avoid floats. */
    for(unsigned y=0;y<height;y++){int crossings[512];unsigned n=0;int py=(int)y*2+1;
        for(unsigned i=0,j=count-1;i<count;j=i++){int x0=points[j][0]*2+1,y0=points[j][1]*2+1,x1=points[i][0]*2+1,y1=points[i][1]*2+1;if((y0>py)==(y1>py))continue;crossings[n++]=x0*256+(py-y0)*(x1-x0)*256/(y1-y0);}
        for(unsigned i=1;i<n;i++){int v=crossings[i];unsigned j=i;while(j&&crossings[j-1]>v){crossings[j]=crossings[j-1];j--;}crossings[j]=v;}
        for(unsigned x=0;x<width;x++){int px=((int)x*2+1)*256;for(unsigned i=0;i+1<n;i+=2)if(px>=crossings[i]&&px<crossings[i+1]){mask_set(mask,y*stride+x);break;}}
    }
}
bool pd_mask_bounds(const uint32_t *m,unsigned w,unsigned h,int *x,int *y,int *rw,int *rh){return pd_mask_bounds_stride(m,w,h,128,x,y,rw,rh);}
void pd_mask_rectangle(uint32_t *m,int x,int y,int r,int b){pd_mask_rectangle_stride(m,128,128,x,y,r,b);}
void pd_mask_polygon(uint32_t *m,const int16_t p[][2],unsigned n,unsigned w,unsigned h){pd_mask_polygon_stride(m,p,n,w,h,128);}
bool pd_copy_selection(const PdDocument *d,const uint32_t *mask,PdClipboard *clip){
    int x,y,w,h;if(!pd_mask_bounds_stride(mask,d->width,d->height,d->stride,&x,&y,&w,&h))return false;if(w>128||h>128)return false;memset(clip,0,sizeof(*clip));clip->width=(unsigned)w;clip->height=(unsigned)h;
    for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){unsigned source=(unsigned)((y+yy)*d->stride+x+xx),target=(unsigned)(yy*128+xx);if(mask_has(mask,source)){mask_set(clip->mask,target);clip->pixels[target]=pd_pixels(&d->layer[d->active])[source];}}return true;
}
/* Straight-alpha source-over. Zero-alpha clipboard pixels are true no-ops. */
static uint32_t selection_over(uint32_t source,uint32_t dest){
    unsigned sa=source>>24,da=dest>>24;if(!sa)return dest;if(sa==255)return source;
    unsigned alpha=sa*255+da*(255-sa);uint32_t out=((alpha+127)/255)<<24;
    for(unsigned shift=0;shift<24;shift+=8){unsigned numerator=((source>>shift)&255)*sa*255+((dest>>shift)&255)*da*(255-sa);out|=((numerator+alpha/2)/alpha)<<shift;}return out;
}
bool pd_move_selection(PdDocument *d,const uint32_t *mask,int dx,int dy,bool erase){
    int x,y,w,h;if(!d->editing||!d->layer[d->active].visible||(d->layer[d->active].flags&(PD_LAYER_LOCK|PD_LAYER_ALPHA_LOCK))||!pd_mask_bounds_stride(mask,d->width,d->height,d->stride,&x,&y,&w,&h))return false;
    if(!erase&&(x+dx<0||y+dy<0||x+dx+w>(int)d->width||y+dy+h>(int)d->height))return false;
    uint32_t *pixels=pd_pixels(&d->layer[d->active]);memcpy(pixels,pd_before(d),pd_bytes(d));
    for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++){unsigned i=(unsigned)(yy*d->stride+xx);if(mask_has(mask,i))pixels[i]=0;}
    if(!erase)for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++){unsigned i=(unsigned)(yy*d->stride+xx);if(mask_has(mask,i))pixels[(yy+dy)*d->stride+xx+dx]=selection_over(pd_before(d)[i],pixels[(yy+dy)*d->stride+xx+dx]);}return true;
}
bool pd_paste_selection(PdDocument *d,const PdClipboard *clip,int x,int y){
    if(!clip->width||!clip->height||clip->width>128||clip->height>128||x<0||y<0||x+(int)clip->width>(int)d->width||y+(int)clip->height>(int)d->height||!d->layer[d->active].visible||(d->layer[d->active].flags&PD_LAYER_LOCK))return false;
    if(!pd_begin(d))return false;
    for(unsigned yy=0;yy<clip->height;yy++)for(unsigned xx=0;xx<clip->width;xx++){unsigned i=yy*128+xx;if(!mask_has(clip->mask,i))continue;if(d->layer[d->active].flags&PD_LAYER_ALPHA_LOCK)pd_line(d,x+(int)xx,y+(int)yy,x+(int)xx,y+(int)yy,clip->pixels[i]);else{unsigned target=(unsigned)(y+(int)yy)*d->stride+(unsigned)x+xx;pd_pixels(&d->layer[d->active])[target]=selection_over(clip->pixels[i],pd_pixels(&d->layer[d->active])[target]);}}return pd_commit(d);
}

static bool ellipse_inside(int x,int y,int width,int height){
    if(x<0||y<0||x>=width||y>=height)return false;
    int cx=2*x-width+1,cy=2*y-height+1;unsigned ww=(unsigned)(width*width),hh=(unsigned)(height*height);
    return (uint64_t)(cx*cx)*hh+(uint64_t)(cy*cy)*ww<=(uint64_t)ww*hh;
}
void pd_shape(PdDocument *d,int x0,int y0,int x1,int y1,unsigned kind,bool filled,uint32_t color,unsigned size,bool round,unsigned guides,uint32_t *damage,const uint32_t *mask){
    if(kind>2||x0< -4096||x0>4096||x1< -4096||x1>4096||y0< -4096||y0>4096||y1< -4096||y1>4096||abs(x1-x0)>=(int)d->width||abs(y1-y0)>=(int)d->height)return;
    if(!kind){pd_masked_line(d,x0,y0,x1,y1,color,size,round,guides,damage,mask);return;}
    if(x0>x1){int t=x0;x0=x1;x1=t;}if(y0>y1){int t=y0;y0=y1;y1=t;}int width=x1-x0+1,height=y1-y0+1;
    for(int y=0;y<height;y++)for(int x=0;x<width;x++){
        bool inside=kind==1||ellipse_inside(x,y,width,height);if(!inside)continue;
        bool edge=kind==1?(!x||!y||x==width-1||y==height-1):(!ellipse_inside(x-1,y,width,height)||!ellipse_inside(x+1,y,width,height)||!ellipse_inside(x,y-1,width,height)||!ellipse_inside(x,y+1,width,height));
        if(filled||edge)pd_masked_line(d,x0+x,y0+y,x0+x,y0+y,color,filled?1:size,round,guides,damage,mask);
    }
}
bool pd_transform_selection(PdDocument *d,const uint32_t *mask,unsigned operation,uint32_t *result){
    if(operation>2||d->editing||!d->layer[d->active].visible||(d->layer[d->active].flags&(PD_LAYER_LOCK|PD_LAYER_ALPHA_LOCK)))return false;
    int x,y,w,h;if(!pd_mask_bounds_stride(mask,d->width,d->height,d->stride,&x,&y,&w,&h))return false;int nw=operation==2?h:w,nh=operation==2?w:h;if(w>128||h>128||nw>(int)d->width||nh>(int)d->height)return false;
    PdClipboard *clip=malloc(sizeof(*clip));if(!clip)return false;pd_copy_selection(d,mask,clip);if(!pd_begin(d)){free(clip);return false;}pd_move_selection(d,mask,0,0,true);
    if(x+nw>(int)d->width)x=(int)d->width-nw;if(y+nh>(int)d->height)y=(int)d->height-nh;memset(result,0,pd_plane(d)/8);
    for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){unsigned i=(unsigned)(yy*128+xx);if(!mask_has(clip->mask,i))continue;int tx=operation==0?w-1-xx:operation==2?h-1-yy:xx,ty=operation==1?h-1-yy:operation==2?xx:yy;unsigned target=(unsigned)((y+ty)*d->stride+x+tx);pd_pixels(&d->layer[d->active])[target]=selection_over(clip->pixels[i],pd_pixels(&d->layer[d->active])[target]);mask_set(result,target);}
    free(clip);pd_commit(d);return true;
}

bool pd_duplicate_selection(PdDocument *d,const uint32_t *mask){
    int x,y,w,h;if(d->editing||!pd_mask_bounds_stride(mask,d->width,d->height,d->stride,&x,&y,&w,&h))return false;
    unsigned source=d->active,slot=0;while(slot<d->capacity&&(d->enabled&(1u<<slot)))slot++;if(slot==d->capacity)return false;
    d->active=slot;if(!pd_begin(d)){d->active=source;return false;}memset(pd_pixels(&d->layer[slot]),0,pd_bytes(d));
    for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++){unsigned i=(unsigned)yy*d->stride+(unsigned)xx;if(mask_has(mask,i))pd_pixels(&d->layer[slot])[i]=pd_pixels(&d->layer[source])[i];}
    d->enabled|=(uint8_t)(1u<<slot);d->layer[slot].visible=1;d->layer[slot].opacity=d->layer[source].opacity;d->layer[slot].blend=d->layer[source].blend;d->layer[slot].flags=0;
    char name[64];snprintf(name,sizeof(name),"%.58s COPY",d->names[source]);memset(d->names[slot],0,64);memcpy(d->names[slot],name,strlen(name)+1);move_slot(d,slot,7);unsigned at=0;while(d->order[at]!=source)at++;move_slot(d,slot,at+1);return pd_commit(d);
}
bool pd_scale_selection(PdDocument *d,const uint32_t *mask,int factor,uint32_t *result){
    if((factor!=2&&factor!=3&&factor!=4&&factor!=-2)||d->editing||!d->layer[d->active].visible||(d->layer[d->active].flags&(PD_LAYER_LOCK|PD_LAYER_ALPHA_LOCK)))return false;
    int x,y,w,h;if(!pd_mask_bounds_stride(mask,d->width,d->height,d->stride,&x,&y,&w,&h)||w>128||h>128)return false;
    int nw=factor>0?w*factor:(w+1)/2,nh=factor>0?h*factor:(h+1)/2;if(nw>(int)d->width||nh>(int)d->height)return false;
    PdClipboard *clip=malloc(sizeof(*clip));if(!clip)return false;if(!pd_copy_selection(d,mask,clip)||!pd_begin(d)){free(clip);return false;}
    pd_move_selection(d,mask,0,0,true);if(x+nw>(int)d->width)x=(int)d->width-nw;if(y+nh>(int)d->height)y=(int)d->height-nh;memset(result,0,pd_plane(d)/8);
    for(int yy=0;yy<nh;yy++)for(int xx=0;xx<nw;xx++){unsigned src=(unsigned)(factor>0?yy/factor:yy*2)*128+(unsigned)(factor>0?xx/factor:xx*2);if(!mask_has(clip->mask,src)||!(clip->pixels[src]>>24))continue;unsigned dest=(unsigned)(y+yy)*d->stride+(unsigned)(x+xx);pd_pixels(&d->layer[d->active])[dest]=selection_over(clip->pixels[src],pd_pixels(&d->layer[d->active])[dest]);mask_set(result,dest);}
    free(clip);pd_commit(d);return true;
}

void pd_perfect_line(PdDocument *d,PdPerfectStroke *state,int x0,int y0,int x1,int y1,uint32_t color,unsigned guides,uint32_t *damage,const uint32_t *mask){
    if(!d->editing||!d->layer[d->active].visible||(d->layer[d->active].flags&PD_LAYER_LOCK))return;
    if(x0< -4096||x0>4096||x1< -4096||x1>4096||y0< -4096||y0>4096||y1< -4096||y1>4096)return;
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    for(;;){
        if(!state->points||x0!=state->bx||y0!=state->by){
            bool corner=state->points>=2&&abs(state->bx-state->ax)+abs(state->by-state->ay)==1&&abs(x0-state->bx)+abs(y0-state->by)==1&&abs(x0-state->ax)==1&&abs(y0-state->ay)==1;
            if(corner){for(unsigned j=0;j<state->saved_count;j++){unsigned i=state->index[j];pd_pixels(&d->layer[d->active])[i]=state->old[j];if(damage)damage[i/32]|=1u<<(i%32);}}
            else if(state->points){state->ax=state->bx;state->ay=state->by;}
            state->saved_count=0;
            int px=x0,py=y0;if(guides&4)px=(px%(int)d->width+(int)d->width)%(int)d->width;if(guides&8)py=(py%(int)d->height+(int)d->height)%(int)d->height;
            if(px>=0&&py>=0&&px<(int)d->width&&py<(int)d->height){for(unsigned mirror=0;mirror<=(guides&3);mirror++){if(mirror&~guides)continue;unsigned i=(unsigned)(((mirror&2)?(int)d->height-1-py:py)*d->stride+((mirror&1)?(int)d->width-1-px:px));if(mask&&!(mask[i/32]&(1u<<(i%32))))continue;bool duplicate=false;for(unsigned j=0;j<state->saved_count;j++)if(state->index[j]==i)duplicate=true;if(!duplicate){unsigned j=state->saved_count++;state->index[j]=i;state->old[j]=pd_pixels(&d->layer[d->active])[i];}}}
            pd_masked_line(d,x0,y0,x0,y0,color,1,false,guides,damage,mask);state->bx=x0;state->by=y0;if(state->points<2)state->points++;
        }
        if(x0==x1&&y0==y1)break;int twice=2*err;if(twice>=dy){err+=dy;x0+=sx;}if(twice<=dx){err+=dx;y0+=sy;}
    }
}
