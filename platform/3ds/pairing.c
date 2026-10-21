#include <3ds.h>
#include <3ds/synchronization.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "app.h"
#include "community_https.h"
#ifdef PD_PRIVATE_BUILD
#include "../../private_webhook.h"
#endif

#define PAIR_ORIGIN "https://pixels.7db.pw"
#define PAIR_RESPONSE_MAX 32768

static char device_code[44];
static char access_token[44];
static char worker_path[384],worker_body[160],worker_response[PAIR_RESPONSE_MAX],worker_error[48];
static char share_png_path[PD_PATH];
static bool share_visible;
static LightLock worker_lock;
static Thread worker;
static bool worker_started,worker_busy,worker_done,worker_ok,worker_is_begin;
static unsigned worker_kind,worker_art_index;
static int worker_status;
static size_t worker_response_size;
static void cleanup_share_file(void){if(share_png_path[0]){char path[PD_PATH];remove(share_png_path);if(snprintf(path,sizeof(path),"%s.tmp",share_png_path)<(int)sizeof(path))remove(path);if(snprintf(path,sizeof(path),"%s.bak",share_png_path)<(int)sizeof(path))remove(path);share_png_path[0]=0;}}
#ifdef PD_PRIVATE_BUILD
static bool webhook_path(char out[384],const char *directory){
    char config[PD_PATH],url[512]={0};if(!pd_join(config,directory,"discord-webhook.txt"))return false;
    FILE *f=fopen(config,"rb");
    if(!f){if(pd_exists(config))return false;int n=snprintf(out,384,"%s",PD_DISCORD_WEBHOOK_PATH);return n>0&&n<384;}
    bool ok=fgets(url,sizeof(url),f)!=NULL&&!ferror(f);if(fgetc(f)!=EOF)ok=false;fclose(f);
    size_t n=strlen(url);while(n&&(url[n-1]=='\r'||url[n-1]=='\n'||url[n-1]==' '||url[n-1]=='\t'))url[--n]=0;
    const char *prefix="https://discord.com/api/webhooks/";size_t plen=strlen(prefix);
    if(n<=plen+30||strncmp(url,prefix,plen))ok=false;
    const char *p=url+plen,*slash=strchr(p,'/');if(!slash||slash==p||!slash[1])ok=false;
    if(ok){for(const char *q=p;*q;q++)if(!((*q>='0'&&*q<='9')||(*q>='A'&&*q<='Z')||(*q>='a'&&*q<='z')||*q=='_'||*q=='-'||*q=='/')){ok=false;break;}
        if(strchr(slash+1,'/'))ok=false;for(const char *q=p;q<slash;q++)if(*q<'0'||*q>'9')ok=false;if(strlen(slash+1)<20)ok=false;}
    if(ok){for(const char *q=p;q<slash;q++)if(*q<'0'||*q>'9')ok=false;}
    if(ok){int m=snprintf(out,384,"%s?wait=true",url+strlen("https://discord.com"));ok=m>0&&m<384;}
    memset(url,0,sizeof(url));return ok;
}
#endif

// Main-thread response storage must not consume the 3DS default stack.
// Kept separate from worker_response so a new worker cannot overwrite parsing.
static char tick_response[PAIR_RESPONSE_MAX];
static uint32_t credential_sequence;
#ifdef PD_PRIVATE_BUILD
static void pd_discord_share_start(PdApp *app,bool visible);
static void pd_discord_share_tick(PdApp *app);
#endif
static void pd_gallery_list(PdApp *app,unsigned mode,unsigned offset);
static void pd_gallery_favorite(PdApp *app,unsigned item,bool favorite);
static void clear_gallery_cache(PdApp *app);

static void set_message(PdApp *app,const char *message){
    snprintf(app->pairing_message,sizeof(app->pairing_message),"%s",message);
    app->redraw=true;
}

static bool json_string(const char *json,const char *key,char *out,size_t cap){
    char needle[40];snprintf(needle,sizeof(needle),"\"%s\":\"",key);
    const char *p=strstr(json,needle);if(!p)return false;p+=strlen(needle);size_t n=0;
    while(*p&&*p!='"'){
        unsigned char c=(unsigned char)*p++;
        if(c=='\\'){
            c=(unsigned char)*p++;
            if(c=='"'||c=='\\'||c=='/'){}
            else if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';else return false;
        }
        if(c<0x20||n+1>=cap)return false;out[n++]=(char)c;
    }
    if(*p!='"')return false;out[n]=0;return true;
}

static unsigned json_number(const char *json,const char *key,unsigned fallback){
    char needle[40];snprintf(needle,sizeof(needle),"\"%s\":",key);const char *p=strstr(json,needle);
    if(!p)return fallback;p+=strlen(needle);if(*p<'0'||*p>'9')return fallback;
    unsigned n=0;while(*p>='0'&&*p<='9'){if(n>1000000)return fallback;n=n*10+(unsigned)(*p++-'0');}return n;
}

static bool approved_scopes(const char *json){
    const char *p=strstr(json,"\"scopes\":[");if(!p)return false;p+=strlen("\"scopes\":[");bool read=false,favorites=false,submit=false;
    for(;;){while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')p++;if(*p==']')return read;if(*p++!='\"')return false;const char *start=p;while(*p&&*p!='\"')p++;if(!*p)return false;size_t n=(size_t)(p-start);p++;if(n==sizeof("gallery:read")-1&&!strncmp(start,"gallery:read",n)){if(read)return false;read=true;}else if(n==sizeof("favorites:write")-1&&!strncmp(start,"favorites:write",n)){if(favorites)return false;favorites=true;}else if(n==sizeof("artworks:submit")-1&&!strncmp(start,"artworks:submit",n)){if(submit)return false;submit=true;}else return false;while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')p++;if(*p==']')return read;if(*p++!=',')return false;}
}

static void request_worker(void *unused){
    (void)unused;unsigned char response[PAIR_RESPONSE_MAX]={0};char error[48]={0};
    bool ok=false;int status=0;size_t received=0;
    if(worker_kind==1||worker_kind==2)ok=pd_https_get_public(worker_path,response,sizeof(response),&received,&status,error,sizeof(error));
    else if(worker_kind==3||worker_kind==4){if(access_token[0])ok=pd_https_get(worker_path,access_token,response,sizeof(response),&received,&status,error,sizeof(error));else{ok=false;snprintf(error,sizeof(error),"DEVICE GRANT REQUIRED; RE-PAIR");}}
    else if(worker_kind==5){ok=pd_https_post_bearer(worker_path,access_token,worker_body,(char*)response,sizeof(response),&status,error,sizeof(error));if(ok)received=strlen((char*)response);}
    else if(worker_kind==6){
        FILE *f=fopen(share_png_path,"rb");unsigned char *png=NULL,*body=NULL;size_t png_size=0,body_size=0;
        if(f&&fseek(f,0,SEEK_END)==0){long end=ftell(f);if(end>0&&end<=2*1024*1024&&fseek(f,0,SEEK_SET)==0){png_size=(size_t)end;png=(unsigned char*)malloc(png_size);if(png&&fread(png,1,png_size,f)==png_size){const char *prefix="--PDShareBoundary7db\r\nContent-Disposition: form-data; name=\"payload_json\"\r\nContent-Type: application/json\r\n\r\n{\"content\":\"Pixel Doodle sprite\"}\r\n--PDShareBoundary7db\r\nContent-Disposition: form-data; name=\"files[0]\"; filename=\"PixelDoodle.png\"\r\nContent-Type: image/png\r\n\r\n";const char *suffix="\r\n--PDShareBoundary7db--\r\n";size_t a=strlen(prefix),b=strlen(suffix);body_size=a+png_size+b;body=(unsigned char*)malloc(body_size);if(body){memcpy(body,prefix,a);memcpy(body+a,png,png_size);memcpy(body+a+png_size,suffix,b);char type[]="multipart/form-data; boundary=PDShareBoundary7db";ok=pd_https_post_multipart(worker_path,body,body_size,type,&status,error,sizeof(error));if(ok)received=0;}}}}
        if(f)fclose(f);if(png){memset(png,0,png_size);free(png);}if(body){memset(body,0,body_size);free(body);}if(!ok&&!error[0])snprintf(error,sizeof(error),"Could not prepare sprite upload");
    }
    else{ok=pd_https_post(worker_path,worker_body,(char*)response,sizeof(response),error,sizeof(error));if(ok)received=strlen((char*)response);}
    if(worker_kind&&ok&&status!=200){ok=false;if(worker_kind==6)snprintf(error,sizeof(error),"Discord rejected the sprite (HTTP %d)",status);else if(status==401)snprintf(error,sizeof(error),"GRANT EXPIRED OR REVOKED; PAIR AGAIN");else if(status==403)snprintf(error,sizeof(error),"GRANT NEEDS MORE SCOPES; RE-PAIR");else snprintf(error,sizeof(error),"GALLERY HTTP %d",status);}
    LightLock_Lock(&worker_lock);if(received>=sizeof(worker_response))received=sizeof(worker_response)-1;memcpy(worker_response,response,received);worker_response[received]=0;worker_response_size=received;worker_status=status;snprintf(worker_error,sizeof(worker_error),"%s",error);worker_ok=ok;worker_done=true;LightLock_Unlock(&worker_lock);
}
static bool request_start(const char *path,const char *body,bool begin){
    if(worker_busy)return false;
    snprintf(worker_path,sizeof(worker_path),"%s",path);snprintf(worker_body,sizeof(worker_body),"%s",body);
    worker_is_begin=begin;worker_kind=0;worker_done=false;worker_ok=false;worker_response[0]=worker_error[0]=0;worker_response_size=0;
    worker=threadCreate(request_worker,NULL,96*1024,0x18,-2,false);
    if(!worker)return false;worker_started=true;worker_busy=true;return true;
}
static bool request_gallery(const char *path,unsigned kind,unsigned art_index){if(worker_busy)return false;snprintf(worker_path,sizeof(worker_path),"%s",path);worker_body[0]=0;worker_is_begin=false;worker_kind=kind;worker_art_index=art_index;worker_done=false;worker_ok=false;worker_response[0]=worker_error[0]=0;worker_response_size=0;worker=threadCreate(request_worker,NULL,128*1024,0x18,-2,false);if(!worker)return false;worker_started=true;worker_busy=true;return true;}
static bool request_favorite(const char *path,const char *body,unsigned item){if(worker_busy||!access_token[0])return false;snprintf(worker_path,sizeof(worker_path),"%s",path);snprintf(worker_body,sizeof(worker_body),"%s",body);worker_is_begin=false;worker_kind=5;worker_art_index=item;worker_done=false;worker_ok=false;worker_response[0]=worker_error[0]=0;worker_response_size=0;worker=threadCreate(request_worker,NULL,128*1024,0x18,-2,false);if(!worker)return false;worker_started=true;worker_busy=true;return true;}
static bool request_take(bool *begin,unsigned *kind,unsigned *art_index,char *response,size_t cap,size_t *size,int *status,char *error,size_t error_cap){
    LightLock_Lock(&worker_lock);bool done=worker_done;LightLock_Unlock(&worker_lock);if(!done)return false;
    if(worker_started){if(R_FAILED(threadJoin(worker,1000000000ULL)))return false;threadFree(worker);worker_started=false;}
    LightLock_Lock(&worker_lock);worker_busy=false;*begin=worker_is_begin;*kind=worker_kind;*art_index=worker_art_index;bool ok=worker_ok;*status=worker_status;worker_status=0;*size=worker_response_size;if(*size>=cap)*size=cap-1;memcpy(response,worker_response,*size);response[*size]=0;
    snprintf(error,error_cap,"%s",worker_error);memset(worker_response,0,sizeof(worker_response));memset(worker_error,0,sizeof(worker_error));worker_response_size=0;LightLock_Unlock(&worker_lock);return ok;
}

static uint32_t crc32_bytes(const uint8_t *data,size_t n){
    uint32_t c=0xffffffffu;for(size_t i=0;i<n;i++){c^=data[i];for(unsigned b=0;b<8;b++)c=(c>>1)^((c&1)?0xedb88320u:0);}return c^0xffffffffu;
}

static bool save_credential(const char *token){
    uint8_t record[55];if(++credential_sequence==0)++credential_sequence;memcpy(record,"PDT1",4);
    record[4]=(uint8_t)credential_sequence;record[5]=(uint8_t)(credential_sequence>>8);record[6]=(uint8_t)(credential_sequence>>16);record[7]=(uint8_t)(credential_sequence>>24);
    memcpy(record+8,token,43);uint32_t sum=crc32_bytes(record,51);
    record[51]=(uint8_t)sum;record[52]=(uint8_t)(sum>>8);record[53]=(uint8_t)(sum>>16);record[54]=(uint8_t)(sum>>24);
    unsigned slot=credential_sequence&1;char temp[80],target[80];snprintf(temp,sizeof(temp),"sdmc:/3ds/PixelDoodle/device-credential%u.tmp",slot);snprintf(target,sizeof(target),"sdmc:/3ds/PixelDoodle/device-credential%u.bin",slot);
    FILE *f=fopen(temp,"wb");if(!f)return false;
    bool ok=fwrite(record,1,sizeof(record),f)==sizeof(record);if(fflush(f)!=0)ok=false;if(fclose(f)!=0)ok=false;
    if(!ok){remove(temp);return false;}
    remove(target);if(rename(temp,target)!=0){remove(temp);return false;}
    return true;
}

static bool read_credential(unsigned slot,uint8_t record[55]){
    char path[80];snprintf(path,sizeof(path),"sdmc:/3ds/PixelDoodle/device-credential%u.bin",slot);FILE *f=fopen(path,"rb");if(!f)return false;
    bool ok=fread(record,1,55,f)==55&&fgetc(f)==EOF;fclose(f);
    uint32_t sum=(uint32_t)record[51]|((uint32_t)record[52]<<8)|((uint32_t)record[53]<<16)|((uint32_t)record[54]<<24);
    ok=ok&&!memcmp(record,"PDT1",4)&&crc32_bytes(record,51)==sum;
    for(unsigned i=0;ok&&i<43;i++)if(!((record[8+i]>='A'&&record[8+i]<='Z')||(record[8+i]>='a'&&record[8+i]<='z')||(record[8+i]>='0'&&record[8+i]<='9')||record[8+i]=='_'||record[8+i]=='-'))ok=false;
    return ok;
}

static bool load_credential(void){
    uint8_t a[55]={0},b[55]={0};bool va=read_credential(0,a),vb=read_credential(1,b);if(!va&&!vb)return false;
    uint8_t *record=va&&(!vb||(int32_t)(((uint32_t)a[4]|((uint32_t)a[5]<<8)|((uint32_t)a[6]<<16)|((uint32_t)a[7]<<24))-((uint32_t)b[4]|((uint32_t)b[5]<<8)|((uint32_t)b[6]<<16)|((uint32_t)b[7]<<24)))>0)?a:b;
    credential_sequence=(uint32_t)record[4]|((uint32_t)record[5]<<8)|((uint32_t)record[6]<<16)|((uint32_t)record[7]<<24);
    memcpy(access_token,record+8,43);access_token[43]=0;memset(a,0,sizeof(a));memset(b,0,sizeof(b));return true;
}
static void clear_credential(void){for(unsigned i=0;i<2;i++){char path[80],temp[80];snprintf(path,sizeof(path),"sdmc:/3ds/PixelDoodle/device-credential%u.bin",i);snprintf(temp,sizeof(temp),"sdmc:/3ds/PixelDoodle/device-credential%u.tmp",i);remove(path);remove(temp);}memset(access_token,0,sizeof(access_token));}

void pd_pairing_init(PdApp *app){
    LightLock_Init(&worker_lock);
    app->online_begin=pd_pairing_begin;
    app->online_list=pd_gallery_list;
    app->online_favorite=pd_gallery_favorite;
    app->online_tick=pd_pairing_tick;
#ifdef PD_PRIVATE_BUILD
    app->share_current=pd_discord_share_start;
    app->share_tick=pd_discord_share_tick;
#endif
    if(load_credential()){app->pairing_state=5;set_message(app,"SAVED GRANT MAY EXPIRE OR BE REVOKED");}
}

void pd_pairing_exit(void){bool safe=true;if(worker_started){safe=R_SUCCEEDED(threadJoin(worker,65ULL*1000000000ULL));if(safe){threadFree(worker);worker_started=false;worker_busy=false;}}if(safe)pd_https_shutdown();memset(worker_body,0,sizeof(worker_body));memset(worker_response,0,sizeof(worker_response));memset(device_code,0,sizeof(device_code));memset(access_token,0,sizeof(access_token));}

void pd_pairing_begin(PdApp *app){
    memset(device_code,0,sizeof(device_code));app->pairing_user_code[0]=0;app->pairing_state=3;
    app->pairing_state=6;set_message(app,"CONNECTING OVER VERIFIED HTTPS");
    char body[128];
    snprintf(body,sizeof(body),"{\"label\":\"Nintendo 3DS\",\"requestedScopes\":[\"gallery:read\",\"favorites:write\",\"artworks:submit\"]}");
    if(!request_start("/api/device/pair/begin",body,true)){app->pairing_state=3;set_message(app,"NETWORK WORKER BUSY; RETRY");}
}

static void pd_gallery_list(PdApp *app,unsigned mode,unsigned offset){
    clear_gallery_cache(app);char path[96];unsigned kind=0;if(mode==1){kind=1;snprintf(path,sizeof(path),"/api/gallery?limit=6&offset=%u&compact=1",offset);}else if(mode==2){kind=3;snprintf(path,sizeof(path),"/api/device/favorites?limit=6&offset=%u&compact=1",offset);}else if(mode==3){kind=4;snprintf(path,sizeof(path),"/api/device/uploads?offset=%u",offset);}else return;
    if(!request_gallery(path,kind,UINT32_MAX)){snprintf(app->community_status,sizeof(app->community_status),access_token[0]?"NETWORK BUSY; RETRY":"PAIR TO ACCESS THIS PAGE");app->community_loading=false;app->redraw=true;}
}
static void pd_gallery_favorite(PdApp *app,unsigned item,bool favorite){if(item>=app->community_page.count||!access_token[0]){app->community_action_busy=false;snprintf(app->community_status,sizeof(app->community_status),"PAIR WITH FAVORITES SCOPE FIRST");app->redraw=true;return;}const char *id=app->community_page.items[item].id;char path[112],body[24];snprintf(path,sizeof(path),"/api/device/favorites/%s",id);snprintf(body,sizeof(body),"{\"favorite\":%s}",favorite?"true":"false");if(!request_favorite(path,body,item)){app->community_action_busy=false;snprintf(app->community_status,sizeof(app->community_status),"NETWORK BUSY; RETRY");app->redraw=true;}}
static bool gallery_thumbnail(PdApp *app,unsigned index){
    if(index>=app->community_page.count)return false;const char *id=app->community_page.items[index].id;if(!*id||strlen(id)>64)return false;
    for(const char *p=id;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'))return false;
    char path[96];snprintf(path,sizeof(path),"/thumbs/%s.png",id);return request_gallery(path,2,index);
}
static bool save_gallery_thumbnail(PdApp *app,unsigned index,const unsigned char *data,size_t size){
    if(index>=app->community_page.count||!data||!size||size>=PAIR_RESPONSE_MAX)return false;char directory[PD_PATH],target[PD_PATH],temp[PD_PATH],name[80];
    if(!pd_join(directory,app->directory,"community-cache")||(!pd_exists(directory)&&!pd_mkdir(directory)))return false;
    snprintf(name,sizeof(name),"%s.png",app->community_page.items[index].id);if(!pd_join(target,directory,name))return false;snprintf(name,sizeof(name),"%s.png.tmp",app->community_page.items[index].id);if(!pd_join(temp,directory,name))return false;
    FILE *f=fopen(temp,"wb");if(!f)return false;bool ok=fwrite(data,1,size,f)==size;if(fflush(f)!=0)ok=false;if(fclose(f)!=0)ok=false;
    if(ok){remove(target);if(rename(temp,target)!=0)ok=false;}if(!ok){remove(temp);return false;}
    char error[64];if(!pd_preview_file_profile(target,app->file_preview,1,error)){remove(target);return false;}
    for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++)app->gallery_thumbs[index][y*32+x]=app->file_preview[(y*2)*64+x*2];
    app->gallery_ready[index]=true;return true;
}
static void clear_gallery_cache(PdApp *app){char dir[PD_PATH];PdListing items;if(!pd_join(dir,app->directory,"community-cache"))return;if(!pd_exists(dir))pd_mkdir(dir);if(!pd_list(dir,&items,app->community_status))return;for(unsigned i=0;i<items.count;i++){const char *n=items.entries[i].name;size_t z=strlen(n);if(items.entries[i].directory||z<5)continue;size_t cut=!strcmp(n+z-4,".png")?4:(z>=8&&!strcmp(n+z-8,".png.tmp")?8:0);if(!cut)continue;bool slug=true;for(size_t k=0;k<z-cut;k++)if(!((n[k]>='a'&&n[k]<='z')||(n[k]>='0'&&n[k]<='9')||n[k]=='-'))slug=false;if(slug){char p[PD_PATH];if(pd_join(p,dir,n))remove(p);}}}

static void pairing_tick_impl(PdApp *app){
    char *response=tick_response;char error[48],value[96];bool begin=false;unsigned kind=0,art_index=0;size_t response_size=0;int http_status=0;
    if(worker_busy){
        bool ok=request_take(&begin,&kind,&art_index,response,sizeof(tick_response),&response_size,&http_status,error,sizeof(error));if(!ok){if(worker_busy)return;
            if(kind==3||kind==4||kind==5){if(http_status==401){clear_credential();app->pairing_state=0;set_message(app,"GRANT EXPIRED OR REVOKED; PAIR AGAIN");}else if(http_status==403){app->pairing_state=5;set_message(app,"PAIR AGAIN TO APPROVE THESE SCOPES");}}
            if(kind==6){cleanup_share_file();memset(worker_path,0,sizeof(worker_path));snprintf(app->notice,sizeof(app->notice),"%.63s",error[0]?error:"DISCORD UPLOAD FAILED");app->redraw=true;return;}
            if(kind){app->community_loading=false;app->community_action_busy=false;app->community_loaded=kind==2&&app->community_loaded;snprintf(app->community_status,sizeof(app->community_status),"%.60s",error[0]?error:"ONLINE GALLERY REQUEST FAILED");app->redraw=true;return;}
            if(begin){app->pairing_state=3;memset(device_code,0,sizeof(device_code));set_message(app,error[0]?error:"PAIRING REQUEST FAILED");}
            else{app->pairing_poll_at=app->now_ms+15000;set_message(app,error[0]?error:"NETWORK ERROR; WILL RETRY SECURELY");}return;}
        if(kind==1||kind==3||kind==4){if(!pd_community_parse_page(response,response_size,&app->community_page)){app->community_loading=false;app->community_loaded=false;snprintf(app->community_status,sizeof(app->community_status),"INVALID COMMUNITY RESPONSE");}
            else{memset(app->gallery_ready,0,sizeof(app->gallery_ready));app->community_loading=false;app->community_loaded=true;app->community_status[0]=0;app->selected=-1;app->preview_attempted=false;if(kind!=4&&app->community_page.count&&!gallery_thumbnail(app,0)){snprintf(app->community_status,sizeof(app->community_status),"THUMBNAIL QUEUE BUSY");}}app->redraw=true;return;}
        if(kind==2){if(!save_gallery_thumbnail(app,art_index,(unsigned char*)response,response_size)){snprintf(app->community_status,sizeof(app->community_status),"THUMBNAIL UNAVAILABLE");}else if(art_index+1<app->community_page.count&&!gallery_thumbnail(app,art_index+1)){snprintf(app->community_status,sizeof(app->community_status),"THUMBNAIL QUEUE BUSY");}app->redraw=true;return;}
        if(kind==5){app->community_action_busy=false;if(art_index<app->community_page.count&&strstr(response,"\"favorite\":true")){app->community_page.items[art_index].favorite=true;snprintf(app->community_status,sizeof(app->community_status),"ADDED TO FAVORITES");}else if(art_index<app->community_page.count&&strstr(response,"\"favorite\":false")){app->community_page.items[art_index].favorite=false;snprintf(app->community_status,sizeof(app->community_status),"REMOVED FROM FAVORITES");if(app->community_mode==2)pd_gallery_list(app,2,app->gallery_page*PD_COMMUNITY_PAGE);}else snprintf(app->community_status,sizeof(app->community_status),"INVALID FAVORITE RESPONSE");app->redraw=true;return;}
        if(kind==6){cleanup_share_file();memset(worker_path,0,sizeof(worker_path));snprintf(app->notice,sizeof(app->notice),"%s",share_visible?"VISIBLE SPRITE SENT TO DISCORD":"ACTIVE LAYER SENT TO DISCORD");app->redraw=true;return;}
        if(begin){
            if(!json_string(response,"deviceCode",device_code,sizeof(device_code))||strlen(device_code)!=43||
               !json_string(response,"userCode",value,sizeof(value))||strlen(value)!=9||
               !json_string(response,"verificationUri",value+32,sizeof(value)-32)||strcmp(value+32,"https://pixels.7db.pw/#account")){
                memset(device_code,0,sizeof(device_code));app->pairing_state=3;set_message(app,"INVALID PAIR RESPONSE");return;
            }
            json_string(response,"userCode",app->pairing_user_code,sizeof(app->pairing_user_code));
            app->pairing_state=1;app->pairing_poll_at=app->now_ms+5000;set_message(app,"CODE EXPIRES IN 10 MINUTES; APPROVE ON WEBSITE");return;
        }
        if(json_string(response,"error",value,sizeof(value))){
            if(!strcmp(value,"authorization_pending")){app->pairing_poll_at=app->now_ms+5000;set_message(app,"WAITING FOR WEBSITE APPROVAL");return;}
            if(!strcmp(value,"slow_down")){unsigned s=json_number(response,"interval",10);if(s<5)s=5;if(s>60)s=60;app->pairing_poll_at=app->now_ms+s*1000;set_message(app,"WAITING FOR WEBSITE APPROVAL");return;}
            app->pairing_state=3;memset(device_code,0,sizeof(device_code));set_message(app,!strcmp(value,"access_denied")?"DEVICE REQUEST WAS DENIED":"PAIRING CODE EXPIRED; START AGAIN");return;
        }
        if(!json_string(response,"accessToken",access_token,sizeof(access_token))||strlen(access_token)!=43||!approved_scopes(response)){
            app->pairing_state=3;memset(device_code,0,sizeof(device_code));memset(access_token,0,sizeof(access_token));set_message(app,"SERVER RETURNED AN INVALID DEVICE GRANT");return;
        }
        if(!save_credential(access_token)){app->pairing_state=3;memset(access_token,0,sizeof(access_token));memset(device_code,0,sizeof(device_code));set_message(app,"CREDENTIAL STORAGE FAILED; DEVICE NOT CONNECTED");return;}
        app->pairing_state=2;memset(device_code,0,sizeof(device_code));set_message(app,"CONNECTED; ACCESS CAN BE REVOKED ON THE WEBSITE");return;
    }
    if(app->pairing_state!=1||!device_code[0]||(int32_t)(app->now_ms-app->pairing_poll_at)<0)return;
    char body[96];snprintf(body,sizeof(body),"{\"deviceCode\":\"%s\"}",device_code);
    if(!request_start("/api/device/pair/poll",body,false)){app->pairing_poll_at=app->now_ms+15000;set_message(app,"NETWORK WORKER BUSY; WILL RETRY");}
}

void pd_pairing_tick(PdApp *app){
    pairing_tick_impl(app);
    memset(tick_response,0,sizeof(tick_response));
}

#ifdef PD_PRIVATE_BUILD
static void pd_discord_share_start(PdApp *app,bool visible){
    if(worker_busy){snprintf(app->notice,sizeof(app->notice),"NETWORK BUSY; TRY AGAIN");app->redraw=true;return;}
    char directory[PD_PATH],error[64]={0};if(!app->directory[0]||!pd_join(directory,app->directory,"private")){snprintf(app->notice,sizeof(app->notice),"SAVED SD CARD REQUIRED");app->redraw=true;return;}
    mkdir(directory,0777);if(!webhook_path(worker_path,directory)){snprintf(app->notice,sizeof(app->notice),"WEBHOOK OVERRIDE INVALID");app->redraw=true;return;}
    if(!pd_join(share_png_path,directory,"share.png")){memset(worker_path,0,sizeof(worker_path));snprintf(app->notice,sizeof(app->notice),"SPRITE PATH TOO LONG");app->redraw=true;return;}
    cleanup_share_file();if(!pd_join(share_png_path,directory,"share.png")||!pd_write_file(&app->doc,share_png_path,visible?2:3,false,error)){memset(worker_path,0,sizeof(worker_path));share_png_path[0]=0;snprintf(app->notice,sizeof(app->notice),"%.63s",error[0]?error:"SPRITE EXPORT FAILED");app->redraw=true;return;}
    worker_body[0]=0;worker_is_begin=false;worker_kind=6;worker_art_index=0;worker_done=false;worker_ok=false;worker_status=0;worker_response[0]=worker_error[0]=0;worker_response_size=0;share_visible=visible;
    worker=threadCreate(request_worker,NULL,128*1024,0x18,-2,false);if(!worker){cleanup_share_file();memset(worker_path,0,sizeof(worker_path));snprintf(app->notice,sizeof(app->notice),"NETWORK WORKER UNAVAILABLE");app->redraw=true;return;}
    worker_started=true;worker_busy=true;snprintf(app->notice,sizeof(app->notice),"UPLOADING SPRITE TO DISCORD");app->redraw=true;
}
static void pd_discord_share_tick(PdApp *app){if(worker_busy)pd_pairing_tick(app);}
#endif
