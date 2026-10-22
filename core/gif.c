#include "gif.h"
#include "animation.h"
#include "files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <io.h>
#define GIF_SYNC(f) _commit(_fileno(f))
#else
#include <unistd.h>
#define GIF_SYNC(f) fsync(fileno(f))
#endif
typedef struct { FILE *f; size_t bytes,limit; bool ok; unsigned bits,bit_count,count; unsigned char block[255]; } Writer;
static void bytes(Writer *w,const void *p,size_t n){if(!w->ok)return;if(n>w->limit-w->bytes||fwrite(p,1,n,w->f)!=n){w->ok=false;return;}w->bytes+=n;}
static void byte(Writer *w,unsigned n){unsigned char b=(unsigned char)n;bytes(w,&b,1);}
static void word(Writer *w,unsigned n){byte(w,n);byte(w,n>>8);}
static void flush_block(Writer *w){if(w->count){byte(w,w->count);bytes(w,w->block,w->count);w->count=0;}}
static void packed_byte(Writer *w,unsigned n){w->block[w->count++]=(unsigned char)n;if(w->count==255)flush_block(w);}
static void code(Writer *w,unsigned n){w->bits|=n<<w->bit_count;w->bit_count+=9;while(w->bit_count>=8){packed_byte(w,w->bits&255);w->bits>>=8;w->bit_count-=8;}}
/* Reset before the 9-bit dictionary grows; memory and code width stay bounded. */
static void lzw(Writer *w,const unsigned char *pixels,unsigned count){
    unsigned keys[1024];unsigned short values[1024];memset(values,0,sizeof(values));
    unsigned next=258,prefix=pixels[0];byte(w,8);code(w,256);
    for(unsigned i=1;i<count&&w->ok;i++){
        unsigned key=(prefix<<8)|pixels[i],slot=(key*2654435761u)>>22;
        while(values[slot]&&keys[slot]!=key)slot=(slot+1)&1023;
        if(values[slot])prefix=values[slot];
        else {code(w,prefix);keys[slot]=key;values[slot]=(unsigned short)next++;prefix=pixels[i];
            if(next==510){code(w,256);memset(values,0,sizeof(values));next=258;}}
    }
    code(w,prefix);code(w,257);if(w->bit_count)packed_byte(w,w->bits&255);w->bits=w->bit_count=0;flush_block(w);byte(w,0);
}
static void frame(Writer *w,PdDocument *d,unsigned char *indices){
    unsigned colors[256]={0},keys[512]={0};unsigned short slots[512]={0};unsigned count=1;bool exact=true;
    pd_composite(d);const uint32_t *pixels=pd_image(d);
    for(unsigned y=0;y<d->height&&exact;y++)for(unsigned x=0;x<d->width;x++){
        uint32_t c=pixels[y*d->stride+x];unsigned index=0;
        if((c>>24)>=128){c&=0xffffff;unsigned h=(c*2654435761u)>>23;while(slots[h]&&keys[h]!=c)h=(h+1)&511;
            if(!slots[h]){if(count==256){exact=false;break;}keys[h]=c;slots[h]=(unsigned short)count;colors[count++]=c;}index=slots[h];}
        indices[y*d->width+x]=(unsigned char)index;
    }
    if(!exact){
        memset(colors,0,sizeof(colors));for(unsigned r=0;r<6;r++)for(unsigned g=0;g<6;g++)for(unsigned b=0;b<6;b++)colors[1+r*36+g*6+b]=(r*51<<16)|(g*51<<8)|b*51;
        for(unsigned y=0;y<d->height;y++)for(unsigned x=0;x<d->width;x++){
            uint32_t c=pixels[y*d->stride+x];indices[y*d->width+x]=(unsigned char)((c>>24)<128?0:1+((((c>>16)&255)+25)/51)*36+((((c>>8)&255)+25)/51)*6+((c&255)+25)/51);
        }
    }
    byte(w,0x21);byte(w,0xf9);byte(w,4);byte(w,9);word(w,(pd_frame_duration(d)+5)/10);byte(w,0);byte(w,0);
    byte(w,0x2c);word(w,0);word(w,0);word(w,d->width);word(w,d->height);byte(w,0x87);
    for(unsigned i=0;i<256;i++){byte(w,colors[i]>>16);byte(w,colors[i]>>8);byte(w,colors[i]);}
    lzw(w,indices,d->width*d->height);
}
bool pd_gif_write(PdDocument *d,const char *path,size_t limit,char error[64]){
    char temp[PD_PATH];PdPlayback p={0};
    if(d->editing||!d->width||!d->height||limit<32){snprintf(error,64,"Invalid GIF export");return false;}
    if(snprintf(temp,sizeof(temp),"%s.tmp",path)>=(int)sizeof(temp)||pd_exists(path)||pd_exists(temp)){snprintf(error,64,"GIF output or temporary file exists");return false;}
    unsigned char *indices=malloc(d->width*d->height);if(!indices){snprintf(error,64,"Not enough memory for GIF");return false;}
    unsigned count=pd_frame_count(d);if(count>1&&!pd_playback_start(d,&p,0)){free(indices);snprintf(error,64,"Not enough memory to snapshot animation");return false;}
    FILE *f=fopen(temp,"wb");if(!f){pd_playback_stop(d,&p);free(indices);snprintf(error,64,"Cannot create GIF temporary file");return false;}
    Writer w={.f=f,.limit=limit,.ok=true};bytes(&w,"GIF89a",6);word(&w,d->width);word(&w,d->height);byte(&w,0x70);byte(&w,0);byte(&w,0);
    static const unsigned char loop[]={0x21,0xff,11,'N','E','T','S','C','A','P','E','2','.','0',3,1,0,0,0};bytes(&w,loop,sizeof(loop));
    for(unsigned i=0;i<count&&w.ok;i++){if(count>1&&!pd_playback_seek(d,&p,i)){w.ok=false;break;}frame(&w,d,indices);}
    byte(&w,0x3b);pd_playback_stop(d,&p);pd_composite(d);free(indices);
    if(fflush(f)||GIF_SYNC(f))w.ok=false;if(fclose(f))w.ok=false;
    if(!w.ok){remove(temp);snprintf(error,64,"GIF write failed or exceeds 2 MiB limit");return false;}
    if(rename(temp,path)){remove(temp);snprintf(error,64,"Cannot finish GIF export");return false;}return true;
}
