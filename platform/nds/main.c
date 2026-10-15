#include <nds.h>
#include <fat.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "app.h"
static unsigned actions(unsigned k){unsigned a=0;
    if(k&(KEY_B|KEY_DOWN))a|=PD_TOOLS;if(k&(KEY_A|KEY_LEFT))a|=PD_PAN;
    if(k&(KEY_X|KEY_UP))a|=PD_SAMPLE;if(k&(KEY_Y|KEY_RIGHT))a|=PD_ZOOM;
    if(k&(KEY_L|KEY_R))a|=PD_ERASE;if(k&KEY_L)a|=PD_SHOULDER_L;if(k&KEY_R)a|=PD_SHOULDER_R;if(k&KEY_START)a|=PD_SAVE;if(k&KEY_SELECT)a|=PD_MENU;return a;
}
static bool readable(const char *name){DIR *d=opendir(name);if(!d)return false;closedir(d);return true;}
static void present(uint16_t *fb,uint32_t *p,int x0,int y0,int x1,int y1){for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){unsigned i=(unsigned)(y*256+x);uint32_t c=p[i];fb[i]=(uint16_t)(0x8000|((c>>19)&31)|((c>>6)&992)|((c<<7)&31744));}}
int main(void){
    videoSetMode(MODE_5_2D);vramSetBankA(VRAM_A_MAIN_BG);int bg=bgInit(3,BgType_Bmp16,BgSize_B16_256x256,0,0);
    videoSetModeSub(MODE_5_2D);vramSetBankC(VRAM_C_SUB_BG);int sub=bgInitSub(3,BgType_Bmp16,BgSize_B16_256x256,0,0);lcdMainOnBottom();
    PdApp *a=calloc(1,sizeof(*a));uint32_t *bottom=malloc(256*192*4),*top=malloc(256*192*4);
    if(!a||!bottom||!top){free(a);free(bottom);free(top);return 1;}
    bool twl=isDSiMode(),mounted=fatInitDefault();const char *dir="";
    if(mounted&&readable("fat:/")){mkdir("fat:/PixelDoodle",0777);if(readable("fat:/PixelDoodle"))dir="fat:/PixelDoodle";}
    else if(mounted&&twl&&readable("sd:/")){mkdir("sd:/PixelDoodle",0777);if(readable("sd:/PixelDoodle"))dir="sd:/PixelDoodle";}
    app_init(a,256,192,dir,twl?"DSi TWL":"DS NTR");a->doc.profile=twl?2:1;
    cpuStartTiming(0);uint32_t previous=0,clock_previous=0;uint64_t elapsed_us=0;bool was_drawing=false;
    while(!a->exit_requested){
        cothread_yield_irq(IRQ_VBLANK);uint32_t start=cpuGetTiming();scanKeys();unsigned down=keysDown(),held=keysHeld();touchPosition t={0};touchRead(&t);
        elapsed_us+=timerTicks2usec(start-clock_previous);clock_previous=start;a->now_ms=(uint32_t)(elapsed_us/1000);app_input(a,(held&KEY_TOUCH)!=0,t.px,t.py,app_bindings(a,actions(down)),app_bindings(a,actions(held)));
        uint32_t input_end=cpuGetTiming(),render_end=input_end,copy_end=input_end;
        bool drawing=a->stroke;
        if(a->redraw){app_render(a,bottom,top,256,192);render_end=cpuGetTiming();
            present(bgGetGfxPtr(bg),bottom,a->px0,a->py0,a->px1,a->py1);
            if(a->present_top)present(bgGetGfxPtr(sub),top,0,0,256,192);copy_end=cpuGetTiming();}
        if(drawing){
            unsigned in=timerTicks2usec(input_end-start),render=timerTicks2usec(render_end-input_end),copy=timerTicks2usec(copy_end-render_end);
            if(in>a->input_us)a->input_us=in;if(render>a->render_us)a->render_us=render;if(copy>a->present_us)a->present_us=copy;
            if(was_drawing){unsigned gap=timerTicks2usec(start-previous);if(gap>a->gap_us)a->gap_us=gap;}a->samples++;
        }
        previous=start;was_drawing=drawing;
    }
    cpuEndTiming();pd_dispose(&a->doc);free(a);free(bottom);free(top);return 0;
}
