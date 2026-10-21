#include <3ds.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "app.h"
static unsigned actions(u32 k){unsigned a=0;
    if(k&(KEY_B|KEY_DDOWN))a|=PD_TOOLS;if(k&(KEY_A|KEY_DLEFT))a|=PD_PAN;
    if(k&(KEY_X|KEY_DUP))a|=PD_SAMPLE;if(k&(KEY_Y|KEY_DRIGHT))a|=PD_ZOOM;
    if(k&(KEY_L|KEY_R))a|=PD_ERASE;if(k&KEY_L)a|=PD_SHOULDER_L;if(k&KEY_R)a|=PD_SHOULDER_R;if(k&KEY_START)a|=PD_SAVE;if(k&KEY_SELECT)a|=PD_MENU;return a;
}
static void present(gfxScreen_t screen,uint32_t *pixels,int w,int x0,int y0,int x1,int y1){
    u8 *fb=gfxGetFramebuffer(screen,GFX_LEFT,NULL,NULL);
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){uint32_t p=pixels[y*w+x];unsigned at=(unsigned)(x*240+239-y)*3;fb[at]=(u8)p;fb[at+1]=(u8)(p>>8);fb[at+2]=(u8)(p>>16);}
}
int main(void){
    gfxInitDefault();gfxSetDoubleBuffering(GFX_TOP,false);gfxSetDoubleBuffering(GFX_BOTTOM,false);
    PdApp *a=calloc(1,sizeof(*a));uint32_t *bottom=malloc(320*240*4),*top=malloc(400*240*4);
    if(!a||!bottom||!top){free(a);free(bottom);free(top);gfxExit();return 1;}
    mkdir("sdmc:/3ds",0777);mkdir("sdmc:/3ds/PixelDoodle",0777);
    bool newer=false;bool expanded=R_SUCCEEDED(APT_CheckNew3DS(&newer))&&newer&&osGetMemRegionSize(MEMREGION_APPLICATION)>=96u*1024u*1024u;
    app_init(a,320,240,"sdmc:/3ds/PixelDoodle",newer?(expanded?"New 3DS":"New 3DS (STD RAM)"):"3DS");a->doc.profile=expanded?4:3;
    pd_pairing_init(a);
    u64 previous=0;bool was_drawing=false;
    while(aptMainLoop()&&!a->exit_requested){
        gspWaitForVBlank();u64 start=svcGetSystemTick();hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();touchPosition t={0};hidTouchRead(&t);
        a->now_ms=(uint32_t)(start/(SYSCLOCK_ARM11/1000));app_input(a,(held&KEY_TOUCH)!=0,t.px,t.py,app_bindings(a,actions(down)),app_bindings(a,actions(held)));
        circlePosition circle;hidCircleRead(&circle);
        int dx=circle.dx>30?(circle.dx-30)/32:circle.dx< -30?(circle.dx+30)/32:0;
        int dy=circle.dy>30?-(circle.dy-30)/32:circle.dy< -30?-(circle.dy+30)/32:0;
        app_pan_pixels(a,dx*2,dy*2);
        u64 input_end=svcGetSystemTick(),render_end=input_end,copy_end=input_end;bool drawing=a->stroke;
        if(a->redraw){app_render(a,bottom,top,400,240);render_end=svcGetSystemTick();
            present(GFX_BOTTOM,bottom,320,a->px0,a->py0,a->px1,a->py1);
            if(a->present_top)present(GFX_TOP,top,400,0,0,400,240);gfxFlushBuffers();copy_end=svcGetSystemTick();}
        if(drawing){
            unsigned in=(unsigned)((input_end-start)*1000000/SYSCLOCK_ARM11),render=(unsigned)((render_end-input_end)*1000000/SYSCLOCK_ARM11),copy=(unsigned)((copy_end-render_end)*1000000/SYSCLOCK_ARM11);
            if(in>a->input_us)a->input_us=in;if(render>a->render_us)a->render_us=render;if(copy>a->present_us)a->present_us=copy;
            if(was_drawing){unsigned gap=(unsigned)((start-previous)*1000000/SYSCLOCK_ARM11);if(gap>a->gap_us)a->gap_us=gap;}a->samples++;
        }
        previous=start;was_drawing=drawing;
    }
    pd_pairing_exit();pd_dispose(&a->doc);free(a);free(bottom);free(top);gfxExit();return 0;
}
