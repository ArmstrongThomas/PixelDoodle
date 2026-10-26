#include "animation.h"
#include "zlib.h"
#include <stdlib.h>
#include <string.h>

typedef struct Tile {
    struct Tile *next;
    uint32_t hash,refs,disk;
    unsigned bytes,mode;
    unsigned char data[];
} Tile;
typedef struct { Tile *tile; unsigned position; } Ref;
typedef struct {
    Ref *refs;
    unsigned count,duration;
    uint16_t link[PD_LAYERS]; /* linked-cel group per layer; 0 = unlinked */
} Frame;
struct PdAnimation {
    Tile *buckets[257];
    Frame *frames[PD_FRAMES];
    PdTag tags[PD_TAGS];
    unsigned count,current,tag_count;
    uint64_t structure,saved_structure,visual;
    size_t used,budget;
};
typedef struct PdAnimation Animation;
static uint16_t new_link(const Animation *a,unsigned l);
static bool linked_refs(Animation *a,const Frame *f,const Frame *g,unsigned tiles,Ref **out,unsigned *count);
static void replace_refs(Animation *a,Frame *g,Ref *refs,unsigned count);
static void restore(PdDocument *d,unsigned index);
static bool enable(PdDocument *d);
static uint64_t visual_serial;
#ifdef PD_TEST
static int fail_after=-1;
void pd_frames_test_fail_after(int n){fail_after=n;}
bool pd_frames_test_budget(PdDocument *d,size_t bytes){if(!d->animation||bytes<d->animation->used)return false;d->animation->budget=bytes;return true;}
#endif
static size_t budget_for(unsigned profile){return (size_t)(profile<=1?1:profile==2?6:profile==3?12:32)*1024*1024;}
static void *take(Animation *a,size_t bytes){
    size_t cost=bytes+sizeof(size_t)+16;
    if(cost>a->budget-a->used)return NULL;
#ifdef PD_TEST
    if(fail_after==0)return NULL;if(fail_after>0)fail_after--;
#endif
    size_t *p=calloc(1,bytes+sizeof(size_t));if(!p)return NULL;*p=cost;a->used+=cost;return p+1;
}
static void give(Animation *a,void *p){if(p){size_t *b=(size_t *)p-1;a->used-=*b;free(b);}}
static unsigned get32(const unsigned char *p){return p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static void put32(unsigned char *p,unsigned n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static void decode(const Tile *t,uint32_t pixels[64]){
    for(unsigned i=0;i<64;i++)pixels[i]=t->mode==2?get32(t->data):t->mode==1?get32(t->data+1+4*t->data[1+4*t->data[0]+i]):get32(t->data+4*i);
}
static unsigned encode_tile(const uint32_t pixels[64],unsigned char data[257],unsigned *kind,uint32_t *key){
    uint32_t hash=2166136261u,colors[48];unsigned count=0;unsigned char indices[64];bool empty=true;
    for(unsigned i=0;i<64;i++){hash=(hash^pixels[i])*16777619u;if(pixels[i])empty=false;
        unsigned k=0;while(k<count&&colors[k]!=pixels[i])k++;if(k==count&&count<48)colors[count++]=pixels[i];indices[i]=(unsigned char)k;}
    if(empty)return 0;
    unsigned mode=count==1?2:count<=47?1:0,bytes=mode==2?4:mode==1?1+count*4+64:256;
    if(mode==2)put32(data,colors[0]);else if(mode==1){data[0]=(unsigned char)count;for(unsigned i=0;i<count;i++)put32(data+1+4*i,colors[i]);memcpy(data+1+4*count,indices,64);}else for(unsigned i=0;i<64;i++)put32(data+4*i,pixels[i]);
    *kind=mode;*key=hash;return bytes;
}
static Tile *find_tile(Animation *a,unsigned mode,unsigned bytes,uint32_t hash,const unsigned char *data){
    for(Tile *t=a->buckets[hash%257];t;t=t->next)if(t->hash==hash&&t->mode==mode&&t->bytes==bytes&&!memcmp(t->data,data,bytes))return t;return NULL;
}
static Tile *intern(Animation *a,const uint32_t pixels[64]){
    unsigned char data[257];unsigned mode;uint32_t hash;unsigned bytes=encode_tile(pixels,data,&mode,&hash);if(!bytes)return NULL;
    Tile *found=find_tile(a,mode,bytes,hash,data);if(found){found->refs++;return found;}
    unsigned bucket=hash%257;
    Tile *t=take(a,sizeof(*t)+bytes);if(!t)return NULL;t->hash=hash;t->refs=1;t->mode=mode;t->bytes=bytes;memcpy(t->data,data,bytes);t->next=a->buckets[bucket];a->buckets[bucket]=t;return t;
}
static void release(Animation *a,Tile *t){
    if(--t->refs)return;Tile **p=&a->buckets[t->hash%257];while(*p!=t)p=&(*p)->next;*p=t->next;give(a,t);
}
static void free_frame(Animation *a,Frame *f){if(!f)return;for(unsigned i=0;i<f->count;i++)release(a,f->refs[i].tile);give(a,f->refs);give(a,f);}
static void destroy(Animation *a){if(!a)return;for(unsigned i=0;i<a->count;i++)free_frame(a,a->frames[i]);free(a);}
void pd_frames_dispose(PdDocument *d){destroy(d->animation);d->animation=NULL;}
unsigned pd_frame_count(const PdDocument *d){return d->animation?d->animation->count:1;}
unsigned pd_frame_index(const PdDocument *d){return d->animation?d->animation->current:0;}
unsigned pd_frame_duration(const PdDocument *d){return d->animation?d->animation->frames[d->animation->current]->duration:100;}
uint64_t pd_frames_visual_generation(const PdDocument *d){return d->animation?d->animation->visual:0;}
bool pd_frame_alpha(const PdDocument *d,unsigned frame,unsigned char *out){
    if(frame>=pd_frame_count(d)||!out)return false;memset(out,0,pd_plane(d));
    if(frame==pd_frame_index(d)){
        for(unsigned k=0;k<PD_LAYERS;k++){unsigned l=d->order[k];if(!(d->enabled&(1u<<l))||!d->layer[l].visible)continue;
            for(unsigned y=0;y<d->height;y++)for(unsigned x=0;x<d->width;x++){unsigned i=y*d->stride+x,sa=((pd_pixels(&d->layer[l])[i]>>24)*d->layer[l].opacity+127)/255;out[i]=(unsigned char)(sa+(out[i]*(255-sa)+127)/255);}}
    }else{
        const Frame *f=d->animation->frames[frame];unsigned tiles=pd_plane(d)/64,cols=d->stride/8;uint32_t pixels[64];
        for(unsigned k=0;k<PD_LAYERS;k++){unsigned l=d->order[k];if(!(d->enabled&(1u<<l))||!d->layer[l].visible)continue;
            for(unsigned r=0;r<f->count;r++){const Ref *ref=&f->refs[r];if(ref->position/tiles!=l)continue;unsigned id=ref->position%tiles;decode(ref->tile,pixels);
                for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){unsigned i=(id/cols*8+y)*d->stride+id%cols*8+x,sa=((pixels[y*8+x]>>24)*d->layer[l].opacity+127)/255;out[i]=(unsigned char)(sa+(out[i]*(255-sa)+127)/255);}}}
    }return true;
}
/* Normal "over" with layer opacity on straight RGBA. Blend modes are not applied: previews only. */
void pd_rgba_over(uint32_t *dst,uint32_t src,unsigned opacity){
    unsigned sa=((src>>24)*opacity+127)/255;if(!sa)return;uint32_t d=*dst;unsigned da=d>>24,oa=sa+(da*(255-sa)+127)/255,rgb=0;
    for(unsigned shift=0;shift<24;shift+=8){unsigned s=(src>>shift)&255,c=(d>>shift)&255;rgb|=((s*sa*255+c*da*(255-sa)+oa*255/2)/(oa*255))<<shift;}
    *dst=(oa<<24)|rgb;
}
/* Visits a frame's visible cels bottom to top: each callback gets one 8x8 tile of a layer. */
typedef void (*TileVisit)(void *ctx,const PdDocument *d,unsigned layer,unsigned x0,unsigned y0,const uint32_t pixels[64]);
static bool frame_tiles(const PdDocument *d,unsigned frame,TileVisit visit,void *ctx){
    if(frame>=pd_frame_count(d))return false;unsigned cols=d->stride/8,tiles=pd_plane(d)/64;uint32_t pixels[64];
    for(unsigned k=0;k<PD_LAYERS;k++){unsigned l=d->order[k];if(!(d->enabled&(1u<<l))||!d->layer[l].visible)continue;
        if(frame==pd_frame_index(d)){const uint32_t *src=pd_pixels(&d->layer[l]);
            for(unsigned id=0;id<tiles;id++){unsigned x0=id%cols*8,y0=id/cols*8;if(x0>=d->width||y0>=d->height)continue;bool any=false;
                for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){pixels[y*8+x]=src[(y0+y)*d->stride+x0+x];if(pixels[y*8+x])any=true;}if(any)visit(ctx,d,l,x0,y0,pixels);}
        }else{const Frame *f=d->animation->frames[frame];
            for(unsigned r=0;r<f->count;r++){const Ref *ref=&f->refs[r];if(ref->position/tiles!=l)continue;unsigned id=ref->position%tiles;decode(ref->tile,pixels);visit(ctx,d,l,id%cols*8,id/cols*8,pixels);}}
    }return true;
}
static void rgba_visit(void *ctx,const PdDocument *d,unsigned l,unsigned x0,unsigned y0,const uint32_t pixels[64]){
    uint32_t *out=ctx;for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++)pd_rgba_over(&out[(y0+y)*d->stride+x0+x],pixels[y*8+x],d->layer[l].opacity);
}
bool pd_frame_rgba(const PdDocument *d,unsigned frame,uint32_t *out){
    if(!out||frame>=pd_frame_count(d))return false;memset(out,0,pd_plane(d)*4);return frame_tiles(d,frame,rgba_visit,out);
}
typedef struct { uint32_t *out; unsigned w,h; } Sample;
static void sample_visit(void *ctx,const PdDocument *d,unsigned l,unsigned x0,unsigned y0,const uint32_t pixels[64]){
    Sample *s=ctx;unsigned ox0=(x0*s->w+d->width-1)/d->width,oy0=(y0*s->h+d->height-1)/d->height;
    for(unsigned oy=oy0;oy<s->h;oy++){unsigned y=oy*d->height/s->h;if(y>=y0+8)break;
        for(unsigned ox=ox0;ox<s->w;ox++){unsigned x=ox*d->width/s->w;if(x>=x0+8)break;pd_rgba_over(&s->out[oy*s->w+ox],pixels[(y-y0)*8+x-x0],d->layer[l].opacity);}}
}
bool pd_frame_sample(const PdDocument *d,unsigned frame,uint32_t *out,unsigned w,unsigned h){
    if(!out||!w||!h||frame>=pd_frame_count(d))return false;memset(out,0,(size_t)w*h*4);Sample s={out,w,h};return frame_tiles(d,frame,sample_visit,&s);
}
bool pd_frame_slot_unused(const PdDocument *d,unsigned layer){
    if(!d->animation)return true;const Animation *a=d->animation;unsigned tiles=pd_plane(d)/64;
    for(unsigned f=0;f<a->count;f++)if(f!=a->current)for(unsigned r=0;r<a->frames[f]->count;r++)if(a->frames[f]->refs[r].position/tiles==layer)return false;
    return true;
}
bool pd_frame_has_cel(const PdDocument *d,unsigned frame,unsigned layer){
    if(frame>=pd_frame_count(d)||layer>=d->capacity)return false;
    if(frame==pd_frame_index(d)){for(unsigned y=0;y<d->height;y++)for(unsigned x=0;x<d->width;x++)if(pd_pixels(&d->layer[layer])[y*d->stride+x])return true;return false;}
    const Frame *f=d->animation->frames[frame];unsigned tiles=pd_plane(d)/64;
    for(unsigned i=0;i<f->count;i++)if(f->refs[i].position/tiles==layer)return true;return false;
}
bool pd_frame_duplicate_layer_cels(PdDocument *d,unsigned source,unsigned target){
    if(d->editing||source>=d->capacity||target>=d->capacity||source==target||!pd_frame_slot_unused(d,target))return false;
    if(!d->animation)return true;Animation *a=d->animation;unsigned tiles=pd_plane(d)/64;Ref *pending[PD_FRAMES]={0};
    for(unsigned i=0;i<a->count;i++)if(i!=a->current){Frame *f=a->frames[i];unsigned extra=0;for(unsigned r=0;r<f->count;r++)if(f->refs[r].position/tiles==source)extra++;
        if(!extra)continue;pending[i]=take(a,(f->count+extra)*sizeof(Ref));if(!pending[i]){for(unsigned j=0;j<i;j++)give(a,pending[j]);return false;}
        unsigned at=0;for(unsigned l=0;l<d->capacity;l++){
            for(unsigned r=0;r<f->count;r++)if(f->refs[r].position/tiles==l)pending[i][at++]=f->refs[r];
            if(l==target)for(unsigned r=0;r<f->count;r++)if(f->refs[r].position/tiles==source)pending[i][at++]=(Ref){f->refs[r].tile,target*tiles+f->refs[r].position%tiles};
        }
    }
    for(unsigned i=0;i<a->count;i++)if(pending[i]){Frame *f=a->frames[i];unsigned extra=0;for(unsigned r=0;r<f->count;r++)if(f->refs[r].position/tiles==source){f->refs[r].tile->refs++;extra++;}give(a,f->refs);f->refs=pending[i];f->count+=extra;}
    for(unsigned i=0;i<a->count;i++)a->frames[i]->link[target]=a->frames[i]->link[source];
    return true;
}
void pd_frame_clear_links(PdDocument *d,unsigned layer){if(d->animation&&layer<PD_LAYERS)for(unsigned i=0;i<d->animation->count;i++)d->animation->frames[i]->link[layer]=0;}
unsigned pd_cel_link(const PdDocument *d,unsigned frame,unsigned layer){return d->animation&&frame<d->animation->count&&layer<PD_LAYERS?d->animation->frames[frame]->link[layer]:0;}
bool pd_cel_set_link(PdDocument *d,unsigned frame,unsigned layer,unsigned source){
    if(d->editing||!d->animation||layer>=PD_LAYERS)return false;Animation *a=d->animation;
    if(frame>=a->count||source>=a->count||frame==source)return false;
    uint16_t group=a->frames[source]->link[layer];if(!group)group=new_link(a,layer);
    a->frames[source]->link[layer]=group;a->frames[frame]->link[layer]=group;a->structure++;return true;
}
bool pd_frame_layer_copy(const PdDocument *d,unsigned frame,unsigned layer,uint32_t *out){
    if(!out||frame>=pd_frame_count(d)||layer>=d->capacity)return false;memset(out,0,pd_bytes(d));
    if(frame==pd_frame_index(d)){if(pd_pixels(&d->layer[layer])!=out)memcpy(out,pd_pixels(&d->layer[layer]),pd_bytes(d));return true;}
    if(!d->animation)return false;const Frame *f=d->animation->frames[frame];unsigned tiles=pd_plane(d)/64,cols=d->stride/8;uint32_t pixels[64];
    for(unsigned r=0;r<f->count;r++){const Ref *ref=&f->refs[r];if(ref->position/tiles!=layer)continue;unsigned id=ref->position%tiles;decode(ref->tile,pixels);
        for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++)out[(id/cols*8+y)*d->stride+id%cols*8+x]=pixels[y*8+x];}
    return true;
}
/* Removes one frame/layer from its link group, dropping a group that would be left with one member. */
static void drop_link_at(Animation *a,unsigned frame,unsigned layer){
    Frame *f=a->frames[frame];uint16_t id=f->link[layer];if(!id)return;f->link[layer]=0;
    unsigned left=0,last=0;for(unsigned i=0;i<a->count;i++)if(a->frames[i]->link[layer]==id){left++;last=i;}
    if(left==1)a->frames[last]->link[layer]=0;
}
bool pd_cel_move(PdDocument *d,unsigned sf,unsigned sl,unsigned df,unsigned dl){
    if(d->editing||sl>=d->capacity||dl>=d->capacity)return false;unsigned count=pd_frame_count(d);
    if(sf>=count||df>=count||(sf==df&&sl==dl))return sf<count&&df<count;
    Animation *a=d->animation;
    if(!a){
        if(sf||df)return false;uint32_t *src=pd_pixels(&d->layer[sl]),*dst=pd_pixels(&d->layer[dl]);
        memcpy(dst,src,pd_bytes(d));memset(src,0,pd_bytes(d));d->active=dl;d->revision++;d->count=d->cursor=d->history_used=0;pd_composite(d);return true;
    }
    if(sf==df&&sf==a->current){
        drop_link_at(a,sf,sl);drop_link_at(a,sf,dl);
        uint32_t *src=pd_pixels(&d->layer[sl]),*dst=pd_pixels(&d->layer[dl]);
        memcpy(dst,src,pd_bytes(d));memset(src,0,pd_bytes(d));
        d->active=dl;d->revision++;d->count=d->cursor=d->history_used=0;a->structure++;a->visual=++visual_serial;pd_composite(d);return true;
    }
    if(!pd_frame_snapshot(d))return false;
    Frame *src=a->frames[sf],*dst=a->frames[df];unsigned tiles=pd_plane(d)/64,moved=0,keep=0,add=0;
    for(unsigned r=0;r<src->count;r++)if(src->refs[r].position/tiles==sl)moved++;
    for(unsigned r=0;r<src->count;r++)if(src->refs[r].position/tiles!=sl)keep++;
    for(unsigned r=0;r<dst->count;r++){unsigned l=dst->refs[r].position/tiles;if(l!=dl&&!(sf==df&&l==sl))add++;}
    unsigned take_dst=add+moved;
    Ref *rsrc=keep?take(a,keep*sizeof(Ref)):NULL,*rdst=take_dst?take(a,take_dst*sizeof(Ref)):NULL;
    if((keep&&!rsrc)||(take_dst&&!rdst)){give(a,rsrc);give(a,rdst);return false;}
    unsigned p=0;for(unsigned l=0;l<d->capacity;l++)if(l!=sl)for(unsigned r=0;r<src->count;r++)if(src->refs[r].position/tiles==l)rsrc[p++]=src->refs[r];
    p=0;for(unsigned l=0;l<d->capacity;l++){
        if(l==dl){for(unsigned r=0;r<src->count;r++)if(src->refs[r].position/tiles==sl){rdst[p]=src->refs[r];rdst[p].position=dl*tiles+(src->refs[r].position%tiles);rdst[p].tile->refs++;p++;}}
        else if(!(sf==df&&l==sl))for(unsigned r=0;r<dst->count;r++)if(dst->refs[r].position/tiles==l)rdst[p++]=dst->refs[r];
    }
    for(unsigned r=0;r<src->count;r++)release(a,src->refs[r].tile);give(a,src->refs);
    if(dst==src){give(a,rsrc);src->refs=rdst;src->count=take_dst;}
    else{src->refs=rsrc;src->count=keep;for(unsigned r=0;r<dst->count;r++)release(a,dst->refs[r].tile);give(a,dst->refs);dst->refs=rdst;dst->count=take_dst;}
    drop_link_at(a,sf,sl);drop_link_at(a,df,dl);
    a->structure++;a->visual=++visual_serial;d->active=dl;d->revision++;d->count=d->cursor=d->history_used=0;
    restore(d,df);pd_composite(d);return true;
}
static bool same_layer_refs(const Frame *f,const Frame *g,unsigned l,unsigned tiles){
    unsigned i=0,j=0;for(;;){while(i<f->count&&f->refs[i].position/tiles!=l)i++;while(j<g->count&&g->refs[j].position/tiles!=l)j++;
        if(i==f->count||j==g->count)return i==f->count&&j==g->count;if(f->refs[i].position!=g->refs[j].position||f->refs[i].tile!=g->refs[j].tile)return false;i++;j++;}
}
bool pd_cel_link_previous(PdDocument *d,unsigned layer,bool *cleared){
    if(cleared)*cleared=false;
    if(d->editing||!d->animation||!d->animation->current||layer>=PD_LAYERS||!pd_frame_snapshot(d))return false;
    Animation *a=d->animation;unsigned cur=a->current,tiles=pd_plane(d)/64;Frame *f=a->frames[cur],*p=a->frames[cur-1];
    if(p->link[layer]&&p->link[layer]==f->link[layer])return false;
    uint16_t group=p->link[layer]?p->link[layer]:new_link(a,layer),keep=f->link[layer];
    if(!same_layer_refs(f,p,layer,tiles)){
        /* A probe sharing only this layer copies the previous image into the current frame. */
        Frame probe=*p;memset(probe.link,0,sizeof(probe.link));probe.link[layer]=group;f->link[layer]=group;
        Ref *refs;unsigned count;if(!linked_refs(a,&probe,f,tiles,&refs,&count)){f->link[layer]=keep;return false;}
        replace_refs(a,f,refs,count);d->count=d->cursor=d->history_used=0;if(cleared)*cleared=true;
    }
    f->link[layer]=p->link[layer]=group;
    if(keep){unsigned left=0,last=0;for(unsigned i=0;i<a->count;i++)if(a->frames[i]->link[layer]==keep){left++;last=i;}if(left==1)a->frames[last]->link[layer]=0;}
    a->structure++;a->visual=++visual_serial;restore(d,cur);return true;
}
bool pd_cel_unlink(PdDocument *d,unsigned layer){
    if(d->editing||!d->animation||layer>=PD_LAYERS)return false;Animation *a=d->animation;Frame *f=a->frames[a->current];uint16_t id=f->link[layer];if(!id)return false;
    f->link[layer]=0;unsigned left=0,last=0;for(unsigned i=0;i<a->count;i++)if(a->frames[i]->link[layer]==id){left++;last=i;}
    if(left==1)a->frames[last]->link[layer]=0;a->structure++;return true;
}
unsigned pd_tag_count(const PdDocument *d){return d->animation?d->animation->tag_count:0;}
const PdTag *pd_tag(const PdDocument *d,unsigned i){return d->animation&&i<d->animation->tag_count?&d->animation->tags[i]:NULL;}
int pd_tag_at(const PdDocument *d,unsigned frame){
    int best=-1;unsigned span=0;for(unsigned i=0;i<pd_tag_count(d);i++){const PdTag *t=&d->animation->tags[i];if(frame<t->from||frame>t->to)continue;
        if(best<0||t->to-t->from<span){best=(int)i;span=t->to-t->from;}}return best;
}
static bool tag_valid(const PdDocument *d,const PdTag *t){return t->from<=t->to&&t->to<pd_frame_count(d)&&t->direction<=3&&t->repeat<=65535&&t->color<=0xffffff&&memchr(t->name,0,PD_TAG_NAME_MAX);}
int pd_tag_add(PdDocument *d,const PdTag *t){
    if(!tag_valid(d,t)||!enable(d)||d->animation->tag_count>=PD_TAGS)return -1;Animation *a=d->animation;a->tags[a->tag_count]=*t;a->structure++;return (int)a->tag_count++;
}
bool pd_tag_set(PdDocument *d,unsigned i,const PdTag *t){if(i>=pd_tag_count(d)||!tag_valid(d,t))return false;d->animation->tags[i]=*t;d->animation->structure++;return true;}
bool pd_tag_delete(PdDocument *d,unsigned i){
    if(i>=pd_tag_count(d))return false;Animation *a=d->animation;memmove(a->tags+i,a->tags+i+1,(a->tag_count-i-1)*sizeof(PdTag));a->tag_count--;a->structure++;return true;
}
size_t pd_frame_memory(const PdDocument *d){return d->animation?d->animation->used:0;}
size_t pd_frame_budget(const PdDocument *d){return d->animation?d->animation->budget:budget_for(d->profile);}
bool pd_frames_dirty(const PdDocument *d){return d->animation&&d->animation->structure!=d->animation->saved_structure;}
uint64_t pd_frames_generation(const PdDocument *d){return d->animation?d->animation->structure:0;}
void pd_frames_saved(PdDocument *d){if(d->animation)d->animation->saved_structure=d->animation->structure;}
static bool enable(PdDocument *d){
    if(d->animation)return true;Animation *a=calloc(1,sizeof(*a));if(!a)return false;a->budget=budget_for(d->profile);a->used=sizeof(*a)+16;
    Frame *f=take(a,sizeof(*f));if(!f){free(a);return false;}f->duration=100;a->frames[0]=f;a->count=1;a->visual=++visual_serial;d->animation=a;return true;
}
static bool tile_pixels(const PdDocument *d,unsigned l,unsigned id,uint32_t pixels[64]){
    unsigned cols=d->stride/8;bool any=false;
    for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){unsigned xx=id%cols*8+x,yy=id/cols*8+y;uint32_t c=xx<d->width&&yy<d->height?pd_pixels(&d->layer[l])[yy*d->stride+xx]:0;pixels[y*8+x]=c;if(c)any=true;}return any;
}
static bool shares_link(const Frame *f,const Frame *g,unsigned l){return f->link[l]&&f->link[l]==g->link[l];}
/* g's cels with every layer it shares with f replaced by f's cels, still ordered by position. */
static bool linked_refs(Animation *a,const Frame *f,const Frame *g,unsigned tiles,Ref **out,unsigned *count){
    unsigned n=0;for(unsigned r=0;r<g->count;r++)if(!shares_link(f,g,g->refs[r].position/tiles))n++;
    for(unsigned r=0;r<f->count;r++)if(shares_link(f,g,f->refs[r].position/tiles))n++;
    *out=NULL;*count=0;if(n&&!(*out=take(a,n*sizeof(Ref))))return false;
    for(unsigned l=0;l<PD_LAYERS;l++){const Frame *src=shares_link(f,g,l)?f:g;
        for(unsigned r=0;r<src->count;r++)if(src->refs[r].position/tiles==l)(*out)[(*count)++]=src->refs[r];}
    return true;
}
static void replace_refs(Animation *a,Frame *g,Ref *refs,unsigned count){
    for(unsigned r=0;r<count;r++)refs[r].tile->refs++;
    for(unsigned r=0;r<g->count;r++)release(a,g->refs[r].tile);give(a,g->refs);g->refs=refs;g->count=count;
}
/* Copies the packed frame f into every other frame linked to it; nothing changes on failure. */
static bool propagate_links(Animation *a,unsigned index,const Frame *f,unsigned tiles){
    bool any=false;for(unsigned l=0;l<PD_LAYERS;l++)if(f->link[l])any=true;if(!any)return true;
    Ref *pending[PD_FRAMES];unsigned counts[PD_FRAMES];bool touched[PD_FRAMES]={0};
    for(unsigned i=0;i<a->count;i++){if(i==index)continue;bool linked=false;for(unsigned l=0;l<PD_LAYERS;l++)if(shares_link(f,a->frames[i],l))linked=true;if(!linked)continue;
        if(!linked_refs(a,f,a->frames[i],tiles,&pending[i],&counts[i])){for(unsigned j=0;j<i;j++)if(touched[j])give(a,pending[j]);return false;}touched[i]=true;}
    for(unsigned i=0;i<a->count;i++)if(touched[i])replace_refs(a,a->frames[i],pending[i],counts[i]);
    return true;
}
bool pd_frame_snapshot(PdDocument *d){
    if(d->editing)return false;if(!d->animation)return true;Animation *a=d->animation;Frame *old=a->frames[a->current];
    Frame *f=take(a,sizeof(*f));if(!f)return false;f->duration=old->duration;memcpy(f->link,old->link,sizeof(f->link));
    unsigned tiles=pd_plane(d)/64,count=0;uint32_t pixels[64];
    for(unsigned l=0;l<d->capacity;l++)for(unsigned id=0;id<tiles;id++)if(tile_pixels(d,l,id,pixels))count++;
    if(count){f->refs=take(a,count*sizeof(Ref));if(!f->refs){free_frame(a,f);return false;}}
    for(unsigned l=0;l<d->capacity;l++)for(unsigned id=0;id<tiles;id++)if(tile_pixels(d,l,id,pixels)){
        Tile *t=intern(a,pixels);if(!t){free_frame(a,f);return false;}f->refs[f->count++]=(Ref){t,l*tiles+id};}
    if(!propagate_links(a,a->current,f,tiles)){free_frame(a,f);return false;}
    a->frames[a->current]=f;free_frame(a,old);a->visual=++visual_serial;return true;
}
static void restore(PdDocument *d,unsigned index){
    Animation *a=d->animation;Frame *f=a->frames[index];unsigned tiles=pd_plane(d)/64,cols=d->stride/8;
    for(unsigned l=0;l<d->capacity;l++)memset(pd_pixels(&d->layer[l]),0,pd_bytes(d));
    for(unsigned i=0;i<f->count;i++){Ref *r=&f->refs[i];unsigned l=r->position/tiles,id=r->position%tiles;uint32_t pixels[64];decode(r->tile,pixels);
        for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++)pd_pixels(&d->layer[l])[(id/cols*8+y)*d->stride+id%cols*8+x]=pixels[y*8+x];}
    a->current=index;
}
bool pd_playback_start(PdDocument *d,PdPlayback *p,uint32_t now){
    if(p->active||d->editing||pd_frame_count(d)<2||!pd_frame_snapshot(d))return false;
    unsigned f=pd_frame_index(d);int tag=pd_tag_at(d,f);const PdTag *t=tag>=0?pd_tag(d,(unsigned)tag):NULL;if(t&&t->from==t->to)t=NULL;
    p->from=t?t->from:0;p->to=t?t->to:pd_frame_count(d)-1;p->direction=t?t->direction:0;p->backward=p->direction==1||p->direction==3;
    p->origin=f;p->deadline=now+pd_frame_duration(d);p->active=true;return true;
}
/* Next frame in the playback range; ping-pong turns at either end. */
static unsigned playback_next(PdPlayback *p,unsigned i){
    if(p->from==p->to)return p->from;
    if(!p->backward){if(i<p->to)return i+1;if(p->direction>=2){p->backward=true;return i-1;}return p->from;}
    if(i>p->from)return i-1;if(p->direction>=2){p->backward=false;return i+1;}return p->to;
}
bool pd_playback_tick(PdDocument *d,PdPlayback *p,uint32_t now){
    if(!p->active||d->editing||(int32_t)(now-p->deadline)<0)return false;
    Animation *a=d->animation;unsigned cycle=0;for(unsigned i=p->from;i<=p->to;i++)cycle+=a->frames[i]->duration;
    if(p->direction>=2&&p->to>p->from)cycle=2*cycle-a->frames[p->from]->duration-a->frames[p->to]->duration;
    unsigned overdue=now-p->deadline;if(cycle&&overdue>=cycle)p->deadline+=(overdue/cycle)*cycle;
    unsigned index=a->current;if(index<p->from||index>p->to)index=p->backward?p->from:p->to;
    do{index=playback_next(p,index);p->deadline+=a->frames[index]->duration;}while((int32_t)(now-p->deadline)>=0);
    restore(d,index);return true;
}
bool pd_playback_seek(PdDocument *d,PdPlayback *p,unsigned frame){
    if(!p->active||d->editing||frame>=pd_frame_count(d))return false;restore(d,frame);return true;
}
void pd_playback_stop(PdDocument *d,PdPlayback *p){
    if(!p->active)return;restore(d,p->origin);p->active=false;
}
bool pd_frame_switch(PdDocument *d,unsigned index){
    if(d->editing||index>=pd_frame_count(d))return false;if(index==pd_frame_index(d))return true;
    if(!pd_frame_snapshot(d))return false;restore(d,index);return true;
}
bool pd_frame_select_packed(PdDocument *d,unsigned index){
    if(d->editing||!d->animation||index>=d->animation->count)return false;restore(d,index);return true;
}
static uint16_t new_link(const Animation *a,unsigned l){unsigned max=0;for(unsigned i=0;i<a->count;i++)if(a->frames[i]->link[l]>max)max=a->frames[i]->link[l];return (uint16_t)(max+1);}
/* Frames inserted at index shift later tags and extend a tag they land inside. */
static void tags_insert(Animation *a,unsigned index){for(unsigned i=0;i<a->tag_count;i++){PdTag *t=&a->tags[i];if(t->from>=index)t->from++;if(t->to>=index)t->to++;}}
static void tags_delete(Animation *a,unsigned index){
    for(unsigned i=0;i<a->tag_count;){PdTag *t=&a->tags[i];if(t->from==index&&t->to==index){memmove(t,t+1,(a->tag_count-i-1)*sizeof(*t));a->tag_count--;continue;}
        if(t->from>index)t->from--;if(t->to>=index)t->to--;i++;}
}
static bool frame_insert(PdDocument *d,unsigned mode);
bool pd_frame_add(PdDocument *d,bool duplicate){return frame_insert(d,duplicate?1:0);}
bool pd_frame_add_linked(PdDocument *d){return frame_insert(d,2);}
static bool frame_insert(PdDocument *d,unsigned mode){
    bool duplicate=mode!=0;
    if(d->editing||pd_frame_count(d)>=PD_FRAMES||!enable(d))return false;
    Animation *a=d->animation;Frame *f=take(a,sizeof(*f));if(!f)return false;
    if(!pd_frame_snapshot(d)){give(a,f);return false;}Frame *source=a->frames[a->current];*f=*source;f->refs=NULL;f->count=0;memset(f->link,0,sizeof(f->link));
    if(mode==2)for(unsigned l=0;l<PD_LAYERS;l++){if(!source->link[l])source->link[l]=new_link(a,l);f->link[l]=source->link[l];}
    if(duplicate&&source->count){f->refs=take(a,source->count*sizeof(Ref));if(!f->refs){give(a,f);return false;}memcpy(f->refs,source->refs,source->count*sizeof(Ref));f->count=source->count;for(unsigned i=0;i<f->count;i++)f->refs[i].tile->refs++;}
    unsigned index=a->current+1;memmove(a->frames+index+1,a->frames+index,(a->count-index)*sizeof(Frame *));a->frames[index]=f;a->count++;a->structure++;tags_insert(a,index);
    for(unsigned i=0;i<d->count;i++)if(d->history[i].frame>=index)d->history[i].frame++;
    restore(d,index);return true;
}
bool pd_frame_move(PdDocument *d,unsigned target){
    if(d->editing||!d->animation||target>=d->animation->count||target==d->animation->current)return false;
    uint8_t marked[PD_FRAMES]={0};marked[d->animation->current]=1;
    return pd_frames_move(d,marked,target,NULL);
}
/* Applies a permutation of the frame pointers. The working pixels stay with their Frame object,
 * so the current frame moves with its art; no snapshot or allocation is needed. */
bool pd_frames_move(PdDocument *d,const uint8_t *marked,unsigned start,uint16_t *new_index){
    if(d->editing||!d->animation||!marked)return false;Animation *a=d->animation;unsigned n=a->count,selected=0;
    for(unsigned i=0;i<n;i++)if(marked[i])selected++;
    if(!selected||selected>=n||start>n-selected)return false;
    uint16_t order[PD_FRAMES];unsigned at=0,pos=0;
    for(unsigned i=0;i<n;i++)if(!marked[i]){if(at==start)for(unsigned j=0;j<n;j++)if(marked[j])order[pos++]=j;order[pos++]=i;at++;}
    if(at==start)for(unsigned j=0;j<n;j++)if(marked[j])order[pos++]=j;
    bool changed=false;for(unsigned i=0;i<n;i++)if(order[i]!=(uint16_t)i){changed=true;break;}
    if(!changed)return false;
    uint8_t map[PD_FRAMES];for(unsigned i=0;i<n;i++)map[order[i]]=(uint8_t)i;
    Frame *next[PD_FRAMES];for(unsigned i=0;i<n;i++)next[i]=a->frames[order[i]];
    if(new_index)for(unsigned i=0;i<n;i++)new_index[i]=map[i];
    a->current=map[a->current];
    for(unsigned i=0;i<n;i++)a->frames[i]=next[i];
    for(unsigned i=0;i<d->count;i++)d->history[i].frame=map[d->history[i].frame];
    a->structure++;return true;
}
bool pd_frame_delete(PdDocument *d){
    if(d->editing||!d->animation||d->animation->count<2)return false;Animation *a=d->animation;unsigned at=a->current;
    free_frame(a,a->frames[at]);memmove(a->frames+at,a->frames+at+1,(a->count-at-1)*sizeof(Frame *));a->count--;a->structure++;tags_delete(a,at);
    /* Deleted-frame XOR history must never target a different frame. */
    d->count=d->cursor=d->history_used=0;restore(d,at<a->count?at:at-1);return true;
}
bool pd_frame_set_duration(PdDocument *d,unsigned ms){
    if(d->editing||ms<10||ms>60000||!enable(d))return false;Animation *a=d->animation;Frame *f=a->frames[a->current];if(f->duration!=ms){f->duration=ms;a->structure++;}return true;
}
bool pd_frame_set_all_durations(PdDocument *d,unsigned ms){
    if(d->editing||ms<10||ms>60000||!enable(d))return false;Animation *a=d->animation;bool changed=false;
    for(unsigned i=0;i<a->count;i++)if(a->frames[i]->duration!=ms){a->frames[i]->duration=ms;changed=true;}
    if(changed)a->structure++;return true;
}

typedef struct {FILE *f;uint32_t crc;size_t bytes;bool ok;} Stream;
static void out(Stream *s,const void *p,unsigned n){if(!s->ok)return;s->crc=(uint32_t)crc32(s->crc,p,n);s->bytes+=n;if(s->f&&fwrite(p,1,n,s->f)!=n)s->ok=false;}
static void number(Stream *s,unsigned v){unsigned char b[4];put32(b,v);out(s,b,4);}
static void payload(Stream *s,Animation *a,const PdDocument *d,unsigned base){
    number(s,d->enabled);number(s,d->active);
    for(unsigned l=0;l<8;l++){unsigned char p[4]={d->layer[l].visible,d->layer[l].opacity,d->layer[l].blend,d->layer[l].flags};out(s,p,4);}
    out(s,d->names,512);out(s,d->order,8);
    for(unsigned i=0;i<d->palette_count;i++)number(s,d->palette[i]);
    for(unsigned b=0;b<257;b++)for(Tile *t=a->buckets[b];t;t=t->next)if(t->disk!=UINT32_MAX){number(s,t->mode);number(s,t->bytes);out(s,t->data,t->bytes);}
    unsigned per_layer=pd_plane(d)/64,active_count=0;
    for(unsigned l=0;l<d->capacity;l++)for(unsigned id=0;id<per_layer;id++){uint32_t pixels[64],hash;unsigned mode;unsigned char data[257];if(!tile_pixels(d,l,id,pixels))continue;active_count++;
        unsigned bytes=encode_tile(pixels,data,&mode,&hash);Tile *t=find_tile(a,mode,bytes,hash,data);if(!t||t->disk==UINT32_MAX){number(s,mode);number(s,bytes);out(s,data,bytes);}}
    for(unsigned i=0;i<a->count;i++){Frame *f=a->frames[i];number(s,f->duration);number(s,i==a->current?active_count:f->count);
        if(i!=a->current){for(unsigned j=0;j<f->count;j++){number(s,f->refs[j].position);number(s,f->refs[j].tile->disk);}}
        else {unsigned next=base;for(unsigned l=0;l<d->capacity;l++)for(unsigned id=0;id<per_layer;id++){uint32_t pixels[64],hash;unsigned mode;unsigned char data[257];if(!tile_pixels(d,l,id,pixels))continue;
            unsigned bytes=encode_tile(pixels,data,&mode,&hash);Tile *t=find_tile(a,mode,bytes,hash,data);number(s,l*per_layer+id);number(s,t&&t->disk!=UINT32_MAX?t->disk:next++);}}
    }
    /* Version 11: linked-cel groups per frame and layer, then tags. */
    for(unsigned i=0;i<a->count;i++){unsigned char links[2*PD_LAYERS];for(unsigned l=0;l<PD_LAYERS;l++){links[2*l]=(unsigned char)a->frames[i]->link[l];links[2*l+1]=(unsigned char)(a->frames[i]->link[l]>>8);}out(s,links,sizeof(links));}
    number(s,a->tag_count);for(unsigned i=0;i<a->tag_count;i++){const PdTag *t=&a->tags[i];number(s,t->from);number(s,t->to);number(s,t->direction|t->repeat<<8);number(s,t->color);out(s,t->name,PD_TAG_NAME_MAX);}
}
bool pd_frames_write(PdDocument *d,FILE *f){
    if(d->editing||d->palette_count>256||!d->animation)return false;Animation *a=d->animation;unsigned tiles=0;
    for(unsigned b=0;b<257;b++)for(Tile *t=a->buckets[b];t;t=t->next)t->disk=UINT32_MAX;
    for(unsigned i=0;i<a->count;i++)if(i!=a->current)for(unsigned j=0;j<a->frames[i]->count;j++)a->frames[i]->refs[j].tile->disk=0;
    for(unsigned b=0;b<257;b++)for(Tile *t=a->buckets[b];t;t=t->next)if(t->disk!=UINT32_MAX)t->disk=tiles++;
    unsigned base=tiles,per_layer=pd_plane(d)/64;
    for(unsigned l=0;l<d->capacity;l++)for(unsigned id=0;id<per_layer;id++){uint32_t pixels[64],hash;unsigned mode;unsigned char data[257];if(!tile_pixels(d,l,id,pixels))continue;
        unsigned bytes=encode_tile(pixels,data,&mode,&hash);Tile *t=find_tile(a,mode,bytes,hash,data);if(!t||t->disk==UINT32_MAX)tiles++;}
    unsigned char h[64]={0};memcpy(h,"PDDA",4);put32(h+4,11);put32(h+8,d->width);put32(h+12,d->height);put32(h+16,a->count);put32(h+20,a->current);put32(h+24,tiles);put32(h+28,d->palette_count);
    Stream measure={NULL,0,0,true};payload(&measure,a,d,base);if(measure.bytes>64u*1024u*1024u)return false;put32(h+32,(unsigned)measure.bytes);put32(h+36,measure.crc);put32(h+40,(uint32_t)crc32(0,h,40));
    if(fwrite(h,1,64,f)!=64)return false;Stream write={f,0,0,true};payload(&write,a,d,base);return write.ok;
}
static bool input(Stream *s,void *p,unsigned n){if(!s->ok||n>s->bytes){s->ok=false;return false;}if(fread(p,1,n,s->f)!=n){s->ok=false;return false;}s->crc=(uint32_t)crc32(s->crc,p,n);s->bytes-=n;return true;}
static unsigned value(Stream *s){unsigned char b[4]={0};input(s,b,4);return get32(b);}
static bool valid_tile(unsigned mode,unsigned bytes,const unsigned char *p){
    if(mode==0)return bytes==256;if(mode==2)return bytes==4;if(mode!=1||bytes<69||p[0]<2||p[0]>47||bytes!=65u+4u*p[0])return false;
    for(unsigned i=0;i<64;i++)if(p[1+4*p[0]+i]>=p[0])return false;return true;
}
bool pd_frames_read(PdDocument *d,FILE *f){
    unsigned char h[64];if(d->editing||fread(h,1,64,f)!=64||memcmp(h,"PDDA",4)||(get32(h+4)!=10&&get32(h+4)!=11)||get32(h+40)!=(uint32_t)crc32(0,h,40))return false;
    for(unsigned i=44;i<64;i++)if(h[i])return false;
    unsigned w=get32(h+8),height=get32(h+12),count=get32(h+16),current=get32(h+20),tiles=get32(h+24),pal_count=get32(h+28),length=get32(h+32),capacity=pd_capacity(w,height,d->profile);
    if(!capacity||!count||count>256||current>=count||pal_count>256||length>64u*1024u*1024u||tiles>length/12)return false;
    Animation *a=calloc(1,sizeof(*a));if(!a)return false;size_t persistent_budget=budget_for(d->profile);
    unsigned stride=(w>128||height>128)?(w+7)/8*8:128,rows=(w>128||height>128)?(height+7)/8*8:128,per_layer=stride*rows/64;
    a->budget=persistent_budget+(size_t)capacity*per_layer*(sizeof(Tile)+256+sizeof(Ref)+3*(sizeof(size_t)+16));a->used=sizeof(*a)+16;
    Tile **lookup=tiles?take(a,tiles*sizeof(Tile *)):NULL;if(tiles&&!lookup){destroy(a);return false;}
    Stream s={f,0,length,true};uint32_t palette[256]={0};unsigned enabled=value(&s),active=value(&s);unsigned char properties[8][4],names[8][64],order[8];
    if(!enabled||enabled>=(1u<<capacity)||active>=capacity||!(enabled&(1u<<active))||!input(&s,properties,32)||!input(&s,names,512)||!input(&s,order,8))s.ok=false;
    unsigned seen=0;for(unsigned l=0;l<8&&s.ok;l++){unsigned char *p=properties[l];if(p[0]>1||p[2]>3||p[3]>3||!memchr(names[l],0,64)||order[l]>=8||(seen&(1u<<order[l])))s.ok=false;else seen|=1u<<order[l];}
    for(unsigned i=0;i<pal_count;i++)palette[i]=value(&s);
    for(unsigned i=0;i<tiles&&s.ok;i++){unsigned mode=value(&s),bytes=value(&s);unsigned char data[257];if(bytes>256||!input(&s,data,bytes)||!valid_tile(mode,bytes,data)){s.ok=false;break;}
        Tile *raw=malloc(sizeof(Tile)+bytes);if(!raw){s.ok=false;break;}raw->mode=mode;memcpy(raw->data,data,bytes);uint32_t pixels[64];decode(raw,pixels);free(raw);
        Tile *t=intern(a,pixels);if(!t){s.ok=false;break;}lookup[i]=t;
    }
    for(unsigned i=0;i<count&&s.ok;i++){
        Frame *frame=take(a,sizeof(*frame));if(!frame){s.ok=false;break;}a->frames[a->count++]=frame;
        unsigned duration=value(&s),refs=value(&s);
        if(duration<10||duration>60000||refs>capacity*per_layer){s.ok=false;break;}
        frame->duration=duration;if(refs){frame->refs=take(a,refs*sizeof(Ref));if(!frame->refs){s.ok=false;break;}}
        unsigned previous=0;for(unsigned j=0;j<refs&&s.ok;j++){unsigned position=value(&s),tile=value(&s);
            if(!s.ok||position>=capacity*per_layer||(j&&position<=previous)||tile>=tiles){s.ok=false;break;}
            previous=position;frame->refs[frame->count++]=(Ref){lookup[tile],position};lookup[tile]->refs++;}
    }
    if(get32(h+4)==11&&s.ok&&a->count==count){
        for(unsigned i=0;i<count&&s.ok;i++){unsigned char links[2*PD_LAYERS];if(input(&s,links,sizeof(links)))for(unsigned l=0;l<PD_LAYERS;l++)a->frames[i]->link[l]=(uint16_t)(links[2*l]|links[2*l+1]<<8);}
        unsigned tags=value(&s);if(tags>PD_TAGS)s.ok=false;
        for(unsigned i=0;i<tags&&s.ok;i++){PdTag *t=&a->tags[i];t->from=value(&s);t->to=value(&s);unsigned mode=value(&s);t->direction=mode&255;t->repeat=mode>>8;t->color=value(&s);
            if(!input(&s,t->name,PD_TAG_NAME_MAX)||t->from>t->to||t->to>=count||t->direction>3||t->repeat>65535||t->color>0xffffff||!memchr(t->name,0,PD_TAG_NAME_MAX))s.ok=false;}
        if(s.ok)a->tag_count=tags;
    }
    if(s.bytes||s.crc!=get32(h+36)||fgetc(f)!=EOF||ferror(f))s.ok=false;
    for(unsigned i=0;i<tiles;i++)if(lookup&&lookup[i])release(a,lookup[i]);give(a,lookup);
    if(!s.ok||a->count!=count){destroy(a);return false;}
    Frame *live=a->frames[current];size_t retained=a->used;
    for(unsigned b=0;b<257;b++)for(Tile *t=a->buckets[b];t;t=t->next)t->disk=0;
    for(unsigned i=0;i<live->count;i++)live->refs[i].tile->disk++;
    for(unsigned b=0;b<257;b++)for(Tile *t=a->buckets[b];t;t=t->next)if(t->disk==t->refs)retained-=sizeof(*t)+t->bytes+sizeof(size_t)+16;
    if(live->refs)retained-=live->count*sizeof(Ref)+sizeof(size_t)+16;
    if(retained>persistent_budget||!pd_create(d,w,height,d->profile)){destroy(a);return false;}
    a->visual=++visual_serial;d->animation=a;d->enabled=(uint8_t)enabled;d->active=active;memcpy(d->names,names,512);memcpy(d->order,order,8);
    for(unsigned l=0;l<8;l++){d->layer[l].visible=properties[l][0];d->layer[l].opacity=properties[l][1];d->layer[l].blend=properties[l][2];d->layer[l].flags=properties[l][3];}
    memcpy(d->palette,palette,sizeof(palette));memcpy(d->saved_palette,palette,sizeof(palette));d->palette_count=d->saved_palette_count=pal_count;restore(d,current);
    for(unsigned i=0;i<live->count;i++)release(a,live->refs[i].tile);give(a,live->refs);live->refs=NULL;live->count=0;a->budget=persistent_budget;
    pd_composite(d);return true;
}
bool pd_frames_verify(FILE *f){
    unsigned char h[64],buf[1024];if(fread(h,1,64,f)!=64||memcmp(h,"PDDA",4)||get32(h+40)!=(uint32_t)crc32(0,h,40))return false;
    unsigned left=get32(h+32);if(left>64u*1024u*1024u)return false;uint32_t crc=0;
    while(left){unsigned n=left>sizeof(buf)?sizeof(buf):left;if(fread(buf,1,n,f)!=n)return false;crc=(uint32_t)crc32(crc,buf,n);left-=n;}
    return crc==get32(h+36)&&fgetc(f)==EOF&&!ferror(f);
}
