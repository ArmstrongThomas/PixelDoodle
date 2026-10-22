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
} Frame;
struct PdAnimation {
    Tile *buckets[257];
    Frame *frames[PD_FRAMES];
    unsigned count,current;
    uint64_t structure,saved_structure;
    size_t used,budget;
};
typedef struct PdAnimation Animation;
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
    return true;
}
size_t pd_frame_memory(const PdDocument *d){return d->animation?d->animation->used:0;}
size_t pd_frame_budget(const PdDocument *d){return d->animation?d->animation->budget:budget_for(d->profile);}
bool pd_frames_dirty(const PdDocument *d){return d->animation&&d->animation->structure!=d->animation->saved_structure;}
uint64_t pd_frames_generation(const PdDocument *d){return d->animation?d->animation->structure:0;}
void pd_frames_saved(PdDocument *d){if(d->animation)d->animation->saved_structure=d->animation->structure;}
static bool enable(PdDocument *d){
    if(d->animation)return true;Animation *a=calloc(1,sizeof(*a));if(!a)return false;a->budget=budget_for(d->profile);a->used=sizeof(*a)+16;
    Frame *f=take(a,sizeof(*f));if(!f){free(a);return false;}f->duration=100;a->frames[0]=f;a->count=1;d->animation=a;return true;
}
static bool tile_pixels(const PdDocument *d,unsigned l,unsigned id,uint32_t pixels[64]){
    unsigned cols=d->stride/8;bool any=false;
    for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){unsigned xx=id%cols*8+x,yy=id/cols*8+y;uint32_t c=xx<d->width&&yy<d->height?pd_pixels(&d->layer[l])[yy*d->stride+xx]:0;pixels[y*8+x]=c;if(c)any=true;}return any;
}
bool pd_frame_snapshot(PdDocument *d){
    if(d->editing)return false;if(!d->animation)return true;Animation *a=d->animation;Frame *old=a->frames[a->current];
    Frame *f=take(a,sizeof(*f));if(!f)return false;f->duration=old->duration;
    unsigned tiles=pd_plane(d)/64,count=0;uint32_t pixels[64];
    for(unsigned l=0;l<d->capacity;l++)for(unsigned id=0;id<tiles;id++)if(tile_pixels(d,l,id,pixels))count++;
    if(count){f->refs=take(a,count*sizeof(Ref));if(!f->refs){free_frame(a,f);return false;}}
    for(unsigned l=0;l<d->capacity;l++)for(unsigned id=0;id<tiles;id++)if(tile_pixels(d,l,id,pixels)){
        Tile *t=intern(a,pixels);if(!t){free_frame(a,f);return false;}f->refs[f->count++]=(Ref){t,l*tiles+id};}
    a->frames[a->current]=f;free_frame(a,old);return true;
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
    p->origin=pd_frame_index(d);p->deadline=now+pd_frame_duration(d);p->active=true;return true;
}
bool pd_playback_tick(PdDocument *d,PdPlayback *p,uint32_t now){
    if(!p->active||d->editing||(int32_t)(now-p->deadline)<0)return false;
    Animation *a=d->animation;unsigned cycle=0;for(unsigned i=0;i<a->count;i++)cycle+=a->frames[i]->duration;
    unsigned overdue=now-p->deadline;if(overdue>=cycle)p->deadline+=(overdue/cycle)*cycle;
    unsigned index=a->current;
    do{index=(index+1)%a->count;p->deadline+=a->frames[index]->duration;}while((int32_t)(now-p->deadline)>=0);
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
bool pd_frame_add(PdDocument *d,bool duplicate){
    if(d->editing||pd_frame_count(d)>=PD_FRAMES||!enable(d))return false;
    Animation *a=d->animation;Frame *f=take(a,sizeof(*f));if(!f)return false;
    if(!pd_frame_snapshot(d)){give(a,f);return false;}Frame *source=a->frames[a->current];*f=*source;f->refs=NULL;f->count=0;
    if(duplicate&&source->count){f->refs=take(a,source->count*sizeof(Ref));if(!f->refs){give(a,f);return false;}memcpy(f->refs,source->refs,source->count*sizeof(Ref));f->count=source->count;for(unsigned i=0;i<f->count;i++)f->refs[i].tile->refs++;}
    unsigned index=a->current+1;memmove(a->frames+index+1,a->frames+index,(a->count-index)*sizeof(Frame *));a->frames[index]=f;a->count++;a->structure++;
    for(unsigned i=0;i<d->count;i++)if(d->history[i].frame>=index)d->history[i].frame++;
    restore(d,index);return true;
}
bool pd_frame_delete(PdDocument *d){
    if(d->editing||!d->animation||d->animation->count<2)return false;Animation *a=d->animation;unsigned at=a->current;
    free_frame(a,a->frames[at]);memmove(a->frames+at,a->frames+at+1,(a->count-at-1)*sizeof(Frame *));a->count--;a->structure++;
    /* Deleted-frame XOR history must never target a different frame. */
    d->count=d->cursor=d->history_used=0;restore(d,at<a->count?at:at-1);return true;
}
bool pd_frame_set_duration(PdDocument *d,unsigned ms){
    if(d->editing||ms<10||ms>60000||!enable(d))return false;Animation *a=d->animation;Frame *f=a->frames[a->current];if(f->duration!=ms){f->duration=ms;a->structure++;}return true;
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
}
bool pd_frames_write(PdDocument *d,FILE *f){
    if(d->editing||d->palette_count>256||!d->animation)return false;Animation *a=d->animation;unsigned tiles=0;
    for(unsigned b=0;b<257;b++)for(Tile *t=a->buckets[b];t;t=t->next)t->disk=UINT32_MAX;
    for(unsigned i=0;i<a->count;i++)if(i!=a->current)for(unsigned j=0;j<a->frames[i]->count;j++)a->frames[i]->refs[j].tile->disk=0;
    for(unsigned b=0;b<257;b++)for(Tile *t=a->buckets[b];t;t=t->next)if(t->disk!=UINT32_MAX)t->disk=tiles++;
    unsigned base=tiles,per_layer=pd_plane(d)/64;
    for(unsigned l=0;l<d->capacity;l++)for(unsigned id=0;id<per_layer;id++){uint32_t pixels[64],hash;unsigned mode;unsigned char data[257];if(!tile_pixels(d,l,id,pixels))continue;
        unsigned bytes=encode_tile(pixels,data,&mode,&hash);Tile *t=find_tile(a,mode,bytes,hash,data);if(!t||t->disk==UINT32_MAX)tiles++;}
    unsigned char h[64]={0};memcpy(h,"PDDA",4);put32(h+4,10);put32(h+8,d->width);put32(h+12,d->height);put32(h+16,a->count);put32(h+20,a->current);put32(h+24,tiles);put32(h+28,d->palette_count);
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
    unsigned char h[64];if(d->editing||fread(h,1,64,f)!=64||memcmp(h,"PDDA",4)||get32(h+4)!=10||get32(h+40)!=(uint32_t)crc32(0,h,40))return false;
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
    if(s.bytes||s.crc!=get32(h+36)||fgetc(f)!=EOF||ferror(f))s.ok=false;
    for(unsigned i=0;i<tiles;i++)if(lookup&&lookup[i])release(a,lookup[i]);give(a,lookup);
    if(!s.ok||a->count!=count){destroy(a);return false;}
    Frame *live=a->frames[current];size_t retained=a->used;
    for(unsigned b=0;b<257;b++)for(Tile *t=a->buckets[b];t;t=t->next)t->disk=0;
    for(unsigned i=0;i<live->count;i++)live->refs[i].tile->disk++;
    for(unsigned b=0;b<257;b++)for(Tile *t=a->buckets[b];t;t=t->next)if(t->disk==t->refs)retained-=sizeof(*t)+t->bytes+sizeof(size_t)+16;
    if(live->refs)retained-=live->count*sizeof(Ref)+sizeof(size_t)+16;
    if(retained>persistent_budget||!pd_create(d,w,height,d->profile)){destroy(a);return false;}
    d->animation=a;d->enabled=(uint8_t)enabled;d->active=active;memcpy(d->names,names,512);memcpy(d->order,order,8);
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
