#include "ase_export.h"
#include "animation.h"
#include "zlib.h"
#include <string.h>

typedef struct { FILE *f; size_t size; uint32_t crc; bool ok; } AseWriter;
static void put16(unsigned char *p,unsigned n){p[0]=(unsigned char)n;p[1]=(unsigned char)(n>>8);}
static void put32(unsigned char *p,unsigned n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static void output(AseWriter *w,const void *p,size_t n){
    if(!w->ok)return;if(n>64u*1024u*1024u-w->size||fwrite(p,1,n,w->f)!=n){w->ok=false;return;}
    w->crc=(uint32_t)crc32(w->crc,p,(uInt)n);w->size+=n;
}
/* Tags chunk 0x2018 plus one user-data chunk per tag carrying the modern colour. */
static size_t tags_bytes(const PdDocument *d){
    size_t n=16;for(unsigned i=0;i<pd_tag_count(d);i++)n+=19+strlen(pd_tag(d,i)->name);return n;
}
static void write_tags(AseWriter *w,const PdDocument *d){
    unsigned count=pd_tag_count(d);unsigned char chunk[29];memset(chunk,0,sizeof(chunk));
    put32(chunk,(unsigned)tags_bytes(d));put16(chunk+4,0x2018);put16(chunk+6,count);output(w,chunk,16);
    for(unsigned i=0;i<count;i++){
        const PdTag *t=pd_tag(d,i);unsigned len=(unsigned)strlen(t->name);memset(chunk,0,sizeof(chunk));
        put16(chunk,t->from);put16(chunk+2,t->to);chunk[4]=(unsigned char)t->direction;put16(chunk+5,t->repeat);
        chunk[13]=(unsigned char)(t->color>>16);chunk[14]=(unsigned char)(t->color>>8);chunk[15]=(unsigned char)t->color;
        put16(chunk+17,len);output(w,chunk,19);if(len)output(w,t->name,len);
    }
    for(unsigned i=0;i<count;i++){const PdTag *t=pd_tag(d,i);unsigned char ud[14]={0};
        put32(ud,14);put16(ud+4,0x2020);put32(ud+6,2);ud[10]=(unsigned char)(t->color>>16);ud[11]=(unsigned char)(t->color>>8);ud[12]=(unsigned char)t->color;ud[13]=255;output(w,ud,sizeof(ud));}
}
bool pd_ase_stream(PdDocument *d,FILE *f,uint32_t *crc,size_t *bytes,char error[64]){
    unsigned layers=0,count=pd_frame_count(d);size_t names=0;
    for(unsigned i=0;i<8;i++)if(d->enabled&(1u<<i)){layers++;names+=24+(unsigned)strlen(d->names[i]);}
    size_t raw_cel=26+(size_t)d->width*d->height*4,linked_cel=24;
    size_t palette_size=d->palette_count?26+d->palette_count*6:0,tags=pd_tag_count(d)?tags_bytes(d)+14u*pd_tag_count(d):0,total=128;
    /* The first frame of each linked-cel group is stored raw; later members link to it. */
    unsigned rep[PD_LAYERS];for(unsigned l=0;l<PD_LAYERS;l++)rep[l]=UINT_MAX;
    for(unsigned frame=0;frame<count;frame++)for(unsigned l=0;l<PD_LAYERS;l++)if((d->enabled&(1u<<l))&&pd_cel_link(d,frame,l)&&rep[l]==UINT_MAX)rep[l]=frame;
    for(unsigned frame=0;frame<count;frame++){
        size_t frame_size=16+(frame?0:names+(palette_size?palette_size:0)+(tags?tags:0));
        for(unsigned l=0;l<PD_LAYERS;l++)if(d->enabled&(1u<<l))frame_size+=pd_cel_link(d,frame,l)&&rep[l]!=frame?linked_cel:raw_cel;
        total+=frame_size;
    }
    if(d->editing||total>64u*1024u*1024u){snprintf(error,64,"Animation ASE exceeds export limit");return false;}
    PdPlayback playback={0};if(count>1&&!pd_playback_start(d,&playback,0)){snprintf(error,64,"Not enough memory to snapshot animation");return false;}
    if(count>1)pd_playback_seek(d,&playback,0);
    unsigned char header[128]={0};put32(header,(unsigned)total);put16(header+4,0xa5e0);put16(header+6,count);put16(header+8,d->width);put16(header+10,d->height);put16(header+12,32);put32(header+14,1);put16(header+18,pd_frame_duration(d));put16(header+32,d->palette_count);header[34]=header[35]=1;
    AseWriter w={.f=f,.ok=true};output(&w,header,sizeof(header));
    for(unsigned frame=0;frame<count&&w.ok;frame++){
        if(count>1)pd_playback_seek(d,&playback,frame);
        size_t frame_size=16+(frame?0:names+(palette_size?palette_size:0)+(tags?tags:0));
        unsigned chunks=frame?0:layers;
        for(unsigned l=0;l<PD_LAYERS;l++)if(d->enabled&(1u<<l)){bool linked=pd_cel_link(d,frame,l)&&rep[l]!=frame;frame_size+=linked?linked_cel:raw_cel;chunks++;}
        if(!frame&&palette_size)chunks++;if(!frame&&tags)chunks+=1+pd_tag_count(d);
        unsigned char frame_header[16]={0};put32(frame_header,(unsigned)frame_size);put16(frame_header+4,0xf1fa);put16(frame_header+6,chunks);put16(frame_header+8,pd_frame_duration(d));put32(frame_header+12,chunks);output(&w,frame_header,sizeof(frame_header));
        if(!frame)for(unsigned k=0;k<8;k++){unsigned l=d->order[k];if(!(d->enabled&(1u<<l)))continue;
            unsigned name=(unsigned)strlen(d->names[l]);unsigned char chunk[24]={0};put32(chunk,24+name);put16(chunk+4,0x2004);put16(chunk+6,((d->layer[l].flags&PD_LAYER_LOCK)?0:2)|d->layer[l].visible);put16(chunk+16,d->layer[l].blend);chunk[18]=d->layer[l].opacity;put16(chunk+22,name);output(&w,chunk,sizeof(chunk));output(&w,d->names[l],name);
        }
        unsigned compact=0;for(unsigned k=0;k<8&&w.ok;k++){unsigned l=d->order[k];if(!(d->enabled&(1u<<l)))continue;
            unsigned char chunk[26]={0};unsigned at=compact++;
            if(pd_cel_link(d,frame,l)&&rep[l]!=frame){size_t n=24;put32(chunk,(unsigned)n);put16(chunk+4,0x2005);put16(chunk+6,at);chunk[12]=255;put16(chunk+13,1);put16(chunk+22,rep[l]);output(&w,chunk,n);continue;}
            put32(chunk,(unsigned)raw_cel);put16(chunk+4,0x2005);put16(chunk+6,at);chunk[12]=255;put16(chunk+22,d->width);put16(chunk+24,d->height);output(&w,chunk,sizeof(chunk));
            unsigned char row[512*4];for(unsigned y=0;y<d->height&&w.ok;y++){
                for(unsigned x=0;x<d->width;x++){uint32_t c=pd_pixels(&d->layer[l])[y*d->stride+x];row[x*4]=(unsigned char)(c>>16);row[x*4+1]=(unsigned char)(c>>8);row[x*4+2]=(unsigned char)c;row[x*4+3]=(unsigned char)(c>>24);}output(&w,row,d->width*4);
            }
        }
        if(!frame&&d->palette_count){unsigned char chunk[26]={0};put32(chunk,(unsigned)palette_size);put16(chunk+4,0x2019);put32(chunk+6,d->palette_count);put32(chunk+14,d->palette_count-1);output(&w,chunk,sizeof(chunk));
            for(unsigned i=0;i<d->palette_count;i++){uint32_t c=d->palette[i];unsigned char entry[6]={0,0,(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c,(unsigned char)(c>>24)};output(&w,entry,sizeof(entry));}
        }
        if(!frame&&tags)write_tags(&w,d);
    }
    pd_playback_stop(d,&playback);pd_composite(d);*crc=w.crc;*bytes=w.size;
    if(!w.ok||w.size!=total){snprintf(error,64,"Animation ASE write failed");return false;}return true;
}
