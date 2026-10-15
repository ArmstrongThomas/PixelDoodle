#include "pixel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#define PD_SYNC(f) _commit(_fileno(f))
#else
#include <unistd.h>
#define PD_SYNC(f) fsync(fileno(f))
#endif
/* Prototype PDD1: 32-byte LE header, 8 metadata bytes, then 4 RGBA layers.
 * Header: magic, version, width, height, layers, sequence, payload bytes, CRC32.
 * CRC covers header bytes 0..27 and payload. No native structs on disk. */
#define PAYLOAD (8u+4*PD_PIXELS*4u)
static uint32_t crc(uint32_t c,const unsigned char *p,size_t n) {
    while(n--){c^=*p++;for(unsigned j=0;j<8;j++)c=(c>>1)^(0xedb88320u & (0u-(c&1)));}return c;
}
static void put(unsigned char *p,uint32_t x){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(x>>(8*i));}
static uint32_t get(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static bool path(char p[512],const char *dir,unsigned slot){int n=snprintf(p,512,"%s/project%u.pdd",dir,slot);return n>0&&n<512;}
/* -2: inaccessible, -1: invalid, 0: absent, 1: valid */
static int read_slot(const char *dir,unsigned slot,unsigned char *out,uint32_t *sequence) {
    char name[512];unsigned char h[32],buf[4096];if(!path(name,dir,slot))return -2;
    FILE *f=fopen(name,"rb");if(!f)return errno==ENOENT?0:-2;
    bool ok=fread(h,1,32,f)==32;
    if(ok)ok=!memcmp(h,"PDD1",4)&&get(h+4)==1&&get(h+8)==PD_SIDE&&get(h+12)==PD_SIDE&&get(h+16)==4&&get(h+20)>0&&get(h+24)==PAYLOAD;
    uint32_t c=ok?crc(~0u,h,28):0;unsigned at=0;
    while(ok&&at<PAYLOAD){unsigned n=PAYLOAD-at;if(n>sizeof(buf))n=sizeof(buf);
        if(fread(buf,1,n,f)!=n){ok=false;break;}c=crc(c,buf,n);if(out)memcpy(out+at,buf,n);at+=n;
    }
    if(ok)ok=(~c==get(h+28))&&fgetc(f)==EOF&&!ferror(f);
    if(fclose(f)!=0)ok=false;
    if(ok&&out)for(unsigned i=0;i<4;i++)if(out[i*2]>1)ok=false;
    if(ok)*sequence=get(h+20);
    return ok?1:-1;
}
static bool encode(PdDocument *d,unsigned char *p){
    for(unsigned l=0;l<4;l++){
        p[l*2]=d->layer[l].visible;p[l*2+1]=d->layer[l].opacity;
        for(unsigned i=0;i<PD_PIXELS;i++)put(p+8+(l*PD_PIXELS+i)*4,d->layer[l].pixels[i]);
    }return true;
}
int pd_load(PdDocument *d,const char *dir) {
    if(d->editing)return -1;
    uint32_t seq[2]={0};int state[2];
    unsigned char *p=malloc(PAYLOAD);if(!p)return -1;
    for(unsigned i=0;i<2;i++)state[i]=read_slot(dir,i,p,&seq[i]);
    if(state[0]!=1&&state[1]!=1){free(p);return state[0]==0&&state[1]==0?0:-1;}
    unsigned slot=state[0]==1&&(state[1]!=1||seq[0]>=seq[1])?0:1;
    if(read_slot(dir,slot,p,&seq[slot])!=1){free(p);return -1;}
    pd_dispose(d);unsigned profile=d->profile;pd_init(d);d->profile=profile;d->enabled=15; /* Legacy PDD1 always had four layers. */
    for(unsigned l=0;l<4;l++){
        d->layer[l].visible=p[l*2];d->layer[l].opacity=p[l*2+1];
        for(unsigned i=0;i<PD_PIXELS;i++)d->layer[l].pixels[i]=get(p+8+(l*PD_PIXELS+i)*4);
    }
    d->save_sequence=seq[slot];free(p);pd_composite(d);return 1;
}
bool pd_save(PdDocument *d,const char *dir) {
    if(d->width!=128||d->height!=128)return false;
    /* Legacy writer cannot represent layer existence; named saves use PDD3. */
    if(d->editing||d->palette_count||d->enabled!=15)return false;
    for(unsigned i=0;i<4;i++)if(d->order[i]!=i||(d->layer[i].blend||d->layer[i].flags))return false;
    unsigned char *p=malloc(PAYLOAD);if(!p)return false;
    uint32_t seq[2]={0};int s[2];
    for(unsigned i=0;i<2;i++)s[i]=read_slot(dir,i,p,&seq[i]);
    if(s[0]==-2||s[1]==-2){free(p);return false;}
    unsigned slot=s[0]!=1?0:s[1]!=1?1:seq[0]<=seq[1]?0:1;
    uint32_t newest=seq[0]>seq[1]?seq[0]:seq[1];
    if(newest==UINT32_MAX){free(p);return false;}
    unsigned char h[32]={0};memcpy(h,"PDD1",4);put(h+4,1);put(h+8,PD_SIDE);put(h+12,PD_SIDE);put(h+16,4);put(h+20,newest+1);put(h+24,PAYLOAD);
    encode(d,p);put(h+28,~crc(crc(~0u,h,28),p,PAYLOAD));
    char name[512];if(!path(name,dir,slot)){free(p);return false;}
    FILE *f=fopen(name,"wb");if(!f){free(p);return false;}
    bool ok=fwrite(h,1,32,f)==32&&fwrite(p,1,PAYLOAD,f)==PAYLOAD;
    if(fflush(f)!=0)ok=false;
    if(PD_SYNC(f)!=0)ok=false;
    if(fclose(f)!=0)ok=false;
    uint32_t verified=0;
    if(ok)ok=read_slot(dir,slot,p,&verified)==1&&verified==newest+1;
    if(ok){d->saved_revision=d->revision;d->save_sequence=verified;}
    free(p);return ok;
}
