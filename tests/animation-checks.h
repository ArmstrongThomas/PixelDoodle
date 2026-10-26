#include "animation.h"
#include "version.h"
#include "gif.h"
#include "share.h"

static void tap_point(PdApp *a,int part,int index){int x=0,y=0;assert(app_test_point(a,part,index,&x,&y));tap(a,x,y);}
static uint32_t px(PdDocument *d,unsigned x,unsigned y){return pd_pixels(&d->layer[0])[y*d->stride+x];}
static void tl16(unsigned char *p,unsigned v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static void tl32(unsigned char *p,unsigned v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}

static void frame_move_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d));assert(d);pd_init(d);assert(pd_create(d,16,16,1));dot(d,0,1,1,0xff111111);assert(pd_frame_add(d,false));dot(d,0,2,2,0xff222222);assert(pd_frame_add(d,false));dot(d,0,3,3,0xff333333);
    unsigned history=d->count,cursor=d->cursor;size_t pool=pd_frame_memory(d);pd_frames_test_fail_after(0);assert(pd_frame_move(d,0));pd_frames_test_fail_after(-1);assert(pd_frame_memory(d)==pool&&pd_frame_index(d)==0&&d->count==history&&d->cursor==cursor&&pd_pixels(&d->layer[0])[387]==0xff333333);
    assert(pd_undo(d)&&pd_frame_index(d)==0&&!pd_pixels(&d->layer[0])[387]);assert(pd_redo(d)&&pd_pixels(&d->layer[0])[387]==0xff333333);
    assert(pd_frame_move(d,2));assert(pd_undo(d)&&pd_frame_index(d)==2);assert(pd_undo(d)&&pd_frame_index(d)==1&&!pd_pixels(&d->layer[0])[258]);assert(pd_redo(d)&&pd_pixels(&d->layer[0])[258]==0xff222222);
    char path[512],error[64];snprintf(path,sizeof(path),"%s/moved.pdd",dir);assert(pd_write_file(d,path,0,false,error)&&!pd_dirty(d));assert(pd_frame_move(d,0)&&pd_dirty(d));assert(!pd_frame_move(d,0)&&!pd_frame_move(d,99));
    assert(pd_begin(d));assert(!pd_frame_move(d,1));pd_cancel(d);pd_dispose(d);free(d);
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);
    for(unsigned mode=0;mode<2;mode++){
        app_init(a,mode?320:256,mode?240:192,"","TEST");assert(pd_frame_add(&a->doc,false));a->page=1;a->layers_tab=1;tap_point(a,PD_TL_NUMBER,1);assert(a->frame_sheet);
        tap_point(a,PD_SHEET_MOVE,0);assert(pd_frame_index(&a->doc)==0&&a->frame_sheet);tap_point(a,PD_SHEET_MOVE,0);assert(pd_frame_index(&a->doc)==0);tap_point(a,PD_SHEET_MOVE,1);assert(pd_frame_index(&a->doc)==1);
        app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-frame-actions":"nds-frame-actions",b,a->width,a->height);
        tap_point(a,PD_SHEET_CLOSE,0);assert(a->page==1&&!a->frame_sheet);app_dispose(a);
    }
    free(a);free(b);free(t);puts("Frame move: allocation-free reorder, original working pixels, cross-frame undo remapping, dirty state, bounds and native actions: PASS");
}
static void move_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d));assert(d);pd_init(d);assert(pd_create(d,16,16,2));d->enabled=1;
    for(unsigned i=0;i<6;i++){if(i)assert(pd_frame_add(d,false));assert(pd_frame_set_duration(d,100+i));dot(d,0,(int)i,0,0xff000000u|(i+1));}
    uint16_t map[PD_FRAMES];uint8_t marked[PD_FRAMES]={0};unsigned order[6]={0,3,4,5,1,2};
    assert(pd_frame_switch(d,2));dot(d,0,10,10,0xff00ff00);
    marked[1]=marked[2]=1;assert(pd_frames_move(d,marked,4,map));
    for(unsigned i=0;i<6;i++)assert(map[i]==(uint16_t)(i==0?0:i==1?4:i==2?5:i==3?1:i==4?2:3));
    assert(pd_frame_index(d)==5&&px(d,10,10)==0xff00ff00);
    for(unsigned f=0;f<6;f++){assert(pd_frame_switch(d,f));unsigned old=order[f];assert(pd_pixels(&d->layer[0])[old]==(0xff000000u|(old+1)));assert(pd_frame_duration(d)==100+old);}
    assert(pd_undo(d)&&pd_frame_index(d)==5&&!px(d,10,10));assert(pd_redo(d)&&px(d,10,10)==0xff00ff00);
    /* Non-contiguous marks, allocation-free move, identity/invalid refusals and pool stability. */
    memset(marked,0,sizeof(marked));marked[0]=marked[3]=marked[5]=1;assert(pd_frames_move(d,marked,1,map));
    unsigned second[6]={3,0,5,2,4,1};for(unsigned i=0;i<6;i++)assert(map[i]==(uint16_t)(i==0?1:i==1?0:i==2?4:i==3?2:i==4?5:3));
    for(unsigned f=0;f<6;f++){assert(pd_frame_switch(d,f));assert(pd_pixels(&d->layer[0])[second[f]]==(0xff000000u|(second[f]+1)));}
    size_t pool=pd_frame_memory(d);memset(marked,0,sizeof(marked));marked[1]=marked[0]=marked[3]=1;
    pd_frames_test_fail_after(0);assert(pd_frames_move(d,marked,2,map));pd_frames_test_fail_after(-1);assert(pd_frame_memory(d)==pool);
    memset(marked,0,sizeof(marked));marked[0]=1;assert(!pd_frames_move(d,marked,0,map));
    memset(marked,0,sizeof(marked));assert(!pd_frames_move(d,marked,0,map));
    memset(marked,0,sizeof(marked));for(unsigned i=0;i<6;i++)marked[i]=1;assert(!pd_frames_move(d,marked,0,map));
    memset(marked,0,sizeof(marked));marked[0]=1;assert(!pd_frames_move(d,marked,6,map));
    /* Single cel moves: within one frame, across frames, links cleared and undo history cleared. */
    pd_dispose(d);pd_init(d);assert(pd_create(d,16,16,2));d->enabled=3;d->order[0]=0;d->order[1]=1;
    dot(d,0,1,1,0xffff0000);dot(d,1,2,2,0xff00ff00);assert(pd_frame_add(d,false));dot(d,0,3,3,0xff0000ff);dot(d,1,4,4,0xff00ffff);
    assert(pd_frame_add(d,false));dot(d,0,5,5,0xff112233);dot(d,1,6,6,0xff445566);
    assert(pd_cel_move(d,2,0,2,0)==true);
    assert(pd_cel_move(d,2,0,2,1)&&pd_frame_index(d)==2&&d->active==1&&d->count==0);
    assert(pd_pixels(&d->layer[1])[5*128+5]==0xff112233&&!pd_pixels(&d->layer[0])[5*128+5]&&!pd_pixels(&d->layer[1])[6*128+6]);
    assert(pd_cel_move(d,0,0,1,1)&&pd_frame_index(d)==1&&d->active==1);
    assert(pd_pixels(&d->layer[1])[1*128+1]==0xffff0000&&!pd_pixels(&d->layer[1])[4*128+4]&&!pd_pixels(&d->layer[1])[2*128+2]);
    assert(pd_frame_switch(d,0)&&!pd_pixels(&d->layer[0])[1*128+1]);
    assert(pd_cel_move(d,0,0,1,0)&&!pd_pixels(&d->layer[0])[3*128+3]);
    assert(pd_frame_switch(d,2));bool cleared=false;assert(pd_cel_link_previous(d,1,&cleared));
    unsigned group=pd_cel_link(d,2,1);assert(group&&pd_cel_link(d,1,1)==group);
    assert(pd_cel_move(d,2,1,0,0));
    assert(!pd_cel_link(d,2,1)&&!pd_cel_link(d,1,1)&&pd_frame_index(d)==0&&d->active==0);
    pd_dispose(d);free(d);
    /* A project with no animation can still move a cel between layers of its single frame. */
    PdDocument *s=calloc(1,sizeof(*s));assert(s);pd_init(s);assert(pd_create(s,8,8,2));s->enabled=3;dot(s,0,2,2,0xffabcdef);dot(s,1,3,3,0xff123456);
    assert(pd_cel_move(s,0,0,0,1)&&s->active==1&&pd_pixels(&s->layer[1])[2*128+2]==0xffabcdef&&!pd_pixels(&s->layer[0])[2*128+2]);
    pd_dispose(s);free(s);
    puts("Precise moves: multi-frame blocks, non-contiguous marks, current/undo remapping, allocation-free reorder, cel move across layers/frames, link clearing and single-frame cel move: PASS");
}
static void move_ui_checks(const char *dir){
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);
    for(unsigned mode=0;mode<2;mode++){
        int tw=mode?400:256;app_init(a,mode?320:256,mode?240:192,"","TEST");assert(app_set_profile(a,mode?3:1));
        a->doc.enabled=3;a->doc.order[0]=0;a->doc.order[1]=1;a->page=1;a->layers_tab=1;
        for(unsigned f=0;f<4;f++){if(f)assert(pd_frame_add(&a->doc,false));dot(&a->doc,0,(int)f,0,0xff000000u|(f+1));dot(&a->doc,1,(int)f,1,0xff000000u|0x10u|(f+1));}
        assert(pd_frame_switch(&a->doc,0));
        int x=0,y=0;assert(app_test_point(a,PD_TL_NUMBER,1,&x,&y));app_input(a,true,x,y,0,PD_SHOULDER_L);app_input(a,false,0,0,0,0);
        assert(app_test_point(a,PD_TL_NUMBER,2,&x,&y));app_input(a,true,x,y,0,PD_SHOULDER_L);app_input(a,false,0,0,0,0);
        assert(a->frame_mark_count==2&&a->frame_mark[1]&&a->frame_mark[2]);
        a->redraw=true;app_render(a,b,t,tw,a->height);ppm(dir,mode?"3ds-frame-marks":"nds-frame-marks",b,a->width,a->height);
        /* Hold marked frame 2 and drop at column 0: frames 2,3 move to the front. */
        assert(app_test_point(a,PD_TL_NUMBER,2,&x,&y));a->now_ms=1000;app_input(a,true,x,y,0,0);a->now_ms=1350;app_input(a,true,x,y,0,0);assert(a->timeline_reorder);
        int tx=0,ty=0;assert(app_test_point(a,PD_TL_NUMBER,0,&tx,&ty));app_input(a,true,tx,ty,0,0);assert(a->timeline_target==0);app_input(a,false,0,0,0,0);
        assert(a->frame_mark_count==2&&a->frame_mark[0]&&a->frame_mark[1]&&pd_frame_index(&a->doc)==2&&pd_pixels(&a->doc.layer[0])[0]==0xff000001u);
        assert(pd_frame_switch(&a->doc,0)&&pd_pixels(&a->doc.layer[0])[1]==0xff000002u);assert(pd_frame_switch(&a->doc,1)&&pd_pixels(&a->doc.layer[0])[2]==0xff000003u);
        /* Hold the cel in frame 2/layer 0 and drop it on frame 1/layer 1. */
        assert(pd_frame_switch(&a->doc,2));a->doc.active=0;
        assert(app_test_point(a,PD_TL_CEL,2|(1<<8),&x,&y));a->now_ms=2000;app_input(a,true,x,y,0,0);a->now_ms=2450;app_input(a,true,x,y,0,0);assert(a->cel_drag&&a->cel_src_frame==2&&a->cel_src_layer==0);
        int dx=0,dy=0;assert(app_test_point(a,PD_TL_CEL,0|(0<<8),&dx,&dy));app_input(a,true,dx,dy,0,0);assert(a->cel_drag_valid&&a->cel_dst_frame==0&&a->cel_dst_layer==1);
        a->redraw=true;app_render(a,b,t,tw,a->height);ppm(dir,mode?"3ds-cel-drag":"nds-cel-drag",b,a->width,a->height);
        app_input(a,false,0,0,0,0);
        assert(pd_frame_index(&a->doc)==0&&a->doc.active==1&&a->frame_mark_count==0&&a->doc.count==0);
        assert(pd_pixels(&a->doc.layer[1])[0]==0xff000001u&&!pd_pixels(&a->doc.layer[1])[129]&&pd_pixels(&a->doc.layer[0])[1]==0xff000002u);
        assert(pd_frame_switch(&a->doc,2)&&!pd_pixels(&a->doc.layer[0])[0]);
        /* Adding a frame drops old marks; the Menu no longer has a Frames button. */
        int mx=0,my=0;assert(!app_test_menu_point(a,10,&mx,&my)&&app_test_menu_point(a,4,&mx,&my));
        assert(pd_frame_switch(&a->doc,0));assert(app_test_point(a,PD_TL_NUMBER,0,&x,&y));app_input(a,true,x,y,0,PD_SHOULDER_L);app_input(a,false,0,0,0,0);assert(a->frame_mark_count==1);
        tap_point(a,PD_TL_FOOTER,0);assert(pd_frame_count(&a->doc)==5&&a->frame_mark_count==0);
        app_dispose(a);
    }
    free(a);free(b);free(t);puts("Timeline move UI: L+tap frame marks, marked block drop, hold-drag cel across layer/frame, mark clearing and Menu without Frames: PASS");
}
static void ase_animation_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d));assert(d);pd_init(d);assert(pd_create(d,16,16,1));d->enabled=3;d->order[0]=1;d->order[1]=0;strcpy(d->names[0],"Ink");strcpy(d->names[1],"Shade");d->layer[0].flags=PD_LAYER_LOCK;d->layer[1].opacity=128;d->layer[1].blend=1;
    d->palette_count=3;d->palette[0]=0;d->palette[1]=0xff123456;d->palette[2]=0x80abcdef;
    char path[512],error[64];unsigned durations[]={40,230,600};
    for(unsigned frame=0;frame<3;frame++){
        if(frame)assert(pd_frame_add(d,false));assert(pd_frame_set_duration(d,durations[frame]));
        for(unsigned l=0;l<2;l++){
            for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)pd_pixels(&d->layer[l])[y*128+x]=(frame==2?0:((x+y)%5?0xff000000u:0x70000000u))|((frame*40+l*80+x)<<16)|((y*10)<<8)|(x+y);
            snprintf(path,sizeof(path),"%s/ase-frame-%u-%u.rgba",dir,frame,l);FILE *f=fopen(path,"wb");assert(f);for(unsigned y=0;y<16;y++)assert(fwrite(pd_pixels(&d->layer[l])+y*128,4,16,f)==16);assert(!fclose(f));
        }
    }
    assert(pd_frame_switch(d,1));unsigned history=d->count,cursor=d->cursor;uint64_t revision=d->revision,generation=pd_frames_generation(d);bool dirty=pd_dirty(d);
    snprintf(path,sizeof(path),"%s/animated-layers.aseprite",dir);assert(pd_write_file(d,path,1,false,error));assert(pd_frame_index(d)==1&&d->count==history&&d->cursor==cursor&&d->revision==revision&&pd_frames_generation(d)==generation&&pd_dirty(d)==dirty);
    assert(!pd_write_file(d,path,1,false,error));assert(pd_write_file(d,path,1,true,error));
    snprintf(path,sizeof(path),"%s/ase-no-memory.aseprite",dir);pd_frames_test_fail_after(0);assert(!pd_write_file(d,path,1,false,error)&&!pd_exists(path));pd_frames_test_fail_after(-1);assert(pd_frame_index(d)==1&&d->revision==revision);
    pd_dispose(d);free(d);puts("Animated ASE export: streamed frames/layers/durations/palette, readback, overwrite backup, state preservation and allocation refusal: PASS");
}
static void onion_checks(const char *dir){
    PdApp *a=calloc(1,sizeof(*a));PdFb *b=malloc(320*240*sizeof(PdFb)),*top=malloc(400*240*sizeof(PdFb));unsigned char *alpha=malloc(PD_PIXELS);assert(a&&b&&top&&alpha);
    for(unsigned mode=0;mode<2;mode++){
        app_init(a,mode?320:256,mode?240:192,"","TEST");a->zoom=2;
        dot(&a->doc,0,5,5,0xff123456);assert(pd_frame_add(&a->doc,false));dot(&a->doc,0,9,5,0xff778899);assert(pd_frame_add(&a->doc,false));dot(&a->doc,0,13,5,0xffaabbcc);assert(pd_frame_switch(&a->doc,1));
        unsigned count=a->doc.count,cursor=a->doc.cursor;uint64_t revision=a->doc.revision,generation=pd_frames_generation(&a->doc);size_t pool=pd_frame_memory(&a->doc);
        a->onion_enabled=true;app_render(a,b,top,mode?400:256,a->height);assert(a->onion&&a->onion[5*128+5]==255&&a->onion[a->onion_plane+5*128+13]==255);
        ppm(dir,mode?"3ds-onion":"nds-onion",b,a->width,a->height);
        assert(a->doc.count==count&&a->doc.cursor==cursor&&a->doc.revision==revision&&pd_frames_generation(&a->doc)==generation&&pd_frame_memory(&a->doc)==pool&&pd_frame_index(&a->doc)==1);
        uint64_t key=a->onion_key;unsigned char *cache=a->onion;int left=mode?32:0;
        a->now_ms=16;app_input(a,true,left+41,65,0,0);app_render(a,b,top,mode?400:256,a->height);a->now_ms=32;app_input(a,false,0,0,0,0);a->now_ms=200;app_input(a,false,0,0,0,0);app_render(a,b,top,mode?400:256,a->height);assert(a->onion==cache&&a->onion_key==key);
        uint32_t *reference=malloc((size_t)a->width*a->height*4);assert(reference);memcpy(reference,b,(size_t)a->width*a->height*4);a->partial=false;a->redraw=true;app_render(a,b,top,mode?400:256,a->height);assert(!memcmp(reference,b,(size_t)a->width*a->height*4));free(reference);
        assert(!pd_pixels(&a->doc.layer[0])[5*128+5]&&!pd_pixels(&a->doc.layer[0])[5*128+13]);
        a->doc.layer[0].opacity=100;a->redraw=true;app_render(a,b,top,mode?400:256,a->height);assert(a->onion[5*128+5]==100&&a->onion_key!=key);
        pd_frames_test_fail_after(0);assert(pd_frame_alpha(&a->doc,0,alpha)&&alpha[5*128+5]==100);pd_frames_test_fail_after(-1);
        char saved[512],error[64];snprintf(saved,sizeof(saved),"%s/onion-load-%u.pdd",dir,mode);assert(pd_write_file(&a->doc,saved,0,false,error));
        assert(pd_open_file(&a->doc,saved,error));a->redraw=true;app_render(a,b,top,mode?400:256,a->height);key=a->onion_key;
        assert(pd_open_file(&a->doc,saved,error));a->redraw=true;app_render(a,b,top,mode?400:256,a->height);assert(a->onion_key!=key);
        a->doc.layer[0].visible=0;a->redraw=true;app_render(a,b,top,mode?400:256,a->height);assert(!a->onion[5*128+5]);
        app_dispose(a);
    }
    /* Several neighbours merge far to near with fading; untinted ghosts keep the neighbours' colours. */
    app_init(a,256,192,"","TEST");dot(&a->doc,0,3,3,0xff102030);assert(pd_frame_add(&a->doc,false));dot(&a->doc,0,4,3,0xff405060);assert(pd_frame_add(&a->doc,false));assert(pd_frame_add(&a->doc,false));dot(&a->doc,0,6,3,0xff708090);assert(pd_frame_switch(&a->doc,2));
    a->onion_enabled=true;a->onion_behind=2;app_render(a,b,top,256,192);size_t pl=a->onion_plane;assert(a->onion_bytes==pl*3&&a->onion[3*128+3]==179&&a->onion[3*128+4]==255&&a->onion[pl+3*128+6]==255);
    a->onion_tint=false;a->redraw=true;app_render(a,b,top,256,192);const uint32_t *ghost=(const uint32_t *)a->onion;assert(a->onion_bytes==pl*12&&ghost[3*128+4]==0xff405060&&ghost[3*128+3]==0xb3102030&&ghost[pl+3*128+6]==0xff708090);
    a->onion_behind=1;a->onion_opacity=90;a->redraw=true;app_render(a,b,top,256,192);ghost=(const uint32_t *)a->onion;assert(a->onion_bytes==pl*8&&!ghost[3*128+3]&&ghost[3*128+4]==0xff405060);ppm(dir,"nds-onion-untinted",b,256,192);
    uint32_t *full=malloc(PD_PIXELS*4),*thumb=malloc(PD_PIXELS*4);assert(full&&thumb);
    for(unsigned f=0;f<4;f++){assert(pd_frame_rgba(&a->doc,f,full)&&pd_frame_sample(&a->doc,f,thumb,128,128));for(unsigned y=0;y<128;y++)for(unsigned x=0;x<128;x++)assert(thumb[y*128+x]==full[y*a->doc.stride+x]);}
    assert(full[3*128+6]==0xff708090);assert(pd_frame_sample(&a->doc,1,thumb,16,16)&&thumb[0]==0);assert(pd_frame_rgba(&a->doc,2,full)&&!full[3*128+6]);
    free(full);free(thumb);app_dispose(a);
    PdDocument *d=calloc(1,sizeof(*d));assert(d);pd_init(d);assert(pd_create(d,16,16,3));d->enabled=7;
    for(unsigned l=0;l<3;l++){d->layer[l].opacity=(uint8_t)(80+l*60);d->layer[l].blend=(uint8_t)l;for(unsigned i=0;i<PD_PIXELS;i++)pd_pixels(&d->layer[l])[i]=((i*17+l*71)<<24)|0x123456;}
    pd_composite(d);assert(pd_frame_alpha(d,0,alpha));for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)assert(alpha[y*128+x]==pd_image(d)[y*128+x]>>24);
    assert(pd_frame_add(d,false));assert(pd_frame_alpha(d,0,alpha));assert(pd_frame_switch(d,0));pd_composite(d);for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)assert(alpha[y*128+x]==pd_image(d)[y*128+x]>>24);
    pd_dispose(d);free(d);free(alpha);free(a);free(b);free(top);puts("Onion skin: read-only adjacent alpha, cached drawing, metadata invalidation, exact ordered alpha and no packed-pool allocations: PASS");
}
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
        a->page=1;a->layers_tab=1;app_render(a,b,t,mode?400:256,a->height);
        unsigned history=a->doc.count,cursor=a->doc.cursor;uint64_t revision=a->doc.revision,generation=pd_frames_generation(&a->doc);
        int px=0,py=0;assert(app_test_point(a,PD_TL_PLAY,0,&px,&py));a->now_ms=0xfffffff0u;app_input(a,true,px,py,0,0);assert(a->playback.active&&pd_frame_index(&a->doc)==0);
        app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-playback":"nds-playback",b,a->width,a->height);
        app_input(a,false,0,0,0,0);a->now_ms+=99;app_input(a,false,0,0,0,0);assert(pd_frame_index(&a->doc)==0);
        a->now_ms++;app_input(a,false,0,0,0,0);assert(pd_frame_index(&a->doc)==1&&pd_pixels(&a->doc.layer[0])[258]==0xff445566);
        app_render(a,b,t,mode?400:256,a->height);a->now_ms+=250;app_input(a,false,0,0,0,0);assert(pd_frame_index(&a->doc)==0);
        a->now_ms+=350*1000+110;app_input(a,false,0,0,0,0);assert(pd_frame_index(&a->doc)==1);
        a->timeline_first=1;app_input(a,true,120,100,PD_SAVE,PD_SAVE);assert(!a->timeline_first);assert(!a->playback.active&&a->page==1&&a->layers_tab&&pd_frame_index(&a->doc)==0&&!a->doc.editing);
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
    snprintf(path,sizeof(path),"%s/animation.aseprite",dir);assert(pd_write_file(d,path,1,false,error)&&pd_exists(path));assert(pd_open_file(r,path,error)&&!pd_dirty(r)&&pd_frame_count(r)==pd_frame_count(d)&&r->enabled==d->enabled&&!strcmp(r->names[0],d->names[0]));
    for(unsigned f=0;f<pd_frame_count(d);f++){assert(pd_frame_switch(d,f)&&pd_frame_switch(r,f)&&pd_frame_duration(r)==pd_frame_duration(d));for(unsigned l=0;l<PD_LAYERS;l++)if(d->enabled&(1u<<l))assert(!memcmp(pd_pixels(&d->layer[l]),pd_pixels(&r->layer[l]),pd_bytes(d)));}
    assert(pd_frame_switch(d,1));
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
static void tag_link_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d)),*r=calloc(1,sizeof(*r));assert(d&&r);pd_init(d);pd_init(r);char path[512],error[64];
    assert(pd_create(d,16,16,1));dot(d,0,1,1,0xff0000aa);
    /* A linked frame shares every cel; edits on either side reach the other when packed. */
    assert(pd_frame_add_linked(d)&&pd_frame_index(d)==1&&px(d,1,1)==0xff0000aa);unsigned group=pd_cel_link(d,0,0);assert(group&&pd_cel_link(d,1,0)==group);
    dot(d,0,2,2,0xff0000bb);assert(pd_frame_switch(d,0)&&px(d,2,2)==0xff0000bb);
    assert(pd_undo(d)&&pd_frame_index(d)==1&&!px(d,2,2));assert(pd_frame_switch(d,0)&&!px(d,2,2));assert(pd_redo(d)&&pd_frame_switch(d,0)&&px(d,2,2)==0xff0000bb);
    /* A plain new frame is never linked, even after a linked one. Linking takes the previous image and resets undo only if pixels change. */
    assert(pd_frame_switch(d,1)&&pd_frame_add(d,false)&&pd_frame_index(d)==2&&!pd_cel_link(d,2,0));dot(d,0,3,3,0xff0000cc);
    bool cleared=false;assert(pd_cel_link_previous(d,0,&cleared)&&cleared&&!d->count&&pd_cel_link(d,2,0)==group&&!px(d,3,3)&&px(d,2,2)==0xff0000bb);
    assert(!pd_cel_link_previous(d,0,&cleared));
    dot(d,0,4,4,0xff0000dd);assert(pd_frame_switch(d,0)&&px(d,4,4)==0xff0000dd);
    assert(pd_frame_add_linked(d)&&pd_frame_delete(d)&&pd_frame_count(d)==3);assert(pd_frame_switch(d,1)&&pd_frame_add(d,true)&&!pd_cel_link(d,2,0)&&pd_cel_link_previous(d,0,&cleared)&&!cleared&&pd_frame_delete(d));
    assert(pd_frame_switch(d,0)&&pd_cel_unlink(d,0)&&!pd_cel_link(d,0,0)&&pd_cel_link(d,1,0)==group);dot(d,0,5,5,0xff0000ee);assert(pd_frame_switch(d,1)&&!px(d,5,5));
    assert(pd_frame_switch(d,2)&&pd_cel_unlink(d,0)&&!pd_cel_link(d,1,0)&&!pd_cel_link(d,2,0));assert(pd_frame_switch(d,2)&&pd_cel_link_previous(d,0,&cleared)&&pd_cel_link(d,1,0)==pd_cel_link(d,2,0));
    /* Tags: validation, insertion/deletion shifts, innermost lookup. */
    PdTag tag={"hop",1,2,2,0,0xc3a6ff};assert(pd_tag_add(d,&tag)==0&&pd_tag_at(d,1)==0&&pd_tag_at(d,0)==-1);
    PdTag bad=tag;bad.from=3;assert(pd_tag_add(d,&bad)<0);bad=tag;bad.to=3;assert(pd_tag_add(d,&bad)<0);bad=tag;bad.direction=4;assert(pd_tag_add(d,&bad)<0);
    PdTag inner={"land",2,2,0,3,0x5aa9ff};assert(pd_tag_add(d,&inner)==1&&pd_tag_at(d,2)==1&&pd_tag_at(d,1)==0);
    assert(pd_frame_switch(d,0)&&pd_frame_add(d,false)&&pd_tag(d,0)->from==2&&pd_tag(d,0)->to==3&&pd_tag(d,1)->from==3);
    assert(pd_frame_delete(d)&&pd_tag(d,0)->from==1&&pd_tag(d,0)->to==2);
    assert(pd_frame_switch(d,2)&&pd_frame_add(d,false)&&pd_tag(d,0)->to==2);assert(pd_frame_delete(d));
    /* Version 11 saves keep links and tags. */
    snprintf(path,sizeof(path),"%s/tags-links.pdd",dir);assert(pd_write_file(d,path,0,false,error));assert(pd_open_file(r,path,error)&&pd_frame_count(r)==3&&pd_tag_count(r)==2);
    assert(!strcmp(pd_tag(r,0)->name,"hop")&&pd_tag(r,0)->direction==2&&pd_tag(r,0)->color==0xc3a6ff&&pd_tag(r,1)->repeat==3);
    assert(pd_cel_link(r,1,0)&&pd_cel_link(r,1,0)==pd_cel_link(r,2,0)&&!pd_cel_link(r,0,0));
    assert(pd_frame_switch(r,1));dot(r,0,6,6,0xff0000ff);assert(pd_frame_switch(r,2)&&px(r,6,6)==0xff0000ff);assert(pd_frame_switch(r,0)&&!px(r,6,6));
    /* Playback loops the tag: ping-pong, reverse and a single-frame tag that falls back to every frame. */
    PdPlayback p={0};assert(pd_frame_switch(r,1)&&pd_playback_start(r,&p,0)&&p.from==1&&p.to==2);
    unsigned expect[]={2,1,2,1};for(unsigned i=0;i<4;i++){assert(pd_playback_tick(r,&p,100*(i+1))&&pd_frame_index(r)==expect[i]);}pd_playback_stop(r,&p);assert(pd_frame_index(r)==1);
    PdTag rev=*pd_tag(r,0);rev.direction=1;assert(pd_tag_set(r,0,&rev));memset(&p,0,sizeof(p));assert(pd_playback_start(r,&p,0));unsigned back[]={2,1,2};for(unsigned i=0;i<3;i++){assert(pd_playback_tick(r,&p,100*(i+1))&&pd_frame_index(r)==back[i]);}
    assert(pd_playback_tick(r,&p,100*1000+50)&&pd_frame_index(r)>=1&&pd_frame_index(r)<=2);pd_playback_stop(r,&p);
    assert(pd_frame_switch(r,2));memset(&p,0,sizeof(p));assert(pd_playback_start(r,&p,0)&&p.from==0&&p.to==2);pd_playback_stop(r,&p);
    assert(pd_tag_delete(r,1)&&pd_tag_count(r)==1&&!pd_tag_delete(r,5));
    /* Packing refuses atomically when linked copies cannot be allocated. */
    assert(pd_frame_switch(r,1));dot(r,0,7,7,0xff00aa00);size_t used=pd_frame_memory(r);pd_frames_test_fail_after(1);assert(!pd_frame_switch(r,0));pd_frames_test_fail_after(-1);
    assert(pd_frame_index(r)==1&&px(r,7,7)==0xff00aa00&&pd_frame_memory(r)==used);assert(pd_frame_switch(r,2)&&px(r,7,7)==0xff00aa00);
    pd_dispose(d);pd_dispose(r);free(d);free(r);puts("Tags and linked cels: shared edits, undo through links, link/unlink, tag validation/shifts, PDDA v11, tag playback and atomic packing: PASS");
}
static void ase_link_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d)),*r=calloc(1,sizeof(*r));assert(d&&r);pd_init(d);pd_init(r);
    assert(pd_create(d,16,16,2));d->enabled=3;d->order[0]=0;d->order[1]=1;strcpy(d->names[0],"Base");strcpy(d->names[1],"Top");d->layer[1].opacity=160;d->layer[1].blend=1;
    d->palette_count=2;d->palette[0]=0xff112233;d->palette[1]=0x80112233;
    for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++){pd_pixels(&d->layer[0])[y*128+x]=0xff000000u|(x<<16)|(y<<8)|0x40;pd_pixels(&d->layer[1])[y*128+x]=(x+y)&1?0x80ffffff:0;}
    assert(pd_frame_set_duration(d,120));
    assert(pd_frame_add(d,false));dot(d,0,3,4,0xff00ff00);assert(pd_frame_set_duration(d,240));
    assert(pd_frame_add(d,false));dot(d,0,5,6,0xff0000ff);dot(d,1,7,7,0xff00ffff);
    PdTag walk={"walk",0,1,0,0,0x123456};assert(pd_tag_add(d,&walk)==0);PdTag hold={"hold",1,2,2,3,0xabcdef};assert(pd_tag_add(d,&hold)==1);
    /* Frame 3 shares layer 0 with frame 1 and layer 1 with frame 2, so groups are non-contiguous. */
    assert(pd_frame_layer_copy(d,0,0,pd_pixels(&d->layer[0]))&&pd_cel_set_link(d,2,0,0));
    assert(pd_frame_layer_copy(d,1,1,pd_pixels(&d->layer[1]))&&pd_cel_set_link(d,2,1,1));
    char path[512],error[64];snprintf(path,sizeof(path),"%s/tagged-links.aseprite",dir);assert(pd_write_file(d,path,1,false,error)&&pd_exists(path));
    bool imported=pd_open_file(r,path,error);
    assert(imported&&!pd_dirty(r)&&pd_frame_count(r)==3&&pd_tag_count(r)==2&&r->enabled==3&&r->palette_count==2);
    assert(!strcmp(pd_tag(r,0)->name,"walk")&&pd_tag(r,0)->from==0&&pd_tag(r,0)->to==1&&pd_tag(r,0)->direction==0&&pd_tag(r,0)->repeat==0&&pd_tag(r,0)->color==0x123456);
    assert(!strcmp(pd_tag(r,1)->name,"hold")&&pd_tag(r,1)->from==1&&pd_tag(r,1)->to==2&&pd_tag(r,1)->direction==2&&pd_tag(r,1)->repeat==3&&pd_tag(r,1)->color==0xabcdef);
    assert(pd_cel_link(r,2,0)&&pd_cel_link(r,2,0)==pd_cel_link(r,0,0)&&!pd_cel_link(r,1,0)&&pd_cel_link(r,2,1)==pd_cel_link(r,1,1)&&!pd_cel_link(r,0,1));
    assert(r->layer[1].opacity==160&&r->layer[1].blend==1&&!strcmp(r->names[1],"Top")&&r->palette[1]==0x80112233);
    for(unsigned f=0;f<3;f++){assert(pd_frame_switch(d,f)&&pd_frame_switch(r,f)&&pd_frame_duration(r)==pd_frame_duration(d));for(unsigned l=0;l<2;l++)assert(!memcmp(pd_pixels(&d->layer[l]),pd_pixels(&r->layer[l]),pd_bytes(d)));}
    /* A shared edit still reaches every member of both imported groups. */
    assert(pd_frame_switch(r,2));dot(r,0,9,9,0xff123456);dot(r,1,10,10,0xff654321);
    assert(pd_frame_switch(r,0)&&px(r,9,9)==0xff123456);
    assert(pd_frame_switch(r,1)&&pd_pixels(&r->layer[1])[10*r->stride+10]==0xff654321);
    /* Hand-built file: tag colour in user data, background layer, modern palette before a legacy
     * palette, ignored cel-extra/slice chunks and a linked frame. */
    enum { CW=8,CH=8,CLAYER=25,CCEL=26+CW*CH*4,CPAL=32,COLD=13,CTAGS=38,CUSER=18,CEXTRA=42,CSLICE=20,CF0=16+CLAYER+CCEL+CPAL+COLD+CTAGS+CUSER+CEXTRA+CSLICE,CF1=16+24,CTOTAL=128+CF0+CF1 };
    unsigned char *f=malloc(CTOTAL);assert(f);memset(f,0,CTOTAL);
    tl32(f,CTOTAL);tl16(f+4,0xa5e0);tl16(f+6,2);tl16(f+8,CW);tl16(f+10,CH);tl16(f+12,32);tl32(f+14,1);tl16(f+18,100);f[34]=f[35]=1;
    size_t at=128;tl32(f+at,CF0);tl16(f+at+4,0xf1fa);tl16(f+at+6,8);tl16(f+at+8,100);tl32(f+at+12,8);at+=16;
    unsigned char *p=f+at;tl32(p,CLAYER);tl16(p+4,0x2004);tl16(p+6,11);p[12]=255;tl16(p+16,1);p[18]='A';at+=CLAYER;
    p=f+at;tl32(p,CCEL);tl16(p+4,0x2005);p[12]=255;tl16(p+22,CW);tl16(p+24,CH);p[26+(1*CW+1)*4]=255;p[26+(1*CW+1)*4+3]=255;at+=CCEL;
    p=f+at;tl32(p,CPAL);tl16(p+4,0x2019);tl32(p+6,1);tl32(p+14,0);p[28]=0x11;p[29]=0x22;p[30]=0x33;p[31]=0xff;at+=CPAL;
    p=f+at;tl32(p,COLD);tl16(p+4,0x0004);tl16(p+6,1);p[8]=0;p[9]=1;p[10]=0x00;p[11]=0xff;p[12]=0x00;at+=COLD;
    p=f+at;tl32(p,CTAGS);tl16(p+4,0x2018);tl16(p+6,1);tl16(p+16,0);tl16(p+18,0);p[20]=3;tl16(p+21,2);tl16(p+33,3);memcpy(p+35,"one",3);at+=CTAGS;
    p=f+at;tl32(p,CUSER);tl16(p+4,0x2020);tl32(p+6,3);tl16(p+10,2);memcpy(p+12,"hi",2);p[14]=0x40;p[15]=0x80;p[16]=0xc0;p[17]=0xff;at+=CUSER;
    p=f+at;tl32(p,CEXTRA);tl16(p+4,0x2006);at+=CEXTRA;
    p=f+at;tl32(p,CSLICE);tl16(p+4,0x2022);tl32(p+6,0);at+=CSLICE;
    tl32(f+at,CF1);tl16(f+at+4,0xf1fa);tl16(f+at+6,1);tl16(f+at+8,100);tl32(f+at+12,1);p=f+at+16;tl32(p,24);tl16(p+4,0x2005);p[12]=255;tl16(p+13,1);tl16(p+22,0);at+=CF1;
    assert(at==CTOTAL);snprintf(path,sizeof(path),"%s/crafted-tags.aseprite",dir);FILE *out=fopen(path,"wb");assert(out&&fwrite(f,1,CTOTAL,out)==CTOTAL&&!fclose(out));free(f);
    assert(pd_open_file(r,path,error)&&pd_frame_count(r)==2&&pd_tag_count(r)==1&&!pd_dirty(r)&&r->palette_count==1&&r->palette[0]==0xff112233);
    const PdTag *tg=pd_tag(r,0);assert(tg&&!strcmp(tg->name,"one")&&tg->from==0&&tg->to==0&&tg->direction==3&&tg->repeat==2&&tg->color==0x4080c0);
    assert(pd_cel_link(r,1,0)&&pd_cel_link(r,1,0)==pd_cel_link(r,0,0)&&px(r,1,1)==0xffff0000&&pd_frame_switch(r,1)&&px(r,1,1)==0xffff0000);
    uint32_t preview[4096];PdPreviewInfo pi;assert(pd_preview_file_info(path,preview,2,&pi,error)&&pi.frames==2&&pi.width==8&&pi.layers==1);
    /* A single-frame project still exports and reimports its tag. */
    PdDocument *s=calloc(1,sizeof(*s)),*q=calloc(1,sizeof(*q));assert(s&&q);pd_init(s);pd_init(q);assert(pd_create(s,8,8,2));dot(s,0,2,2,0xff336699);
    PdTag solo={"solo",0,0,1,5,0x00ff00};assert(pd_tag_add(s,&solo)==0);snprintf(path,sizeof(path),"%s/solo-tag.aseprite",dir);assert(pd_write_file(s,path,1,false,error)&&pd_exists(path));
    assert(pd_open_file(q,path,error)&&pd_frame_count(q)==1&&pd_tag_count(q)==1&&!strcmp(pd_tag(q,0)->name,"solo")&&pd_tag(q,0)->direction==1&&pd_tag(q,0)->repeat==5&&pd_tag(q,0)->color==0x00ff00&&px(q,2,2)==0xff336699);
    pd_dispose(s);pd_dispose(q);free(s);free(q);
    pd_dispose(d);pd_dispose(r);free(d);free(r);puts("Aseprite tags and linked cels: chunk 0x2018, user-data colours, non-contiguous linked cels, single-frame tags, layer/palette properties, exact roundtrip and shared edits after import: PASS");
}
static void tag_ui_checks(const char *dir){
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);
    for(unsigned mode=0;mode<2;mode++){
        int tw=mode?400:256;app_init(a,mode?320:256,mode?240:192,"","TEST");a->page=1;a->layers_tab=1;
        dot(&a->doc,0,4,4,0xff112233);assert(pd_frame_add(&a->doc,false));dot(&a->doc,0,5,4,0xff445566);assert(pd_frame_add(&a->doc,false));dot(&a->doc,0,6,4,0xff778899);
        /* A frame number opens the frame sheet; its TAG row creates a one-frame tag there. */
        tap_point(a,PD_TL_NUMBER,1);assert(!a->frame_sheet&&pd_frame_index(&a->doc)==1);tap_point(a,PD_TL_NUMBER,1);assert(a->frame_sheet);
        tap_point(a,PD_SHEET_TAG,0);assert(a->tag_sheet&&!a->frame_sheet&&pd_tag_count(&a->doc)==1);
        const PdTag *tg=pd_tag(&a->doc,0);assert(tg&&tg->from==1&&tg->to==1);
        /* Range steppers cannot cross, and direction and colour cycle. */
        tap_point(a,PD_TAG_TO,1);assert(pd_tag(&a->doc,0)->to==2);
        tap_point(a,PD_TAG_FROM,0);assert(pd_tag(&a->doc,0)->from==0);tap_point(a,PD_TAG_FROM,1);assert(pd_tag(&a->doc,0)->from==1);
        unsigned d0=pd_tag(&a->doc,0)->direction;tap_point(a,PD_TAG_DIRECTION,0);assert(pd_tag(&a->doc,0)->direction==(d0+1)%4);
        uint32_t c0=pd_tag(&a->doc,0)->color;tap_point(a,PD_TAG_COLOR,0);assert(pd_tag(&a->doc,0)->color!=c0);
        a->redraw=true;app_render(a,b,t,tw,a->height);ppm(dir,mode?"3ds-tag-sheet":"nds-tag-sheet",b,a->width,a->height);
        tap_point(a,PD_TAG_DONE,0);assert(!a->tag_sheet&&a->page==1);
        a->redraw=true;app_render(a,b,t,tw,a->height);ppm(dir,mode?"3ds-tag-band":"nds-tag-band",b,a->width,a->height);
        /* The band opens the tag it covers; an untagged column does nothing. */
        tap_point(a,PD_TL_TAGBAND,0);assert(!a->tag_sheet);
        tap_point(a,PD_TL_TAGBAND,1);assert(a->tag_sheet&&a->tag_edit==0);
        tap_point(a,PD_TAG_NAME,0);assert(a->page==5&&a->name_action==6);
        app_text_result(a,"walk",true);app_input(a,false,0,0,0,0);assert(a->page==1&&a->tag_sheet&&!strcmp(pd_tag(&a->doc,0)->name,"walk"));
        tap_point(a,PD_TAG_DELETE,0);assert(!a->tag_sheet&&pd_tag_count(&a->doc)==0);
        /* Cel sheet: link to the previous frame, unlink, then add a linked frame. */
        tap_point(a,PD_TL_CEL,1);assert(a->cel_sheet&&pd_frame_index(&a->doc)==1&&a->doc.active==0);
        tap_point(a,PD_CEL_LINK,0);assert(pd_cel_link(&a->doc,1,0)&&pd_cel_link(&a->doc,0,0)==pd_cel_link(&a->doc,1,0));
        tap_point(a,PD_CEL_UNLINK,0);assert(!pd_cel_link(&a->doc,1,0)&&!pd_cel_link(&a->doc,0,0));
        a->redraw=true;app_render(a,b,t,tw,a->height);ppm(dir,mode?"3ds-cel-sheet":"nds-cel-sheet",b,a->width,a->height);
        unsigned before=pd_frame_count(&a->doc);tap_point(a,PD_CEL_NEW_LINKED,0);assert(pd_frame_count(&a->doc)==before+1&&!a->cel_sheet&&pd_cel_link(&a->doc,2,0));
        app_dispose(a);
    }
    free(a);free(b);free(t);puts("Tag and cel sheets: frame TAG row, range/direction/colour edits, keyboard rename, band entry, link/unlink/add-linked: PASS");
}
static void strip_checks(const char *dir){
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);
    for(unsigned mode=0;mode<2;mode++){int tw=mode?400:256,x=0,y=0;app_init(a,mode?320:256,mode?240:192,"","TEST");
        /* One frame keeps the full-height canvas and no strip. */
        assert(!app_test_point(a,PD_STRIP_ITEM,0,&x,&y));
        dot(&a->doc,0,10,10,0xff0000ff);assert(pd_frame_add(&a->doc,true));dot(&a->doc,0,12,10,0xff00ff00);assert(pd_frame_add(&a->doc,true));dot(&a->doc,0,14,10,0xffff0000);assert(pd_frame_switch(&a->doc,1));
        a->zoom=2;a->redraw=true;app_render(a,b,t,tw,a->height);
        tap_point(a,PD_STRIP_ITEM,2);assert(pd_frame_index(&a->doc)==2);tap_point(a,PD_STRIP_ITEM,2);assert(pd_frame_index(&a->doc)==0);tap_point(a,PD_STRIP_ITEM,0);assert(pd_frame_index(&a->doc)==2);
        tap_point(a,PD_STRIP_TICK,1);assert(pd_frame_index(&a->doc)==1&&!a->page);
        /* Ticks scrub while dragged; touches on the strip never paint. */
        unsigned edits=a->doc.count;assert(app_test_point(a,PD_STRIP_TICK,0,&x,&y));app_input(a,true,x,y,0,0);assert(pd_frame_index(&a->doc)==0);
        int x2=0,y2=0;assert(app_test_point(a,PD_STRIP_TICK,2,&x2,&y2));app_input(a,true,x2,y2,0,0);assert(pd_frame_index(&a->doc)==2);app_input(a,false,0,0,0,0);assert(a->doc.count==edits&&!a->stroke);
        tap_point(a,PD_STRIP_ITEM,1);assert(a->page==1&&a->layers_tab==1);a->page=0;
        /* Onion: a tap toggles, a 400 ms hold opens the settings sheet over the canvas without toggling. */
        tap_point(a,PD_STRIP_ITEM,4);assert(a->onion_enabled);assert(app_test_point(a,PD_STRIP_ITEM,4,&x,&y));a->now_ms=1000;app_input(a,true,x,y,0,0);a->now_ms=1450;app_input(a,true,x,y,0,0);app_input(a,false,0,0,0,0);assert(a->onion_enabled&&a->onion_sheet);
        tap_point(a,PD_ONION_BEHIND,1);assert(a->onion_behind==2);a->redraw=true;app_render(a,b,t,tw,a->height);ppm(dir,mode?"3ds-canvas-onion-sheet":"nds-canvas-onion-sheet",b,a->width,a->height);
        tap(a,2,a->height/2);assert(!a->onion_sheet&&a->doc.count==edits);a->redraw=true;app_render(a,b,t,tw,a->height);ppm(dir,mode?"3ds-canvas-strip":"nds-canvas-strip",b,a->width,a->height);
        /* The strip's top row stays strip-coloured after an incremental stroke ends at the canvas bottom. */
        int bottom_y=a->height-(mode?22:18);uint32_t strip_px=b[(bottom_y+1)*a->width+a->width-6];
        a->now_ms=3000;app_input(a,true,a->width/2,bottom_y-1,0,0);app_render(a,b,t,tw,a->height);assert(b[(bottom_y+1)*a->width+a->width-6]==strip_px);app_input(a,false,0,0,0,0);
        tap_point(a,PD_STRIP_ITEM,3);assert(a->playback.active);tap(a,a->width/2,80);assert(!a->playback.active&&a->doc.count==edits+1);
        /* Sample+L toggles onion, Sample+R plays; any button then stops. */
        app_input(a,false,0,0,PD_SHOULDER_L,PD_SAMPLE|PD_SHOULDER_L);assert(!a->onion_enabled);app_input(a,false,0,0,0,0);
        app_input(a,false,0,0,PD_SHOULDER_R,PD_SAMPLE|PD_SHOULDER_R);assert(a->playback.active);app_input(a,false,0,0,0,0);assert(a->playback.active);app_input(a,false,0,0,PD_SAMPLE,PD_SAMPLE);assert(!a->playback.active);
        app_dispose(a);
    }
    free(a);free(b);free(t);puts("Canvas frame strip: step/wrap, tick scrub, timeline shortcut, onion tap/hold sheet, no ink on the strip, clipping and Sample+L/R: PASS");
}
static void timeline_checks(const char *dir){
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);
    for(unsigned mode=0;mode<2;mode++){app_init(a,mode?320:256,mode?240:192,"","TEST");assert(app_set_profile(a,mode?3:1));a->doc.enabled=255;dot(&a->doc,0,2,2,0xffabcdef);a->page=3;tap(a,a->width*7/12,10);assert(a->page==1&&!a->layers_tab);tap_point(a,PD_TL_TAB,1);assert(a->page==1&&a->layers_tab==1);
        /* Tabs switch between the layer list and the timeline on the same Layers screen. */
        tap_point(a,PD_TL_TAB,0);assert(a->page==1&&!a->layers_tab);app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-layers-tab":"nds-layers-tab",b,a->width,a->height);tap_point(a,PD_TL_TAB,1);assert(a->layers_tab==1);
        tap_point(a,PD_TL_FOOTER,0);assert(pd_frame_count(&a->doc)==2&&pd_frame_index(&a->doc)==1&&!pd_frame_has_cel(&a->doc,1,0));
        tap_point(a,PD_TL_FOOTER,1);assert(pd_frame_count(&a->doc)==3&&pd_frame_index(&a->doc)==2);
        /* The first visible layer is the topmost stable slot, not slot zero. */
        tap_point(a,PD_TL_CEL,0);assert(pd_frame_index(&a->doc)==0&&a->doc.active==7);
        tap_point(a,PD_TL_FOOTER,3);assert(a->frame_sheet);tap_point(a,PD_SHEET_PLUS,0);assert(pd_frame_duration(&a->doc)==110);tap_point(a,PD_SHEET_MINUS,0);tap_point(a,PD_SHEET_MINUS,0);assert(pd_frame_duration(&a->doc)==90);
        tap_point(a,PD_SHEET_PRESET,1);assert(pd_frame_duration(&a->doc)==80);assert(pd_frame_switch(&a->doc,1)&&pd_frame_duration(&a->doc)==100&&pd_frame_switch(&a->doc,0));
        tap_point(a,PD_SHEET_SCOPE,1);assert(a->frame_apply_all);tap_point(a,PD_SHEET_PRESET,3);for(unsigned f=0;f<3;f++){assert(pd_frame_switch(&a->doc,f)&&pd_frame_duration(&a->doc)==150);}assert(pd_frame_switch(&a->doc,0));
        tap_point(a,PD_SHEET_SCOPE,0);assert(!a->frame_apply_all);app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-frame-sheet":"nds-frame-sheet",b,a->width,a->height);
        tap(a,2,a->height-2);assert(!a->frame_sheet);
        app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-timeline":"nds-timeline",b,a->width,a->height);ppm(dir,mode?"3ds-timeline-top":"nds-timeline-top",t,mode?400:256,a->height);
        /* Eight layers need the scrollbar on both screens; the bottom layer is slot zero. */
        while(a->timeline_layer_first<8u-(mode?6u:5u)){unsigned before=a->timeline_layer_first;tap_point(a,PD_TL_SCROLL,1);assert(a->timeline_layer_first==before+1);}
        tap_point(a,PD_TL_SCROLL,1);assert(a->timeline_layer_first==8u-(mode?6u:5u));tap_point(a,PD_TL_NAME,7);assert(a->doc.active==0&&pd_frame_index(&a->doc)==0);tap_point(a,PD_TL_SCROLL,0);assert(a->timeline_layer_first==2u-(mode?1u:0u));
        tap_point(a,PD_TL_FOOTER,2);assert(a->frame_delete);tap(a,a->width-20,130);assert(!a->frame_delete&&pd_frame_count(&a->doc)==3);
        /* Sideways drags scroll frames without switching; holding a number drags the frame to a new column. */
        for(unsigned i=0;i<17;i++)assert(pd_frame_add(&a->doc,false));assert(pd_frame_switch(&a->doc,0));a->timeline_first=a->timeline_layer_first=0;unsigned cols=mode?14:13;
        int x0=0,y0=0;assert(app_test_point(a,PD_TL_CEL,5,&x0,&y0));a->now_ms=5000;app_input(a,true,x0,y0,0,0);a->now_ms+=16;app_input(a,true,x0-60,y0,0,0);app_input(a,false,0,0,0,0);
        assert(a->timeline_first==(mode?3u:4u)&&pd_frame_index(&a->doc)==0);a->timeline_first=0;
        dot(&a->doc,0,4,4,0xff00ff00);assert(app_test_point(a,PD_TL_NUMBER,0,&x0,&y0));a->now_ms=6000;app_input(a,true,x0,y0,0,0);a->now_ms+=350;app_input(a,true,x0,y0,0,0);assert(a->timeline_reorder);
        app_input(a,true,x0+3*(mode?16:14),y0,0,0);app_input(a,false,0,0,0,0);assert(!a->timeline_reorder&&pd_frame_index(&a->doc)==3&&pd_pixels(&a->doc.layer[0])[4*128+4]==0xff00ff00);
        a->timeline_first=20-cols;tap_point(a,PD_TL_NUMBER,19);assert(pd_frame_index(&a->doc)==19&&a->timeline_first==20-cols);
        /* Onion sheet: toggle, counts clamp to 1-3, opacity slider and tint. */
        tap_point(a,PD_TL_ONION,0);assert(a->onion_sheet);tap_point(a,PD_ONION_TOGGLE,0);assert(a->onion_enabled);
        for(int i=0;i<4;i++)tap_point(a,PD_ONION_BEHIND,1);assert(a->onion_behind==3);tap_point(a,PD_ONION_AHEAD,0);assert(a->onion_ahead==1);
        tap_point(a,PD_ONION_SLIDER,90);assert(a->onion_opacity==90);tap_point(a,PD_ONION_SLIDER,10);assert(a->onion_opacity==10);tap_point(a,PD_ONION_SLIDER,50);assert(a->onion_opacity==50);
        tap_point(a,PD_ONION_TINT,0);assert(!a->onion_tint);app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-onion-sheet":"nds-onion-sheet",b,a->width,a->height);
        tap_point(a,PD_ONION_TINT,0);tap_point(a,PD_ONION_TOGGLE,0);assert(!a->onion_enabled&&!a->onion);tap_point(a,PD_SHEET_CLOSE,0);assert(!a->onion_sheet);
        assert(pd_frame_switch(&a->doc,0));while(pd_frame_count(&a->doc)>3)assert(pd_frame_delete(&a->doc));assert(pd_frame_switch(&a->doc,0));
        tap(a,a->width*7/12,10);assert(!a->page);app_input(a,false,0,0,PD_SHOULDER_R,PD_PAN|PD_ERASE|PD_SHOULDER_R);assert(pd_frame_index(&a->doc)==1);app_input(a,false,0,0,0,0);app_input(a,false,0,0,PD_FRAME_PREV,0);assert(pd_frame_index(&a->doc)==0);
        app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-build-label":"nds-build-label",t,mode?400:256,a->height);
        a->page=7;a->input_contacts=a->ink_starts=24;app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-input-diagnostics":"nds-input-diagnostics",b,a->width,a->height);
    }
    app_dispose(a);free(a);free(b);free(t);puts("Timeline: native layouts, ordered cel hitboxes, layer scrolling, frame creation/duplication, duration, delete cancel and frame shortcuts: PASS");
}

