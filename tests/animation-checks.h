#include "animation.h"
#include "version.h"
#include "gif.h"
#include "share.h"

static void gif_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d));assert(d);pd_init(d);assert(pd_create(d,128,128,1));
    uint32_t random=123;
    for(unsigned frame=0;frame<3;frame++){
        if(frame)assert(pd_frame_add(d,false));assert(pd_frame_set_duration(d,frame==0?105:frame==1?250:500));
        assert(pd_begin(d));for(unsigned y=0;y<128;y++)for(unsigned x=0;x<128;x++){random=random*1664525u+1013904223u;unsigned c=(random>>16)%251;pd_pixels(&d->layer[0])[y*128+x]=frame==2?0:((x+y)%7==0?0:0xff000000u|(c<<16)|((250-c)<<8)|c);}bool changed=pd_commit(d);assert(changed||frame==2);
        pd_composite(d);char path[512];snprintf(path,sizeof(path),"%s/gif-frame-%u.rgba",dir,frame);FILE *f=fopen(path,"wb");assert(f&&fwrite(pd_image(d),4,PD_PIXELS,f)==PD_PIXELS&&!fclose(f));
    }
    assert(pd_frame_switch(d,1));unsigned history=d->count,cursor=d->cursor;uint64_t revision=d->revision,generation=pd_frames_generation(d);char path[512],error[64];snprintf(path,sizeof(path),"%s/animation.gif",dir);
    assert(pd_gif_write(d,path,2u*1024u*1024u,error));assert(pd_frame_index(d)==1&&d->count==history&&d->cursor==cursor&&d->revision==revision&&pd_frames_generation(d)==generation);
    assert(!pd_gif_write(d,path,2u*1024u*1024u,error));snprintf(path,sizeof(path),"%s/limited.gif",dir);assert(!pd_gif_write(d,path,64,error)&&!pd_exists(path));char tmp[520];snprintf(tmp,sizeof(tmp),"%s.tmp",path);assert(!pd_exists(tmp)&&pd_frame_index(d)==1);
    assert(pd_create(d,128,128,1));for(unsigned i=0;i<PD_PIXELS;i++)pd_pixels(&d->layer[0])[i]=0x7f000000u|i;
    pd_pixels(&d->layer[0])[1]=0x80ff0000;for(unsigned i=2;i<PD_PIXELS;i++)pd_pixels(&d->layer[0])[i]=0xff000000u|i*1777;
    snprintf(path,sizeof(path),"%s/quantized.gif",dir);assert(pd_gif_write(d,path,2u*1024u*1024u,error));
    char prefix[512];int n=pd_share_prefix(prefix,sizeof(prefix),true);assert(n>0&&n<(int)sizeof(prefix)&&strstr(prefix,"PixelDoodle.gif")&&strstr(prefix,"image/gif")&&strstr(prefix,"files[0]")&&strstr(prefix,"allowed_mentions"));pd_share_prefix(prefix,sizeof(prefix),false);assert(strstr(prefix,"PixelDoodle.png")&&strstr(prefix,"image/png"));
    pd_dispose(d);free(d);puts("GIF: frames, timing, bounded output, snapshot restoration, palette fallback, transparency and shared PNG/GIF multipart metadata: PASS");
}
static void playback_checks(const char *dir){
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);
    for(unsigned mode=0;mode<2;mode++){
        app_init(a,mode?320:256,mode?240:192,"","TEST");assert(!pd_playback_start(&a->doc,&a->playback,0));
        dot(&a->doc,0,1,1,0xff112233);assert(pd_frame_add(&a->doc,false));dot(&a->doc,0,2,2,0xff445566);assert(pd_frame_set_duration(&a->doc,250));
        assert(pd_frame_switch(&a->doc,0)&&pd_frame_set_duration(&a->doc,100));
        a->page=30;app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-gif-send":"nds-gif-send",b,a->width,a->height);
        a->page=32;app_render(a,b,t,mode?400:256,a->height);
        unsigned history=a->doc.count,cursor=a->doc.cursor;uint64_t revision=a->doc.revision,generation=pd_frames_generation(&a->doc);
        a->now_ms=0xfffffff0u;app_input(a,true,110,38,0,0);assert(a->playback.active&&pd_frame_index(&a->doc)==0);
        app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-playback":"nds-playback",b,a->width,a->height);
        app_input(a,false,0,0,0,0);a->now_ms+=99;app_input(a,false,0,0,0,0);assert(pd_frame_index(&a->doc)==0);
        a->now_ms++;app_input(a,false,0,0,0,0);assert(pd_frame_index(&a->doc)==1&&pd_pixels(&a->doc.layer[0])[258]==0xff445566);
        app_render(a,b,t,mode?400:256,a->height);a->now_ms+=250;app_input(a,false,0,0,0,0);assert(pd_frame_index(&a->doc)==0);
        a->now_ms+=350*1000+110;app_input(a,false,0,0,0,0);assert(pd_frame_index(&a->doc)==1);
        app_input(a,true,120,100,PD_SAVE,PD_SAVE);assert(!a->playback.active&&a->page==32&&pd_frame_index(&a->doc)==0&&!a->doc.editing);
        assert(a->doc.count==history&&a->doc.cursor==cursor&&a->doc.revision==revision&&pd_frames_generation(&a->doc)==generation);
        assert(pd_pixels(&a->doc.layer[0])[129]==0xff112233&&!pd_pixels(&a->doc.layer[0])[258]);
        assert(pd_playback_start(&a->doc,&a->playback,0));pd_frames_test_fail_after(0);assert(pd_playback_tick(&a->doc,&a->playback,100));pd_playback_stop(&a->doc,&a->playback);pd_frames_test_fail_after(-1);
        app_dispose(a);
    }
    free(a);free(b);free(t);puts("Playback: unequal durations, wraparound, long stalls, stop isolation, exact return, unchanged history and allocation-free ticking: PASS");
}
static void animation_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d)),*r=calloc(1,sizeof(*r));assert(d&&r);pd_init(d);pd_init(r);char path[512],error[64];
    assert(pd_create(d,16,16,1));dot(d,0,1,1,0xff112233);assert(pd_frame_count(d)==1&&!pd_frame_memory(d));
    assert(pd_frame_add(d,true)&&pd_frame_count(d)==2&&pd_frame_index(d)==1);assert(pd_pixels(&d->layer[0])[129]==0xff112233);
    size_t packed=pd_frame_memory(d);assert(pd_frame_snapshot(d)&&pd_frame_memory(d)==packed);
    dot(d,0,2,2,0x7fabcdef);assert(pd_frame_set_duration(d,230));assert(!pd_frame_set_duration(d,0));
    assert(pd_frame_switch(d,0)&&pd_pixels(&d->layer[0])[258]==0);assert(pd_undo(d)&&pd_frame_index(d)==1&&pd_pixels(&d->layer[0])[258]==0);assert(pd_redo(d)&&pd_pixels(&d->layer[0])[258]==0x7fabcdef);
    assert(pd_rename_layer(d,"INK"));assert(pd_frame_switch(d,0)&&!strcmp(d->names[0],"INK"));assert(pd_undo(d)&&!strcmp(d->names[0],"Layer 1"));assert(pd_redo(d));
    assert(pd_frame_switch(d,1)&&pd_frame_duration(d)==230);assert(pd_frame_add(d,false)&&!pd_pixels(&d->layer[0])[129]);assert(pd_frame_count(d)==3);
    assert(pd_frame_switch(d,0)&&pd_duplicate_layer(d));unsigned copied=d->active;assert(pd_frame_switch(d,1)&&pd_pixels(&d->layer[copied])[258]==0x7fabcdef);
    assert(pd_undo(d)&&!(d->enabled&(1u<<copied)));assert(pd_redo(d)&&(d->enabled&(1u<<copied)));assert(!pd_merge_down(d));d->active=0;assert(pd_frame_switch(d,2));
    dot(d,0,5,5,0xff445566);assert(pd_frame_switch(d,0));uint64_t rev=d->revision;unsigned cursor=d->cursor;size_t used=pd_frame_memory(d);
    pd_frames_test_fail_after(0);assert(!pd_frame_switch(d,1)&&pd_frame_index(d)==0&&pd_pixels(&d->layer[0])[129]==0xff112233&&d->revision==rev&&d->cursor==cursor&&pd_frame_memory(d)==used);pd_frames_test_fail_after(-1);
    size_t budget=pd_frame_budget(d);assert(pd_frames_test_budget(d,used));assert(!pd_frame_add(d,true)&&pd_frame_count(d)==3);assert(pd_frames_test_budget(d,budget));
    snprintf(path,sizeof(path),"%s/animation.pdd",dir);assert(pd_write_file(d,path,0,false,error)&&!pd_dirty(d));assert(pd_open_file(r,path,error)&&pd_frame_count(r)==3&&pd_frame_index(r)==0&&!pd_dirty(r));
    assert(!strcmp(r->names[0],"INK")&&pd_frame_switch(r,1)&&pd_frame_duration(r)==230&&pd_pixels(&r->layer[0])[258]==0x7fabcdef);
    assert(pd_frame_switch(r,2)&&pd_pixels(&r->layer[0])[645]==0xff445566);assert(pd_frame_delete(r)&&pd_frame_count(r)==2&&pd_dirty(r)&&!r->count);
    assert(pd_frame_switch(d,1));assert(pd_frame_set_duration(d,100));snprintf(path,sizeof(path),"%s/animation-recovery.pdd",dir);rev=d->saved_revision;assert(pd_write_recovery(d,path,error)&&pd_dirty(d)&&d->saved_revision==rev);assert(pd_open_file(r,path,error)&&pd_frame_duration(r)==100);
    snprintf(path,sizeof(path),"%s/animation.aseprite",dir);assert(!pd_write_file(d,path,1,false,error)&&!pd_exists(path));
    snprintf(path,sizeof(path),"%s/animation-low-memory.pdd",dir);used=pd_frame_memory(d);assert(pd_frames_test_budget(d,used));pd_frames_test_fail_after(0);assert(pd_write_file(d,path,0,false,error));pd_frames_test_fail_after(-1);assert(pd_frames_test_budget(d,budget));assert(pd_open_file(r,path,error)&&pd_frame_count(r)==3&&pd_frame_index(r)==1&&pd_pixels(&r->layer[0])[258]==0x7fabcdef);
    snprintf(path,sizeof(path),"%s/animation-recovery.pdd",dir);FILE *f=fopen(path,"r+b");assert(f&&fseek(f,80,SEEK_SET)==0);int byte=fgetc(f);assert(byte!=EOF&&fseek(f,80,SEEK_SET)==0&&fputc(byte^1,f)!=EOF&&!fclose(f));
    unsigned frames=pd_frame_count(d),frame=pd_frame_index(d);rev=d->revision;assert(!pd_open_file(d,path,error)&&pd_frame_count(d)==frames&&pd_frame_index(d)==frame&&d->revision==rev&&pd_pixels(&d->layer[0])[258]==0x7fabcdef);
    /* Every allocation failure while packing a dense, newly edited frame rolls back. */
    for(int fail=0;fail<10;fail++){assert(pd_frame_switch(d,0));assert(pd_begin(d));for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)pd_pixels(&d->layer[0])[y*128+x]=0x80000000u+(unsigned)fail*256+x+y*16;assert(pd_commit(d));
        used=pd_frame_memory(d);pd_frames_test_fail_after(fail);bool ok=pd_frame_switch(d,1);pd_frames_test_fail_after(-1);if(!ok){assert(pd_frame_index(d)==0&&pd_frame_memory(d)==used&&pd_pixels(&d->layer[0])[129]==0x80000011u+(unsigned)fail*256);}else assert(pd_frame_index(d)==1);}
    assert(pd_create(d,128,128,1)&&pd_frame_add(d,false));d->enabled=255;
    for(unsigned l=0;l<8;l++)for(unsigned i=0;i<PD_PIXELS;i++)pd_pixels(&d->layer[l])[i]=0x80000000u|(l<<24)|i;
    used=pd_frame_memory(d);assert(pd_frames_test_budget(d,used));pd_frames_test_fail_after(0);snprintf(path,sizeof(path),"%s/animation-dense-no-headroom.pdd",dir);assert(pd_write_file(d,path,0,false,error));pd_frames_test_fail_after(-1);
    assert(pd_open_file(r,path,error)&&pd_frame_count(r)==2&&pd_frame_index(r)==1&&pd_pixels(&r->layer[7])[PD_PIXELS-1]==(0x87000000u|16383));assert(pd_frame_memory(r)<8192);
    assert(pd_create(d,8,8,1));for(unsigned i=1;i<PD_FRAMES;i++)assert(pd_frame_add(d,true));assert(pd_frame_count(d)==256&&!pd_frame_add(d,false));
    pd_dispose(d);pd_dispose(r);free(d);free(r);puts("Animation: shared tiles, exact RGBA, empty/duplicate frames, global layers, cross-frame undo, limits, allocation rollback, streaming native/recovery and corrupt-file preservation: PASS");
}
static void rapid_stroke_checks(void){
    PdApp *a=calloc(1,sizeof(*a));uint32_t *bottom=malloc(320*240*4),*top=malloc(400*240*4);assert(a&&bottom&&top);
    for(unsigned mode=0;mode<2;mode++){app_init(a,mode?320:256,mode?240:192,"","TEST");a->zoom=2;a->color=0xff123456;app_render(a,bottom,top,mode?400:256,a->height);
        for(unsigned i=0;i<24;i++){a->now_ms+=16;int x=(mode?32:0)+16+(int)i*4;app_input(a,true,x,60,0,0);assert(a->stroke&&pd_pixels(&a->doc.layer[0])[18*128+8+i*2]==a->color);app_render(a,bottom,top,mode?400:256,a->height);
            unsigned calls=pd_test_composites();a->now_ms+=16;app_input(a,false,0,0,0,0);assert(!a->stroke&&!a->redraw&&a->overview_pending&&pd_test_composites()==calls);}
        a->now_ms+=81;app_input(a,false,0,0,0,0);assert(a->redraw&&!a->overview_pending);app_render(a,bottom,top,mode?400:256,a->height);assert(a->present_top);
        uint32_t header[64*7];unsigned hw=(unsigned)strlen(PD_BUILD_LABEL)*6;assert(hw<=64);int tw=mode?400:256;bool ink=false;
        for(unsigned y=0;y<7;y++)for(unsigned x=0;x<hw;x++){header[y*64+x]=top[(6+y)*tw+8+x];if(header[y*64+x]!=0x182126)ink=true;}assert(ink);
        snprintf(a->project,sizeof(a->project),"/A very long project filename that must not cover the version.pdd");a->redraw=true;app_render(a,bottom,top,tw,a->height);
        for(unsigned y=0;y<7;y++)for(unsigned x=0;x<hw;x++)assert(top[(6+y)*tw+8+x]==header[y*64+x]);
        assert(a->doc.count==24);for(unsigned i=0;i<24;i++)assert(pd_undo(&a->doc));for(unsigned i=0;i<24;i++)assert(!pd_pixels(&a->doc.layer[0])[18*128+8+i*2]);
        assert(pd_create(&a->doc,16,16,1));int left=(a->width-32)/2,upper=24+(a->height-24-32)/2;
        app_input(a,true,left-2,upper+2,0,0);assert(!a->stroke);app_input(a,true,left+2,upper+2,0,0);assert(a->stroke&&pd_pixels(&a->doc.layer[0])[129]==a->color);app_input(a,false,0,0,0,0);
    }
    app_dispose(a);free(a);free(bottom);free(top);puts("Rapid single-sample strokes: both screen sizes, first dot, distinct undo entries and deferred pen-release redraw: PASS");
}
static void timeline_checks(const char *dir){
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);
    for(unsigned mode=0;mode<2;mode++){app_init(a,mode?320:256,mode?240:192,"","TEST");assert(app_set_profile(a,mode?3:1));a->doc.enabled=255;dot(&a->doc,0,2,2,0xffabcdef);a->page=3;tap(a,a->width-30,110);assert(a->page==32);
        tap(a,24,a->height-40);assert(pd_frame_count(&a->doc)==2&&pd_frame_index(&a->doc)==1&&!pd_frame_has_cel(&a->doc,1,0));
        tap(a,a->width/2,a->height-40);assert(pd_frame_count(&a->doc)==3&&pd_frame_index(&a->doc)==2);
        /* The first visible layer is the topmost stable slot, not slot zero. */
        tap(a,100,76);assert(pd_frame_index(&a->doc)==0&&a->doc.active==7);tap(a,a->width-20,48);assert(a->page==33);tap(a,20,112);assert(pd_frame_duration(&a->doc)==300);tap(a,20,a->height-12);assert(a->page==32);
        app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-timeline":"nds-timeline",b,a->width,a->height);
        if(!mode){tap(a,90,138);assert(a->timeline_layer_first==1);tap(a,90,138);assert(a->timeline_layer_first==2);tap(a,100,138);assert(a->doc.active==0);}
        tap(a,a->width-20,a->height-40);assert(a->page==34);tap(a,a->width-20,112);assert(a->page==32&&pd_frame_count(&a->doc)==3);
        tap(a,20,a->height-12);assert(!a->page);app_input(a,false,0,0,PD_SHOULDER_R,PD_PAN|PD_ERASE|PD_SHOULDER_R);assert(pd_frame_index(&a->doc)==1);app_input(a,false,0,0,0,0);app_input(a,false,0,0,PD_FRAME_PREV,0);assert(pd_frame_index(&a->doc)==0);
        app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-build-label":"nds-build-label",t,mode?400:256,a->height);
        a->page=7;a->input_contacts=a->ink_starts=24;app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-input-diagnostics":"nds-input-diagnostics",b,a->width,a->height);
    }
    app_dispose(a);free(a);free(b);free(t);puts("Timeline: native layouts, ordered cel hitboxes, layer scrolling, frame creation/duplication, duration, delete cancel and frame shortcuts: PASS");
}
