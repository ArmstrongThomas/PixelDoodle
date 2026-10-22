#include "app.h"
#include "community.h"
#include "dns_packet.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"
/* Render-equivalence copies own their masks, as the former embedded arrays did; clipboard and thumbnails stay with the source. */
static void clone_app(PdApp *dst,const PdApp *src){
    uint32_t *own[4]={dst->selection,dst->selection_base,dst->selection_draft,dst->guide_damage};
    const uint32_t *from[4]={src->selection,src->selection_base,src->selection_draft,src->guide_damage};
    for(int i=0;i<4;i++)if(own[i]!=from[i])free(own[i]);
    *dst=*src;dst->clipboard=NULL;dst->gallery_thumbs=NULL;
    uint32_t **to[4]={&dst->selection,&dst->selection_base,&dst->selection_draft,&dst->guide_damage};
    for(int i=0;i<4;i++){*to[i]=malloc(src->mask_words*4u);assert(*to[i]);memcpy(*to[i],from[i],src->mask_words*4u);}
}
static void dot(PdDocument *d,unsigned l,int x,int y,uint32_t c){d->enabled|=(uint8_t)(1u<<l);d->active=l;assert(pd_begin(d));pd_line(d,x,y,x,y,c);assert(pd_commit(d));}
static void community_json_checks(void){
    static const char body[]={"{\"version\":1,\"items\":[{\"id\":\"art-1\",\"title\":\"Blue \\\"bird\\\"\",\"artist\":{\"id\":\"artist-1\",\"name\":\"A. Artist\",\"bio\":\"ignored\"},\"tags\":[\"blue\"],\"width\":64,\"height\":48,\"image\":\"/media/art-1.png\"}],\"total\":9,\"nextOffset\":6,\"tags\":[\"blue\"]}"};
    PdCommunityPage page;assert(pd_community_parse_page(body,sizeof(body)-1,&page));assert(page.count==1&&page.total==9&&page.has_total&&page.has_next&&page.next_offset==6);assert(!strcmp(page.items[0].id,"art-1")&&!strcmp(page.items[0].title,"Blue \"bird\"")&&!strcmp(page.items[0].artist,"A. Artist"));assert(page.items[0].width==64&&page.items[0].height==48);
    static const char unicode[]={"{\"items\":[{\"id\":\"unicode\",\"title\":\"Caf\\u00e9 \\ud83d\\ude80\",\"artist\":{\"name\":\"Ren\\u00e9\"},\"width\":32,\"height\":24}],\"total\":1,\"nextOffset\":null}"};assert(pd_community_parse_page(unicode,sizeof(unicode)-1,&page));assert(!strcmp(page.items[0].title,"Caf\xc3\xa9 \xf0\x9f\x9a\x80")&&!strcmp(page.items[0].artist,"Ren\xc3\xa9"));
    static const char bad_surrogate[]={"{\"items\":[{\"id\":\"bad\",\"title\":\"\\ud83d\",\"artist\":{\"name\":\"A\"},\"width\":1,\"height\":1}],\"total\":1,\"nextOffset\":null}"};assert(!pd_community_parse_page(bad_surrogate,sizeof(bad_surrogate)-1,&page));
    static const char uploads[]={"{\"profileRequired\":false,\"items\":[{\"id\":\"art-2\",\"title\":\"My art\",\"status\":\"rejected\",\"reason\":\"Please revise\",\"width\":32,\"height\":24}],\"nextOffset\":null}"};assert(pd_community_parse_page(uploads,sizeof(uploads)-1,&page));assert(!page.has_total&&!page.profile_required&&page.count==1&&!strcmp(page.items[0].status,"rejected")&&!strcmp(page.items[0].reason,"Please revise"));
    static const char bad_dim[]={"{\"items\":[{\"id\":\"x\",\"title\":\"X\",\"artist\":{\"name\":\"Y\"},\"width\":1921,\"height\":48}],\"total\":1,\"nextOffset\":null}"};assert(!pd_community_parse_page(bad_dim,sizeof(bad_dim)-1,&page));
    static const char bad_trailing[]={"{\"items\":[],\"total\":0,\"nextOffset\":null} junk"};assert(!pd_community_parse_page(bad_trailing,sizeof(bad_trailing)-1,&page));
}
static void dns_packet_checks(void){uint8_t q[64]={0},r[96]={0};size_t qn=pd_dns_build_a_query(q,sizeof(q),0x1234,"pixels.7db.pw");assert(qn==31&&q[0]==0x12&&q[1]==0x34&&q[5]==1);assert(!pd_dns_build_a_query(q,16,1,"pixels.7db.pw"));assert(!pd_dns_build_a_query(q,sizeof(q),1,"pixels..7db.pw"));r[0]=0x12;r[1]=0x34;r[2]=0x81;r[3]=0x80;r[5]=1;r[7]=1;memcpy(r+12,q+12,qn-12);size_t at=qn;r[at++]=0xc0;r[at++]=0x0c;r[at++]=0;r[at++]=1;r[at++]=0;r[at++]=1;at+=4;r[at++]=0;r[at++]=4;r[at++]=203;r[at++]=0;r[at++]=113;r[at++]=25;uint32_t ip=0;assert(pd_dns_parse_a_response(r,at,0x1234,&ip));const uint8_t *bytes=(const uint8_t*)&ip;assert(bytes[0]==203&&bytes[1]==0&&bytes[2]==113&&bytes[3]==25);assert(!pd_dns_parse_a_response(r,at,0x1235,&ip));assert(!pd_dns_parse_a_response(r,at-1,0x1234,&ip));r[3]=0x83;assert(!pd_dns_parse_a_response(r,at,0x1234,&ip));puts("Bounded DNS query/parser, compressed answer, ID, rcode and truncation checks: PASS");}
static void ppm(const char *dir,const char *name,uint32_t *p,int w,int h){char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",w,h);for(int i=0;i<w*h;i++){unsigned char c[3]={(unsigned char)(p[i]>>16),(unsigned char)(p[i]>>8),(unsigned char)p[i]};assert(fwrite(c,1,3,f)==3);}assert(!fclose(f));}
static uint32_t reference_pixel(PdDocument *d,unsigned i){
    unsigned a=0,r=0,g=0,b=0;
    for(unsigned j=0;j<PD_LAYERS;j++){
        PdLayer *l=&d->layer[j];if(!l->visible)continue;
        uint32_t p=l->pixels[i];unsigned sa=((p>>24)*l->opacity+127)/255;
        r=((p>>16)&255)*sa+(r*(255-sa)+127)/255;
        g=((p>>8)&255)*sa+(g*(255-sa)+127)/255;
        b=(p&255)*sa+(b*(255-sa)+127)/255;a=sa+(a*(255-sa)+127)/255;
    }
    return a?(a<<24)|(((r+a/2)/a)<<16)|(((g+a/2)/a)<<8)|((b+a/2)/a):0;
}
static void render_checks(PdApp *a,uint32_t *bottom,uint32_t *top){
    a->doc.enabled=15;
    uint32_t seed=17;
    for(unsigned l=0;l<4;l++)for(unsigned i=0;i<PD_PIXELS;i++){
        seed=seed*1664525u+1013904223u;a->doc.layer[l].pixels[i]=seed;
    }
    for(unsigned mode=0;mode<8;mode++){
        for(unsigned l=0;l<4;l++){a->doc.layer[l].opacity=(uint8_t)(mode&1?255:64+l*63);a->doc.layer[l].visible=(uint8_t)(mode&2?l!=2:1);}
        if(mode&4)for(unsigned i=0;i<PD_PIXELS;i++)a->doc.layer[0].pixels[i]|=0xff000000u;
        pd_composite(&a->doc);
        for(unsigned i=0;i<PD_PIXELS;i++)assert(a->doc.composite[i]==reference_pixel(&a->doc,i));
    }
    a->page=0;
    for(int size=0;size<2;size++)for(int z=1;z<=32;z*=2){
        a->width=size?320:256;a->height=size?240:192;a->zoom=z;
        a->ox=z>1?3:0;a->oy=z>1?5:0;
        app_render(a,bottom,top,size?400:256,a->height);
        int left=128*z<a->width?(a->width-128*z)/2:0;
        int upper=24+(128*z<a->height-24?(a->height-24-128*z)/2:0);
        for(int y=24;y<a->height;y++)for(int x=0;x<a->width;x++){
            int wx=a->ox+(x-left)/z,wy=a->oy+(y-upper)/z;uint32_t expected=0x53616b;
            if(x>=left&&y>=upper&&wx<128&&wy<128){
                uint32_t p=a->doc.composite[wy*128+wx];unsigned alpha=p>>24,bg=((wx/8+wy/8)&1)?205:238;
                unsigned r=(((p>>16)&255)*alpha+bg*(255-alpha)+127)/255;
                unsigned g=(((p>>8)&255)*alpha+bg*(255-alpha)+127)/255;
                unsigned b=((p&255)*alpha+bg*(255-alpha)+127)/255;expected=(r<<16)|(g<<8)|b;
            }
            assert(bottom[y*a->width+x]==expected);
        }
    }
    puts("131072 composite oracle comparisons and DS/3DS viewport comparisons at all zoom levels: PASS");
}
static void incremental_checks(void){
    PdApp *a=calloc(1,sizeof(*a)),*reference=calloc(1,sizeof(*reference));
    uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4),*rb=malloc(320*240*4),*rt=malloc(400*240*4);
    assert(a&&reference&&b&&t&&rb&&rt);unsigned partials=0;
    for(unsigned pattern=0;pattern<4;pattern++)for(int dim=0;dim<14;dim++)for(int mode=0;mode<2;mode++)for(int z=1;z<=32;z*=2){
        int w=mode?320:256,h=mode?240:192,tw=mode?400:256;
        app_init(a,w,h,"","TEST");a->brush_pattern=pattern;a->doc.enabled=15;a->doc.width=dim==1?16:dim==2?37:128;a->doc.height=dim==1?32:dim==2?19:128;if(dim>=5){a->doc.width=dim==8?16:37;a->doc.height=dim==8?16:19;a->guides=dim==5?1:dim==6?2:dim==7?12:15;}a->zoom=z;a->ox=(int)a->doc.width>w/z?3:0;a->oy=(int)a->doc.height>(h-24)/z?5:0;
        if(dim>=9){a->guides=dim==9?4:dim==10?8:15;a->ox=(a->guides&4)?-12:a->ox;a->oy=(a->guides&8)?8:a->oy;}
        for(unsigned i=0;i<PD_PIXELS;i++){a->doc.layer[0].pixels[i]=0x80773399;a->doc.layer[2].pixels[i]=(i%7)?0:0x9022ee66;}
        a->doc.active=1;a->doc.layer[1].opacity=128;if(dim>=3){a->brush_size[0]=dim==3?4:16;a->brush_size[1]=7;a->brush_round[0]=a->brush_round[1]=dim==4;a->pixel_grid=true;}
        if(dim>=12){a->selection_valid=true;a->selection_layer=a->doc.active;a->selection_revision=a->doc.revision;pd_mask_rectangle(a->selection,2,2,12,12);a->guides=dim==12?0:15;if(dim==12)a->ox=a->oy=0;}
        app_render(a,b,t,tw,h);
        for(int step=0;step<40;step++){
            int x=70+(step*3)%100,y=65+(step*5)%70;
            app_input(a,true,x,y,0,step>20?PD_ERASE:0);
            if(!a->redraw)continue;
            clone_app(reference,a);reference->partial=false;
            app_render(reference,rb,rt,tw,h);app_render(a,b,t,tw,h);
            if(memcmp(b,rb,(size_t)w*h*4)){for(int i=0;i<w*h;i++)if(b[i]!=rb[i]){fprintf(stderr,"render mismatch dim=%d mode=%d zoom=%d step=%d at %d,%d actual=%x expected=%x selection=%d\n",dim,mode,z,step,i%w,i/w,b[i],rb[i],a->selection_valid);break;}assert(0);}
            if(!a->present_top){partials++;assert(a->px0>=0&&a->py0>=24&&a->px1<=w&&a->py1<=h);}
        }
        app_input(a,false,0,0,0,0);assert(!a->partial);app_render(a,b,t,tw,h);
        clone_app(reference,a);app_render(reference,rb,rt,tw,h);assert(!memcmp(b,rb,(size_t)w*h*4));assert(!memcmp(t,rt,(size_t)tw*h*4));
        app_input(a,true,5,5,0,0);app_render(a,b,t,tw,h);clone_app(reference,a);app_render(reference,rb,rt,tw,h);assert(!memcmp(b,rb,(size_t)w*h*4));
    }
    assert(partials>100);printf("Incremental/full render equivalence: %u stroke frames, both screens/all zooms/alpha/erase/undo: PASS\n",partials);
    free(a);free(reference);free(b);free(t);free(rb);free(rt);
}
static uint32_t fake_ticks;
static uint32_t fake_clock(void){return fake_ticks+=2500;}
static void tap(PdApp *a,int x,int y){app_input(a,true,x,y,0,0);app_input(a,false,0,0,0,0);}
static unsigned community_calls,community_last_mode,community_last_offset,community_favorite_calls,community_mock_http;
static bool community_mock_token,community_mock_saved;
static void community_mock_list(PdApp *a,unsigned mode,unsigned offset){community_calls++;community_last_mode=mode;community_last_offset=offset;if(mode>=2&&(community_mock_http==403||(!community_mock_token&&community_mock_http==401))){snprintf(a->community_status,sizeof(a->community_status),community_mock_http==403?"GRANT NEEDS MORE SCOPES; RE-PAIR":"GRANT EXPIRED OR REVOKED; PAIR AGAIN");if(community_mock_http==401)community_mock_saved=false;a->community_loading=false;a->redraw=true;return;}a->community_loading=false;a->community_loaded=true;a->community_page.count=1;a->community_page.total=1;a->community_page.has_total=true;a->community_page.items[0]=(PdCommunityArt){0};strcpy(a->community_page.items[0].id,"fixture-art");strcpy(a->community_page.items[0].title,"Fixture");strcpy(a->community_page.items[0].artist,"Artist");strcpy(a->community_page.items[0].status,"rejected");strcpy(a->community_page.items[0].reason,"Review reason");a->redraw=true;}
static void community_mock_favorite(PdApp *a,unsigned item,bool favorite){assert(community_mock_token&&item==0);community_favorite_calls++;a->community_action_busy=false;a->community_page.items[0].favorite=favorite;}
static void dropdown_component_checks(void){PdDropdown m={0};unsigned chosen=0;pd_dropdown_open(&m,8,48,240,18,26,3,4,0,true);assert(pd_dropdown_input(&m,true,true,20,80,&chosen)==PD_DROPDOWN_NONE);assert(pd_dropdown_input(&m,false,false,20,80,&chosen)==PD_DROPDOWN_NONE&&m.open);pd_dropdown_close(&m);pd_dropdown_open(&m,8,48,240,18,26,3,4,0,false);assert(pd_dropdown_input(&m,true,true,2,170,&chosen)==PD_DROPDOWN_NONE);assert(pd_dropdown_input(&m,false,false,2,170,&chosen)==PD_DROPDOWN_CLOSED&&!m.open);pd_dropdown_open(&m,8,48,240,18,26,3,4,0,false);pd_dropdown_input(&m,true,true,20,131,&chosen);assert(pd_dropdown_input(&m,true,false,20,100,&chosen)==PD_DROPDOWN_SCROLLED&&m.first==1);assert(pd_dropdown_input(&m,false,false,20,100,&chosen)==PD_DROPDOWN_SCROLLED&&m.open);pd_dropdown_input(&m,true,true,20,131,&chosen);assert(pd_dropdown_input(&m,false,false,20,131,&chosen)==PD_DROPDOWN_SELECTED&&chosen==3&&!m.open);puts("Reusable dropdown: touch capture, outside dismissal, clipped rows and swipe scrolling: PASS");}
static void community_ui_checks(const char *dir){PdApp *a=calloc(1,sizeof(*a));uint32_t *bottom=calloc(320*240,sizeof(uint32_t)),*top=calloc(400*240,sizeof(uint32_t));assert(a&&bottom&&top);community_mock_token=true;community_mock_saved=true;community_mock_http=200;app_init(a,256,192,dir,"TEST");a->page=28;a->online_list=community_mock_list;a->online_favorite=community_mock_favorite;community_calls=community_last_mode=community_last_offset=community_favorite_calls=0;
    tap(a,128,36);assert(a->community_dropdown.open&&a->community_mode==0);tap(a,20,105);assert(a->community_mode==1&&community_calls==1&&community_last_mode==1);tap(a,20,60);assert(a->selected==0&&a->preview_attempted);assert(app_reserve_gallery(a));a->gallery_ready[0]=true;for(unsigned i=0;i<1024;i++)a->gallery_thumbs[0][i]=0xff224466;app_render(a,bottom,top,256,192);assert(top[100*256+128]!=0);char cache[PD_PATH],cached[PD_PATH];assert(pd_join(cache,dir,"community-cache"));if(!pd_exists(cache))assert(pd_mkdir(cache));assert(pd_join(cached,cache,"fixture-art.png"));FILE *f=fopen(cached,"wb");assert(f);fputs("fixture",f);fclose(f);tap(a,80,176);assert(a->confirm==4);tap(a,192,176);assert(!a->confirm);tap(a,128,176);assert(community_favorite_calls==1&&a->community_page.items[0].favorite);
    tap(a,128,36);tap(a,20,131);assert(a->community_mode==2&&community_last_mode==2);tap(a,20,60);tap(a,128,176);assert(community_favorite_calls==2&&!a->community_page.items[0].favorite);
    tap(a,128,36);app_input(a,true,20,131,0,0);app_input(a,true,20,100,0,0);app_input(a,false,20,100,0,0);assert(a->community_dropdown.open&&a->community_dropdown.first==1);tap(a,20,131);assert(a->community_mode==3&&community_last_mode==3);tap(a,20,60);app_render(a,bottom,top,256,192);assert(a->selected==0);
    tap(a,128,36);app_input(a,true,20,80,0,0);app_input(a,true,20,110,0,0);app_input(a,false,20,110,0,0);assert(a->community_dropdown.open&&a->community_dropdown.first==0);tap(a,20,80);assert(a->community_mode==0&&a->gallery_count==0);
    /* Relaunch restores the saved grant, and revoked grants stop authenticated reads. */
    free(a);a=calloc(1,sizeof(*a));assert(a);app_init(a,256,192,dir,"TEST");a->page=28;a->online_list=community_mock_list;community_mock_token=community_mock_saved;tap(a,128,36);tap(a,20,105);tap(a,128,36);tap(a,20,131);assert(community_mock_token&&community_last_mode==2);
    community_mock_http=401;community_mock_token=false;tap(a,128,36);tap(a,20,131);assert(!community_mock_saved&&!community_mock_token&&strstr(a->community_status,"EXPIRED OR REVOKED"));
    community_mock_http=403;community_mock_token=true;community_mock_saved=true;tap(a,128,36);tap(a,20,131);assert(community_mock_saved&&community_mock_token&&strstr(a->community_status,"MORE SCOPES"));
    free(a);free(bottom);free(top);puts("Community dropdown mode switch, favorite, My Uploads, relaunch and 401/403 grant fixtures: PASS");}
static void adjustment_checks(const char *dir){
    assert(pd_adjust_color(0xffff0000,120,0,0)==0xff00ff00);
    assert(pd_adjust_color(0xffff0000,0,-100,0)==0xffffffff);
    assert((pd_adjust_color(0x80abcdef,180,0,0)&0xffffffu)==(pd_adjust_color(0xffabcdef,180,0,0)&0xffffffu));
    assert(pd_adjust_color(0x00112233,90,50,50)==0x00112233);

    PdDocument *d=calloc(1,sizeof(*d));assert(d);pd_init(d);d->enabled=3;
    dot(d,0,2,2,0x80ff0000);dot(d,1,2,2,0xff0000ff);d->active=0;d->history_used=d->count=d->cursor=0;
    assert(pd_adjust_layers(d,1,NULL,120,0,0));assert(d->layer[0].pixels[2*128+2]==0x8000ff00);assert(d->layer[1].pixels[2*128+2]==0xff0000ff);assert(d->count==1);
    assert(pd_undo(d)&&d->layer[0].pixels[2*128+2]==0x80ff0000&&d->cursor==0);assert(pd_redo(d)&&d->layer[0].pixels[2*128+2]==0x8000ff00);
    pd_init(d);d->enabled=3;d->active=0;dot(d,0,2,4,0xffff0000);dot(d,1,2,4,0xff0000ff);d->active=0;d->history_used=d->count=d->cursor=0;pd_composite(d);
    assert(pd_adjust_preview(d,2,NULL,120,0,0));assert(pd_before(d)[4*128+2]==0xffff0000);assert(pd_adjust_preview(d,1,NULL,120,0,0));assert(pd_before(d)[4*128+2]==0xff0000ff);assert(d->layer[1].pixels[4*128+2]==0xff0000ff&&!d->count);
    pd_init(d);d->enabled=3;d->active=0;dot(d,0,2,5,0x80ff0000);dot(d,1,2,5,0xff0000ff);d->layer[1].opacity=128;d->layer[1].blend=1;d->active=0;d->history_used=d->count=d->cursor=0;pd_composite(d);
    assert(pd_adjust_preview(d,2,NULL,60,20,-10));uint32_t expected=pd_before(d)[5*128+2];assert(pd_adjust_layers(d,2,NULL,60,20,-10));pd_composite(d);assert(pd_image(d)[5*128+2]==expected);

    pd_init(d);d->enabled=3;d->active=0;dot(d,0,3,3,0xffff0000);dot(d,1,3,3,0xff0000ff);dot(d,1,4,4,0xff00ff00);d->active=0;d->history_used=d->count=d->cursor=0;
    assert(pd_adjust_layers(d,2,NULL,120,0,0));assert(d->layer[0].pixels[3*128+3]==0xff00ff00&&d->layer[1].pixels[3*128+3]==0xffff0000&&d->layer[1].pixels[4*128+4]==0xff0000ff&&d->count==2&&d->active==0);
    assert(pd_undo(d)&&d->layer[0].pixels[3*128+3]==0xffff0000&&d->layer[1].pixels[3*128+3]==0xff0000ff&&d->layer[1].pixels[4*128+4]==0xff00ff00&&d->cursor==0);assert(pd_redo(d)&&d->layer[0].pixels[3*128+3]==0xff00ff00&&d->layer[1].pixels[3*128+3]==0xffff0000);
    pd_init(d);d->enabled=3;d->active=0;dot(d,0,5,5,0xffff0000);dot(d,1,5,5,0xff0000ff);d->history_used=d->count=d->cursor=0;d->layer[1].visible=0;assert(pd_adjust_layers(d,2,NULL,120,0,0));assert(d->layer[0].pixels[5*128+5]==0xff00ff00&&d->layer[1].pixels[5*128+5]==0xff0000ff);
    pd_init(d);d->enabled=3;d->active=0;dot(d,0,6,6,0xffff0000);d->layer[0].flags=PD_LAYER_LOCK;assert(!pd_adjust_layers(d,1,NULL,120,0,0));
    pd_init(d);dot(d,0,7,7,0x80ff0000);dot(d,0,8,7,0x80ff0000);d->history_used=d->count=d->cursor=0;uint32_t mask[PD_PIXELS/32]={0};unsigned index=7*128+7;mask[index/32]=1u<<(index%32);
    assert(pd_adjust_layers(d,0,mask,120,0,0));assert(d->layer[0].pixels[index]==0x8000ff00&&d->layer[0].pixels[7*128+8]==0x80ff0000);assert(pd_undo(d)&&d->layer[0].pixels[index]==0x80ff0000);
    assert(pd_create(d,512,512,3));d->enabled=7;for(unsigned i=0;i<3;i++)for(unsigned j=0;j<pd_plane(d);j++)pd_pixels(&d->layer[i])[j]=i==0?0xffff0000:i==1?0xff0000ff:0xff00ff00;d->history_used=d->count=d->cursor=0;d->active=0;
    assert(!pd_adjust_layers(d,2,NULL,120,0,0));assert(pd_pixels(&d->layer[0])[0]==0xffff0000&&pd_pixels(&d->layer[1])[0]==0xff0000ff&&!d->count);

    PdApp *a=calloc(1,sizeof(*a));uint32_t *bottom=calloc(320*240,sizeof(uint32_t)),*top=calloc(400*240,sizeof(uint32_t));assert(a&&bottom&&top);
    app_init(a,256,192,dir,"TEST");dot(&a->doc,0,9,9,0xffff0000);a->doc.history_used=a->doc.count=a->doc.cursor=0;app_input(a,false,0,0,PD_MENU,0);tap(a,128,164);assert(a->page==31&&a->adjust_scope==1&&a->adjust_preview_ready&&!a->doc.count);
    tap(a,42,40);assert(a->adjust_scope==0);tap(a,80,175);assert(a->page==31&&!a->doc.count);
    tap(a,128,40);tap(a,236,70);assert(a->adjust_scope==1&&a->adjust_hue==5&&a->adjust_preview_ready&&!a->adjust_preview_stale);a->adjust_hue=120;a->touching=true;a->adjust_preview_stale=true;app_input(a,false,0,0,0,0);assert(a->adjust_hue==120&&!a->adjust_preview_stale&&a->adjust_preview_ready);a->width=320;a->height=240;app_render(a,bottom,top,400,240);
    bool green_after=false;for(int i=0;i<400*240;i++)if(top[i]==0x00ff00){green_after=true;break;}assert(green_after);ppm(dir,"3ds-adjust-colors",bottom,320,240);ppm(dir,"3ds-adjust-preview-top",top,400,240);a->width=256;a->height=192;
    tap(a,80,175);assert(!a->page&&a->doc.layer[0].pixels[9*128+9]==0xff00ff00&&a->doc.count==1&&a->doc.active==0);
    app_init(a,256,192,dir,"TEST");dot(&a->doc,0,10,10,0xffff0000);a->doc.history_used=a->doc.count=a->doc.cursor=0;app_input(a,false,0,0,PD_MENU,0);tap(a,128,164);a->adjust_hue=120;a->touching=true;a->adjust_preview_stale=true;app_input(a,false,0,0,0,0);app_render(a,bottom,top,256,192);ppm(dir,"nds-adjust-colors",bottom,256,192);ppm(dir,"nds-adjust-preview-top",top,256,192);tap(a,190,175);assert(!a->page&&!a->doc.count&&a->doc.layer[0].pixels[10*128+10]==0xffff0000);
    free(a);free(bottom);free(top);free(d);puts("Hue/saturation/brightness: selection/current/visible scope, grouped undo, locks, UI and cancellation: PASS");
}
static void color_checks(const char *dir){
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);
    for(int mode=0;mode<2;mode++){
        int w=mode?320:256,h=mode?240:192;app_init(a,w,h,"","TEST");a->color=0xff2060a0;
        tap(a,w*5/12,10);assert(a->page==2&&a->color_tab==1&&!a->palette_edit);
        tap(a,w/2,35);assert(a->color_tab==0&&a->palette_picker_active);
        int square=h-114;if(square>104)square=104;int top=68;
        app_input(a,true,6,top,0,0);assert(a->color==0xffffffff);
        app_input(a,true,6+square-1,top,0,0);app_input(a,true,6,top+square-1,0,0);assert((a->color&0xffffff)==0);
        app_input(a,false,0,0,0,0);
        tap(a,6+square-2,top+square/2);uint32_t picked=a->color;
        tap(a,6+square+10,top+square-1);assert((a->color>>16)>=((a->color>>8)&255)&&(a->color>>16)>=(a->color&255));
        tap(a,20,top+square+8);assert((a->color>>24)<255);
        uint32_t changed=a->color;tap(a,w/4,h-17);assert(a->color==0xff2060a0&&a->color_tab==1);
        tap(a,w/2,35);tap(a,6+square-2,top+square/2);tap(a,3*w/4,h-17);assert(a->color!=0xff2060a0&&a->color_tab==1);
        tap(a,w*5/12,10);assert(!a->page&&!a->doc.count);
        tap(a,w*5/12,10);tap(a,w/2,35);tap(a,w*5/12,10);assert(!a->page&&!a->doc.count);
        a->color=0xff2060a0;a->page=2;a->color_tab=0;app_render(a,b,t,mode?400:256,h);ppm(dir,mode?"3ds-picker":"nds-picker",b,w,h);ppm(dir,mode?"3ds-picker-top":"nds-picker-top",t,mode?400:256,h);
        a->color_tab=1;a->palette_edit=false;app_render(a,b,t,mode?400:256,h);ppm(dir,mode?"3ds-palette":"nds-palette",b,w,h);ppm(dir,mode?"3ds-palette-top":"nds-palette-top",t,mode?400:256,h);
        a->palette_edit=true;a->color_tab=1;app_render(a,b,t,mode?400:256,h);ppm(dir,mode?"3ds-palette-edit":"nds-palette-edit",b,w,h);a->palette_edit=false;
        (void)picked;(void)changed;
        tap(a,w*5/12,10);assert(!a->page);
        app_input(a,true,100,90,0,0);app_input(a,false,0,0,0,0);assert(a->doc.count==1); /* next gesture draws */
        a->page=8;a->draft_handed=1;tap(a,20,135);assert(a->page==9);
        for(int tab=0;tab<4;tab++){tap(a,(2*tab+1)*w/8,44);assert(a->help_tab==(unsigned)tab);app_render(a,b,t,mode?400:256,h);char name[64];snprintf(name,sizeof(name),"%s-help-%d",mode?"3ds":"nds",tab);ppm(dir,name,b,w,h);}
        tap(a,w-20,h-16);assert(a->page==8&&a->draft_handed==1);
        a->page=2;a->color_tab=0;a->palette_scroll[0]=0;a->picker_update=false;a->recent_count=12;for(unsigned i=0;i<12;i++)a->recent[i]=0xff000000u|((i*21)<<16)|((255-i*20)<<8)|128;app_render(a,b,t,mode?400:256,h);
        uint32_t guard[320*24];for(int y=44;y<68;y++)memcpy(guard+(y-44)*w,b+y*w,w*4);
        a->palette_scroll[0]=5;app_render(a,b,t,mode?400:256,h);
        for(int y=44;y<68;y++)assert(!memcmp(guard+(y-44)*w,b+y*w,w*4));
        a->palette_scroll[0]=0;app_render(a,b,t,mode?400:256,h);
        int top_w=mode?400:256,outer=mode?16:12;uint32_t margin=t[25*top_w];
        for(int y=20;y<h-17;y++){for(int x=0;x<outer-1;x++)assert(t[y*top_w+x]==margin);for(int x=top_w-outer+1;x<top_w;x++)assert(t[y*top_w+x]==margin);}
        ppm(dir,mode?"3ds-picker-recents":"nds-picker-recents",b,w,h);
        a->draft_bindings=a->bindings;for(int page=1;page<=17;page++){a->page=page;a->confirm=0;app_render(a,b,t,mode?400:256,h);char name[64];snprintf(name,sizeof(name),"%s-dark-page%d",mode?"3ds":"nds",page);ppm(dir,name,b,w,h);}
        a->confirm=2;snprintf(a->notice,sizeof(a->notice),"New clears all four layers");app_render(a,b,t,mode?400:256,h);ppm(dir,mode?"3ds-dark-confirm":"nds-dark-confirm",b,w,h);
    }
    free(a);free(b);free(t);puts("HSV corners, captured drag, opacity, immediate color/close and gesture isolation: PASS");
}
static void palette_checks(const char *dir){
    PdApp *a=calloc(1,sizeof(*a)),*r=calloc(1,sizeof(*r));assert(a&&r);app_init(a,256,192,dir,"TEST");assert(a->swatch_count>=8);
    tap(a,106,10);assert(a->page==2&&a->color_tab==1);
    a->swatch_count=40;for(unsigned i=0;i<a->swatch_count;i++)a->swatches[i]=0xff000000u|(i*0x000f0f0fu);a->swatch_selected=0;a->palette_scroll[1]=0;
    tap(a,12,76);assert(a->swatch_selected==0&&a->color==a->swatches[0]);
    tap(a,60,56);assert(a->palette_target==1);uint32_t first_bg=a->swatches[1];tap(a,42,76);assert(a->background==first_bg);
    uint32_t fg=a->color,bg=a->background;tap(a,100,56);assert(a->color==bg&&a->background==fg);
    tap(a,130,56);assert(a->palette_density==0);tap(a,170,56);assert(a->palette_density==1);tap(a,220,56);assert(a->palette_density==2);
    tap(a,2*a->width/3+20,35);assert(a->palette_edit);tap(a,20,56);unsigned count=a->swatch_count;a->color=0xffabcdef;tap(a,30,92);assert(a->swatch_count==count+1);
    tap(a,a->width/2,92);assert(a->swatches[a->swatch_selected]==0xffabcdef);
    tap(a,4*a->width/6,120);assert(a->swatch_count==count+1);
    tap(a,a->width/2,120);assert(a->swatch_count==count+1);
    tap(a,a->width-20,176);assert(!a->palette_edit);
    a->swatch_count=256;for(unsigned i=0;i<256;i++)a->swatches[i]=0xff000000u|i;a->palette_scroll[1]=0;a->swatch_selected=0;a->color_tab=1;a->now_ms=3000;app_input(a,true,20,150,0,0);a->now_ms=3100;app_input(a,true,20,80,0,0);assert(a->swatch_scrolling&&a->palette_scroll[1]>0);app_input(a,false,0,0,0,0);
    a->palette_scroll[1]=0;a->now_ms=4000;app_input(a,true,20,140,0,0);a->now_ms=4150;app_input(a,true,20,160,0,0);assert(a->swatch_drag);app_input(a,false,0,0,0,0);
    a->color_tab=0;a->palette_scroll[0]=0;a->now_ms=5000;app_input(a,true,120,150,0,0);a->now_ms=5100;app_input(a,true,120,80,0,0);assert(a->swatch_scrolling&&a->palette_scroll[0]>0);app_input(a,false,0,0,0,0);
    free(a);free(r);puts("Square palette density/targets/edit, scrolling versus 150ms reorder and picker mini scrolling: PASS");
}
static void palette_file_checks(const char *dir){
    uint32_t colors[256],read[256];for(unsigned i=0;i<256;i++)colors[i]=(i<<24)|((255-i)<<16)|(i<<8)|i;unsigned count=0;char path[512],error[64];
    uint32_t c;assert(pd_color_parse("12345680",&c)&&c==0x80123456);assert(pd_color_parse("#abcdef",&c)&&c==0xffabcdef);assert(!pd_color_parse("xyzabc",&c));
    for(unsigned kind=1;kind<=2;kind++){snprintf(path,sizeof(path),"%s/palette.%s",dir,kind==1?"hex":"png");assert(pd_palette_write(path,colors,256,kind,false,error));assert(pd_palette_read(path,read,&count,error)&&count==256&&!memcmp(colors,read,sizeof(colors)));assert(!pd_palette_write(path,colors,256,kind,false,error));}
    snprintf(path,sizeof(path),"%s/palette.gpl",dir);assert(!pd_palette_write(path,colors,256,0,false,error));for(unsigned i=0;i<256;i++)colors[i]|=0xff000000u;assert(pd_palette_write(path,colors,256,0,false,error));assert(pd_palette_read(path,read,&count,error)&&count==256&&!memcmp(colors,read,sizeof(colors)));
    snprintf(path,sizeof(path),"%s/bad.hex",dir);FILE *f=fopen(path,"wb");assert(f);fputs("ABCDEF\nBAD\n",f);fclose(f);assert(!pd_palette_read(path,read,&count,error)&&count==256&&!memcmp(colors,read,sizeof(colors)));
    PdDocument *d=calloc(1,sizeof(*d)),*r=calloc(1,sizeof(*r));assert(d&&r);pd_init(d);d->palette_count=3;d->palette[0]=0x80123456;d->palette[1]=0xffabcdef;d->palette[2]=0;assert(pd_dirty(d));
    snprintf(path,sizeof(path),"%s/project-palette.pdd",dir);assert(pd_write_file(d,path,0,false,error)&&!pd_dirty(d));assert(pd_open_file(r,path,error)&&r->palette_count==3&&!memcmp(d->palette,r->palette,12));
    snprintf(path,sizeof(path),"%s/project-palette.aseprite",dir);assert(pd_write_file(d,path,1,false,error));assert(pd_open_file(r,path,error)&&r->palette_count==3&&!memcmp(d->palette,r->palette,12));
    d->palette[0]++;assert(pd_dirty(d));free(d);free(r);
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);
    for(unsigned mode=0;mode<2;mode++){
        int w=mode?320:256,h=mode?240:192;app_init(a,w,h,"","TEST");tap(a,w*5/12,10);tap(a,w/2,35);assert(a->page==2&&a->color_tab==0);
        tap(a,150,56);assert(a->page==5&&a->name_action==5&&a->text_mode==3);app_render(a,b,t,mode?400:256,h);ppm(dir,mode?"3ds-hex-osk":"nds-hex-osk",b,w,h);
        snprintf(a->filename,sizeof(a->filename),"12ABCD");app_text_result(a,"12ABCD",true);app_input(a,false,0,0,0,0);assert(a->page==2&&(a->color&0xffffff)==0x12abcd);
        tap(a,60,56);assert(a->palette_target==1);tap(a,150,56);assert(a->page==5&&a->name_action==5);app_text_result(a,"ABCDEF",true);app_input(a,false,0,0,0,0);assert((a->background&0xffffff)==0xabcdef);
        int side=mode?132:106,tile=mode?27:22;tap(a,side+4,72);assert((a->background&0xffffff)==(a->swatches[0]&0xffffff));
        tap(a,100,56);uint32_t fg=a->color,bg=a->background;tap(a,100,56);assert(a->color==bg&&a->background==fg);
    }
    free(a);free(b);free(t);
    puts("Palette HEX/GPL/PNG, alpha, transactional rejection, PDD5/ASE project palettes and hex-OSK/swap UI: PASS");
}
static void browser_checks(const char *dir){
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);app_init(a,256,192,dir,"DS NTR");
    app_input(a,false,0,0,PD_MENU,0);tap(a,40,55);assert(a->page==4&&a->file_mode==1);
    tap(a,220,175);assert(a->page==5);tap(a,28,103);assert(!strcmp(a->filename,"A"));tap(a,220,175);assert(a->page==0&&a->project_native);
    char path[512];assert(pd_join(path,dir,"A.pdd"));assert(pd_exists(path));assert(!a->doc.count);
    dot(&a->doc,0,10,10,0xff123456);app_input(a,false,0,0,PD_SAVE,0);assert(!pd_dirty(&a->doc));
    app_input(a,false,0,0,PD_MENU,0);tap(a,40,55);tap(a,220,175);tap(a,28,103);tap(a,220,175);assert(a->confirm==5);
    tap(a,200,175);assert(!a->confirm&&a->page==5);assert(a->doc.layer[0].pixels[1290]==0xff123456);
    a->page=4;a->file_mode=0;assert(pd_list(dir,&a->listing,a->notice));a->selected=-1;
    for(unsigned i=0;i<a->listing.count;i++)if(!strcmp(a->listing.entries[i].name,"A.pdd"))a->selected=(int)i;
    assert(a->selected>=0);dot(&a->doc,0,11,11,0xff456789);tap(a,220,175);assert(a->confirm==4);tap(a,200,175);assert(a->doc.layer[0].pixels[1419]==0xff456789);
    tap(a,220,175);tap(a,20,175);assert(a->page==0);assert(a->doc.layer[0].pixels[1419]==0);assert(a->doc.layer[0].pixels[1290]==0xff123456);
    for(int mode=0;mode<2;mode++){a->width=mode?320:256;a->height=mode?240:192;
        for(int page=3;page<=7;page++){a->page=page;a->confirm=0;a->partial=false;app_render(a,b,t,mode?400:256,a->height);char name[64];snprintf(name,sizeof(name),"%s-files-page%d",mode?"3ds":"nds",page);ppm(dir,name,b,a->width,a->height);}
    }
    a->width=256;a->height=192;a->page=5;a->name_action=2;strcpy(a->filename,"AB");a->text_cursor=2;tap(a,200,145);assert(a->text_cursor==1);tap(a,20,145);tap(a,16,75);assert(!strcmp(a->filename,"AqB"));
    a->now_ms=1000;app_input(a,true,20,180,0,0);assert(!strcmp(a->filename,"AB"));a->now_ms=1399;app_input(a,true,20,180,0,0);assert(!strcmp(a->filename,"AB"));a->now_ms=1400;app_input(a,true,20,180,0,0);assert(!strcmp(a->filename,"B"));app_input(a,false,0,0,0,0);
    app_text_result(a,"Renamed Layer",true);assert(a->page==1&&!strcmp(a->doc.names[a->doc.active],"Renamed Layer"));a->page=5;a->name_action=0;strcpy(a->filename,"safe");app_text_result(a,"../bad",true);assert(a->page==5);app_text_result(a,"",false);assert(a->page==4);
    app_input(a,false,0,0,0,0);a->page=4;a->selected=-1;assert(pd_list(dir,&a->listing,a->notice));for(unsigned i=0;i<a->listing.count;i++)if(!strcmp(a->listing.entries[i].name,"A.pdd"))a->selected=(int)i;
    tap(a,240,32);assert(a->page==16);tap(a,30,80);assert(a->page==5&&a->name_action==4);app_text_result(a,"A-copy",true);assert(a->page==4);char copy[512],renamed[512],error[64];assert(pd_join(copy,dir,"A-copy.pdd")&&pd_exists(copy));assert(pd_join(renamed,dir,"A-renamed.pdd"));assert(!pd_file_manage(copy,path,0,error));assert(pd_file_manage(copy,renamed,0,error)&&!pd_exists(copy)&&pd_exists(renamed));assert(!pd_file_manage(dir,copy,2,error));
    app_input(a,false,0,0,0,0);snprintf(a->file_source,sizeof(a->file_source),"%s",renamed);a->page=16;tap(a,30,115);assert(a->confirm==7);tap(a,220,180);assert(!a->confirm&&pd_exists(renamed));tap(a,30,115);tap(a,20,180);assert(!pd_exists(renamed)&&a->page==4);
    assert(a->recent_project_count&&strstr(a->recent_projects[0],"A.pdd"));
    PdApp *reloaded=calloc(1,sizeof(*reloaded));assert(reloaded);app_init(reloaded,256,192,dir,"TEST");assert(reloaded->recent_project_count==a->recent_project_count&&!strcmp(reloaded->recent_projects[0],a->recent_projects[0]));free(reloaded);
    a->page=4;a->file_mode=0;tap(a,180,32);assert(a->page==17);uint64_t rev=a->doc.revision;tap(a,30,36);assert(a->preview_ready&&a->doc.revision==rev);tap(a,20,180);assert(a->confirm==4);tap(a,220,180);assert(!a->confirm&&a->doc.revision==rev&&a->page==17);
    for(int mode=0;mode<2;mode++){int w=mode?320:256,h=mode?240:192;a->width=w;a->height=h;app_render(a,b,t,mode?400:256,h);ppm(dir,mode?"3ds-recent-projects":"nds-recent-projects",b,w,h);ppm(dir,mode?"3ds-file-preview":"nds-file-preview",t,mode?400:256,h);}
    free(a);free(b);free(t);puts("Browser save-as/keyboard/save/overwrite-cancel and native UI fixtures: PASS");
}
static void brush_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d));assert(d);pd_init(d);
    for(unsigned size=1;size<=16;size++)for(unsigned round=0;round<2;round++){pd_init(d);assert(pd_begin(d));pd_brush_line(d,64,64,64,64,0xffff0000,size,round!=0);assert(pd_commit(d));unsigned count=0;for(unsigned i=0;i<PD_PIXELS;i++)if(d->layer[0].pixels[i])count++;assert(count>0&&count<=size*size);if(!round)assert(count==size*size);assert(pd_undo(d)&&!d->layer[0].pixels[64*128+64]);assert(pd_redo(d));}
    pd_init(d);d->width=16;d->height=32;assert(pd_begin(d));pd_brush_line(d,0,0,15,31,0xff123456,16,true);assert(pd_commit(d));for(unsigned y=0;y<128;y++)for(unsigned x=0;x<128;x++)if(x>=16||y>=32)assert(!d->layer[0].pixels[y*128+x]);
    /* Wrapped footprints and exact mirrored even tips, including undo. */
    pd_init(d);d->width=d->height=16;assert(pd_begin(d));pd_guided_line(d,0,0,0,0,0xff123456,2,false,15);assert(pd_commit(d));assert(d->layer[0].pixels[15*128+15]==0xff123456);assert(pd_undo(d)&&!d->layer[0].pixels[15*128+15]);assert(pd_redo(d));
    pd_init(d);d->width=d->height=16;assert(pd_begin(d));pd_guided_line(d,15,4,17,4,0xffffffff,1,false,4);assert(pd_commit(d));assert(d->layer[0].pixels[4*128+15]&&d->layer[0].pixels[4*128]&&d->layer[0].pixels[4*128+1]&&!d->layer[0].pixels[4*128+8]);
    pd_init(d);d->width=16;d->height=32;assert(pd_begin(d));pd_guided_line(d,3,4,5,8,0xffabcdef,4,true,3);assert(pd_commit(d));for(unsigned y=0;y<32;y++)for(unsigned x=0;x<16;x++){uint32_t c=d->layer[0].pixels[y*128+x];assert(c==d->layer[0].pixels[y*128+15-x]&&c==d->layer[0].pixels[(31-y)*128+x]);}
    PdApp *a=calloc(1,sizeof(*a)),*r=calloc(1,sizeof(*r));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&r&&b&&t);char root[PD_PATH];assert(pd_join(root,dir,"brush-prefs")&&pd_mkdir(root));app_init(a,256,192,root,"TEST");tap(a,192,12);assert(a->toolbox&&!a->page);tap(a,30,35);assert(!a->page&&a->toolbox&&a->tool_options_open&&a->tool==0);tap(a,230,90);tap(a,120,120);tap(a,120,145);app_input(a,false,0,0,PD_TOOLS,0);assert(!a->toolbox);app_input(a,false,0,0,PD_TOOLS,0);tap(a,80,170);assert(a->tool_panel==8&&a->tool_options_open);tap(a,120,50);assert(a->brush_size[0]==2&&a->brush_round[0]&&a->pixel_grid&&a->pixel_perfect);app_init(r,256,192,root,"TEST");assert(r->brush_size[0]==2&&r->brush_round[0]&&r->pixel_grid&&r->pixel_perfect);
    app_init(a,256,192,root,"TEST");tap(a,192,12);tap(a,30,35);app_render(a,b,t,256,192);ppm(dir,"nds-tool-options",b,256,192);a->width=320;a->height=240;app_render(a,b,t,400,240);ppm(dir,"3ds-tool-options",b,320,240);a->width=256;a->height=192;
    a->brush_size[0]=1;app_input(a,true,150,90,0,0);app_input(a,true,220,90,0,0);app_input(a,true,150,90,0,0);app_input(a,false,0,0,0,0);assert(a->brush_size[0]==3);tap(a,250,100);assert(!a->toolbox&&!a->tool_options_open);tap(a,192,12);tap(a,30,35);
    tap(a,80,170);tap(a,120,72);tap(a,120,116);assert(a->guides==5);app_init(r,256,192,root,"TEST");assert(r->guides==5);
    for(int m=0;m<2;m++){a->width=m?320:256;a->height=m?240:192;a->page=21;app_render(a,b,t,m?400:256,a->height);ppm(dir,m?"3ds-brush-options":"nds-brush-options",b,a->width,a->height);a->page=22;app_render(a,b,t,m?400:256,a->height);ppm(dir,m?"3ds-guides":"nds-guides",b,a->width,a->height);a->page=0;a->toolbox=false;a->tool_options_open=false;a->pixel_grid=true;a->zoom=4;a->guides=0;a->ox=a->oy=a->subx=a->suby=0;pd_init(&a->doc);a->doc.width=a->doc.height=16;dot(&a->doc,0,3,3,0xff123456);app_render(a,b,t,m?400:256,a->height);ppm(dir,m?"3ds-pixel-grid":"nds-pixel-grid",b,a->width,a->height);int left=(a->width-64)/2,upper=24+(a->height-24-64)/2;assert(b[(upper+12)*a->width+left+12]!=0x123456);assert(b[(upper+13)*a->width+left+13]==0x123456);assert(a->doc.layer[0].pixels[3*128+3]==0xff123456);}
    /* Draw directly on a repeated copy and across its seam: no chord through the middle. */
    app_init(a,256,192,"","TEST");a->doc.width=a->doc.height=16;a->zoom=4;a->guides=4;a->color=0xffaabbcc;
    app_input(a,true,156,76,0,0);app_render(a,b,t,256,192);app_input(a,true,164,76,0,0);app_render(a,b,t,256,192);app_input(a,false,0,0,0,0);
    assert(a->doc.layer[0].pixels[15]&&a->doc.layer[0].pixels[0]&&a->doc.layer[0].pixels[1]&&!a->doc.layer[0].pixels[8]);assert(pd_undo(&a->doc)&&!a->doc.layer[0].pixels[0]);
    for(unsigned guides=4;guides<=12;guides+=4){a->guides=guides;app_render(a,b,t,256,192);for(int y=24;y<192;y++)for(int x=0;x<256;x++){int dx=x-96,dy=y-76,wx=dx>=0?dx/4:-((-dx+3)/4),wy=dy>=0?dy/4:-((-dy+3)/4);uint32_t expected=0x53616b;if(((guides&4)||(wx>=0&&wx<16))&&((guides&8)||(wy>=0&&wy<16))){wx=(wx%16+16)%16;wy=(wy%16+16)%16;expected=((wx/8+wy/8)&1)?0xcdcdcd:0xeeeeee;}if(x>=96&&x<=159&&y>=76&&y<=139&&(x==96||x==159||y==76||y==139))expected=0xffbf47;assert(b[y*256+x]==expected);}}
    free(d);free(a);free(r);free(b);free(t);puts("Brush sizes/tips, clipped edges, undo, tool persistence and display-only grid: PASS");
}
static void fill_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d));assert(d);pd_init(d);d->width=d->height=16;
    for(unsigned y=0;y<16;y++)d->layer[0].pixels[y*128+8]=0xff000000;
    assert(pd_fill(d,0,0,0xffff0000,0));assert(d->count==1&&d->layer[0].pixels[7]==0xffff0000&&d->layer[0].pixels[9]==0);assert(pd_undo(d)&&!d->layer[0].pixels[0]);assert(pd_redo(d));assert(!pd_fill(d,0,0,0xffff0000,0)&&d->count==1);
    assert(pd_undo(d));assert(pd_fill(d,0,0,0xff00ff00,4));assert(d->layer[0].pixels[15]==0xff00ff00&&d->layer[0].pixels[8]==0xff000000);assert(pd_undo(d));assert(pd_fill(d,0,0,0xff0000ff,1));assert(d->layer[0].pixels[15]==0xff0000ff&&d->layer[0].pixels[8]==0xff000000);
    pd_init(d);d->width=d->height=2;d->layer[0].pixels[1]=d->layer[0].pixels[128]=0xffffffff;assert(pd_fill(d,0,0,0xff123456,0));assert(!d->layer[0].pixels[129]);
    pd_init(d);d->width=1;d->height=128;assert(pd_fill(d,0,0,0xffabcdef,12));for(unsigned y=0;y<128;y++){assert(d->layer[0].pixels[y*128]==0xffabcdef);assert(!d->layer[0].pixels[y*128+1]);}
    pd_init(d);assert(pd_fill(d,0,0,0x80123456,15));for(unsigned i=0;i<PD_PIXELS;i++)assert(d->layer[0].pixels[i]==0x80123456);assert(pd_undo(d)&&!d->layer[0].pixels[PD_PIXELS-1]);assert(pd_redo(d));
    d->layer[0].flags=PD_LAYER_ALPHA_LOCK;assert(pd_fill(d,0,0,0xffff0000,0));assert(d->layer[0].pixels[0]==0x80ff0000);assert(!pd_fill(d,0,0,0,0));d->layer[0].flags=PD_LAYER_LOCK;assert(!pd_fill(d,0,0,0xffffffff,0));d->layer[0].flags=0;d->layer[0].visible=0;assert(!pd_fill(d,0,0,0xffffffff,0));
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);app_init(a,256,192,"","TEST");a->doc.width=a->doc.height=16;a->zoom=4;
    tap(a,192,12);tap(a,46,85);assert(a->tool==3&&a->toolbox&&a->tool_options_open&&!a->page);tap(a,120,50);tap(a,120,75);tap(a,230,102);tap(a,120,172);assert(!a->tool_options_open&&a->fill_global&&a->fill_visible);tap(a,192,12);assert(!a->toolbox);app_input(a,true,110,90,0,0);assert(a->doc.count==1);app_input(a,true,130,100,0,0);assert(a->doc.count==1);app_input(a,false,0,0,0,0);
    a->guides=4;app_pan(a,-100,-100);assert(a->ox<0&&a->oy==0);app_pan(a,200,200);assert(a->ox>16&&a->oy==0);a->page=22;tap(a,20,120);assert(!a->guides&&a->ox==0);a->page=0;a->guides=12;app_pan(a,-100,-100);assert(a->ox<0&&a->oy<0);
    a->ox=8;a->oy=8;a->zoom=16;for(int m=0;m<2;m++){a->width=m?320:256;a->height=m?240:192;app_render(a,b,t,m?400:256,a->height);ppm(dir,m?"3ds-tile-pan":"nds-tile-pan",b,a->width,a->height);a->toolbox=true;app_render(a,b,t,m?400:256,a->height);ppm(dir,m?"3ds-fill-tools":"nds-fill-tools",b,a->width,a->height);a->toolbox=false;}
    free(d);free(a);free(b);free(t);puts("Fill connectivity/wrapping/mirrored seeds/alpha/lock/undo/bounds and tile panning: PASS");
}
static bool mask_at(const uint32_t *mask,int x,int y){return (mask[(y*128+x)/32]&(1u<<((y*128+x)%32)))!=0;}
static void fill_options_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d));assert(d);pd_init(d);d->width=5;d->height=1;uint32_t initial[5]={0xff101010,0xff181010,0xffffffff,0xff101010,0xff191010};memcpy(d->layer[0].pixels,initial,sizeof(initial));
    assert(pd_fill_options(d,0,0,0xffff0000,0,NULL,false,7,false));assert(d->layer[0].pixels[0]==0xffff0000&&d->layer[0].pixels[1]==initial[1]&&d->layer[0].pixels[3]==initial[3]);assert(pd_undo(d));
    assert(pd_fill_options(d,0,0,0xffff0000,0,NULL,false,8,false));assert(d->layer[0].pixels[1]==0xffff0000&&d->layer[0].pixels[3]==initial[3]);assert(pd_undo(d));
    assert(pd_fill_options(d,0,0,0xffff0000,0,NULL,true,8,false));assert(d->layer[0].pixels[1]==0xffff0000&&d->layer[0].pixels[3]==0xffff0000&&d->layer[0].pixels[4]==initial[4]);assert(pd_undo(d));uint32_t mask[PD_PIXELS/32]={1};assert(pd_fill_options(d,0,0,0xff00ff00,0,mask,true,255,false));assert(d->layer[0].pixels[1]==initial[1]);assert(pd_undo(d));assert(!pd_fill_options(d,0,0,0xff00ff00,0,NULL,true,256,false));
    pd_init(d);d->width=d->height=8;d->enabled=3;for(unsigned y=0;y<8;y++)d->layer[1].pixels[y*128+4]=0xff000000;assert(pd_fill_options(d,0,0,0xffff0000,0,NULL,false,0,true));assert(d->layer[0].pixels[3]==0xffff0000&&!d->layer[0].pixels[4]&&!d->layer[0].pixels[5]&&d->layer[1].pixels[4]==0xff000000);assert(pd_undo(d));assert(pd_fill_options(d,0,0,0xffff0000,0,NULL,true,0,true));assert(d->layer[0].pixels[5]==0xffff0000&&!d->layer[0].pixels[4]);assert(pd_undo(d));assert(pd_fill_options(d,0,0,0xffff0000,0,NULL,false,0,false));assert(d->layer[0].pixels[4]==0xffff0000);assert(pd_undo(d));
    pd_init(d);d->width=2;d->height=1;d->layer[0].pixels[1]=0x00123456;assert(pd_fill_options(d,0,0,0xff010203,0,NULL,true,0,false)&&d->layer[0].pixels[1]==0xff010203);
    for(unsigned pattern=1;pattern<4;pattern++){pd_init(d);d->width=d->height=6;for(unsigned y=0;y<6;y++)d->layer[0].pixels[y*128+3]=0xff000000;d->layer[0].pixels[1*128+1]=0xff00ff00;assert(pd_fill_pattern(d,0,0,0xffff0000,0,NULL,false,0,false,pattern));for(unsigned y=0;y<6;y++)for(unsigned x=0;x<6;x++){uint32_t v=d->layer[0].pixels[y*128+x];if(x==3)assert(v==0xff000000);else if(x==1&&y==1)assert(v==0xff00ff00);else if(x>3)assert(!v);else assert(v==(pd_dither_gap(x,y,pattern)?0u:0xffff0000u));}unsigned painted=0;for(unsigned y=0;y<4;y++)for(unsigned x=0;x<2;x++)painted+=!pd_dither_gap(x,y,pattern);assert(painted==pattern*2);assert(pd_undo(d)&&!d->layer[0].pixels[0]);}
    pd_init(d);d->width=d->height=4;assert(!pd_fill_pattern(d,0,0,0xffff0000,0,NULL,false,0,false,4)&&!d->count);
    PdApp *a=calloc(1,sizeof(*a)),*r=calloc(1,sizeof(*r));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&r&&b&&t);char root[PD_PATH];assert(pd_join(root,dir,"fill-options")&&pd_mkdir(root));app_init(a,256,192,root,"TEST");tap(a,192,12);tap(a,46,85);assert(a->toolbox&&a->tool_options_open&&!a->page&&a->tool==3);tap(a,120,50);tap(a,120,75);tap(a,230,102);assert(a->fill_global&&a->fill_visible&&a->fill_tolerance==1);app_input(a,true,110,122,0,0);app_input(a,true,240,122,0,0);app_input(a,false,0,0,0,0);assert(a->fill_tolerance==255);tap(a,120,150);tap(a,120,150);assert(a->fill_pattern==2&&a->tool_options_open);for(int m=0;m<2;m++){a->width=m?320:256;a->height=m?240:192;app_render(a,b,t,m?400:256,a->height);ppm(dir,m?"3ds-fill-dither":"nds-fill-dither",b,a->width,a->height);}a->width=256;a->height=192;tap(a,40,136);assert(a->tool_panel==6&&a->shape_pattern==0);tap(a,150,120);assert(a->shape_pattern==1);tap(a,40,85);assert(a->tool_panel==3);app_init(r,256,192,root,"TEST");assert(r->fill_global&&r->fill_visible&&r->fill_tolerance==255&&r->fill_pattern==2&&r->shape_pattern==1);{char legacy_root[PD_PATH],path[PD_PATH];assert(pd_join(legacy_root,dir,"tools-pdb4")&&pd_mkdir(legacy_root)&&pd_join(path,legacy_root,"tools0.bin"));unsigned char legacy[20]={'P','D','B','4',1,0,0,0,3,5,1,0,0,0,7,40};uint32_t crc=(uint32_t)crc32(0,legacy,16);for(int k=0;k<4;k++)legacy[16+k]=(unsigned char)(crc>>(8*k));FILE *f=fopen(path,"wb");assert(f&&fwrite(legacy,1,20,f)==20&&!fclose(f));app_init(r,256,192,legacy_root,"TEST");assert(r->brush_size[0]==3&&r->brush_size[1]==5&&r->pixel_perfect&&r->fill_global&&r->fill_visible&&r->fill_tolerance==40&&!r->fill_pattern&&!r->shape_pattern);}
    for(int m=0;m<2;m++){a->width=m?320:256;a->height=m?240:192;app_render(a,b,t,m?400:256,a->height);ppm(dir,m?"3ds-fill-options":"nds-fill-options",b,a->width,a->height);}free(a);free(r);free(b);free(t);free(d);puts("Fill options: global/contiguous, tolerance boundaries, visible snapshot, masks, alpha and persistence: PASS");
}
static void selection_alpha_checks(void){
    PdDocument *d=calloc(1,sizeof(*d));PdClipboard *clip=calloc(1,sizeof(*clip));uint32_t mask[PD_PIXELS/32]={0};assert(d&&clip);pd_init(d);pd_mask_rectangle(mask,0,0,3,3);
    d->layer[0].pixels[1*128+1]=0x80ff0000;d->layer[0].pixels[1*128+9]=0xff0000ff;d->layer[0].pixels[2*128+10]=0xff00ff00;
    assert(pd_copy_selection(d,mask,clip));assert(pd_begin(d));assert(pd_move_selection(d,mask,8,0,false));assert(d->layer[0].pixels[1*128+9]==0xff80007f);assert(d->layer[0].pixels[2*128+10]==0xff00ff00);assert(!d->layer[0].pixels[1*128+1]);assert(pd_commit(d));assert(pd_undo(d)&&d->layer[0].pixels[1*128+1]==0x80ff0000&&d->layer[0].pixels[1*128+9]==0xff0000ff);
    assert(pd_paste_selection(d,clip,8,0));assert(d->layer[0].pixels[1*128+9]==0xff80007f&&d->layer[0].pixels[2*128+10]==0xff00ff00);assert(pd_undo(d));d->layer[0].pixels[1*128+9]=0x800000ff;assert(pd_paste_selection(d,clip,8,0));assert(d->layer[0].pixels[1*128+9]==0xc0aa0055);assert(pd_undo(d));
    d->layer[0].flags=PD_LAYER_ALPHA_LOCK;assert(pd_paste_selection(d,clip,8,0));assert(d->layer[0].pixels[1*128+9]==0x8080007f&&d->layer[0].pixels[2*128+10]==0xff00ff00);
    PdApp *a=calloc(1,sizeof(*a));assert(a);app_init(a,256,192,"","TEST");tap(a,192,12);tap(a,40,102);assert(a->tool==4&&a->tool_options_open);tap(a,200,54);assert(a->selection_shape==1);tap(a,180,78);assert(a->selection_mode==1);tap(a,220,78);assert(a->selection_mode==2);tap(a,120,78);assert(a->selection_mode==0);a->selection_valid=true;a->selection_layer=a->doc.active;a->selection_revision=a->doc.revision;pd_mask_rectangle(a->selection,0,0,3,3);tap(a,120,102);assert(a->clipboard&&a->clipboard->width==4&&!a->doc.count);tap(a,200,128);assert(!a->selection_valid);
    free(a);free(d);free(clip);puts("Selection transparent move/paste, partial-alpha source-over, alpha lock and quick icons: PASS");
}
static void selection_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d));PdClipboard *clip=calloc(1,sizeof(*clip));uint32_t mask[PD_PIXELS/32]={0};assert(d&&clip);pd_init(d);d->width=d->height=16;
    const int16_t polygon[][2]={{1,1},{12,1},{12,6},{6,6},{6,12},{1,12}};pd_mask_polygon(mask,polygon,6,16,16);
    for(int y=0;y<16;y++)for(int x=0;x<16;x++){bool inside=false;for(int i=0,j=5;i<6;j=i++){double xi=polygon[i][0],yi=polygon[i][1],xj=polygon[j][0],yj=polygon[j][1];if((yi>y)!=(yj>y)&&x<(xj-xi)*(y-yi)/(yj-yi)+xi)inside=!inside;}assert(mask_at(mask,x,y)==inside);}
    memset(mask,0,sizeof(mask));pd_mask_rectangle(mask,4,4,2,2);int bx,by,bw,bh;assert(pd_mask_bounds(mask,16,16,&bx,&by,&bw,&bh)&&bx==2&&by==2&&bw==3&&bh==3);
    dot(d,0,2,2,0xff112233);dot(d,0,3,3,0x80445566);dot(d,0,5,5,0xffabcdef);assert(pd_copy_selection(d,mask,clip)&&clip->width==3&&clip->height==3);
    assert(pd_begin(d));assert(pd_move_selection(d,mask,1,1,false));assert(d->layer[0].pixels[3*128+3]==0xff112233&&d->layer[0].pixels[4*128+4]==0x80445566&&!d->layer[0].pixels[2*128+2]);assert(pd_move_selection(d,mask,2,2,false));assert(d->layer[0].pixels[4*128+4]==0xff112233);assert(pd_commit(d));assert(pd_undo(d)&&d->layer[0].pixels[2*128+2]==0xff112233);assert(pd_redo(d));
    assert(pd_begin(d));assert(!pd_move_selection(d,mask,-3,0,false));pd_cancel(d);assert(pd_paste_selection(d,clip,8,8));assert(d->layer[0].pixels[8*128+8]==0xff112233&&d->layer[0].pixels[9*128+9]==0x80445566);assert(pd_undo(d)&&!d->layer[0].pixels[8*128+8]);
    d->layer[0].flags=PD_LAYER_ALPHA_LOCK;assert(pd_begin(d));assert(!pd_move_selection(d,mask,0,0,true));pd_cancel(d);assert(!pd_paste_selection(d,clip,8,8));d->layer[0].flags=PD_LAYER_LOCK;assert(!pd_paste_selection(d,clip,8,8));d->layer[0].flags=0;
    pd_init(d);assert(pd_begin(d));pd_masked_line(d,0,3,15,3,0xff123456,4,false,3,NULL,mask);assert(pd_commit(d));for(int y=0;y<128;y++)for(int x=0;x<128;x++)if(d->layer[0].pixels[y*128+x])assert(mask_at(mask,x,y));pd_init(d);assert(pd_fill_mask(d,2,2,0xffabcdef,0,mask));assert(d->layer[0].pixels[2*128+2]&&!d->layer[0].pixels[1*128+1]);
    PdApp *a=calloc(1,sizeof(*a)),*r=calloc(1,sizeof(*r));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4),*rb=malloc(320*240*4),*rt=malloc(400*240*4);assert(a&&r&&b&&t&&rb&&rt);app_init(a,256,192,"","TEST");a->doc.width=a->doc.height=16;a->zoom=8;dot(&a->doc,0,3,3,0xff00ffff);app_render(a,b,t,256,192);
    tap(a,192,12);tap(a,40,102);assert(a->page==0&&a->toolbox&&a->tool_options_open&&a->tool==4);tap(a,192,12);assert(!a->toolbox);
    app_input(a,true,80,73,0,0);app_render(a,b,t,256,192);app_input(a,true,112,105,0,0);app_render(a,b,t,256,192);app_input(a,false,0,0,0,0);assert(a->selection_valid&&mask_at(a->selection,2,2)&&mask_at(a->selection,6,6));
    unsigned edits=a->doc.count;app_input(a,true,88,81,0,0);app_input(a,true,104,89,0,0);app_render(a,b,t,256,192);clone_app(r,a);r->selection_drag=false;pd_composite(&r->doc);app_render(r,rb,rt,256,192);assert(!memcmp(b,rb,256*192*4));app_input(a,false,0,0,0,0);assert(a->doc.count==edits+1&&a->doc.layer[0].pixels[4*128+5]==0xff00ffff);
    a->selection_mode=1;app_input(a,true,64,57,0,0);app_input(a,true,72,65,0,0);app_input(a,false,0,0,0,0);assert(mask_at(a->selection,0,0)&&mask_at(a->selection,5,4));a->selection_mode=2;tap(a,64,57);assert(!mask_at(a->selection,0,0)&&mask_at(a->selection,1,1));
    a->selection_mode=0;a->selection_shape=1;app_input(a,true,64,57,0,0);app_input(a,true,120,57,0,0);app_input(a,true,120,113,0,0);app_input(a,true,64,113,0,0);app_input(a,false,0,0,0,0);assert(mask_at(a->selection,2,2)&&!mask_at(a->selection,8,8));
    a->page=23;tap(a,20,115);assert((a->clipboard&&a->clipboard->width)&&a->page==0);a->page=23;tap(a,100,115);assert(!a->doc.layer[0].pixels[4*128+5]);a->page=23;tap(a,190,115);assert(a->doc.layer[0].pixels[4*128+5]==0xff00ffff);
    /* Pan cancels a live move without a new history entry. */
    unsigned before_cancel=a->doc.count;app_input(a,true,104,89,0,0);app_input(a,true,112,97,0,0);assert(a->selection_move&&a->doc.editing);app_input(a,true,112,97,0,PD_PAN);assert(!a->doc.editing&&!a->selection_drag&&a->doc.count==before_cancel&&a->doc.layer[0].pixels[4*128+5]==0xff00ffff);app_input(a,false,0,0,0,0);
    for(int m=0;m<2;m++){a->width=m?320:256;a->height=m?240:192;a->page=0;app_render(a,b,t,m?400:256,a->height);ppm(dir,m?"3ds-selection":"nds-selection",b,a->width,a->height);a->page=23;app_render(a,b,t,m?400:256,a->height);ppm(dir,m?"3ds-selection-options":"nds-selection-options",b,a->width,a->height);}
    app_init(a,256,192,"","TEST");assert(a->mask_words==PD_MAX_PIXELS/32&&!a->clipboard&&!a->gallery_thumbs);
    assert(app_set_profile(a,1)&&a->doc.profile==1&&a->mask_words==PD_PIXELS/32);tap(a,192,12);tap(a,40,102);assert(a->tool==4&&a->tool_options_open);
    a->selection_valid=true;a->selection_layer=a->doc.active;a->selection_revision=a->doc.revision;pd_mask_rectangle(a->selection,0,0,3,3);tap(a,120,102);assert(a->clipboard&&a->clipboard->width==4&&a->mask_words==PD_PIXELS/32);
    assert(pd_create(&a->doc,256,256,2));tap(a,250,180);assert(a->mask_words==PD_MAX_PIXELS/32&&!a->selection_valid);assert(app_set_profile(a,2)&&a->mask_words==PD_MAX_PIXELS/32);app_dispose(a);assert(!a->selection&&!a->clipboard);
    {char diag[PD_PATH],probe[PD_PATH];assert(pd_join(diag,dir,"diagnostics")&&(pd_exists(diag)||pd_mkdir(diag)));app_init(a,256,192,diag,"TEST");
    a->page=7;app_render(a,b,t,256,192);tap(a,128,180);assert(!strcmp(a->diag_status,"NO TIMER FOR CARD TEST")&&!a->free_kb&&!a->card_read_kbs);
    a->clock_ticks=fake_clock;a->clock_hz=1000000;a->memory_limit=1u<<20;tap(a,128,180);assert(a->memory_measured&&a->largest_kb>=1000&&a->free_kb>=a->largest_kb&&a->free_kb<=1024&&a->card_write_kbs==102400&&a->card_read_kbs==102400);
    assert(pd_join(probe,diag,".diag-speed.bin")&&!pd_exists(probe));app_render(a,b,t,256,192);ppm(dir,"nds-diagnostics",b,256,192);app_dispose(a);}
    free(d);free(clip);free(a);free(r);free(b);free(t);free(rb);free(rt);puts("Selection polygon/mask, overlap move previews, clipboard, protection, undo and UI add/subtract: PASS");
}
static void shape_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d));assert(d);uint32_t mask[PD_PIXELS/32]={0},out[PD_PIXELS/32]={0};
    for(unsigned kind=0;kind<3;kind++)for(unsigned fill=0;fill<2;fill++){pd_init(d);d->width=d->height=16;assert(pd_begin(d));pd_shape(d,2,3,12,11,kind,fill!=0,0xff123456,1,false,0,NULL,NULL);assert(pd_commit(d));if(kind==1)assert(d->layer[0].pixels[7*128+7]==(fill?0xff123456:0));if(kind==2){assert(d->layer[0].pixels[3*128+7]);assert(!d->layer[0].pixels[3*128+2]);for(unsigned y=3;y<=11;y++)for(unsigned x=2;x<=12;x++)assert(d->layer[0].pixels[y*128+x]==d->layer[0].pixels[(14-y)*128+14-x]);}assert(pd_undo(d)&&!d->layer[0].pixels[3*128+7]);assert(pd_redo(d));}
    pd_init(d);pd_mask_rectangle(mask,0,0,3,3);assert(pd_begin(d));pd_shape(d,0,0,10,10,1,true,0xffabcdef,16,true,3,NULL,mask);assert(pd_commit(d));for(unsigned y=0;y<128;y++)for(unsigned x=0;x<128;x++)if(d->layer[0].pixels[y*128+x])assert(mask_at(mask,(int)x,(int)y));
    pd_init(d);d->width=d->height=16;assert(pd_begin(d));pd_shape(d,15,1,17,4,1,true,0xffabcdef,1,false,4,NULL,NULL);assert(pd_commit(d)&&d->layer[0].pixels[1*128]&&d->layer[0].pixels[1*128+15]&&!d->layer[0].pixels[1*128+8]);
    for(unsigned kind=0;kind<3;kind++)for(unsigned fill=0;fill<2;fill++){uint32_t *solid=calloc(16*16,4);assert(solid);pd_init(d);d->width=d->height=16;assert(pd_begin(d));pd_shape(d,1,2,13,12,kind,fill!=0,0xff123456,2,false,0,NULL,NULL);assert(pd_commit(d));for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)solid[y*16+x]=d->layer[0].pixels[y*128+x];pd_init(d);d->width=d->height=16;d->layer[0].pixels[0]=0xff777777;assert(pd_begin(d));pd_pattern_shape(d,1,2,13,12,kind,fill!=0,0xff123456,2,false,0,NULL,NULL,2);assert(pd_commit(d));unsigned drawn=0;for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++){uint32_t v=d->layer[0].pixels[y*128+x],want=solid[y*16+x]&&!pd_dither_gap(x,y,2)?0xff123456u:(x||y?0u:0xff777777u);assert(v==want);drawn+=v==0xff123456u;}assert(drawn);free(solid);}
    pd_init(d);d->width=d->height=16;memset(mask,0,sizeof(mask));pd_mask_rectangle(mask,2,2,4,3);d->layer[0].pixels[2*128+2]=0x80ff0000;d->layer[0].pixels[3*128+4]=0xff0000ff;assert(pd_transform_selection(d,mask,0,out));assert(d->layer[0].pixels[2*128+4]==0x80ff0000&&d->layer[0].pixels[3*128+2]==0xff0000ff);assert(pd_undo(d));assert(pd_transform_selection(d,mask,1,out));assert(d->layer[0].pixels[3*128+2]==0x80ff0000);assert(pd_undo(d));assert(pd_transform_selection(d,mask,2,out));assert(d->layer[0].pixels[2*128+3]==0x80ff0000&&d->layer[0].pixels[4*128+2]==0xff0000ff);assert(mask_at(out,3,4)&&!mask_at(out,4,3));assert(pd_undo(d));d->layer[0].flags=PD_LAYER_ALPHA_LOCK;assert(!pd_transform_selection(d,mask,0,out));d->layer[0].flags=0;d->height=2;memset(mask,0,sizeof(mask));pd_mask_rectangle(mask,0,0,5,1);assert(!pd_transform_selection(d,mask,2,out));
    PdApp *a=calloc(1,sizeof(*a)),*r=calloc(1,sizeof(*r));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4),*rb=malloc(320*240*4),*rt=malloc(400*240*4);assert(a&&r&&b&&t&&rb&&rt);
    for(int mode=0;mode<2;mode++)for(unsigned kind=0;kind<3;kind++)for(unsigned guides=0;guides<=15;guides+=15){int w=mode?320:256,h=mode?240:192,tw=mode?400:256;app_init(a,w,h,"","TEST");a->doc.width=a->doc.height=32;a->zoom=4;a->tool=5+kind;a->guides=guides;a->shape_filled=kind==2;a->brush_size[0]=3;app_render(a,b,t,tw,h);
        int left=(w-128)/2,top=24+(h-24-128)/2;unsigned count=a->doc.count;for(int step=0;step<12;step++){int x=left+20+(step<6?step*8:(12-step)*8),y=top+20+step*3;app_input(a,true,x,y,0,0);assert(a->doc.editing&&a->doc.count==count);clone_app(r,a);r->partial=false;app_render(r,rb,rt,tw,h);app_render(a,b,t,tw,h);assert(!memcmp(b,rb,(size_t)w*h*4));}app_input(a,false,0,0,0,0);assert(a->doc.count==count+1);assert(pd_undo(&a->doc));assert(pd_redo(&a->doc));
        count=a->doc.count;app_input(a,true,left+10,top+10,0,0);app_input(a,true,left+30,top+30,0,0);app_input(a,true,left+30,top+30,0,PD_PAN);assert(!a->shape_drag&&!a->doc.editing&&a->doc.count==count);app_input(a,false,0,0,0,0);
    }
    for(int mode=0;mode<2;mode++){a->width=mode?320:256;a->height=mode?240:192;a->page=24;app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-shapes":"nds-shapes",b,a->width,a->height);a->page=25;app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-transforms":"nds-transforms",b,a->width,a->height);a->page=0;a->toolbox=true;app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-tools-06":"nds-tools-06",b,a->width,a->height);a->toolbox=false;}
    app_init(a,256,192,"","TEST");tap(a,192,12);tap(a,40,119);assert(a->tool_panel==5&&a->tool_options_open);app_input(a,true,150,90,0,0);app_input(a,true,220,90,0,0);app_input(a,false,0,0,0,0);assert(a->brush_size[0]>1);
    app_init(a,256,192,"","TEST");tap(a,192,12);tap(a,40,43);assert(a->tool_panel==1&&a->brush_edit==1);a->brush_size[0]=a->brush_size[1]=1;tap(a,40,136);assert(a->tool_panel==6);tap(a,150,90);assert(a->brush_size[0]==3&&a->brush_size[1]==1);app_input(a,true,150,90,0,0);app_input(a,true,226,90,0,0);app_input(a,false,0,0,0,0);assert(a->brush_size[0]==16&&a->brush_size[1]==1);for(int m=0;m<2;m++){a->width=m?320:256;a->height=m?240:192;a->tool_panel=6;a->tool=6;app_render(a,b,t,m?400:256,a->height);ppm(dir,m?"3ds-drawer-shape":"nds-drawer-shape",b,a->width,a->height);a->tool_panel=4;a->tool=4;app_render(a,b,t,m?400:256,a->height);ppm(dir,m?"3ds-drawer-select":"nds-drawer-select",b,a->width,a->height);int ix=(m?116:96)+16,iw=a->width-ix-16;for(int row=0;row<2;row++)for(int col=0;col<2;col++){int bx=col?ix+iw/2+2:ix,bw=col?iw-iw/2-2:iw/2-2,by=116+row*24;uint32_t fill=b[(by+2)*a->width+bx+2];for(int yy=by+2;yy<by+18;yy++)assert(b[yy*a->width+bx+bw-2]==fill);}}a->width=256;a->height=192;
    free(d);free(a);free(r);free(b);free(t);free(rb);free(rt);puts("Shapes: raster/ellipse symmetry/filled/clipping/tile/preview restoration/cancel; selection flips/rotation/alpha/undo: PASS");
}
static void perfect_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d));assert(d);PdPerfectStroke state={0};pd_init(d);d->layer[0].pixels[2*128+3]=0x80556677;assert(pd_begin(d));pd_perfect_line(d,&state,2,2,3,2,0xffff0000,0,NULL,NULL);pd_perfect_line(d,&state,3,2,3,3,0xffff0000,0,NULL,NULL);assert(d->layer[0].pixels[2*128+2]==0xffff0000&&d->layer[0].pixels[3*128+3]==0xffff0000&&d->layer[0].pixels[2*128+3]==0x80556677);assert(pd_commit(d));assert(pd_undo(d)&&!d->layer[0].pixels[2*128+2]&&d->layer[0].pixels[2*128+3]==0x80556677);assert(pd_redo(d));
    pd_init(d);d->width=d->height=16;memset(&state,0,sizeof(state));uint32_t damage[PD_PIXELS/32]={0};assert(pd_begin(d));pd_perfect_line(d,&state,15,2,16,2,0xffabcdef,6,damage,NULL);pd_perfect_line(d,&state,16,2,16,3,0xffabcdef,6,damage,NULL);assert(!d->layer[0].pixels[2*128]&&!d->layer[0].pixels[13*128]);assert(d->layer[0].pixels[3*128]&&d->layer[0].pixels[12*128]);assert(mask_at(damage,0,2)&&mask_at(damage,0,13));pd_cancel(d);
    pd_init(d);d->layer[0].flags=PD_LAYER_ALPHA_LOCK;d->layer[0].pixels[2*128+2]=d->layer[0].pixels[2*128+3]=d->layer[0].pixels[3*128+3]=0x800000ff;memset(&state,0,sizeof(state));assert(pd_begin(d));pd_perfect_line(d,&state,2,2,3,2,0x80ff0000,0,NULL,NULL);pd_perfect_line(d,&state,3,2,3,3,0x80ff0000,0,NULL,NULL);assert(d->layer[0].pixels[2*128+3]==0x800000ff&&d->layer[0].pixels[3*128+3]==0x8080007f);pd_cancel(d);
    PdApp *a=calloc(1,sizeof(*a)),*r=calloc(1,sizeof(*r));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4),*rb=malloc(320*240*4),*rt=malloc(400*240*4);assert(a&&r&&b&&t&&rb&&rt);char root[PD_PATH];assert(pd_join(root,dir,"perfect-prefs")&&pd_mkdir(root));app_init(a,256,192,root,"TEST");tap(a,192,12);tap(a,30,35);tap(a,120,145);assert(a->pixel_perfect);app_init(r,256,192,root,"TEST");assert(r->pixel_perfect);
    for(int mode=0;mode<2;mode++)for(unsigned guides=0;guides<=15;guides+=15){int w=mode?320:256,h=mode?240:192,tw=mode?400:256;app_init(a,w,h,"","TEST");a->pixel_perfect=true;a->zoom=8;a->guides=guides;app_render(a,b,t,tw,h);for(int step=0;step<24;step++){int wx=2+(step+1)/2,wy=2+step/2;app_input(a,true,wx*8+1,24+wy*8+1,0,0);clone_app(r,a);r->partial=false;app_render(r,rb,rt,tw,h);app_render(a,b,t,tw,h);assert(!memcmp(b,rb,(size_t)w*h*4));}app_input(a,false,0,0,0,0);assert(a->doc.count==1);assert(pd_undo(&a->doc));assert(pd_redo(&a->doc));}
    for(int mode=0;mode<2;mode++){a->width=mode?320:256;a->height=mode?240:192;a->page=21;a->brush_edit=0;app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-perfect-options":"nds-perfect-options",b,a->width,a->height);}
    free(a);free(r);free(b);free(t);free(rb);free(rt);free(d);puts("Pixel-perfect corner restoration, alpha lock, symmetry/wrapping damage, undo, persistence and partial render: PASS");
}
static void protection_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d)),*r=calloc(1,sizeof(*r));assert(d&&r);pd_init(d);dot(d,0,0,0,0x800000ff);
    assert(pd_begin(d));d->layer[0].flags=PD_LAYER_ALPHA_LOCK;assert(pd_commit(d));assert(pd_undo(d)&&!d->layer[0].flags);assert(pd_redo(d)&&d->layer[0].flags==2);
    assert(pd_begin(d));pd_line(d,0,0,1,0,0x80ff0000);assert(pd_commit(d));assert(d->layer[0].pixels[0]==0x8080007f&&!d->layer[0].pixels[1]);assert(pd_begin(d));pd_line(d,0,0,0,0,0);assert(!pd_commit(d)&&d->layer[0].pixels[0]==0x8080007f);
    assert(pd_begin(d));d->layer[0].flags=3;assert(pd_commit(d));assert(pd_begin(d));pd_line(d,0,0,0,0,0xffffffff);assert(!pd_commit(d));assert(!pd_delete_layer(d));assert(pd_add_layer(d));dot(d,1,0,0,0xff123456);assert(!pd_merge_down(d));
    char path[PD_PATH],error[64];assert(pd_join(path,dir,"protected.pdd")&&pd_write_file(d,path,0,false,error));assert(pd_open_file(r,path,error)&&r->layer[0].flags==3);assert(pd_join(path,dir,"protected-recovery.pdd")&&pd_write_recovery(d,path,error));assert(pd_open_file(r,path,error)&&r->layer[0].flags==3);
    d->layer[0].flags=PD_LAYER_LOCK;assert(pd_join(path,dir,"protected.aseprite")&&pd_write_file(d,path,1,false,error));assert(pd_open_file(r,path,error)&&r->layer[0].flags==PD_LAYER_LOCK);
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);for(int m=0;m<2;m++){app_init(a,m?320:256,m?240:192,"","TEST");a->page=1;tap(a,a->width-20,a->height-16);assert(!a->layer_delete);tap(a,a->width-12,40);assert(a->layer_popup);tap(a,50,125);assert(a->doc.layer[0].flags==1);tap(a,a->width-50,125);assert(a->doc.layer[0].flags==3);app_render(a,b,t,m?400:256,a->height);ppm(dir,m?"3ds-layer-protection":"nds-layer-protection",b,a->width,a->height);tap(a,a->width-28,37);assert(!a->layer_popup);tap(a,a->width-20,a->height-16);assert(!a->layer_delete);a->page=0;tap(a,a->width/2,a->height/2);assert(!a->doc.layer[0].pixels[64*128+64]);}
    free(a);free(b);free(t);free(d);free(r);puts("Layer/alpha protection: paint, erase, empty pixels, undo, delete/merge, native/recovery/ASE and UI: PASS");
}
static void eight_layer_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d)),*r=calloc(1,sizeof(*r));assert(d&&r);pd_init(d);
    for(unsigned i=0;i<PD_LAYERS;i++){if(i)assert(pd_add_layer(d));dot(d,i,7,9,0x40112233u+i*0x000b0805u);char name[64];snprintf(name,64,"Plane %u",i+1);assert(pd_rename_layer(d,name));d->layer[i].opacity=(uint8_t)(100+i*20);}
    assert(d->enabled==255&&!pd_add_layer(d)&&!pd_duplicate_layer(d));pd_composite(d);uint32_t expected=reference_pixel(d,9*128+7);assert(d->composite[9*128+7]==expected);
    assert(pd_move_layer(d,0));assert(pd_undo(d)&&d->order[7]==7);assert(pd_redo(d)&&d->order[0]==7);assert(pd_delete_layer(d)&&d->enabled==127);assert(pd_undo(d)&&d->enabled==255&&d->active==7);d->layer[6].blend=2;d->layer[5].visible=0;
    char path[PD_PATH],error[64];for(unsigned kind=0;kind<2;kind++){snprintf(path,sizeof(path),"%s/eight-layers.%s",dir,kind?"aseprite":"pdd");assert(pd_write_file(d,path,kind,false,error));assert(pd_open_file(r,path,error)&&r->enabled==255);pd_composite(r);pd_composite(d);assert(!memcmp(r->composite,d->composite,sizeof(d->composite)));}
    assert(pd_join(path,dir,"eight-recovery.pdd")&&pd_write_recovery(d,path,error)&&pd_open_file(r,path,error)&&r->enabled==255);
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*top=malloc(400*240*4);assert(a&&b&&top);
    for(int m=0;m<2;m++){app_init(a,m?320:256,m?240:192,"","TEST");for(unsigned i=1;i<8;i++)assert(pd_add_layer(&a->doc));a->page=1;uint8_t order[PD_LAYERS];memcpy(order,a->doc.order,PD_LAYERS);a->now_ms=1000;app_input(a,true,90,140,0,0);a->now_ms=1050;app_input(a,true,90,60,0,0);app_input(a,false,0,0,0,0);assert((m? a->layer_scroll==0:a->layer_scroll>0)&&!memcmp(order,a->doc.order,PD_LAYERS));
        app_render(a,b,top,m?400:256,a->height);ppm(dir,m?"3ds-eight-layers":"nds-eight-layers",b,a->width,a->height);
        a->now_ms=2000;app_input(a,true,90,70,0,0);a->now_ms=2150;app_input(a,true,90,70,0,0);assert(a->layer_drag);a->now_ms=2300;app_input(a,true,90,45,0,0);app_input(a,false,0,0,0,0);assert(a->doc.count&&a->layer_scroll>=0);
        a->layer_scroll=9999;app_render(a,b,top,m?400:256,a->height);assert(a->layer_scroll<=224);tap(a,34,a->height-45);assert(a->doc.active<8);
    }
    free(a);free(b);free(top);free(d);free(r);puts("Eight layers: full capacity, blend oracle, high-slot undo/reorder, files/recovery and scrolling: PASS");
}
static void dimension_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d)),*r=calloc(1,sizeof(*r));assert(d&&r);char path[PD_PATH],error[64];unsigned sizes[][2]={{1,1},{16,16},{32,32},{64,64},{16,32},{37,19},{128,1},{1,128}};
    for(unsigned k=0;k<8;k++){pd_init(d);d->width=sizes[k][0];d->height=sizes[k][1];dot(d,0,(int)d->width-1,(int)d->height-1,0xff123456);assert(pd_begin(d));pd_line(d,(int)d->width,0,(int)d->width,0,0xffffffff);assert(!pd_commit(d));
        for(unsigned kind=0;kind<3;kind++){snprintf(path,sizeof(path),"%s/size-%ux%u.%s",dir,d->width,d->height,kind==0?"pdd":kind==1?"aseprite":"png");assert(pd_write_file(d,path,kind,false,error));if(kind<2){assert(pd_open_file(r,path,error)&&r->width==d->width&&r->height==d->height);assert(!memcmp(r->layer,d->layer,sizeof(d->layer)));}}
        assert(!pd_save(d,dir));assert(pd_undo(d)&&!d->layer[0].pixels[(d->height-1)*128+d->width-1]);assert(pd_redo(d));
    }
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*top=malloc(400*240*4);assert(a&&b&&top);app_init(a,256,192,"","TEST");a->page=18;tap(a,30,35);assert(a->page==20);tap(a,20,40);assert(a->new_width==16&&a->new_height==16);tap(a,180,70);tap(a,210,100);tap(a,125,100);tap(a,220,160);assert(a->new_height==32);tap(a,40,180);tap(a,40,180);assert(a->confirm==2);tap(a,40,180);assert(a->doc.width==16&&a->doc.height==32);
    for(int m=0;m<2;m++){a->width=m?320:256;a->height=m?240:192;a->zoom=4;a->page=0;app_render(a,b,top,m?400:256,a->height);ppm(dir,m?"3ds-rectangle":"nds-rectangle",b,a->width,a->height);ppm(dir,m?"3ds-rectangle-top":"nds-rectangle-top",top,m?400:256,a->height);a->page=20;app_render(a,b,top,m?400:256,a->height);ppm(dir,m?"3ds-size-picker":"nds-size-picker",b,a->width,a->height);}
    free(a);free(b);free(top);free(d);free(r);puts("Variable dimensions: bounds, native/ASE roundtrip, presets/custom UI and undo: PASS");
}
static void tile_history_checks(void){
    PdDocument *d=calloc(1,sizeof(*d));assert(d);pd_init(d);
    for(unsigned i=0;i<40;i++)dot(d,0,(int)i,0,0xff000000u+i);
    assert(d->count==32&&d->history_used==32*260);for(int i=39;i>=8;i--){assert(pd_undo(d));assert(d->layer[0].pixels[i]==0);}assert(!pd_undo(d));
    for(unsigned i=8;i<40;i++){assert(pd_redo(d));assert(d->layer[0].pixels[i]==0xff000000u+i);}
    for(unsigned i=0;i<10;i++)assert(pd_undo(d));dot(d,0,100,100,0xffabcdef);assert(!pd_redo(d)&&d->history_used==23*260);assert(pd_undo(d)&&!d->layer[0].pixels[12900]);assert(pd_redo(d));
    pd_init(d);for(unsigned n=1;n<=10;n++){assert(pd_begin(d));for(unsigned i=0;i<PD_PIXELS;i++)d->layer[0].pixels[i]=0xff000000u+n;assert(pd_commit(d));}
    assert(d->count==8&&d->history_used==PD_HISTORY_BYTES);for(unsigned n=9;n>=2;n--){assert(pd_undo(d));for(unsigned i=0;i<PD_PIXELS;i++)assert(d->layer[0].pixels[i]==0xff000000u+n);}assert(!pd_undo(d));for(unsigned n=3;n<=10;n++)assert(pd_redo(d));
    /* Property-only transactions use no pixel storage and survive mixed eviction. */
    assert(pd_rename_layer(d,"Renamed"));assert(d->history_used==PD_HISTORY_BYTES&&d->history[d->count-1].bytes==0);assert(pd_undo(d)&&strcmp(d->names[0],"Renamed"));assert(pd_redo(d)&&!strcmp(d->names[0],"Renamed"));dot(d,0,127,127,0xff123456);assert(d->history_used<=PD_HISTORY_BYTES);assert(pd_undo(d)&&d->layer[0].pixels[PD_PIXELS-1]==0xff00000a);
    assert(pd_begin(d));pd_line(d,0,0,127,127,0xffffffff);unsigned used=d->history_used;pd_cancel(d);assert(d->history_used==used&&d->layer[0].pixels[0]==0xff00000a);
    free(d);puts("Tile history: sparse capacity, dense budget, eviction, branching, metadata and cancellation: PASS");
}
static void workflow_checks(const char *dir){
    char root[PD_PATH],path[PD_PATH],error[64];assert(pd_join(root,dir,"workflow")&&pd_mkdir(root));
    PdApp *a=calloc(1,sizeof(*a)),*reload=calloc(1,sizeof(*reload));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&reload&&b&&t);app_init(a,256,192,root,"TEST");
    uint32_t colors[256]={0xffff0000,0x8000ff00,0xff0000ff};assert(pd_join(path,root,"colors.hex")&&pd_palette_write(path,colors,3,1,false,error));
    for(unsigned i=0;i<12;i++){char name[64];snprintf(name,64,"art-%02u.pdd",i);assert(pd_join(path,root,name)&&pd_write_file(&a->doc,path,0,false,error));}
    app_input(a,false,0,0,PD_MENU,0);tap(a,20,70);assert(a->page==4&&a->file_filter==0);
    uint64_t rev=a->doc.revision;app_input(a,true,40,140,0,0);app_input(a,true,40,90,0,0);app_input(a,false,0,0,0,0);assert(a->file_scroll==50&&a->selected==-1&&!a->preview_attempted&&a->doc.revision==rev);
    tap(a,40,90);assert(a->selected>=0&&a->preview_ready);
    tap(a,100,60);tap(a,100,60);tap(a,100,60);assert(a->file_filter==3&&a->selected==-1&&a->file_scroll==0);tap(a,40,90);assert(a->preview_palette_count==3&&a->preview_ready&&a->file_preview[0]==colors[0]&&a->file_preview[16]==colors[1]&&a->doc.revision==rev);
    for(int mode=0;mode<2;mode++){a->width=mode?320:256;a->height=mode?240:192;app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-palette-preview":"nds-palette-preview",t,mode?400:256,a->height);a->file_filter=0;a->file_scroll=13;app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-scroll-files":"nds-scroll-files",b,a->width,a->height);a->file_filter=3;}
    a->width=256;a->height=192;a->page=18;a->new_layers=1;a->new_background=0;tap(a,40,60);tap(a,40,95);tap(a,40,175);assert(a->confirm==2);tap(a,220,175);assert(a->doc.revision==rev&&a->page==18);tap(a,40,175);tap(a,40,175);assert(a->page==0&&a->doc.enabled==3&&a->doc.layer[0].pixels[0]==0xffffffff&&pd_dirty(&a->doc));
    assert(pd_join(path,root,"named.pdd")&&pd_write_file(&a->doc,path,0,false,error));strcpy(a->project,path);a->project_native=true;dot(&a->doc,1,2,2,0xff123456);uint64_t saved=a->doc.saved_revision;
    a->now_ms=119999;app_input(a,false,0,0,0,0);assert(!a->recovery_available);a->now_ms=120000;a->page=1;app_input(a,false,0,0,0,0);assert(!a->recovery_available);a->page=0;app_input(a,true,0,100,0,0);app_input(a,false,0,0,0,0);a->now_ms=122001;app_input(a,false,0,0,0,0);assert(a->recovery_available&&pd_dirty(&a->doc)&&a->doc.saved_revision==saved);
    assert(pd_open_file(&reload->doc,path,error)&&reload->doc.layer[1].pixels[258]==0); /* named file unchanged */
    app_init(reload,256,192,root,"TEST");assert(reload->page==19&&reload->recovery_pending);tap(reload,40,50);tap(reload,220,175);assert(reload->recovery_pending&&reload->page==19);tap(reload,40,50);tap(reload,40,175);assert(!reload->page&&!reload->recovery_pending&&reload->doc.layer[1].pixels[258]==0xff123456&&pd_dirty(&reload->doc)&&!reload->project[0]);
    dot(&a->doc,1,3,3,0xffabcdef);a->now_ms=242002;app_input(a,false,0,0,0,0);char recovery[PD_PATH];snprintf(recovery,sizeof(recovery),"%s/.recovery/session.pdd",root);FILE *f=fopen(recovery,"wb");assert(f);fputs("corrupt",f);fclose(f);
    app_init(reload,256,192,root,"TEST");tap(reload,40,50);tap(reload,40,175);assert(reload->doc.layer[1].pixels[258]==0xff123456&&reload->doc.layer[1].pixels[387]==0); /* valid backup fallback */
    for(int mode=0;mode<2;mode++){a->width=mode?320:256;a->height=mode?240:192;for(int page=18;page<=19;page++){a->page=page;app_render(a,b,t,mode?400:256,a->height);char name[64];snprintf(name,64,"%s-workflow-%d",mode?"3ds":"nds",page);ppm(dir,name,b,a->width,a->height);}}
    reload->page=19;tap(reload,40,80);tap(reload,40,175);assert(!reload->recovery_available&&reload->doc.layer[1].pixels[258]==0xff123456);
    free(a);free(reload);free(b);free(t);puts("Browser drag/filter/palette previews, new project cancellation, idle recovery and corrupt-backup fallback: PASS");
}
static void file_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d)),*loaded=calloc(1,sizeof(*loaded));assert(d&&loaded);pd_init(d);
    dot(d,0,2,3,0xffe04444);dot(d,1,2,3,0x80218c63);dot(d,2,80,40,0xff1850b0);dot(d,3,30,60,0x60445566);
    for(unsigned l=0;l<4;l++){d->active=l;assert(pd_begin(d));pd_line(d,10,10,90,90,0xff223344+l*0x00112233);assert(pd_commit(d));}
    d->layer[2].visible=0;d->layer[1].opacity=128;snprintf(d->names[0],64,"Outline");
    char path[512],native[512],error[64];assert(pd_join(native,dir,"roundtrip.pdd"));
    assert(pd_write_file(d,native,0,false,error));assert(!pd_dirty(d));assert(pd_open_file(loaded,native,error));assert(!memcmp(d->layer,loaded->layer,sizeof(d->layer)));assert(!strcmp(loaded->names[0],"Outline"));
    assert(!pd_write_file(d,native,0,false,error));assert(pd_write_file(d,native,0,true,error));
    snprintf(path,sizeof(path),"%s.bak",native);assert(pd_exists(path));
    assert(pd_join(path,dir,"roundtrip.aseprite"));assert(pd_write_file(d,path,1,false,error));assert(pd_open_file(loaded,path,error));assert(!memcmp(d->layer,loaded->layer,sizeof(d->layer)));
    d->active=1;assert(pd_join(path,dir,"active-layer.png"));assert(pd_write_file(d,path,3,false,error));assert(pd_open_file(loaded,path,error)&&loaded->enabled==1);for(unsigned y=0;y<d->height;y++)for(unsigned x=0;x<d->width;x++){uint32_t c=d->layer[d->active].pixels[y*d->stride+x];uint32_t expected=(c&0x00ffffffu)|((((c>>24)*d->layer[d->active].opacity+127)/255)<<24);assert(loaded->layer[0].pixels[y*loaded->stride+x]==expected);}
    assert(pd_join(path,dir,"roundtrip.png"));assert(pd_write_file(d,path,2,false,error));
    /* Rejected import is transactional, including history/revision. */
    uint64_t revision=d->revision;assert(pd_open_file(loaded,path,error)&&loaded->enabled==1);pd_composite(d);assert(!memcmp(loaded->layer[0].pixels,d->composite,sizeof(d->composite)));assert(d->revision==revision&&d->layer[0].pixels[386]==0xffe04444);
    assert(pd_join(path,dir,"truncated.aseprite"));FILE *f=fopen(path,"wb");assert(f);fputs("bad",f);fclose(f);assert(!pd_open_file(d,path,error));assert(d->revision==revision);
    assert(!pd_join(path,dir,"../escape"));assert(!pd_join(path,dir,"foo/bar"));
    PdListing *list=calloc(1,sizeof(*list));assert(list);assert(pd_list(dir,list,error));assert(list->count>=4);free(list);
    uint32_t preview[64*64];assert(pd_join(path,dir,"roundtrip.pdd"));assert(pd_open_file(loaded,path,error));pd_composite(loaded);assert(pd_preview_file(path,preview,error));for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)assert(preview[y*64+x]==loaded->composite[(y*2)*128+x*2]);
    uint32_t first=preview[0];assert(pd_join(path,dir,"truncated.aseprite"));assert(!pd_preview_file(path,preview,error)&&preview[0]==first);
    free(d);free(loaded);puts("Named native/ASE/PNG, backups, rejected-import preservation and directory checks: PASS");
}
static void layer_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d)),*r=calloc(1,sizeof(*r));assert(d&&r);pd_init(d);assert(d->enabled==1&&!pd_delete_layer(d));
    assert(pd_add_layer(d)&&d->active==1&&d->enabled==3);assert(pd_rename_layer(d,"Ink"));
    dot(d,1,20,20,0xffef1234);assert(pd_delete_layer(d)&&d->enabled==1);
    assert(pd_undo(d)&&d->enabled==3&&d->active==1&&d->layer[1].pixels[2580]==0xffef1234&&!strcmp(d->names[1],"Ink"));
    assert(pd_redo(d)&&d->enabled==1);assert(pd_undo(d));assert(pd_undo(d));assert(pd_undo(d));assert(!strcmp(d->names[1],"Layer 2"));assert(pd_undo(d)&&d->enabled==1);
    assert(pd_add_layer(d));assert(!pd_redo(d));assert(pd_add_layer(d));assert(pd_add_layer(d));
    d->active=1;assert(pd_delete_layer(d)&&d->enabled==13);d->active=2;assert(pd_rename_layer(d,"Sky"));dot(d,2,30,30,0xffabcdef);
    char path[512],error[64];assert(pd_join(path,dir,"layer-set.pdd"));assert(pd_write_file(d,path,0,false,error));assert(pd_open_file(r,path,error));assert(r->enabled==13&&!memcmp(d->layer,r->layer,sizeof(d->layer))&&!memcmp(d->names,r->names,sizeof(d->names)));
    assert(pd_join(path,dir,"layer-set.aseprite"));assert(pd_write_file(d,path,1,false,error));assert(pd_open_file(r,path,error));assert(r->enabled==7&&!strcmp(r->names[1],"Sky")&&r->layer[1].pixels[3870]==0xffabcdef);
    pd_init(d);assert(pd_join(path,dir,"one-layer.aseprite"));assert(pd_write_file(d,path,1,false,error));assert(pd_open_file(r,path,error)&&r->enabled==1);
    dot(d,0,0,0,0xff808080);assert(pd_add_layer(d));dot(d,1,0,0,0xff808080);
    d->layer[1].blend=1;pd_composite(d);assert(d->composite[0]==0xff404040);
    d->layer[1].blend=2;pd_composite(d);assert(d->composite[0]==0xffc0c0c0);
    d->layer[1].blend=3;pd_composite(d);assert(d->composite[0]==0xff808080);
    assert(!pd_merge_down(d));d->layer[1].blend=0;d->layer[1].pixels[0]=0x80ff0000;pd_composite(d);uint32_t merged=d->composite[0];
    assert(pd_merge_down(d)&&d->enabled==1);pd_composite(d);assert(d->composite[0]==merged);
    assert(pd_undo(d)&&d->enabled==3&&d->layer[0].pixels[0]==0xff808080);assert(pd_redo(d));assert(pd_undo(d));
    d->active=1;assert(pd_duplicate_layer(d)&&d->enabled==7);assert(d->layer[d->active].pixels[0]==0x80ff0000);assert(pd_undo(d)&&d->enabled==3);
    d->active=1;assert(pd_move_layer(d,0));pd_composite(d);assert(d->composite[0]==0xff808080);assert(pd_undo(d));pd_composite(d);assert(d->composite[0]==merged);assert(pd_redo(d));
    d->layer[0].blend=2;assert(pd_join(path,dir,"ordered-blend.pdd"));assert(pd_write_file(d,path,0,false,error));assert(pd_open_file(r,path,error));assert(!memcmp(d->order,r->order,PD_LAYERS)&&r->layer[0].blend==2);
    assert(pd_join(path,dir,"ordered-blend.aseprite"));assert(pd_write_file(d,path,1,false,error));assert(pd_open_file(r,path,error));assert(r->layer[0].pixels[0]==0x80ff0000&&r->layer[1].blend==2);
    free(d);free(r);
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);
    for(int mode=0;mode<2;mode++){
        int w=mode?320:256,h=mode?240:192,tw=mode?400:256;app_init(a,w,h,"","TEST");tap(a,w*7/12,10);assert(a->page==1);
        tap(a,20,h-16);assert(a->doc.enabled==3&&a->doc.active==1);tap(a,w-12,40);assert(a->layer_popup);tap(a,50,150);assert(a->page==5&&a->name_action==2);
        snprintf(a->filename,sizeof(a->filename),"INK");tap(a,w-20,h-20);assert(a->page==1&&!strcmp(a->doc.names[1],"INK"));
        dot(&a->doc,0,64,64,0xff0000ff);dot(&a->doc,1,64,64,0xffff0000);a->doc.layer[1].visible=0;
        app_render(a,b,t,tw,h);int margin=8,gap=8,panel=(tw-margin*3-gap)/2,image_h=h-70,image=panel-8;if(image_h<image)image=image_h;int second=margin+panel+gap;assert(t[(36+(image_h-image)/2+image/2)*tw+margin+(panel-image)/2+image/2]==0x0000ff);assert(t[(36+(image_h-image)/2+image/2)*tw+second+(panel-image)/2+image/2]==0xff0000);
        ppm(dir,mode?"3ds-layers":"nds-layers",b,w,h);ppm(dir,mode?"3ds-layer-preview":"nds-layer-preview",t,tw,h);
        tap(a,w-20,h-16);assert(a->layer_delete);tap(a,w-40,134);assert(!a->layer_delete&&a->doc.enabled==3);
        tap(a,w-20,h-16);tap(a,40,134);assert(a->doc.enabled==1);tap(a,10,10);assert(a->doc.enabled==3&&a->doc.layer[1].pixels[8256]==0xffff0000);
        tap(a,34,50);assert(a->doc.layer[1].visible); /* eye */
        tap(a,w-40,50);assert(a->doc.layer[1].flags&PD_LAYER_LOCK);tap(a,w-40,50);assert(!(a->doc.layer[1].flags&PD_LAYER_LOCK));
        tap(a,w-12,40);assert(a->layer_popup);app_render(a,b,t,tw,h);ppm(dir,mode?"3ds-layer-options":"nds-layer-options",b,w,h);
        uint8_t blend=a->doc.layer[1].blend;tap(a,w/2,100);assert(a->layer_blend_dropdown.open&&a->doc.layer[1].blend==blend);app_render(a,b,t,tw,h);ppm(dir,mode?"3ds-layer-blend":"nds-layer-blend",b,w,h);tap(a,w/2,140);assert(!a->layer_blend_dropdown.open&&a->doc.layer[1].blend==1);
        unsigned edits=a->doc.count;app_input(a,true,50,58,0,0);app_input(a,true,w/2,58,0,0);app_input(a,false,0,0,0,0);assert(a->doc.count==(edits<PD_HISTORY?edits+1:PD_HISTORY));assert(a->doc.layer[1].opacity<255);
        tap(a,w-20,37);assert(!a->layer_popup);tap(a,w-20,h-16);app_render(a,b,t,tw,h);ppm(dir,mode?"3ds-layer-delete":"nds-layer-delete",b,w,h);tap(a,w-40,134);
        uint8_t order[PD_LAYERS];memcpy(order,a->doc.order,PD_LAYERS);a->now_ms=1000;app_input(a,true,80,50,0,0);a->now_ms=1149;app_input(a,true,80,82,0,0);assert(!a->layer_drag);app_input(a,false,0,0,0,0);assert(!memcmp(order,a->doc.order,PD_LAYERS));
        a->now_ms=2000;app_input(a,true,80,50,0,0);a->now_ms=2150;app_input(a,true,80,82,0,0);assert(a->layer_drag);app_render(a,b,t,tw,h);ppm(dir,mode?"3ds-layer-drag":"nds-layer-drag",b,w,h);app_input(a,false,0,0,0,0);assert(memcmp(order,a->doc.order,PD_LAYERS));tap(a,10,10);assert(!memcmp(order,a->doc.order,PD_LAYERS));
    }
    free(a);free(b);free(t);puts("Layer add/rename/delete/undo, sparse native/ASE roundtrip, confirmation and composite/active preview: PASS");
}
static void large_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d)),*r=calloc(1,sizeof(*r));assert(d&&r);pd_init(d);pd_init(r);r->profile=3;
    uint32_t *mask=calloc(PD_MAX_PIXELS/32,4);assert(mask);char path[PD_PATH],error[64];unsigned sizes[][2]={{129,129},{256,256},{512,512},{1,512},{512,1},{137,319}};
    assert(!pd_create(d,129,128,1)&&d->width==128);assert(!pd_create(d,513,512,3)&&d->width==128);
    for(unsigned k=0;k<6;k++){
        unsigned w=sizes[k][0],h=sizes[k][1];assert(pd_create(d,w,h,2));assert(d->allocation&&d->stride>=w&&d->rows>=h);unsigned last=(h-1)*d->stride+w-1;
        assert(pd_begin(d));pd_line(d,(int)w-1,(int)h-1,(int)w-1,(int)h-1,0x80123456);assert(pd_commit(d));assert(pd_pixels(&d->layer[0])[last]==0x80123456);assert(pd_undo(d)&&!pd_pixels(&d->layer[0])[last]);assert(pd_redo(d));
        for(unsigned kind=0;kind<3;kind++){snprintf(path,sizeof(path),"%s/large-%u.%s",dir,k,kind==0?"pdd":kind==1?"aseprite":"png");assert(pd_write_file(d,path,kind,false,error));if(kind<2){assert(pd_open_file(r,path,error));assert(r->width==w&&r->height==h&&pd_pixels(&r->layer[0])[last]==0x80123456);assert(!memcmp(pd_pixels(&d->layer[0]),pd_pixels(&r->layer[0]),pd_bytes(d)));uint32_t preview[4096];assert(pd_preview_file_profile(path,preview,2,error));}}
        assert(pd_fill(d,0,0,0xff556677,0));assert(pd_pixels(&d->layer[0])[0]==0xff556677);assert(pd_pixels(&d->layer[0])[last]==0x80123456||last==0);assert(pd_undo(d));
    }
    assert(pd_create(d,512,512,4)&&d->history_capacity==8u*1024u*1024u);assert(pd_create(d,128,128,4)&&d->history_capacity==2u*1024u*1024u);assert(pd_create(d,512,512,2)&&d->capacity==2);assert(pd_duplicate_layer(d));assert(!pd_add_layer(d));assert(pd_undo(d));d->active=0;
    for(unsigned pass=0;pass<3;pass++)assert(pd_fill_options(d,0,0,0xff000001+pass,0,NULL,true,255,false));assert(d->count==2&&d->history_used==2*1024*1024);assert(pd_undo(d)&&pd_pixels(&d->layer[0])[262143]==0xff000002);assert(pd_undo(d)&&pd_pixels(&d->layer[0])[262143]==0xff000001);assert(!pd_undo(d));assert(pd_redo(d));
    assert(pd_create(d,512,512,2));memset(mask,0,PD_MAX_PIXELS/8);pd_mask_rectangle_stride(mask,d->stride,d->rows,400,400,402,402);assert(pd_begin(d));pd_masked_line(d,400,400,402,402,0xffabcdef,1,false,0,NULL,mask);assert(pd_commit(d));assert(pd_pixels(&d->layer[0])[400*512+400]);assert(pd_begin(d));assert(pd_move_selection(d,mask,10,10,false));assert(pd_commit(d)&&pd_pixels(&d->layer[0])[410*512+410]);assert(pd_undo(d));
    PdClipboard *clip=calloc(1,sizeof(*clip));assert(clip&&pd_copy_selection(d,mask,clip));assert(pd_paste_selection(d,clip,500,500)&&pd_pixels(&d->layer[0])[500*512+500]);free(clip);
    memset(mask,0,PD_MAX_PIXELS/8);pd_mask_rectangle_stride(mask,512,512,0,0,511,511);clip=calloc(1,sizeof(*clip));assert(!pd_copy_selection(d,mask,clip));free(clip);
    PdPerfectStroke perfect={0};assert(pd_begin(d));pd_perfect_line(d,&perfect,450,450,451,450,0xffffffff,0,NULL,NULL);pd_perfect_line(d,&perfect,451,450,451,451,0xffffffff,0,NULL,NULL);assert(!pd_pixels(&d->layer[0])[450*512+451]&&pd_pixels(&d->layer[0])[451*512+451]);pd_cancel(d);
    assert(pd_begin(d));pd_shape(d,0,0,511,511,2,false,0xffffffff,1,false,0,NULL,NULL);assert(pd_commit(d));assert(pd_pixels(&d->layer[0])[255]==0xffffffff&&!pd_pixels(&d->layer[0])[0]);assert(pd_undo(d));
    snprintf(path,sizeof(path),"%s/large-recovery.pdd",dir);uint64_t saved=d->saved_revision;assert(pd_write_recovery(d,path,error)&&d->saved_revision==saved);assert(pd_open_file(r,path,error));
    uint32_t *original=pd_pixels(&d->layer[0]);uint32_t pixel=original[400*512+400];pd_test_fail_allocation(true);assert(!pd_create(d,256,256,2));assert(pd_pixels(&d->layer[0])==original&&original[400*512+400]==pixel);assert(!pd_open_file(d,path,error)&&pd_pixels(&d->layer[0])==original);pd_test_fail_allocation(false);
    assert(pd_create(r,128,128,1));r->layer[0].pixels[0]=0xffabcdef;assert(!pd_open_file(r,path,error)&&r->layer[0].pixels[0]==0xffabcdef&&r->width==128);
    PdApp *a=calloc(1,sizeof(*a)),*ref=calloc(1,sizeof(*ref));uint32_t *b=malloc(320*240*4),*top=malloc(400*240*4),*rb=malloc(320*240*4),*rt=malloc(400*240*4);assert(a&&ref&&b&&top&&rb&&rt);
    for(unsigned mode=0;mode<2;mode++){
        app_init(a,mode?320:256,mode?240:192,"","LARGE TEST");a->doc.profile=mode?3:2;a->page=20;app_render(a,b,top,mode?400:256,a->height);ppm(dir,mode?"3ds-large-size":"nds-large-size",b,a->width,a->height);tap(a,a->width-10,40);assert(a->new_width==512);a->page=18;tap(a,20,a->height-12);assert(a->confirm==2);app_render(a,b,top,mode?400:256,a->height);ppm(dir,mode?"3ds-large-warning":"nds-large-warning",b,a->width,a->height);tap(a,20,a->height-16);assert(a->doc.width==512&&a->doc.stride==512);
        a->ox=a->oy=300;a->pixel_perfect=true;for(unsigned guides=0;guides<=15;guides+=15){a->guides=guides;a->redraw=true;for(int j=0;j<4;j++){app_input(a,true,50+j,80+j,0,0);assert(a->stroke);clone_app(ref,a);ref->partial=false;app_render(a,b,top,mode?400:256,a->height);app_render(ref,rb,rt,mode?400:256,a->height);assert(!memcmp(b,rb,a->width*a->height*4));}app_input(a,false,0,0,0,0);}
        pd_dispose(&a->doc);
    }
    free(a);free(ref);free(b);free(top);free(rb);free(rt);pd_dispose(d);pd_dispose(r);free(d);free(r);free(mask);puts("Large canvas profiles, high indices, dense history, files, masks, pixel-perfect, memory failure and UI/render: PASS");
}

static void pan_checks(void){
    PdApp *a=calloc(1,sizeof(*a)),*ref=calloc(1,sizeof(*ref));uint32_t *b=malloc(320*240*4),*top=malloc(400*240*4),*rb=malloc(320*240*4),*rt=malloc(400*240*4);assert(a&&ref&&b&&top&&rb&&rt);unsigned frames=0;
    for(unsigned mode=0;mode<2;mode++)for(unsigned size=128;size<=512;size*=4){
        app_init(a,mode?320:256,mode?240:192,"","PAN TEST");assert(pd_create(&a->doc,size,size,3));a->pixel_grid=true;
        for(unsigned y=0;y<size;y++)for(unsigned x=0;x<size;x++)pd_pixels(&a->doc.layer[0])[y*a->doc.stride+x]=((x+y)%7)?(0x80123456u+x*11+y*77):0;pd_composite(&a->doc);
        for(unsigned z=4;z<=32;z*=2)for(unsigned guides=0;guides<=12;guides+=4){
            a->zoom=(int)z;a->guides=guides;a->ox=a->oy=20;a->subx=a->suby=0;a->redraw=true;app_render(a,b,top,mode?400:256,a->height);
            app_input(a,true,100,100,0,PD_PAN);for(int i=1;i<=8;i++){
                int oldx=a->ox*a->zoom+a->subx,oldy=a->oy*a->zoom+a->suby;app_input(a,true,100-i,100-i,0,PD_PAN);assert(a->ox*a->zoom+a->subx==oldx+1);assert(a->oy*a->zoom+a->suby==oldy+1);assert(a->pan_frame);
                unsigned calls=pd_test_composites();clone_app(ref,a);ref->pan_frame=false;app_render(a,b,top,mode?400:256,a->height);assert(pd_test_composites()==calls);app_render(ref,rb,rt,mode?400:256,a->height);assert(!memcmp(b,rb,a->width*a->height*4)&&!memcmp(top,rt,(mode?400:256)*a->height*4));frames++;
            }app_input(a,false,0,0,0,0);a->redraw=false;app_input(a,true,100,100,0,PD_PAN);assert(!a->redraw);app_input(a,false,0,0,0,0);
            a->color=0xffabcdef;a->ox=a->oy=10;a->subx=1;a->suby=(int)z-1;a->redraw=true;app_render(a,b,top,mode?400:256,a->height);
            a->color=0xffabcdef;for(int j=0;j<3;j++){app_input(a,true,71+j,83+j,0,0);clone_app(ref,a);ref->partial=false;ref->pan_frame=false;app_render(a,b,top,mode?400:256,a->height);app_render(ref,rb,rt,mode?400:256,a->height);assert(!memcmp(b,rb,a->width*a->height*4));}app_input(a,false,0,0,0,0);app_render(a,b,top,mode?400:256,a->height);
        }pd_dispose(&a->doc);
    }
    free(a);free(ref);free(b);free(top);free(rb);free(rt);printf("Smooth pan: %u cached/full frames, one-screen-pixel motion, no recomposition, stationary gestures and fractional drawing: PASS\n",frames);
}

static void png_browser_checks(const char *dir){
    char folder[PD_PATH],path[PD_PATH],error[64];assert(pd_join(folder,dir,"png-browser")&&pd_mkdir(folder));assert(pd_join(path,folder,"art.png"));PdDocument *d=calloc(1,sizeof(*d));PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(d&&a&&b&&t);pd_init(d);d->width=16;d->height=32;d->layer[0].pixels[31*128+15]=0x80abcdef;assert(pd_write_file(d,path,2,false,error));
    for(unsigned mode=0;mode<2;mode++){app_init(a,mode?320:256,mode?240:192,folder,"PNG TEST");app_input(a,false,0,0,PD_MENU,0);tap(a,20,70);tap(a,30,90);assert(a->page==4&&a->preview_ready&&!a->preview_palette_count);app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-png-preview":"nds-png-preview",t,mode?400:256,a->height);tap(a,a->width-20,a->height-18);assert(a->confirm==4);tap(a,20,a->height-16);assert(a->page==0&&a->doc.width==16&&a->doc.height==32&&!a->project_native&&pd_dirty(&a->doc)&&a->doc.layer[0].pixels[31*128+15]==0x80abcdef);}
    free(d);free(a);free(b);free(t);puts("PNG browser default filtering, artwork preview, confirmed open and imported Save As state: PASS");
}

static void scale_duplicate_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d));uint32_t *mask=calloc(PD_MAX_PIXELS/32,4),*result=calloc(PD_MAX_PIXELS/32,4);assert(d&&mask&&result);pd_init(d);
    pd_mask_rectangle(mask,2,2,3,3);d->layer[0].pixels[2*128+2]=0x80ff0000;d->layer[0].pixels[3*128+3]=0xff0000ff;d->layer[0].pixels[5*128+2]=0xff00ff00;
    assert(pd_scale_selection(d,mask,2,result));assert(d->layer[0].pixels[2*128+2]==0x80ff0000&&d->layer[0].pixels[3*128+3]==0x80ff0000&&d->layer[0].pixels[5*128+5]==0xff0000ff);assert(d->layer[0].pixels[5*128+2]==0xff00ff00&&!(result[(5*128+2)/32]&(1u<<((5*128+2)%32))));assert(d->count==1&&pd_undo(d)&&d->layer[0].pixels[3*128+3]==0xff0000ff);assert(pd_redo(d));assert(pd_undo(d));
    d->layer[0].flags=PD_LAYER_LOCK;assert(!pd_scale_selection(d,mask,2,result));assert(pd_duplicate_selection(d,mask)&&d->active==1&&d->layer[1].flags==0&&d->layer[1].pixels[2*128+2]==0x80ff0000&&!d->layer[1].pixels[5*128+2]);assert(d->layer[0].pixels[3*128+3]==0xff0000ff);assert(pd_undo(d)&&d->enabled==1);assert(pd_redo(d)&&d->enabled==3);
    assert(pd_create(d,128,128,1));memset(mask,0,PD_MAX_PIXELS/8);pd_mask_rectangle(mask,0,0,2,2);for(unsigned y=0;y<3;y++)for(unsigned x=0;x<3;x++)d->layer[0].pixels[y*128+x]=0xff000000+y*3+x;assert(pd_scale_selection(d,mask,-2,result));assert(d->layer[0].pixels[0]==0xff000000&&d->layer[0].pixels[1]==0xff000002&&d->layer[0].pixels[129]==0xff000008);assert(pd_undo(d));
    memset(mask,0,PD_MAX_PIXELS/8);pd_mask_rectangle(mask,0,0,127,127);uint64_t revision=d->revision;assert(!pd_scale_selection(d,mask,2,result)&&d->revision==revision);
    assert(pd_create(d,512,512,2));memset(mask,0,PD_MAX_PIXELS/8);pd_mask_rectangle_stride(mask,512,512,384,384,511,511);pd_pixels(&d->layer[0])[511*512+511]=0xffffffff;assert(pd_scale_selection(d,mask,4,result));assert(pd_pixels(&d->layer[0])[511*512+511]==0xffffffff&&pd_pixels(&d->layer[0])[508*512+508]==0xffffffff);assert(pd_undo(d));assert(pd_duplicate_selection(d,mask)&&!pd_duplicate_selection(d,mask));pd_dispose(d);free(d);free(mask);free(result);
    PdApp *a=calloc(1,sizeof(*a));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&b&&t);for(unsigned mode=0;mode<2;mode++){app_init(a,mode?320:256,mode?240:192,"","TEST");a->page=25;a->selection_valid=true;pd_mask_rectangle(a->selection,2,2,3,3);a->doc.layer[0].pixels[258]=0xffff0000;app_render(a,b,t,mode?400:256,a->height);ppm(dir,mode?"3ds-transforms-010":"nds-transforms-010",b,a->width,a->height);tap(a,a->width*3/5,86);assert(a->doc.active==1&&a->doc.enabled==3&&a->page==0);a->page=25;tap(a,40,112);assert(a->page==0&&a->doc.layer[1].pixels[3*128+3]==0xffff0000);}free(a);free(b);free(t);puts("Duplicate selection and nearest-neighbor scaling: alpha, holes, odd half-size, undo, bounds, locks, large output and UI: PASS");
}

static void dither_checks(const char *dir){
    PdDocument *d=calloc(1,sizeof(*d));assert(d);uint32_t damage[PD_MAX_PIXELS/32],mask[PD_MAX_PIXELS/32];
    /* Independent 2x2 coverage oracle, untouched gaps, repeat overlap, undo, all guides. */
    const unsigned rank[2][2]={{0,2},{3,1}};
    for(unsigned large=0;large<2;large++)for(unsigned pattern=1;pattern<=3;pattern++)for(unsigned guides=0;guides<16;guides++){
        pd_init(d);if(large)assert(pd_create(d,512,512,3));else{d->width=16;d->height=16;}
        for(unsigned i=0;i<pd_plane(d);i++)pd_pixels(&d->layer[0])[i]=0x80445566;
        memset(mask,0,sizeof(mask));for(unsigned y=0;y<d->height;y++)for(unsigned x=0;x<d->width;x++)if(x%5)mask[(y*d->stride+x)/32]|=1u<<((y*d->stride+x)%32);
        memset(damage,0,sizeof(damage));assert(pd_begin(d));
        for(unsigned y=0;y<d->height;y++)pd_pattern_line(d,0,(int)y,(int)d->width-1,(int)y,0xc0abcdef,1,false,guides,damage,mask,pattern);
        for(unsigned y=0;y<d->height;y++)pd_pattern_line(d,0,(int)y,(int)d->width-1,(int)y,0xc0abcdef,1,false,guides,damage,mask,pattern);
        assert(pd_commit(d));
        for(unsigned y=0;y<d->height;y++)for(unsigned x=0;x<d->width;x++){unsigned i=y*d->stride+x;bool painted=x%5&&rank[y%2][x%2]<pattern;assert(pd_pixels(&d->layer[0])[i]==(painted?0xc0abcdef:0x80445566));assert(((damage[i/32]>>(i%32))&1)==(unsigned)painted);}
        assert(pd_undo(d));for(unsigned i=0;i<pd_plane(d);i++)assert(pd_pixels(&d->layer[0])[i]==0x80445566);assert(pd_redo(d));pd_dispose(d);
    }
    pd_init(d);d->layer[0].pixels[0]=0x80445566;d->layer[0].flags=PD_LAYER_ALPHA_LOCK;assert(pd_begin(d));pd_pattern_line(d,0,0,3,0,0xffffffff,1,false,0,NULL,NULL,2);assert(pd_commit(d));assert(d->layer[0].pixels[0]==0x80ffffff&&!d->layer[0].pixels[2]);assert(pd_undo(d));d->layer[0].flags=PD_LAYER_LOCK;assert(pd_begin(d));pd_pattern_line(d,0,0,3,0,0xffffffff,1,false,0,NULL,NULL,2);assert(!pd_commit(d));free(d);
    PdApp *a=calloc(1,sizeof(*a)),*r=calloc(1,sizeof(*r));uint32_t *b=malloc(320*240*4),*t=malloc(400*240*4);assert(a&&r&&b&&t);
    for(unsigned mode=0;mode<2;mode++)for(unsigned handed=0;handed<2;handed++){
        app_init(a,mode?320:256,mode?240:192,"","TEST");a->handed=handed;a->page=21;int left=handed?a->width-200:0;
        app_render(a,b,t,mode?400:256,a->height);if(!handed)ppm(dir,mode?"3ds-pencil-011":"nds-pencil-011",b,a->width,a->height);
        tap(a,left+50,177);assert(a->page==27);int middle=a->width*3/5;tap(a,middle,70);assert(a->brush_pattern==2);tap(a,middle,108);assert(a->brush_preset==2);tap(a,50,150);assert(a->brush_size[0]==8&&a->brush_pattern==2);
        a->brush_size[0]=11;a->brush_round[0]=1;tap(a,180,150);a->brush_size[0]=1;tap(a,50,150);assert(a->brush_size[0]==11&&a->brush_round[0]==1);
        app_render(a,b,t,mode?400:256,a->height);if(!handed)ppm(dir,mode?"3ds-dither-011":"nds-dither-011",b,a->width,a->height);
        tap(a,left+50,176);assert(a->page==0&&a->toolbox&&a->tool_options_open&&!a->doc.count);app_input(a,false,0,0,PD_TOOLS,0);assert(!a->toolbox&&!a->page);
    }
    app_init(a,256,192,dir,"TEST");a->page=27;a->brush_size[0]=13;a->brush_round[0]=1;a->pixel_perfect=true;tap(a,150,70);tap(a,20,108);tap(a,180,150);
    app_init(r,256,192,dir,"TEST");assert(r->brush_pattern==2&&r->brush_presets[0][0]==13&&r->brush_presets[0][1]==1&&r->brush_presets[0][2]==1&&r->brush_presets[0][3]==2);
    /* Slot 1 is newer; a corrupt latest record must fall back to prior pattern record. */
    char path[PD_PATH];snprintf(path,sizeof(path),"%s/brush-presets1.bin",dir);FILE *f=fopen(path,"wb");assert(f);fputs("corrupt",f);fclose(f);app_init(r,256,192,dir,"TEST");assert(r->brush_pattern==2&&r->brush_presets[0][0]==1);
    /* Leave no preset state in the shared fixture root used by legacy checks. */
    assert(remove(path)==0);snprintf(path,sizeof(path),"%s/brush-presets0.bin",dir);assert(remove(path)==0);
    free(a);free(r);free(b);free(t);puts("Dither: coverage/gaps/overlap/alpha/locks/masks/guides/512/undo; presets: UI, persistence, corrupt fallback: PASS");
}

static void shortcut_checks(void){
    PdApp *a=calloc(1,sizeof(*a));assert(a);
    for(unsigned mode=0;mode<2;mode++){
        app_init(a,mode?320:256,mode?240:192,"","TEST");dot(&a->doc,0,3,3,0xffffffff);dot(&a->doc,0,4,4,0xffabcdef);
        unsigned l=PD_ERASE|PD_SHOULDER_L,r=PD_ERASE|PD_SHOULDER_R;
        app_input(a,false,0,0,l,l|PD_ZOOM);assert(a->doc.cursor==1&&a->zoom_overlay);
        app_input(a,false,0,0,0,l|PD_ZOOM);assert(a->doc.cursor==1);
        app_input(a,false,0,0,r,r|PD_ZOOM);assert(a->doc.cursor==2);
        app_input(a,false,0,0,l|r,l|r|PD_ZOOM);assert(a->doc.cursor==2);
        app_input(a,false,0,0,l,l);assert(a->doc.cursor==2&&!a->zoom_overlay);
        a->page=2;app_input(a,false,0,0,l,l|PD_ZOOM);assert(a->doc.cursor==2&&a->page==2);a->page=0;
        a->toolbox=true;app_input(a,false,0,0,l,l|PD_ZOOM);assert(a->doc.cursor==2);a->toolbox=false;
        a->confirm=2;app_input(a,false,0,0,l,l|PD_ZOOM);assert(a->doc.cursor==2);a->confirm=0;
        app_input(a,false,0,0,0,0);app_input(a,true,90,90,0,0);unsigned count=a->doc.count;
        app_input(a,true,95,95,l,l|PD_ZOOM);assert(a->doc.cursor>=count);unsigned cursor=a->doc.cursor;
        app_input(a,false,0,0,l,l|PD_ZOOM);assert(a->doc.cursor==cursor); /* Pen-up transition is not history. */
        a->bindings=27; /* reversed action mapping: B now Zoom */
        app_input(a,false,0,0,app_bindings(a,l),app_bindings(a,l|PD_TOOLS));assert(a->doc.cursor==cursor-1);
        app_input(a,false,0,0,app_bindings(a,r),app_bindings(a,r|PD_TOOLS));assert(a->doc.cursor==cursor);
    }
    free(a);puts("History chords: both platforms, remapping, no repeat, simultaneous buttons, menus and pen isolation: PASS");
}

static void gallery_checks(const char *dir){
    (void)dir;PdApp *a=calloc(1,sizeof(*a));assert(a);
    for(unsigned mode=0;mode<2;mode++){app_init(a,mode?320:256,mode?240:192,"","TEST");app_input(a,false,0,0,PD_MENU,0);tap(a,a->width-40,164);assert(a->page==3);assert(!a->community_loading&&!a->community_loaded);}
    free(a);puts("Postponed community menu is hidden in standard NDS/3DS builds: PASS");
}

#include "animation-checks.h"
int main(int argc,char **argv){
    if(argc==4&&!strcmp(argv[1],"png-check")){PdDocument *d=calloc(1,sizeof(*d));assert(d);pd_init(d);d->profile=3;char error[64];if(!pd_open_file(d,argv[2],error)){fprintf(stderr,"PNG failed: %s\n",error);assert(0);}FILE *f=fopen(argv[3],"rb");assert(f);unsigned dims[2];assert(fread(dims,4,2,f)==2&&d->width==dims[0]&&d->height==dims[1]&&d->enabled==1);for(unsigned y=0;y<d->height;y++)for(unsigned x=0;x<d->width;x++){unsigned char rgba[4];assert(fread(rgba,1,4,f)==4);uint32_t c=((uint32_t)rgba[3]<<24)|((uint32_t)rgba[0]<<16)|((uint32_t)rgba[1]<<8)|rgba[2];assert(pd_pixels(&d->layer[0])[y*d->stride+x]==c);}assert(fgetc(f)==EOF);fclose(f);pd_dispose(d);free(d);return 0;}

    if(argc==3&&!strcmp(argv[1],"reject")){PdDocument *x=calloc(1,sizeof(*x)),*before=malloc(sizeof(*x));char error[64];assert(x&&before);pd_init(x);dot(x,0,5,5,0xff123456);memcpy(before,x,sizeof(*x));assert(!pd_open_file(x,argv[2],error));assert(!memcmp(x,before,sizeof(*x)));printf("Rejected unchanged: %s\n",error);free(x);free(before);return 0;}
    if(argc==4&&!strcmp(argv[1],"palette-compare")){uint32_t a[256],b[256];unsigned na=0,nb=0;char error[64];assert(pd_palette_read(argv[2],a,&na,error));assert(pd_palette_read(argv[3],b,&nb,error));assert(na==nb&&!memcmp(a,b,na*4));puts("Independent palette import: PASS");return 0;}
    if(argc==4){PdDocument *x=calloc(1,sizeof(*x)),*y=calloc(1,sizeof(*y));char error[64];assert(x&&y);pd_init(x);pd_init(y);if(!strcmp(argv[1],"compare-large"))x->profile=y->profile=3;
        if(!pd_open_file(x,argv[2],error)){puts(error);return 1;}if(!pd_open_file(y,argv[3],error)){puts(error);return 1;}
        assert(x->width==y->width&&x->height==y->height);
        if(x->palette_count){assert(x->palette_count==y->palette_count&&!memcmp(x->palette,y->palette,x->palette_count*4));}
        unsigned j=0;for(unsigned k=0;k<PD_LAYERS;k++){unsigned i=x->order[k];if(!(x->enabled&(1u<<i)))continue;while(j<PD_LAYERS&&!(y->enabled&(1u<<y->order[j])))j++;assert(j<PD_LAYERS);unsigned other=y->order[j];assert(x->width==y->width&&x->height==y->height);assert(!memcmp(pd_pixels(&x->layer[i]),pd_pixels(&y->layer[other]),pd_bytes(x)));assert(x->layer[i].visible==y->layer[other].visible&&x->layer[i].opacity==y->layer[other].opacity&&x->layer[i].blend==y->layer[other].blend&&x->layer[i].flags==y->layer[other].flags);if(strcmp(argv[1],"compare-pixels"))assert(!strcmp(x->names[i],y->names[other]));j++;}while(j<PD_LAYERS){assert(!(y->enabled&(1u<<y->order[j])));j++;}puts("External Aseprite enabled-layer roundtrip: PASS");free(x);free(y);return 0;}
    assert(argc==2);community_json_checks();dns_packet_checks();gallery_checks(argv[1]);animation_checks(argv[1]);playback_checks(argv[1]);gif_checks(argv[1]);onion_checks(argv[1]);rapid_stroke_checks();timeline_checks(argv[1]);shortcut_checks();dither_checks(argv[1]);png_browser_checks(argv[1]);scale_duplicate_checks(argv[1]);pan_checks();large_checks(argv[1]);brush_checks(argv[1]);fill_checks(argv[1]);fill_options_checks(argv[1]);selection_checks(argv[1]);selection_alpha_checks();shape_checks(argv[1]);perfect_checks(argv[1]);protection_checks(argv[1]);eight_layer_checks(argv[1]);dimension_checks(argv[1]);tile_history_checks();file_checks(argv[1]);workflow_checks(argv[1]);browser_checks(argv[1]);adjustment_checks(argv[1]);color_checks(argv[1]);layer_checks(argv[1]);palette_checks(argv[1]);palette_file_checks(argv[1]);dropdown_component_checks();community_ui_checks(argv[1]);
    PdApp *prefs=calloc(1,sizeof(*prefs));assert(prefs);app_init(prefs,256,192,argv[1],"TEST");
    app_input(prefs,false,0,0,PD_MENU,0);tap(prefs,40,145);assert(prefs->page==8);
    tap(prefs,40,42);tap(prefs,40,175);assert(prefs->handed==1&&!prefs->page);
    app_init(prefs,256,192,argv[1],"TEST");assert(prefs->handed==1);
    app_input(prefs,false,0,0,PD_MENU,0);tap(prefs,40,145);tap(prefs,40,42);tap(prefs,40,175);assert(prefs->handed==0);
    char badsettings[512];snprintf(badsettings,sizeof(badsettings),"%s/settings1.bin",argv[1]);FILE *sf=fopen(badsettings,"wb");assert(sf);fputs("broken",sf);fclose(sf);
    app_init(prefs,256,192,argv[1],"TEST");assert(prefs->handed==1);
    assert(prefs->bindings==228&&app_bindings(prefs,127)==127);
    app_input(prefs,false,0,0,PD_MENU,0);tap(prefs,40,145);tap(prefs,20,135);tap(prefs,20,180);assert(prefs->page==10);
    tap(prefs,40,40);assert(prefs->draft_bindings==225&&prefs->bindings==228);
    tap(prefs,220,180);tap(prefs,220,175);assert(prefs->bindings==228); /* cancel draft */
    app_input(prefs,false,0,0,PD_MENU,0);tap(prefs,40,145);tap(prefs,20,135);tap(prefs,20,180);tap(prefs,40,40);tap(prefs,220,180);tap(prefs,40,175);
    app_init(prefs,256,192,argv[1],"TEST");assert(prefs->bindings==225&&app_bindings(prefs,PD_TOOLS)==PD_PAN&&app_bindings(prefs,PD_PAN)==PD_TOOLS&&app_bindings(prefs,PD_SAVE|PD_MENU|PD_ERASE)==(PD_SAVE|PD_MENU|PD_ERASE));
    /* Legacy settings remain readable; a CRC-valid invalid mapping is rejected. */
    unsigned char record[16]={0};memcpy(record,"PDS1",4);record[4]=10;record[8]=1;unsigned crc=(unsigned)crc32(0,record,12);for(int i=0;i<4;i++)record[12+i]=(unsigned char)(crc>>(8*i));
    sf=fopen(badsettings,"wb");assert(sf);assert(fwrite(record,1,16,sf)==16);fclose(sf);app_init(prefs,256,192,argv[1],"TEST");assert(prefs->bindings==228);
    memcpy(record,"PDS2",4);crc=(unsigned)crc32(0,record,12);for(int i=0;i<4;i++)record[12+i]=(unsigned char)(crc>>(8*i));sf=fopen(badsettings,"wb");assert(sf);assert(fwrite(record,1,16,sf)==16);fclose(sf);app_init(prefs,256,192,argv[1],"TEST");assert(prefs->bindings==228&&prefs->handed==1);
    prefs->zoom=8;app_pan(prefs,10,12);assert(prefs->ox==10&&prefs->oy==12);
    prefs->touching=true;app_pan(prefs,10,12);assert(prefs->ox==10);free(prefs);
    puts("Settings persistence/corrupt-slot recovery and analog pan isolation: PASS");
    PdDocument *d=calloc(1,sizeof(*d)),*loaded=calloc(1,sizeof(*loaded));assert(d&&loaded);pd_init(d);
    dot(d,0,5,5,0xffff0000);dot(d,1,5,5,0xff0000ff);pd_composite(d);assert(d->composite[5*128+5]==0xff0000ff);
    assert(pd_undo(d));pd_composite(d);assert(d->composite[5*128+5]==0xffff0000);assert(pd_redo(d));
    assert(pd_begin(d));d->layer[1].opacity=128;assert(pd_commit(d));pd_composite(d);assert(d->composite[645]==0xff7f0080);
    assert(pd_begin(d));d->layer[1].visible=0;assert(pd_commit(d));pd_composite(d);assert(d->composite[645]==0xffff0000);assert(pd_undo(d));
    dot(d,1,5,5,0);pd_composite(d);assert(d->composite[645]==0xffff0000);assert(!pd_redo(d));
    assert(pd_begin(d));pd_line(d,0,0,127,127,0xff00ff00);pd_cancel(d);assert(!d->layer[1].pixels[0]);
    assert(pd_begin(d));pd_line(d,0,0,127,127,0xff00ff00);assert(pd_commit(d));for(int i=0;i<128;i++)assert(d->layer[1].pixels[i*129]==0xff00ff00);
    for(int i=0;i<40;i++)dot(d,2,i,100,0xff123456);assert(d->count==PD_HISTORY);for(int i=0;i<PD_HISTORY;i++)assert(pd_undo(d));assert(!pd_undo(d));for(int i=0;i<PD_HISTORY;i++)assert(pd_redo(d));
    d->enabled=15;assert(pd_save(d,argv[1]));assert(!pd_dirty(d));assert(pd_load(loaded,argv[1])==1);assert(!memcmp(d->layer,loaded->layer,sizeof(d->layer)));
    dot(d,3,50,50,0xffabcdef);assert(pd_dirty(d));assert(pd_save(d,argv[1]));assert(pd_load(loaded,argv[1])==1);assert(loaded->layer[3].pixels[6450]==0xffabcdef);
    char path[512];snprintf(path,sizeof(path),"%s/project1.pdd",argv[1]);FILE *f=fopen(path,"wb");assert(f);fputs("truncated",f);fclose(f);
    assert(pd_load(loaded,argv[1])==1);assert(loaded->layer[3].pixels[6450]==0);
    assert(pd_undo(d));assert(pd_dirty(d));assert(pd_redo(d));assert(!pd_dirty(d));
    assert(!pd_save(d,"Z:/no-such-pixel-doodle-directory"));
    PdApp *a=calloc(1,sizeof(*a));uint32_t *bottom=malloc(320*240*4),*top=malloc(400*240*4);assert(a&&bottom&&top);
    app_init(a,256,192,"","DS NTR");app_input(a,true,70,60,0,0);app_input(a,false,0,0,0,0);assert(a->doc.count==1);
    app_input(a,true,90,80,0,PD_PAN);app_input(a,true,100,90,0,0);app_input(a,true,110,100,0,0);app_input(a,false,0,0,0,0);assert(a->doc.count==1);
    app_input(a,true,10,10,0,0);app_input(a,true,100,100,0,0);app_input(a,false,0,0,0,0);assert(a->doc.cursor==0);assert(a->doc.count==1);
    /* Zoom must not cycle on press, repeat on drag, or leak ink. */
    app_input(a,false,0,0,PD_ZOOM,PD_ZOOM);assert(a->zoom==1&&a->zoom_overlay);
    app_input(a,true,32,65,0,PD_ZOOM);assert(a->zoom==2);
    app_input(a,true,32,65,0,PD_ZOOM);assert(a->zoom==2);
    app_input(a,true,110,100,0,0);assert(!a->stroke&&!a->zoom_overlay);
    app_input(a,false,0,0,0,0);assert(a->doc.count==1);
    app_input(a,false,0,0,PD_ZOOM,PD_ZOOM);
    app_input(a,true,32,138,0,PD_ZOOM);assert(a->zoom==1);
    app_input(a,false,0,0,0,0);
    unsigned old_count=a->doc.count;
    app_input(a,false,0,0,PD_TOOLS,0);assert(a->toolbox);
    app_render(a,bottom,top,256,192);ppm(argv[1],"nds-toolbox",bottom,256,192);
    app_input(a,true,40,50,0,0);assert(a->tool==1&&a->toolbox&&a->tool_options_open);
    app_input(a,true,110,100,0,0);assert(!a->stroke&&a->doc.count==old_count);
    app_input(a,false,0,0,0,0);
    app_input(a,false,0,0,PD_TOOLS,0);assert(!a->toolbox);app_input(a,false,0,0,PD_TOOLS,0);tap(a,40,35);assert(a->tool==0&&a->tool_options_open);
    tap(a,110,10);assert(a->page==2);tap(a,235,10);assert(!a->page);
    /* Synthetic composition and native-size snapshots; not user art. */
    for(int i=0;i<100;i++){dot(&a->doc,0,14+i,14,0xff1850b0);dot(&a->doc,1,14+i,14+i,0xffe04444);}
    for(int page=0;page<4;page++){a->page=page;app_render(a,bottom,top,256,192);char name[32];snprintf(name,sizeof(name),"nds-page%d",page);ppm(argv[1],name,bottom,256,192);}
    a->width=320;a->height=240;a->page=0;app_render(a,bottom,top,400,240);ppm(argv[1],"3ds-bottom",bottom,320,240);ppm(argv[1],"3ds-top",top,400,240);
    printf("Core, layers, bounded tile history, alpha, branching, storage roundtrip/corruption and input gesture checks: PASS\nPdDocument=%zu bytes\n",sizeof(*d));
    render_checks(a,bottom,top);incremental_checks();
    free(a);free(bottom);free(top);free(d);free(loaded);return 0;
}

