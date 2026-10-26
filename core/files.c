#include "files.h"
#include "animation.h"
#include "ase_export.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "zlib.h"
#ifdef _WIN32
#include <io.h>
#include <direct.h>
#define SYNC(f) _commit(_fileno(f))
#else
#include <dirent.h>
#include <unistd.h>
#define SYNC(f) fsync(fileno(f))
#endif
#define MAX_FILE (9u*1024u*1024u)
#define NATIVE_BYTES (8u+4u*PD_NAME+4u*PD_PIXELS*4u)
#define NATIVE8_BYTES (16u+8u*PD_NAME+8u*PD_PIXELS*4u)
static bool fail(char *e,const char *s){snprintf(e,64,"%s",s);return false;}
static unsigned u16(const unsigned char *p){return p[0]|((unsigned)p[1]<<8);}
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void w16(unsigned char *p,unsigned v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static void w32(unsigned char *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static void be32(unsigned char *p,uint32_t v){for(unsigned i=0;i<4;i++)p[3-i]=(unsigned char)(v>>(8*i));}
bool pd_join(char out[PD_PATH],const char *dir,const char *name){
    if(!dir[0]||!name[0]||strchr(name,'/')||strchr(name,'\\')||strchr(name,':')||!strcmp(name,".")||!strcmp(name,".."))return false;
    int n=snprintf(out,PD_PATH,"%s/%s",dir,name);return n>0&&n<PD_PATH;
}
bool pd_exists(const char *p){struct stat st;return stat(p,&st)==0;}
bool pd_mkdir(const char *p){
#ifdef _WIN32
    return _mkdir(p)==0;
#else
    return mkdir(p,0777)==0;
#endif
}
static int compare(const void *a,const void *b){const PdEntry *x=a,*y=b;if(x->directory!=y->directory)return x->directory?-1:1;return strcmp(x->name,y->name);}
static bool supported(const char *n){const char *s=strrchr(n,'.');char ext[12];if(!s||strlen(s)>=sizeof(ext))return false;unsigned i;for(i=0;s[i];i++)ext[i]=(char)(s[i]>='A'&&s[i]<='Z'?s[i]+32:s[i]);ext[i]=0;return !strcmp(ext,".pdd")||!strcmp(ext,".ase")||!strcmp(ext,".aseprite")||!strcmp(ext,".gpl")||!strcmp(ext,".hex")||!strcmp(ext,".png")||!strcmp(ext,".bak")||!strcmp(ext,".tmp");}
static void add_entry(PdListing *o,const char *n,bool directory){
    if(n[0]=='.'||(!directory&&!supported(n)))return;
    if(strlen(n)>=PD_NAME||o->count==PD_ENTRIES){o->truncated=true;return;}
    PdEntry *e=&o->entries[o->count++];snprintf(e->name,sizeof(e->name),"%s",n);e->directory=directory;
}
bool pd_list(const char *path,PdListing *o,char error[64]){
    memset(o,0,sizeof(*o));
#ifdef _WIN32
    char pattern[PD_PATH];if(!pd_join(pattern,path,"*"))return fail(error,"Folder path too long");
    struct _finddata_t f;intptr_t h=_findfirst(pattern,&f);if(h==-1)return fail(error,"Cannot read folder");
    do{add_entry(o,f.name,(f.attrib&_A_SUBDIR)!=0);}while(_findnext(h,&f)==0);_findclose(h);
#else
    DIR *d=opendir(path);if(!d)return fail(error,"Cannot read folder");struct dirent *f;
    while((f=readdir(d))!=NULL){char full[PD_PATH];struct stat st;if(pd_join(full,path,f->d_name)&&stat(full,&st)==0)add_entry(o,f->d_name,S_ISDIR(st.st_mode));}closedir(d);
#endif
    qsort(o->entries,o->count,sizeof(PdEntry),compare);return true;
}
#define LARGE_META (8u*68u+8u+4u+1024u)
static void free_layers(PdLayer *layers){for(unsigned i=0;i<8;i++)free(layers[i].large);free(layers);}
static bool prepare_layer(PdLayer *layer,unsigned width,unsigned height){if(width<=128&&height<=128)return true;unsigned stride=(width+7)/8*8,rows=(height+7)/8*8;layer->large=calloc(stride*rows,4);return layer->large!=NULL;}
static bool large_read(const unsigned char *b,size_t n,PdLayer *layers,char names[8][PD_NAME],uint8_t *enabled,uint8_t order[8],uint32_t pal[256],unsigned *pal_count,unsigned *width,unsigned *height,unsigned profile,char *error){
    if(n<32+LARGE_META||u32(b+4)!=9||u32(b+16)!=8)return fail(error,"Invalid large project");
    *width=u32(b+8);*height=u32(b+12);unsigned cap=pd_capacity(*width,*height,profile),mask=u32(b+20),count=0;
    if(!cap||!mask||mask>=1u<<cap)return fail(error,"Project exceeds device size/layer limit");
    for(unsigned i=0;i<8;i++)if(mask&(1u<<i))count++;
    unsigned payload=LARGE_META+count*(*width)*(*height)*4;if(n!=32+payload||u32(b+24)!=payload)return fail(error,"Invalid large project length");
    if(crc32(crc32(0,b,28),b+32,payload)!=u32(b+28))return fail(error,"Project checksum failed");
    const unsigned char *p=b+32;unsigned seen=0;*enabled=(uint8_t)mask;
    for(unsigned i=0;i<8;i++){const unsigned char *m=p+i*68;if(m[0]>1||m[2]>3||m[3]>3||!memchr(m+4,0,64))return fail(error,"Invalid layer properties");layers[i].visible=m[0];layers[i].opacity=m[1];layers[i].blend=m[2];layers[i].flags=m[3];memcpy(names[i],m+4,64);unsigned slot=p[544+i];if(slot>=8||(seen&(1u<<slot)))return fail(error,"Invalid layer order");seen|=1u<<slot;order[i]=(uint8_t)slot;}
    *pal_count=u32(p+552);if(*pal_count>256)return fail(error,"Invalid palette size");for(unsigned i=0;i<*pal_count;i++)pal[i]=u32(p+556+i*4);
    unsigned stride=(*width>128||*height>128)?(*width+7)/8*8:128,at=LARGE_META;
    for(unsigned l=0;l<8;l++)if(mask&(1u<<l)){if(!prepare_layer(&layers[l],*width,*height))return fail(error,"Not enough memory to open project");for(unsigned y=0;y<*height;y++)for(unsigned x=0;x<*width;x++){pd_pixels(&layers[l])[y*stride+x]=u32(p+at);at+=4;}}return true;
}
static bool native_read(const unsigned char *b,size_t n,PdLayer *layers,char names[PD_LAYERS][PD_NAME],uint8_t *enabled,uint8_t order[PD_LAYERS],uint32_t pal[256],unsigned *pal_count,unsigned *width,unsigned *height,unsigned profile,char *error){
    (void)profile;bool v8=n>=32&&!memcmp(b,"PDD8",4);bool v7=v8||(n>=32&&!memcmp(b,"PDD7",4));unsigned slots=v7?8:4,base=v7?NATIVE8_BYTES:NATIVE_BYTES;bool v6=v7||(n>=32&&!memcmp(b,"PDD6",4));bool v5=v6||(n>=32&&!memcmp(b,"PDD5",4));bool v4=v5||(n>=32&&!memcmp(b,"PDD4",4));bool newest=n>=32&&!memcmp(b,"PDD3",4);bool old=n>=32&&!memcmp(b,"PDD1",4);unsigned payload=old?8+4*PD_PIXELS*4:base+(v4?slots*2:0)+(v5?1028:0)+(v8?8:0);
    if(n!=32+payload||memcmp(b,old?"PDD1":v8?"PDD8":v7?"PDD7":v6?"PDD6":v5?"PDD5":v4?"PDD4":newest?"PDD3":"PDD2",4)||u32(b+4)!=(old?1u:v8?8u:v7?7u:v6?6u:v5?5u:v4?4u:newest?3u:2u)||(v6?(!u32(b+8)||u32(b+8)>128||!u32(b+12)||u32(b+12)>128):(u32(b+8)!=128||u32(b+12)!=128))||u32(b+16)!=slots||u32(b+24)!=payload)return fail(error,"Invalid PDD project");
    *width=u32(b+8);*height=u32(b+12);
    uLong crc=crc32(0,b,28);crc=crc32(crc,b+32,payload);if(crc!=u32(b+28))return fail(error,"Project checksum failed");
    *enabled=(newest||v4)?(uint8_t)u32(b+20):15;if((newest||v4)&&(u32(b+20)<1||u32(b+20)>((1u<<slots)-1)))return fail(error,"Invalid layer set");
    if(v4){unsigned seen=0;for(unsigned i=0;i<slots;i++){unsigned blend=b[32+base+i],slot=b[32+base+slots+i];if(blend>3||slot>=slots||(seen&(1u<<slot)))return fail(error,"Invalid layer mode or order");seen|=1u<<slot;layers[i].blend=(uint8_t)blend;order[i]=(uint8_t)slot;}}
    if(v5){*pal_count=u32(b+32+base+slots*2);if(*pal_count>256)return fail(error,"Invalid palette size");for(unsigned i=0;i<*pal_count;i++)pal[i]=u32(b+32+base+slots*2+4+i*4);}
    if(v8)for(unsigned i=0;i<8;i++){unsigned flags=b[32+base+16+1028+i];if(flags>3)return fail(error,"Invalid layer protection");layers[i].flags=(uint8_t)flags;}
    const unsigned char *p=b+32;for(unsigned l=0;l<slots;l++){
        if(p[l*2]>1)return fail(error,"Invalid layer properties");layers[l].visible=p[l*2];layers[l].opacity=p[l*2+1];
        if(!old){const unsigned char *name=p+slots*2+l*PD_NAME;if(!memchr(name,0,PD_NAME))return fail(error,"Invalid layer name");memcpy(names[l],name,PD_NAME);}
        for(unsigned i=0;i<PD_PIXELS;i++)layers[l].pixels[i]=u32(p+slots*2+(old?0:slots*PD_NAME)+(l*PD_PIXELS+i)*4);
    }return true;
}
static bool ase_read(const unsigned char *b,size_t n,PdLayer *layers,char names[PD_LAYERS][PD_NAME],uint8_t *enabled,uint8_t order[PD_LAYERS],uint32_t pal[256],unsigned *pal_count,unsigned *width,unsigned *height,unsigned profile,char *error){
    if(n<144||u32(b)!=n||u16(b+4)!=0xa5e0)return fail(error,"Invalid Aseprite file");
    if(u16(b+6)!=1)return fail(error,"Animation not supported yet");
    if(!pd_capacity(u16(b+8),u16(b+10),profile)||u16(b+12)!=32)return fail(error,"RGBA sprite exceeds device size limit");
    *width=u16(b+8);*height=u16(b+10);
    if(b[34]&&b[35]&&b[34]!=b[35])return fail(error,"Non-square pixels unsupported");
    if(u32(b+128)!=n-128||u16(b+132)!=0xf1fa)return fail(error,"Invalid Aseprite frame");
    unsigned chunks=u32(b+140);if(!chunks)chunks=u16(b+134);if(chunks>4096)return fail(error,"Too many Aseprite chunks");
    size_t at=144;unsigned count=0;bool seen[PD_LAYERS]={0},modern=false;
    for(unsigned k=0;k<chunks;k++){
        if(n-at<6)return fail(error,"Truncated chunk");unsigned size=u32(b+at),type=u16(b+at+4);if(size<6||size>n-at)return fail(error,"Invalid chunk bounds");
        const unsigned char *p=b+at+6;unsigned len=size-6;
        if(type==0x2004){
            if(count==pd_capacity(*width,*height,profile))return fail(error,"Project exceeds device layer limit");if(!prepare_layer(&layers[count],*width,*height))return fail(error,"Not enough memory for layers");
            if(len<18||u16(p+16)>len-18)return fail(error,"Invalid layer chunk");
            if(u16(p+2)||u16(p+4)||u16(p+10)>3||(u16(p)&~3u))return fail(error,"Unsupported layer type or mode");
            unsigned name_len=u16(p+16);if(name_len>=PD_NAME||memchr(p+18,0,name_len))return fail(error,"Layer name exceeds 63 bytes");
            layers[count].flags=(u16(p)&2)?0:PD_LAYER_LOCK;layers[count].blend=(uint8_t)u16(p+10);order[count]=(uint8_t)count;layers[count].visible=(uint8_t)(u16(p)&1);layers[count].opacity=(u32(b+14)&1)?p[12]:255;
            memcpy(names[count],p+18,name_len);names[count][name_len]=0;count++;
        }else if(type==0x2005){
            if(len<20)return fail(error,"Invalid cel chunk");unsigned layer=u16(p),kind=u16(p+7),w=u16(p+16),h=u16(p+18);
            int x=(int16_t)u16(p+2),y=(int16_t)u16(p+4);
            if(layer>=count||seen[layer]||u16(p+9)||p[6]!=255||(kind!=0&&kind!=2))return fail(error,"Unsupported cel properties");
            if(!w||!h||w>512||h>512||x<0||y<0||x+(int)w>(int)*width||y+(int)h>(int)*height)return fail(error,"Cel outside canvas");
            unsigned bytes=w*h*4;unsigned char *raw=malloc(bytes);if(!raw)return fail(error,"Not enough memory");bool ok=true;
            if(kind==0){ok=len-20==bytes;if(ok)memcpy(raw,p+20,bytes);}else{
                z_stream z={0};z.next_in=(Bytef *)(p+20);z.avail_in=len-20;z.next_out=raw;z.avail_out=bytes;
                if(inflateInit(&z)!=Z_OK)ok=false;else{int ret=inflate(&z,Z_FINISH);ok=ret==Z_STREAM_END&&z.total_out==bytes&&z.total_in==len-20;inflateEnd(&z);}
            }
            if(!ok){free(raw);return fail(error,"Invalid compressed cel");}
            for(unsigned yy=0;yy<h;yy++)for(unsigned xx=0;xx<w;xx++){unsigned char *q=raw+(yy*w+xx)*4;pd_pixels(&layers[layer])[(y+(int)yy)*((*width>128||*height>128)?(*width+7)/8*8:128)+x+(int)xx]=((uint32_t)q[3]<<24)|((uint32_t)q[0]<<16)|((uint32_t)q[1]<<8)|q[2];}
            free(raw);seen[layer]=true;
        }else if(type==0x2019){
            if(len<20||u32(p)>256||!u32(p)||u32(p+4)>u32(p+8)||u32(p+8)>=u32(p))return fail(error,"Invalid palette chunk");
            *pal_count=u32(p);unsigned pos=20;for(unsigned i=u32(p+4);i<=u32(p+8);i++){if(len-pos<6||u16(p+pos)>1)return fail(error,"Invalid palette entry");unsigned flags=u16(p+pos);pal[i]=((uint32_t)p[pos+5]<<24)|((uint32_t)p[pos+2]<<16)|((uint32_t)p[pos+3]<<8)|p[pos+4];pos+=6;if(flags){if(len-pos<2||u16(p+pos)>len-pos-2)return fail(error,"Invalid palette name");pos+=2+u16(p+pos);}}if(pos!=len)return fail(error,"Invalid palette length");modern=true;
        }else if((type==0x0004||type==0x0011)&&!modern){
            if(len<2)return fail(error,"Invalid old palette");unsigned pos=2,index=0;for(unsigned packet=0;packet<u16(p);packet++){if(len-pos<2)return fail(error,"Invalid palette packet");index+=p[pos++];unsigned num=p[pos++];if(!num)num=256;if(index+num>256||num*3>len-pos)return fail(error,"Palette exceeds 256 colors");for(unsigned j=0;j<num;j++){unsigned r=p[pos++],g=p[pos++],b=p[pos++];if(type==0x0011){if(r>63||g>63||b>63)return fail(error,"Invalid six-bit color");r=r*255/63;g=g*255/63;b=b*255/63;}pal[index++]=0xff000000u|(r<<16)|(g<<8)|b;}if(index>*pal_count)*pal_count=index;}if(pos!=len)return fail(error,"Invalid palette length");
        }else if(type==0x2007){if(len<16||u16(p)>1||(u16(p+2)&1))return fail(error,"Custom color profile unsupported");}
        else if(type!=0x2019&&type!=0x0004&&type!=0x0011){return fail(error,"Unsupported Aseprite metadata");}
        at+=size;
    }
    if(at!=n||!count)return fail(error,"Invalid Aseprite layout");*enabled=(uint8_t)((1u<<count)-1);return true;
}
/* Bounded non-interlaced PNG reader; sample values remain unmodified. */
static uint32_t read_be(const unsigned char *p);
static int paeth(int a,int b,int c);
static bool artwork_png(const unsigned char *b,size_t n,PdLayer *layers,char names[8][PD_NAME],uint8_t *enabled,uint32_t pal[256],unsigned *pal_count,unsigned *width,unsigned *height,unsigned profile,char *error){
    unsigned w=0,h=0,depth=0,type=0,channels=0,entries=0,zn=0;uint32_t colors[256];for(unsigned i=0;i<256;i++)colors[i]=0xff000000;
    bool header=false,plte=false,trans=false,data=false,closed=false,end=false;unsigned key[3]={0};
    for(size_t at=8;at<n;){
        if(n-at<12)return fail(error,"Truncated PNG chunk");unsigned len=read_be(b+at);if(len>n-at-12)return fail(error,"Invalid PNG chunk bounds");const unsigned char *t=b+at+4,*p=t+4;
        for(unsigned i=0;i<4;i++)if(!((t[i]>='A'&&t[i]<='Z')||(t[i]>='a'&&t[i]<='z')))return fail(error,"Invalid PNG chunk type");
        if(t[2]&32)return fail(error,"Invalid PNG reserved bit");if((uint32_t)crc32(0,t,len+4)!=read_be(p+len))return fail(error,"PNG checksum failed");
        if(!memcmp(t,"IHDR",4)){
            if(header||at!=8||len!=13)return fail(error,"Invalid PNG header");w=read_be(p);h=read_be(p+4);depth=p[8];type=p[9];
            if(!pd_capacity(w,h,profile))return fail(error,"PNG exceeds device canvas limit");
            if(p[10]||p[11]||p[12])return fail(error,"Use non-interlaced PNG");
            if((type!=0&&type!=2&&type!=3&&type!=4&&type!=6)||((type==0||type==3)?(depth!=1&&depth!=2&&depth!=4&&depth!=8):depth!=8))return fail(error,"Unsupported PNG color depth");
            channels=type==2?3:type==6?4:type==4?2:1;header=true;
        }else if(!header)return fail(error,"PNG header must come first");
        else if(!memcmp(t,"PLTE",4)){
            if(plte||trans||data||type==0||type==4||!len||len%3||len>768)return fail(error,"Invalid PNG palette");entries=len/3;if(type==3&&entries>(1u<<depth))return fail(error,"PNG palette exceeds bit depth");for(unsigned i=0;i<entries;i++)colors[i]=0xff000000u|((uint32_t)p[i*3]<<16)|((uint32_t)p[i*3+1]<<8)|p[i*3+2];plte=true;
        }else if(!memcmp(t,"tRNS",4)){
            if(trans||data)return fail(error,"Invalid PNG transparency order");trans=true;
            if(type==3){if(!plte||!len||len>entries)return fail(error,"Invalid palette transparency");for(unsigned i=0;i<len;i++)colors[i]=(colors[i]&0xffffff)|((uint32_t)p[i]<<24);}
            else if(type==0||type==2){unsigned count=type==0?1:3;if(len!=count*2)return fail(error,"Invalid PNG transparency key");for(unsigned i=0;i<count;i++){key[i]=(unsigned)p[i*2]*256+p[i*2+1];if(key[i]>((1u<<depth)-1))return fail(error,"Invalid PNG transparency sample");}}
            else return fail(error,"Unexpected PNG transparency");
        }else if(!memcmp(t,"IDAT",4)){
            if(closed||(type==3&&!plte)||len>MAX_FILE-zn)return fail(error,"Invalid PNG image data order");data=true;zn+=len;
        }else if(!memcmp(t,"IEND",4)){if(len||!data||at+12!=n)return fail(error,"Invalid PNG ending");end=true;}
        else {if(data)closed=true;if(!memcmp(t,"acTL",4)||!memcmp(t,"fcTL",4)||!memcmp(t,"fdAT",4))return fail(error,"Animated PNG not supported yet");if(!memcmp(t,"iCCP",4))return fail(error,"Convert PNG to sRGB without ICC profile");if(!(t[0]&32))return fail(error,"Unsupported critical PNG chunk");}
        at+=len+12;
    }
    if(!header||!end||!zn)return fail(error,"Incomplete PNG");unsigned rowbytes=(w*channels*depth+7)/8,rawbytes=(rowbytes+1)*h,bpp=(channels*depth+7)/8;
    unsigned char *raw=malloc(rawbytes),*z=malloc(zn);if(!raw||!z){free(raw);free(z);return fail(error,"Not enough memory for PNG");}unsigned offset=0;
    for(size_t at=8;at<n;){unsigned len=read_be(b+at);if(!memcmp(b+at+4,"IDAT",4)){memcpy(z+offset,b+at+8,len);offset+=len;}at+=len+12;}
    z_stream stream={0};stream.next_in=z;stream.avail_in=zn;stream.next_out=raw;stream.avail_out=rawbytes;bool ok=false;
    if(inflateInit(&stream)==Z_OK){int ret=inflate(&stream,Z_FINISH);ok=ret==Z_STREAM_END&&stream.total_in==zn&&stream.total_out==rawbytes;inflateEnd(&stream);}free(z);if(!ok){free(raw);return fail(error,"Invalid PNG compressed pixels");}
    if(!prepare_layer(&layers[0],w,h)){free(raw);return fail(error,"Not enough memory for PNG layer");}unsigned stride=(w>128||h>128)?(w+7)/8*8:128;
    for(unsigned y=0;y<h;y++){
        unsigned char *row=raw+y*(rowbytes+1)+1,*prev=y?row-rowbytes-1:NULL;unsigned filter=row[-1];if(filter>4){free(raw);return fail(error,"Invalid PNG filter");}
        for(unsigned x=0;x<rowbytes;x++){int left=x>=bpp?row[x-bpp]:0,up=prev?prev[x]:0,corner=prev&&x>=bpp?prev[x-bpp]:0;row[x]=(unsigned char)(row[x]+(filter==1?left:filter==2?up:filter==3?(left+up)/2:filter==4?paeth(left,up,corner):0));}
        for(unsigned x=0;x<w;x++){
            unsigned r=0,g=0,blue=0,alpha=255;uint32_t c;
            if(type==0||type==3){unsigned v=(row[x*depth/8]>>(8-depth-(x*depth%8)))&((1u<<depth)-1);if(type==3){if(v>=entries){free(raw);return fail(error,"PNG palette index out of range");}c=colors[v];}else {r=v*255/((1u<<depth)-1);alpha=trans&&v==key[0]?0:255;c=((uint32_t)alpha<<24)|(r<<16)|(r<<8)|r;}}
            else {const unsigned char *p=row+x*channels;if(type==4){r=g=blue=p[0];alpha=p[1];}else{r=p[0];g=p[1];blue=p[2];if(type==6)alpha=p[3];else if(trans&&r==key[0]&&g==key[1]&&blue==key[2])alpha=0;}c=((uint32_t)alpha<<24)|(r<<16)|(g<<8)|blue;}
            pd_pixels(&layers[0])[y*stride+x]=c;
        }
    }
    free(raw);*width=w;*height=h;*enabled=1;snprintf(names[0],PD_NAME,"PNG Image");if(type==3){*pal_count=entries;memcpy(pal,colors,entries*4);}return true;
}
static bool read_layers(const char *path,PdLayer *layers,char names[PD_LAYERS][PD_NAME],uint8_t *enabled,uint8_t order[PD_LAYERS],uint32_t pal[256],unsigned *pal_count,unsigned *width,unsigned *height,unsigned profile,char *error){
    FILE *f=fopen(path,"rb");if(!f)return fail(error,"Cannot open file");
    if(fseek(f,0,SEEK_END)){fclose(f);return fail(error,"Cannot seek file");}long length=ftell(f);
    if(length<32||length>(long)MAX_FILE){fclose(f);return fail(error,"Invalid file size (max 9 MiB)");}rewind(f);
    unsigned char head[32];if(fread(head,1,32,f)!=32){fclose(f);return fail(error,"Cannot read header");}bool png=!memcmp(head,"\x89PNG\r\n\x1a\n",8);unsigned fw=png?read_be(head+16):!memcmp(head,"PDD",3)?u32(head+8):u16(head+8),fh=png?read_be(head+20):!memcmp(head,"PDD",3)?u32(head+12):u16(head+10);if(!pd_capacity(fw,fh,profile)){fclose(f);return fail(error,"Project exceeds device canvas limit");}rewind(f);
    unsigned char *b=malloc((size_t)length);if(!b){fclose(f);return fail(error,"Not enough memory");}
    bool ok=fread(b,1,(size_t)length,f)==(size_t)length;fclose(f);
    if(ok){if(png)ok=artwork_png(b,(size_t)length,layers,names,enabled,pal,pal_count,width,height,profile,error);else if(!memcmp(b,"PDD9",4))ok=large_read(b,(size_t)length,layers,names,enabled,order,pal,pal_count,width,height,profile,error);else if(!memcmp(b,"PDD",3))ok=native_read(b,(size_t)length,layers,names,enabled,order,pal,pal_count,width,height,profile,error);else ok=ase_read(b,(size_t)length,layers,names,enabled,order,pal,pal_count,width,height,profile,error);}else fail(error,"Cannot read file");free(b);return ok;
}
bool pd_preview_file_info(const char *path,uint32_t preview[64*64],unsigned profile,PdPreviewInfo *info,char error[64]){
    if(info)memset(info,0,sizeof(*info));
    FILE *probe=fopen(path,"rb");if(probe){unsigned char magic[4];bool animation=fread(magic,1,4,probe)==4&&!memcmp(magic,"PDDA",4);rewind(probe);
        if(animation){PdDocument *scratch=calloc(1,sizeof(*scratch));if(!scratch){fclose(probe);return fail(error,"Not enough memory for preview");}pd_init(scratch);scratch->profile=profile;bool ok=pd_frames_read(scratch,probe);fclose(probe);
            if(ok){unsigned max=scratch->width>scratch->height?scratch->width:scratch->height,pw=scratch->width*64/max,ph=scratch->height*64/max;if(!pw)pw=1;if(!ph)ph=1;memset(preview,0,64*64*4);for(unsigned y=0;y<ph;y++)for(unsigned x=0;x<pw;x++)preview[(y+(64-ph)/2)*64+x+(64-pw)/2]=pd_image(scratch)[(y*scratch->height/ph)*scratch->stride+x*scratch->width/pw];}
            if(ok&&info){info->width=scratch->width;info->height=scratch->height;info->frames=pd_frame_count(scratch);for(unsigned l=0;l<PD_LAYERS;l++)if(scratch->enabled&(1u<<l))info->layers++;}pd_dispose(scratch);free(scratch);return ok?true:fail(error,"Animation preview unavailable");}fclose(probe);}

    PdLayer *layers=calloc(PD_LAYERS,sizeof(PdLayer));if(!layers)return fail(error,"Not enough memory for preview");char names[PD_LAYERS][PD_NAME]={{0}};uint32_t pal[256]={0};unsigned count=0,width=128,height=128;uint8_t enabled=1,order[PD_LAYERS]={0,1,2,3,4,5,6,7};for(unsigned i=0;i<PD_LAYERS;i++){layers[i].visible=1;layers[i].opacity=255;}
    bool ok=read_layers(path,layers,names,&enabled,order,pal,&count,&width,&height,profile,error);if(ok&&info){info->width=width;info->height=height;info->frames=1;for(unsigned l=0;l<PD_LAYERS;l++)if(enabled&(1u<<l))info->layers++;}if(ok){unsigned max=width>height?width:height,pw=width*64/max,ph=height*64/max;if(!pw)pw=1;if(!ph)ph=1;memset(preview,0,64*64*4);for(unsigned y=0;y<ph;y++)for(unsigned x=0;x<pw;x++)preview[(y+(64-ph)/2)*64+x+(64-pw)/2]=pd_sample_layers(layers,enabled,order,(y*height/ph)*((width>128||height>128)?(width+7)/8*8:128)+x*width/pw);}free_layers(layers);return ok;
}
bool pd_preview_file_profile(const char *path,uint32_t preview[64*64],unsigned profile,char error[64]){return pd_preview_file_info(path,preview,profile,NULL,error);}
bool pd_preview_file(const char *path,uint32_t preview[64*64],char error[64]){return pd_preview_file_profile(path,preview,1,error);}
bool pd_open_file(PdDocument *d,const char *path,char error[64]){
    if(d->editing)return fail(error,"Finish stroke first");
    FILE *probe=fopen(path,"rb");if(probe){unsigned char magic[4];bool animation=fread(magic,1,4,probe)==4&&!memcmp(magic,"PDDA",4);rewind(probe);if(animation){bool ok=pd_frames_read(d,probe);fclose(probe);return ok?true:fail(error,"Invalid animation or not enough memory");}fclose(probe);}
    PdLayer *layers=calloc(PD_LAYERS,sizeof(PdLayer));if(!layers)return fail(error,"Not enough memory");
    char names[PD_LAYERS][PD_NAME]={{0}};for(unsigned l=0;l<PD_LAYERS;l++){layers[l].visible=1;layers[l].opacity=255;snprintf(names[l],PD_NAME,"Layer %u",l+1);}
    uint32_t pal[256]={0};unsigned pal_count=0,width=128,height=128;uint8_t enabled=1,order[PD_LAYERS]={0,1,2,3,4,5,6,7};bool ok=read_layers(path,layers,names,&enabled,order,pal,&pal_count,&width,&height,d->profile,error);if(ok&&!pd_create(d,width,height,d->profile)){fail(error,"Not enough memory - current art retained");ok=false;}if(ok){d->width=width;d->height=height;d->palette_count=d->saved_palette_count=pal_count;memcpy(d->palette,pal,sizeof(pal));memcpy(d->saved_palette,pal,sizeof(pal));d->enabled=enabled;memcpy(d->order,order,PD_LAYERS);while(!(enabled&(1u<<d->active)))d->active++;for(unsigned l=0;l<8;l++){d->layer[l].visible=layers[l].visible;d->layer[l].opacity=layers[l].opacity;d->layer[l].blend=layers[l].blend;d->layer[l].flags=layers[l].flags;if(enabled&(1u<<l))memcpy(pd_pixels(&d->layer[l]),pd_pixels(&layers[l]),pd_bytes(d));}memcpy(d->names,names,sizeof(d->names));pd_composite(d);}free_layers(layers);return ok;
}
static unsigned char *large_encode(PdDocument *d,size_t *n){
    unsigned count=0;for(unsigned i=0;i<8;i++)if(d->enabled&(1u<<i))count++;unsigned payload=LARGE_META+count*d->width*d->height*4;*n=32+payload;
    unsigned char *b=calloc(1,*n);if(!b)return NULL;memcpy(b,"PDD9",4);w32(b+4,9);w32(b+8,d->width);w32(b+12,d->height);w32(b+16,8);w32(b+20,d->enabled);w32(b+24,payload);unsigned char *p=b+32;
    for(unsigned l=0;l<8;l++){unsigned char *m=p+l*68;m[0]=d->layer[l].visible;m[1]=d->layer[l].opacity;m[2]=d->layer[l].blend;m[3]=d->layer[l].flags;memcpy(m+4,d->names[l],64);p[544+l]=d->order[l];}w32(p+552,d->palette_count);for(unsigned i=0;i<d->palette_count;i++)w32(p+556+i*4,d->palette[i]);
    unsigned at=LARGE_META;for(unsigned l=0;l<8;l++)if(d->enabled&(1u<<l))for(unsigned y=0;y<d->height;y++)for(unsigned x=0;x<d->width;x++){w32(p+at,pd_pixels(&d->layer[l])[y*d->stride+x]);at+=4;}w32(b+28,(uint32_t)crc32(crc32(0,b,28),p,payload));return b;
}
static unsigned char *native_encode(PdDocument *d,size_t *n){
    if(d->width>128||d->height>128)return large_encode(d,n);
    *n=32+NATIVE8_BYTES+16+1028+8;unsigned char *b=calloc(1,*n);if(!b)return NULL;memcpy(b,"PDD8",4);w32(b+4,8);w32(b+20,d->enabled);w32(b+8,d->width);w32(b+12,d->height);w32(b+16,8);w32(b+24,NATIVE8_BYTES+16+1028+8);
    unsigned char *p=b+32;for(unsigned l=0;l<PD_LAYERS;l++){p[l*2]=d->layer[l].visible;p[l*2+1]=d->layer[l].opacity;memcpy(p+16+l*PD_NAME,d->names[l],PD_NAME);for(unsigned i=0;i<PD_PIXELS;i++)w32(p+16+8*PD_NAME+(l*PD_PIXELS+i)*4,(d->enabled&(1u<<l))&&i%128<d->width&&i/128<d->height?d->layer[l].pixels[i]:0);}
    for(unsigned i=0;i<PD_LAYERS;i++){p[NATIVE8_BYTES+i]=d->layer[i].blend;p[NATIVE8_BYTES+8+i]=d->order[i];}w32(p+NATIVE8_BYTES+16,d->palette_count);for(unsigned i=0;i<d->palette_count;i++)w32(p+NATIVE8_BYTES+20+i*4,d->palette[i]);for(unsigned i=0;i<8;i++)p[NATIVE8_BYTES+16+1028+i]=d->layer[i].flags;uLong c=crc32(0,b,28);w32(b+28,(uint32_t)crc32(c,p,NATIVE8_BYTES+16+1028+8));return b;
}
static unsigned char *ase_encode(PdDocument *d,size_t *n){
    unsigned count=0;*n=144+(d->palette_count?26+d->palette_count*6:0);for(unsigned l=0;l<PD_LAYERS;l++)if(d->enabled&(1u<<l)){count++;*n+=24+strlen(d->names[l])+26+(d->width*d->height)*4;}
    unsigned char *b=calloc(1,*n);if(!b)return NULL;w32(b,(uint32_t)*n);w16(b+4,0xa5e0);w16(b+6,1);w16(b+8,d->width);w16(b+10,d->height);w16(b+12,32);w32(b+14,1);w16(b+18,100);b[34]=b[35]=1;
    w32(b+128,(uint32_t)*n-128);w16(b+132,0xf1fa);w16(b+134,count*2+(d->palette_count?1:0));w16(b+32,d->palette_count);w16(b+136,100);size_t at=144;
    for(unsigned k=0;k<PD_LAYERS;k++){unsigned l=d->order[k];if(!(d->enabled&(1u<<l)))continue;unsigned len=(unsigned)strlen(d->names[l]);unsigned char *p=b+at;w32(p,24+len);w16(p+4,0x2004);w16(p+6,((d->layer[l].flags&PD_LAYER_LOCK)?0:2)|d->layer[l].visible);p[18]=d->layer[l].opacity;w16(p+16,d->layer[l].blend);w16(p+22,len);memcpy(p+24,d->names[l],len);at+=24+len;}
    unsigned compact=0;for(unsigned k=0;k<PD_LAYERS;k++){unsigned l=d->order[k];if(!(d->enabled&(1u<<l)))continue;unsigned char *p=b+at;w32(p,26+(d->width*d->height)*4);w16(p+4,0x2005);w16(p+6,compact++);p[12]=255;w16(p+22,d->width);w16(p+24,d->height);for(unsigned i=0;i<(d->width*d->height);i++){uint32_t c=pd_pixels(&d->layer[l])[(i/d->width)*d->stride+i%d->width];p[26+i*4]=(unsigned char)(c>>16);p[27+i*4]=(unsigned char)(c>>8);p[28+i*4]=(unsigned char)c;p[29+i*4]=(unsigned char)(c>>24);}at+=26+(d->width*d->height)*4;}
    if(d->palette_count){unsigned char *p=b+at;w32(p,26+d->palette_count*6);w16(p+4,0x2019);w32(p+6,d->palette_count);w32(p+14,d->palette_count-1);for(unsigned i=0;i<d->palette_count;i++){uint32_t c=d->palette[i];p[28+i*6]=(unsigned char)(c>>16);p[29+i*6]=(unsigned char)(c>>8);p[30+i*6]=(unsigned char)c;p[31+i*6]=(unsigned char)(c>>24);}}return b;
}
static void png_chunk(unsigned char *b,const char *type,const unsigned char *data,unsigned n){be32(b,n);memcpy(b+4,type,4);if(n)memcpy(b+8,data,n);be32(b+8+n,(uint32_t)crc32(0,b+4,n+4));}
static unsigned char *png_encode(PdDocument *d,size_t *n,bool visible){
    unsigned stride=1+d->width*4,raw_n=d->height*stride;unsigned char *raw=malloc(raw_n);if(!raw)return NULL;if(visible)pd_composite(d);const uint32_t *pixels=visible?pd_image(d):pd_pixels(&d->layer[d->active]);unsigned opacity=visible?255:d->layer[d->active].opacity;
    for(unsigned y=0;y<d->height;y++){raw[y*stride]=0;for(unsigned x=0;x<d->width;x++){uint32_t c=pixels[y*d->stride+x];if(!visible)c=(c&0x00ffffffu)|((((c>>24)*opacity+127)/255)<<24);unsigned char *p=raw+y*stride+1+x*4;p[0]=(unsigned char)(c>>16);p[1]=(unsigned char)(c>>8);p[2]=(unsigned char)c;p[3]=(unsigned char)(c>>24);}}
    /* Two uncompressed DEFLATE blocks: no compression workspace on DS. */
    unsigned zlen=2+raw_n+5*((raw_n+65534)/65535)+4;unsigned char *z=malloc(zlen);if(!z){free(raw);return NULL;}z[0]=0x78;z[1]=1;unsigned at=2,off=0;
    while(off<raw_n){unsigned len=raw_n-off;if(len>65535)len=65535;z[at++]=(off+len==raw_n)?1:0;w16(z+at,len);w16(z+at+2,len^65535);at+=4;memcpy(z+at,raw+off,len);at+=len;off+=len;}be32(z+at,(uint32_t)adler32(1,raw,raw_n));free(raw);
    *n=8+25+12+zlen+12;unsigned char *b=calloc(1,*n);if(!b){free(z);return NULL;}memcpy(b,"\x89PNG\r\n\x1a\n",8);unsigned char ihdr[13]={0};be32(ihdr,d->width);be32(ihdr+4,d->height);ihdr[8]=8;ihdr[9]=6;png_chunk(b+8,"IHDR",ihdr,13);png_chunk(b+33,"IDAT",z,zlen);png_chunk(b+45+zlen,"IEND",NULL,0);free(z);return b;
}
static bool write_bytes_mode(const char *path,unsigned char *b,size_t n,bool overwrite,char error[64],PdDocument *animation,bool ase){
    if(ase)error[0]=0;
    struct stat st;
    if(stat(path,&st)==0&&(st.st_mode&S_IFMT)!=S_IFREG)return fail(error,"Destination is not a regular file");
    if(pd_exists(path)&&!overwrite)return fail(error,"File already exists");
    char temp[PD_PATH],backup[PD_PATH];if(snprintf(temp,sizeof(temp),"%s.tmp",path)>=(int)sizeof(temp)||snprintf(backup,sizeof(backup),"%s.bak",path)>=(int)sizeof(backup))return fail(error,"Path too long");
    if(pd_exists(temp))return fail(error,"Recovery .tmp exists; use another name");
    FILE *f=fopen(temp,"wb");if(!f){return fail(error,"Cannot create temporary file");}
    uint32_t stream_crc=0;size_t stream_size=0;bool ok=ase?pd_ase_stream(animation,f,&stream_crc,&stream_size,error):animation?pd_frames_write(animation,f):fwrite(b,1,n,f)==n;if(fflush(f)||SYNC(f))ok=false;if(fclose(f))ok=false;
    /* Verify exact bytes before replacing the previous file. */
    f=fopen(temp,"rb");if(!f)ok=false;else if(ase){unsigned char buf[1024];uint32_t crc=0;size_t left=stream_size;while(ok&&left){size_t len=left>sizeof(buf)?sizeof(buf):left;if(fread(buf,1,len,f)!=len)ok=false;else crc=(uint32_t)crc32(crc,buf,(uInt)len);left-=len;}ok=ok&&crc==stream_crc&&fgetc(f)==EOF&&!ferror(f);fclose(f);}else if(animation){ok=ok&&pd_frames_verify(f);fclose(f);}else{unsigned char buf[1024];for(size_t off=0;ok&&off<n;){size_t len=n-off;if(len>sizeof(buf))len=sizeof(buf);if(fread(buf,1,len,f)!=len||memcmp(buf,b+off,len))ok=false;off+=len;}if(ok&&(fgetc(f)!=EOF||ferror(f)))ok=false;fclose(f);}
    if(!ok){if(ase&&error[0])return false;return fail(error,"Write failed; original retained");}
    bool existed=pd_exists(path);if(existed){if(stat(backup,&st)==0&&(st.st_mode&S_IFMT)!=S_IFREG)return fail(error,"Backup path is not a regular file");if(pd_exists(backup)&&remove(backup))return fail(error,"Cannot replace backup");if(rename(path,backup))return fail(error,"Cannot back up original");}
    if(rename(temp,path)){if(existed)rename(backup,path);return fail(error,"Cannot finish save; check .bak/.tmp");}
    return true;
}

static bool write_bytes(const char *path,unsigned char *b,size_t n,bool overwrite,char error[64],PdDocument *animation){return write_bytes_mode(path,b,n,overwrite,error,animation,false);}

bool pd_write_file(PdDocument *d,const char *path,unsigned kind,bool overwrite,char error[64]){
    if(d->editing||!pd_capacity(d->width,d->height,d->profile)||kind>3||d->palette_count>256)return fail(error,"Invalid save operation");
    if(kind==1&&pd_frame_count(d)>1)return write_bytes_mode(path,NULL,0,overwrite,error,d,true);
    if(kind==0&&d->animation){bool ok=write_bytes(path,NULL,0,overwrite,error,d);if(ok){d->saved_revision=d->revision;d->saved_palette_count=d->palette_count;memcpy(d->saved_palette,d->palette,sizeof(d->palette));pd_frames_saved(d);}return ok;}size_t n=0;unsigned char *b=kind==0?native_encode(d,&n):kind==1?ase_encode(d,&n):png_encode(d,&n,kind==2);if(!b)return fail(error,"Not enough memory");bool ok=write_bytes(path,b,n,overwrite,error,NULL);free(b);if(ok&&kind==0){d->saved_revision=d->revision;d->saved_palette_count=d->palette_count;memcpy(d->saved_palette,d->palette,sizeof(d->palette));}return ok;
}

bool pd_write_recovery(PdDocument *d,const char *path,char error[64]){
    if(d->editing||d->palette_count>256)return fail(error,"Finish editing before recovery");
    if(d->animation)return write_bytes(path,NULL,0,true,error,d);
    size_t n=0;unsigned char *b=native_encode(d,&n);if(!b)return fail(error,"Not enough memory");
    bool ok=write_bytes(path,b,n,true,error,NULL);free(b);return ok;
}

/* Palette files: bounded, transactional imports; explicit verified replacements. */
static uint32_t read_be(const unsigned char *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static int hex_digit(int c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
bool pd_color_parse(const char *s,uint32_t *color){
    if(*s=='#')s++;size_t n=strlen(s);if(n!=6&&n!=8)return false;uint32_t c=0;for(size_t i=0;i<n;i++){int v=hex_digit(s[i]);if(v<0)return false;c=(c<<4)|(unsigned)v;}*color=n==6?0xff000000u|c:(c>>8)|(c<<24);return true;
}
static int paeth(int a,int b,int c){int p=a+b-c,da=abs(p-a),db=abs(p-b),dc=abs(p-c);return da<=db&&da<=dc?a:db<=dc?b:c;}
static bool palette_png(const unsigned char *b,size_t n,uint32_t colors[256],unsigned *count,char *error){
    unsigned w=0,h=0,bpp=0,zn=0;unsigned char z[2048],raw[1280];bool header=false,end=false;
    for(size_t at=8;at<n;){if(n-at<12)return fail(error,"Truncated PNG");unsigned len=read_be(b+at);if(len>n-at-12)return fail(error,"Invalid PNG chunk");const unsigned char *p=b+at+8;const unsigned char *type=b+at+4;if((uint32_t)crc32(0,type,len+4)!=read_be(p+len))return fail(error,"PNG checksum failed");
        if(!memcmp(type,"IHDR",4)){if(header||at!=8||len!=13)return fail(error,"Invalid PNG header");w=read_be(p);h=read_be(p+4);if(!w||!h||w>256||h>256||(w!=1&&h!=1)||p[8]!=8||(p[9]!=2&&p[9]!=6)||p[10]||p[11]||p[12])return fail(error,"Use 1xN or Nx1 RGB/RGBA PNG (max 256)");bpp=p[9]==6?4:3;header=true;}
        else if(!memcmp(type,"IDAT",4)){if(!header||len>sizeof(z)-zn)return fail(error,"PNG compressed data too large");memcpy(z+zn,p,len);zn+=len;}
        else if(!memcmp(type,"IEND",4)){if(len||at+12!=n)return fail(error,"Invalid PNG ending");end=true;}
        else if(!memcmp(type,"tRNS",4))return fail(error,"Convert transparent RGB strip to RGBA");
        else if(!(type[0]&32))return fail(error,"Unsupported PNG chunk");
        at+=len+12;
    }
    if(!header||!end||!zn)return fail(error,"Incomplete PNG");unsigned stride=w*bpp,rawlen=(stride+1)*h;z_stream stream={0};stream.next_in=z;stream.avail_in=zn;stream.next_out=raw;stream.avail_out=rawlen;
    if(inflateInit(&stream)!=Z_OK)return fail(error,"PNG inflate failed");int ret=inflate(&stream,Z_FINISH);bool ok=ret==Z_STREAM_END&&stream.total_in==zn&&stream.total_out==rawlen;inflateEnd(&stream);if(!ok)return fail(error,"Invalid PNG pixels");
    for(unsigned y=0;y<h;y++){unsigned char *row=raw+y*(stride+1)+1,*prev=y?row-(stride+1):NULL;unsigned filter=row[-1];if(filter>4)return fail(error,"Invalid PNG filter");for(unsigned x=0;x<stride;x++){int left=x>=bpp?row[x-bpp]:0,up=prev?prev[x]:0,corner=prev&&x>=bpp?prev[x-bpp]:0;row[x]=(unsigned char)(row[x]+(filter==1?left:filter==2?up:filter==3?(left+up)/2:filter==4?paeth(left,up,corner):0));}
        for(unsigned x=0;x<w;x++){unsigned char *p=row+x*bpp;colors[y*w+x]=((uint32_t)(bpp==4?p[3]:255)<<24)|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];}}
    *count=w*h;return true;
}
bool pd_palette_read(const char *path,uint32_t colors[256],unsigned *count,char error[64]){
    FILE *f=fopen(path,"rb");if(!f)return fail(error,"Cannot open palette");unsigned char *b=malloc(32769);if(!b){fclose(f);return fail(error,"Not enough memory");}size_t n=fread(b,1,32769,f);bool ok=!ferror(f)&&n<=32768;fclose(f);uint32_t tmp[256];unsigned used=0;
    if(!ok){free(b);return fail(error,"Palette exceeds 32 KiB");}b[n]=0;
    if(n>=8&&!memcmp(b,"\x89PNG\r\n\x1a\n",8))ok=palette_png(b,n,tmp,&used,error);
    else {if(memchr(b,0,n)){free(b);return fail(error,"Invalid text palette");}bool gpl=n>=12&&!memcmp(b,"GIMP Palette",12);char *line=(char*)b;while(*line&&ok){char *next=strchr(line,'\n');if(next)*next++=0;size_t len=strlen(line);while(len&&(line[len-1]=='\r'||line[len-1]==' '||line[len-1]=='\t'))line[--len]=0;while(*line==' '||*line=='\t')line++;
        if(*line&&!(gpl&&(*line=='#'||!strncmp(line,"GIMP Palette",12)||!strncmp(line,"Name:",5)||!strncmp(line,"Columns:",8)))){uint32_t c=0;if(gpl){unsigned rgb[3]={0};char *p=line;for(unsigned j=0;j<3;j++){while(*p==' '||*p=='\t')p++;char *end;errno=0;unsigned long v=strtoul(p,&end,10);if(p==end||errno||v>255||*p=='-'){ok=false;break;}rgb[j]=(unsigned)v;p=end;if(j<2&&*p!=' '&&*p!='\t'){ok=false;break;}}if(ok)c=0xff000000u|(rgb[0]<<16)|(rgb[1]<<8)|rgb[2];}else ok=pd_color_parse(line,&c);if(ok){if(used==256)ok=false;else tmp[used++]=c;}}
        if(!next)break;line=next;
    }if(!ok)fail(error,"Invalid palette (HEX RGB/RGBA or GPL RGB)");}
    free(b);if(!ok)return false;if(!used)return fail(error,"Empty palette");memcpy(colors,tmp,used*sizeof(uint32_t));*count=used;return true;
}
bool pd_palette_write(const char *path,const uint32_t colors[256],unsigned count,unsigned kind,bool overwrite,char error[64]){
    if(!count||count>256||kind>2)return fail(error,"Invalid palette");unsigned char *b=calloc(1,16384);if(!b)return fail(error,"Not enough memory");size_t n=0;
    if(kind<2){if(!kind){for(unsigned i=0;i<count;i++)if((colors[i]>>24)!=255){free(b);return fail(error,"GPL cannot preserve alpha; use HEX or PNG");}n=(size_t)sprintf((char*)b,"GIMP Palette\nName: Pixel Doodle\nColumns: 8\n#\n");}
        for(unsigned i=0;i<count;i++){unsigned c=(unsigned)colors[i];if(!kind)n+=(size_t)sprintf((char*)b+n,"%u %u %u\n",(c>>16)&255,(c>>8)&255,c&255);else n+=(size_t)sprintf((char*)b+n,"%06X%02X\n",c&0xffffff,c>>24);}}
    else {unsigned char raw[1025]={0},z[1036],ihdr[13]={0};unsigned rn=1+count*4;for(unsigned i=0;i<count;i++){uint32_t c=colors[i];raw[1+i*4]=(unsigned char)(c>>16);raw[2+i*4]=(unsigned char)(c>>8);raw[3+i*4]=(unsigned char)c;raw[4+i*4]=(unsigned char)(c>>24);}z[0]=0x78;z[1]=1;z[2]=1;w16(z+3,rn);w16(z+5,rn^65535);memcpy(z+7,raw,rn);be32(z+7+rn,(uint32_t)adler32(1,raw,rn));unsigned zn=rn+11;memcpy(b,"\x89PNG\r\n\x1a\n",8);be32(ihdr,count);be32(ihdr+4,1);ihdr[8]=8;ihdr[9]=6;png_chunk(b+8,"IHDR",ihdr,13);png_chunk(b+33,"IDAT",z,zn);png_chunk(b+45+zn,"IEND",NULL,0);n=57+zn;}
    bool ok=write_bytes(path,b,n,overwrite,error,NULL);free(b);return ok;
}
/* File management stays within paths selected by the app browser. No directory operations. */
bool pd_file_manage(const char *source,const char *destination,unsigned operation,char error[64]){
    struct stat st;if(stat(source,&st)||((st.st_mode&S_IFMT)!=S_IFREG))return fail(error,"Select a regular file");
    if(operation==2)return remove(source)==0?true:fail(error,"Cannot delete file");
    if(operation>2||!destination||!destination[0])return fail(error,"Invalid file operation");
    if(pd_exists(destination))return fail(error,"Name already exists; choose another");
    if(operation==0)return rename(source,destination)==0?true:fail(error,"Cannot rename file");
    if(st.st_size<0||st.st_size>(long)MAX_FILE)return fail(error,"File too large to duplicate (max 9 MiB)");
    size_t n=(size_t)st.st_size;unsigned char *b=malloc(n?n:1);if(!b)return fail(error,"Not enough memory");FILE *f=fopen(source,"rb");if(!f){free(b);return fail(error,"Cannot open source");}bool ok=fread(b,1,n,f)==n&&fgetc(f)==EOF&&!ferror(f);fclose(f);if(ok)ok=write_bytes(destination,b,n,false,error,NULL);else fail(error,"Cannot read source");free(b);return ok;
}
