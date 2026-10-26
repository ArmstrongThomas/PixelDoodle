#include "version.h"
#include "app.h"
#include "animation.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "zlib.h"
#ifdef _WIN32
#include <io.h>
#define SETTINGS_SYNC(f) _commit(_fileno(f))
#else
#include <unistd.h>
#define SETTINGS_SYNC(f) fsync(fileno(f))
#endif
#include "menu_font.h"
#define PALETTE_DEFAULT_COUNT 40u
static const uint32_t palette[PALETTE_DEFAULT_COUNT]={
    0xff182126,0xffffffff,0xffe04444,0xfff5a623,0xfff0d850,0xff218c63,0xff1850b0,0xff864bc1,
    0xff126e72,0xff75858d,0xff753542,0xffd69b6a,0xff6d8fcf,0xff8dbe7c,0xffc883d9,0xff365564,
    0xff2f6b4f,0xff8b5a2b,0xffd8d2c5,0xff8c1c2b,0xff2c87a3,0xffd46a9c,0xffb7a44c,0xff4c667a,
    0xffe07035,0xff694f94,0xff1c1c1c,0xfff8e1b6,0xffa55c3a,0xffbf8f52,0xff88a0b3,0xff4d6574,
    0xff9f6aa8,0xffb64862,0xffd07f76,0xff64b4a4,0xffa7bb76,0xffc8b04d,0xff767676,0xff252f34
};
/* Shared dark surfaces, white labels and bright accents; never remap art pixels. */
#define UI_BG 0x182126u
#define UI_PANEL 0x35444bu
#define UI_EDGE 0x73868fu
#define UI_TEXT 0xf5f8fau
#define UI_MUTED 0xb9cbd2u
#define UI_ACCENT 0x79e0d3u
#define UI_SELECTED 0x0d7a75u
#define UI_DEEP 0x202c32u
#define UI_LINE 0x25333au
#define UI_WARN 0xf2c46bu
static const char *control_names[]={"B / DOWN","A / LEFT","X / UP","Y / RIGHT"};
static const char *action_names[]={"TOOLS","PAN","SAMPLE","ZOOM"};
static bool bindings_valid(unsigned b){unsigned seen=0;for(unsigned i=0;i<4;i++)seen|=1u<<((b>>(i*2))&3);return seen==15;}
unsigned app_bindings(const PdApp *a,unsigned actions){unsigned out=actions&~15u;for(unsigned i=0;i<4;i++)if(actions&(1u<<i))out|=1u<<((a->bindings>>(i*2))&3);return out;}
static const char *binding_name(PdApp *a,unsigned action){for(unsigned i=0;i<4;i++)if(((a->bindings>>(i*2))&3)==action)return control_names[i];return "?";}
static uint32_t hsv(unsigned h,unsigned s,unsigned v){
    unsigned c=(v*s+127)/255,m=v-c,f=h%60;
    unsigned x=(c*((h/60)&1?60-f:f)+30)/60,r=0,g=0,b=0;
    switch(h/60){case 0:r=c;g=x;break;case 1:r=x;g=c;break;case 2:g=c;b=x;break;case 3:g=x;b=c;break;case 4:r=x;b=c;break;default:r=c;b=x;break;}
    return 0xff000000u|((r+m)<<16)|((g+m)<<8)|(b+m);
}
static uint32_t *palette_color_target(PdApp *a){return a->palette_target?&a->background:&a->color;}
static void text_begin_hex(PdApp *a);
static void message(PdApp *a,const char *s);
static void timeline_follow(PdApp *a);
static void palette_load(PdApp *a);
static bool palette_save(PdApp *a);
static void recent_add(PdApp *a,bool promote){unsigned at=0;while(at<a->recent_count&&a->recent[at]!=a->color)at++;if(at<a->recent_count&&!promote)return;if(at==a->recent_count&&a->recent_count<12)a->recent_count++;if(at>11)at=11;memmove(a->recent+1,a->recent,at*sizeof(uint32_t));a->recent[0]=a->color;}
static void project_palette(PdApp *a){a->doc.palette_count=a->swatch_count;memcpy(a->doc.palette,a->swatches,a->swatch_count*sizeof(uint32_t));}
static void numeric_open(PdApp *a,unsigned mode){a->numeric_mode=mode;a->numeric_replace=true;a->page=11;a->picker_update=false;a->redraw=true;if(!mode)snprintf(a->numeric,sizeof(a->numeric),"%06X%02X",(unsigned)(a->color&0xffffff),(unsigned)(a->color>>24));else {unsigned shifts[]={0,16,8,0,24};snprintf(a->numeric,sizeof(a->numeric),"%u",(unsigned)((a->color>>shifts[mode])&255));}}
static void color_hsv(PdApp *a){
    uint32_t color=*palette_color_target(a);
    int r=(color>>16)&255,g=(color>>8)&255,b=color&255;
    int max=r>g?r:g;if(b>max)max=b;int min=r<g?r:g;if(b<min)min=b;int d=max-min;
    a->value=(unsigned)max;a->saturation=max?(unsigned)((d*255+max/2)/max):0;
    if(d){int h=max==r?60*(g-b)/d:max==g?120+60*(b-r)/d:240+60*(r-g)/d;if(h<0)h+=360;a->hue=(unsigned)h;}
}
static unsigned palette_cols(const PdApp *a){static const unsigned cols[]={8,6,5};return cols[a->palette_density>2?2:a->palette_density];}
static int palette_row_pitch(PdApp *a){int cols=(int)palette_cols(a),pitch=(a->width-16)/cols;return pitch<21?21:pitch;}
static int palette_tile(PdApp *a){int tile=palette_row_pitch(a)-3;return tile<18?18:tile;}
static int palette_view_height(PdApp *a){return a->height-90;}
static int palette_content_y(PdApp *a){(void)a;return 68;}
static void palette_clamp(PdApp *a){
    int cols=(int)palette_cols(a),rows=((int)a->swatch_count+cols-1)/cols,max=rows*palette_row_pitch(a)-palette_view_height(a);
    if(max<0)max=0;int *p=&a->palette_scroll[1];if(*p<0)*p=0;if(*p>max)*p=max;
}
static int palette_hit(PdApp *a,int x,int y){
    if(x<8||x>=a->width-8||y<palette_content_y(a)||y>=a->height-22)return -1;
    int cols=(int)palette_cols(a),pitch=palette_row_pitch(a),col=(x-8)/pitch;
    if(col<0||col>=cols)return -1;
    int row=(y-palette_content_y(a)+a->palette_scroll[1])/pitch,i=row*cols+col;
    return i<(int)a->swatch_count?i:-1;
}
static int picker_main_height(PdApp *a){return a->height-100;}
static int picker_square(PdApp *a){int max=picker_main_height(a)-14;if(max>104)max=104;if(max<48)max=48;return max;}
static int picker_side_left(PdApp *a){return 6+picker_square(a)+22;}
static int picker_tile(PdApp *a){int width=a->width-picker_side_left(a)-8,cols=6,tile=(width-(cols-1)*2)/cols;return tile<14?14:tile;}
static void picker_palette_clamp(PdApp *a){
    int cols=6,rows=((int)a->swatch_count+cols-1)/cols,max=rows*(picker_tile(a)+2)-picker_main_height(a);if(max<0)max=0;
    if(a->palette_scroll[0]<0)a->palette_scroll[0]=0;if(a->palette_scroll[0]>max)a->palette_scroll[0]=max;
}
static int picker_palette_hit(PdApp *a,int x,int y){
    int left=picker_side_left(a),top=palette_content_y(a),tile=picker_tile(a);if(x<left||x>=a->width-4||y<top||y>=top+picker_main_height(a))return -1;
    int col=(x-left)/(tile+2),row=(y-top+a->palette_scroll[0])/(tile+2),index=row*6+col;
    return col>=0&&col<6&&index<(int)a->swatch_count?index:-1;
}
static void palette_select(PdApp *a,unsigned index){
    a->swatch_selected=index;uint32_t *target=palette_color_target(a);*target=a->swatches[index];color_hsv(a);a->picker_update=false;a->redraw=true;
}
static void picker_store(PdApp *a){a->palette_picker_active=true;a->palette_picker_color=a->color;a->palette_picker_background=a->background;a->palette_picker_hue=a->hue;a->palette_picker_saturation=a->saturation;a->palette_picker_value=a->value;}
static void picker_cancel(PdApp *a){a->color=a->palette_picker_color;a->background=a->palette_picker_background;a->hue=a->palette_picker_hue;a->saturation=a->palette_picker_saturation;a->value=a->palette_picker_value;a->palette_picker_active=false;a->color_tab=1;a->palette_edit=false;a->redraw=true;}
static void color_open(PdApp *a){a->color_drag=0;a->color_tab=1;a->palette_edit=false;a->palette_target=0;a->palette_picker_active=false;a->swatch_mini=false;color_hsv(a);palette_clamp(a);picker_palette_clamp(a);}
static void color_input(PdApp *a,int x,int y,bool pressed){
    uint32_t *target=palette_color_target(a),old=*target;
    unsigned old_hue=a->hue,old_s=a->saturation,old_v=a->value,old_tab=a->color_tab;bool old_edit=a->palette_edit;
    int square=picker_square(a),top=palette_content_y(a),hue_x=6+square+4;
    if(pressed){
        if(y>=26&&y<44){
            unsigned tab=(unsigned)(x*3/a->width);
            if(tab==0){a->palette_edit=false;a->color_tab=1;a->palette_picker_active=false;}
            else if(tab==1){a->palette_edit=false;a->color_tab=0;if(!a->palette_picker_active)picker_store(a);}
            else {a->palette_edit=true;a->color_tab=1;a->palette_picker_active=false;}
            a->color_drag=0;a->swatch_hold=-1;palette_clamp(a);picker_palette_clamp(a);a->redraw=true;
        }else if(y>=46&&y<66){
            if(x<44)a->palette_target=0;
            else if(x<88)a->palette_target=1;
            else if(x<112){uint32_t c=a->color;a->color=a->background;a->background=c;}
            else if(a->color_tab){unsigned density=(unsigned)((x-116)/45);if(density>2)density=2;a->palette_density=density;palette_clamp(a);}
            else text_begin_hex(a);
            color_hsv(a);a->redraw=true;
        }else if(a->palette_edit){
            if(y>=a->height-28)a->palette_edit=false;
            else if(y>=72&&y<a->height-34){
                unsigned row=(unsigned)(y-72)/28,col=(unsigned)(x*3/a->width),action=row*3+col;
                if(action==0&&a->swatch_count<256){a->swatch_selected=a->swatch_count;a->swatches[a->swatch_count++]=*target;message(a,"Color added to palette");}
                else if(action==1&&a->swatch_count){a->swatches[a->swatch_selected]=*target;message(a,"Swatch replaced");}
                else if(action==2&&a->swatch_count>1){memmove(a->swatches+a->swatch_selected,a->swatches+a->swatch_selected+1,(a->swatch_count-a->swatch_selected-1)*sizeof(uint32_t));a->swatch_count--;if(a->swatch_selected>=a->swatch_count)a->swatch_selected=a->swatch_count-1;message(a,"Swatch removed");}
                else if(action==3&&a->swatch_count>1){unsigned next=(a->swatch_selected+1)%a->swatch_count;uint32_t c=a->swatches[a->swatch_selected];a->swatches[a->swatch_selected]=a->swatches[next];a->swatches[next]=c;a->swatch_selected=next;message(a,"Swatch moved");}
                else if(action==4){palette_load(a);message(a,"Saved palette loaded");}
                else if(action==5)message(a,palette_save(a)?"Default palette saved":"Default palette save failed");
                if(a->swatch_selected>=a->swatch_count)a->swatch_selected=0;
                project_palette(a);a->palette_dirty=true;palette_clamp(a);picker_palette_clamp(a);
            }
            a->redraw=true;
        }else if(a->color_tab){
            int index=palette_hit(a,x,y);if(index>=0){a->swatch_hold=index;a->swatch_target=index;a->swatch_hold_ms=a->now_ms;a->swatch_nav_ms=a->now_ms;a->swatch_start_y=y;a->swatch_start_scroll=a->palette_scroll[1];a->swatch_scrolling=false;a->swatch_mini=false;}
        }else{
            if(y>=a->height-28){if(x<a->width/2){picker_cancel(a);message(a,"Picker changes cancelled");}else{a->palette_picker_active=false;a->color_tab=1;message(a,"Color applied");}a->redraw=true;}
            else if(x>=6&&x<6+square&&y>=top&&y<top+square)a->color_drag=1;
            else if(x>=hue_x&&x<hue_x+12&&y>=top&&y<top+square)a->color_drag=2;
            else if(y>=top+square+2&&y<top+square+14&&x>=6&&x<hue_x+12)a->color_drag=3;
            else {int index=picker_palette_hit(a,x,y);if(index>=0){a->swatch_hold=index;a->swatch_target=index;a->swatch_hold_ms=a->now_ms;a->swatch_nav_ms=a->now_ms;a->swatch_start_y=y;a->swatch_start_scroll=a->palette_scroll[0];a->swatch_scrolling=false;a->swatch_mini=true;}}
        }
    }
    if(!pressed&&a->swatch_hold>=0){
        int delta=y-a->swatch_start_y;uint32_t elapsed=a->now_ms-a->swatch_hold_ms;int *scroll=&a->palette_scroll[a->swatch_mini?0:1];
        if(!a->swatch_drag&&(elapsed<150)&&(delta>5||delta< -5))a->swatch_scrolling=true;
        if(a->swatch_scrolling){int old_scroll=*scroll;*scroll=a->swatch_start_scroll-delta;if(a->swatch_mini)picker_palette_clamp(a);else palette_clamp(a);if(old_scroll!=*scroll)a->redraw=true;}
        else if(elapsed>=150&&!a->swatch_mini&&!a->palette_edit){
            a->swatch_drag=true;if((uint32_t)(a->now_ms-a->swatch_nav_ms)>=100){if(y<72)*scroll-=palette_row_pitch(a);else if(y>=a->height-24)*scroll+=palette_row_pitch(a);palette_clamp(a);a->swatch_nav_ms=a->now_ms;}
            int yy=y;if(yy<68)yy=68;if(yy>=a->height-22)yy=a->height-23;int index=palette_hit(a,x,yy);if(index>=0)a->swatch_target=index;a->redraw=true;
        }
    }
    if(a->color_drag==1){int sx=x-6,vy=y-top;if(sx<0)sx=0;if(sx>=square)sx=square-1;if(vy<0)vy=0;if(vy>=square)vy=square-1;a->saturation=(unsigned)(sx*255/(square-1));a->value=(unsigned)(255-vy*255/(square-1));}
    if(a->color_drag==2){int hy=y-top;if(hy<0)hy=0;if(hy>=square)hy=square-1;a->hue=(unsigned)(hy*359/(square-1));}
    if(a->color_drag==1||a->color_drag==2)*target=(*target&0xff000000u)|(hsv(a->hue,a->saturation,a->value)&0xffffffu);
    if(a->color_drag==3){int alpha_x=x-6,alpha_w=square+12;if(alpha_x<0)alpha_x=0;if(alpha_x>=alpha_w)alpha_x=alpha_w-1;*target=(*target&0xffffffu)|((unsigned)(alpha_x*255/(alpha_w-1))<<24);}
    if(a->page==2&&(old!=*target||old_hue!=a->hue||old_s!=a->saturation||old_v!=a->value||old_tab!=a->color_tab||old_edit!=a->palette_edit)){a->picker_update=!a->redraw;a->redraw=true;}
}
static void message(PdApp *a,const char *s){a->picker_update=false;snprintf(a->notice,sizeof(a->notice),"%s",s);a->redraw=true;}
/* Masks cover the largest canvas the profile allows: 2 KiB each in DS mode, 32 KiB at 512 square. */
static bool masks_reserve(PdApp *a,unsigned words){
    if(a->selection&&a->mask_words==words)return true;
    uint32_t *m[4];for(int i=0;i<4;i++){m[i]=calloc(words,4);if(!m[i]){while(i--)free(m[i]);return false;}}
    free(a->selection);free(a->selection_base);free(a->selection_draft);free(a->guide_damage);
    a->selection=m[0];a->selection_base=m[1];a->selection_draft=m[2];a->guide_damage=m[3];a->mask_words=words;a->selection_valid=false;return true;
}
static unsigned mask_bytes(const PdApp *a){return a->mask_words*4u;}
static bool masks_fit(PdApp *a){return (a->selection&&pd_plane(&a->doc)/32<=a->mask_words)||masks_reserve(a,pd_plane(&a->doc)/32>PD_PIXELS/32?PD_MAX_PIXELS/32:PD_PIXELS/32);}
/* Measured by allocation, so the result is what a new buffer could really get. */
static size_t largest_block(size_t limit){size_t lo=0,hi=limit+1;while(hi-lo>1024){size_t mid=lo+(hi-lo)/2;void *p=malloc(mid);if(p){*(volatile unsigned char *)p=0;free(p);lo=mid;}else hi=mid;}return lo;}
static unsigned elapsed_us(const PdApp *a,uint32_t start){return (unsigned)((uint64_t)(uint32_t)(a->clock_ticks()-start)*1000000u/a->clock_hz);}
static void diagnostics_run(PdApp *a){
    a->free_kb=a->largest_kb=a->card_write_kbs=a->card_read_kbs=0;a->memory_measured=false;
    if(a->memory_limit){void *held[64];unsigned n=0;size_t total=0;a->largest_kb=(unsigned)(largest_block(a->memory_limit)/1024);
        a->memory_measured=true;while(n<64&&total<a->memory_limit){size_t s=largest_block(a->memory_limit-total);void *p=s>=4096?malloc(s):NULL;if(!p)break;held[n++]=p;total+=s;}
        while(n)free(held[--n]);a->free_kb=(unsigned)(total/1024);}
    if(!a->directory[0]){snprintf(a->diag_status,sizeof(a->diag_status),"NO STORAGE FOR CARD TEST");return;}
    if(!a->clock_ticks||!a->clock_hz){snprintf(a->diag_status,sizeof(a->diag_status),"NO TIMER FOR CARD TEST");return;}
    enum{CHUNK=64*1024,CHUNKS=4};char path[PD_PATH];unsigned char *buf=malloc(CHUNK),*back=malloc(CHUNK);bool ok=buf&&back&&pd_join(path,a->directory,".diag-speed.bin");
    if(ok)for(unsigned i=0;i<CHUNK;i++)buf[i]=(unsigned char)(i*31u+7u);
    FILE *f=ok?fopen(path,"wb"):NULL;uint32_t t=a->clock_ticks();
    for(unsigned i=0;f&&ok&&i<CHUNKS;i++)ok=fwrite(buf,1,CHUNK,f)==CHUNK;
    if(f){ok=ok&&fflush(f)==0&&SETTINGS_SYNC(f)==0;if(fclose(f))ok=false;}else ok=false;
    unsigned write_us=ok?elapsed_us(a,t):0;f=ok?fopen(path,"rb"):NULL;t=a->clock_ticks();
    for(unsigned i=0;f&&ok&&i<CHUNKS;i++)ok=fread(back,1,CHUNK,f)==CHUNK&&!memcmp(buf,back,CHUNK);
    unsigned read_us=ok&&f?elapsed_us(a,t):0;if(f)fclose(f);if(ok||f)remove(path);free(buf);free(back);
    if(!ok){snprintf(a->diag_status,sizeof(a->diag_status),"CARD TEST FILE FAILED");return;}
    a->card_write_kbs=(unsigned)((uint64_t)CHUNK*CHUNKS/1024*1000000u/(write_us?write_us:1));a->card_read_kbs=(unsigned)((uint64_t)CHUNK*CHUNKS/1024*1000000u/(read_us?read_us:1));
    snprintf(a->diag_status,sizeof(a->diag_status),"256 KB TEST FILE, REMOVED AFTER");
}
bool app_set_profile(PdApp *a,unsigned profile){a->doc.profile=profile;return masks_reserve(a,(profile<=1?PD_PIXELS:PD_MAX_PIXELS)/32);}
bool app_reserve_gallery(PdApp *a){if(!a->gallery_thumbs)a->gallery_thumbs=calloc(6,sizeof(*a->gallery_thumbs));return a->gallery_thumbs!=NULL;}
void app_dispose(PdApp *a){free(a->onion);a->onion=NULL;a->onion_plane=a->onion_bytes=0;free(a->film);a->film=NULL;pd_frames_dispose(&a->doc);free(a->selection);free(a->selection_base);free(a->selection_draft);free(a->guide_damage);free(a->clipboard);free(a->gallery_thumbs);
    a->selection=a->selection_base=a->selection_draft=a->guide_damage=NULL;a->clipboard=NULL;a->gallery_thumbs=NULL;a->mask_words=0;}
static uint32_t pal_get(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void pal_put(unsigned char *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static unsigned palette_read(PdApp *a,unsigned slot,unsigned char b[1040]){
    char path[512];if(!a->directory[0])return 0;snprintf(path,sizeof(path),"%s/palette%u.bin",a->directory,slot);FILE *f=fopen(path,"rb");if(!f)return 0;
    bool ok=fread(b,1,1040,f)==1040&&fgetc(f)==EOF&&!ferror(f);if(fclose(f))ok=false;
    if(!ok||memcmp(b,"PPL1",4)||pal_get(b+8)<1||pal_get(b+8)>256||pal_get(b+1036)!=(uint32_t)crc32(0,b,1036))return 0;return pal_get(b+4);
}
static void palette_load(PdApp *a){unsigned char b[1040],c[1040];unsigned s0=palette_read(a,0,b),s1=palette_read(a,1,c);if(!s0&&!s1)return;unsigned char *p=s1>s0?c:b;a->swatch_count=pal_get(p+8);for(unsigned i=0;i<a->swatch_count;i++)a->swatches[i]=pal_get(p+12+i*4);}
static bool palette_save(PdApp *a){
    unsigned char b[1040]={0},c[1040];unsigned s0=palette_read(a,0,b),s1=palette_read(a,1,c),seq=s0>s1?s0:s1;if(!a->directory[0]||seq==UINT32_MAX)return false;
    memset(b,0,sizeof(b));memcpy(b,"PPL1",4);pal_put(b+4,++seq);pal_put(b+8,a->swatch_count);for(unsigned i=0;i<a->swatch_count;i++)pal_put(b+12+i*4,a->swatches[i]);pal_put(b+1036,(uint32_t)crc32(0,b,1036));
    unsigned slot=s0<=s1?0:1;char path[512];snprintf(path,sizeof(path),"%s/palette%u.bin",a->directory,slot);FILE *f=fopen(path,"wb");if(!f)return false;
    bool ok=fwrite(b,1,1040,f)==1040&&fflush(f)==0&&SETTINGS_SYNC(f)==0;if(fclose(f))ok=false;return ok&&palette_read(a,slot,c)==seq&&!memcmp(b,c,1040);
}
#define RECENT_BYTES (16+8*PD_PATH)
static bool recent_path_valid(const char *p){if(!p[0]||p[0]=='/'||strchr(p,'\\')||strchr(p,':'))return false;while(*p){const char *end=strchr(p,'/');size_t n=end?(size_t)(end-p):strlen(p);if(!n||(n==1&&p[0]=='.')||(n==2&&p[0]=='.'&&p[1]=='.'))return false;if(!end)break;p=end+1;if(!*p)return false;}return true;}
static unsigned recent_read(PdApp *a,unsigned slot,unsigned char *b){char path[PD_PATH];if(!a->directory[0])return 0;snprintf(path,sizeof(path),"%s/recent%u.bin",a->directory,slot);FILE *f=fopen(path,"rb");if(!f)return 0;bool ok=fread(b,1,RECENT_BYTES,f)==RECENT_BYTES&&fgetc(f)==EOF&&!ferror(f);fclose(f);if(!ok||memcmp(b,"PRJ1",4)||pal_get(b+8)>8||pal_get(b+RECENT_BYTES-4)!=(uint32_t)crc32(0,b,RECENT_BYTES-4))return 0;for(unsigned i=0;i<pal_get(b+8);i++){char *p=(char*)b+12+i*PD_PATH;if(!memchr(p,0,PD_PATH)||!recent_path_valid(p))return 0;}return pal_get(b+4);}
static void recent_projects_load(PdApp *a){unsigned char *b=malloc(RECENT_BYTES*2);if(!b)return;unsigned s0=recent_read(a,0,b),s1=recent_read(a,1,b+RECENT_BYTES);if(s0||s1){unsigned char *p=s1>s0?b+RECENT_BYTES:b;a->recent_project_count=pal_get(p+8);memcpy(a->recent_projects,p+12,sizeof(a->recent_projects));}free(b);}
static void recent_projects_save(PdApp *a){unsigned char *b=malloc(RECENT_BYTES*2);a->recent_files_unsaved=true;if(!b)return;unsigned char *other=b+RECENT_BYTES;unsigned s0=recent_read(a,0,b),s1=recent_read(a,1,other),seq=s0>s1?s0:s1;if(!a->directory[0]||seq==UINT32_MAX){free(b);return;}memset(b,0,RECENT_BYTES);memcpy(b,"PRJ1",4);pal_put(b+4,++seq);pal_put(b+8,a->recent_project_count);memcpy(b+12,a->recent_projects,sizeof(a->recent_projects));pal_put(b+RECENT_BYTES-4,(uint32_t)crc32(0,b,RECENT_BYTES-4));unsigned slot=s0<=s1?0:1;char path[PD_PATH];snprintf(path,sizeof(path),"%s/recent%u.bin",a->directory,slot);FILE *f=fopen(path,"wb");bool ok=false;if(f){ok=fwrite(b,1,RECENT_BYTES,f)==RECENT_BYTES&&fflush(f)==0&&SETTINGS_SYNC(f)==0;if(fclose(f))ok=false;}if(ok)ok=recent_read(a,slot,other)==seq&&!memcmp(b,other,RECENT_BYTES);free(b);a->recent_files_unsaved=!ok;}
static unsigned brush_read(PdApp *a,unsigned slot,unsigned char b[20]){char path[PD_PATH];if(!a->directory[0])return 0;snprintf(path,sizeof(path),"%s/tools%u.bin",a->directory,slot);FILE *f=fopen(path,"rb");if(!f)return 0;bool ok=fread(b,1,20,f)==20&&fgetc(f)==EOF&&!ferror(f);fclose(f);if(!ok||(memcmp(b,"PDB1",4)&&memcmp(b,"PDB2",4)&&memcmp(b,"PDB3",4)&&memcmp(b,"PDB4",4)&&memcmp(b,"PDB5",4))||b[13]>15||(!memcmp(b,"PDB3",4)&&b[14]>3)||(!memcmp(b,"PDB4",4)&&b[14]>7)||(!memcmp(b,"PDB5",4)&&b[14]>127)||b[8]<1||b[8]>16||b[9]<1||b[9]>16||b[10]>1||b[11]>1||b[12]>1||pal_get(b+16)!=(uint32_t)crc32(0,b,16))return 0;return pal_get(b+4);}
static void brush_load(PdApp *a){unsigned char b[20],c[20];unsigned s0=brush_read(a,0,b),s1=brush_read(a,1,c);if(s0||s1){unsigned char *p=s1>s0?c:b;a->brush_size[0]=p[8];a->brush_size[1]=p[9];a->brush_round[0]=p[10];a->brush_round[1]=p[11];a->pixel_grid=p[12]!=0;a->guides=memcmp(p,"PDB1",4)?p[13]:0;if(!memcmp(p,"PDB3",4)||!memcmp(p,"PDB4",4)||!memcmp(p,"PDB5",4)){a->pixel_perfect=memcmp(p,"PDB3",4)&&(p[14]&4)!=0;a->fill_pattern=!memcmp(p,"PDB5",4)?(p[14]>>3)&3:0;a->shape_pattern=!memcmp(p,"PDB5",4)?(p[14]>>5)&3:0;a->fill_global=(p[14]&1)!=0;a->fill_visible=(p[14]&2)!=0;a->fill_tolerance=p[15];}}}
static void brush_save(PdApp *a){unsigned char b[20]={0},c[20];unsigned s0=brush_read(a,0,b),s1=brush_read(a,1,c),seq=s0>s1?s0:s1;if(!a->directory[0]||seq==UINT32_MAX)return;memset(b,0,20);memcpy(b,"PDB5",4);pal_put(b+4,seq+1);b[8]=(unsigned char)a->brush_size[0];b[9]=(unsigned char)a->brush_size[1];b[10]=(unsigned char)a->brush_round[0];b[11]=(unsigned char)a->brush_round[1];b[12]=a->pixel_grid;b[13]=(unsigned char)a->guides;b[14]=(unsigned char)((a->fill_global?1:0)|(a->fill_visible?2:0)|(a->pixel_perfect?4:0)|((a->fill_pattern&3)<<3)|((a->shape_pattern&3)<<5));b[15]=(unsigned char)a->fill_tolerance;pal_put(b+16,(uint32_t)crc32(0,b,16));unsigned slot=s0<=s1?0:1;char path[PD_PATH];snprintf(path,sizeof(path),"%s/tools%u.bin",a->directory,slot);FILE *f=fopen(path,"wb");bool ok=false;if(f){ok=fwrite(b,1,20,f)==20&&fflush(f)==0&&SETTINGS_SYNC(f)==0;if(fclose(f))ok=false;}if(!ok||brush_read(a,slot,c)!=seq+1||memcmp(b,c,20))message(a,"Tool preferences not saved");}
/* Separate dual slots preserve the established PDB5 tool settings format. */
static unsigned presets_read(PdApp *a,unsigned slot,unsigned char b[32]){
    char path[PD_PATH];if(!a->directory[0])return 0;snprintf(path,sizeof(path),"%s/brush-presets%u.bin",a->directory,slot);
    FILE *f=fopen(path,"rb");if(!f)return 0;bool ok=fread(b,1,32,f)==32&&fgetc(f)==EOF&&!ferror(f);fclose(f);
    if(!ok||memcmp(b,"PBP1",4)||b[8]>3||b[9]||b[10]||b[11]||pal_get(b+28)!=(uint32_t)crc32(0,b,28))return 0;
    for(unsigned i=12;i<28;i+=4)if(b[i]<1||b[i]>16||b[i+1]>1||b[i+2]>1||b[i+3]>3)return 0;
    return pal_get(b+4);
}
static void presets_load(PdApp *a){
    static const unsigned char defaults[4][4]={{1,0,1,0},{4,1,0,0},{8,0,0,2},{8,1,0,1}};
    memcpy(a->brush_presets,defaults,sizeof(defaults));unsigned char b[32],c[32];unsigned s0=presets_read(a,0,b),s1=presets_read(a,1,c);
    if(s0||s1){unsigned char *p=s1>s0?c:b;a->brush_pattern=p[8];memcpy(a->brush_presets,p+12,16);}
}
static void presets_save(PdApp *a){
    unsigned char b[32]={0},c[32];unsigned s0=presets_read(a,0,b),s1=presets_read(a,1,c),seq=s0>s1?s0:s1;
    if(!a->directory[0]||seq==UINT32_MAX){message(a,"Brush presets are session-only");return;}
    memset(b,0,32);memcpy(b,"PBP1",4);pal_put(b+4,seq+1);b[8]=(unsigned char)a->brush_pattern;memcpy(b+12,a->brush_presets,16);pal_put(b+28,(uint32_t)crc32(0,b,28));
    unsigned slot=s0<=s1?0:1;char path[PD_PATH];snprintf(path,sizeof(path),"%s/brush-presets%u.bin",a->directory,slot);FILE *f=fopen(path,"wb");bool ok=false;
    if(f){ok=fwrite(b,1,32,f)==32&&fflush(f)==0&&SETTINGS_SYNC(f)==0;if(fclose(f))ok=false;}
    if(!ok||presets_read(a,slot,c)!=seq+1||memcmp(b,c,32))message(a,"Brush presets not saved");
}
static const char *relative_project(PdApp *a,const char *path){size_t n=strlen(a->directory);return n&&!strncmp(path,a->directory,n)&&path[n]=='/'&&recent_path_valid(path+n+1)?path+n+1:NULL;}
static void recent_project_add(PdApp *a,const char *path){const char *relative=relative_project(a,path);if(!relative)return;unsigned i=0;while(i<a->recent_project_count&&strcmp(a->recent_projects[i],relative))i++;if(i==a->recent_project_count&&a->recent_project_count<8)a->recent_project_count++;if(i>7)i=7;memmove(a->recent_projects+1,a->recent_projects,i*PD_PATH);snprintf(a->recent_projects[0],PD_PATH,"%s",relative);recent_projects_save(a);}
static void recent_project_change(PdApp *a,const char *old,const char *replacement){const char *relative=relative_project(a,old),*next=replacement?relative_project(a,replacement):NULL;if(!relative)return;for(unsigned i=0;i<a->recent_project_count;i++)if(!strcmp(a->recent_projects[i],relative)){if(next)snprintf(a->recent_projects[i],PD_PATH,"%s",next);else {memmove(a->recent_projects+i,a->recent_projects+i+1,(a->recent_project_count-i-1)*PD_PATH);a->recent_project_count--;memset(a->recent_projects[a->recent_project_count],0,PD_PATH);}recent_projects_save(a);break;}}
static unsigned file_type(const char *name){const char *ext=strrchr(name,'.');char e[12]={0};if(!ext||strlen(ext)>=sizeof(e))return 0;for(unsigned i=0;ext[i];i++)e[i]=(char)(ext[i]>='A'&&ext[i]<='Z'?ext[i]+32:ext[i]);if(!strcmp(e,".pdd"))return 1;if(!strcmp(e,".ase")||!strcmp(e,".aseprite"))return 2;if(!strcmp(e,".gpl"))return 3;if(!strcmp(e,".hex"))return 4;if(!strcmp(e,".png"))return 5;return 0;}
static void preview_file(PdApp *a,const char *path){
    const char *base=strrchr(path,'/');snprintf(a->preview_name,sizeof(a->preview_name),"%s",base?base+1:path);a->preview_attempted=true;a->preview_palette_count=0;
    a->preview_width=a->preview_height=a->preview_layers=a->preview_frames=0;a->preview_bytes=-1;
    FILE *size_probe=fopen(path,"rb");if(size_probe){if(!fseek(size_probe,0,SEEK_END))a->preview_bytes=ftell(size_probe);fclose(size_probe);}
    unsigned type=file_type(path);if(type==3||type==4||(type==5&&(a->file_mode>=4||a->file_filter==3))){uint32_t colors[256];unsigned count=0;a->preview_ready=pd_palette_read(path,colors,&count,a->preview_error);if(a->preview_ready){a->preview_palette_count=count;unsigned cols=count<=16?4:count<=64?8:16;memset(a->file_preview,0,sizeof(a->file_preview));for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){unsigned i=(y/(64/cols))*cols+x/(64/cols);if(i<count)a->file_preview[y*64+x]=colors[i];}}}
    else{PdPreviewInfo info;a->preview_ready=pd_preview_file_info(path,a->file_preview,a->doc.profile,&info,a->preview_error);
        a->preview_width=info.width;a->preview_height=info.height;a->preview_layers=info.layers;a->preview_frames=info.frames;}
    a->redraw=true;
}
static const char *filter_names[]={"PROJECTS","PDD","ASEPRITE","PALETTES","GPL","HEX","PNG","ALL"};
static bool file_matches(PdApp *a,PdEntry *e){unsigned t=file_type(e->name);return e->directory||a->file_filter==7||(a->file_filter==0&&(t==1||t==2||t==5))||(a->file_filter==1&&t==1)||(a->file_filter==2&&t==2)||(a->file_filter==3&&t>=3)||(a->file_filter>=4&&a->file_filter<=6&&t==a->file_filter-1);}
static unsigned file_rows(PdApp *a){unsigned n=0;for(unsigned i=0;i<a->listing.count;i++)if(file_matches(a,&a->listing.entries[i]))n++;return n;}
static int file_index(PdApp *a,unsigned row){for(unsigned i=0;i<a->listing.count;i++)if(file_matches(a,&a->listing.entries[i])){if(!row)return (int)i;row--;}return -1;}
/* Browser geometry shared by drawing and touch: one-line rows, 7 visible on DSi and 9 on 3DS. */
typedef struct { int row_h,list_top,list_bottom,footer_y,footer_h; } BrowserLayout;
static BrowserLayout browser_layout(const PdApp *a){
    BrowserLayout l;bool big=a->height>192;l.row_h=big?16:14;l.footer_h=big?24:20;l.footer_y=(int)a->height-l.footer_h-4;l.list_top=64;l.list_bottom=l.footer_y-3;return l;
}
static void file_clamp(PdApp *a){BrowserLayout l=browser_layout(a);int max=(int)file_rows(a)*l.row_h-(l.list_bottom-l.list_top);if(max<0)max=0;if(a->file_scroll<0)a->file_scroll=0;if(a->file_scroll>max)a->file_scroll=max;}
/* Type filter dropdown: icon and extensions shown beside each of the eight filters. */
enum { ICON_FILE, ICON_PDD, ICON_ASE, ICON_PAL, ICON_PNG, ICON_DIR };
static const unsigned filter_icons[8]={ICON_PDD,ICON_PDD,ICON_ASE,ICON_PAL,ICON_PAL,ICON_PAL,ICON_PNG,ICON_FILE};
static const char *filter_exts[8]={"PDD ASE PNG","PDD","ASE","GPL HEX PNG","GPL","HEX","PNG","*"};
static unsigned type_icon_for(unsigned type,bool directory){return directory?ICON_DIR:type==1?ICON_PDD:type==2?ICON_ASE:type==3||type==4?ICON_PAL:type==5?ICON_PNG:ICON_FILE;}
static void browser_open_filter(PdApp *a){
    BrowserLayout l=browser_layout(a);int row=l.row_h+1;pd_dropdown_open(&a->file_type_dropdown,30,63,(int)a->width-34,1,row,8,8,a->file_filter,true);
}

static int canvas_top(const PdApp *a){return a->tool==4?50:24;}
/* With two or more frames the canvas ends above the frame strip. */
static int strip_h(const PdApp *a){return pd_frame_count(&a->doc)>1?(a->width>256?22:18):0;}
static int canvas_bottom(const PdApp *a){return a->height-strip_h(a);}
static void clamp(PdApp *a){int vw=a->width/a->zoom,vh=(canvas_bottom(a)-canvas_top(a))/a->zoom;
    int mx=(int)a->doc.width-vw,my=(int)a->doc.height-vh;if(mx<0)mx=0;if(my<0)my=0;
    int minx=(a->guides&4)?-vw:0,miny=(a->guides&8)?-vh:0;if(a->guides&4)mx=(int)a->doc.width+vw;if(a->guides&8)my=(int)a->doc.height+vh;
    if(a->ox<minx){a->ox=minx;a->subx=0;}if(a->oy<miny){a->oy=miny;a->suby=0;}if(a->ox>=mx){a->ox=mx;a->subx=0;}if(a->oy>=my){a->oy=my;a->suby=0;}
}
static void zoom(PdApp *a,int dir){int vw=a->width/a->zoom,vh=(canvas_bottom(a)-canvas_top(a))/a->zoom;if(vw>(int)a->doc.width)vw=(int)a->doc.width;if(vh>(int)a->doc.height)vh=(int)a->doc.height;int cx=a->ox+vw/2,cy=a->oy+vh/2;
    if(dir>0&&a->zoom<32)a->zoom*=2;if(dir<0&&a->zoom>1)a->zoom/=2;
    a->subx=a->suby=0;a->ox=cx-a->width/(2*a->zoom);a->oy=cy-(canvas_bottom(a)-canvas_top(a))/(2*a->zoom);clamp(a);a->redraw=true;
}
static bool world(PdApp *a,int x,int y,int *wx,int *wy){
    int left=(int)a->doc.width*a->zoom<a->width?(a->width-(int)a->doc.width*a->zoom)/2:0;
    int top=canvas_top(a)+((int)a->doc.height*a->zoom<canvas_bottom(a)-canvas_top(a)?(canvas_bottom(a)-canvas_top(a)-(int)a->doc.height*a->zoom)/2:0);
    int dx=x-left+a->subx,dy=y-top+a->suby;*wx=a->ox+(dx>=0?dx/a->zoom:-((-dx+a->zoom-1)/a->zoom));*wy=a->oy+(dy>=0?dy/a->zoom:-((-dy+a->zoom-1)/a->zoom));
    return ((a->guides&4)||(*wx>=0&&*wx<(int)a->doc.width))&&((a->guides&8)||(*wy>=0&&*wy<(int)a->doc.height));
}
static void finish(PdApp *a){
    if(a->stroke){pd_commit(&a->doc);a->stroke=false;a->selection_revision=a->doc.revision;
        if(!a->page&&!a->confirm&&!a->toolbox&&!a->zoom_overlay){a->overview_pending=true;a->overview_at=a->now_ms+80;}
        else {a->partial=false;a->redraw=true;}}
    a->lastx=-4097;
}
void app_pan(PdApp *a,int dx,int dy){
    if(!dx&&!dy)return;
    if(a->page||a->confirm||a->toolbox||a->zoom_overlay||a->touching)return;
    int x=a->ox,y=a->oy;a->ox+=dx;a->oy+=dy;clamp(a);
    if(x!=a->ox||y!=a->oy){a->pan_frame=!a->redraw;a->partial=false;a->redraw=true;}
}
static void pan_pixels(PdApp *a,int dx,int dy){
    int oldx=a->ox*a->zoom+a->subx,oldy=a->oy*a->zoom+a->suby,px=oldx+dx,py=oldy+dy;
    a->ox=px>=0?px/a->zoom:-((-px+a->zoom-1)/a->zoom);a->oy=py>=0?py/a->zoom:-((-py+a->zoom-1)/a->zoom);a->subx=px-a->ox*a->zoom;a->suby=py-a->oy*a->zoom;clamp(a);
    if(oldx!=a->ox*a->zoom+a->subx||oldy!=a->oy*a->zoom+a->suby){a->pan_frame=!a->redraw;a->partial=false;a->redraw=true;}
}
void app_pan_pixels(PdApp *a,int dx,int dy){if(a->page||a->confirm||a->toolbox||a->zoom_overlay||a->touching||(!dx&&!dy))return;pan_pixels(a,dx,dy);}
static int zoom_left(PdApp *a){return a->zoom_side==2||(a->zoom_side==0&&a->handed)?a->width-56:8;}
/* Two independent versioned records; a failed write leaves the other slot intact. */
static unsigned settings_read(PdApp *a,unsigned slot,unsigned char r[16]){
    char path[512];if(!a->directory[0])return 0;
    snprintf(path,sizeof(path),"%s/settings%u.bin",a->directory,slot);
    FILE *f=fopen(path,"rb");if(!f)return 0;
    bool ok=fread(r,1,16,f)==16&&fgetc(f)==EOF&&!ferror(f);if(fclose(f))ok=false;
    if(!ok||r[8]>1||r[9]>2||r[11])return 0;
    if(!memcmp(r,"PDS1",4)){if(r[10])return 0;}else if(memcmp(r,"PDS2",4)||!bindings_valid(r[10]))return 0;
    uint32_t crc=(uint32_t)crc32(0,r,12),stored=0,seq=0;
    for(unsigned i=0;i<4;i++){stored|=(uint32_t)r[12+i]<<(8*i);seq|=(uint32_t)r[4+i]<<(8*i);}
    return crc==stored?seq:0;
}
static bool settings_save(PdApp *a){
    unsigned char r[16]={0},other[16];unsigned s0=settings_read(a,0,r),s1=settings_read(a,1,other),seq=(s0>s1?s0:s1);
    if(!a->directory[0]||seq==0xffffffffu)return false;
    seq++;memset(r,0,16);memcpy(r,"PDS2",4);r[10]=(unsigned char)a->draft_bindings;r[8]=(unsigned char)a->draft_handed;r[9]=(unsigned char)a->draft_zoom_side;
    for(unsigned i=0;i<4;i++)r[4+i]=(unsigned char)(seq>>(8*i));
    uint32_t crc=(uint32_t)crc32(0,r,12);for(unsigned i=0;i<4;i++)r[12+i]=(unsigned char)(crc>>(8*i));
    unsigned slot=s0<=s1?0:1;char path[512];snprintf(path,sizeof(path),"%s/settings%u.bin",a->directory,slot);
    FILE *f=fopen(path,"wb");if(!f)return false;
    bool ok=fwrite(r,1,16,f)==16&&fflush(f)==0&&SETTINGS_SYNC(f)==0;if(fclose(f))ok=false;
    return ok&&settings_read(a,slot,other)==seq&&!memcmp(r,other,16);
}
static void browse_refresh(PdApp *a){a->preview_attempted=false;a->preview_ready=false;
    a->selected=-1;a->file_page=0;a->file_scroll=0;a->file_hold=-1;
    if(!pd_list(a->folder,&a->listing,a->notice))a->listing.count=0;
    else message(a,a->listing.truncated?"Some entries omitted (128 max)":"Choose a folder or file");
    a->redraw=true;
}
static void browse(PdApp *a,unsigned mode){
    finish(a);pd_dropdown_close(&a->file_type_dropdown);a->file_mode=mode;a->file_filter=mode>=4?3:mode==3?6:0;a->page=4;a->confirm=0;
    if(!a->folder[0])snprintf(a->folder,sizeof(a->folder),"%s",a->directory);
    browse_refresh(a);
}
static void write_pending(PdApp *a,bool replace){
    bool ok=a->file_mode>=5?pd_palette_write(a->pending,a->swatches,a->swatch_count,a->file_mode-5,replace,a->notice):pd_write_file(&a->doc,a->pending,a->file_mode-1,replace,a->notice);
    if(ok){
        if(a->file_mode==1){snprintf(a->project,sizeof(a->project),"%s",a->pending);a->project_native=true;recent_project_add(a,a->project);}
        message(a,a->file_mode==1?"Project saved and verified":"Export saved and verified");if(a->file_mode==2){for(unsigned i=0;i<PD_LAYERS;i++)if((a->doc.enabled&(1u<<i))&&(a->doc.layer[i].flags&PD_LAYER_ALPHA_LOCK)){message(a,"Export saved; alpha lock is native-only");break;}}a->page=a->file_mode>=5?2:0;if(a->file_mode>=5)a->color_tab=1;
    }else a->redraw=true;
}
static void save(PdApp *a){
    finish(a);
    if(!a->project_native||!a->project[0]){browse(a,1);return;}
    a->file_mode=1;snprintf(a->pending,sizeof(a->pending),"%s",a->project);write_pending(a,true);
}
static void frame_marks_recount(PdApp *a){unsigned n=0;for(unsigned i=0;i<PD_FRAMES;i++)if(a->frame_mark[i])n++;a->frame_mark_count=n;}
static void frame_marks_clear(PdApp *a){memset(a->frame_mark,0,sizeof(a->frame_mark));a->frame_mark_count=0;}
static void frame_marks_clamp(PdApp *a){unsigned n=pd_frame_count(&a->doc);bool any=false;for(unsigned i=n;i<PD_FRAMES;i++)if(a->frame_mark[i]){a->frame_mark[i]=0;any=true;}if(any)frame_marks_recount(a);}
static void frame_mark_toggle(PdApp *a,unsigned f){if(f>=PD_FRAMES)return;a->frame_mark[f]^=1;frame_marks_recount(a);}
static void open_pending(PdApp *a){
    if(a->file_mode==4){if(pd_palette_read(a->pending,a->swatches,&a->swatch_count,a->notice)){a->swatch_selected=0;a->palette_scroll[0]=a->palette_scroll[1]=0;project_palette(a);a->page=2;a->color_tab=1;message(a,"Project palette imported");}else a->redraw=true;return;}
    if(pd_open_file(&a->doc,a->pending,a->notice)){a->selection_valid=false;memset(a->selection,0,mask_bytes(a));frame_marks_clear(a);
        const char *ext=strrchr(a->pending,'.');a->project_native=ext&&!strcmp(ext,".pdd");
        snprintf(a->project,sizeof(a->project),"%s",a->pending);recent_project_add(a,a->project);snprintf(a->folder,sizeof(a->folder),"%s",a->project);char *parent=strrchr(a->folder,'/');if(parent)*parent=0;
        if(!a->project_native)a->doc.revision=++a->doc.serial;
        a->recovery_revision=0;a->recovery_at=a->now_ms;
        if(a->doc.palette_count){a->swatch_count=a->doc.palette_count;memcpy(a->swatches,a->doc.palette,a->swatch_count*4);}else {a->swatch_count=PALETTE_DEFAULT_COUNT;memcpy(a->swatches,palette,sizeof(palette));palette_load(a);project_palette(a);a->doc.saved_palette_count=a->doc.palette_count;memcpy(a->doc.saved_palette,a->doc.palette,sizeof(a->doc.palette));}a->swatch_selected=0;a->palette_scroll[0]=a->palette_scroll[1]=0;
        a->ox=a->oy=a->subx=a->suby=0;a->page=0;message(a,"Project opened");
    }else a->redraw=true;
}
/* Recovery is a separate native snapshot; it never marks the named project saved. */
static bool recovery_path(PdApp *a,char path[PD_PATH],const char *name){char folder[PD_PATH];return pd_join(folder,a->directory,".recovery")&&pd_join(path,folder,name);}
static void recovery_scan(PdApp *a){char path[PD_PATH];a->recovery_available=(recovery_path(a,path,"session.pdd")&&pd_exists(path))||(recovery_path(a,path,"session.pdd.bak")&&pd_exists(path))||(recovery_path(a,path,"session.pdd.tmp")&&pd_exists(path));}
static void recovery_tick(PdApp *a,bool touch,unsigned down){
    if(!a->recovery_clock){a->recovery_clock=true;a->recovery_at=a->idle_at=a->now_ms;}
    if(touch||down)a->idle_at=a->now_ms;
    if(!a->recovery_enabled||a->recovery_pending||touch||a->touching||a->stroke||a->doc.editing||a->page||a->confirm||a->toolbox||a->zoom_overlay||!a->directory[0]||!pd_dirty(&a->doc))return;
    if((uint32_t)(a->now_ms-a->idle_at)<2000||(uint32_t)(a->now_ms-a->recovery_at)<120000)return;
    a->recovery_at=a->now_ms;uint32_t crc=(uint32_t)crc32(0,(const Bytef*)a->doc.palette,a->doc.palette_count*4)^a->doc.palette_count;
    if(a->recovery_revision==a->doc.revision&&a->recovery_palette==crc&&a->recovery_frames==pd_frames_generation(&a->doc))return;
    char folder[PD_PATH],path[PD_PATH],error[64];if(!pd_join(folder,a->directory,".recovery")||(!pd_exists(folder)&&!pd_mkdir(folder))||!recovery_path(a,path,"session.pdd")){message(a,"Recovery unavailable - save manually");return;}
    if(pd_write_recovery(&a->doc,path,error)){a->recovery_revision=a->doc.revision;a->recovery_palette=crc;a->recovery_frames=pd_frames_generation(&a->doc);a->recovery_available=true;message(a,"Recovery snapshot saved - project still unsaved");}else message(a,error);
}
static void recovery_restore(PdApp *a){
    char path[PD_PATH],error[64];const char *names[]={"session.pdd","session.pdd.tmp","session.pdd.bak"};bool ok=false;
    for(unsigned i=0;i<3&&!ok;i++)if(recovery_path(a,path,names[i]))ok=pd_open_file(&a->doc,path,error);
    if(!ok){message(a,"No valid recovery - current art retained");return;}
    a->selection_valid=false;memset(a->selection,0,mask_bytes(a));frame_marks_clear(a);a->project[0]=0;a->project_native=false;a->doc.revision=++a->doc.serial;a->recovery_pending=false;a->recovery_revision=0;a->recovery_at=a->now_ms;
    if(a->doc.palette_count){a->swatch_count=a->doc.palette_count;memcpy(a->swatches,a->doc.palette,a->swatch_count*4);}else project_palette(a);
    a->swatch_selected=0;a->palette_scroll[0]=a->palette_scroll[1]=0;a->ox=a->oy=a->subx=a->suby=0;a->page=0;message(a,"Recovery restored - Save As to keep it");
}
static void recovery_discard(PdApp *a){char path[PD_PATH];const char *names[]={"session.pdd","session.pdd.bak","session.pdd.tmp"};bool ok=true;for(unsigned i=0;i<3;i++)if(recovery_path(a,path,names[i])&&pd_exists(path)&&remove(path))ok=false;recovery_scan(a);a->recovery_pending=a->recovery_available;a->recovery_revision=0;a->recovery_at=a->now_ms;message(a,ok?"Recovery cleared; drawing retained":"Could not clear recovery files");}
static void new_project(PdApp *a){
    if(a->new_layers>pd_capacity(a->new_width,a->new_height,a->doc.profile)){message(a,"Too many layers for this canvas size");return;}if(!pd_create(&a->doc,a->new_width,a->new_height,a->doc.profile)){message(a,"Not enough memory - current art retained");return;}a->selection_valid=false;memset(a->selection,0,mask_bytes(a));frame_marks_clear(a);a->project[0]=0;a->project_native=false;
    if(!a->new_current_palette){a->swatch_count=PALETTE_DEFAULT_COUNT;memcpy(a->swatches,palette,sizeof(palette));palette_load(a);}project_palette(a);
    if(a->new_background){uint32_t c=a->new_background==1?0xffffffff:a->background;for(unsigned y=0;y<a->doc.height;y++)for(unsigned x=0;x<a->doc.width;x++)pd_pixels(&a->doc.layer[0])[y*a->doc.stride+x]=c;snprintf(a->doc.names[0],64,"Background");}
    a->doc.enabled=(uint8_t)((1u<<a->new_layers)-1);a->swatch_selected=0;a->palette_scroll[0]=a->palette_scroll[1]=0;a->doc.revision=++a->doc.serial;a->zoom=1;while(a->zoom<32&&(int)a->doc.width*a->zoom*2<=a->width&&(int)a->doc.height*a->zoom*2<=canvas_bottom(a)-canvas_top(a))a->zoom*=2;a->ox=a->oy=a->subx=a->suby=0;a->page=0;a->recovery_revision=0;a->recovery_at=a->now_ms;message(a,"New project - not saved");
}
static void text_begin(PdApp *a){a->page=5;a->text_cursor=(unsigned)strlen(a->filename);a->text_mode=0;a->text_shift=0;a->text_repeat=false;if(a->palette_picker_active)a->picker_update=false;}
static void text_begin_hex(PdApp *a){
    uint32_t color=*palette_color_target(a);a->name_action=5;snprintf(a->filename,sizeof(a->filename),"%06X",(unsigned)(color&0xffffff));a->text_cursor=6;a->text_mode=3;a->text_repeat=false;a->numeric_replace=true;a->picker_update=false;a->page=5;a->redraw=true;
}
static void text_delete(PdApp *a){unsigned end=a->text_cursor;if(end){unsigned start=end-1;while(start&&((unsigned char)a->filename[start]&0xc0)==0x80)start--;memmove(a->filename+start,a->filename+end,strlen(a->filename+end)+1);a->text_cursor=start;}a->redraw=true;}
static int text_row_height(const PdApp *a){return ((int)a->height-112)/4;}
static const char *text_keys(unsigned mode,unsigned row){
    static const char *letters[]={"qwertyuiop","asdfghjkl","zxcvbnm"};
    static const char *symbols[]={"1234567890","_-().,!@+&","[]{}=#$%"};
    static const char *hex[]={"0123456789","ABCDEF",""};
    return mode==3?hex[row]:mode==2?symbols[row]:letters[row];
}
static unsigned text_start(PdApp *a){unsigned start=a->text_cursor>26?a->text_cursor-26:0;while(start&&((unsigned char)a->filename[start]&0xc0)==0x80)start--;return start;}
/* Text-entry keys (hex entry keeps its own layout). Rows 0-1 are characters; row 2 adds Shift and
 * Backspace (symbols: Backspace only); row 3 holds mode, dash, space, underscore and cursor keys.
 * Drawing, taps and hold-to-delete all use this table. */
enum { KEY_CHAR, KEY_SHIFT, KEY_BKSP, KEY_MODE, KEY_SPACE, KEY_LEFT, KEY_RIGHT };
typedef struct { int x,y,w,h; unsigned kind; char ch; } KeyRect;
static unsigned keyboard_keys(const PdApp *a,KeyRect *out){
    int w=(int)a->width,kw=(w-8)/10,x0=4+(w-8-kw*10)/2,rh=text_row_height(a),top=72,sw=kw*3/2;unsigned n=0;bool symbols=a->text_mode==2;
    for(unsigned row=0;row<3;row++){
        const char *chars=text_keys(a->text_mode,row);int count=(int)strlen(chars),y=top+(int)row*rh,x=x0;
        if(row<2)x=x0+(10-count)*kw/2;
        else if(!symbols){out[n++]=(KeyRect){x0,y,sw,rh,KEY_SHIFT,0};x=x0+sw;}
        for(int i=0;i<count;i++)out[n++]=(KeyRect){x+i*kw,y,kw,rh,KEY_CHAR,chars[i]};
        if(row==2){int bx=x+count*kw;out[n++]=(KeyRect){bx,y,x0+kw*10-bx,rh,KEY_BKSP,0};}
    }
    static const unsigned kinds[6]={KEY_MODE,KEY_CHAR,KEY_SPACE,KEY_CHAR,KEY_LEFT,KEY_RIGHT};static const char marks[6]={0,'-',' ','_',0,0};
    int widths[6]={sw,kw,kw*4,kw,kw*5/4,0},y=top+3*rh,x=x0;
    for(int i=0;i<6;i++){int kwid=i==5?x0+kw*10-x:widths[i];out[n++]=(KeyRect){x,y,kwid,rh,kinds[i],marks[i]};x+=kwid;}
    return n;
}
static char key_char(const PdApp *a,char c){return a->text_mode==0&&a->text_shift&&c>='a'&&c<='z'?(char)(c-'a'+'A'):c;}
static bool keyboard_delete_hit(const PdApp *a,int x,int y){
    if(a->name_action==5)return x<(int)a->width/3&&y>=(int)a->height-28;
    KeyRect k[40];unsigned n=keyboard_keys(a,k);for(unsigned i=0;i<n;i++)if(k[i].kind==KEY_BKSP&&x>=k[i].x&&x<k[i].x+k[i].w&&y>=k[i].y&&y<k[i].y+k[i].h)return true;return false;
}
static void text_insert(PdApp *a,char c){
    if(a->name_action==5){
        if(c>='a'&&c<='f')c=(char)(c-'a'+'A');if(!((c>='0'&&c<='9')||(c>='A'&&c<='F')))return;
        if(a->numeric_replace){a->filename[0]=0;a->text_cursor=0;a->numeric_replace=false;}
        size_t n=strlen(a->filename);if(n<6){memmove(a->filename+a->text_cursor+1,a->filename+a->text_cursor,n-a->text_cursor+1);a->filename[a->text_cursor++]=c;}
        return;
    }
    size_t n=strlen(a->filename);if(n<(a->name_action==6?31u:a->name_action==2?63u:55u)){memmove(a->filename+a->text_cursor+1,a->filename+a->text_cursor,n-a->text_cursor+1);a->filename[a->text_cursor++]=c;}
}

static void name_done(PdApp *a){
    if(a->name_action==5){
        char digits[8];const char *s=a->filename;if(*s=='#')s++;if(strlen(s)!=6){message(a,"HEX needs six digits");return;}
        snprintf(digits,sizeof(digits),"#%s",s);uint32_t color=0;if(!pd_color_parse(digits,&color)){message(a,"HEX needs six digits");return;}
        uint32_t *target=palette_color_target(a);*target=(*target&0xff000000u)|(color&0xffffffu);color_hsv(a);a->page=2;a->redraw=true;message(a,"HEX color applied");return;
    }
    size_t n=strlen(a->filename);if(!n){message(a,"Enter a name first");return;}
    if(a->name_action==2){pd_rename_layer(&a->doc,a->filename);a->page=1;message(a,"Layer name updated");return;}
    if(a->name_action==6){const PdTag *cur=pd_tag(&a->doc,(unsigned)a->tag_edit);if(cur){PdTag t=*cur;snprintf(t.name,sizeof(t.name),"%s",a->filename);pd_tag_set(&a->doc,(unsigned)a->tag_edit,&t);message(a,"Tag renamed");}a->page=1;a->tag_sheet=cur!=NULL;a->redraw=true;return;}
    if(n>55||a->filename[0]=='.'||a->filename[n-1]=='.'||a->filename[n-1]==' '||strpbrk(a->filename,"/\\:*?\"<>|")){message(a,"Use up to 55 bytes; no path characters");return;}
    if(a->name_action>=3){const char *base=strrchr(a->file_source,'/');base=base?base+1:a->file_source;const char *ext=strrchr(base,'.');char name[80];snprintf(name,sizeof(name),"%s%s",a->filename,ext?ext:"");if(!pd_join(a->pending,a->folder,name)){message(a,"Path too long");return;}
        if(pd_file_manage(a->file_source,a->pending,a->name_action==3?0:1,a->notice)){if(a->name_action==3)recent_project_change(a,a->file_source,a->pending);if(a->name_action==3&&!strcmp(a->project,a->file_source))snprintf(a->project,sizeof(a->project),"%s",a->pending);a->page=4;browse_refresh(a);message(a,a->name_action==3?"File renamed":"File duplicated and verified");}else a->redraw=true;return;
    }
    char name[80];snprintf(name,sizeof(name),"%s%s",a->filename,a->name_action?"":a->file_mode==1?".pdd":a->file_mode==2?".aseprite":a->file_mode==5?".gpl":a->file_mode==6?".hex":".png");
    if(!pd_join(a->pending,a->folder,name)){message(a,"Path too long");return;}
    if(a->name_action){if(pd_mkdir(a->pending)){a->page=4;browse_refresh(a);}else message(a,"Cannot create folder");}
    else if(pd_exists(a->pending)){a->confirm=5;message(a,"Replace existing file? Backup kept");}
    else write_pending(a,false);
}
void app_text_result(PdApp *a,const char *text,bool accepted){
    a->text_repeat=false;a->blocked=true;a->touching=false;
    if(!accepted){a->page=a->name_action==5?2:(a->name_action==2||a->name_action==6)?1:4;if(a->name_action==6)a->tag_sheet=true;}
    else if(strlen(text)>=sizeof(a->filename))message(a,"Name exceeds 63 bytes");
    else {snprintf(a->filename,sizeof(a->filename),"%s",text);a->text_cursor=(unsigned)strlen(text);name_done(a);}
    a->redraw=true;
}
static void browser_input(PdApp *a,int x,int y){
    a->blocked=true;
    if(a->page==5){
        
        int rh=text_row_height(a);
        if(y>=44&&y<66){unsigned at=text_start(a)+(unsigned)(x>8?(x-8)/6:0),n=(unsigned)strlen(a->filename);if(at>n)at=n;while(at&&((unsigned char)a->filename[at]&0xc0)==0x80)at--;a->text_cursor=at;}
        else if(a->name_action==5&&y>=72&&y<72+rh*3){unsigned row=(unsigned)(y-72)/rh;const char *keys=text_keys(a->text_mode,row);int kw=(a->width-8)/10,left=(a->width-(int)strlen(keys)*kw)/2;if(x>=left){unsigned k=(unsigned)(x-left)/kw;if(k<strlen(keys))text_insert(a,keys[k]);}}
        else if(a->name_action==5&&y>=72+rh*3&&y<72+rh*4){
            int button=x*3/a->width;
            if(button==0){a->filename[0]=0;a->text_cursor=0;a->numeric_replace=false;}
            else if(button==1&&a->text_cursor){a->text_cursor--;}
            else if(button==2&&a->text_cursor<strlen(a->filename)){a->text_cursor++;}
        }
        else if(a->name_action!=5&&y>=72&&y<72+rh*4){
            KeyRect k[40];unsigned n=keyboard_keys(a,k);
            for(unsigned i=0;i<n;i++){KeyRect *r=&k[i];if(x<r->x||x>=r->x+r->w||y<r->y||y>=r->y+r->h)continue;
                if(r->kind==KEY_CHAR){text_insert(a,key_char(a,r->ch));if(a->text_shift==1)a->text_shift=0;}
                else if(r->kind==KEY_SPACE)text_insert(a,' ');
                else if(r->kind==KEY_SHIFT)a->text_shift=(a->text_shift+1)%3;
                else if(r->kind==KEY_MODE)a->text_mode=a->text_mode==2?0:2;
                else if(r->kind==KEY_BKSP){text_delete(a);a->text_repeat=true;a->text_repeat_at=a->now_ms+400;}
                else if(r->kind==KEY_LEFT&&a->text_cursor){do{a->text_cursor--;}while(a->text_cursor&&((unsigned char)a->filename[a->text_cursor]&0xc0)==0x80);}
                else if(r->kind==KEY_RIGHT&&a->text_cursor<strlen(a->filename)){do{a->text_cursor++;}while(((unsigned char)a->filename[a->text_cursor]&0xc0)==0x80);}
                break;}
        }
        else if(y>=a->height-28){
            if(a->name_action==5){int button=x*3/a->width;if(button==0){text_delete(a);a->text_repeat=true;a->text_repeat_at=a->now_ms+400;}else if(button==1)a->page=2;else name_done(a);}
            else if(x<a->width/2){a->page=(a->name_action==2||a->name_action==6)?1:4;if(a->name_action==6)a->tag_sheet=true;}else name_done(a);
        }
        a->redraw=true;return;
    }
    BrowserLayout l=browser_layout(a);
    if(a->file_mode<=1&&x>=a->width-58&&x<a->width-4&&y>=25&&y<41){a->page=17;a->recent_project_selected=-1;a->preview_attempted=false;}
    else if(y>=44&&y<62){if(x<28){if(strlen(a->folder)>strlen(a->directory)){char *slash=strrchr(a->folder,'/');if(slash)*slash=0;browse_refresh(a);}else message(a,"Already at the app folder");}else if(x>=30)browser_open_filter(a);}
    else if(y>=l.list_top&&y<l.list_bottom){a->file_hold=file_index(a,(unsigned)(y-l.list_top+a->file_scroll)/(unsigned)l.row_h);a->file_start_y=y;a->file_start_scroll=a->file_scroll;a->file_scrolling=false;a->blocked=false;
    }else if(y>=l.footer_y){int fw=((int)a->width-20)/4,col=(x-4)/(fw+4);if(col<0)col=0;if(col>3)col=3;
        if(col==1){if(a->selected>=0&&!a->listing.entries[a->selected].directory&&pd_join(a->file_source,a->folder,a->listing.entries[a->selected].name))a->page=16;else message(a,"Select a file first");a->redraw=true;return;}
        int b=col==0?0:col==2?1:2;
        if(b==0){a->name_action=1;a->filename[0]=0;text_begin(a);}
        if(b==1)a->page=a->file_mode>=4?12:3;
        if(b==2){
            if(a->file_mode&&a->file_mode!=4){a->name_action=0;a->filename[0]=0;
                if(a->file_mode>=2&&a->project[0]){
                    const char *base=strrchr(a->project,'/');base=base?base+1:a->project;
                    const char *ext=strrchr(base,'.');size_t n=ext&&ext!=base?(size_t)(ext-base):strlen(base);
                    if(n<sizeof(a->filename)){memcpy(a->filename,base,n);a->filename[n]=0;}
                }
                text_begin(a);}
            else if(a->selected>=0){PdEntry *e=&a->listing.entries[a->selected];if(pd_join(a->pending,a->folder,e->name)){a->confirm=4;message(a,a->file_mode==4?"Replace current project palette?":"Open replaces current drawing");}}
            else message(a,"Select a project first");
        }
    }a->redraw=true;
}
void app_init(PdApp *a,int w,int h,const char *dir,const char *platform){
    app_dispose(a);memset(a,0,sizeof(*a));pd_init(&a->doc);masks_reserve(a,PD_MAX_PIXELS/32);a->width=w;a->height=h;a->zoom=1;a->lastx=-4097;
    a->onion_behind=a->onion_ahead=1;a->onion_opacity=35;a->onion_tint=true;
    a->file_hold=-1;a->layer_hold=-1;a->swatch_hold=-1;a->swatch_count=PALETTE_DEFAULT_COUNT;memcpy(a->swatches,palette,sizeof(palette));
    a->color=palette[0];a->background=0xffffffff;a->platform=platform;snprintf(a->directory,sizeof(a->directory),"%s",dir);
    palette_load(a);recent_projects_load(a);a->bindings=228;a->brush_size[0]=a->brush_size[1]=1;a->new_layers=1;a->new_width=a->new_height=128;a->recovery_enabled=true;brush_load(a);presets_load(a);
    unsigned char r0[16],r1[16];unsigned s0=settings_read(a,0,r0),s1=settings_read(a,1,r1);
    if(s0||s1){unsigned char *r=s1>s0?r1:r0;a->handed=r[8];a->zoom_side=r[9];a->bindings=!memcmp(r,"PDS2",4)?r[10]:228;}
    int loaded=dir[0]?pd_load(&a->doc,dir):-1;
    project_palette(a);a->doc.saved_palette_count=a->doc.palette_count;memcpy(a->doc.saved_palette,a->doc.palette,sizeof(a->doc.palette));
    message(a,loaded==1?"Saved project loaded":loaded==0?"New project - START saves":"Storage unavailable or save invalid");
    recovery_scan(a);if(a->recovery_available){a->recovery_pending=true;a->page=19;message(a,"Recovery available - restore or keep for later");}
}
/* Offline cache stage. Only bounded PNG derivatives, never account/approval claims. */
static bool gallery_png(const char *name){const char *ext=strrchr(name,'.');return ext&&strlen(ext)==4&&(ext[1]=='p'||ext[1]=='P')&&(ext[2]=='n'||ext[2]=='N')&&(ext[3]=='g'||ext[3]=='G')&&!ext[4];}
static void gallery_select(PdApp *a,unsigned index){
    a->selected=(int)index;a->preview_attempted=true;a->preview_palette_count=0;a->preview_ready=false;
    if(index>=a->listing.count)return;snprintf(a->preview_name,sizeof(a->preview_name),"%s",a->listing.entries[index].name);
    char path[PD_PATH];if(pd_join(path,a->folder,a->preview_name))a->preview_ready=pd_preview_file_profile(path,a->file_preview,1,a->preview_error);
    a->redraw=true;
}
static void gallery_page_load(PdApp *a){
    a->selected=-1;a->preview_attempted=false;a->preview_ready=false;memset(a->gallery_ready,0,sizeof(a->gallery_ready));
    unsigned start=a->gallery_page*6;a->gallery_count=a->listing.count>start?a->listing.count-start:0;if(a->gallery_count>6)a->gallery_count=6;
    if(!app_reserve_gallery(a)){a->gallery_count=0;message(a,"Not enough memory for thumbnails");}
    for(unsigned i=0;i<a->gallery_count;i++){gallery_select(a,start+i);a->gallery_ready[i]=a->preview_ready;if(a->preview_ready)for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++)a->gallery_thumbs[i][y*32+x]=a->file_preview[y*128+x*2];}
    if(a->gallery_count)gallery_select(a,start);a->redraw=true;
}
static void gallery_open(PdApp *a){
    a->page=28;a->gallery_page=0;a->community_mode=0;a->community_loaded=false;a->community_loading=false;a->file_mode=0;memset(&a->listing,0,sizeof(a->listing));a->folder[0]=0;
    if(!pd_join(a->folder,a->directory,"community")||!pd_list(a->folder,&a->listing,a->notice)){memset(&a->listing,0,sizeof(a->listing));message(a,"Add PNGs to PixelDoodle/community");}
    unsigned count=0;for(unsigned i=0;i<a->listing.count;i++)if(!a->listing.entries[i].directory&&gallery_png(a->listing.entries[i].name))a->listing.entries[count++]=a->listing.entries[i];a->listing.count=count;
    gallery_page_load(a);
}
static void community_set_mode(PdApp *a,unsigned mode){
    if(mode>3)return;pd_dropdown_close(&a->community_dropdown);a->community_mode=mode;a->gallery_page=0;a->selected=-1;a->preview_attempted=false;a->preview_ready=false;
    if(mode){a->community_loaded=false;a->community_loading=a->online_list!=NULL;snprintf(a->community_status,sizeof(a->community_status),a->online_list?"LOADING COMMUNITY PAGE":"ONLINE BROWSING UNAVAILABLE");if(a->online_list)a->online_list(a,mode,0);}
    else gallery_page_load(a);a->redraw=true;
}
static unsigned adjustment_visible_count(const PdApp *a){
    unsigned count=0;for(unsigned i=0;i<a->doc.capacity;i++)if((a->doc.enabled&(1u<<i))&&a->doc.layer[i].visible)count++;return count;
}
static bool adjustment_preview_build(PdApp *a){
    if(a->adjust_scope==0&&(!a->selection_valid||a->selection_layer!=a->doc.active||a->selection_revision!=a->doc.revision))a->adjust_preview_ready=false;
    else a->adjust_preview_ready=pd_adjust_preview(&a->doc,a->adjust_scope,a->adjust_scope==0?a->selection:NULL,a->adjust_hue,a->adjust_saturation,a->adjust_brightness);
    a->adjust_preview_stale=false;a->redraw=true;return a->adjust_preview_ready;
}
static void adjustment_open(PdApp *a){
    a->adjust_scope=a->selection_valid?a->selection_layer==a->doc.active&&a->selection_revision==a->doc.revision?0:1:1;
    a->adjust_hue=a->adjust_saturation=a->adjust_brightness=0;a->adjust_drag=0;a->adjust_preview_stale=false;a->page=31;adjustment_preview_build(a);a->redraw=true;
}
static int adjustment_scope_y(int height){return height>=220?40:32;}
static int adjustment_slider_y(int height,unsigned row){int first=height>=220?80:60,pitch=height>=220?42:35;return first+(int)row*pitch;}
static int *adjustment_value(PdApp *a,unsigned row){return row==0?&a->adjust_hue:row==1?&a->adjust_saturation:&a->adjust_brightness;}
static void adjustment_set_value(PdApp *a,unsigned row,int x){
    int right=a->width-88;if(right<9)right=9;if(x<8)x=8;if(x>right)x=right;int value=(int)row==0?(x-8)*360/(right-8)-180:(x-8)*200/(right-8)-100;
    *adjustment_value(a,row)=value;
}
static void adjustment_apply(PdApp *a){
    if(!a->adjust_hue&&!a->adjust_saturation&&!a->adjust_brightness){message(a,"Move a slider before applying");return;}
    if(a->adjust_scope==0&&(!a->selection_valid||a->selection_layer!=a->doc.active||a->selection_revision!=a->doc.revision)){message(a,"Selection is no longer available");return;}
    if(a->adjust_scope<2&&(!a->doc.layer[a->doc.active].visible||(a->doc.layer[a->doc.active].flags&PD_LAYER_LOCK))){message(a,a->doc.layer[a->doc.active].visible?"Unlock current layer first":"Current layer is hidden");return;}
    if(a->adjust_scope==2){
        unsigned visible=0;for(unsigned i=0;i<a->doc.capacity;i++)if((a->doc.enabled&(1u<<i))&&a->doc.layer[i].visible){
            visible++;if(a->doc.layer[i].flags&PD_LAYER_LOCK){message(a,"Unlock all visible layers first");return;}
        }
        if(!visible){message(a,"No visible layers to adjust");return;}
    }
    if(!pd_adjust_layers(&a->doc,a->adjust_scope,a->adjust_scope==0?a->selection:NULL,a->adjust_hue,a->adjust_saturation,a->adjust_brightness)){
        message(a,"Adjustment too large or colors unchanged");return;
    }
    if(a->adjust_scope==0)a->selection_revision=a->doc.revision;
    a->adjust_preview_ready=a->adjust_preview_stale=false;
    a->page=1;message(a,a->adjust_scope==0?"Selection colors adjusted":a->adjust_scope==1?"Layer colors adjusted":"Visible layer colors adjusted");
}
static void adjustment_input(PdApp *a,int x,int y,bool pressed){
    if(pressed){
        int scope_y=adjustment_scope_y(a->height);
        if(y>=a->height-28){if(x<a->width/2)adjustment_apply(a);else{a->adjust_preview_ready=a->adjust_preview_stale=false;a->page=1;message(a,"Color adjustment cancelled");}a->adjust_drag=0;}
        else if(y>=scope_y&&y<scope_y+20){unsigned scope=(unsigned)(x*3/a->width);if(scope!=a->adjust_scope){a->adjust_scope=scope;a->adjust_preview_stale=true;}a->adjust_drag=0;}
        else {
            for(unsigned row=0;row<3;row++){
                int sy=adjustment_slider_y(a->height,row);if(y<sy||y>=sy+28)continue;
                int step=5,*value=adjustment_value(a,row),old=*value;
                if(x>=a->width-42)*value+=step;
                else if(x>=a->width-84)*value-=step;
                else {a->adjust_drag=row+1;adjustment_set_value(a,row,x);}
                if(*value!=old)a->adjust_preview_stale=true;
                break;
            }
        }
        a->blocked=true;a->redraw=true;
    }
    if(a->adjust_drag){int old=*adjustment_value(a,a->adjust_drag-1);adjustment_set_value(a,a->adjust_drag-1,x);if(old!=*adjustment_value(a,a->adjust_drag-1))a->adjust_preview_stale=true;a->redraw=true;}
    if(a->adjust_hue< -180)a->adjust_hue=-180;if(a->adjust_hue>180)a->adjust_hue=180;
    if(a->adjust_saturation< -100)a->adjust_saturation=-100;if(a->adjust_saturation>100)a->adjust_saturation=100;
    if(a->adjust_brightness< -100)a->adjust_brightness=-100;if(a->adjust_brightness>100)a->adjust_brightness=100;
}
/* Menu rows: New Project | Frames; Save | Save As | Export; Open; Settings | Recovery; Exit | Send.
 * Drawing and touch both use this table. Adjust lives in the Layers footer. */
typedef struct { int x,y,w,h,action; const char *label; } MenuButton;
static unsigned menu_buttons(const PdApp *a,MenuButton *out){
    static const int actions[5][3]={{4,-1,-1},{0,1,3},{2,-1,-1},{6,7,-1},{5,9,-1}};
    static const char *labels[5][3]={{"NEW PROJECT",NULL,NULL},{"SAVE","SAVE AS","EXPORT"},{"OPEN / FILE BROWSER",NULL,NULL},{"SETTINGS","RECOVERY",NULL},{"EXIT","SEND SPRITE",NULL}};
    bool big=a->height>192;int bh=big?28:22,gap=big?6:5,y=big?32:30,full=(int)a->width-8;unsigned n=0;
    for(int r=0;r<5;r++){
        int cols=0;while(cols<3&&labels[r][cols])cols++;
#ifndef PD_PRIVATE_BUILD
        if(r==4)cols=1;
#endif
        int bw=(full-(cols-1)*4)/cols;
        for(int c=0;c<cols;c++){MenuButton *b=&out[n++];b->x=4+c*(bw+4);b->y=y;b->w=c==cols-1?full-c*(bw+4):bw;b->h=bh;b->action=actions[r][c];b->label=labels[r][c];}
        y+=bh+gap;
    }
    return n;
}
#ifdef PD_TEST
bool app_test_menu_point(const PdApp *a,int action,int *x,int *y){MenuButton b[12];unsigned n=menu_buttons(a,b);for(unsigned i=0;i<n;i++)if(b[i].action==action){*x=b[i].x+b[i].w/2;*y=b[i].y+b[i].h/2;return true;}return false;}
#endif
static void action(PdApp *a,int index){
    if(a->page==1){
        if(index<4){unsigned row=0;for(unsigned i=0;i<4;i++)if(a->doc.enabled&(1u<<i)){if(row++==(unsigned)index){a->doc.active=i;message(a,"Selected layer preview");break;}}}
        else if(index==4){pd_begin(&a->doc);a->doc.layer[a->doc.active].visible^=1;pd_commit(&a->doc);}
        else if(index==5){pd_begin(&a->doc);unsigned v=a->doc.layer[a->doc.active].opacity;a->doc.layer[a->doc.active].opacity=(uint8_t)(v==255?192:v==192?128:v==128?64:255);pd_commit(&a->doc);}
    }else if(a->page==3){
        if(index==0)save(a);
        if(index==1)browse(a,1);
        if(index==2)browse(a,0);
        if(index==3)a->page=6;
        if(index==4){a->page=18;a->new_background=0;a->new_layers=1;a->new_current_palette=false;}
        if(index==7)a->page=19;
#ifdef PD_PRIVATE_BUILD
        if(index==9)a->page=30;
#endif
        if(index==5){a->confirm=3;message(a,"Exit - save first if needed");}
        if(index==6){a->page=8;a->draft_handed=a->handed;a->draft_zoom_side=a->zoom_side;a->draft_bindings=a->bindings;}
    }
    a->redraw=true;
}
static int layer_at(PdApp *a,int row){int at=0;for(int k=PD_LAYERS-1;k>=0;k--){unsigned i=a->doc.order[k];if(a->doc.enabled&(1u<<i)){if(at++==row)return (int)i;}}return -1;}
static unsigned layer_count(PdApp *a){unsigned n=0;for(unsigned i=0;i<PD_LAYERS;i++)if(a->doc.enabled&(1u<<i))n++;return n;}
static int layer_list_top(void){return 42;}
static int layer_row_height(void){return 20;}
static int layer_footer_y(const PdApp *a){return a->height-30;}
static int layer_options_width(const PdApp *a){return a->width<=256?192:240;}
static int layer_options_x(const PdApp *a){return (a->width-layer_options_width(a))/2;}
static int layer_options_inner_x(const PdApp *a){return layer_options_x(a)+8;}
static int layer_options_inner_w(const PdApp *a){return layer_options_width(a)-16;}
static int tool_rail_width(const PdApp *a){return a->width<=256?96:116;}
static int tool_row_height(const PdApp *a){return (a->height-32)/9;}
static int tool_panel_x(const PdApp *a){return tool_rail_width(a)+8;}
static int tool_panel_w(const PdApp *a){return a->width-tool_panel_x(a)-8;}
static int advanced_sheet_x(const PdApp *a){int sw=a->width<=256?224:288;return (a->width-sw)/2;}
static int advanced_sheet_w(const PdApp *a){return a->width<=256?224:288;}
static void layer_clamp(PdApp *a){int max=(int)layer_count(a)*layer_row_height()-(layer_footer_y(a)-layer_list_top());if(max<0)max=0;if(a->layer_scroll<0)a->layer_scroll=0;if(a->layer_scroll>max)a->layer_scroll=max;}
/* Layers + Timeline screen, after the animation mockup's tlLayout/frameLayout/onionLayout.
 * One table each drives drawing, taps and the test hooks. */
typedef struct { int x,y,w,h; } Box;
static bool inside(Box b,int x,int y){return x>=b.x&&x<b.x+b.w&&y>=b.y&&y<b.y+b.h;}
typedef struct { Box tabs[2],play,onion,foot[4]; int name_w,cw,row_h,gx,tag_y,num_y,rows_y,rows,cols,meter_y; } Timeline;
static Timeline timeline_layout(const PdApp *a){
    Timeline t;int w=a->width,h=a->height;bool big=w>256;int fh=big?24:20,fy=h-fh-4,bw=(w-20)/4;
    t.tabs[0]=(Box){4,25,56,15};t.tabs[1]=(Box){62,25,62,15};t.onion=(Box){w-24,25,20,15};t.play=(Box){w-48,25,20,15};
    t.name_w=big?70:58;t.cw=big?16:14;t.row_h=big?20:16;t.gx=4+t.name_w+2;t.tag_y=44;t.num_y=55;t.rows_y=65;
    for(int i=0;i<4;i++)t.foot[i]=(Box){4+i*(bw+4),fy,i==3?w-8-3*(bw+4):bw,fh};
    t.meter_y=fy-18;t.rows=(t.meter_y-3-t.rows_y)/t.row_h;t.cols=(w-t.gx-10)/t.cw;return t;
}
static int timeline_slot(const PdApp *a,unsigned row){unsigned seen=0;for(unsigned i=0;i<PD_LAYERS;i++){unsigned slot=a->doc.order[PD_LAYERS-1-i];if(a->doc.enabled&(1u<<slot)){if(seen++==row)return (int)slot;}}return -1;}
static void timeline_clamp(PdApp *a){
    Timeline t=timeline_layout(a);unsigned count=pd_frame_count(&a->doc),layers=layer_count(a);
    if(a->timeline_first+(unsigned)t.cols>count)a->timeline_first=count>(unsigned)t.cols?count-(unsigned)t.cols:0;
    if(a->timeline_layer_first+(unsigned)t.rows>layers)a->timeline_layer_first=layers>(unsigned)t.rows?layers-(unsigned)t.rows:0;
}
/* Keeps the current frame's column on screen. */
static void timeline_follow(PdApp *a){
    Timeline t=timeline_layout(a);unsigned f=pd_frame_index(&a->doc),cols=(unsigned)t.cols;
    if(f<a->timeline_first)a->timeline_first=f;else if(f>=a->timeline_first+cols)a->timeline_first=f-cols+1;timeline_clamp(a);
}
static void frame_changed(PdApp *a){clamp(a);a->selection_valid=false;memset(a->selection,0,mask_bytes(a));a->overview_pending=false;a->partial=false;timeline_follow(a);frame_marks_clamp(a);}
/* Centred sheets: frame sheet and onion skin settings share the box. */
static Box sheet_box(const PdApp *a){int sw=a->width>256?260:216,sh=a->width>256?184:150;return (Box){(a->width-sw)/2,30,sw,sh};}
typedef struct { Box close,minus,plus,presets[4],scope[2],tag,move[2]; int ix,iw,dur_y; } FrameSheet;
static FrameSheet frame_sheet_layout(const PdApp *a){
    FrameSheet s;Box b=sheet_box(a);int y=b.y,half;s.ix=b.x+8;s.iw=b.w-16;half=(s.iw-4)/2;s.close=(Box){b.x+b.w-18,y,18,18};
    s.dur_y=y+22;s.minus=(Box){s.ix+s.iw-96,s.dur_y,22,20};s.plus=(Box){s.ix+s.iw-22,s.dur_y,22,20};
    for(int i=0;i<4;i++)s.presets[i]=(Box){s.ix+i*((s.iw+4)/4),y+46,(s.iw+4)/4-4,18};
    s.scope[0]=(Box){s.ix,y+70,half,18};s.scope[1]=(Box){s.ix+half+4,y+70,s.iw-half-4,18};s.tag=(Box){s.ix,y+94,s.iw,20};
    s.move[0]=(Box){s.ix,y+118,half,20};s.move[1]=(Box){s.ix+half+4,y+118,s.iw-half-4,20};return s;
}
static const unsigned frame_presets[4]={50,80,100,150};
static void frame_duration_set(PdApp *a,unsigned ms){
    bool ok=a->frame_apply_all?pd_frame_set_all_durations(&a->doc,ms):pd_frame_set_duration(&a->doc,ms);
    if(!ok)message(a,"Not enough frame memory");else if(a->frame_apply_all)message(a,"Duration set for all frames");
}
/* Tag sheet: name, range, direction, colour, delete. Edits apply at once and are saved with the project. */
static const uint32_t tag_colors[6]={0xc3a6ff,0x5aa9ff,0x8fdc8a,0xf2c46b,0xff6b6b,0xff9ec7};
static const char *const tag_directions[4]={"FORWARD","REVERSE","PING-PONG","PONG-PING"};
typedef struct { Box close,name,from_minus,from_plus,to_minus,to_plus,direction,color,remove,done; int ix,iw,from_y,to_y; } TagSheet;
static TagSheet tag_sheet_layout(const PdApp *a){
    TagSheet s;Box b=sheet_box(a);int y=b.y,half;s.ix=b.x+8;s.iw=b.w-16;half=(s.iw-4)/2;int bx=s.ix+s.iw-96;s.close=(Box){b.x+b.w-18,y,18,18};
    s.name=(Box){s.ix,y+20,s.iw,20};s.from_y=y+46;s.to_y=y+70;s.from_minus=(Box){bx,s.from_y,22,20};s.from_plus=(Box){bx+74,s.from_y,22,20};s.to_minus=(Box){bx,s.to_y,22,20};s.to_plus=(Box){bx+74,s.to_y,22,20};
    s.direction=(Box){s.ix,y+96,half,20};s.color=(Box){s.ix+half+4,y+96,s.iw-half-4,20};s.remove=(Box){s.ix,y+122,half,20};s.done=(Box){s.ix+half+4,y+122,s.iw-half-4,20};return s;
}
/* Opens the innermost tag on frame f, creating a one-frame tag there when none exists. */
static void tag_open(PdApp *a,unsigned f){
    int t=pd_tag_at(&a->doc,f);
    if(t<0){PdTag tag;memset(&tag,0,sizeof(tag));unsigned n=pd_tag_count(&a->doc);snprintf(tag.name,sizeof(tag.name),"Tag %u",n+1);tag.from=tag.to=f;tag.color=tag_colors[n%6];
        t=pd_tag_add(&a->doc,&tag);if(t<0){message(a,"Tag limit reached or not enough memory");return;}message(a,"Tag added - set its frames");}
    a->tag_edit=t;a->tag_sheet=true;a->frame_sheet=false;
}
static void tag_sheet_input(PdApp *a,int x,int y){
    TagSheet s=tag_sheet_layout(a);const PdTag *current=pd_tag(&a->doc,(unsigned)a->tag_edit);if(!current){a->tag_sheet=false;return;}
    PdTag t=*current;unsigned last=pd_frame_count(&a->doc)-1;
    if(inside(s.close,x,y)||inside(s.done,x,y)||!inside(sheet_box(a),x,y)){a->tag_sheet=false;return;}
    if(inside(s.name,x,y)){a->name_action=6;snprintf(a->filename,sizeof(a->filename),"%s",t.name);text_begin(a);return;}
    if(inside(s.remove,x,y)){pd_tag_delete(&a->doc,(unsigned)a->tag_edit);a->tag_sheet=false;message(a,"Tag deleted");return;}
    if(inside(s.from_minus,x,y)&&t.from)t.from--;else if(inside(s.from_plus,x,y)&&t.from<t.to)t.from++;
    else if(inside(s.to_minus,x,y)&&t.to>t.from)t.to--;else if(inside(s.to_plus,x,y)&&t.to<last)t.to++;
    else if(inside(s.direction,x,y))t.direction=(t.direction+1)&3;
    else if(inside(s.color,x,y)){unsigned i=0;while(i<6&&tag_colors[i]!=t.color)i++;t.color=tag_colors[(i+1)%6];}
    else return;
    pd_tag_set(&a->doc,(unsigned)a->tag_edit,&t);
}
/* Cel sheet: opened by tapping the selected cel again. */
typedef struct { Box close,link,unlink,new_linked; int ix,iw,info_y; } CelSheet;
static CelSheet cel_sheet_layout(const PdApp *a){
    CelSheet s;Box b=sheet_box(a);int y=b.y;s.ix=b.x+8;s.iw=b.w-16;s.close=(Box){b.x+b.w-18,y,18,18};s.info_y=y+20;
    s.link=(Box){s.ix,y+46,s.iw,20};s.unlink=(Box){s.ix,y+72,s.iw,20};s.new_linked=(Box){s.ix,y+98,s.iw,20};return s;
}
static void cel_sheet_input(PdApp *a,int x,int y){
    CelSheet s=cel_sheet_layout(a);unsigned layer=a->doc.active;
    if(inside(s.close,x,y)||!inside(sheet_box(a),x,y)){a->cel_sheet=false;return;}
    if(inside(s.link,x,y)){bool cleared=false;if(!pd_frame_index(&a->doc))message(a,"The first frame has no previous cel");
        else if(pd_cel_link_previous(&a->doc,layer,&cleared)){frame_changed(a);message(a,cleared?"Linked - cel took the previous image; undo cleared":"Linked with the previous cel");}
        else message(a,pd_cel_link(&a->doc,pd_frame_index(&a->doc),layer)?"Already linked with the previous cel":"Not enough frame memory; art retained");}
    else if(inside(s.unlink,x,y))message(a,pd_cel_unlink(&a->doc,layer)?"Cel unlinked - edits stay on this frame":"This cel is not linked");
    else if(inside(s.new_linked,x,y)){if(pd_frame_add_linked(&a->doc)){frame_marks_clear(a);frame_changed(a);a->cel_sheet=false;message(a,"Linked frame added - edits reach both");}else message(a,"Frame limit or not enough frame memory");}
}
static void frame_sheet_input(PdApp *a,int x,int y){
    FrameSheet s=frame_sheet_layout(a);unsigned ms=pd_frame_duration(&a->doc),f=pd_frame_index(&a->doc),count=pd_frame_count(&a->doc);
    if(inside(s.close,x,y)||!inside(sheet_box(a),x,y)){a->frame_sheet=false;return;}
    if(inside(s.minus,x,y)){if(ms>10)frame_duration_set(a,ms-10);return;}
    if(inside(s.plus,x,y)){if(ms<60000)frame_duration_set(a,ms+10);return;}
    for(int i=0;i<4;i++)if(inside(s.presets[i],x,y)){frame_duration_set(a,frame_presets[i]);return;}
    for(int i=0;i<2;i++)if(inside(s.scope[i],x,y)){a->frame_apply_all=i==1;return;}
    if(inside(s.tag,x,y)){tag_open(a,f);return;}
    for(int i=0;i<2;i++)if(inside(s.move[i],x,y)){unsigned target=i?f+1:f?f-1:f;
        if(target<count&&target!=f&&pd_frame_move(&a->doc,target)){frame_marks_clear(a);timeline_follow(a);message(a,"Frame moved; paint undo retained");}else message(a,"Frame is already at that end");return;}
}
typedef struct { Box toggle,behind_minus,behind_plus,ahead_minus,ahead_plus,slider,tint,close; int ix,iw,behind_y,ahead_y; } OnionSheet;
static OnionSheet onion_sheet_layout(const PdApp *a){
    OnionSheet s;Box b=sheet_box(a);int y=b.y;s.ix=b.x+8;s.iw=b.w-16;int bx=s.ix+s.iw-96;
    s.toggle=(Box){s.ix,y+20,s.iw,20};s.behind_y=y+46;s.ahead_y=y+70;
    s.behind_minus=(Box){bx,s.behind_y,22,20};s.behind_plus=(Box){bx+74,s.behind_y,22,20};s.ahead_minus=(Box){bx,s.ahead_y,22,20};s.ahead_plus=(Box){bx+74,s.ahead_y,22,20};
    s.slider=(Box){s.ix+56,y+98,s.iw-60,14};s.tint=(Box){s.ix,y+120,s.iw,20};s.close=(Box){b.x+b.w-18,y,18,18};return s;
}
static void onion_set(PdApp *a,bool on){a->onion_enabled=on;a->onion_key=0;if(!on){free(a->onion);a->onion=NULL;a->onion_plane=a->onion_bytes=0;}}
/* 10-90 percent in steps of 5; the knob follows the stylus. */
static void onion_opacity_from(PdApp *a,int x){OnionSheet s=onion_sheet_layout(a);int px=x-s.slider.x,range=s.slider.w-4;if(px<0)px=0;if(px>range)px=range;unsigned v=10u+(unsigned)((px*80+range/2)/range);a->onion_opacity=(v+2)/5*5;}
static void onion_sheet_input(PdApp *a,int x,int y,bool touch,bool pressed){
    OnionSheet s=onion_sheet_layout(a);
    if(!touch){a->onion_drag=false;return;}
    if(!pressed){if(a->onion_drag){onion_opacity_from(a,x);a->redraw=true;}return;}
    a->redraw=true;
    if(y>=s.slider.y-4&&y<s.slider.y+s.slider.h+4&&x>=s.slider.x-4&&x<s.slider.x+s.slider.w+4){a->onion_drag=true;onion_opacity_from(a,x);return;}
    a->blocked=true;
    if(inside(s.close,x,y)||!inside(sheet_box(a),x,y))a->onion_sheet=false;
    else if(inside(s.toggle,x,y))onion_set(a,!a->onion_enabled);
    else if(inside(s.behind_minus,x,y)){if(a->onion_behind>1)a->onion_behind--;}
    else if(inside(s.behind_plus,x,y)){if(a->onion_behind<3)a->onion_behind++;}
    else if(inside(s.ahead_minus,x,y)){if(a->onion_ahead>1)a->onion_ahead--;}
    else if(inside(s.ahead_plus,x,y)){if(a->onion_ahead<3)a->onion_ahead++;}
    else if(inside(s.tint,x,y))a->onion_tint=!a->onion_tint;
}
static bool timeline_cel_at(const PdApp *a,Timeline t,int x,int y,unsigned *frame,unsigned *layer,unsigned *row){
    if(x<t.gx||y<t.rows_y)return false;int col=(x-t.gx)/t.cw,r=(y-t.rows_y)/t.row_h;
    if(col<0||col>=t.cols||r<0||r>=t.rows)return false;unsigned f=a->timeline_first+(unsigned)col;
    if(f>=pd_frame_count(&a->doc))return false;int slot=timeline_slot(a,a->timeline_layer_first+(unsigned)r);if(slot<0)return false;
    *frame=f;*layer=(unsigned)slot;*row=(unsigned)r;return true;
}
static void timeline_input(PdApp *a,int x,int y,bool touch,bool pressed,unsigned held){
    Timeline t=timeline_layout(a);timeline_clamp(a);unsigned count=pd_frame_count(&a->doc),current=pd_frame_index(&a->doc),layers=layer_count(a);
    if(a->frame_delete){if(pressed){if(y>=120&&y<148){if(x<a->width/2&&pd_frame_delete(&a->doc)){frame_marks_clear(a);frame_changed(a);message(a,"Frame deleted; undo history cleared");}a->frame_delete=false;}a->blocked=true;a->redraw=true;}return;}
    if(a->frame_sheet){if(pressed){frame_sheet_input(a,x,y);a->blocked=true;a->redraw=true;}return;}
    if(a->tag_sheet){if(pressed){tag_sheet_input(a,x,y);a->blocked=true;a->redraw=true;}return;}
    if(a->cel_sheet){if(pressed){cel_sheet_input(a,x,y);a->blocked=true;a->redraw=true;}return;}
    if(a->onion_sheet||a->onion_drag){onion_sheet_input(a,x,y,touch,pressed);return;}
    if(pressed){
        a->timeline_press=0;a->timeline_scrolling=a->timeline_reorder=false;a->cel_drag=a->cel_drag_valid=false;bool ok=true,changed=false;
        if(inside(t.tabs[0],x,y))a->layers_tab=0;
        else if(inside(t.tabs[1],x,y)){}
        else if(inside(t.play,x,y)){if(pd_playback_start(&a->doc,&a->playback,a->now_ms)){a->overview_pending=false;a->selection_valid=false;}else message(a,count<2?"Add a second frame to play":"Not enough memory to start playback");}
        else if(inside(t.onion,x,y))a->onion_sheet=true;
        else if(inside(t.foot[0],x,y)||inside(t.foot[1],x,y)){ok=pd_frame_add(&a->doc,inside(t.foot[1],x,y));changed=ok;if(ok)frame_marks_clear(a);}
        else if(inside(t.foot[2],x,y)){if(count>1)a->frame_delete=true;else message(a,"Keep at least one frame");}
        else if(inside(t.foot[3],x,y))a->frame_sheet=true;
        else if(y>=t.tag_y&&y<t.num_y-1&&x>=t.gx){unsigned f=a->timeline_first+(unsigned)(x-t.gx)/(unsigned)t.cw;if(f<count&&pd_tag_at(&a->doc,f)>=0)tag_open(a,f);}
        else if(y>=t.num_y&&y<t.rows_y+t.rows*t.row_h){
            unsigned row=y>=t.rows_y?(unsigned)(y-t.rows_y)/(unsigned)t.row_h:0;
            a->timeline_press=y<t.rows_y?1:x>=t.gx+t.cols*t.cw?4:x>=t.gx?2:3;
            if(a->timeline_press==1&&x<t.gx)a->timeline_press=0;
            a->timeline_press_frame=a->timeline_first+(x>=t.gx?(unsigned)(x-t.gx)/(unsigned)t.cw:0);a->timeline_press_row=row;
            if((a->timeline_press==1||a->timeline_press==2)&&a->timeline_press_frame>=count)a->timeline_press=0;
            if(a->timeline_press==1&&(held&PD_SHOULDER_L)){frame_mark_toggle(a,a->timeline_press_frame);a->timeline_press=0;message(a,a->frame_mark_count?"Frame marked - hold a marked number to move":"Frame unmarked");a->blocked=true;a->redraw=true;return;}
            if(a->timeline_press==1&&a->frame_mark_count&&!a->frame_mark[a->timeline_press_frame])frame_marks_clear(a);
            a->timeline_start_x=x;a->timeline_start_y=y;a->timeline_press_ms=a->now_ms;a->timeline_target=a->timeline_press_frame;
            a->timeline_start_first=a->timeline_first;a->timeline_start_layer_first=a->timeline_layer_first;
            if(a->timeline_press)return;
        }
        if(changed)frame_changed(a);if(!ok)message(a,"Not enough frame memory; art retained");
        a->blocked=true;a->redraw=true;return;
    }
    unsigned kind=a->timeline_press;if(!kind)return;
    if(touch){
        int dx=x-a->timeline_start_x,dy=y-a->timeline_start_y;
        if(a->cel_drag){unsigned f=0,l=0,r=0;if(timeline_cel_at(a,t,x,y,&f,&l,&r)){a->cel_drag_valid=true;a->cel_dst_frame=f;a->cel_dst_layer=l;a->cel_dst_row=r;}else a->cel_drag_valid=false;a->redraw=true;return;}
        if(!a->timeline_scrolling&&!a->timeline_reorder){
            if(kind==1&&abs(dx)<=5&&abs(dy)<=5&&a->now_ms-a->timeline_press_ms>=300){a->timeline_reorder=true;message(a,a->frame_mark_count>1?"Drag marked frames to a new position":"Drag to a new position");}
            else if(kind==2&&abs(dx)<=5&&abs(dy)<=5&&a->now_ms-a->timeline_press_ms>=400){
                int slot=timeline_slot(a,a->timeline_layer_first+a->timeline_press_row);
                if(slot>=0&&a->timeline_press_frame<count){a->cel_drag=true;a->cel_drag_valid=true;a->cel_src_frame=a->timeline_press_frame;a->cel_src_layer=(unsigned)slot;a->cel_src_row=a->timeline_press_row;a->cel_dst_frame=a->cel_src_frame;a->cel_dst_layer=a->cel_src_layer;a->cel_dst_row=a->cel_src_row;message(a,"Drag cel to another layer or frame");a->redraw=true;return;}
            }
            else if(abs(dx)>5||abs(dy)>5)a->timeline_scrolling=true;
        }
        if(a->timeline_reorder){
            unsigned selected=a->frame_mark_count?a->frame_mark_count:1,maxstart=count>selected?count-selected:0;
            int col=(x-t.gx)/t.cw;if(x<t.gx)col=0;if(col>=t.cols)col=t.cols-1;int target=(int)a->timeline_first+col;
            if(target<0)target=0;if(target>(int)maxstart)target=(int)maxstart;
            a->timeline_target=(unsigned)target;a->redraw=true;}
        else if(a->timeline_scrolling){
            if(kind!=3&&kind!=4){int f=(int)a->timeline_start_first-dx/t.cw;a->timeline_first=f<0?0:(unsigned)f;}
            if(kind>=2){int r=(int)a->timeline_start_layer_first-dy/t.row_h;a->timeline_layer_first=r<0?0:(unsigned)r;}
            timeline_clamp(a);a->redraw=true;}
        return;
    }
    if(a->cel_drag){
        unsigned sf=a->cel_src_frame,sl=a->cel_src_layer,df=a->cel_dst_frame,dl=a->cel_dst_layer;bool valid=a->cel_drag_valid;
        a->cel_drag=false;a->cel_drag_valid=false;a->timeline_press=0;a->redraw=true;
        if(!valid||(sf==df&&sl==dl)){message(a,"Cel move cancelled");return;}
        bool linked=pd_cel_link(&a->doc,sf,sl)||pd_cel_link(&a->doc,df,dl);
        if(pd_cel_move(&a->doc,sf,sl,df,dl)){a->doc.active=dl;frame_changed(a);frame_marks_clear(a);message(a,linked?"Cel moved; links and undo cleared":"Cel moved; undo cleared");}
        else message(a,"Cannot move cel; art retained");
        return;
    }
    a->timeline_press=0;a->redraw=true;bool ok=true,changed=false;unsigned f=a->timeline_press_frame;
    if(a->timeline_reorder){
        uint8_t marks[PD_FRAMES];unsigned selected=0;
        if(a->frame_mark_count){memcpy(marks,a->frame_mark,sizeof(marks));selected=a->frame_mark_count;}
        else {memset(marks,0,sizeof(marks));marks[f]=1;selected=1;}
        uint16_t map[PD_FRAMES];
        if(selected>=count)message(a,"Leave at least one frame unmarked");
        else if(pd_frames_move(&a->doc,marks,a->timeline_target,map)){
            if(a->frame_mark_count){uint8_t old[PD_FRAMES];memcpy(old,a->frame_mark,sizeof(old));memset(a->frame_mark,0,sizeof(a->frame_mark));for(unsigned i=0;i<count;i++)if(old[i])a->frame_mark[map[i]]=1;frame_marks_recount(a);}
            message(a,selected>1?"Frames moved; paint undo retained":"Frame moved; paint undo retained");
            timeline_follow(a);a->partial=false;a->overview_pending=false;a->redraw=true;
        }else message(a,selected>1?"Marked frames are already there":"Frame is already there");
        a->timeline_scrolling=a->timeline_reorder=false;return;
    }
    if(!a->timeline_scrolling){
        int slot=timeline_slot(a,a->timeline_layer_first+a->timeline_press_row);
        if(kind==1){if(f==current)a->frame_sheet=true;else{ok=pd_frame_switch(&a->doc,f);changed=ok;}}
        else if(kind==2&&slot>=0){if(f==current&&(unsigned)slot==a->doc.active)a->cel_sheet=true;else{if(f!=current){ok=pd_frame_switch(&a->doc,f);changed=ok;}if(ok)a->doc.active=(unsigned)slot;}}
        else if(kind==3&&slot>=0)a->doc.active=(unsigned)slot;
        else if(kind==4){if(a->timeline_press_row<(unsigned)t.rows/2){if(a->timeline_layer_first)a->timeline_layer_first--;}else if(a->timeline_layer_first+(unsigned)t.rows<layers)a->timeline_layer_first++;}
    }
    a->timeline_scrolling=a->timeline_reorder=false;
    if(changed)frame_changed(a);if(!ok)message(a,"Not enough frame memory; art retained");
}
/* Canvas frame strip, after the mockup's stripLayout: prev, n/m, next, play, onion, then one tick per frame. */
typedef struct { Box items[5]; int y,h,tx,tw; } Strip;
static Strip strip_layout(const PdApp *a){
    Strip s;int b=a->width>256?22:18,x[5]={2,4+b,46+b,50+2*b,54+3*b},bw[5]={b,40,b,b,b};s.h=strip_h(a);s.y=a->height-s.h;
    for(int i=0;i<5;i++)s.items[i]=(Box){x[i],s.y+2,bw[i],s.h-4};s.tx=58+4*b;s.tw=a->width-s.tx-4;return s;
}
static unsigned strip_frame_at(const PdApp *a,int x){Strip s=strip_layout(a);unsigned n=pd_frame_count(&a->doc);int px=x-s.tx;if(px<0)px=0;if(px>=s.tw)px=s.tw-1;return (unsigned)px*n/(unsigned)s.tw;}
static bool strip_switch(PdApp *a,unsigned f){
    if(f==pd_frame_index(&a->doc))return true;if(!pd_frame_switch(&a->doc,f)){message(a,"Not enough frame memory; art retained");return false;}frame_changed(a);a->redraw=true;return true;
}
/* Taps act on press except onion: tap toggles it, holding 400 ms opens its settings. Ticks scrub. */
static void strip_input(PdApp *a,int x,int y,bool touch,bool pressed){
    Strip s=strip_layout(a);unsigned n=pd_frame_count(&a->doc),f=pd_frame_index(&a->doc);
    if(pressed){
        a->strip_press=0;a->strip_held=false;a->strip_press_ms=a->now_ms;
        for(unsigned i=0;i<5;i++)if(inside(s.items[i],x,y))a->strip_press=i+1;
        if(!a->strip_press&&x>=s.tx)a->strip_press=6;
        if(a->strip_press==1)strip_switch(a,(f+n-1)%n);
        else if(a->strip_press==3)strip_switch(a,(f+1)%n);
        else if(a->strip_press==2){a->page=1;a->layers_tab=1;timeline_follow(a);}
        else if(a->strip_press==4){if(pd_playback_start(&a->doc,&a->playback,a->now_ms)){a->overview_pending=false;a->selection_valid=false;}else message(a,"Not enough memory to start playback");}
        else if(a->strip_press==6)strip_switch(a,strip_frame_at(a,x));
        if(a->strip_press!=5&&a->strip_press!=6)a->strip_press=0;
        a->blocked=!a->strip_press;a->partial=false;a->redraw=true;return;
    }
    if(touch){
        if(a->strip_press==6)strip_switch(a,strip_frame_at(a,x));
        else if(a->strip_press==5&&!a->strip_held&&a->now_ms-a->strip_press_ms>=400){a->strip_held=true;a->onion_sheet=true;a->blocked=true;a->redraw=true;}
        return;
    }
    if(a->strip_press==5&&!a->strip_held){onion_set(a,!a->onion_enabled);message(a,a->onion_enabled?"Onion skin on - hold for settings":"Onion skin off");a->partial=false;a->redraw=true;}
    a->strip_press=0;a->strip_held=false;
}
#ifdef PD_TEST
static bool box_centre(Box b,int *x,int *y){*x=b.x+b.w/2;*y=b.y+b.h/2;return true;}
bool app_test_point(const PdApp *a,int part,int index,int *x,int *y){
    Timeline t=timeline_layout(a);FrameSheet f=frame_sheet_layout(a);OnionSheet o=onion_sheet_layout(a);
    TagSheet g=tag_sheet_layout(a);CelSheet c=cel_sheet_layout(a);
    switch(part){
    case PD_TL_TAB:return index>=0&&index<2&&box_centre(t.tabs[index],x,y);
    case PD_TL_PLAY:return box_centre(t.play,x,y);
    case PD_TL_ONION:return box_centre(t.onion,x,y);
    case PD_TL_FOOTER:return index>=0&&index<4&&box_centre(t.foot[index],x,y);
    case PD_TL_NUMBER:if(index<(int)a->timeline_first||index>=(int)a->timeline_first+t.cols)return false;*x=t.gx+(index-(int)a->timeline_first)*t.cw+t.cw/2;*y=t.num_y+5;return true;
    case PD_TL_CEL:{int col=(index&255)-(int)a->timeline_first,row=(index>>8)-(int)a->timeline_layer_first;if(col<0||col>=t.cols||row<0||row>=t.rows)return false;*x=t.gx+col*t.cw+t.cw/2;*y=t.rows_y+row*t.row_h+t.row_h/2;return true;}
    case PD_TL_NAME:{int row=index-(int)a->timeline_layer_first;if(row<0||row>=t.rows)return false;*x=4+t.name_w/2;*y=t.rows_y+row*t.row_h+t.row_h/2;return true;}
    case PD_TL_SCROLL:*x=a->width-5;*y=index?t.rows_y+t.rows*t.row_h-4:t.rows_y+4;return true;
    case PD_SHEET_CLOSE:return box_centre(f.close,x,y);
    case PD_SHEET_MINUS:return box_centre(f.minus,x,y);
    case PD_SHEET_PLUS:return box_centre(f.plus,x,y);
    case PD_SHEET_PRESET:return index>=0&&index<4&&box_centre(f.presets[index],x,y);
    case PD_SHEET_SCOPE:return index>=0&&index<2&&box_centre(f.scope[index],x,y);
    case PD_SHEET_MOVE:return index>=0&&index<2&&box_centre(f.move[index],x,y);
    case PD_ONION_TOGGLE:return box_centre(o.toggle,x,y);
    case PD_ONION_BEHIND:return box_centre(index?o.behind_plus:o.behind_minus,x,y);
    case PD_ONION_AHEAD:return box_centre(index?o.ahead_plus:o.ahead_minus,x,y);
    case PD_ONION_SLIDER:*x=o.slider.x+(o.slider.w-4)*(index-10)/80;*y=o.slider.y+o.slider.h/2;return true;
    case PD_ONION_TINT:return box_centre(o.tint,x,y);
    case PD_STRIP_ITEM:{Strip s=strip_layout(a);return s.h&&index>=0&&index<5&&box_centre(s.items[index],x,y);}
    case PD_STRIP_TICK:{Strip s=strip_layout(a);unsigned n=pd_frame_count(&a->doc);if(!s.h||index<0||(unsigned)index>=n)return false;*x=s.tx+(int)(((unsigned)index*2+1)*(unsigned)s.tw/(2*n));*y=s.y+s.h/2;return true;}
    case PD_TL_TAGBAND:if(index<(int)a->timeline_first||index>=(int)a->timeline_first+t.cols)return false;*x=t.gx+(index-(int)a->timeline_first)*t.cw+t.cw/2;*y=t.tag_y+5;return true;
    case PD_SHEET_TAG:return box_centre(f.tag,x,y);
    case PD_TAG_NAME:return box_centre(g.name,x,y);
    case PD_TAG_FROM:return box_centre(index?g.from_plus:g.from_minus,x,y);
    case PD_TAG_TO:return box_centre(index?g.to_plus:g.to_minus,x,y);
    case PD_TAG_DIRECTION:return box_centre(g.direction,x,y);
    case PD_TAG_COLOR:return box_centre(g.color,x,y);
    case PD_TAG_DELETE:return box_centre(g.remove,x,y);
    case PD_TAG_DONE:return box_centre(g.done,x,y);
    case PD_CEL_LINK:return box_centre(c.link,x,y);
    case PD_CEL_UNLINK:return box_centre(c.unlink,x,y);
    case PD_CEL_NEW_LINKED:return box_centre(c.new_linked,x,y);
    }
    return false;
}
#endif
static void layer_input(PdApp *a,int x,int y,bool pressed){
    layer_clamp(a);
    if(a->layer_delete){if(pressed&&y>=120&&y<148){if(x<a->width/2)message(a,pd_delete_layer(&a->doc)?"Layer deleted - Undo to restore":"Unlock first / keep at least one layer");a->layer_delete=false;a->layer_popup=false;a->blocked=true;a->redraw=true;}return;}
    if(a->layer_popup){
        if(pressed){
            int ox=layer_options_x(a),ow=layer_options_width(a),inner_x=layer_options_inner_x(a),inner_w=layer_options_inner_w(a);
            if(y>=58&&y<76&&x>=inner_x&&x<inner_x+inner_w){a->opacity_drag=pd_begin(&a->doc);}
            else if(y>=89&&y<111&&x>=inner_x&&x<inner_x+inner_w)pd_dropdown_open(&a->layer_blend_dropdown,ox,106,ow,10,19,4,4,a->doc.layer[a->doc.active].blend,true);
            else if(y>=117&&y<139&&x>=inner_x&&x<inner_x+inner_w){pd_begin(&a->doc);a->doc.layer[a->doc.active].flags^=(uint8_t)(x<inner_x+inner_w/2?PD_LAYER_LOCK:PD_LAYER_ALPHA_LOCK);pd_commit(&a->doc);}
            else if(y>=145&&y<167&&x>=inner_x&&x<inner_x+inner_w){int action_w=(inner_w-8)/3;if(x<inner_x+action_w){a->name_action=2;snprintf(a->filename,sizeof(a->filename),"%s",a->doc.names[a->doc.active]);text_begin(a);a->layer_popup=false;}else if(x<inner_x+2*action_w+4){message(a,pd_duplicate_layer(&a->doc)?"Layer duplicated":"No free layer slots");a->layer_popup=false;}else{message(a,pd_merge_down(&a->doc)?"Merged down - Undo to restore":pd_frame_count(&a->doc)>1?"Animated layer merge is not available yet":"Merge needs unlocked, visible Normal layers");a->layer_popup=false;}}
            else {a->layer_popup=false;}
            if(!a->opacity_drag)a->blocked=true;a->redraw=true;
        }
        if(a->opacity_drag){int slider_x=layer_options_inner_x(a),range=layer_options_inner_w(a)-1,px=x-slider_x;if(px<0)px=0;if(px>range)px=range;uint8_t value=(uint8_t)(px*255/range);if(value!=a->doc.layer[a->doc.active].opacity){a->doc.layer[a->doc.active].opacity=value;a->redraw=true;}}
        return;
    }
    if(pressed){
        int footer=layer_footer_y(a),top=layer_list_top();
        Timeline t=timeline_layout(a);
        if(y>=t.tabs[0].y&&y<t.tabs[0].y+t.tabs[0].h){if(inside(t.tabs[1],x,y)){a->layers_tab=1;timeline_follow(a);}a->blocked=true;a->redraw=true;return;}
        if(y>=footer){
            int fw=(a->width-16)/3;
            if(x<6+fw){message(a,pd_add_layer(&a->doc)?"New layer selected":"No free layer slots");a->layer_scroll=0;}
            else if(x<10+2*fw){a->blocked=true;adjustment_open(a);a->redraw=true;return;}
            else if(layer_count(a)>1&&!(a->doc.layer[a->doc.active].flags&PD_LAYER_LOCK)){a->layer_popup=false;a->layer_delete=true;message(a,"Delete selected layer?");}
            else message(a,layer_count(a)<=1?"Keep at least one layer":"Unlock the selected layer before deleting");
            a->blocked=true;a->redraw=true;return;
        }
        int row=(y-top+a->layer_scroll)/layer_row_height(),slot=y>=top?layer_at(a,row):-1;
        if(slot>=0&&y<footer){a->layer_previous=a->doc.active;a->doc.active=(unsigned)slot;
            if(x>=a->width-26){a->layer_popup=true;a->blocked=true;}
            else if(x>=a->width-46){pd_begin(&a->doc);a->doc.layer[slot].flags^=PD_LAYER_LOCK;pd_commit(&a->doc);a->blocked=true;}
            else if(x>=22&&x<44){pd_begin(&a->doc);a->doc.layer[slot].visible^=1;pd_commit(&a->doc);a->blocked=true;}
            else if(x<22||x>=44){a->layer_hold=slot;a->layer_hold_ms=a->now_ms;a->layer_scroll_at=a->now_ms;a->layer_drag_y=y;a->layer_start_y=y;a->layer_start_scroll=a->layer_scroll;a->layer_scrolling=false;}
            a->redraw=true;
        }
    }else if(a->layer_hold>=0){
        uint32_t elapsed=a->now_ms-a->layer_hold_ms;int delta=y-a->layer_start_y;
        if(!a->layer_drag&&elapsed<150&&abs(delta)>5)a->layer_scrolling=true;
        if(a->layer_scrolling){a->doc.active=a->layer_previous;a->layer_scroll=a->layer_start_scroll-delta;layer_clamp(a);a->redraw=true;}
        else if(elapsed>=150){a->layer_drag=true;a->layer_drag_y=y;if((uint32_t)(a->now_ms-a->layer_scroll_at)>=100){if(y<layer_list_top()+12)a->layer_scroll-=14;else if(y>=layer_footer_y(a)-12)a->layer_scroll+=14;layer_clamp(a);a->layer_scroll_at=a->now_ms;}a->redraw=true;}
    }
}
static bool selected(PdApp *a,const uint32_t *mask,int x,int y){return x>=0&&y>=0&&x<(int)a->doc.width&&y<(int)a->doc.height&&(mask[(y*a->doc.stride+x)/32]&(1u<<((y*a->doc.stride+x)%32)));}
static void selection_stamp(PdApp *a){a->selection_layer=a->doc.active;a->selection_revision=a->doc.revision;}
static void selection_combine(PdApp *a){
    a->selection_valid=false;for(unsigned i=0;i<pd_plane(&a->doc)/32;i++){a->selection[i]=a->selection_mode==1?a->selection_base[i]|a->selection_draft[i]:a->selection_mode==2?a->selection_base[i]&~a->selection_draft[i]:a->selection_draft[i];if(a->selection[i])a->selection_valid=true;}selection_stamp(a);
}
static void selection_release(PdApp *a,bool cancel){
    if(!a->selection_drag)return;
    if(cancel){if(a->selection_move)pd_cancel(&a->doc);memcpy(a->selection,a->selection_base,mask_bytes(a));}
    else if(a->selection_move){pd_commit(&a->doc);memset(a->selection,0,mask_bytes(a));for(int y=0;y<(int)a->doc.height;y++)for(int x=0;x<(int)a->doc.width;x++)if(selected(a,a->selection_base,x,y)){int i=(y+a->selection_dy)*a->doc.stride+x+a->selection_dx;a->selection[i/32]|=1u<<(i%32);}}
    else if(a->selection_shape){if(a->lasso_overflow){memcpy(a->selection,a->selection_base,mask_bytes(a));message(a,"Lasso too long - selection unchanged");}else{memset(a->selection_draft,0,(pd_plane(&a->doc)/8));if(a->lasso_count<3)pd_mask_rectangle_stride(a->selection_draft,a->doc.stride,a->doc.rows,a->selection_x,a->selection_y,a->selection_x,a->selection_y);else pd_mask_polygon_stride(a->selection_draft,a->lasso,a->lasso_count,a->doc.width,a->doc.height,a->doc.stride);selection_combine(a);}}
    int x,y,w,h;a->selection_valid=pd_mask_bounds_stride(a->selection,a->doc.width,a->doc.height,a->doc.stride,&x,&y,&w,&h);a->selection_drag=a->selection_move=false;selection_stamp(a);a->redraw=true;
}
static void selection_input(PdApp *a,int x,int y,bool pressed){
    int wx,wy;if(!world(a,x,y,&wx,&wy)&&!a->selection_drag)return;
    if(pressed){a->selection_offset_x=(a->guides&4)?wx-((wx%(int)a->doc.width+(int)a->doc.width)%(int)a->doc.width):0;a->selection_offset_y=(a->guides&8)?wy-((wy%(int)a->doc.height+(int)a->doc.height)%(int)a->doc.height):0;}
    wx-=a->selection_offset_x;wy-=a->selection_offset_y;if(wx<0)wx=0;if(wy<0)wy=0;if(wx>=(int)a->doc.width)wx=(int)a->doc.width-1;if(wy>=(int)a->doc.height)wy=(int)a->doc.height-1;
    if(pressed){memcpy(a->selection_base,a->selection,mask_bytes(a));a->selection_x=wx;a->selection_y=wy;a->selection_dx=a->selection_dy=0;a->lasso_count=0;a->lasso_overflow=false;a->selection_move=!a->selection_mode&&a->selection_valid&&selected(a,a->selection,wx,wy);
        pd_composite(&a->doc);if(a->selection_move){if(!a->doc.layer[a->doc.active].visible||(a->doc.layer[a->doc.active].flags&(PD_LAYER_LOCK|PD_LAYER_ALPHA_LOCK))||!pd_begin(&a->doc)){a->selection_move=false;a->blocked=true;message(a,"Move requires a visible unlocked layer");return;}}a->selection_drag=true;
    }
    if(!a->selection_drag)return;
    if(a->selection_move){int bx,by,bw,bh;pd_mask_bounds_stride(a->selection_base,a->doc.width,a->doc.height,a->doc.stride,&bx,&by,&bw,&bh);int dx=wx-a->selection_x,dy=wy-a->selection_y;if(dx< -bx)dx=-bx;if(dy< -by)dy=-by;if(dx>(int)a->doc.width-bx-bw)dx=(int)a->doc.width-bx-bw;if(dy>(int)a->doc.height-by-bh)dy=(int)a->doc.height-by-bh;
        if(dx!=a->selection_dx||dy!=a->selection_dy){pd_move_selection(&a->doc,a->selection_base,dx,dy,false);int minx=bx,miny=by,maxx=bx+bw,maxy=by+bh;int offsetsx[2]={a->selection_dx,dx},offsetsy[2]={a->selection_dy,dy};for(int i=0;i<2;i++){if(bx+offsetsx[i]<minx)minx=bx+offsetsx[i];if(by+offsetsy[i]<miny)miny=by+offsetsy[i];if(bx+bw+offsetsx[i]>maxx)maxx=bx+bw+offsetsx[i];if(by+bh+offsetsy[i]>maxy)maxy=by+bh+offsetsy[i];}pd_composite_rect(&a->doc,minx,miny,maxx,maxy);a->selection_dx=dx;a->selection_dy=dy;a->redraw=true;}
    }else if(a->selection_shape){if(!a->lasso_count||a->lasso[a->lasso_count-1][0]!=wx||a->lasso[a->lasso_count-1][1]!=wy){if(a->lasso_count<512){a->lasso[a->lasso_count][0]=(int16_t)wx;a->lasso[a->lasso_count++][1]=(int16_t)wy;}else a->lasso_overflow=true;a->redraw=true;}}
    else{memset(a->selection_draft,0,(pd_plane(&a->doc)/8));pd_mask_rectangle_stride(a->selection_draft,a->doc.stride,a->doc.rows,a->selection_x,a->selection_y,wx,wy);selection_combine(a);a->redraw=true;}
}
static void selection_action(PdApp *a,int action){
    int x=0,y=0,w=0,h=0;bool has=pd_mask_bounds_stride(a->selection,a->doc.width,a->doc.height,a->doc.stride,&x,&y,&w,&h);
    if((action==0||action==1)&&(w>128||h>128)){message(a,"Copy/cut limit: 128 x 128 selection");return;}if(action==0||action==1){if(!has){message(a,"Select pixels first");return;}if(action==1&&(!a->doc.layer[a->doc.active].visible||(a->doc.layer[a->doc.active].flags&(PD_LAYER_LOCK|PD_LAYER_ALPHA_LOCK)))){message(a,"Cut requires a visible unlocked layer");return;}if(!a->clipboard&&!(a->clipboard=calloc(1,sizeof(*a->clipboard)))){message(a,"Not enough memory to copy");return;}pd_copy_selection(&a->doc,a->selection,a->clipboard);if(action==1&&pd_begin(&a->doc)){pd_move_selection(&a->doc,a->selection,0,0,true);pd_commit(&a->doc);}message(a,action?"Cut - clipboard ready":"Copied active-layer pixels");}
    if(action==2){if(!a->clipboard||!a->clipboard->width){message(a,"Clipboard is empty");return;}if(a->clipboard->width>a->doc.width||a->clipboard->height>a->doc.height){message(a,"Clipboard is larger than this canvas");return;}if(!has){x=a->ox;y=a->oy;}if(x<0)x=0;if(y<0)y=0;if(x+(int)a->clipboard->width>(int)a->doc.width)x=(int)a->doc.width-(int)a->clipboard->width;if(y+(int)a->clipboard->height>(int)a->doc.height)y=(int)a->doc.height-(int)a->clipboard->height;
        if(!pd_paste_selection(&a->doc,a->clipboard,x,y)){message(a,"Paste made no change - check layer locks");return;}memset(a->selection,0,mask_bytes(a));for(unsigned yy=0;yy<a->clipboard->height;yy++)for(unsigned xx=0;xx<a->clipboard->width;xx++)if((a->clipboard->mask[(yy*128+xx)/32]&(1u<<((yy*128+xx)%32)))&&(a->clipboard->pixels[yy*128+xx]>>24)){unsigned i=(unsigned)(y+(int)yy)*a->doc.stride+(unsigned)x+xx;a->selection[i/32]|=1u<<(i%32);}a->selection_valid=true;message(a,"Pasted - drag inside selection to move");}
    if(action==3){memset(a->selection,0,mask_bytes(a));pd_mask_rectangle_stride(a->selection,a->doc.stride,a->doc.rows,0,0,(int)a->doc.width-1,(int)a->doc.height-1);a->selection_valid=true;}
    if(action==4){memset(a->selection,0,mask_bytes(a));a->selection_valid=false;}
    if(action==5&&has){if(pd_begin(&a->doc)){if(pd_move_selection(&a->doc,a->selection,0,0,true))pd_commit(&a->doc);else{pd_cancel(&a->doc);message(a,"Delete requires a visible unlocked layer");return;}}}
    selection_stamp(a);a->page=0;a->redraw=true;
}
static void shape_release(PdApp *a,bool cancel){
    if(!a->shape_drag)return;if(cancel)pd_cancel(&a->doc);else pd_commit(&a->doc);a->shape_drag=false;selection_stamp(a);a->partial=false;a->redraw=true;
}
static void shape_input(PdApp *a,int x,int y,bool pressed){
    int wx,wy;bool in=world(a,x,y,&wx,&wy);if(!in&&!a->shape_drag)return;
    if(!(a->guides&4)){if(wx<0)wx=0;if(wx>=(int)a->doc.width)wx=(int)a->doc.width-1;}if(!(a->guides&8)){if(wy<0)wy=0;if(wy>=(int)a->doc.height)wy=(int)a->doc.height-1;}
    if(pressed){if(!a->doc.layer[a->doc.active].visible||(a->doc.layer[a->doc.active].flags&PD_LAYER_LOCK)||!pd_begin(&a->doc)){a->blocked=true;message(a,"Shape requires a visible unlocked layer");return;}a->shape_drag=true;a->shape_x=wx;a->shape_y=wy;a->shape_lastx=a->shape_lasty=-4097;memset(a->selection_draft,0,(pd_plane(&a->doc)/8));}
    if(!a->shape_drag)return;int mx=(int)a->doc.width-1,my=(int)a->doc.height-1;if(wx-a->shape_x>mx)wx=a->shape_x+mx;if(wx-a->shape_x< -mx)wx=a->shape_x-mx;if(wy-a->shape_y>my)wy=a->shape_y+my;if(wy-a->shape_y< -my)wy=a->shape_y-my;
    if(wx==a->shape_lastx&&wy==a->shape_lasty)return;
    /* Keep old and new damage so shrinking a preview restores its old pixels. */
    memcpy(pd_pixels(&a->doc.layer[a->doc.active]),pd_before(&a->doc),pd_bytes(&a->doc));memcpy(a->guide_damage,a->selection_draft,(pd_plane(&a->doc)/8));memset(a->selection_draft,0,(pd_plane(&a->doc)/8));
    pd_pattern_shape(&a->doc,a->shape_x,a->shape_y,wx,wy,a->tool-5,a->shape_filled,a->color,a->brush_size[0],a->brush_round[0]!=0,a->guides,a->selection_draft,a->selection_valid?a->selection:NULL,a->shape_pattern);
    for(unsigned i=0;i<pd_plane(&a->doc)/32;i++)a->guide_damage[i]|=a->selection_draft[i];
    a->shape_lastx=wx;a->shape_lasty=wy;a->partial=!a->redraw;a->redraw=true;
}
/* Round to the nearest step so the knob stays under the stylus. Shapes share the pencil size. */
static void tool_slider_set(PdApp *a,int x){
    int ix=tool_panel_x(a)+8,iw=tool_panel_w(a)-16;
    int left=a->tool_panel==3?ix:ix+30,range=a->tool_panel==3?iw-5:iw-60,raw=x-left;
    if(raw<0)raw=0;if(raw>range)raw=range;
    if(a->tool_panel==3)a->fill_tolerance=(unsigned)((raw*255+range/2)/range);
    else a->brush_size[a->tool_panel>=5?0:a->brush_edit]=1+(unsigned)((raw*15+range/2)/range);
    a->brush_dirty=true;a->redraw=true;
}
static void tool_options_input(PdApp *a,int x,int y,bool pressed){
    int px=tool_panel_x(a),pw=tool_panel_w(a),ix=px+8,iw=pw-16;
    if(a->brush_drag){tool_slider_set(a,x);return;}
    if(!pressed||x<px||x>=px+pw)return;
    if(y>=26&&y<42&&x>=px+pw-18)a->tool_options_open=false;
    else if(a->tool_panel==0||a->tool_panel==1){
        if(y>=76&&y<100){
            if(x<ix+24&&a->brush_size[a->brush_edit]>1)a->brush_size[a->brush_edit]--;
            else if(x>=px+pw-32&&a->brush_size[a->brush_edit]<16)a->brush_size[a->brush_edit]++;
            else{a->brush_drag=true;tool_slider_set(a,x);}
            a->brush_dirty=true;
        }
        else if(y>=110&&y<130){a->brush_round[a->brush_edit]^=1;a->brush_dirty=true;}
        else if(a->tool_panel==0&&y>=134&&y<154){a->pixel_perfect=!a->pixel_perfect;a->brush_dirty=true;}
        else if(a->tool_panel==0&&y>=158&&y<180){a->page=27;a->toolbox=false;a->tool_options_open=false;}
    }else if(a->tool_panel==3){
        if(y>=44&&y<64)a->fill_global=!a->fill_global;
        else if(y>=68&&y<88)a->fill_visible=!a->fill_visible;
        else if(y>=92&&y<112){if(x<ix+24&&a->fill_tolerance)a->fill_tolerance--;else if(x>=px+pw-32&&a->fill_tolerance<255)a->fill_tolerance++;}
        else if(y>=114&&y<136){a->brush_drag=true;tool_slider_set(a,x);}
        else if(y>=140&&y<160)a->fill_pattern=(a->fill_pattern+1)&3;
        else if(y>=164&&y<184)a->tool_options_open=false;
        a->brush_dirty=true;
    }else if(a->tool_panel==4){
        if(y>=44&&y<64)a->selection_shape=x>=ix+iw/2;
        else if(y>=68&&y<88){unsigned action=(unsigned)((x-ix)*3/iw);if(action>2)action=2;a->selection_mode=action;}
        else if(y>=92&&y<112){unsigned action=(unsigned)((x-ix)*3/iw);selection_action(a,(int)(action>2?2:action));}
        else if(y>=116&&y<136)selection_action(a,x<ix+iw/2?3:4);
        else if(y>=140&&y<160){if(x<ix+iw/2)selection_action(a,5);else{a->page=25;a->toolbox=false;a->tool_options_open=false;}}
    }else if(a->tool_panel>=5&&a->tool_panel<8){
        if(y>=44&&y<64&&a->tool!=5)a->shape_filled=!a->shape_filled;
        else if(y>=82&&y<102){if(x<ix+24&&a->brush_size[0]>1)a->brush_size[0]--;else if(x>=px+pw-32&&a->brush_size[0]<16)a->brush_size[0]++;else{a->brush_drag=true;tool_slider_set(a,x);}a->brush_dirty=true;}
        else if(y>=110&&y<130){a->shape_pattern=(a->shape_pattern+1)&3;a->brush_dirty=true;}
    }else if(a->tool_panel==8&&y>=44&&y<164){
        int row=(y-44)/22;if(row>=0&&row<5){if(row==0)a->pixel_grid=!a->pixel_grid;else a->guides^=1u<<(row-1);a->brush_dirty=true;}
    }
    a->blocked=!a->brush_drag;a->redraw=true;
}
static void onion_cache(PdApp *a){
    if(!a->onion_enabled||a->playback.active||pd_frame_count(&a->doc)<2)return;
    size_t plane=pd_plane(&a->doc);uint64_t key=1469598103934665603ULL;
    key=(key^pd_frames_visual_generation(&a->doc))*1099511628211ULL;key=(key^pd_frames_generation(&a->doc))*1099511628211ULL;
    key=(key^pd_frame_index(&a->doc))*1099511628211ULL;
    for(unsigned i=0;i<8;i++){const PdLayer *l=&a->doc.layer[i];key=(key^a->doc.order[i])*1099511628211ULL;key=(key^l->opacity^((unsigned)l->visible<<8))*1099511628211ULL;}
    key=(key^(a->onion_behind|a->onion_ahead<<2|(unsigned)a->onion_tint<<4))*1099511628211ULL;
    key^=a->doc.enabled;if(a->onion&&a->onion_plane==plane&&a->onion_key==key)return;
    /* Behind side, ahead side, then a scratch plane when a side merges several frames. Tinted ghosts keep
     * alpha only; untinted ones keep each neighbour's colours. */
    size_t bpp=a->onion_tint?1:4,bytes=plane*bpp*((a->onion_behind>1||a->onion_ahead>1)?3:2);
    if(a->onion_bytes!=bytes||!a->onion){unsigned char *p=realloc(a->onion,bytes);if(!p){free(a->onion);a->onion=NULL;a->onion_plane=a->onion_bytes=0;a->onion_enabled=false;message(a,"Not enough memory for onion skin");return;}a->onion=p;a->onion_bytes=bytes;}
    a->onion_plane=plane;memset(a->onion,0,bytes);int f=(int)pd_frame_index(&a->doc),count=(int)pd_frame_count(&a->doc);
    static const unsigned fade[3]={255,179,102};
    for(int side=0;side<2;side++){
        int n=(int)(side?a->onion_ahead:a->onion_behind);unsigned char *out=a->onion+(size_t)side*plane*bpp,*scratch=a->onion+2*plane*bpp;
        for(int dist=n;dist>=1;dist--){int nf=side?f+dist:f-dist;if(nf<0||nf>=count)continue;
            unsigned char *target=n==1?out:scratch;
            if(bpp==1)pd_frame_alpha(&a->doc,(unsigned)nf,target);else pd_frame_rgba(&a->doc,(unsigned)nf,(uint32_t *)target);
            if(n==1)continue;
            if(bpp==1){for(size_t i=0;i<plane;i++){unsigned s=(scratch[i]*fade[dist-1]+127)/255;out[i]=(unsigned char)(s+(out[i]*(255-s)+127)/255);}}
            else {uint32_t *o=(uint32_t *)out;const uint32_t *s=(const uint32_t *)scratch;for(size_t i=0;i<plane;i++)if(s[i])pd_rgba_over(&o[i],s[i],fade[dist-1]);}
        }
    }
    a->onion_key=key;
}
void app_input(PdApp *a,bool touch,int x,int y,unsigned down,unsigned held){
    if(!masks_fit(a)){message(a,"Not enough memory for selection masks");return;}
#ifdef PD_PRIVATE_BUILD
    if(a->share_tick)a->share_tick(a);
#else
    if((a->page==28||a->page==29)&&a->online_tick)a->online_tick(a);
#endif
    if(a->playback.active){
        bool stop=down!=0||(touch&&!a->touching);
        if(stop){pd_playback_stop(&a->doc,&a->playback);timeline_follow(a);a->blocked=touch;a->redraw=true;}
        else if(pd_playback_tick(&a->doc,&a->playback,a->now_ms)){timeline_follow(a);a->redraw=true;}
        a->touching=touch;a->partial=false;return;
    }
    a->pan_frame=false;
    if(a->overview_pending&&!touch&&!a->touching&&(int32_t)(a->now_ms-a->overview_at)>=0){a->overview_pending=false;a->partial=false;a->redraw=true;}
    if(a->shape_drag&&(!touch||down||(held&(PD_PAN|PD_SAMPLE|PD_ZOOM|PD_ERASE))||(touch&&y<24)))shape_release(a,touch||down);

    if(a->selection_drag&&(!touch||down||(held&(PD_PAN|PD_ERASE|PD_SAMPLE))||(touch&&y<(a->tool==4?50:24))))selection_release(a,touch||down);
    if(a->selection_valid&&(a->selection_layer!=a->doc.active||a->selection_revision!=a->doc.revision)){memset(a->selection,0,mask_bytes(a));a->selection_valid=false;a->redraw=true;}

    recovery_tick(a,touch,down);
    if(!touch&&a->swatch_hold>=0){
        if(a->swatch_drag&&a->page==2&&!a->swatch_mini&&!a->palette_edit){unsigned from=(unsigned)a->swatch_hold,to=(unsigned)a->swatch_target;uint32_t c=a->swatches[from];
            if(from<to)memmove(a->swatches+from,a->swatches+from+1,(to-from)*4);else if(from>to)memmove(a->swatches+to+1,a->swatches+to,(from-to)*4);
            a->swatches[to]=c;a->swatch_selected=to;a->palette_dirty=from!=to;a->redraw=true;
        }else if(!a->swatch_scrolling&&a->page==2&&(a->swatch_mini||!a->palette_edit)){
            palette_select(a,(unsigned)a->swatch_hold);
            if(!a->palette_target)recent_add(a,true);
        }
        a->swatch_hold=-1;a->swatch_drag=false;a->swatch_scrolling=false;a->swatch_mini=false;
    }
    if((!touch||down)&&a->palette_dirty){a->palette_dirty=false;project_palette(a);a->picker_update=false;a->redraw=true;}
    if(a->palette_edit&&a->page!=2){a->palette_edit=false;}
    if(down){a->swatch_hold=-1;a->swatch_drag=false;}
    if(a->opacity_drag&&(!touch||down)){pd_commit(&a->doc);a->opacity_drag=false;a->redraw=true;}
    if(!touch&&a->layer_hold>=0){
        if(a->layer_drag&&a->page==1){int top=layer_list_top(),bottom=layer_footer_y(a),yy=a->layer_drag_y;if(yy<top)yy=top;if(yy>=bottom)yy=bottom-1;int row=(yy-top+a->layer_scroll)/layer_row_height();if(row<0)row=0;if(row>=PD_LAYERS)row=PD_LAYERS-1;int target=layer_at(a,row);while(target<0&&row>0)target=layer_at(a,--row);
            if(target>=0){unsigned pos=0;while(a->doc.order[pos]!=(unsigned)target)pos++;pd_move_layer(&a->doc,pos);}}
        a->layer_hold=-1;a->layer_drag=false;a->redraw=true;
    }
    if(down){a->layer_hold=-1;a->layer_drag=false;a->layer_popup=false;a->layer_delete=false;a->frame_sheet=a->frame_delete=a->onion_sheet=a->onion_drag=a->tag_sheet=a->cel_sheet=false;a->timeline_press=a->strip_press=0;}
    if(down||(touch&&y<24))a->picker_update=false;
    /* Pending non-stroke work or multiple input batches require a full refresh. */
    if(a->redraw)a->partial=false;
    if(!touch&&a->touching&&a->page==31&&a->adjust_preview_stale)adjustment_preview_build(a);
    if(!touch&&a->touching&&a->page==2&&a->palette_picker_active&&!a->palette_target)recent_add(a,a->color_drag!=0);
    bool pressed=touch&&!a->touching;
    if(!touch){finish(a);a->blocked=false;a->pan=false;a->color_drag=0;a->adjust_drag=0;}
    if(a->layer_blend_dropdown.open){
        PdDropdown *m=&a->layer_blend_dropdown;
        if(m->ignore_open_touch){
            if(!touch)m->ignore_open_touch=false;
        }else if(touch&&pressed){
            int rows_y=y-(m->y+m->header_h);
            if(x>=m->x&&x<m->x+m->w&&rows_y>=0&&rows_y<m->row_h*m->visible_rows){
                unsigned chosen=(unsigned)rows_y/m->row_h;
                if(chosen<m->count&&chosen!=a->doc.layer[a->doc.active].blend){
                    pd_begin(&a->doc);a->doc.layer[a->doc.active].blend=(uint8_t)chosen;pd_commit(&a->doc);
                }
            }
            pd_dropdown_close(m);
        }
        a->blocked=touch;a->touching=touch;a->redraw=true;return;
    }
    if(a->file_type_dropdown.open){
        if(a->page!=4||(down&(PD_TOOLS|PD_MENU))){pd_dropdown_close(&a->file_type_dropdown);a->blocked=touch;a->touching=touch;a->redraw=true;return;}
        unsigned chosen=a->file_filter;PdDropdownEvent event=pd_dropdown_input(&a->file_type_dropdown,touch,pressed,x,y,&chosen);
        if(event==PD_DROPDOWN_SELECTED&&chosen<8){a->file_filter=chosen;a->file_scroll=0;a->selected=-1;a->preview_attempted=false;}
        a->blocked=touch;a->touching=touch;a->redraw=true;return;
    }
    if(a->community_dropdown.open){
        if(down&(PD_TOOLS|PD_MENU)){pd_dropdown_close(&a->community_dropdown);a->blocked=touch;a->touching=touch;a->redraw=true;return;}
        if(down&PD_SAMPLE)pd_dropdown_scroll(&a->community_dropdown,-1);
        if(down&PD_ZOOM)pd_dropdown_scroll(&a->community_dropdown,1);
        unsigned chosen=a->community_mode;PdDropdownEvent event=pd_dropdown_input(&a->community_dropdown,touch,pressed,x,y,&chosen);
        if(event==PD_DROPDOWN_SELECTED)community_set_mode(a,chosen);
        a->blocked=touch;a->touching=touch;a->redraw=true;return;
    }
    /* Edge-triggered history chords are canvas-only, with the pen fully lifted. */
    if(!touch&&!a->touching&&!a->page&&!a->confirm&&!a->toolbox&&!a->doc.editing&&!(down&(PD_MENU|PD_SAVE|PD_TOOLS))){
        unsigned prev=down&PD_FRAME_PREV,next=down&PD_FRAME_NEXT;
        if((held&PD_PAN)&&!(held&(PD_SAMPLE|PD_ZOOM))){prev|=down&PD_SHOULDER_L;next|=down&PD_SHOULDER_R;}
        if((prev!=0)!=(next!=0)){unsigned frame=pd_frame_index(&a->doc),count=pd_frame_count(&a->doc);bool ok=next?frame+1<count&&pd_frame_switch(&a->doc,frame+1):frame>0&&pd_frame_switch(&a->doc,frame-1);
            if(ok){a->selection_valid=false;memset(a->selection,0,mask_bytes(a));a->partial=false;a->overview_pending=false;a->redraw=true;}else message(a,"Frame limit or not enough memory");}
        /* Sample+L toggles onion skin; Sample+R plays (any later button or touch stops). */
        unsigned shoulder=down&(PD_SHOULDER_L|PD_SHOULDER_R);
        if((held&PD_SAMPLE)&&!(held&(PD_PAN|PD_ZOOM))&&shoulder&&shoulder!=(PD_SHOULDER_L|PD_SHOULDER_R)&&pd_frame_count(&a->doc)>1){
            if(shoulder==PD_SHOULDER_L){onion_set(a,!a->onion_enabled);message(a,a->onion_enabled?"Onion skin on":"Onion skin off");}
            else if(pd_playback_start(&a->doc,&a->playback,a->now_ms)){a->overview_pending=false;a->selection_valid=false;}else message(a,"Not enough memory to start playback");
            a->partial=false;a->redraw=true;
        }
    }
    if(!touch&&!a->touching&&!a->page&&!a->confirm&&!a->toolbox&&!a->doc.editing&&(held&PD_ZOOM)&&!(down&(PD_MENU|PD_SAVE|PD_TOOLS))){
        unsigned shoulders=held&(PD_SHOULDER_L|PD_SHOULDER_R),pressed_shoulder=down&(PD_SHOULDER_L|PD_SHOULDER_R);
        if(pressed_shoulder&&shoulders!= (PD_SHOULDER_L|PD_SHOULDER_R)){
            bool redo=pressed_shoulder==PD_SHOULDER_R;
            bool changed=redo?pd_redo(&a->doc):pd_undo(&a->doc);
            if(changed){a->selection_valid=false;memset(a->selection,0,mask_bytes(a));}
            message(a,changed?(redo?"Redo":"Undo"):(redo?"No redo":"No more undo"));a->partial=false;a->redraw=true;
        }
    }
    bool overlay=(held&PD_ZOOM)!=0&&!a->page&&!a->confirm;
    if(overlay!=a->zoom_overlay){
        finish(a);a->zoom_overlay=overlay;a->redraw=true;a->partial=false;
        if(touch&&!pressed)a->blocked=true;
    }
    if(down&(PD_MENU|PD_TOOLS|PD_SAVE)){
        finish(a);pd_dropdown_close(&a->layer_blend_dropdown);if(a->page==31)a->adjust_preview_ready=a->adjust_preview_stale=false;a->blocked=touch;
        if(down&PD_SAVE)save(a);
        else if(down&PD_MENU){a->page=a->page==3?0:3;a->confirm=0;}
        else if(down&PD_TOOLS){a->tool_options_open=false;if(a->page){a->toolbox=a->page==21||a->page==23||a->page==24||a->page==26||a->page==27;a->page=0;}else a->toolbox=!a->toolbox;a->confirm=0;}
        if(down&(PD_MENU|PD_SAVE))a->toolbox=false;
        a->zoom_overlay=false;
        a->redraw=true;
    }
    if(!touch){a->text_repeat=false;a->brush_drag=false;if(a->brush_dirty){brush_save(a);a->brush_dirty=false;}if(a->presets_dirty){presets_save(a);a->presets_dirty=false;}}
    if(a->page==5&&touch&&a->text_repeat){if(keyboard_delete_hit(a,x,y)){if((int32_t)(a->now_ms-a->text_repeat_at)>=0){text_delete(a);a->text_repeat_at=a->now_ms+60;}}else a->text_repeat=false;}
    if(a->file_hold>=0){
        if(a->page!=4||down||a->confirm)a->file_hold=-1;
        else if(touch){int delta=y-a->file_start_y;if(abs(delta)>5)a->file_scrolling=true;if(a->file_scrolling){a->file_scroll=a->file_start_scroll-delta;file_clamp(a);a->redraw=true;}a->touching=true;return;}
        else {int i=a->file_hold;a->file_hold=-1;if(!a->file_scrolling){PdEntry *e=&a->listing.entries[i];char path[PD_PATH];if(pd_join(path,a->folder,e->name)){if(e->directory){snprintf(a->folder,sizeof(a->folder),"%s",path);browse_refresh(a);}else {a->selected=i;preview_file(a,path);}}}a->touching=false;return;}
    }
    if(touch&&a->blocked){a->touching=touch;return;}
    if(a->zoom_overlay){
        if(pressed){
            int zx=zoom_left(a);if(x>=zx&&x<zx+48){
                if(y>=40&&y<96)zoom(a,1);
                else if(y>=112&&y<168)zoom(a,-1);
            }
            a->blocked=true;
        }
        a->touching=touch;return;
    }
    if(a->confirm){
        if(pressed&&y>=a->height-32){
            if(x<a->width/2){
                if(a->confirm==1)message(a,pd_load(&a->doc,a->directory)==1?"Project reloaded":"Reload failed - drawing retained");
                if(a->confirm==4)open_pending(a);
                if(a->confirm==5)write_pending(a,true);
                if(a->confirm==2)new_project(a);
                if(a->confirm==8)recovery_restore(a);
                if(a->confirm==9)recovery_discard(a);
                if(a->confirm==3)a->exit_requested=true;
                if(a->confirm==7){if(pd_file_manage(a->file_source,NULL,2,a->notice)){recent_project_change(a,a->file_source,NULL);if(!strcmp(a->project,a->file_source)){a->project[0]=0;a->project_native=false;a->doc.revision=++a->doc.serial;}a->page=4;browse_refresh(a);message(a,"File deleted");}else a->redraw=true;}
                if(a->confirm==6)message(a,pd_delete_layer(&a->doc)?"Layer deleted - Undo to restore":"Keep at least one layer");
            }
            a->confirm=0;a->blocked=true;a->redraw=true;
        }
    }else if(pressed&&y<24){
        pd_dropdown_close(&a->layer_blend_dropdown);a->layer_popup=false;a->layer_delete=false;a->layer_hold=-1;a->layer_drag=false;
        a->frame_sheet=a->frame_delete=a->onion_sheet=a->onion_drag=a->tag_sheet=a->cel_sheet=false;a->timeline_press=0;
        finish(a);int b=x*6/a->width;
        if(b==0)message(a,pd_undo(&a->doc)?"Undo":"No more undo");
        if(b==1)message(a,pd_redo(&a->doc)?"Redo":"No redo");
        if(b==2){a->page=a->page==2?0:2;if(a->page==2)color_open(a);else a->palette_target=0;}
        if(b==3)a->page=a->page==1?0:1;
        if(b==4){a->page=0;a->toolbox=!a->toolbox;if(a->toolbox){a->tool_panel=a->tool;a->tool_options_open=false;}else a->tool_options_open=false;}
        else a->toolbox=false;
        if(b==5)a->page=a->page?0:3;
        a->blocked=true;a->redraw=true;
    }else if(a->toolbox&&!a->page){
        if(a->brush_drag&&touch){
            tool_options_input(a,x,y,false);
        }else if(pressed){
            int rail=tool_rail_width(a),row_h=tool_row_height(a),left=4,top=26,px=tool_panel_x(a),pw=tool_panel_w(a);
            if(x>=left&&x<left+rail&&y>=top){
                int pick=(y-top)/row_h;
                if(pick>=0&&pick<9&&y<top+pick*row_h+row_h){
                    a->tool_panel=(unsigned)pick;
                    if(pick<5)a->tool=(unsigned)pick;
                    else if(pick<8)a->tool=5+(unsigned)(pick-5);
                    a->tool_options_open=pick!=2;
                    if(pick==0||pick==1)a->brush_edit=a->tool;
                    if(pick==8)a->tool_options_open=true;
                    message(a,pick==0?"Pencil":pick==1?"Eraser":pick==2?"Sample visible color":pick==3?"Fill":pick==4?"Selection":pick==5?"Line":pick==6?"Rectangle":pick==7?"Ellipse":"Guides");
                    clamp(a);
                }
            }else if(a->tool_options_open&&x>=px&&x<px+pw)tool_options_input(a,x,y,true);
            else {a->toolbox=false;a->tool_options_open=false;}
            a->blocked=!a->brush_drag;a->redraw=true;
        }
    }else if(!a->page&&(a->onion_sheet||a->onion_drag)){
        onion_sheet_input(a,x,y,touch,pressed);a->partial=false;a->touching=touch;return;
    }else if(!a->page&&(a->strip_press||(pressed&&strip_h(a)&&y>=canvas_bottom(a)))){
        strip_input(a,x,y,touch,pressed);a->touching=touch;return;
    }else if(a->page){
        if(a->page==1&&a->layers_tab&&(touch||a->touching)){timeline_input(a,x,y,touch,pressed,held);a->touching=touch;return;}
        if(a->page==7&&touch){if(pressed&&y>=a->height-28){diagnostics_run(a);}a->blocked=true;a->redraw=true;a->touching=true;return;}
        if(a->page==26&&touch){if(pressed){int left=a->handed?a->width-224:0,xx=x-left;if(xx<0||xx>=224||y>=184)a->page=0;
            else if(y>=48&&y<72){a->fill_global=!a->fill_global;a->brush_dirty=true;}
            else if(y>=76&&y<100){a->fill_visible=!a->fill_visible;a->brush_dirty=true;}
            else if(y>=104&&y<128){if(xx<48&&a->fill_tolerance)a->fill_tolerance--;else if(xx>=176&&a->fill_tolerance<255)a->fill_tolerance++;a->brush_dirty=true;}
            else if(y>=132&&y<154)a->brush_drag=true;else if(y>=158)a->page=0;if(!a->brush_drag)a->blocked=true;a->redraw=true;}
            if(a->brush_drag){int left=a->handed?a->width-224:0,px=x-left-16;if(px<0)px=0;if(px>191)px=191;a->fill_tolerance=(unsigned)(px*255/191);a->brush_dirty=true;a->redraw=true;}a->touching=true;return;}
        if(a->page==24&&touch){if(pressed){int left=a->handed?a->width-224:0,xx=x-left;if(xx<0||xx>=224||y>=184)a->page=0;
            else if(y>=48&&y<72)a->tool=5+(unsigned)(xx*3/224);
            else if(y>=76&&y<100&&a->tool!=5)a->shape_filled=!a->shape_filled;
            else if(y>=104&&y<128){if(xx<48&&a->brush_size[0]>1)a->brush_size[0]--;else if(xx>=176&&a->brush_size[0]<16)a->brush_size[0]++;a->brush_dirty=true;}
            else if(y>=132&&y<154)a->brush_drag=true;else if(y>=158)a->page=0;if(!a->brush_drag)a->blocked=true;a->redraw=true;}
            if(a->brush_drag){int left=a->handed?a->width-224:0,px=x-left-16;if(px<0)px=0;if(px>191)px=191;a->brush_size[0]=1+(unsigned)(px*15/191);a->brush_dirty=true;a->redraw=true;}a->touching=true;return;}
        if(a->page==25&&touch){if(pressed){int sx=advanced_sheet_x(a),sw=advanced_sheet_w(a),ix=sx+8,iw=sw-16,xx=x-ix;
            if(x<sx||x>=sx+sw||y<28||y>=(int)a->height-8||(y<46&&x>=sx+sw-18)){a->page=0;a->toolbox=true;a->tool_options_open=true;a->tool_panel=4;}
            else if(y>=52&&y<152){unsigned row=(unsigned)(y-52)/25,col=xx>=iw/2?1u:0u,op=row*2+col;bool ok=false;if(op>=8)op=7;if(a->selection_valid){if(op<3)ok=pd_transform_selection(&a->doc,a->selection,op,a->selection_draft);else if(op==3)ok=pd_duplicate_selection(&a->doc,a->selection);else ok=pd_scale_selection(&a->doc,a->selection,op==7?-2:(int)op-2,a->selection_draft);}if(ok){if(op!=3)memcpy(a->selection,a->selection_draft,mask_bytes(a));int bx,by,bw,bh;a->selection_valid=pd_mask_bounds_stride(a->selection,a->doc.width,a->doc.height,a->doc.stride,&bx,&by,&bw,&bh);selection_stamp(a);a->page=0;a->toolbox=true;a->tool_options_open=true;a->tool_panel=4;message(a,op==3?"Duplicated to layer - drag to move":"Selection transformed");}else message(a,op==3?"Select pixels; needs a free layer":"Needs <=128 selection, fit, unlocked");}
            else if(y>=158&&y<180){a->page=0;a->toolbox=true;a->tool_options_open=true;a->tool_panel=4;}a->blocked=true;a->redraw=true;}a->touching=true;return;}

        if(a->page==23&&touch){if(pressed){int left=a->handed?a->width-224:0,xx=x-left;if(xx<0||xx>=224||y>=186)a->page=0;
            else if(y>=28&&y<46&&xx>=164)a->page=25;
            else if(y>=48&&y<72)a->selection_shape=xx>=112;
            else if(y>=76&&y<100)a->selection_mode=(unsigned)(xx*3/224);
            else if(y>=104&&y<128)selection_action(a,xx*3/224);
            else if(y>=132&&y<156)selection_action(a,xx<112?3:4);
            else if(y>=160&&y<184){if(xx<112)selection_action(a,5);else a->page=0;}
            a->blocked=true;a->redraw=true;}a->touching=true;return;}
        if(a->page==28&&touch){if(pressed){
            if(y>=28&&y<46&&x<54){a->page=29;a->pairing_state=0;snprintf(a->pairing_message,sizeof(a->pairing_message),"PAIR YOUR HANDHELD");}
            else if(y>=28&&y<46&&x>=a->width-58){if(a->community_mode){if(a->online_list){a->community_loading=true;a->community_loaded=false;snprintf(a->community_status,sizeof(a->community_status),"LOADING COMMUNITY PAGE");a->online_list(a,a->community_mode,a->gallery_page*PD_COMMUNITY_PAGE);}}else gallery_open(a);}
            else if(y>=28&&y<46){pd_dropdown_open(&a->community_dropdown,8,48,a->width-16,18,26,3,4,a->community_mode,true);}
            else if(y>=48&&y<144){unsigned index=(unsigned)((y-48)/48*3+x*3/a->width);if(a->community_mode){if(index<a->community_page.count){a->selected=(int)index;a->preview_attempted=true;a->preview_ready=false;snprintf(a->preview_name,sizeof(a->preview_name),"%.63s",a->community_page.items[index].title);snprintf(a->preview_error,sizeof(a->preview_error),"BY %.42s",a->community_page.items[index].artist);}}else if(index<a->gallery_count)gallery_select(a,a->gallery_page*6+index);}
            else if(y>=a->height-28){if(x<44){if(a->gallery_page){a->gallery_page--;if(a->community_mode){a->community_loading=true;if(a->online_list)a->online_list(a,a->community_mode,a->gallery_page*PD_COMMUNITY_PAGE);}else gallery_page_load(a);}}
                else if(x>=a->width-44){if(a->community_mode){if(a->community_page.has_next){a->gallery_page++;a->community_loading=true;if(a->online_list)a->online_list(a,a->community_mode,a->community_page.next_offset);}}else if((a->gallery_page+1)*6<a->listing.count){a->gallery_page++;gallery_page_load(a);}}
                else if(a->community_mode==1||a->community_mode==2){if(x<a->width/2){if(a->selected>=0&&(unsigned)a->selected<a->community_page.count&&a->gallery_ready[a->selected]){char dir[PD_PATH],name[80];snprintf(name,sizeof(name),"%s.png",a->community_page.items[a->selected].id);if(pd_join(dir,a->directory,"community-cache")&&pd_join(a->pending,dir,name)&&pd_exists(a->pending)){a->file_mode=0;a->confirm=4;message(a,"Import device-sized copy? Unsaved art will be lost");}}}
                    else if(a->selected>=0&&(unsigned)a->selected<a->community_page.count&&!a->community_action_busy&&a->online_favorite){a->community_action_busy=true;a->online_favorite(a,(unsigned)a->selected,a->community_mode==1);}}
                else if(!a->community_mode&&a->selected>=0&&a->preview_ready&&pd_join(a->pending,a->folder,a->listing.entries[a->selected].name)){a->file_mode=0;a->confirm=4;message(a,"Open image? Unsaved art will be lost");}}
            a->blocked=true;a->redraw=true;}a->touching=true;return;}
        if(a->page==29&&touch){if(pressed){
            if(y>=a->height-32){a->page=28;}
            else if(y>=78&&y<110&&a->pairing_state!=1&&a->pairing_state!=6){if(a->online_begin)a->online_begin(a);else{a->pairing_state=4;snprintf(a->pairing_message,sizeof(a->pairing_message),"VERIFIED PAIRING UNAVAILABLE HERE");}}
            a->blocked=true;a->redraw=true;}a->touching=true;return;}
#ifdef PD_PRIVATE_BUILD
        if(a->page==30&&touch){if(pressed){if(y>=a->height-32){a->page=0;}else if(y>=52&&y<80){a->share_animation=false;a->page=0;if(a->share_current)a->share_current(a,false);else message(a,"Sprite sharing is unavailable");}else if(y>=86&&y<114){a->share_animation=false;a->page=0;if(a->share_current)a->share_current(a,true);else message(a,"Sprite sharing is unavailable");}else if(y>=120&&y<148){a->share_animation=true;a->page=0;if(a->share_current)a->share_current(a,true);else message(a,"Sprite sharing is unavailable");}a->blocked=true;a->redraw=true;}a->touching=true;return;}
#endif
        if(a->page==31&&touch){adjustment_input(a,x,y,pressed);a->touching=touch;return;}
        if(a->page==27&&touch){if(pressed){int sx=advanced_sheet_x(a),sw=advanced_sheet_w(a),ix=sx+8,iw=sw-16,xx=x-ix;
            if(x<sx||x>=sx+sw||y<28||y>=(int)a->height-8||(y<46&&x>=sx+sw-18)){a->page=0;a->toolbox=true;a->tool_options_open=true;a->tool_panel=0;}
            else if(y>=60&&y<82&&xx>=0&&xx<iw){a->brush_pattern=(unsigned)(xx*4/iw);a->presets_dirty=true;}
            else if(y>=98&&y<134&&xx>=0&&xx<iw){a->brush_preset=(unsigned)(xx*4/iw);if(a->brush_preset>3)a->brush_preset=3;}
            else if(y>=148&&y<168&&xx>=0&&xx<iw){unsigned char *p=a->brush_presets[a->brush_preset];
                if(xx<112){a->brush_size[0]=p[0];a->brush_round[0]=p[1];a->pixel_perfect=p[2]!=0;a->brush_pattern=p[3];a->brush_dirty=true;message(a,"Pencil preset loaded");}
                else{p[0]=(unsigned char)a->brush_size[0];p[1]=(unsigned char)a->brush_round[0];p[2]=a->pixel_perfect;p[3]=(unsigned char)a->brush_pattern;message(a,"Pencil preset stored");}a->presets_dirty=true;
            }else if(y>=170&&y<184){a->page=0;a->toolbox=true;a->tool_options_open=true;a->tool_panel=0;}
            a->blocked=true;a->redraw=true;}a->touching=true;return;}
        if(a->page==21&&touch){
            int left=a->handed?a->width-200:0;
            if(pressed){if(x<left||x>=left+200||y>=(a->brush_edit?136:190)){a->page=0;}else if(y<54&&x>=left+140){a->page=0;a->toolbox=true;}else if(y>=56&&y<84){if(x<left+44&&a->brush_size[a->brush_edit]>1)a->brush_size[a->brush_edit]--;else if(x>=left+156&&a->brush_size[a->brush_edit]<16)a->brush_size[a->brush_edit]++;}else if(y>=88&&y<108)a->brush_drag=true;else if(y>=112&&y<134)a->brush_round[a->brush_edit]^=1;else if(!a->brush_edit&&y>=166&&y<188)a->page=27;else if(!a->brush_edit&&!a->brush_pattern&&y>=140&&y<164)a->pixel_perfect=!a->pixel_perfect;if(!a->brush_drag)a->blocked=true;a->brush_dirty=true;a->redraw=true;}
            if(a->brush_drag){int px=x-left-16;if(px<0)px=0;if(px>167)px=167;a->brush_size[a->brush_edit]=1+(unsigned)(px*15/167);a->brush_dirty=true;a->redraw=true;}a->touching=true;return;
        }
        if(a->page==22&&touch){if(pressed){int row=(y-40)/24;if(y>=40&&row<5){if(!row)a->pixel_grid=!a->pixel_grid;else a->guides^=1u<<(row-1);clamp(a);a->brush_dirty=true;}else if(y>=a->height-24)a->page=0;a->blocked=true;a->redraw=true;}a->touching=true;return;}
        if(a->page==1&&touch){layer_input(a,x,y,pressed);a->touching=touch;return;}
        if(a->page==2&&touch){color_input(a,x,y,pressed);a->touching=touch;return;}
        if(pressed){
            if(a->page==8){
                if(y>=32&&y<60)a->draft_handed^=1;
                else if(y>=64&&y<92)a->draft_zoom_side=(a->draft_zoom_side+1)%3;
                else if(y>=96&&y<120){a->draft_handed=0;a->draft_zoom_side=0;a->draft_bindings=228;}
                else if(y>=124&&y<148){if(x<a->width/2){a->page=9;a->help_tab=0;}else a->page=7;}
                else if(y>=a->height-32){
                    if(x<a->width/2){if(settings_save(a)){a->handed=a->draft_handed;a->zoom_side=a->draft_zoom_side;a->bindings=a->draft_bindings;a->page=0;message(a,"Settings saved");}else message(a,"Settings save failed - retry");}
                    else a->page=0;
                }
                a->blocked=true;a->redraw=true;
            }
            else if(a->page==9){if(y>=32&&y<56)a->help_tab=(unsigned)(x*4/a->width);else if(y>=a->height-28)a->page=x<a->width/2?10:8;a->blocked=true;a->redraw=true;}
            else if(a->page==10){
                if(y>=32&&y<136){unsigned row=(unsigned)(y-32)/26,old=(a->draft_bindings>>(row*2))&3,next=(old+1)&3;for(unsigned i=0;i<4;i++)if(i!=row&&((a->draft_bindings>>(i*2))&3)==next){a->draft_bindings=(a->draft_bindings&~(3u<<(i*2)))|(old<<(i*2));break;}a->draft_bindings=(a->draft_bindings&~(3u<<(row*2)))|(next<<(row*2));}
                else if(y>=a->height-28){if(x<a->width/2)a->draft_bindings=228;else a->page=8;}
                a->blocked=true;a->redraw=true;
            }
            else if(a->page==11){
                if(y>=30&&y<54)numeric_open(a,(unsigned)(x*5/a->width));
                else if(y>=80&&y<144){unsigned key=(unsigned)((y-80)/16*4+x*4/a->width);const char *keys="0123456789ABCDEF";if(key<16&&(!a->numeric_mode||key<10)){if(a->numeric_replace){a->numeric[0]=0;a->numeric_replace=false;}size_t n=strlen(a->numeric);if(n<(a->numeric_mode?3u:8u)){a->numeric[n]=keys[key];a->numeric[n+1]=0;}}}
                else if(y>=a->height-28){int b=x*3/a->width;if(!b){size_t n=strlen(a->numeric);if(n)a->numeric[n-1]=0;a->numeric_replace=false;}else if(b==1)a->page=2;else {uint32_t color=a->color;bool ok;if(!a->numeric_mode)ok=pd_color_parse(a->numeric,&color);else {unsigned v=(unsigned)strtoul(a->numeric,NULL,10),shifts[]={0,16,8,0,24};ok=a->numeric[0]&&v<=255;if(ok)color=(color&~(255u<<shifts[a->numeric_mode]))|(v<<shifts[a->numeric_mode]);}if(ok){a->color=color;color_hsv(a);a->page=2;if(!a->palette_edit)recent_add(a,true);}else message(a,"Use 6/8 HEX digits or decimal 0-255");}}
                a->blocked=true;a->redraw=true;a->picker_update=false;
            }
            else if(a->page==12){
                if(y>=a->height-24)a->page=2;
                else if(y>=30&&y<162){int row=(y-30)/22;if(row==0)numeric_open(a,0);if(row==1){uint32_t c=a->color;a->color=a->background;a->background=c;color_hsv(a);a->page=2;}if(row==2)a->page=13;if(row==3)browse(a,4);if(row==4)a->page=14;if(row==5)a->page=15;}
                a->blocked=true;a->redraw=true;
            }
            else if(a->page==13){if(y>=a->height-28)a->page=12;else if(y>=40&&y<136){unsigned i=(unsigned)((y-40)/32*4+x*4/a->width);if(i<a->recent_count){a->color=a->recent[i];color_hsv(a);a->page=2;}}a->blocked=true;a->redraw=true;}
            else if(a->page==14){if(y>=a->height-28)a->page=12;else if(y>=40&&y<136)browse(a,5+(unsigned)(y-40)/32);a->blocked=true;a->redraw=true;}
            else if(a->page==15){
                if(y>=a->height-28)a->page=12;
                else if(y>=40&&y<68)message(a,palette_save(a)?"Default saved for new projects":"Default save failed");
                else if((y>=76&&y<104)||(y>=112&&y<140)){bool builtin=y>=112;a->swatch_count=PALETTE_DEFAULT_COUNT;memcpy(a->swatches,palette,sizeof(palette));if(!builtin)palette_load(a);project_palette(a);a->palette_dirty=false;a->swatch_selected=0;a->palette_scroll[0]=a->palette_scroll[1]=0;a->color_tab=1;a->page=2;message(a,builtin?"Built-in colors restored to project":"Saved default copied into project");}
                a->blocked=true;a->redraw=true;
            }
            else if(a->page==18){if(y>=a->height-28){if(x<a->width/2){a->confirm=2;message(a,"Create replaces the current drawing");}else a->page=3;}else if(y>=26&&y<48){a->page=20;a->size_axis=0;a->numeric_replace=true;snprintf(a->numeric,sizeof(a->numeric),"%u",a->new_width);}else if(y>=52&&y<82)a->new_background=(a->new_background+1)%3;else if(y>=86&&y<116)a->new_layers=a->new_layers%pd_capacity(a->new_width,a->new_height,a->doc.profile)+1;else if(y>=120&&y<150)a->new_current_palette=!a->new_current_palette;a->blocked=true;a->redraw=true;}
            else if(a->page==20){
                if(y>=32&&y<56){unsigned sizes[]={16,32,64,128,256,512};unsigned n=a->doc.profile>=2?6:4;a->new_width=a->new_height=sizes[x*(int)n/a->width];if(a->new_layers>pd_capacity(a->new_width,a->new_height,a->doc.profile))a->new_layers=pd_capacity(a->new_width,a->new_height,a->doc.profile);snprintf(a->numeric,sizeof(a->numeric),"%u",a->new_width);a->numeric_replace=true;}
                else if(y>=60&&y<84){a->size_axis=x>=a->width/2;snprintf(a->numeric,sizeof(a->numeric),"%u",a->size_axis?a->new_height:a->new_width);a->numeric_replace=true;}
                else if(y>=90&&y<150){unsigned key=(unsigned)((y-90)/20*3+x*3/a->width);if(key<9){size_t n=strlen(a->numeric);if(a->numeric_replace)n=0;if(n<3){a->numeric[n]=(char)('1'+key);a->numeric[n+1]=0;}a->numeric_replace=false;}}
                else if(y>=150&&y<170){if(x<a->width/3){size_t n=strlen(a->numeric);if(n)a->numeric[n-1]=0;}else if(x<2*a->width/3){size_t n=a->numeric_replace?0:strlen(a->numeric);if(n<3){a->numeric[n]='0';a->numeric[n+1]=0;}a->numeric_replace=false;}else {unsigned v=(unsigned)atoi(a->numeric);if(v>=1&&v<=(a->doc.profile>=2?512u:128u)){if(a->size_axis)a->new_height=v;else a->new_width=v;if(a->new_layers>pd_capacity(a->new_width,a->new_height,a->doc.profile))a->new_layers=pd_capacity(a->new_width,a->new_height,a->doc.profile);a->numeric_replace=true;}else message(a,a->doc.profile>=2?"Size must be 1 to 512":"Size must be 1 to 128");}}
                else if(y>=a->height-20)a->page=18;
                a->blocked=true;a->redraw=true;
            }
            else if(a->page==19){if(y>=a->height-28)a->page=0;else if(y>=40&&y<66&&a->recovery_available){a->confirm=8;message(a,"Restore replaces current drawing");}else if(y>=72&&y<98&&a->recovery_available){a->confirm=9;message(a,"Delete recovery copies? Art is retained");}else if(y>=104&&y<130){a->recovery_enabled=!a->recovery_enabled;message(a,a->recovery_enabled?"Recovery enabled for this session":"Recovery paused for this session");}a->blocked=true;a->redraw=true;}
            else if(a->page==16){if(y>=a->height-28)a->page=4;else if(y>=40&&y<136){unsigned row=(unsigned)(y-40)/32;if(row==2){a->confirm=7;message(a,"Delete selected file from storage?");}else {a->name_action=row==0?3:4;const char *base=strrchr(a->file_source,'/');base=base?base+1:a->file_source;const char *ext=strrchr(base,'.');size_t n=ext?(size_t)(ext-base):strlen(base);if(n>=sizeof(a->filename))n=sizeof(a->filename)-1;memcpy(a->filename,base,n);a->filename[n]=0;text_begin(a);}}a->blocked=true;a->redraw=true;}
            else if(a->page==17){if(y>=a->height-28){if(x>=a->width/2){a->page=4;a->preview_attempted=false;}else if(a->recent_project_selected>=0){int n=snprintf(a->pending,sizeof(a->pending),"%s/%s",a->directory,a->recent_projects[a->recent_project_selected]);if(n<(int)sizeof(a->pending)){a->file_mode=0;a->confirm=4;message(a,"Open replaces current drawing");}}}else if(y>=30&&y<158){unsigned i=(unsigned)(y-30)/16;if(i<a->recent_project_count){a->recent_project_selected=(int)i;char path[PD_PATH];if(snprintf(path,sizeof(path),"%s/%s",a->directory,a->recent_projects[i])<(int)sizeof(path))preview_file(a,path);}}a->blocked=true;a->redraw=true;}
            else if(a->page==4||a->page==5)browser_input(a,x,y);
            else if(a->page==6){if(y>=45&&y<85)browse(a,2);else if(y>=95&&y<135)browse(a,3);}
            else if(a->page==3){MenuButton b[12];unsigned n=menu_buttons(a,b);for(unsigned i=0;i<n;i++)if(x>=b[i].x&&x<b[i].x+b[i].w&&y>=b[i].y&&y<b[i].y+b[i].h){action(a,b[i].action);break;}}
            else {int i=(y-30)/24;if(y>=30&&i<6)action(a,i);}
        }
    }else if(a->tool==4&&touch&&y<50){
        if(pressed){int action=x*8/a->width;if(action==0){a->selection_shape^=1;message(a,a->selection_shape?"Lasso selection":"Rectangle selection");}else if(action<=3){a->selection_mode=(unsigned)(action-1);message(a,action==1?"Replace / drag inside to move":action==2?"Add to selection":"Remove from selection");}else if(action<=6)selection_action(a,action-4);else selection_action(a,4);a->blocked=true;a->redraw=true;}
    }else if(touch){
        int wx=0,wy=0;
        if(held&PD_PAN){
            finish(a);
            if(!a->pan){a->pan=true;a->panx=x;a->pany=y;}
            int dx=a->panx-x,dy=a->pany-y;a->panx=x;a->pany=y;pan_pixels(a,dx,dy);
        }else if(a->pan){a->blocked=true;}
        else if((held&PD_SAMPLE)||a->tool==2){finish(a);pd_composite(&a->doc);if(world(a,x,y,&wx,&wy))a->color=pd_image(&a->doc)[((wy%(int)a->doc.height+(int)a->doc.height)%(int)a->doc.height)*a->doc.stride+(wx%(int)a->doc.width+(int)a->doc.width)%(int)a->doc.width]|0xff000000u;a->blocked=true;message(a,"Color sampled");}
        else if(a->tool>=5&&!(held&PD_ERASE)){shape_input(a,x,y,pressed);}
        else if(a->tool==4&&!(held&PD_ERASE)){selection_input(a,x,y,pressed);}
        else if(a->tool==3&&!(held&PD_ERASE)&&world(a,x,y,&wx,&wy)){
            finish(a);wx=(wx%(int)a->doc.width+(int)a->doc.width)%(int)a->doc.width;wy=(wy%(int)a->doc.height+(int)a->doc.height)%(int)a->doc.height;
            bool changed=pd_fill_pattern(&a->doc,wx,wy,a->color,a->guides,a->selection_valid?a->selection:NULL,a->fill_global,a->fill_tolerance,a->fill_visible,a->fill_pattern);message(a,changed?(a->fill_global?"Replaced matching colors on active layer":"Filled active layer"):(a->doc.layer[a->doc.active].flags&PD_LAYER_LOCK)?"Layer locked":!a->doc.layer[a->doc.active].visible?"Layer hidden":"Fill made no change");if(changed){recent_add(a,false);selection_stamp(a);}a->blocked=true;a->redraw=true;
        }
        else if(world(a,x,y,&wx,&wy)){
            bool started=false;
            if(!a->stroke){
                if(a->doc.layer[a->doc.active].flags&PD_LAYER_LOCK){message(a,"Layer locked - unlock in layer options");a->blocked=true;}
                else if(!a->doc.layer[a->doc.active].visible){message(a,"Layer hidden - enable in Layers");a->blocked=true;}
                else {a->stroke=pd_begin(&a->doc);started=a->stroke;if(started)a->ink_starts++;memset(&a->perfect_stroke,0,sizeof(a->perfect_stroke));a->lastx=wx;a->lasty=wy;}
            }
            bool erase=(held&PD_ERASE)!=0||a->tool==1;
            if(a->stroke&&(started||pressed||wx!=a->lastx||wy!=a->lasty||erase!=a->last_erase)){if(a->lastx==-4097){memset(&a->perfect_stroke,0,sizeof(a->perfect_stroke));a->lastx=wx;a->lasty=wy;}
                a->partial=!a->redraw;
                a->dx0=a->lastx<wx?a->lastx:wx;a->dx1=(a->lastx>wx?a->lastx:wx)+1;
                a->dy0=a->lasty<wy?a->lasty:wy;a->dy1=(a->lasty>wy?a->lasty:wy)+1;
                unsigned bs=a->brush_size[erase?1:0];int low=(int)(bs/2),high=(int)bs-low-1;
                a->dx0-=low;a->dy0-=low;a->dx1+=high;a->dy1+=high;if(a->dx0<0)a->dx0=0;if(a->dy0<0)a->dy0=0;if(a->dx1>(int)a->doc.width)a->dx1=(int)a->doc.width;if(a->dy1>(int)a->doc.height)a->dy1=(int)a->doc.height;
                if(a->guides)memset(a->guide_damage,0,(pd_plane(&a->doc)/8));
                if(a->pixel_perfect&&!a->brush_pattern&&!erase&&bs==1)pd_perfect_line(&a->doc,&a->perfect_stroke,a->lastx,a->lasty,wx,wy,a->color,a->guides,a->guides?a->guide_damage:NULL,a->selection_valid?a->selection:NULL);
                else{memset(&a->perfect_stroke,0,sizeof(a->perfect_stroke));pd_pattern_line(&a->doc,a->lastx,a->lasty,wx,wy,erase?0:a->color,bs,a->brush_round[erase?1:0]!=0,a->guides,a->guides?a->guide_damage:NULL,a->selection_valid?a->selection:NULL,erase?0:a->brush_pattern);}
                a->lastx=wx;a->lasty=wy;a->last_erase=erase;a->redraw=true;
            }
        }else a->lastx=-4097;
    }
    a->touching=touch;
}
static void rect(PdFb *p,int w,int h,int x,int y,int rw,int rh,uint32_t c){
    for(int yy=y;yy<y+rh;yy++)for(int xx=x;xx<x+rw;xx++)if(xx>=0&&xx<w&&yy>=0&&yy<h)p[yy*w+xx]=pd_fb(c);
}
static void text_glyphs(PdFb *p,int w,int h,int x,int y,const char *s,uint32_t c,bool upper){
    for(;*s;s++,x+=6){unsigned char rows[7];char ch=*s;if(upper&&ch>='a'&&ch<='z')ch=(char)(ch-'a'+'A');glyphRows(ch,rows);
        for(int yy=0;yy<7;yy++)for(int xx=0;xx<5;xx++)if(rows[yy]&(1<<(4-xx)))rect(p,w,h,x+xx,y+yy,1,1,c);
    }
}
/* Labels and messages are drawn in capitals; text_raw keeps the case of names the user typed. */
static void text(PdFb *p,int w,int h,int x,int y,const char *s,uint32_t c){text_glyphs(p,w,h,x,y,s,c,true);}
static void text_raw(PdFb *p,int w,int h,int x,int y,const char *s,uint32_t c){text_glyphs(p,w,h,x,y,s,c,false);}
/* Shortens s to fit max_px (6 px per character) with a trailing "..". */
static void fit_name(char *out,size_t n,const char *s,int max_px){
    if(!n)return;size_t room=max_px>0?(size_t)((max_px+1)/6):0,len=strlen(s);if(room>n-1)room=n-1;
    if(len<=room){memcpy(out,s,len);out[len]=0;return;}
    if(room<2){memcpy(out,s,room);out[room]=0;return;}
    memcpy(out,s,room-2);out[room-2]='.';out[room-1]='.';out[room]=0;
}
/* One-colour pixel art drawn from rows of '#' and '.'. */
static void pixel_rows(PdFb *p,int w,int h,int x,int y,const char *const *rows,int n,uint32_t c){
    for(int yy=0;yy<n;yy++)for(int xx=0;rows[yy][xx];xx++)if(rows[yy][xx]=='#')rect(p,w,h,x+xx,y+yy,1,1,c);
}
/* 10x8 file-type icons: '#' is the type's tint and '+' its darker fill. */
static void type_icon(PdFb *p,int w,int h,int x,int y,unsigned icon){
    static const char *const rows[6][8]={
        {"#######...","#+++++##..","#+++++#+#.","#+++++####","#++++++++#","#++++++++#","#++++++++#","##########"},
        {"##########","#++++++++#","#+##+++++#","#+##++##+#","#+++++##+#","#+++##+++#","#+++##+++#","##########"},
        {"....######","....#++++#","..######+#","..#++++#+#","######+###","#++++#+#..","#++++###..","######...."},
        {"####..####","#++#..#++#","####..####","..........","####..####","#++#..#++#","####..####",".........."},
        {"##########","#++++++++#","#+++++##+#","#+++++##+#","#++#+++++#","#+###+#++#","##########",".........."},
        {".###......","#...#.....","##########","#++++++++#","#++++++++#","#++++++++#","#++++++++#","##########"}};
    static const uint32_t tint[6][2]={{0xb9cbd2,0x46545a},{0x79e0d3,0x2e5a55},{0xc3a6ff,0x4f4669},{0xff9ec7,0x6a4456},{0x8ec5ff,0x3a5370},{0xe9c46a,0x5e5232}};
    for(int yy=0;yy<8;yy++){const char *r=rows[icon][yy];for(int xx=0;r[xx];xx++)if(r[xx]!='.')rect(p,w,h,x+xx,y+yy,1,1,tint[icon][r[xx]=='#'?0:1]);}
}
static void button(PdFb *p,int w,int h,int x,int y,int bw,int bh,const char *label,bool primary){
    rect(p,w,h,x,y,bw,bh,UI_EDGE);rect(p,w,h,x+1,y+1,bw-2,bh-2,primary?UI_SELECTED:UI_PANEL);
    text(p,w,h,x+6,y+(bh-7)/2,label,UI_TEXT);
}
static void frame_box(PdFb *p,int w,int h,int x,int y,int bw,int bh,uint32_t edge,uint32_t fill){rect(p,w,h,x,y,bw,bh,edge);rect(p,w,h,x+1,y+1,bw-2,bh-2,fill);}
static void button_center(PdFb *p,int w,int h,Box b,const char *label,bool primary){
    frame_box(p,w,h,b.x,b.y,b.w,b.h,UI_EDGE,primary?UI_SELECTED:UI_PANEL);text(p,w,h,b.x+(b.w-((int)strlen(label)*6-1))/2,b.y+(b.h-7)/2,label,UI_TEXT);
}
static const char *const icon_play[7]={"##.....","####...","######.","#######","######.","####...","##....."};
static const char *const icon_pause[7]={"##..##.","##..##.","##..##.","##..##.","##..##.","##..##.","##..##."};
static const char *const icon_prev[7]={"...#..#","..##.##",".######","#######",".######","..##.##","...#..#"};
static const char *const icon_next[7]={"#..#...","##.##..","######.","#######","######.","##.##..","#..#..."};
static const char *const icon_onion[7]={"..#####..",".#.....#.","#..###..#","#.#...#.#","#..###..#",".#.....#.","..#####.."};
static void icon_button(PdFb *p,int w,int h,Box b,const char *const *rows,int iw,bool on){
    frame_box(p,w,h,b.x,b.y,b.w,b.h,UI_EDGE,on?UI_SELECTED:UI_PANEL);pixel_rows(p,w,h,b.x+(b.w-iw)/2,b.y+(b.h-7)/2,rows,7,UI_TEXT);
}
/* Label left, then - value + in the right 96 pixels, as in the mockup's stepper. */
static void stepper(PdFb *p,int w,int h,int x,int y,int sw,const char *label,const char *value){
    text(p,w,h,x,y+6,label,UI_MUTED);int bx=x+sw-96;button_center(p,w,h,(Box){bx,y,22,20},"-",false);
    text(p,w,h,bx+24+(48-((int)strlen(value)*6-1))/2,y+6,value,UI_TEXT);button_center(p,w,h,(Box){bx+74,y,22,20},"+",false);
}
static Box sheet_frame(PdFb *p,int w,int h,const PdApp *a,const char *title){
    Box b=sheet_box(a);frame_box(p,w,h,b.x,b.y,b.w,b.h,UI_EDGE,UI_BG);text(p,w,h,b.x+8,b.y+6,title,UI_ACCENT);text(p,w,h,b.x+b.w-14,b.y+6,"X",UI_TEXT);return b;
}
static void frame_sheet_draw(PdApp *a,PdFb *p,int w,int h){
    FrameSheet s=frame_sheet_layout(a);char label[40],range[16];unsigned ms=pd_frame_duration(&a->doc),f=pd_frame_index(&a->doc);
    snprintf(label,sizeof(label),"FRAME %u",f+1);sheet_frame(p,w,h,a,label);
    snprintf(label,sizeof(label),"%u MS",ms);stepper(p,w,h,s.ix,s.dur_y,s.iw,"DURATION",label);
    for(int i=0;i<4;i++){snprintf(label,sizeof(label),"%u",frame_presets[i]);button_center(p,w,h,s.presets[i],label,ms==frame_presets[i]);}
    button_center(p,w,h,s.scope[0],"THIS FRAME",!a->frame_apply_all);button_center(p,w,h,s.scope[1],"ALL FRAMES",a->frame_apply_all);
    int ti=pd_tag_at(&a->doc,f);const PdTag *tg=ti>=0?pd_tag(&a->doc,(unsigned)ti):NULL;
    frame_box(p,w,h,s.tag.x,s.tag.y,s.tag.w,s.tag.h,UI_EDGE,UI_PANEL);rect(p,w,h,s.tag.x+4,s.tag.y+6,8,8,tg?tg->color:UI_DEEP);
    if(tg){fit_name(label,sizeof(label),tg->name,s.tag.w-70);text_raw(p,w,h,s.tag.x+16,s.tag.y+6,label,UI_TEXT);
        snprintf(range,sizeof(range),"%u-%u",tg->from+1,tg->to+1);text(p,w,h,s.tag.x+s.tag.w-8-(int)strlen(range)*6,s.tag.y+6,range,UI_MUTED);}
    else text(p,w,h,s.tag.x+16,s.tag.y+6,"TAG NONE - TAP TO ADD",UI_MUTED);
    button_center(p,w,h,s.move[0],"MOVE <",false);button_center(p,w,h,s.move[1],"MOVE >",false);
}
/* Tag sheet, after the animation mockup's frame TAG row: name, inclusive range, loop direction, colour. */
static void tag_sheet_draw(PdApp *a,PdFb *p,int w,int h){
    const PdTag *t=pd_tag(&a->doc,(unsigned)a->tag_edit);if(!t)return;
    TagSheet s=tag_sheet_layout(a);char label[40];snprintf(label,sizeof(label),"TAG %u",(unsigned)a->tag_edit+1);sheet_frame(p,w,h,a,label);
    frame_box(p,w,h,s.name.x,s.name.y,s.name.w,s.name.h,UI_EDGE,UI_PANEL);text(p,w,h,s.name.x+4,s.name.y+6,"NAME",UI_MUTED);
    fit_name(label,sizeof(label),t->name,s.name.w-40);text_raw(p,w,h,s.name.x+32,s.name.y+6,label,UI_TEXT);
    snprintf(label,sizeof(label),"%u",t->from+1);stepper(p,w,h,s.ix,s.from_y,s.iw,"FROM",label);
    snprintf(label,sizeof(label),"%u",t->to+1);stepper(p,w,h,s.ix,s.to_y,s.iw,"TO",label);
    button_center(p,w,h,s.direction,tag_directions[t->direction],false);
    frame_box(p,w,h,s.color.x,s.color.y,s.color.w,s.color.h,UI_EDGE,UI_PANEL);rect(p,w,h,s.color.x+4,s.color.y+4,12,12,t->color);
    snprintf(label,sizeof(label),"%06X",(unsigned)t->color);text(p,w,h,s.color.x+22,s.color.y+6,label,UI_TEXT);
    button_center(p,w,h,s.remove,"REMOVE",false);button_center(p,w,h,s.done,"DONE",true);
}
/* Cel sheet: link the active cel to the previous frame, unlink it, or add a linked frame. */
static void cel_sheet_draw(PdApp *a,PdFb *p,int w,int h){
    CelSheet s=cel_sheet_layout(a);unsigned f=pd_frame_index(&a->doc),layer=a->doc.active,group=pd_cel_link(&a->doc,f,layer);char line[48],name[32];
    sheet_frame(p,w,h,a,"CEL");fit_name(name,sizeof(name),a->doc.names[layer],s.iw-36);
    snprintf(line,sizeof(line),"LAYER %s",name);text_raw(p,w,h,s.ix,s.info_y,line,UI_TEXT);
    if(group)snprintf(line,sizeof(line),"LINKED IN GROUP %u",group);else snprintf(line,sizeof(line),"NOT LINKED");
    text(p,w,h,s.ix,s.info_y+10,line,group?UI_ACCENT:UI_MUTED);
    button_center(p,w,h,s.link,"LINK WITH PREVIOUS",false);button_center(p,w,h,s.unlink,"UNLINK",false);
    button_center(p,w,h,s.new_linked,"NEW LINKED FRAME",false);
    text(p,w,h,s.ix,s.new_linked.y+26,f?"EDITING ONE EDITS ALL":"THE FIRST FRAME HAS NO PREVIOUS",UI_MUTED);
}
static void onion_sheet_draw(PdApp *a,PdFb *p,int w,int h){
    OnionSheet s=onion_sheet_layout(a);char label[24];sheet_frame(p,w,h,a,"ONION SKIN");
    button(p,w,h,s.toggle.x,s.toggle.y,s.toggle.w,s.toggle.h,a->onion_enabled?"ONION SKIN: ON":"ONION SKIN: OFF",a->onion_enabled);
    snprintf(label,sizeof(label),"%u",a->onion_behind);stepper(p,w,h,s.ix,s.behind_y,s.iw,"BEHIND",label);
    snprintf(label,sizeof(label),"%u",a->onion_ahead);stepper(p,w,h,s.ix,s.ahead_y,s.iw,"AHEAD",label);
    snprintf(label,sizeof(label),"%u%%",a->onion_opacity);text(p,w,h,s.ix,s.slider.y+4,label,UI_MUTED);
    int kx=s.slider.x+(int)(a->onion_opacity-10)*(s.slider.w-4)/80;rect(p,w,h,s.slider.x,s.slider.y+4,s.slider.w,6,UI_PANEL);rect(p,w,h,s.slider.x,s.slider.y+4,kx-s.slider.x,6,UI_SELECTED);rect(p,w,h,kx-2,s.slider.y,4,14,UI_TEXT);
    button(p,w,h,s.tint.x,s.tint.y,s.tint.w,s.tint.h,a->onion_tint?"TINT: RED / BLUE":"TINT: OFF",a->onion_tint);
}
static void frame_delete_draw(PdApp *a,PdFb *p,int w,int h){
    char label[32];frame_box(p,w,h,24,66,w-48,90,UI_EDGE,UI_BG);snprintf(label,sizeof(label),"DELETE FRAME %u?",pd_frame_index(&a->doc)+1);text(p,w,h,36,78,label,UI_TEXT);
    text(p,w,h,36,96,"CLEARS UNDO HISTORY",UI_MUTED);button(p,w,h,32,120,w/2-40,28,"DELETE",true);button(p,w,h,w/2+4,120,w/2-36,28,"CANCEL",false);
}
static void layers_tabs_draw(PdApp *a,PdFb *p,int w,int h){
    Timeline t=timeline_layout(a);button_center(p,w,h,t.tabs[0],"LAYERS",!a->layers_tab);button_center(p,w,h,t.tabs[1],"TIMELINE",a->layers_tab!=0);
}
static void timeline_draw(PdApp *a,PdFb *p,int w,int h){
    Timeline t=timeline_layout(a);timeline_clamp(a);char label[64];unsigned count=pd_frame_count(&a->doc),current=pd_frame_index(&a->doc),layers=layer_count(a);
    layers_tabs_draw(a,p,w,h);icon_button(p,w,h,t.play,a->playback.active?icon_pause:icon_play,7,a->playback.active);icon_button(p,w,h,t.onion,icon_onion,9,a->onion_enabled);
    snprintf(label,sizeof(label),"%u FRAME%s",count,count==1?"":"S");int lx=t.play.x-6-((int)strlen(label)*6-1);if(lx>t.tabs[1].x+t.tabs[1].w+4)text(p,w,h,lx,29,label,UI_MUTED);
    for(int c=0;c<t.cols;c++){unsigned f=a->timeline_first+(unsigned)c;if(f>=count)break;int ti=pd_tag_at(&a->doc,f);if(ti<0)continue;const PdTag *tg=pd_tag(&a->doc,(unsigned)ti);
        rect(p,w,h,t.gx+c*t.cw,t.tag_y,t.cw-1,10,tg->color);if(f==current)rect(p,w,h,t.gx+c*t.cw,t.tag_y,t.cw-1,2,UI_TEXT);}
    text(p,w,h,6,t.num_y+1,"FRAMES",UI_MUTED);bool show_marks=!a->cel_drag;
    unsigned rows=layers-a->timeline_layer_first<(unsigned)t.rows?layers-a->timeline_layer_first:(unsigned)t.rows;
    for(int c=0;c<t.cols;c++){unsigned f=a->timeline_first+(unsigned)c;if(f>=count)break;int x=t.gx+c*t.cw;bool on=f==current,marked=show_marks&&f<PD_FRAMES&&a->frame_mark[f];
        if(on)rect(p,w,h,x,t.num_y-1,t.cw-1,t.rows_y+(int)rows*t.row_h-t.num_y+1,UI_SELECTED);
        if(marked)rect(p,w,h,x,t.num_y+1,2,8,UI_WARN);
        if(a->timeline_reorder&&f==a->timeline_target)rect(p,w,h,x-1,t.num_y-1,2,t.rows_y+(int)rows*t.row_h-t.num_y+1,UI_WARN);
        snprintf(label,sizeof(label),f+1<100?"%u":"%02u",(f+1)%100);text(p,w,h,x+(t.cw-((int)strlen(label)*6-1))/2,t.num_y+1,label,marked?UI_WARN:on?UI_TEXT:UI_MUTED);}
    for(unsigned r=0;r<rows;r++){int found=timeline_slot(a,a->timeline_layer_first+r);if(found<0)break;unsigned slot=(unsigned)found;int y=t.rows_y+(int)r*t.row_h,cy=y+(t.row_h-2)/2;bool active=slot==a->doc.active;
        frame_box(p,w,h,4,y,t.name_w,t.row_h-2,active?UI_ACCENT:UI_EDGE,active?UI_SELECTED:UI_PANEL);
        fit_name(label,sizeof(label),a->doc.names[slot],t.name_w-10);text_raw(p,w,h,9,y+(t.row_h-9)/2,label,a->doc.layer[slot].visible?UI_TEXT:UI_MUTED);
        for(int c=0;c<t.cols;c++){unsigned f=a->timeline_first+(unsigned)c;if(f>=count)break;int x=t.gx+c*t.cw;
            if(pd_frame_has_cel(&a->doc,f,slot))rect(p,w,h,x+3,cy-3,t.cw-7,7,f==current?UI_TEXT:UI_ACCENT);else rect(p,w,h,x+t.cw/2-1,cy-1,2,2,UI_EDGE);
            if(active&&f==current){rect(p,w,h,x,y,t.cw-1,1,UI_WARN);rect(p,w,h,x,y+t.row_h-3,t.cw-1,1,UI_WARN);rect(p,w,h,x,y,1,t.row_h-2,UI_WARN);rect(p,w,h,x+t.cw-2,y,1,t.row_h-2,UI_WARN);}}}
    if(a->cel_drag){
        int sc=(int)a->cel_src_frame-(int)a->timeline_first;
        if(sc>=0&&sc<t.cols&&a->cel_src_row<rows){int x=t.gx+sc*t.cw,y=t.rows_y+(int)a->cel_src_row*t.row_h;frame_box(p,w,h,x,y,t.cw-1,t.row_h-2,UI_WARN,UI_PANEL);}
        if(a->cel_drag_valid&&a->cel_dst_row<rows){int dc=(int)a->cel_dst_frame-(int)a->timeline_first;if(dc>=0&&dc<t.cols){int x=t.gx+dc*t.cw,y=t.rows_y+(int)a->cel_dst_row*t.row_h;frame_box(p,w,h,x,y,t.cw-1,t.row_h-2,UI_ACCENT,UI_SELECTED);}}
    }
    if(layers>(unsigned)t.rows){int track=t.rows*t.row_h-2,thumb=track*t.rows/(int)layers,pos=(int)a->timeline_layer_first*(track-thumb)/(int)(layers-(unsigned)t.rows);rect(p,w,h,w-7,t.rows_y,4,track,UI_PANEL);rect(p,w,h,w-7,t.rows_y+pos,4,thumb,UI_ACCENT);}
    size_t used=pd_frame_memory(&a->doc),budget=pd_frame_budget(&a->doc);if(show_marks&&a->frame_mark_count)snprintf(label,sizeof(label),"%u KB OF %u KB FOR FRAMES  %u SEL",(unsigned)((used+1023)/1024),(unsigned)(budget/1024),a->frame_mark_count);else snprintf(label,sizeof(label),"%u KB OF %u KB FOR FRAMES",(unsigned)((used+1023)/1024),(unsigned)(budget/1024));text(p,w,h,6,t.meter_y,label,UI_MUTED);
    int mw=w-12,fill=budget>=1024?(int)(used/1024*(size_t)mw/(budget/1024)):0;if(fill<2)fill=2;if(fill>mw)fill=mw;rect(p,w,h,6,t.meter_y+10,mw,4,UI_DEEP);rect(p,w,h,6,t.meter_y+10,fill,4,UI_ACCENT);
    static const char *labels[4]={"+ FRAME","DUPE","DELETE","FRAME..."};for(int i=0;i<4;i++)button_center(p,w,h,t.foot[i],labels[i],i==0);
    if(a->frame_sheet)frame_sheet_draw(a,p,w,h);if(a->onion_sheet)onion_sheet_draw(a,p,w,h);if(a->frame_delete)frame_delete_draw(a,p,w,h);
    if(a->tag_sheet)tag_sheet_draw(a,p,w,h);if(a->cel_sheet)cel_sheet_draw(a,p,w,h);
}
static void strip_draw(PdApp *a,PdFb *p,int w,int h){
    Strip s=strip_layout(a);if(!s.h)return;unsigned n=pd_frame_count(&a->doc),f=pd_frame_index(&a->doc);char label[16];
    rect(p,w,h,0,s.y,w,s.h,0x121a1e);
    icon_button(p,w,h,s.items[0],icon_prev,7,false);icon_button(p,w,h,s.items[2],icon_next,7,false);
    icon_button(p,w,h,s.items[3],a->playback.active?icon_pause:icon_play,7,a->playback.active);icon_button(p,w,h,s.items[4],icon_onion,9,a->onion_enabled);
    snprintf(label,sizeof(label),"%u/%u",f+1,n);Box c=s.items[1];frame_box(p,w,h,c.x,c.y,c.w,c.h,UI_EDGE,UI_PANEL);
    int lw=(int)strlen(label)*6-1;if(lw>c.w-2)snprintf(label,sizeof(label),"%u",f+1),lw=(int)strlen(label)*6-1;text(p,w,h,c.x+(c.w-lw)/2,c.y+(c.h-7)/2,label,UI_TEXT);
    for(unsigned i=0;i<=n;i++){unsigned k=i<n?i:f;if(i<n&&k==f)continue;int x0=s.tx+(int)(k*(unsigned)s.tw/n),x1=s.tx+(int)((k+1)*(unsigned)s.tw/n),tw=x1-x0;
        bool near=a->onion_enabled&&k!=f&&((k<f&&f-k<=a->onion_behind)||(k>f&&k-f<=a->onion_ahead));
        int gap=tw>=3?1:0,width=tw-2*gap;if(width<1)width=1;if(k==f&&width<2)width=2;
        rect(p,w,h,x0+gap,s.y+4,width,s.h-8,k==f?UI_ACCENT:near?UI_EDGE:UI_PANEL);}
}
static void file_type_dropdown_draw(PdApp *a,PdFb *p,int w,int h){
    PdDropdown *m=&a->file_type_dropdown;if(!m->open)return;
    int total=m->header_h+m->row_h*m->visible_rows+1;rect(p,w,h,m->x+2,m->y+3,m->w,total,0x111820);rect(p,w,h,m->x,m->y,m->w,total,UI_ACCENT);rect(p,w,h,m->x+1,m->y+1,m->w-2,total-2,UI_DEEP);
    for(int row=0;row<m->visible_rows;row++){unsigned i=m->first+(unsigned)row;if(i>=m->count)break;int yy=m->y+m->header_h+row*m->row_h;bool on=i==m->selected;
        if(on)rect(p,w,h,m->x+1,yy,m->w-2,m->row_h,UI_SELECTED);else if(row)rect(p,w,h,m->x+4,yy,m->w-8,1,UI_LINE);
        type_icon(p,w,h,m->x+5,yy+(m->row_h-8)/2,filter_icons[i]);text(p,w,h,m->x+20,yy+(m->row_h-7)/2,filter_names[i],UI_TEXT);
        text(p,w,h,m->x+20+10*6,yy+(m->row_h-7)/2,filter_exts[i],on?UI_TEXT:UI_MUTED);}
}
static void community_dropdown_draw(PdApp *a,PdFb *p,int w,int h){
    PdDropdown *m=&a->community_dropdown;if(!m->open)return;static const char *names[]={"CACHED","RECENT","FAVORITES","MY UPLOADS"};
    int total=m->header_h+m->row_h*m->visible_rows;rect(p,w,h,m->x+2,m->y+3,m->w,total,0x111820);rect(p,w,h,m->x,m->y,m->w,total,UI_ACCENT);rect(p,w,h,m->x+1,m->y+1,m->w-2,total-2,UI_BG);
    char header[40];unsigned pages=m->count>(unsigned)m->visible_rows?m->count-(unsigned)m->visible_rows+1:1;snprintf(header,sizeof(header),"CHOOSE VIEW  %u/%u",m->first+1,pages);text(p,w,h,m->x+7,m->y+5,header,UI_ACCENT);
    int max_chars=(m->w-22)/6;if(max_chars<1)max_chars=1;if(max_chars>31)max_chars=31;
    for(int row=0;row<m->visible_rows;row++){unsigned i=m->first+(unsigned)row;if(i>=m->count)break;int yy=m->y+m->header_h+row*m->row_h;rect(p,w,h,m->x+2,yy,m->w-4,m->row_h-1,i==m->selected?UI_SELECTED:UI_PANEL);char label[32];snprintf(label,sizeof(label),"%.*s",max_chars,names[i]);text(p,w,h,m->x+8,yy+(m->row_h-7)/2,label,i==m->selected?UI_TEXT:UI_MUTED);if(i==m->selected)text(p,w,h,m->x+m->w-16,yy+(m->row_h-7)/2,">",UI_ACCENT);}
    if(m->first)text(p,w,h,m->x+m->w-13,m->y+5,"^",UI_TEXT);if(m->first+(unsigned)m->visible_rows<m->count)text(p,w,h,m->x+m->w-13,m->y+total-9,"v",UI_TEXT);
}
static void layer_blend_dropdown_draw(PdApp *a,PdFb *p,int w,int h){
    PdDropdown *m=&a->layer_blend_dropdown;if(!m->open)return;static const char *names[]={"NORMAL","MULTIPLY","SCREEN","OVERLAY"};
    int total=m->header_h+m->row_h*m->visible_rows;
    rect(p,w,h,m->x-2,m->y-2,m->w+4,total+4,UI_EDGE);rect(p,w,h,m->x,m->y,m->w,total,UI_BG);
    text(p,w,h,m->x+6,m->y+3,"CHOOSE BLEND MODE",UI_ACCENT);
    for(int row=0;row<m->visible_rows;row++){
        unsigned item=m->first+(unsigned)row;if(item>=m->count)break;
        int y=m->y+m->header_h+row*m->row_h;
        rect(p,w,h,m->x+2,y,m->w-4,m->row_h-1,item==m->selected?UI_SELECTED:UI_PANEL);
        if(item==m->selected)rect(p,w,h,m->x+7,y+7,4,4,UI_ACCENT);
        text(p,w,h,m->x+18,y+(m->row_h-7)/2,names[item],UI_TEXT);
    }
}
static uint32_t grid_color(uint32_t c){return ((c>>1)&0x7f7f7f)+((((c>>16)&255)+((c>>8)&255)+(c&255)<384)?0x808080:0);}
static uint32_t displayed(uint32_t p,int x,int y){unsigned a=p>>24,bg=((x/8+y/8)&1)?205:238;
    if(a==255)return p&0xffffff;if(!a)return bg*0x010101;
    unsigned r=(((p>>16)&255)*a+bg*(255-a)+127)/255,g=(((p>>8)&255)*a+bg*(255-a)+127)/255,b=((p&255)*a+bg*(255-a)+127)/255;return (r<<16)|(g<<8)|b;
}
static bool onion_ready(const PdApp *a){return a->onion_enabled&&a->onion&&!a->playback.active&&!a->confirm&&a->onion_plane==pd_plane(&a->doc)&&pd_frame_count(&a->doc)>=2;}
static uint32_t onion_displayed(PdApp *a,uint32_t p,int x,int y);
static uint32_t canvas_displayed(PdApp *a,uint32_t p,int x,int y){return a->page||!onion_ready(a)?displayed(p,x,y):onion_displayed(a,p,x,y);}
/* Ghosts sit under the current frame and never reach pixels, sampling or files. */
static uint32_t onion_displayed(PdApp *a,uint32_t p,int x,int y){
    if((p>>24)==255)return p&0xffffff;
    unsigned i=(unsigned)y*a->doc.stride+(unsigned)x,background=displayed(0,x,y),alphas[2],colors[2]={0xdf5555,0x558bdf};
    if(a->onion_tint){alphas[0]=a->onion[i]*a->onion_opacity/100u;alphas[1]=a->onion[a->onion_plane+i]*a->onion_opacity/100u;}
    else {const uint32_t *ghost=(const uint32_t *)a->onion;for(unsigned k=0;k<2;k++){uint32_t g=ghost[k*a->onion_plane+i];alphas[k]=(g>>24)*a->onion_opacity/100u;colors[k]=g&0xffffffu;}}
    for(unsigned k=0;k<2;k++){unsigned c=0;for(unsigned shift=0;shift<24;shift+=8)c|=(((((colors[k]>>shift)&255)*alphas[k]+((background>>shift)&255)*(255-alphas[k])+127)/255)<<shift);background=c;}
    unsigned alpha=p>>24,result=0;for(unsigned shift=0;shift<24;shift+=8)result|=(((((p>>shift)&255)*alpha+((background>>shift)&255)*(255-alpha)+127)/255)<<shift);return result;
}
static void brush_tip_preview(PdFb *p,int w,int h,int x,int y,int bw,int bh,unsigned size,unsigned round,unsigned pattern,uint32_t color){
    rect(p,w,h,x-1,y-1,bw+2,bh+2,UI_EDGE);
    for(int yy=0;yy<bh;yy++)for(int xx=0;xx<bw;xx++)p[(y+yy)*w+x+xx]=pd_fb((((xx/8)+(yy/8))&1)?0xcdcdcd:0xeeeeee);
    if(size<1)size=1;if(size>16)size=16;if(size>(unsigned)bh-2)size=(unsigned)bh-2;
    int left=x+(bw-(int)size)/2,top=y+(bh-(int)size)/2;
    for(unsigned yy=0;yy<size;yy++)for(unsigned xx=0;xx<size;xx++){
        int dx=(int)(xx*2+1)-(int)size,dy=(int)(yy*2+1)-(int)size;
        bool ink=!round||dx*dx+dy*dy<=(int)(size*size);
        if(pattern==1&&!((xx+yy)&1))ink=false;
        else if(pattern==2&&(xx&1))ink=false;
        else if(pattern==3&&(xx&1)&&(yy&1))ink=false;
        if(ink)p[(top+(int)yy)*w+left+(int)xx]=pd_fb(displayed(color,x+xx,y+yy));
    }
}
static void render_layer_top(PdApp *a,PdFb *p,int w,int h){
    char label[96];rect(p,w,h,0,0,w,h,UI_BG);rect(p,w,h,0,0,w,20,0x11191d);
    text(p,w,h,8,6,"LAYER PREVIEW",UI_ACCENT);
    text(p,w,h,w-8-(int)strlen("GRIP HOLD + DRAG")*6,6,"GRIP HOLD + DRAG",UI_MUTED);
    int margin=8,gap=8,panel_w=(w-margin*3-gap)/2,preview_top=36,footer=34,image_h=h-preview_top-footer;
    int image=panel_w-8;if(image_h<image)image=image_h;if(image<32)image=32;
    unsigned dw=a->doc.width,dh=a->doc.height,max=dw>dh?dw:dh;
    int mw=(int)(dw*(unsigned)image/max),mh=(int)(dh*(unsigned)image/max);if(mw<1)mw=1;if(mh<1)mh=1;
    const uint32_t *sources[2]={pd_image(&a->doc),pd_pixels(&a->doc.layer[a->doc.active])};
    const char *labels[2]={"COMPOSITE","ACTIVE LAYER"};
    for(unsigned panel=0;panel<2;panel++){
        int x=margin+(int)panel*(panel_w+gap);
        text(p,w,h,x+2,25,labels[panel],panel?UI_TEXT:UI_MUTED);
        rect(p,w,h,x-1,preview_top-1,panel_w+2,image_h+2,UI_EDGE);
        for(int yy=0;yy<image_h;yy++)for(int xx=0;xx<panel_w;xx++)p[(preview_top+yy)*w+x+xx]=pd_fb(UI_BG);
        int left=x+(panel_w-mw)/2,top=preview_top+(image_h-mh)/2;
        for(int y=0;y<mh;y++)for(int xx=0;xx<mw;xx++){
            int wx=xx*(int)dw/mw,wy=y*(int)dh/mh;
            p[(top+y)*w+left+xx]=pd_fb(displayed(sources[panel][wy*a->doc.stride+wx],wx,wy));
        }
        if(panel&&!a->doc.layer[a->doc.active].visible)text(p,w,h,x+6,preview_top+6,"HIDDEN",UI_ACCENT);
    }
    PdLayer *l=&a->doc.layer[a->doc.active];static const char *modes[]={"NORMAL","MULTIPLY","SCREEN","OVERLAY"};
    int chars=(w-16)/6;if(chars>72)chars=72;
    snprintf(label,sizeof(label),"ACTIVE: %s  %s %u%%%s",a->doc.names[a->doc.active],modes[l->blend],(unsigned)l->opacity*100/255,(l->flags&PD_LAYER_LOCK)?"  LOCKED":(l->flags&PD_LAYER_ALPHA_LOCK)?"  ALPHA LOCK":"");
    size_t meta=strlen(label);if(meta>(size_t)chars)label[chars]=0;text(p,w,h,8,h-27,label,UI_TEXT);
    const char *hint="TOUCH TO SELECT  /  HOLD TO REORDER";
    snprintf(label,sizeof(label),"%.*s",chars,hint);text(p,w,h,8,h-15,label,UI_MUTED);
}
static void render_palette(PdApp *a,PdFb *p,int w,int h){
    int top=palette_content_y(a),fh=palette_view_height(a),pitch=palette_row_pitch(a),tile=palette_tile(a),cols=(int)palette_cols(a);
    unsigned first=(unsigned)(a->palette_scroll[1]/pitch)*cols;
    unsigned end=first+(unsigned)((fh+pitch-1)/pitch+1)*cols;if(end>a->swatch_count)end=a->swatch_count;
    for(unsigned i=first;i<end;i++){
        int px=8+(int)(i%(unsigned)cols)*pitch,py=top+(int)(i/(unsigned)cols)*pitch-a->palette_scroll[1];
        if(py+tile<=top||py>=top+fh)continue;
        bool moving=a->swatch_drag&&(unsigned)a->swatch_target==i;uint32_t color=moving?a->swatches[a->swatch_hold]:a->swatches[i];
        for(int yy=py;yy<py+tile;yy++)if(yy>=top&&yy<top+fh)for(int xx=px;xx<px+tile;xx++){
            bool edge=xx<px+2||xx>=px+tile-2||yy<py+2||yy>=py+tile-2;p[yy*w+xx]=pd_fb(edge?(moving||i==a->swatch_selected?UI_ACCENT:UI_EDGE):displayed(color,xx,yy));
        }
        if(moving&&py>=top&&py+14<top+fh){rect(p,w,h,px+2,py+2,26,11,UI_BG);text(p,w,h,px+4,py+4,"MOVE",UI_TEXT);}
    }
    int total=((int)a->swatch_count+cols-1)/cols*pitch;if(total>fh){int thumb=fh*fh/total;if(thumb<8)thumb=8;int sy=top+a->palette_scroll[1]*(fh-thumb)/(total-fh);rect(p,w,h,w-4,top,3,fh,UI_PANEL);rect(p,w,h,w-4,sy,3,thumb,UI_ACCENT);}
}
static void palette_swatch(PdFb *p,int w,int h,int x,int y,int sw,int sh,uint32_t color,bool selected){
    rect(p,w,h,x-1,y-1,sw+2,sh+2,selected?UI_ACCENT:UI_EDGE);
    for(int yy=y;yy<y+sh;yy++)for(int xx=x;xx<x+sw;xx++)
        if(xx>=0&&xx<w&&yy>=0&&yy<h)p[yy*w+xx]=pd_fb(displayed(color,xx,yy));
}
/* Filmstrip thumbnails: a few round-robin slots, so a short playback loop is sampled once. */
enum { FILM_SLOTS=12,FILM_SIZE=26 };
typedef struct { uint64_t key; unsigned w,h; uint32_t px[FILM_SIZE*FILM_SIZE]; } FilmThumb;
static const FilmThumb *film_thumb(PdApp *a,unsigned frame,unsigned size){
    if(!a->film){a->film=calloc(FILM_SLOTS,sizeof(FilmThumb));if(!a->film)return NULL;}
    FilmThumb *slots=a->film;uint64_t key=1469598103934665603ULL;
    uint64_t parts[6]={frame,size,pd_frames_visual_generation(&a->doc),pd_frames_generation(&a->doc),a->doc.revision,a->doc.enabled};
    for(unsigned i=0;i<6;i++)key=(key^parts[i])*1099511628211ULL;
    for(unsigned i=0;i<8;i++){const PdLayer *l=&a->doc.layer[i];key=(key^a->doc.order[i]^(unsigned)l->opacity<<8^(unsigned)l->visible<<16)*1099511628211ULL;}
    key|=1;for(unsigned i=0;i<FILM_SLOTS;i++)if(slots[i].key==key)return &slots[i];
    FilmThumb *t=&slots[a->film_next++%FILM_SLOTS];unsigned dw=a->doc.width,dh=a->doc.height,max=dw>dh?dw:dh;
    t->w=dw*size/max;t->h=dh*size/max;if(!t->w)t->w=1;if(!t->h)t->h=1;t->key=pd_frame_sample(&a->doc,frame,t->px,t->w,t->h)?key:0;return t->key?t:NULL;
}
/* Timeline upper screen, after the mockup's topAnimation: current frame at size, details and a five-frame strip. */
static void render_animation_top(PdApp *a,PdFb *p,int w,int h){
    char label[64],name[48];unsigned current=pd_frame_index(&a->doc),count=pd_frame_count(&a->doc);bool big=w>256;
    rect(p,w,h,0,0,w,h,UI_BG);rect(p,w,h,0,0,w,16,0x121a1e);
    snprintf(label,sizeof(label),"F %u/%u",current+1,count);int tag_w=(int)strlen(label)*6-1,tx=w-tag_w-12;
    frame_box(p,w,h,tx,3,tag_w+8,11,UI_ACCENT,0x121a1e);text(p,w,h,tx+4,5,label,UI_ACCENT);
    const char *project=strrchr(a->project,'/');project=project?project+1:a->project;fit_name(name,sizeof(name),project[0]?project:"Pixel Doodle",tx-16);text_raw(p,w,h,5,5,name,UI_TEXT);
    int pv=big?160:112,px=8,py=22;unsigned dw=a->doc.width,dh=a->doc.height,max=dw>dh?dw:dh;
    int mw=(int)(dw*(unsigned)pv/max),mh=(int)(dh*(unsigned)pv/max);if(mw<1)mw=1;if(mh<1)mh=1;
    frame_box(p,w,h,px-1,py-1,pv+2,pv+2,UI_EDGE,UI_BG);int left=px+(pv-mw)/2,top=py+(pv-mh)/2;
    bool ghosts=a->onion_sheet&&onion_ready(a);
    for(int y=0;y<mh;y++){int wy=y*(int)dh/mh;for(int x=0;x<mw;x++){int wx=x*(int)dw/mw;uint32_t c=pd_image(&a->doc)[wy*a->doc.stride+wx];p[(top+y)*w+left+x]=pd_fb(ghosts?onion_displayed(a,c,wx,wy):displayed(c,wx,wy));}}
    int ix=px+pv+12;const char *names[4]={"FRAME","DURATION","ONION","PLAYBACK"};char values[4][32];
    snprintf(values[0],32,"%u OF %u",current+1,count);snprintf(values[1],32,"%u MS",pd_frame_duration(&a->doc));
    if(a->onion_enabled)snprintf(values[2],32,"%u BEHIND %u AHEAD",a->onion_behind,a->onion_ahead);else snprintf(values[2],32,"OFF");
    snprintf(values[3],32,a->playback.active?"PLAYING":"STOPPED");
    for(int i=0;i<4;i++){text(p,w,h,ix,py+2+i*20,names[i],UI_MUTED);fit_name(label,sizeof(label),values[i],w-ix-6);text(p,w,h,ix,py+11+i*20,label,i==3&&a->playback.active?UI_ACCENT:UI_TEXT);}
    int ts=big?FILM_SIZE:20,fy=py+pv-ts;
    for(int i=0;i<5;i++){int x=ix+i*(ts+3);if(x+ts>w-4)break;unsigned f=(current+count*2+(unsigned)i-2)%count;bool on=i==2;
        frame_box(p,w,h,x-1,fy-1,ts+2,ts+2,on?UI_ACCENT:UI_LINE,UI_BG);if(count<2&&!on)continue;
        const FilmThumb *t=film_thumb(a,f,(unsigned)ts);if(!t)continue;int ox=x+(ts-(int)t->w)/2,oy=fy+(ts-(int)t->h)/2;
        for(unsigned y=0;y<t->h;y++)for(unsigned xx=0;xx<t->w;xx++)p[(oy+(int)y)*w+ox+(int)xx]=pd_fb(displayed(t->px[y*t->w+xx],(int)xx*2,(int)y*2));}
    rect(p,w,h,0,h-16,w,16,0x121a1e);
    const char *hint=a->playback.active?"PLAYING - ANY TOUCH OR BUTTON STOPS":a->frame_sheet?(a->frame_apply_all?"DURATION CHANGES ALL FRAMES":"DURATION CHANGES THIS FRAME"):
        a->onion_sheet?(a->onion_tint?"RED IS BEHIND, BLUE IS AHEAD":"NEIGHBOURS IN THEIR OWN COLORS"):
        a->cel_drag?(a->cel_drag_valid?"RELEASE TO MOVE THE CEL":"DRAG ONTO ANOTHER CEL"):
        a->timeline_reorder?(a->frame_mark_count>1?"RELEASE TO PLACE THE MARKED FRAMES":"RELEASE TO PLACE THE FRAME"):
        a->notice[0]?a->notice:a->frame_mark_count?"HOLD A MARKED NUMBER TO MOVE":"L+TAP NUMBER MARKS - HOLD TO MOVE";
    fit_name(label,sizeof(label),hint,w-10);text(p,w,h,5,h-11,label,a->playback.active?UI_ACCENT:UI_MUTED);
}
static void render_palette_top(PdApp *a,PdFb *p,int w,int h){
    char label[96];uint32_t active=*palette_color_target(a);bool compact=w<340;
    int outer=compact?12:16;
    rect(p,w,h,0,0,w,h,UI_BG);rect(p,w,h,0,0,w,20,0x11191d);
    text(p,w,h,outer,6,a->palette_edit?"PALETTE EDIT":a->color_tab?"PALETTE":"PICKER",UI_ACCENT);
    if(a->palette_edit)snprintf(label,sizeof(label),"SWATCH %u",a->swatch_selected+1);
    else snprintf(label,sizeof(label),"%s TARGET",a->palette_target?"BACKGROUND":"FOREGROUND");
    text(p,w,h,w-outer-(int)strlen(label)*6,6,label,UI_MUTED);

    int active_w=compact?88:148,active_x=outer,active_y=29,fill_y=39,fill_h=compact?64:92;
    text(p,w,h,active_x,active_y,"ACTIVE COLOR",UI_MUTED);
    palette_swatch(p,w,h,active_x,fill_y,active_w,fill_h,active,true);
    int info_y=fill_y+fill_h+6;
    snprintf(label,sizeof(label),"#%06X  A%u",(unsigned)(active&0xffffff),(unsigned)(active>>24));text(p,w,h,active_x,info_y,label,UI_TEXT);
    snprintf(label,sizeof(label),"H%u  S%u  V%u",a->hue*359/255,a->saturation*100/255,a->value*100/255);text(p,w,h,active_x,info_y+13,label,UI_TEXT);
    snprintf(label,sizeof(label),"R%u  G%u  B%u",(unsigned)((active>>16)&255),(unsigned)((active>>8)&255),(unsigned)(active&255));
    text(p,w,h,active_x,info_y+28,label,UI_MUTED);
    if(!compact){
        text(p,w,h,active_x,info_y+43,a->palette_picker_active?"PICKER PREVIEW":"COMMITTED COLOR",UI_ACCENT);
    }

    int rx=active_x+active_w+16,rw=w-rx-outer,pair_gap=8,pair_w=(rw-pair_gap)/2,pair_y=29;
    text(p,w,h,rx,pair_y,"FOREGROUND",a->palette_target==0?UI_ACCENT:UI_MUTED);
    text(p,w,h,rx+pair_w+pair_gap,pair_y,"BACKGROUND",a->palette_target==1?UI_ACCENT:UI_MUTED);
    palette_swatch(p,w,h,rx,pair_y+11,pair_w,compact?30:50,a->color,a->palette_target==0);
    palette_swatch(p,w,h,rx+pair_w+pair_gap,pair_y+11,pair_w,compact?30:50,a->background,a->palette_target==1);
    snprintf(label,sizeof(label),"#%06X",(unsigned)(a->color&0xffffff));text(p,w,h,rx,pair_y+(compact?46:66),label,a->palette_target==0?UI_TEXT:UI_MUTED);
    snprintf(label,sizeof(label),"#%06X",(unsigned)(a->background&0xffffff));text(p,w,h,rx+pair_w+pair_gap,pair_y+(compact?46:66),label,a->palette_target==1?UI_TEXT:UI_MUTED);

    int lower_y=compact?91:125,selected_x=rx,selected_w=compact?28:34,recent_x=compact?rx+68:rx+78;
    text(p,w,h,selected_x,lower_y,"SWATCH",UI_MUTED);
    if(a->swatch_count){
        unsigned selected=a->swatch_selected<a->swatch_count?a->swatch_selected:0;
        snprintf(label,sizeof(label),"%u/%u",selected+1,a->swatch_count);int right=selected_x+selected_w+4;
        text(p,w,h,right,lower_y,label,UI_TEXT);
        palette_swatch(p,w,h,selected_x,lower_y+11,selected_w,selected_w,a->swatches[selected],true);
        if(compact)snprintf(label,sizeof(label),"#%06X",(unsigned)(a->swatches[selected]&0xffffff));
        else snprintf(label,sizeof(label),"#%06X  A%u",(unsigned)(a->swatches[selected]&0xffffff),(unsigned)(a->swatches[selected]>>24));
        text(p,w,h,selected_x,lower_y+15+selected_w,label,UI_TEXT);
    }

    text(p,w,h,recent_x,lower_y,"RECENT",UI_MUTED);
    int cols=compact?4:8,gap=2,recent_width=w-outer-recent_x;
    int tile=(recent_width-gap*(cols-1))/cols;if(tile>(compact?14:16))tile=compact?14:16;if(tile<10)tile=10;
    int recent_y=lower_y+11;
    for(unsigned i=0;i<a->recent_count&&i<(unsigned)(cols*2);i++){
        int xx=recent_x+(int)(i%(unsigned)cols)*(tile+gap),yy=recent_y+(int)(i/(unsigned)cols)*(tile+gap);
        palette_swatch(p,w,h,xx,yy,tile,tile,a->recent[i],false);
    }
    if(!a->recent_count)text(p,w,h,recent_x,recent_y,"NO RECENT COLORS",UI_MUTED);
    snprintf(label,sizeof(label),"PALETTE %u COLORS  CURRENT %u",a->swatch_count,a->swatch_count?a->swatch_selected+1:0);
    text(p,w,h,outer,lower_y+(compact?60:72),label,UI_MUTED);

    int notice_y=h-17,max_chars=(w-2*outer)/6;if(max_chars>80)max_chars=80;
    snprintf(label,sizeof(label),"%.*s",max_chars,a->notice);text(p,w,h,outer,notice_y,label,UI_TEXT);
}
/* Original 8x8 UI glyphs, rendered at 2x for native stylus screens. */
static void icon(PdFb *p,int w,int h,int x,int y,unsigned id,uint32_t c){
    static const unsigned char bits[][8]={
        {0x20,0x60,0xfe,0x61,0x21,0x01,0x02,0x1c}, /* undo */
        {0x04,0x06,0x7f,0x86,0x84,0x80,0x40,0x38}, /* redo */
        {0x3c,0x7e,0xdb,0xff,0xdb,0xf8,0x70,0x20}, /* palette */
        {0x18,0x66,0x81,0x66,0x99,0x66,0x99,0x66}, /* layers */
        {0x18,0x24,0x7e,0x81,0xff,0x99,0x81,0x7e}, /* toolbox */
        {0,0x7e,0x7e,0,0x7e,0x7e,0,0}, /* menu */
        {0x0c,0x1e,0x3a,0x74,0xe8,0xd0,0xa0,0xc0}, /* pencil */
        {0x1c,0x22,0x41,0x82,0x44,0x28,0x10,0x7e}, /* eraser */
        {0x06,0x0f,0x1e,0x2c,0x48,0x50,0x60,0xc0}, /* sampler */
        {0x10,0x30,0x7e,0xff,0x7e,0x30,0x10,0}, /* back */
        {0x24,0x24,0xff,0x24,0x24,0xff,0x24,0x24}, /* guides lattice */
        {0x20,0x50,0x88,0x44,0x2a,0x12,0x01,0x03}, /* fill bucket and drop */
        {0xaa,0x81,0,0x81,0,0x81,0,0x55}, /* rectangular selection */
        {0x3c,0x42,0x81,0x81,0x42,0x3c,0x08,0x10}, /* lasso */
        {0xff,0x81,0x81,0x99,0x99,0x81,0x81,0xff}, /* replace */
        {0,0x18,0x18,0x7e,0x7e,0x18,0x18,0}, /* add */
        {0,0,0,0x7e,0x7e,0,0,0}, /* subtract */
        {0x7c,0x44,0x5f,0x51,0x71,0x11,0x11,0x1f}, /* copy */
        {0x42,0x24,0x18,0x18,0x24,0x5a,0x66,0x42}, /* cut */
        {0x18,0x7e,0x42,0x5a,0x42,0x5a,0x42,0x7e}, /* paste */
        {0x81,0x42,0x24,0x18,0x18,0x24,0x42,0x81}, /* deselect */
        {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80}, /* line */
        {0xff,0x81,0x81,0x81,0x81,0x81,0x81,0xff}, /* rectangle */
        {0x3c,0x42,0x81,0x81,0x81,0x81,0x42,0x3c} /* ellipse */
    };
    for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)if(bits[id][yy]&(128>>xx))rect(p,w,h,x+xx*2,y+yy*2,2,2,c);
}
static void icon_coverage(PdFb *p,int w,int h,int x,int y,const unsigned short rows[4][16],uint32_t c,uint32_t bg){
    for(int yy=0;yy<16;yy++)for(int xx=0;xx<16;xx++){
        unsigned packed=rows[yy/4][(yy%4)*4+xx/4],coverage=(packed>>(12-4*(xx%4)))&15;
        if(!coverage)continue;
        if(coverage==15){rect(p,w,h,x+xx,y+yy,1,1,c);continue;}
        unsigned alpha=coverage*17;
        unsigned r=(((c>>16)&255)*alpha+((bg>>16)&255)*(255-alpha)+127)/255;
        unsigned g=(((c>>8)&255)*alpha+((bg>>8)&255)*(255-alpha)+127)/255;
        unsigned b=((c&255)*alpha+(bg&255)*(255-alpha)+127)/255;
        rect(p,w,h,x+xx,y+yy,1,1,(r<<16)|(g<<8)|b);
    }
}
static void icon16(PdFb *p,int w,int h,int x,int y,unsigned id,uint32_t c,uint32_t bg){
    if(id<=5)icon_coverage(p,w,h,x,y,uiIconAlpha[id],c,bg);
}
/* Border is a one-screen-pixel overlay, clipped below the toolbar. */
static void canvas_border(PdApp *a,PdFb *bottom){
    if(!(a->guides&12))return;int w=a->width,h=a->height,z=a->zoom;
    int x=((int)a->doc.width*z<w?(w-(int)a->doc.width*z)/2:0)-a->ox*z-a->subx;
    int y=canvas_top(a)+((int)a->doc.height*z<canvas_bottom(a)-canvas_top(a)?(canvas_bottom(a)-canvas_top(a)-(int)a->doc.height*z)/2:0)-a->oy*z-a->suby;
    int right=x+(int)a->doc.width*z-1,lower=y+(int)a->doc.height*z-1;
    int top=y<24?24:y,bottomy=lower>=h?h-1:lower;
    if(y>=24)rect(bottom,w,h,x,y,right-x+1,1,0xffbf47);
    if(lower>=24)rect(bottom,w,h,x,lower,right-x+1,1,0xffbf47);
    if(bottomy>=top){rect(bottom,w,h,x,top,1,bottomy-top+1,0xffbf47);rect(bottom,w,h,right,top,1,bottomy-top+1,0xffbf47);}
}
static void selection_line(PdFb *p,int w,int h,int x0,int y0,int x1,int y1){
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    for(;;){if(x0>=0&&x0<w&&y0>=24&&y0<h)p[y0*w+x0]=pd_fb(((x0+y0)/3)&1?0xffffff:0x141b20);if(x0==x1&&y0==y1)break;int twice=2*err;if(twice>=dy){err+=dy;x0+=sx;}if(twice<=dx){err+=dx;y0+=sy;}}
}
static void selection_overlay(PdApp *a,PdFb *bottom){
    if(!a->selection_valid&&!a->selection_drag)return;int w=a->width,h=a->height,z=a->zoom,dw=(int)a->doc.width,dh=(int)a->doc.height;
    int left=(dw*z<w?(w-dw*z)/2:0)-a->ox*z-a->subx,upper=canvas_top(a)+(dh*z<canvas_bottom(a)-canvas_top(a)?(canvas_bottom(a)-canvas_top(a)-dh*z)/2:0)-a->oy*z-a->suby;
    const uint32_t *mask=a->selection_move?a->selection_base:a->selection;int dx=a->selection_move?a->selection_dx:0,dy=a->selection_move?a->selection_dy:0;
    for(int wy=0;wy<dh;wy++)for(int wx=0;wx<dw;wx++){if(!selected(a,mask,wx,wy))continue;bool l=!selected(a,mask,wx-1,wy),r=!selected(a,mask,wx+1,wy),u=!selected(a,mask,wx,wy-1),b=!selected(a,mask,wx,wy+1);if(!l&&!r&&!u&&!b)continue;
        int fx=left+(wx+dx)*z,fy=upper+(wy+dy)*z;if(a->guides&4){fx=(fx%(dw*z)+dw*z)%(dw*z);if(fx>0)fx-=dw*z;}if(a->guides&8){fy=24+((fy-24)%(dh*z)+dh*z)%(dh*z);if(fy>24)fy-=dh*z;}
        for(int y=fy;y<h;y+=(a->guides&8)?dh*z:h+dh*z)for(int x=fx;x<w;x+=(a->guides&4)?dw*z:w+dw*z){if(x+z<=0||y+z<=24)continue;if(l)selection_line(bottom,w,h,x,y,x,y+z-1);if(r)selection_line(bottom,w,h,x+z-1,y,x+z-1,y+z-1);if(u)selection_line(bottom,w,h,x,y,x+z-1,y);if(b)selection_line(bottom,w,h,x,y+z-1,x+z-1,y+z-1);}
    }
    if(a->selection_drag&&!a->selection_move&&a->selection_shape&&a->lasso_count){int ox=left+a->selection_offset_x*z,oy=upper+a->selection_offset_y*z;for(unsigned i=0;i<a->lasso_count;i++){unsigned j=(i+1)%a->lasso_count;selection_line(bottom,w,h,ox+a->lasso[i][0]*z+z/2,oy+a->lasso[i][1]*z+z/2,ox+a->lasso[j][0]*z+z/2,oy+a->lasso[j][1]*z+z/2);}}
}
static void selection_toolbar(PdApp *a,PdFb *bottom){
    if(a->tool!=4||a->page||a->toolbox||a->confirm||a->zoom_overlay)return;int w=a->width,h=a->height;
    rect(bottom,w,h,0,24,w,26,UI_BG);for(int i=0;i<8;i++){int x=i*w/8,width=(i+1)*w/8-x;bool on=i>=1&&i<=3&&a->selection_mode==(unsigned)(i-1);rect(bottom,w,h,x+1,25,width-2,24,on?UI_SELECTED:UI_PANEL);unsigned glyph=i==0?(a->selection_shape?13:12):(unsigned)(13+i);icon(bottom,w,h,x+width/2-8,29,glyph,UI_TEXT);}
}
static void guided_render(PdApp *a,PdFb *bottom){
    int w=a->width,h=canvas_bottom(a),z=a->zoom,dw=(int)a->doc.width,dh=(int)a->doc.height;
    int left=dw*z<w?(w-dw*z)/2:0,upper=canvas_top(a)+(dh*z<canvas_bottom(a)-canvas_top(a)?(canvas_bottom(a)-canvas_top(a)-dh*z)/2:0);
    /* A 2 KiB bitset records actual writes, including reflected/wrapped pixels.
       Composite each changed pixel once, then stamp only its visible copies. */
    for(unsigned word=0;word<pd_plane(&a->doc)/32;word++){uint32_t bits=a->guide_damage[word];if(!bits)continue;
        for(unsigned bit=0;bits;bit++,bits>>=1){if(!(bits&1))continue;unsigned index=word*32+bit;int wx=(int)(index%a->doc.stride),wy=(int)(index/a->doc.stride);
            pd_composite_rect(&a->doc,wx,wy,wx+1,wy+1);uint32_t c=canvas_displayed(a,pd_image(&a->doc)[index],wx,wy);
            int originx=left+(wx-a->ox)*z-a->subx,originy=upper+(wy-a->oy)*z-a->suby;
            int firstx=originx,firsty=originy;if(a->guides&4){firstx=(originx%(dw*z)+dw*z)%(dw*z);if(firstx>0)firstx-=dw*z;}if(a->guides&8){firsty=24+((originy-24)%(dh*z)+dh*z)%(dh*z);if(firsty>24)firsty-=dh*z;}
            for(int y=firsty;y<h;y+=(a->guides&8)?dh*z:h+dh*z){for(int x=firstx;x<w;x+=(a->guides&4)?dw*z:w+dw*z){
                int x0=x<0?0:x,y0=y<24?24:y,x1=x+z>w?w:x+z,y1=y+z>h?h:y+z;
                for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++)bottom[yy*w+xx]=pd_fb((a->pixel_grid&&z>=4&&(yy==y||xx==x))?grid_color(c):c);
            }}
        }
    }
    canvas_border(a,bottom);selection_overlay(a,bottom);selection_toolbar(a,bottom);a->px0=0;a->py0=24;a->px1=w;a->py1=h;a->present_top=false;a->partial=false;a->redraw=false;
}
/* Upper screen while browsing: name and type tag, a framed preview, details, and what opening does. */
static void render_file_preview(PdApp *a,PdFb *top,int tw,int th){
    char line[96];unsigned type=file_type(a->preview_name);bool palette=a->preview_palette_count>0;
    static const char *formats[]={"FILE","PDD","ASE","GPL","HEX","PNG"};const char *tag=a->preview_frames>1?"ANIM":formats[type];
    rect(top,tw,th,0,0,tw,th,UI_BG);rect(top,tw,th,0,0,tw,16,0x121a1e);type_icon(top,tw,th,5,4,palette?ICON_PAL:type_icon_for(type,false));
    int tag_w=(int)strlen(tag)*6+7;fit_name(line,sizeof(line),a->preview_name,tw-19-tag_w-10);text_raw(top,tw,th,19,5,line,UI_TEXT);
    rect(top,tw,th,tw-tag_w-4,3,tag_w,11,UI_ACCENT);rect(top,tw,th,tw-tag_w-3,4,tag_w-2,9,0x121a1e);text(top,tw,th,tw-tag_w,5,tag,UI_ACCENT);
    int box=th-46;if(box>tw/2)box=tw/2;int bx=8,by=23,scale=box/64;if(scale<1)scale=1;int img=64*scale,ox=bx+(box-img)/2,oy=by+(box-img)/2;
    rect(top,tw,th,bx-1,by-1,box+2,box+2,UI_EDGE);rect(top,tw,th,bx,by,box,box,UI_DEEP);
    if(a->preview_ready){for(int y=0;y<img;y++)for(int x=0;x<img;x++)top[(oy+y)*tw+ox+x]=pd_fb(displayed(a->file_preview[(y/scale)*64+x/scale],x,y));}
    else{fit_name(line,sizeof(line),a->preview_error,box-8);text(top,tw,th,bx+4,by+box/2-3,line,UI_MUTED);}
    int ix=bx+box+12,iy=by+2,value_w=tw-ix-6;char value[40];
    const char *labels[5];char values[5][24];unsigned n=0;
    if(palette){labels[n]="COLORS";snprintf(values[n++],24,"%u",a->preview_palette_count);}
    else if(a->preview_width){labels[n]="SIZE";snprintf(values[n++],24,"%u X %u",a->preview_width,a->preview_height);
        labels[n]="LAYERS";snprintf(values[n++],24,"%u",a->preview_layers);
        if(a->preview_frames>1){labels[n]="FRAMES";snprintf(values[n++],24,"%u",a->preview_frames);}}
    if(a->preview_bytes>=0){labels[n]="FILE";if(a->preview_bytes<1024)snprintf(values[n++],24,"%ld B",a->preview_bytes);else snprintf(values[n++],24,"%ld KB",(a->preview_bytes+1023)/1024);}
    labels[n]="FORMAT";snprintf(values[n++],24,"%s",palette&&type==5?"PNG STRIP":formats[type]);
    for(unsigned i=0;i<n;i++){text(top,tw,th,ix,iy+(int)i*20,labels[i],UI_MUTED);fit_name(value,sizeof(value),values[i],value_w);text(top,tw,th,ix,iy+(int)i*20+9,value,UI_TEXT);}
    rect(top,tw,th,0,th-16,tw,16,0x121a1e);
    const char *note;uint32_t color=UI_MUTED;
    if(a->page==17&&a->recent_files_unsaved)note="RECENT LIST NOT SAVED TO DISK";
    else if(palette)note="IMPORT FROM THE PALETTE MENU";
    else if(a->page==4&&a->file_mode!=0)note="SAVING OVER A FILE KEEPS A BACKUP";
    else if(pd_dirty(&a->doc)){note="OPEN REPLACES UNSAVED ART";color=UI_WARN;}
    else note="OPEN REPLACES CURRENT ART";
    text(top,tw,th,6,th-11,note,color);
}
void app_render(PdApp *a,PdFb *bottom,PdFb *top,int tw,int th){
    if(!masks_fit(a))return;
    onion_cache(a);
    int w=a->width,h=a->height;char label[80];
    bool pan_only=a->pan_frame&&!a->page&&!a->confirm&&!a->doc.editing; a->pan_frame=false;
    bool picker_only=a->picker_update&&a->page==2&&!a->confirm;
    bool keep_field=picker_only&&a->picker_field&&!a->color_tab&&a->painted_hue==a->hue;
    a->picker_update=false;
    a->present_top=true;a->px0=0;a->py0=0;a->px1=w;a->py1=h;
    if(a->partial&&(a->stroke||a->shape_drag||a->overview_pending)&&!a->page&&!a->confirm){
        if(a->guides||a->shape_drag){guided_render(a,bottom);return;}
        pd_composite_rect(&a->doc,a->dx0,a->dy0,a->dx1,a->dy1);
        int left=(int)a->doc.width*a->zoom<w?(w-(int)a->doc.width*a->zoom)/2:0;
        int upper=canvas_top(a)+((int)a->doc.height*a->zoom<canvas_bottom(a)-canvas_top(a)?(canvas_bottom(a)-canvas_top(a)-(int)a->doc.height*a->zoom)/2:0);
        a->px0=left+(a->dx0-a->ox)*a->zoom-a->subx;a->px1=left+(a->dx1-a->ox)*a->zoom-a->subx;
        a->py0=upper+(a->dy0-a->oy)*a->zoom-a->suby;a->py1=upper+(a->dy1-a->oy)*a->zoom-a->suby;
        if(a->px0<0)a->px0=0;if(a->px1>w)a->px1=w;
        int cb=canvas_bottom(a);if(a->py0<24)a->py0=24;if(a->py1>cb)a->py1=cb;
        for(int wy=a->dy0;wy<a->dy1;wy++)for(int wx=a->dx0;wx<a->dx1;wx++){
            int x=left+(wx-a->ox)*a->zoom-a->subx,y=upper+(wy-a->oy)*a->zoom-a->suby;
            int x0=x<0?0:x,y0=y<24?24:y,x1=x+a->zoom,y1=y+a->zoom;
            if(x1>w)x1=w;if(y1>cb)y1=cb;
            uint32_t c=canvas_displayed(a,pd_image(&a->doc)[wy*a->doc.stride+wx],wx,wy);
            for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++)bottom[yy*w+xx]=pd_fb((a->pixel_grid&&a->zoom>=4&&(yy==y||xx==x))?grid_color(c):c);
        }
        selection_overlay(a,bottom);selection_toolbar(a,bottom);
        /* Overview and text catch up on lift; no top-screen work while inking. */
        a->present_top=false;a->partial=false;a->redraw=false;return;
    }
    a->partial=false;if(!pan_only&&!picker_only&&!a->selection_drag)pd_composite(&a->doc);
    if(keep_field){
        /* Keep the existing HSV field, restoring only the previous cursor pixels. */
        int fw=h-88,fh=fw;
        for(int yy=a->painted_sy-3;yy<=a->painted_sy+3;yy++)for(int xx=a->painted_sx-3;xx<=a->painted_sx+3;xx++)
            if(xx>=8&&xx<8+fw&&yy>=32&&yy<32+fh)bottom[yy*w+xx]=pd_fb(hsv(a->hue,(unsigned)((xx-8)*255/(fw-1)),(unsigned)(255-(yy-32)*255/(fh-1)))&0xffffffu);
        rect(bottom,w,h,0,0,w,32,UI_BG);rect(bottom,w,h,0,32,8,fh,UI_BG);
        rect(bottom,w,h,8+fw,32,w-8-fw,fh,UI_BG);rect(bottom,w,h,0,32+fh,w,h-32-fh,UI_BG);
    }else rect(bottom,w,h,0,0,w,h,UI_BG);
    if(!picker_only)rect(top,tw,th,0,0,tw,th,UI_BG);
    rect(bottom,w,h,0,0,w,24,0x182126);
    for(int i=0;i<6;i++){
        bool active=(i==2&&a->page==2)||(i==3&&a->page==1)||(i==4&&a->toolbox);
        if(active)rect(bottom,w,h,i*w/6,0,w/6,24,0x0d7a75);
        icon16(bottom,w,h,(2*i+1)*w/12-8,4,(unsigned)i,active?UI_TEXT:UI_ACCENT,active?UI_SELECTED:UI_BG);
    }
    if(a->confirm){text(bottom,w,h,8,48,a->notice,UI_TEXT);text(bottom,w,h,8,70,a->confirm==6?"UNDO CAN RESTORE THIS LAYER":"THIS CANNOT BE UNDONE",0xffaaa0);
        if((a->confirm==2&&(a->new_width>128||a->new_height>128))||((a->confirm==4||a->confirm==8)&&a->doc.profile>=2)){text(bottom,w,h,8,94,"LARGE CANVASES MAY BE SLOWER",UI_TEXT);text(bottom,w,h,8,110,"LAYERS / TOOLS LIMITED BY MEMORY",UI_MUTED);text(bottom,w,h,8,126,"CLIPBOARD / TRANSFORMS: 128 X 128",UI_MUTED);}
        if(a->confirm==7){const char *base=strrchr(a->file_source,'/');snprintf(label,sizeof(label),"%.38s",base?base+1:a->file_source);text(bottom,w,h,8,92,label,UI_TEXT);}
        if(a->confirm==6){snprintf(label,sizeof(label),"%.38s",a->doc.names[a->doc.active]);text_raw(bottom,w,h,8,92,label,UI_TEXT);}
        button(bottom,w,h,4,h-32,w/2-8,28,"CONFIRM",true);button(bottom,w,h,w/2+4,h-32,w/2-8,28,"CANCEL",false);
    }else if(a->page==1&&a->layers_tab){
        timeline_draw(a,bottom,w,h);
    }else if(a->page==1){
        layer_clamp(a);unsigned count=layer_count(a);
        int top=layer_list_top(),footer=layer_footer_y(a),list_h=footer-top,row_h=layer_row_height();
        layers_tabs_draw(a,bottom,w,h);snprintf(label,sizeof(label),"%u/%u LAYERS  %u FREE",count,a->doc.capacity,a->doc.capacity-count);text(bottom,w,h,w-6-((int)strlen(label)*6-1),29,label,UI_MUTED);
        for(int row=0;row<(int)count;row++){
            int slot=layer_at(a,row),y=top+row*row_h-a->layer_scroll;if(y+row_h-1<top||y>=footer)continue;PdLayer *l=&a->doc.layer[slot];
            bool selected=a->doc.active==(unsigned)slot,hidden=!l->visible,locked=(l->flags&PD_LAYER_LOCK)!=0,alpha=(l->flags&PD_LAYER_ALPHA_LOCK)!=0;
            int y0=y>top?y:top,y1=y+row_h-1<footer?y+row_h-1:footer,row_bg=selected?UI_SELECTED:UI_PANEL;
            rect(bottom,w,h,4,y0,w-12,y1-y0,UI_EDGE);rect(bottom,w,h,5,y0+1,w-14,(y1-y0>2?y1-y0-2:1),row_bg);
            if(y>=top&&y+row_h<=footer){
                icon_coverage(bottom,w,h,6,y+2,uiLayerIconAlpha[0],a->layer_drag&&selected?UI_ACCENT:UI_MUTED,row_bg);
                icon_coverage(bottom,w,h,22,y+2,uiLayerIconAlpha[hidden?2:1],hidden?UI_MUTED:UI_TEXT,row_bg);
                int meta_x=44,max_chars=(w-meta_x-50)/6;if(max_chars<6)max_chars=6;
                snprintf(label,sizeof(label),"%.*s",max_chars,a->doc.names[slot]);text_raw(bottom,w,h,meta_x,y+2,label,hidden?UI_MUTED:UI_TEXT);
                static const char *modes[]={"NORMAL","MULTIPLY","SCREEN","OVERLAY"};
                snprintf(label,sizeof(label),"%s %u%%%s",modes[l->blend],(unsigned)l->opacity*100/255,alpha?"  A":"");text(bottom,w,h,meta_x,y+11,label,UI_MUTED);
                icon_coverage(bottom,w,h,w-46,y+2,uiLayerIconAlpha[locked?4:3],locked?UI_ACCENT:UI_MUTED,row_bg);
                icon_coverage(bottom,w,h,w-26,y+2,uiLayerIconAlpha[5],UI_MUTED,row_bg);
            }
        }
        int total=(int)count*row_h,thumb=total>list_h?list_h*list_h/total:list_h,pos=total>list_h?a->layer_scroll*(list_h-thumb)/(total-list_h):0;
        rect(bottom,w,h,w-7,top,4,list_h,UI_PANEL);rect(bottom,w,h,w-7,top+pos,4,thumb,UI_ACCENT);
        if(a->layer_drag){int yy=a->layer_drag_y-10;if(yy<top)yy=top;if(yy>footer-20)yy=footer-20;snprintf(label,sizeof(label),"MOVE %.18s",a->doc.names[a->doc.active]);button(bottom,w,h,44,yy,w-72,20,label,true);}
        int fw=(w-16)/3;
        button(bottom,w,h,4,footer,fw,22,"+ NEW LAYER",count<a->doc.capacity);
        button(bottom,w,h,8+fw,footer,fw,22,"ADJUST",false);
        button(bottom,w,h,12+2*fw,footer,w-16-2*fw,22,w-16-2*fw>=(int)strlen("DELETE LAYER")*6+10?"DELETE LAYER":"DELETE",false);
        if(a->layer_popup){
            PdLayer *l=&a->doc.layer[a->doc.active];static const char *modes[]={"NORMAL","MULTIPLY","SCREEN","OVERLAY"};
            int ox=layer_options_x(a),ow=layer_options_width(a),ix=layer_options_inner_x(a),iw=layer_options_inner_w(a);
            rect(bottom,w,h,ox-2,24,ow+4,h-32,UI_EDGE);rect(bottom,w,h,ox-1,25,ow+2,h-34,UI_BG);
            char title[64];snprintf(title,sizeof(title),"LAYER OPTIONS  %.18s",a->doc.names[a->doc.active]);text(bottom,w,h,ix,31,title,UI_ACCENT);
            text(bottom,w,h,ox+ow-12,31,"X",UI_TEXT);
            snprintf(label,sizeof(label),"OPACITY  %u%%",(unsigned)l->opacity*100/255);text(bottom,w,h,ix,49,label,UI_TEXT);
            rect(bottom,w,h,ix,61,iw-4,10,UI_PANEL);int sx=ix+(int)l->opacity*(iw-5)/255;rect(bottom,w,h,ix,61,sx-ix,10,UI_SELECTED);rect(bottom,w,h,sx-2,58,4,16,UI_TEXT);
            text(bottom,w,h,ix,78,"BLEND",UI_MUTED);button(bottom,w,h,ix,89,iw-4,22,modes[l->blend],false);
            for(int yy=0;yy<4;yy++)rect(bottom,w,h,ix+iw-14+yy,98+yy,7-yy*2,1,UI_TEXT);
            snprintf(label,sizeof(label),"LOCK %s",(l->flags&PD_LAYER_LOCK)?"ON":"OFF");button(bottom,w,h,ix,117,iw/2-2,22,label,(l->flags&PD_LAYER_LOCK)!=0);
            snprintf(label,sizeof(label),"ALPHA %s",(l->flags&PD_LAYER_ALPHA_LOCK)?"ON":"OFF");button(bottom,w,h,ix+iw/2+2,117,iw-iw/2-2,22,label,(l->flags&PD_LAYER_ALPHA_LOCK)!=0);
            int action_w=(iw-8)/3;button(bottom,w,h,ix,145,action_w,22,"RENAME",false);button(bottom,w,h,ix+action_w+4,145,action_w,22,"DUPE",false);button(bottom,w,h,ix+2*action_w+8,145,iw-2*action_w-8,22,"MERGE",false);
        }
        layer_blend_dropdown_draw(a,bottom,w,h);
        if(a->layer_delete){
            rect(bottom,w,h,24,66,w-48,90,UI_EDGE);rect(bottom,w,h,25,67,w-50,88,UI_BG);
            text(bottom,w,h,36,78,"DELETE LAYER?",UI_TEXT);snprintf(label,sizeof(label),"%.28s",a->doc.names[a->doc.active]);text_raw(bottom,w,h,36,96,label,UI_MUTED);
            button(bottom,w,h,32,120,w/2-40,28,"DELETE",true);button(bottom,w,h,w/2+4,120,w/2-36,28,"CANCEL",false);
        }
    }else if(a->page==2){
        const char *tabs[]={"SWATCHES","PICKER","EDIT"};unsigned mode=a->palette_edit?2:a->color_tab?0:1;
        for(unsigned i=0;i<3;i++)button(bottom,w,h,(int)i*w/3+2,26,w/3-4,18,tabs[i],mode==i);
        uint32_t target_color=*palette_color_target(a);
        if(a->palette_edit){
            rect(bottom,w,h,6,48,w-12,28,UI_EDGE);rect(bottom,w,h,7,49,w-14,26,UI_PANEL);
            for(int yy=52;yy<72;yy++)for(int xx=12;xx<32;xx++)bottom[yy*w+xx]=pd_fb(displayed(target_color,xx,yy));
            snprintf(label,sizeof(label),"#%06X  SWATCH %u",(unsigned)(target_color&0xffffff),a->swatch_selected+1);text(bottom,w,h,38,58,label,UI_TEXT);
            const char *actions[]={"ADD","REPLACE","REMOVE","MOVE","LOAD","SAVE"};
            for(unsigned i=0;i<6;i++)button(bottom,w,h,(int)(i%3)*w/3+2,80+(int)(i/3)*28,w/3-4,26,actions[i],false);
            button(bottom,w,h,4,h-28,w-8,24,"DONE / BACK TO SWATCHES",true);
        }else if(a->color_tab){
            button(bottom,w,h,4,46,40,20,"FG",a->palette_target==0);button(bottom,w,h,46,46,40,20,"BG",a->palette_target==1);
            button(bottom,w,h,88,46,24,20,"<>",false);
            for(unsigned i=0;i<3;i++){snprintf(label,sizeof(label),"%u",i==0?8:i==1?6:5);button(bottom,w,h,116+(int)i*45,46,41,20,label,a->palette_density==i);}
            render_palette(a,bottom,w,h);
            snprintf(label,sizeof(label),"TAP USE / HOLD MOVE / %u COLORS",a->swatch_count);text(bottom,w,h,8,h-17,label,UI_MUTED);
        }else{
            int size=picker_square(a),top=palette_content_y(a),hue_x=6+size+4,side=picker_side_left(a),tile=picker_tile(a);
            button(bottom,w,h,4,46,36,20,"FG",a->palette_target==0);button(bottom,w,h,42,46,36,20,"BG",a->palette_target==1);button(bottom,w,h,80,46,24,20,"<>",false);
            rect(bottom,w,h,106,46,w-110,20,UI_EDGE);rect(bottom,w,h,107,47,w-112,18,UI_PANEL);snprintf(label,sizeof(label),"#%06X  HEX",(unsigned)(target_color&0xffffff));text(bottom,w,h,112,52,label,UI_TEXT);
            unsigned char sat[128];uint32_t ramp[256];for(unsigned c=0;c<256;c++)ramp[c]=hsv(a->hue,255,c)&0xffffffu;for(int xx=0;xx<size;xx++)sat[xx]=(unsigned char)(xx*255/(size-1));
            rect(bottom,w,h,5,top-1,size+2,size+2,UI_EDGE);
            for(int yy=0;yy<size;yy++){unsigned val=(unsigned)(255-yy*255/(size-1));for(int xx=0;xx<size;xx++){unsigned c=(val*sat[xx]+127)/255;bottom[(top+yy)*w+xx+6]=pd_fb(ramp[c]+(val-c)*0x010101u);}}
            int sx=6+(int)a->saturation*(size-1)/255,sy=top+(255-(int)a->value)*(size-1)/255;a->painted_sx=sx;a->painted_sy=sy;a->painted_hue=a->hue;
            rect(bottom,w,h,sx-3,sy-3,7,7,0);rect(bottom,w,h,sx-2,sy-2,5,5,UI_TEXT);rect(bottom,w,h,sx-1,sy-1,3,3,hsv(a->hue,a->saturation,a->value)&0xffffffu);
            rect(bottom,w,h,hue_x,top-1,14,size+2,UI_EDGE);for(int yy=0;yy<size;yy++)rect(bottom,w,h,hue_x+1,top+yy,12,1,hsv((unsigned)(yy*359/(size-1)),255,255)&0xffffffu);
            int hy=top+(int)a->hue*(size-1)/359;rect(bottom,w,h,hue_x,hy-1,14,3,0);rect(bottom,w,h,hue_x+1,hy,12,1,UI_TEXT);
            int alpha_y=top+size+3;text(bottom,w,h,6,alpha_y+2,"A",UI_MUTED);rect(bottom,w,h,16,alpha_y+2,size+8,8,UI_PANEL);int ax=16+(int)(target_color>>24)*(size+7)/255;rect(bottom,w,h,16,alpha_y+2,ax-16,8,UI_SELECTED);rect(bottom,w,h,ax-2,alpha_y,4,12,UI_TEXT);
            rect(bottom,w,h,side-2,top-1,w-side+1,picker_main_height(a)+2,UI_EDGE);
            int cols=6;unsigned first=(unsigned)(a->palette_scroll[0]/(tile+2))*cols,end=first+(unsigned)(picker_main_height(a)/(tile+2)+1)*cols;if(end>a->swatch_count)end=a->swatch_count;
            for(unsigned i=first;i<end;i++){
                int px=side+(int)(i%(unsigned)cols)*(tile+2),py=top+(int)(i/(unsigned)cols)*(tile+2)-a->palette_scroll[0];
                int panel_bottom=top+picker_main_height(a);if(py+tile<=top||py>=panel_bottom)continue;
                int y0=py>top?py:top,y1=py+tile<panel_bottom?py+tile:panel_bottom;
                int x0=px>side?px:side,x1=px+tile<w-6?px+tile:w-6;
                for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++){bool edge=xx<px+2||xx>=px+tile-2||yy<py+2||yy>=py+tile-2;bottom[yy*w+xx]=pd_fb(edge?(i==a->swatch_selected?UI_ACCENT:UI_EDGE):displayed(a->swatches[i],xx,yy));}
            }
            int total=((int)a->swatch_count+cols-1)/cols*(tile+2);if(total>picker_main_height(a)){int thumb=picker_main_height(a)*picker_main_height(a)/total;if(thumb<8)thumb=8;int sy2=top+a->palette_scroll[0]*(picker_main_height(a)-thumb)/(total-picker_main_height(a));rect(bottom,w,h,w-4,top,3,picker_main_height(a),UI_PANEL);rect(bottom,w,h,w-4,sy2,3,thumb,UI_ACCENT);}
            button(bottom,w,h,4,h-28,w/2-8,22,"CANCEL",false);button(bottom,w,h,w/2+4,h-28,w/2-8,22,"USE COLOR",true);
        }
    }else if(a->page==30){
        text(bottom,w,h,8,30,"SEND SPRITE TO DISCORD",UI_ACCENT);
        text(bottom,w,h,8,44,"GIF: VISIBLE LAYERS / BINARY ALPHA",UI_MUTED);
        button(bottom,w,h,8,52,w-16,28,"SEND ACTIVE LAYER PNG",false);
        button(bottom,w,h,8,86,w-16,28,"SEND VISIBLE PNG",false);
        button(bottom,w,h,8,120,w-16,28,"SEND ANIMATION GIF",false);
        text(bottom,w,h,8,150,"GIF COLORS MAY BE REDUCED",UI_MUTED);
        button(bottom,w,h,4,h-32,w-8,26,"BACK",false);
    }else if(a->page==31){
        int scope_y=adjustment_scope_y(h);
        snprintf(label,sizeof(label),"COLOR ADJUSTMENT  VISIBLE %u",adjustment_visible_count(a));text(bottom,w,h,8,27,label,UI_ACCENT);
        const char *scopes[]={"SELECTION","CURRENT","VISIBLE"};for(unsigned i=0;i<3;i++)button(bottom,w,h,(int)i*w/3+2,scope_y,w/3-4,20,scopes[i],a->adjust_scope==i);
        const char *names[]={"HUE","SATURATION","BRIGHTNESS"};int maxima[]={180,100,100};
        for(unsigned row=0;row<3;row++){
            int sy=adjustment_slider_y(h,row),value=*adjustment_value(a,row),max=maxima[row];
            snprintf(label,sizeof(label),"%s %+d",names[row],value);text(bottom,w,h,8,sy,label,value?UI_TEXT:UI_MUTED);
            int right=w-88;rect(bottom,w,h,8,sy+15,right-8,8,UI_PANEL);int mid=8+(right-8)/2,sx=8+(value+max)*(right-8)/(max*2);
            rect(bottom,w,h,mid-1,sy+13,2,12,UI_MUTED);if(sx>=mid)rect(bottom,w,h,mid,sy+16,sx-mid,6,UI_SELECTED);else rect(bottom,w,h,sx,sy+16,mid-sx,6,UI_SELECTED);rect(bottom,w,h,sx-2,sy+12,4,14,UI_TEXT);
            button(bottom,w,h,w-84,sy-1,36,22,"-",false);button(bottom,w,h,w-42,sy-1,36,22,"+",false);
        }
        uint32_t before=a->color,after=pd_adjust_color(before,a->adjust_hue,a->adjust_saturation,a->adjust_brightness);
        int py=h>=220?h-46:h-45;rect(bottom,w,h,8,py,24,14,UI_EDGE);for(int yy=py+2;yy<py+12;yy++)for(int xx=10;xx<30;xx++)bottom[yy*w+xx]=pd_fb(before&0xffffff);text(bottom,w,h,38,py+3,">",UI_MUTED);rect(bottom,w,h,48,py,24,14,UI_EDGE);for(int yy=py+2;yy<py+12;yy++)for(int xx=50;xx<70;xx++)bottom[yy*w+xx]=pd_fb(after&0xffffff);
        button(bottom,w,h,4,h-28,w/2-8,24,"APPLY",true);button(bottom,w,h,w/2+4,h-28,w/2-8,24,"CANCEL",false);
    }else if(a->page==3){
        MenuButton b[12];unsigned n=menu_buttons(a,b);for(unsigned i=0;i<n;i++)button(bottom,w,h,b[i].x,b[i].y,b[i].w,b[i].h,b[i].label,false);
        const char *name=strrchr(a->project,'/');if(name){fit_name(label,sizeof(label),name+1,w-16);text_raw(bottom,w,h,8,h-12,label,UI_ACCENT);}else text(bottom,w,h,8,h-12,"UNTITLED - SAVE AS TO NAME",UI_ACCENT);
    }else if(a->page==4){
        BrowserLayout l=browser_layout(a);
        static const char *titles[]={"OPEN","SAVE PROJECT","EXPORT ASEPRITE","EXPORT PNG","IMPORT PALETTE","EXPORT GPL","EXPORT HEX","EXPORT PNG PAL"};
        const char *title=titles[a->file_mode];text(bottom,w,h,6,30,title,UI_ACCENT);
        int path_x=6+(int)strlen(title)*6+6,path_end=a->file_mode<=1?w-62:w-6,chars=(path_end-path_x+1)/6;
        const char *folder=a->folder;if(!strncmp(folder,a->directory,strlen(a->directory)))folder+=strlen(a->directory);
        char path[PD_PATH+2];snprintf(path,sizeof(path),"~%s",folder);size_t len=strlen(path);
        size_t room=chars>0?(size_t)chars:0;if(room>sizeof(label)-1)room=sizeof(label)-1;
        if(len>room&&room>2){size_t tail=room-2;label[0]=label[1]='.';memcpy(label+2,path+len-tail,tail);label[room]=0;}else{size_t c=len<room?len:room;memcpy(label,path,c);label[c]=0;}
        if(chars>0)text_raw(bottom,w,h,path_x,30,label,UI_MUTED);
        if(a->file_mode<=1)button(bottom,w,h,w-58,25,54,16,"RECENT",false);
        bool root=strlen(a->folder)<=strlen(a->directory),open=a->file_type_dropdown.open;
        static const char *const up_arrow[]={"....#....","...###...","..#####..",".#######.","...###...","...###...","...###..."};
        rect(bottom,w,h,4,44,22,18,UI_EDGE);rect(bottom,w,h,5,45,20,16,root?UI_DEEP:UI_PANEL);pixel_rows(bottom,w,h,10,49,up_arrow,7,root?UI_EDGE:UI_TEXT);
        rect(bottom,w,h,30,44,w-34,18,open?UI_ACCENT:UI_EDGE);rect(bottom,w,h,31,45,w-36,16,UI_PANEL);
        type_icon(bottom,w,h,35,49,filter_icons[a->file_filter]);text(bottom,w,h,50,50,filter_names[a->file_filter],UI_TEXT);
        text(bottom,w,h,58+(int)strlen(filter_names[a->file_filter])*6,50,filter_exts[a->file_filter],UI_MUTED);
        static const char *const chevron_down[]={"#######",".#####.","..###..","...#..."},*const chevron_up[]={"...#...","..###..",".#####.","#######"};
        pixel_rows(bottom,w,h,w-16,51,open?chevron_up:chevron_down,4,UI_ACCENT);
        unsigned rows=file_rows(a);int list_h=l.list_bottom-l.list_top;PdFb *list=bottom+l.list_top*w;
        static const char *types[]={"FILE","PDD","ASE","GPL","HEX","PNG"};
        for(unsigned row=0;row<rows;row++){
            int yy=(int)row*l.row_h-a->file_scroll;if(yy+l.row_h<=0||yy>=list_h)continue;
            int i=file_index(a,row);PdEntry *e=&a->listing.entries[i];bool on=a->selected==i;unsigned t=file_type(e->name);
            if(on){rect(list,w,list_h,4,yy,w-14,l.row_h,UI_ACCENT);rect(list,w,list_h,5,yy+1,w-16,l.row_h-2,UI_SELECTED);}else rect(list,w,list_h,6,yy+l.row_h-1,w-18,1,UI_LINE);
            type_icon(list,w,list_h,8,yy+(l.row_h-8)/2,type_icon_for(t,e->directory));
            const char *meta=e->directory?">":types[t];int mx=w-14-(int)strlen(meta)*6;text(list,w,list_h,mx,yy+(l.row_h-7)/2,meta,on?UI_TEXT:UI_MUTED);
            fit_name(label,sizeof(label),e->name,mx-22-6);text_raw(list,w,list_h,22,yy+(l.row_h-7)/2,label,UI_TEXT);
        }
        if(!rows)text(bottom,w,h,10,l.list_top+8,"NO MATCHING FILES HERE",UI_MUTED);
        rect(bottom,w,h,w-8,l.list_top,3,list_h,UI_DEEP);
        if(rows){int total=(int)rows*l.row_h;if(total>list_h){int thumb=list_h*list_h/total;if(thumb<8)thumb=8;int pos=a->file_scroll*(list_h-thumb)/(total-list_h);rect(bottom,w,h,w-8,l.list_top+pos,3,thumb,UI_ACCENT);}else rect(bottom,w,h,w-8,l.list_top,3,list_h,UI_EDGE);}
        int fw=(w-20)/4;const char *labels[4]={"NEW DIR","MORE","CANCEL",a->file_mode==4?"IMPORT":a->file_mode?"NAME":"OPEN"};
        for(int i=0;i<4;i++){int bx=4+i*(fw+4),bw=i==3?w-4-bx:fw;rect(bottom,w,h,bx,l.footer_y,bw,l.footer_h,UI_EDGE);rect(bottom,w,h,bx+1,l.footer_y+1,bw-2,l.footer_h-2,i==3?UI_SELECTED:UI_PANEL);
            text(bottom,w,h,bx+(bw-(int)strlen(labels[i])*6+1)/2,l.footer_y+(l.footer_h-7)/2,labels[i],UI_TEXT);}
        file_type_dropdown_draw(a,bottom,w,h);
    }else if(a->page==18){
        snprintf(label,sizeof(label),"SIZE: %u X %u  >",a->new_width,a->new_height);button(bottom,w,h,4,26,w-8,22,label,true);
        const char *bg[]={"TRANSPARENT","WHITE","BG COLOR"};snprintf(label,sizeof(label),"BACKGROUND: %s",bg[a->new_background]);button(bottom,w,h,4,52,w-8,28,label,false);
        snprintf(label,sizeof(label),"STARTING LAYERS: %u",a->new_layers);button(bottom,w,h,4,86,w-8,28,label,false);button(bottom,w,h,4,120,w-8,28,a->new_current_palette?"PALETTE: CURRENT PROJECT":"PALETTE: MY DEFAULT",false);
        button(bottom,w,h,4,h-28,w/2-8,24,"CREATE",true);button(bottom,w,h,w/2+4,h-28,w/2-8,24,"CANCEL",false);
    }else if(a->page==22){
        icon(bottom,w,h,8,25,10,UI_TEXT);text(bottom,w,h,32,28,"GUIDES",UI_TEXT);const char *names[]={"PIXEL GRID (4X+)","SYMMETRY X","SYMMETRY Y","TILE X","TILE Y"};for(int i=0;i<5;i++){bool on=i?(a->guides&(1u<<(i-1)))!=0:a->pixel_grid;snprintf(label,sizeof(label),"%s: %s",names[i],on?"ON":"OFF");button(bottom,w,h,4,40+i*24,w-8,22,label,on);}button(bottom,w,h,4,h-24,w-8,22,"DONE",false);
    }else if(a->page==20){
        const char *sizes[]={"16","32","64","128","256","512"};int n=a->doc.profile>=2?6:4;for(int i=0;i<n;i++)button(bottom,w,h,i*w/n+2,32,w/n-4,22,sizes[i],false);
        snprintf(label,sizeof(label),"W: %u",a->new_width);button(bottom,w,h,4,60,w/2-8,24,label,!a->size_axis);snprintf(label,sizeof(label),"H: %u",a->new_height);button(bottom,w,h,w/2+4,60,w/2-8,24,label,a->size_axis!=0);
        for(int i=0;i<9;i++){char key[2]={(char)('1'+i),0};button(bottom,w,h,(i%3)*w/3+2,90+(i/3)*20,w/3-4,18,key,false);}
        button(bottom,w,h,2,150,w/3-4,20,"DELETE",false);button(bottom,w,h,w/3+2,150,w/3-4,20,"0",false);snprintf(label,sizeof(label),"SET %s",a->numeric);button(bottom,w,h,2*w/3+2,150,w/3-4,20,label,true);button(bottom,w,h,4,h-20,w-8,18,"DONE",false);
    }else if(a->page==19){
        text(bottom,w,h,8,28,"RECOVERY - SEPARATE FROM SAVES",UI_ACCENT);
        button(bottom,w,h,4,40,w-8,26,a->recovery_available?"RESTORE RECOVERY":"NO RECOVERY COPY",a->recovery_available);
        button(bottom,w,h,4,72,w-8,26,"DISCARD RECOVERY COPIES",false);button(bottom,w,h,4,104,w-8,26,a->recovery_enabled?"AUTOSAVE: ON (THIS SESSION)":"AUTOSAVE: OFF (THIS SESSION)",false);
        text(bottom,w,h,8,138,a->recovery_pending?"RESTORE / DISCARD TO RESUME SAVING":"EVERY 2 MINUTES, AFTER 2S IDLE",UI_MUTED);button(bottom,w,h,4,h-28,w-8,24,"BACK / KEEP FOR LATER",false);
    }else if(a->page==5){
        bool hex=a->name_action==5;static const char *exts[]={"",".pdd",".aseprite",".png","",".gpl",".hex",".png"};
        const char *ext=a->name_action==0&&a->file_mode<8?exts[a->file_mode]:"";int chip=ext[0]?(int)strlen(ext)*6+8:0;
        text(bottom,w,h,8,32,hex?"HEX COLOR - SIX DIGITS":a->name_action==2?"RENAME LAYER":a->name_action==6?"RENAME TAG":a->name_action==3?"RENAME FILE":a->name_action==4?"DUPLICATE FILE":a->name_action?"NEW FOLDER NAME":"FILE NAME",UI_TEXT);
        if(chip)text(bottom,w,h,8+10*6,32,"EXTENSION ADDED",UI_MUTED);
        unsigned start=text_start(a);char shown[64];if(hex)snprintf(shown,sizeof(shown),"#%.37s",a->filename+start);else snprintf(shown,sizeof(shown),"%.*s",(w-16-chip)/6,a->filename+start);
        rect(bottom,w,h,4,43,w-8,22,UI_ACCENT);rect(bottom,w,h,5,44,w-10,20,UI_PANEL);text_raw(bottom,w,h,8,50,shown,UI_ACCENT);rect(bottom,w,h,8+(int)(a->text_cursor-start+(hex?1:0))*6,48,1,13,UI_TEXT);
        if(chip){rect(bottom,w,h,w-8-chip,46,chip,16,UI_EDGE);rect(bottom,w,h,w-7-chip,47,chip-2,14,UI_DEEP);text_raw(bottom,w,h,w-4-chip,51,ext,UI_MUTED);}
        int rh=text_row_height(a);
        if(hex){
            int kw=(w-8)/10;for(unsigned row=0;row<3;row++){const char *keys=text_keys(a->text_mode,row);int left=(w-(int)strlen(keys)*kw)/2;for(unsigned i=0;i<strlen(keys);i++){char key[2]={keys[i],0};int x=left+(int)i*kw,y=72+(int)row*rh;button(bottom,w,h,x+1,y,kw-2,rh-2,"",false);text(bottom,w,h,x+kw/2-3,y+(rh-8)/2,key,UI_TEXT);}}
            int cy=72+rh*3;button(bottom,w,h,4,cy,w/3-8,rh-2,"CLEAR",false);button(bottom,w,h,w/3+4,cy,w/3-8,rh-2,"<",false);button(bottom,w,h,2*w/3+4,cy,w/3-8,rh-2,">",false);
            button(bottom,w,h,4,h-28,w/3-8,24,"DELETE",false);button(bottom,w,h,w/3+4,h-28,w/3-8,24,"CANCEL",false);button(bottom,w,h,2*w/3+4,h-28,w/3-8,24,"DONE",true);
        }else{
            static const char *const shift_off[]={"....#....","...#.#...","..#...#..",".#.....#.","###...###","..#...#..","..#...#..","..#####.."};
            static const char *const shift_on[]={"....#....","...###...","..#####..",".#######.","#########","..#####..","..#####..","..#####.."};
            static const char *const backspace[]={"...########","..#.#...#.#",".#...#.#..#","#.....#...#",".#...#.#..#","..#.#...#.#","...########"};
            KeyRect k[40];unsigned n=keyboard_keys(a,k);
            for(unsigned i=0;i<n;i++){KeyRect *r=&k[i];bool active=(r->kind==KEY_SHIFT&&a->text_shift)||(r->kind==KEY_MODE&&a->text_mode==2);
                rect(bottom,w,h,r->x+1,r->y,r->w-2,r->h-2,UI_EDGE);rect(bottom,w,h,r->x+2,r->y+1,r->w-4,r->h-4,active?UI_SELECTED:r->kind==KEY_CHAR?UI_PANEL:UI_DEEP);
                int cx=r->x+r->w/2,cy=r->y+(r->h-2)/2;
                if(r->kind==KEY_CHAR){char key[2]={key_char(a,r->ch),0};text_raw(bottom,w,h,cx-2,cy-3,key,UI_TEXT);}
                else if(r->kind==KEY_SHIFT){pixel_rows(bottom,w,h,cx-4,cy-4,a->text_shift?shift_on:shift_off,8,UI_TEXT);if(a->text_shift==2)rect(bottom,w,h,cx-4,cy+5,9,1,UI_TEXT);}
                else if(r->kind==KEY_BKSP)pixel_rows(bottom,w,h,cx-5,cy-3,backspace,7,UI_TEXT);
                else{const char *l=r->kind==KEY_MODE?(a->text_mode==2?"abc":"123"):r->kind==KEY_SPACE?"SPACE":r->kind==KEY_LEFT?"<":">";
                    text_raw(bottom,w,h,cx-(int)strlen(l)*3+1,cy-3,l,r->kind==KEY_SPACE?UI_MUTED:UI_TEXT);}
            }
            button(bottom,w,h,4,h-28,w/2-6,24,"CANCEL",false);button(bottom,w,h,w/2+2,h-28,w/2-6,24,"DONE",true);
        }
    }else if(a->page==6){
        button(bottom,w,h,4,45,w-8,40,pd_frame_count(&a->doc)>1?"ASEPRITE - FRAMES + LAYERS":"ASEPRITE - EDITABLE LAYERS",false);
        button(bottom,w,h,4,95,w-8,40,"PNG - VISIBLE IMAGE + ALPHA",false);
    }else if(a->page==8){
        button(bottom,w,h,4,32,w-8,28,a->draft_handed?"TOOLBOX: RIGHT SIDE":"TOOLBOX: LEFT SIDE",false);
        const char *sides[]={"ZOOM SIDE: FOLLOW TOOLBOX","ZOOM SIDE: LEFT","ZOOM SIDE: RIGHT"};button(bottom,w,h,4,64,w-8,28,sides[a->draft_zoom_side],false);
        button(bottom,w,h,4,96,w-8,24,"RESET DRAFT TO DEFAULTS",false);
        button(bottom,w,h,4,124,w/2-8,24,"CONTROLS / HELP",false);button(bottom,w,h,w/2+4,124,w/2-8,24,"TIMINGS",false);
        button(bottom,w,h,4,h-32,w/2-8,28,"SAVE",true);button(bottom,w,h,w/2+4,h-32,w/2-8,28,"CANCEL",false);
    }else if(a->page==9){
        const char *tabs[]={"DRAW","COLOR","LAYERS","KEYS"};for(int i=0;i<4;i++)button(bottom,w,h,i*w/4+4,32,w/4-8,24,tabs[i],a->help_tab==(unsigned)i);
        static const char *lines[4][7]={
            {"A / LEFT + DRAG: PAN","B / DOWN: TOGGLE TOOLBOX","X / UP + TAP: SAMPLE COLOR","HOLD Y / RIGHT: TAP + OR -","L / R: TEMPORARY ERASER","START: SAVE  SELECT: MENU","3DS CIRCLE PAD: PAN"},
            {"TAP A SWATCH TO USE COLOR","QUICK VERTICAL DRAG: SCROLL","HOLD 150 MS: DRAG TO REORDER","DRAG TO EDGE: SCROLL WHILE MOVING","EDIT: PICKER OPENS SELECTED COLOR","SAVE: UPDATE  BACK: DISCARD","ADD / REMOVE: CHANGE PALETTE"},
            {"TAP ROW: SELECT AND PREVIEW","EYE: TOGGLE LAYER VISIBILITY","ROW MENU: OPACITY AND BLEND","RENAME / DUPLICATE / DELETE","MERGE DOWN: VISIBLE NORMAL PAIR","HOLD 150 MS: DRAG TO REORDER","UNDO RESTORES LAYER CHANGES"},
            {"HOLD ZOOM + L: UNDO","HOLD ZOOM + R: REDO","LIFT PEN BEFORE USING SHORTCUTS","TAP SHOULDER ONCE PER STEP","BOTH SHOULDERS: NO HISTORY ACTION","CANVAS ONLY - MENUS KEEP DRAFTS","REMAP ALSO CHANGES ZOOM MODIFIER"}
        };
        for(int i=0;i<7;i++){
            const char *line=lines[a->help_tab][i];
            if(!a->help_tab&&i<4){unsigned action=i==0?1:i==1?0:(unsigned)i;const char *tails[]={" + DRAG: PAN",": TOOLBOX"," + TAP: SAMPLE",": HOLD FOR ZOOM"};snprintf(label,sizeof(label),"%s%s",binding_name(a,action),tails[i]);line=label;}
            if(a->help_tab==3&&i<2){snprintf(label,sizeof(label),"HOLD %s + %s: %s",binding_name(a,3),i?"R":"L",i?"REDO":"UNDO");line=label;}
            text(bottom,w,h,8,64+i*12,line,i&1?UI_MUTED:UI_TEXT);
        }
        button(bottom,w,h,4,h-28,w/2-8,24,"REMAP",false);button(bottom,w,h,w/2+4,h-28,w/2-8,24,"SETTINGS",false);
    }else if(a->page==10){
        for(unsigned i=0;i<4;i++){snprintf(label,sizeof(label),"%s: %s",control_names[i],action_names[(a->draft_bindings>>(i*2))&3]);button(bottom,w,h,4,32+(int)i*26,w-8,24,label,false);}
        text(bottom,w,h,8,140,"TAP TO CYCLE - CONFLICTS SWAP",UI_MUTED);
        button(bottom,w,h,4,h-28,w/2-8,24,"DEFAULTS",false);button(bottom,w,h,w/2+4,h-28,w/2-8,24,"SETTINGS",false);
    }else if(a->page==11){
        const char *names[]={"HEX","R","G","B","A"};for(int i=0;i<5;i++)button(bottom,w,h,i*w/5+2,30,w/5-4,24,names[i],a->numeric_mode==(unsigned)i);
        snprintf(label,sizeof(label),"%s%s",a->numeric_mode?"0-255: ":"RRGGBB[AA]: ",a->numeric);text(bottom,w,h,8,62,label,UI_ACCENT);
        const char *keys="0123456789ABCDEF";for(int i=0;i<16;i++){char key[2]={keys[i],0};button(bottom,w,h,(i%4)*w/4+2,80+(i/4)*16,w/4-4,15,key,false);}
        button(bottom,w,h,4,h-28,w/3-8,24,"DELETE",false);button(bottom,w,h,w/3+4,h-28,w/3-8,24,"BACK",false);button(bottom,w,h,2*w/3+4,h-28,w/3-8,24,"SET",true);
    }else if(a->page==12){
        const char *items[]={"NUMERIC COLOR","SWAP FOREGROUND / BACKGROUND","RECENT COLORS","IMPORT PALETTE","EXPORT PALETTE","DEFAULT PALETTE"};for(int i=0;i<6;i++)button(bottom,w,h,4,30+i*22,w-8,20,items[i],false);button(bottom,w,h,4,h-24,w-8,22,"BACK TO PICKER",false);
    }else if(a->page==13){
        text(bottom,w,h,8,27,"RECENT COLORS - THIS SESSION",UI_MUTED);for(unsigned i=0;i<a->recent_count;i++){int x=(int)(i%4)*w/4+4,y=40+(int)(i/4)*32;rect(bottom,w,h,x,y,w/4-8,28,UI_EDGE);for(int yy=y+3;yy<y+25;yy++)for(int xx=x+3;xx<x+w/4-11;xx++)bottom[yy*w+xx]=pd_fb(displayed(a->recent[i],xx,yy));}button(bottom,w,h,4,h-28,w-8,24,"BACK",false);
    }else if(a->page==14){const char *items[]={"GPL - OPAQUE RGB ONLY","HEX - RRGGBBAA","PNG - RGBA COLOR STRIP"};for(int i=0;i<3;i++)button(bottom,w,h,4,40+i*32,w-8,28,items[i],false);button(bottom,w,h,4,h-28,w-8,24,"BACK",false);
    }else if(a->page==15){button(bottom,w,h,4,40,w-8,28,"SAVE AS MY DEFAULT",false);button(bottom,w,h,4,76,w-8,28,"USE MY DEFAULT",false);button(bottom,w,h,4,112,w-8,28,"RESTORE BUILT-IN COLORS",false);text(bottom,w,h,8,146,"RESTORE CHANGES THIS PROJECT ONLY",UI_MUTED);button(bottom,w,h,4,h-28,w-8,24,"BACK",false);
    }else if(a->page==16){
        const char *base=strrchr(a->file_source,'/');snprintf(label,sizeof(label),"%.38s",base?base+1:a->file_source);text(bottom,w,h,8,27,label,UI_MUTED);const char *items[]={"RENAME FILE","DUPLICATE FILE","DELETE FILE"};for(int i=0;i<3;i++)button(bottom,w,h,4,40+i*32,w-8,28,items[i],false);button(bottom,w,h,4,h-28,w-8,24,"BACK TO FILES",false);
    }else if(a->page==17){
        for(unsigned i=0;i<a->recent_project_count;i++){const char *base=strrchr(a->recent_projects[i],'/');snprintf(label,sizeof(label),"%.38s",base?base+1:a->recent_projects[i]);button(bottom,w,h,4,30+(int)i*16,w-8,15,label,a->recent_project_selected==(int)i);}if(!a->recent_project_count)text(bottom,w,h,8,50,"NO RECENT PROJECTS YET",UI_MUTED);button(bottom,w,h,4,h-28,w/2-8,24,"OPEN",a->recent_project_selected>=0);button(bottom,w,h,w/2+4,h-28,w/2-8,24,"BACK TO FILES",false);
    }else if(a->page==7){
        text(bottom,w,h,8,30,"DRAWING  MAX MICROSECONDS",UI_ACCENT);text(bottom,w,h,w-8-(int)strlen(PD_BUILD_LABEL)*6,30,PD_BUILD_LABEL,UI_ACCENT);
        snprintf(label,sizeof(label),"IN %u RENDER %u COPY %u",a->input_us,a->render_us,a->present_us);text(bottom,w,h,8,41,label,UI_TEXT);
        snprintf(label,sizeof(label),"SAMPLE GAP %u / N %u",a->gap_us,a->samples);text(bottom,w,h,8,51,label,UI_TEXT);
        text(bottom,w,h,8,68,"MEMORY",UI_ACCENT);snprintf(label,sizeof(label),"%s  MASKS %u KB",a->platform?a->platform:"",a->mask_words*16u/1024u);text(bottom,w,h,8,79,label,UI_MUTED);
        if(a->memory_measured){snprintf(label,sizeof(label),"FREE %u KB  LARGEST %u KB",a->free_kb,a->largest_kb);text(bottom,w,h,8,89,label,UI_TEXT);}else text(bottom,w,h,8,89,a->memory_limit?"NOT MEASURED YET":"MEMORY PROBE UNAVAILABLE",UI_MUTED);
        text(bottom,w,h,8,106,"CARD",UI_ACCENT);if(a->card_read_kbs){snprintf(label,sizeof(label),"WRITE %u KB/S  READ %u KB/S",a->card_write_kbs,a->card_read_kbs);text(bottom,w,h,8,117,label,UI_TEXT);}else if(!a->diag_status[0])text(bottom,w,h,8,117,"NOT MEASURED YET",UI_MUTED);
        if(a->diag_status[0])text(bottom,w,h,8,127,a->diag_status,UI_MUTED);
        text(bottom,w,h,8,140,"INPUT DIRECT",UI_MUTED);
        snprintf(label,sizeof(label),"PEN %u  INK %u",a->input_contacts,a->ink_starts);text(bottom,w,h,8,150,label,UI_MUTED);
        button(bottom,w,h,4,h-28,w-8,24,"RUN MEMORY + CARD TEST",true);
    }else{
        /* Map once per row/column: ARM9 has no hardware integer divide. */
        int left=(int)a->doc.width*a->zoom<w?(w-(int)a->doc.width*a->zoom)/2:0;
        int upper=canvas_top(a)+((int)a->doc.height*a->zoom<canvas_bottom(a)-canvas_top(a)?(canvas_bottom(a)-canvas_top(a)-(int)a->doc.height*a->zoom)/2:0);
        left-=a->subx;upper-=a->suby;
        if(!(a->guides&12)&&!a->subx&&!a->suby){
            for(int y=24;y<h;y++){PdFb *row=bottom+y*w;int wy=a->oy+(y-upper)/a->zoom;
                if(y<upper||wy>=(int)a->doc.height){for(int x=0;x<w;x++)row[x]=pd_fb(0x53616b);continue;}
                for(int x=0;x<left;x++)row[x]=pd_fb(0x53616b);int x=left;
                for(int wx=a->ox;x<w&&wx<(int)a->doc.width;wx++){uint32_t c=canvas_displayed(a,pd_image(&a->doc)[wy*a->doc.stride+wx],wx,wy);int end=x+a->zoom;if(end>w)end=w;int start=x;bool gridrow=a->pixel_grid&&a->zoom>=4&&(y-upper)%a->zoom==0;while(x<end){row[x]=pd_fb((a->pixel_grid&&a->zoom>=4&&(gridrow||x==start))?grid_color(c):c);x++;}}
                while(x<w)row[x++]=pd_fb(0x53616b);
            }
        }else for(int y=24;y<h;y++){
            PdFb *row=bottom+y*w;int dy=y-upper,wy=a->oy+(dy>=0?dy/a->zoom:-((-dy+a->zoom-1)/a->zoom));
            if(!(a->guides&8)&&(y<upper||wy<0||wy>=(int)a->doc.height)){for(int x=0;x<w;x++)row[x]=pd_fb(0x53616b);continue;}
            int sy=(wy%(int)a->doc.height+(int)a->doc.height)%(int)a->doc.height;
            for(int x=0;x<w;){int dx=x-left,wx=a->ox+(dx>=0?dx/a->zoom:-((-dx+a->zoom-1)/a->zoom));int phase=(dx%a->zoom+a->zoom)%a->zoom,end=x+a->zoom-phase;if(end>w)end=w;
                bool valid=(a->guides&4)||(x>=left&&wx>=0&&wx<(int)a->doc.width);int sx=(wx%(int)a->doc.width+(int)a->doc.width)%(int)a->doc.width;
                uint32_t c=valid?canvas_displayed(a,pd_image(&a->doc)[sy*a->doc.stride+sx],sx,sy):0x53616b;
                bool gridrow=a->pixel_grid&&a->zoom>=4&&dy%a->zoom==0;int start=x;while(x<end){row[x]=pd_fb((valid&&a->pixel_grid&&a->zoom>=4&&(gridrow||(x==start&&!phase)))?grid_color(c):c);x++;}
            }
        }
    }
    if(!a->page||a->page==21||a->page==23||a->page==24){canvas_border(a,bottom);selection_overlay(a,bottom);}
    if(!a->page||a->page==21||a->page==23||a->page==24||a->page==26)strip_draw(a,bottom,w,h);
    if(a->page==26){int left=a->handed?w-224:0;rect(bottom,w,h,left,24,224,162,UI_EDGE);rect(bottom,w,h,left+2,26,220,158,UI_BG);text(bottom,w,h,left+8,33,"FILL OPTIONS",UI_TEXT);button(bottom,w,h,left+4,48,216,24,a->fill_global?"GLOBAL / REPLACE COLOR":"CONTIGUOUS REGION",a->fill_global);button(bottom,w,h,left+4,76,216,24,a->fill_visible?"MATCH: VISIBLE IMAGE":"MATCH: ACTIVE LAYER",a->fill_visible);button(bottom,w,h,left+4,104,40,24,"-",false);snprintf(label,sizeof(label),"TOLERANCE %u",a->fill_tolerance);text(bottom,w,h,left+54,112,label,UI_TEXT);button(bottom,w,h,left+180,104,40,24,"+",false);rect(bottom,w,h,left+16,140,192,6,UI_PANEL);int sx=left+16+(int)a->fill_tolerance*191/255;rect(bottom,w,h,sx-2,132,4,22,UI_TEXT);button(bottom,w,h,left+4,158,216,24,"DONE",false);}
    if(a->page==24){int left=a->handed?w-224:0;rect(bottom,w,h,left,24,224,162,UI_EDGE);rect(bottom,w,h,left+2,26,220,158,UI_BG);text(bottom,w,h,left+8,33,"SHAPE OPTIONS",UI_TEXT);const char *names[]={"LINE","RECT","ELLIPSE"};for(int i=0;i<3;i++)button(bottom,w,h,left+4+i*74,48,70,24,names[i],a->tool==(unsigned)(5+i));button(bottom,w,h,left+4,76,216,24,a->tool==5?"LINE":a->shape_filled?"FILLED":"OUTLINE",a->shape_filled&&a->tool!=5);button(bottom,w,h,left+4,104,40,24,"-",false);snprintf(label,sizeof(label),"SIZE %u",a->brush_size[0]);text(bottom,w,h,left+80,112,label,UI_TEXT);button(bottom,w,h,left+180,104,40,24,"+",false);rect(bottom,w,h,left+16,140,192,6,UI_PANEL);int sx=left+16+(int)(a->brush_size[0]-1)*191/15;rect(bottom,w,h,sx-2,132,4,22,UI_TEXT);button(bottom,w,h,left+4,158,216,24,"DONE",false);}
    if(a->page==25){int sx=advanced_sheet_x(a),sw=advanced_sheet_w(a),ix=sx+8,iw=sw-16;rect(bottom,w,h,sx,28,sw,h-36,UI_EDGE);rect(bottom,w,h,sx+1,29,sw-2,h-38,UI_BG);text(bottom,w,h,ix,34,"TRANSFORM SELECTION",UI_ACCENT);text(bottom,w,h,sx+sw-16,34,"X",UI_TEXT);const char *names[]={"FLIP H","FLIP V","ROTATE 90","DUP TO LAYER","SCALE 2X","SCALE 3X","SCALE 4X","HALF SIZE"};for(int i=0;i<8;i++)button(bottom,w,h,ix+(i%2)*(iw/2+4),52+(i/2)*25,iw/2-4,22,names[i],false);button(bottom,w,h,ix,158,iw-4,20,"BACK TO SELECTION",false);}
    if(a->page==23){int left=a->handed?w-224:0;rect(bottom,w,h,left,24,224,164,UI_EDGE);rect(bottom,w,h,left+2,26,220,160,UI_BG);text(bottom,w,h,left+8,33,"SELECTION",UI_TEXT);button(bottom,w,h,left+164,28,56,18,"MORE",false);
        button(bottom,w,h,left+4,48,106,24,"RECTANGLE",!a->selection_shape);button(bottom,w,h,left+114,48,106,24,"LASSO",a->selection_shape!=0);
        const char *modes[]={"REPLACE","ADD","REMOVE"},*actions[]={"COPY","CUT","PASTE"};for(int i=0;i<3;i++){button(bottom,w,h,left+4+i*74,76,70,24,modes[i],a->selection_mode==(unsigned)i);button(bottom,w,h,left+4+i*74,104,70,24,actions[i],false);}
        button(bottom,w,h,left+4,132,106,24,"SELECT ALL",false);button(bottom,w,h,left+114,132,106,24,"DESELECT",false);button(bottom,w,h,left+4,160,106,24,"DELETE",false);button(bottom,w,h,left+114,160,106,24,"DONE",false);
    }
    if(a->page==28&&!a->confirm){
        rect(bottom,w,h,0,24,w,h-24,UI_BG);
        const char *modes[]={"CACHED","RECENT","FAVORITES","MY UPLOADS"};char mode_label[32];snprintf(mode_label,sizeof(mode_label),"VIEW: %s v",modes[a->community_mode]);button(bottom,w,h,4,28,50,18,"PAIR",false);button(bottom,w,h,58,28,w-116,18,mode_label,true);button(bottom,w,h,w-58,28,54,18,"REFRESH",false);
        int cw=w/3;if(a->community_mode){for(unsigned i=0;i<a->community_page.count;i++){int xx=(int)(i%3)*cw,yy=48+(int)(i/3)*48;PdCommunityArt *art=&a->community_page.items[i];rect(bottom,w,h,xx+3,yy,cw-6,46,a->selected==(int)i?UI_SELECTED:UI_PANEL);if(a->community_mode!=3&&a->gallery_ready[i])for(int ty=0;ty<32;ty++)for(int tx=0;tx<32;tx++)bottom[(yy+2+ty)*w+xx+(cw-32)/2+tx]=pd_fb(displayed(a->gallery_thumbs[i][ty*32+tx],tx,ty));else{const char *tag=a->community_mode==3&&art->status[0]?art->status:"LOADING";text(bottom,w,h,xx+8,yy+14,tag,UI_MUTED);}snprintf(label,sizeof(label),"%.*s",(cw-12)/6,art->title);text(bottom,w,h,xx+6,yy+36,label,UI_TEXT);}
            if(!a->community_page.count)text(bottom,w,h,8,68,a->community_loading?"LOADING COMMUNITY PAGE...":a->community_page.profile_required?"CREATE AN ARTIST PROFILE ON WEBSITE":a->community_status[0]?a->community_status:"NO ITEMS",UI_TEXT);
            if(a->community_page.has_total)snprintf(label,sizeof(label),"%u ITEMS / PAGE %u",a->community_page.total,a->gallery_page+1);else snprintf(label,sizeof(label),"PAGE %u - %s",a->gallery_page+1,a->community_mode==3?"YOUR SUBMISSIONS":"COMMUNITY");
        }else{for(unsigned i=0;i<a->gallery_count;i++){int xx=(int)(i%3)*cw,yy=48+(int)(i/3)*48;
            rect(bottom,w,h,xx+3,yy,cw-6,46,a->selected==(int)(a->gallery_page*6+i)?UI_SELECTED:UI_PANEL);
            if(a->gallery_ready[i])for(int y=0;y<32;y++)for(int x=0;x<32;x++)bottom[(yy+2+y)*w+xx+(cw-32)/2+x]=pd_fb(displayed(a->gallery_thumbs[i][y*32+x],x,y));
            else text(bottom,w,h,xx+8,yy+14,"UNREADABLE",UI_MUTED);
            snprintf(label,sizeof(label),"%.*s",(cw-12)/6,a->listing.entries[a->gallery_page*6+i].name);text(bottom,w,h,xx+6,yy+36,label,UI_TEXT);
        }
            if(!a->gallery_count){text(bottom,w,h,8,65,"NO CACHED ART YET",UI_TEXT);text(bottom,w,h,8,83,"COPY PNGS (UP TO 128 X 128) TO",UI_MUTED);text(bottom,w,h,8,97,"PIXELDOODLE/COMMUNITY",UI_TEXT);}
            snprintf(label,sizeof(label),"%u ITEMS / PAGE %u%s",a->listing.count,a->gallery_page+1,a->listing.truncated?" (LIMIT)":"");
        }text(bottom,w,h,8,h-40,label,UI_MUTED);
        button(bottom,w,h,4,h-28,36,24,"<",false);if(a->community_mode==1||a->community_mode==2){bool selected=a->selected>=0&&(unsigned)a->selected<a->community_page.count;button(bottom,w,h,44,h-28,(w-88)/2,24,"IMPORT",selected&&a->gallery_ready[a->selected]);button(bottom,w,h,44+(w-88)/2,h-28,(w-88)/2,24,a->community_mode==1?"FAVORITE":"REMOVE FAV",selected&&!a->community_action_busy);}else button(bottom,w,h,44,h-28,w-88,24,a->community_mode==3?"DETAILS":"OPEN COPY",!a->community_mode&&a->preview_ready);button(bottom,w,h,w-40,h-28,36,24,">",false);
        community_dropdown_draw(a,bottom,w,h);
    }
    if(a->page==29){
        rect(bottom,w,h,0,24,w,h-24,UI_BG);text(bottom,w,h,8,36,"CONNECT A HANDHELD",UI_ACCENT);
        text(bottom,w,h,8,50,"Approval requests browse, favorites and uploads",UI_TEXT);text(bottom,w,h,8,62,"Old grant stays active until approval succeeds.",UI_TEXT);
        if(a->pairing_user_code[0]){text(bottom,w,h,8,118,"ENTER THIS CODE AT PIXELS.7DB.PW",UI_MUTED);text(bottom,w,h,8,140,a->pairing_user_code,UI_ACCENT);}
        text(bottom,w,h,8,164,a->pairing_message,UI_TEXT);
        if(a->pairing_state==2)button(bottom,w,h,4,78,w-8,30,"CONNECTED - RE-PAIR TO EXTEND",false);
        else if(a->pairing_state==5)button(bottom,w,h,4,78,w-8,30,"RE-PAIR / REFRESH GRANT",false);
        else if(a->pairing_state==1)button(bottom,w,h,4,78,w-8,30,"WAITING FOR APPROVAL",true);
        else if(a->pairing_state==6)button(bottom,w,h,4,78,w-8,30,"CONNECTING...",true);
        else if(a->pairing_state==4||a->pairing_state==3)button(bottom,w,h,4,78,w-8,30,"PAIRING UNAVAILABLE",false);
        else button(bottom,w,h,4,78,w-8,30,"START PAIRING",false);
        button(bottom,w,h,4,h-32,w-8,26,"BACK TO COMMUNITY",false);
    }
    if(a->page==27){int sx=advanced_sheet_x(a),sw=advanced_sheet_w(a),ix=sx+8,iw=sw-16;rect(bottom,w,h,sx,28,sw,h-36,UI_EDGE);rect(bottom,w,h,sx+1,29,sw-2,h-38,UI_BG);
        text(bottom,w,h,ix,34,"BRUSH DETAILS",UI_ACCENT);text(bottom,w,h,sx+sw-16,34,"X",UI_TEXT);
        const char *patterns[]={"SOLID","25%","50%","75%"};text(bottom,w,h,ix,52,"DITHER",UI_MUTED);
        int bw=(iw-12)/4;for(unsigned i=0;i<4;i++)button(bottom,w,h,ix+(int)i*(bw+4),60,bw,20,patterns[i],a->brush_pattern==i);
        text(bottom,w,h,ix,90,"PRESETS",UI_MUTED);
        for(unsigned i=0;i<4;i++){
            int x=ix+(int)i*(bw+4),y=100;unsigned char *p=a->brush_presets[i];
            rect(bottom,w,h,x,y,bw,34,a->brush_preset==i?UI_ACCENT:UI_EDGE);rect(bottom,w,h,x+1,y+1,bw-2,32,a->brush_preset==i?UI_SELECTED:UI_PANEL);
            brush_tip_preview(bottom,w,h,x+4,y+3,bw-8,18,p[0],p[1],p[3],a->color);
            snprintf(label,sizeof(label),"%u",i+1);text(bottom,w,h,x+bw/2-3,y+24,label,UI_TEXT);
        }
        unsigned char *p=a->brush_presets[a->brush_preset];snprintf(label,sizeof(label),"%u PX  %s  %s%s",p[0],p[1]?"ROUND":"SQUARE",patterns[p[3]],p[2]&&p[0]==1&&!p[3]?"  PP":"");text(bottom,w,h,ix,138,label,UI_TEXT);
        button(bottom,w,h,ix,148,iw/2-2,20,"LOAD",false);button(bottom,w,h,ix+iw/2+2,148,iw-iw/2-2,20,"STORE",false);button(bottom,w,h,ix,170,iw-4,12,"BACK TO BRUSH",false);
    }
    if(a->page==21){int left=a->handed?w-200:0;rect(bottom,w,h,left,24,200,a->brush_edit?114:168,UI_EDGE);rect(bottom,w,h,left+2,26,196,a->brush_edit?110:164,UI_BG);icon(bottom,w,h,left+8,32,6+a->brush_edit,UI_TEXT);text(bottom,w,h,left+32,36,a->brush_edit?"ERASER":"PENCIL",UI_TEXT);button(bottom,w,h,left+140,28,56,24,"TOOLS",false);
        button(bottom,w,h,left+4,56,36,28,"-",false);snprintf(label,sizeof(label),"%u PX",a->brush_size[a->brush_edit]);text(bottom,w,h,left+72,66,label,UI_TEXT);button(bottom,w,h,left+160,56,36,28,"+",false);
        rect(bottom,w,h,left+16,94,168,8,UI_PANEL);int sx=left+16+(int)(a->brush_size[a->brush_edit]-1)*167/15;rect(bottom,w,h,left+16,94,sx-left-16,8,UI_SELECTED);rect(bottom,w,h,sx-2,88,4,20,UI_TEXT);button(bottom,w,h,left+4,112,192,22,a->brush_round[a->brush_edit]?"TIP: ROUND":"TIP: SQUARE",false);if(!a->brush_edit)button(bottom,w,h,left+4,140,192,24,a->brush_pattern?"PIXEL PERFECT: SOLID ONLY":a->pixel_perfect?"PIXEL PERFECT: ON (1PX)":"PIXEL PERFECT: OFF",a->pixel_perfect&&!a->brush_pattern);if(!a->brush_edit)button(bottom,w,h,left+4,166,192,22,"DITHER / PRESETS",a->brush_pattern!=0);
    }
    a->picker_field=a->page==2&&!a->confirm&&!a->color_tab;
    if(a->page==2&&!a->confirm){
        render_palette_top(a,top,tw,th);
        a->present_top=true;a->px0=0;a->py0=0;a->px1=w;a->py1=h;a->partial=false;a->redraw=false;return;
    }
    if(a->toolbox&&!a->page&&!a->confirm){
        int rail=tool_rail_width(a),row_h=tool_row_height(a),px=tool_panel_x(a),pw=tool_panel_w(a),ix=px+8,iw=pw-16;
        rect(bottom,w,h,2,24,rail+4,h-32,UI_EDGE);rect(bottom,w,h,3,25,rail+2,h-34,UI_BG);
        const char *names[]={"PENCIL","ERASER","SAMPLE","FILL","SELECT","LINE","RECT","ELLIPSE","GUIDES"};
        const unsigned glyphs[]={6,7,8,11,12,21,22,23,10};
        for(unsigned i=0;i<9;i++){
            int y=26+(int)i*row_h;bool active=a->tool_panel==i;
            rect(bottom,w,h,4,y,rail-2,row_h-1,active?UI_ACCENT:UI_EDGE);rect(bottom,w,h,5,y+1,rail-4,row_h-3,active?UI_SELECTED:UI_PANEL);
            icon(bottom,w,h,7,y+(row_h-16)/2,glyphs[i],active?UI_TEXT:UI_ACCENT);
            text(bottom,w,h,28,y+(row_h-7)/2,names[i],UI_TEXT);
        }
        if(a->tool_options_open){
            rect(bottom,w,h,px-1,26,pw+2,h-34,UI_EDGE);rect(bottom,w,h,px,27,pw,h-36,UI_BG);
            static const char *const dither_labels[]={"DITHER: SOLID","DITHER: 25%","DITHER: 50%","DITHER: 75%"};
            const char *title=a->tool_panel==0?"PENCIL OPTIONS":a->tool_panel==1?"ERASER OPTIONS":a->tool_panel==3?"FILL OPTIONS":a->tool_panel==4?"SELECT OPTIONS":a->tool_panel==8?"GUIDES":"SHAPE OPTIONS";
            text(bottom,w,h,ix,31,title,UI_ACCENT);text(bottom,w,h,px+pw-16,31,"X",UI_TEXT);
            if(a->tool_panel==0||a->tool_panel==1){
                brush_tip_preview(bottom,w,h,ix,42,iw,22,a->brush_size[a->brush_edit],a->brush_round[a->brush_edit],a->brush_pattern,a->color);
                snprintf(label,sizeof(label),"SIZE  %u PX",a->brush_size[a->brush_edit]);text(bottom,w,h,ix,68,label,UI_TEXT);
                button(bottom,w,h,ix,78,24,20,"-",false);button(bottom,w,h,px+pw-32,78,24,20,"+",false);
                int track=ix+30,range=iw-60,sx=track+(int)(a->brush_size[a->brush_edit]-1)*range/15;rect(bottom,w,h,track,85,range,6,UI_PANEL);rect(bottom,w,h,track,85,sx-track,6,UI_SELECTED);rect(bottom,w,h,sx-2,82,4,12,UI_TEXT);
                button(bottom,w,h,ix,110,iw-4,20,a->brush_round[a->brush_edit]?"TIP: ROUND":"TIP: SQUARE",false);
                if(a->tool_panel==0){button(bottom,w,h,ix,134,iw-4,20,a->pixel_perfect?"PIXEL PERFECT: ON":"PIXEL PERFECT: OFF",a->pixel_perfect);button(bottom,w,h,ix,158,iw-4,18,"DITHER / PRESETS",a->brush_pattern!=0);}
            }else if(a->tool_panel==3){
                button(bottom,w,h,ix,44,iw-4,20,a->fill_global?"REGION: GLOBAL":"REGION: CONTIGUOUS",a->fill_global);
                button(bottom,w,h,ix,68,iw-4,20,a->fill_visible?"MATCH: VISIBLE":"MATCH: ACTIVE",a->fill_visible);
                button(bottom,w,h,ix,92,24,20,"-",false);button(bottom,w,h,px+pw-32,92,24,20,"+",false);
                int gap=pw-32-32;snprintf(label,sizeof(label),"TOLERANCE %u",a->fill_tolerance);if((int)strlen(label)*6>gap-8)snprintf(label,sizeof(label),"TOL %u",a->fill_tolerance);
                text(bottom,w,h,ix+24+(gap-(int)strlen(label)*6)/2,99,label,UI_TEXT);
                int sx=ix+(int)a->fill_tolerance*(iw-5)/255;rect(bottom,w,h,ix,120,iw-4,6,UI_PANEL);rect(bottom,w,h,ix,120,sx-ix,6,UI_SELECTED);rect(bottom,w,h,sx-2,114,4,18,UI_TEXT);
                button(bottom,w,h,ix,140,iw-4,20,dither_labels[a->fill_pattern&3],a->fill_pattern!=0);
                button(bottom,w,h,ix,164,iw-4,18,"DONE",true);
            }else if(a->tool_panel==4){
                button(bottom,w,h,ix,44,iw/2-2,20,"RECT",a->selection_shape==0);button(bottom,w,h,ix+iw/2+2,44,iw-iw/2-2,20,"LASSO",a->selection_shape!=0);
                int aw=(iw-8)/3;const char *modes[]={"REP","ADD","SUB"};for(int i=0;i<3;i++)button(bottom,w,h,ix+i*(aw+4),68,aw,20,modes[i],a->selection_mode==(unsigned)i);
                const char *acts[]={"COPY","CUT","PASTE"};for(int i=0;i<3;i++)button(bottom,w,h,ix+i*(aw+4),92,aw,20,acts[i],false);
                button(bottom,w,h,ix,116,iw/2-2,20,(int)strlen("SELECT ALL")*6+8<=iw/2-2?"SELECT ALL":"SEL ALL",false);button(bottom,w,h,ix+iw/2+2,116,iw-iw/2-2,20,"DESELECT",false);
                button(bottom,w,h,ix,140,iw/2-2,20,"DELETE",false);button(bottom,w,h,ix+iw/2+2,140,iw-iw/2-2,20,"TRANSFORM",false);
            }else if(a->tool_panel>=5&&a->tool_panel<8){
                if(a->tool!=5)button(bottom,w,h,ix,44,iw-4,20,a->shape_filled?"FILLED":"OUTLINE",a->shape_filled);
                snprintf(label,sizeof(label),"SIZE  %u PX",a->brush_size[0]);text(bottom,w,h,ix,70,label,UI_TEXT);
                button(bottom,w,h,ix,82,24,20,"-",false);button(bottom,w,h,px+pw-32,82,24,20,"+",false);
                int range=iw-60,sx=ix+30+(int)(a->brush_size[0]-1)*range/15;rect(bottom,w,h,ix+30,88,range,6,UI_PANEL);rect(bottom,w,h,ix+30,88,sx-ix-30,6,UI_SELECTED);rect(bottom,w,h,sx-2,82,4,18,UI_TEXT);
                button(bottom,w,h,ix,110,iw-4,20,dither_labels[a->shape_pattern&3],a->shape_pattern!=0);
            }else if(a->tool_panel==8){
                const char *guide_names[]={"PIXEL GRID","SYMMETRY X","SYMMETRY Y","TILE X","TILE Y"};
                for(int i=0;i<5;i++){bool on=i?(a->guides&(1u<<(i-1)))!=0:a->pixel_grid;snprintf(label,sizeof(label),"%s: %s",guide_names[i],on?"ON":"OFF");button(bottom,w,h,ix,44+i*22,iw-4,20,label,on);}
            }
        }
    }
    if(!a->page&&!a->confirm&&a->onion_sheet)onion_sheet_draw(a,bottom,w,h);
    if(a->zoom_overlay){
        int zx=zoom_left(a);rect(bottom,w,h,zx,40,48,56,0x0d7a75);rect(bottom,w,h,zx,112,48,56,0x0d7a75);
        rect(bottom,w,h,zx+12,66,24,4,0xffffff);rect(bottom,w,h,zx+22,56,4,24,0xffffff);
        rect(bottom,w,h,zx+12,138,24,4,0xffffff);
    }
    if(a->page==31){
        const char *scopes[]={"SELECTION","CURRENT LAYER","VISIBLE LAYERS"};
        int header_h=22,top_y=30,bottom=th-50,panel_w=(tw-24)/2,image_y=top_y+20,image_h=bottom-image_y;
        rect(top,tw,th,0,0,tw,header_h,0x182126);text(top,tw,th,8,7,"COLOR PREVIEW",UI_ACCENT);
        int scope_x=tw-8-(int)strlen(scopes[a->adjust_scope])*6;text(top,tw,th,scope_x,7,scopes[a->adjust_scope],UI_TEXT);
        rect(top,tw,th,7,top_y-1,panel_w+2,panel_w>0?bottom-top_y+1:1,UI_EDGE);rect(top,tw,th,tw-9-panel_w,top_y-1,panel_w+2,bottom-top_y+1,UI_EDGE);
        text(top,tw,th,12,top_y+4,"BEFORE",UI_MUTED);text(top,tw,th,tw-4-panel_w,top_y+4,a->adjust_preview_stale?"AFTER - ADJUSTING":"AFTER",a->adjust_preview_stale?UI_ACCENT:UI_TEXT);
        unsigned dw=a->doc.width,dh=a->doc.height,max=dw>dh?dw:dh;int avail=panel_w-8;int size=image_h<avail?image_h:avail;int mw=(int)(dw*(unsigned)size/max),mh=(int)(dh*(unsigned)size/max);if(mw<1)mw=1;if(mh<1)mh=1;
        int before_left=8+(panel_w-mw)/2,after_left=tw-8-panel_w+(panel_w-mw)/2,image_center_y=image_y+image_h/2-mh/2;
        for(int y=0;y<mh;y++)for(int x=0;x<mw;x++){int wx=x*(int)dw/mw,wy=y*(int)dh/mh;unsigned i=wy*a->doc.stride+wx;
            top[(image_center_y+y)*tw+before_left+x]=pd_fb(displayed(pd_image(&a->doc)[i],wx,wy));
            if(a->adjust_preview_ready)top[(image_center_y+y)*tw+after_left+x]=pd_fb(displayed(pd_before(&a->doc)[i],wx,wy));
        }
        if(!a->adjust_preview_ready)text(top,tw,th,tw-8-panel_w+12,image_center_y+image_h/2-4,"NO PREVIEW",UI_MUTED);
        snprintf(label,sizeof(label),"H %+d   S %+d   B %+d",a->adjust_hue,a->adjust_saturation,a->adjust_brightness);text(top,tw,th,8,th-44,label,UI_TEXT);
        text(top,tw,th,8,th-31,a->adjust_preview_stale?"LIFT STYLUS TO UPDATE AFTER VIEW":a->adjust_preview_ready?"LIVE LAYER-STACK PREVIEW - ART UNCHANGED":"CHOOSE A VALID SCOPE TO PREVIEW",a->adjust_preview_stale?UI_ACCENT:UI_MUTED);
        text(top,tw,th,8,th-18,"APPLY COMMITS ONE UNDO STEP",UI_TEXT);
        text(top,tw,th,8,th-8,"CANCEL LEAVES EVERY PIXEL UNCHANGED",UI_MUTED);a->redraw=false;return;
    }
    if(a->page==28&&a->community_mode&&a->preview_attempted&&a->selected>=0&&(unsigned)a->selected<a->community_page.count){PdCommunityArt *art=&a->community_page.items[a->selected];text(top,tw,th,8,6,a->community_mode==3?"MY UPLOAD / REVIEW STATUS":"ONLINE GALLERY / APPROVED ART",UI_ACCENT);text(top,tw,th,8,24,art->title,UI_TEXT);if(a->community_mode==3){snprintf(label,sizeof(label),"STATUS: %.15s",art->status);text(top,tw,th,8,44,label,UI_ACCENT);snprintf(label,sizeof(label),"%.54s",art->reason[0]?art->reason:"No review note");text(top,tw,th,8,62,label,UI_MUTED);}else{snprintf(label,sizeof(label),"BY %.35s",art->artist);text(top,tw,th,8,44,label,UI_MUTED);}snprintf(label,sizeof(label),"%u X %u PIXELS",art->width,art->height);text(top,tw,th,8,a->community_mode==3?84:60,label,UI_TEXT);if(a->community_mode!=3&&a->gallery_ready[a->selected]){int size=th-112;if(size>128)size=128;if(size<32)size=32;int left=(tw-size)/2;for(int py=0;py<size;py++)for(int px=0;px<size;px++)top[(84+py)*tw+left+px]=pd_fb(displayed(a->gallery_thumbs[a->selected][(py*32/size)*32+(px*32/size)],px,py));}text(top,tw,th,8,th-24,a->community_mode==3?"STATUS AND REVIEW REASON FROM SERVER":"PUBLIC PREVIEW; DOWNLOAD TO IMPORT",UI_MUTED);a->redraw=false;return;}
    if((a->page==4||a->page==17)&&a->preview_attempted){render_file_preview(a,top,tw,th);a->redraw=false;return;}
    if(a->page==28&&a->preview_attempted){
        text(top,tw,th,8,6,a->page==28?"CACHED ART - OFFLINE PREVIEW":"FILE PREVIEW - CURRENT ART RETAINED",UI_ACCENT);text(top,tw,th,8,24,a->preview_name,UI_TEXT);
        int size=th-80;if(size>128)size=128;int left=(tw-size)/2;if(a->preview_ready){for(int y=0;y<size;y++)for(int x=0;x<size;x++)top[(42+y)*tw+left+x]=pd_fb(displayed(a->file_preview[(y*64/size)*64+x*64/size],x,y));}else text(top,tw,th,8,60,a->preview_error,UI_MUTED);
        if(a->preview_palette_count){snprintf(label,sizeof(label),"PALETTE: %u COLORS",a->preview_palette_count);text(top,tw,th,8,th-34,label,UI_ACCENT);}
        text(top,tw,th,8,th-22,a->preview_palette_count?"IMPORT FROM THE PALETTE MENU":a->page==17&&a->recent_files_unsaved?"RECENT LIST NOT SAVED TO DISK":"SELECT OPEN TO REPLACE CURRENT ART",UI_MUTED);a->redraw=false;return;
    }
    selection_toolbar(a,bottom);
    if(a->selection_drag){a->present_top=false;a->redraw=false;return;}
    if(a->page==1&&!a->confirm){if(a->layers_tab)render_animation_top(a,top,tw,th);else render_layer_top(a,top,tw,th);a->redraw=false;return;}
    bool solo=a->page==5&&a->name_action==2;
    rect(top,tw,th,0,0,tw,20,0x182126);
    const char *project_name=strrchr(a->project,'/');project_name=project_name?project_name+1:a->project;
    snprintf(label,sizeof(label),"L%u  %dX",a->doc.active+1,a->zoom);int badge_x=tw-8-(int)strlen(label)*6;text(top,tw,th,badge_x,6,label,UI_MUTED);
    text(top,tw,th,8,6,PD_BUILD_LABEL,UI_ACCENT);
    int title_x=16+(int)strlen(PD_BUILD_LABEL)*6,title_chars=(badge_x-title_x-8)/6;if(title_chars<0)title_chars=0;
    snprintf(label,sizeof(label),"%.*s",title_chars,solo?a->doc.names[a->doc.active]:project_name[0]?project_name:"PIXEL DOODLE");text_raw(top,tw,th,title_x,6,label,0xffffff);
    int size=th-76;if(size>128)size=128;unsigned dw=a->doc.width,dh=a->doc.height,max=dw>dh?dw:dh;
    int mw=(int)(dw*(unsigned)size/max),mh=(int)(dh*(unsigned)size/max);if(mw<1)mw=1;if(mh<1)mh=1;int left=(tw-mw)/2;
    rect(top,tw,th,left-2,22,mw+4,mh+4,UI_EDGE);rect(top,tw,th,left-1,23,mw+2,mh+2,UI_BG);
    for(int y=0;y<mh;y++)for(int x=0;x<mw;x++){int wx=x*(int)dw/mw,wy=y*(int)dh/mh;uint32_t p=solo?pd_pixels(&a->doc.layer[a->doc.active])[wy*a->doc.stride+wx]:pd_image(&a->doc)[wy*a->doc.stride+wx];top[(24+y)*tw+left+x]=pd_fb(displayed(p,wx,wy));}
    int vx=left+a->ox*mw/(int)dw,vy=24+a->oy*mh/(int)dh;
    int vw=w/a->zoom,vh=(canvas_bottom(a)-canvas_top(a))/a->zoom;if(vw>(int)dw)vw=(int)dw;if(vh>(int)dh)vh=(int)dh;
    vw=vw*mw/(int)dw;vh=vh*mh/(int)dh;if(vw<1)vw=1;if(vh<1)vh=1;
    int vr=vx+vw,vb=vy+vh;if(vx<left)vx=left;if(vy<24)vy=24;if(vr>left+mw)vr=left+mw;if(vb>24+mh)vb=24+mh;vw=vr-vx;vh=vb-vy;
    if(!solo&&vw>0&&vh>0){rect(top,tw,th,vx,vy,vw,1,0x0d7a75);rect(top,tw,th,vx,vy+vh-1,vw,1,0x0d7a75);
    rect(top,tw,th,vx,vy,1,vh,0x0d7a75);rect(top,tw,th,vx+vw-1,vy,1,vh,0x0d7a75);}
    rect(top,tw,th,0,th-50,tw,1,UI_EDGE);
    bool dirty=pd_dirty(&a->doc)||a->stroke;if(pd_frame_count(&a->doc)>1)snprintf(label,sizeof(label),"F%u/%u  L%u  %uPX",pd_frame_index(&a->doc)+1,pd_frame_count(&a->doc),a->doc.active+1,a->doc.width);else snprintf(label,sizeof(label),"%s  L%u  %uPX",a->platform,a->doc.active+1,a->doc.width);text(top,tw,th,8,th-45,label,UI_ACCENT);
    const char *state=dirty?"UNSAVED":"SAVED";int state_x=tw-8-(int)strlen(state)*6;text(top,tw,th,state_x,th-45,state,dirty?0xffaaa0:UI_ACCENT);
    text(top,tw,th,8,th-32,a->notice,UI_TEXT);
    if(solo)snprintf(label,sizeof(label),"UNDO %u/%u  %u/%u KIB",a->doc.cursor,a->doc.count,(a->doc.history_used+1023)/1024,a->doc.history_capacity/1024);
    if(!solo)snprintf(label,sizeof(label),"B:%s A:%s X:%s Y:%s",action_names[a->bindings&3],action_names[(a->bindings>>2)&3],action_names[(a->bindings>>4)&3],action_names[(a->bindings>>6)&3]);
    text(top,tw,th,8,th-19,label,UI_TEXT);
    text(top,tw,th,8,th-9,solo?"RAW LAYER - BEFORE OPACITY/BLEND":"L/R:ERASE START:SAVE SELECT:MENU",UI_TEXT);a->redraw=false;
}
