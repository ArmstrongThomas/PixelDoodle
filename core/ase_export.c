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
bool pd_ase_stream(PdDocument *d,FILE *f,uint32_t *crc,size_t *bytes,char error[64]){
    unsigned layers=0,names=0,count=pd_frame_count(d);for(unsigned i=0;i<8;i++)if(d->enabled&(1u<<i)){layers++;names+=24+(unsigned)strlen(d->names[i]);}
    size_t cel_size=26+(size_t)d->width*d->height*4,frame_size=16+layers*cel_size,palette_size=d->palette_count?26+d->palette_count*6:0,total=128+count*frame_size+names+palette_size;
    if(d->editing||total>64u*1024u*1024u){snprintf(error,64,"Animation ASE exceeds export limit");return false;}
    PdPlayback playback={0};if(count>1&&!pd_playback_start(d,&playback,0)){snprintf(error,64,"Not enough memory to snapshot animation");return false;}
    if(count>1)pd_playback_seek(d,&playback,0);
    unsigned char header[128]={0};put32(header,(unsigned)total);put16(header+4,0xa5e0);put16(header+6,count);put16(header+8,d->width);put16(header+10,d->height);put16(header+12,32);put32(header+14,1);put16(header+18,pd_frame_duration(d));put16(header+32,d->palette_count);header[34]=header[35]=1;
    AseWriter w={.f=f,.ok=true};output(&w,header,sizeof(header));
    for(unsigned frame=0;frame<count&&w.ok;frame++){
        if(count>1)pd_playback_seek(d,&playback,frame);
        unsigned char frame_header[16]={0};put32(frame_header,(unsigned)(frame_size+(frame?0:names+palette_size)));put16(frame_header+4,0xf1fa);put16(frame_header+6,layers+(frame?0:layers+(d->palette_count?1:0)));put16(frame_header+8,pd_frame_duration(d));output(&w,frame_header,sizeof(frame_header));
        if(!frame)for(unsigned k=0;k<8;k++){unsigned l=d->order[k];if(!(d->enabled&(1u<<l)))continue;
            unsigned name=(unsigned)strlen(d->names[l]);unsigned char chunk[24]={0};put32(chunk,24+name);put16(chunk+4,0x2004);put16(chunk+6,((d->layer[l].flags&PD_LAYER_LOCK)?0:2)|d->layer[l].visible);put16(chunk+16,d->layer[l].blend);chunk[18]=d->layer[l].opacity;put16(chunk+22,name);output(&w,chunk,sizeof(chunk));output(&w,d->names[l],name);
        }
        unsigned compact=0;for(unsigned k=0;k<8&&w.ok;k++){unsigned l=d->order[k];if(!(d->enabled&(1u<<l)))continue;
            unsigned char chunk[26]={0};put32(chunk,(unsigned)cel_size);put16(chunk+4,0x2005);put16(chunk+6,compact++);chunk[12]=255;put16(chunk+22,d->width);put16(chunk+24,d->height);output(&w,chunk,sizeof(chunk));
            unsigned char row[512*4];for(unsigned y=0;y<d->height&&w.ok;y++){
                for(unsigned x=0;x<d->width;x++){uint32_t c=pd_pixels(&d->layer[l])[y*d->stride+x];row[x*4]=(unsigned char)(c>>16);row[x*4+1]=(unsigned char)(c>>8);row[x*4+2]=(unsigned char)c;row[x*4+3]=(unsigned char)(c>>24);}output(&w,row,d->width*4);
            }
        }
        if(!frame&&d->palette_count){unsigned char chunk[26]={0};put32(chunk,(unsigned)palette_size);put16(chunk+4,0x2019);put32(chunk+6,d->palette_count);put32(chunk+14,d->palette_count-1);output(&w,chunk,sizeof(chunk));
            for(unsigned i=0;i<d->palette_count;i++){uint32_t c=d->palette[i];unsigned char entry[6]={0,0,(unsigned char)(c>>16),(unsigned char)(c>>8),(unsigned char)c,(unsigned char)(c>>24)};output(&w,entry,sizeof(entry));}
        }
    }
    pd_playback_stop(d,&playback);pd_composite(d);*crc=w.crc;*bytes=w.size;
    if(!w.ok||w.size!=total){snprintf(error,64,"Animation ASE write failed");return false;}return true;
}
