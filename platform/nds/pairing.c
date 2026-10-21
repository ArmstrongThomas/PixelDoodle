#include <nds.h>
#include <dswifi9.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/platform_time.h>
#include "pixel_doodle_mbedtls_config.h"
#include "seed_store.h"
#include "cacert_bin.h"
#include "app.h"
#include <errno.h>
#ifdef PD_PRIVATE_BUILD
#include "../../private_webhook.h"
#endif
#include "dns_packet.h"

#define PAIR_HOST "pixels.7db.pw"
#define PAIR_RESPONSE_CAP 16384
#define PAIR_BODY_CAP 256
enum { P_IDLE, P_WIFI_INIT, P_WIFI, P_DNS, P_TCP, P_TLS_SETUP, P_TLS, P_WRITE, P_BODY, P_READ };
static int stage,fd=-1,dns_fd=-1;
static bool wifi_ready,contexts;
static time_t deadline;
static PdApp *owner;
static char private_dir[300],device_code[44],access_token[44];
static char request[1024],response[PAIR_RESPONSE_CAP],remote_host[254];
static size_t request_size,sent,received,body_at,body_size;
static uint32_t credential_sequence;
static bool request_is_begin,request_is_gallery,request_is_thumbnail,request_is_favorite;
static bool request_is_share;
static bool share_visible;
static FILE *share_file;
static char share_png_path[PD_PATH],webhook_request_path[384];
static char upload_prefix[384],upload_suffix[64];
static unsigned char upload_chunk[4096];
static size_t upload_prefix_size,upload_prefix_sent,upload_suffix_size,upload_suffix_sent;
static size_t upload_png_size,upload_png_loaded,upload_chunk_size,upload_chunk_sent;
static unsigned gallery_thumbnail_index,gallery_request_kind;
static uint32_t due;
static struct sockaddr_in dns_servers[2];
static unsigned dns_server_count,dns_server_index,dns_attempts;
static uint16_t dns_id;
static uint32_t dns_retry_at,dns_server_ip;
static uint8_t dns_query[64];
static size_t dns_query_size;
static unsigned poll_seconds=5;
static void clear_gallery_cache(PdApp *app);
#ifdef PD_PRIVATE_BUILD
static void share_start(PdApp *app,bool visible);
static void share_tick(PdApp *app);
#endif
static mbedtls_ssl_context ssl;
static mbedtls_ssl_config config;
static mbedtls_x509_crt ca;
static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context rng;

mbedtls_ms_time_t mbedtls_ms_time(void){return (mbedtls_ms_time_t)time(NULL)*1000;}
static void notice(const char *s){if(owner){if(request_is_share)snprintf(owner->notice,sizeof(owner->notice),"%s",s);else snprintf(owner->pairing_message,sizeof(owner->pairing_message),"%s",s);owner->redraw=true;}}
static void wipe(void *p,size_t n){mbedtls_platform_zeroize(p,n);}
static bool make_path(char *out,size_t cap,const char *name){int n=snprintf(out,cap,"%s/%s",private_dir,name);return n>0&&(size_t)n<cap;}
static bool credential_write(const char *token){
    uint8_t record[55];char temp[360],target[360];if(!make_path(temp,sizeof(temp),"grant.new"))return false;
    if(++credential_sequence==0)++credential_sequence;memcpy(record,"PDT1",4);
    record[4]=(uint8_t)credential_sequence;record[5]=(uint8_t)(credential_sequence>>8);record[6]=(uint8_t)(credential_sequence>>16);record[7]=(uint8_t)(credential_sequence>>24);
    memcpy(record+8,token,43);uint32_t c=0xffffffffu;for(unsigned i=0;i<51;i++){c^=record[i];for(unsigned b=0;b<8;b++)c=(c>>1)^((c&1)?0xedb88320u:0);}c^=0xffffffffu;
    record[51]=(uint8_t)c;record[52]=(uint8_t)(c>>8);record[53]=(uint8_t)(c>>16);record[54]=(uint8_t)(c>>24);
    FILE *f=fopen(temp,"wb");if(!f){wipe(record,sizeof(record));return false;}bool ok=fwrite(record,1,sizeof(record),f)==sizeof(record)&&fflush(f)==0&&fsync(fileno(f))==0; if(fclose(f))ok=false;wipe(record,sizeof(record));
    unsigned slot=credential_sequence&1;char name[32];snprintf(name,sizeof(name),"grant%u.bin",slot);if(!make_path(target,sizeof(target),name))ok=false;
    if(!ok){remove(temp);return false;}remove(target);if(rename(temp,target)){remove(temp);return false;}return true;
}
static bool credential_read(unsigned slot,uint8_t record[55]){
    char path[360],name[32];snprintf(name,sizeof(name),"grant%u.bin",slot);if(!make_path(path,sizeof(path),name))return false;
    FILE *f=fopen(path,"rb");if(!f)return false;bool ok=fread(record,1,55,f)==55&&fgetc(f)==EOF&&!ferror(f);if(fclose(f))ok=false;
    uint32_t expected=(uint32_t)record[51]|((uint32_t)record[52]<<8)|((uint32_t)record[53]<<16)|((uint32_t)record[54]<<24),c=0xffffffffu;
    for(unsigned i=0;i<51;i++){c^=record[i];for(unsigned b=0;b<8;b++)c=(c>>1)^((c&1)?0xedb88320u:0);}c^=0xffffffffu;ok=ok&&!memcmp(record,"PDT1",4)&&expected==c;
    for(unsigned i=0;ok&&i<43;i++)if(!((record[8+i]>='A'&&record[8+i]<='Z')||(record[8+i]>='a'&&record[8+i]<='z')||(record[8+i]>='0'&&record[8+i]<='9')||record[8+i]=='_'||record[8+i]=='-'))ok=false;return ok;
}
static bool credential_load(void){
    uint8_t a[55]={0},b[55]={0};bool va=credential_read(0,a),vb=credential_read(1,b);if(!va&&!vb)return false;
    uint32_t sa=(uint32_t)a[4]|((uint32_t)a[5]<<8)|((uint32_t)a[6]<<16)|((uint32_t)a[7]<<24),sb=(uint32_t)b[4]|((uint32_t)b[5]<<8)|((uint32_t)b[6]<<16)|((uint32_t)b[7]<<24);
    uint8_t *r=va&&(!vb||(int32_t)(sa-sb)>0)?a:b;credential_sequence=(uint32_t)r[4]|((uint32_t)r[5]<<8)|((uint32_t)r[6]<<16)|((uint32_t)r[7]<<24);memcpy(access_token,r+8,43);access_token[43]=0;wipe(a,sizeof(a));wipe(b,sizeof(b));return true;
}
static void credential_clear(void){char a[360],b[360];if(make_path(a,sizeof(a),"grant0.bin"))remove(a);if(make_path(b,sizeof(b),"grant1.bin"))remove(b);wipe(access_token,sizeof(access_token));}
static bool json_string(const char *json,const char *key,char *out,size_t cap){
    char needle[48];snprintf(needle,sizeof(needle),"\"%s\":\"",key);const char *p=strstr(json,needle);if(!p)return false;p+=strlen(needle);size_t n=0;
    while(*p&&*p!='"'){unsigned char c=(unsigned char)*p++;if(c=='\\'){c=(unsigned char)*p++;if(c=='"'||c=='\\'||c=='/'){}else if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';else return false;}if(c<0x20||n+1>=cap)return false;out[n++]=(char)c;}
    if(*p!='"')return false;out[n]=0;return true;
}
static bool token_format(const char *s){if(strlen(s)!=43)return false;for(unsigned i=0;i<43;i++)if(!((s[i]>='A'&&s[i]<='Z')||(s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9')||s[i]=='_'||s[i]=='-'))return false;return true;}
static unsigned json_number(const char *json,const char *key,unsigned fallback){char needle[48];snprintf(needle,sizeof(needle),"\"%s\":",key);const char *p=strstr(json,needle);if(!p)return fallback;p+=strlen(needle);if(*p<'0'||*p>'9')return fallback;unsigned n=0;while(*p>='0'&&*p<='9'){if(n>100000)return fallback;n=n*10u+(unsigned)(*p++-'0');}return n;}
static bool approved_scopes(const char *json){const char *p=strstr(json,"\"scopes\":[");if(!p)return false;p+=strlen("\"scopes\":[");bool read=false,favorites=false,submit=false;for(;;){while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')p++;if(*p==']')return read;if(*p++!='\"')return false;const char *start=p;while(*p&&*p!='\"')p++;if(!*p)return false;size_t n=(size_t)(p-start);p++;if(n==sizeof("gallery:read")-1&&!strncmp(start,"gallery:read",n)){if(read)return false;read=true;}else if(n==sizeof("favorites:write")-1&&!strncmp(start,"favorites:write",n)){if(favorites)return false;favorites=true;}else if(n==sizeof("artworks:submit")-1&&!strncmp(start,"artworks:submit",n)){if(submit)return false;submit=true;}else return false;while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')p++;if(*p==']')return read;if(*p++!=',')return false;}}
static void close_transport(void){if(fd>=0){closesocket(fd);fd=-1;}if(dns_fd>=0){closesocket(dns_fd);dns_fd=-1;}if(contexts){mbedtls_ssl_free(&ssl);mbedtls_ssl_config_free(&config);mbedtls_x509_crt_free(&ca);mbedtls_ctr_drbg_free(&rng);mbedtls_entropy_free(&entropy);contexts=false;}if(share_file){fclose(share_file);share_file=NULL;}if(share_png_path[0]){char p[PD_PATH];remove(share_png_path);if(snprintf(p,sizeof(p),"%s.tmp",share_png_path)<(int)sizeof(p))remove(p);if(snprintf(p,sizeof(p),"%s.bak",share_png_path)<(int)sizeof(p))remove(p);share_png_path[0]=0;}wipe(request,sizeof(request));wipe(response,sizeof(response));wipe(dns_query,sizeof(dns_query));wipe(upload_prefix,sizeof(upload_prefix));wipe(upload_suffix,sizeof(upload_suffix));wipe(upload_chunk,sizeof(upload_chunk));wipe(webhook_request_path,sizeof(webhook_request_path));dns_query_size=0;request_is_share=false;share_visible=false;remote_host[0]=0;stage=P_IDLE;}
static void transport_error(const char *message);
static bool start_dns(void){
    struct in_addr gateway={0},subnet={0},first={0},second={0};Wifi_GetIPInfo(&gateway,&subnet,&first,&second);dns_server_count=0;dns_server_index=0;dns_attempts=0;
    struct in_addr candidates[2]={first,second};for(unsigned i=0;i<2;i++)if(candidates[i].s_addr){dns_servers[dns_server_count].sin_family=AF_INET;dns_servers[dns_server_count].sin_port=htons(53);dns_servers[dns_server_count].sin_addr=candidates[i];dns_server_count++;}
    if(!dns_server_count)return false;dns_fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(dns_fd<0)return false;int one=1;if(ioctl(dns_fd,FIONBIO,&one)<0){closesocket(dns_fd);dns_fd=-1;return false;}
    if(++dns_id==0)++dns_id;const char *host=remote_host[0]?remote_host:PAIR_HOST;dns_query_size=pd_dns_build_a_query(dns_query,sizeof(dns_query),dns_id,host);if(!dns_query_size){closesocket(dns_fd);dns_fd=-1;return false;}
    dns_retry_at=owner?owner->now_ms:0;stage=P_DNS;notice(remote_host[0]&&!strcmp(remote_host,PAIR_HOST)?"RESOLVING PIXELS.7DB.PW":remote_host[0]?"RESOLVING DISCORD":"RESOLVING PIXELS.7DB.PW");return true;
}
static bool dns_tick(void){
    if(dns_fd<0||dns_server_index>=dns_server_count){transport_error("DNS RESOLUTION FAILED");return false;}
    if((int32_t)(owner->now_ms-dns_retry_at)>=0){if(dns_attempts>=4){dns_server_index++;dns_attempts=0;if(dns_server_index>=dns_server_count){transport_error("DNS RESOLUTION FAILED");return false;}}
        int sent=sendto(dns_fd,dns_query,dns_query_size,0,(struct sockaddr*)&dns_servers[dns_server_index],sizeof(dns_servers[dns_server_index]));if(sent==(int)dns_query_size){dns_attempts++;dns_retry_at=owner->now_ms+1500;}else if(errno!=EAGAIN&&errno!=EWOULDBLOCK){transport_error("DNS REQUEST FAILED");return false;}}
    uint8_t packet[512];struct sockaddr_in from={0};socklen_t from_size=sizeof(from);int n=recvfrom(dns_fd,packet,sizeof(packet),0,(struct sockaddr*)&from,&from_size);if(n<0){if(errno!=EAGAIN&&errno!=EWOULDBLOCK){transport_error("DNS RESPONSE FAILED");return false;}return true;}
    if(from.sin_port!=htons(53)||from.sin_addr.s_addr!=dns_servers[dns_server_index].sin_addr.s_addr||!pd_dns_parse_a_response(packet,(size_t)n,dns_id,&dns_server_ip))return true;
    closesocket(dns_fd);dns_fd=-1;fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0){transport_error("NETWORK SOCKET FAILED");return false;}int one=1;if(ioctl(fd,FIONBIO,&one)<0||setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one))<0){transport_error("NETWORK SOCKET SETUP FAILED");return false;}
    struct sockaddr_in server={0};server.sin_family=AF_INET;server.sin_port=htons(443);server.sin_addr.s_addr=dns_server_ip;int r=connect(fd,(struct sockaddr*)&server,sizeof(server));int e=errno;if(r<0&&e!=EINPROGRESS&&e!=EWOULDBLOCK){transport_error("SERVER CONNECTION FAILED");return false;}stage=r==0?P_TLS_SETUP:P_TCP;notice("DNS READY; CONNECTING TO SERVER");return true;
}
static int entropy_from_seed(void *ctx,unsigned char *out,size_t n){size_t *offset=ctx;char p[360];uint8_t seed[64];if(*offset+n>sizeof(seed)||!make_path(p,sizeof(p),"consuming.bin"))return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;FILE *f=fopen(p,"rb");if(!f)return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;bool ok=fread(seed,1,sizeof(seed),f)==sizeof(seed)&&fgetc(f)==EOF;fclose(f);if(!ok||*offset+n>sizeof(seed)){wipe(seed,sizeof(seed));return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;}memcpy(out,seed+*offset,n);*offset+=n;wipe(seed,sizeof(seed));return 0;}
static int seed_rng(void){
    char seed[360],nextp[360],used[360];uint8_t old[64],next[64];size_t off=0;int r=MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    if(!make_path(seed,sizeof(seed),"seed.bin")||!make_path(nextp,sizeof(nextp),"next.bin")||!make_path(used,sizeof(used),"consuming.bin"))return r;
    if(rename(seed,used))return r;
    FILE *f=fopen(used,"rb");if(!f)return r;bool ok=fread(old,1,sizeof(old),f)==sizeof(old)&&fgetc(f)==EOF;fclose(f);if(!ok){wipe(old,sizeof(old));return r;}
    /* Feed the one-use host-provisioned seed to CTR-DRBG without reusing it. */
    mbedtls_platform_zeroize(old,sizeof(old));
    r=mbedtls_ctr_drbg_seed(&rng,entropy_from_seed,&off,(const unsigned char *)"PixelDoodle NDS seed v1",23);
    if(!r)r=mbedtls_ctr_drbg_random(&rng,next,sizeof(next));
    if(!r){f=fopen(nextp,"wb");if(!f)r=MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;else{ok=fwrite(next,1,sizeof(next),f)==sizeof(next)&&fflush(f)==0&&fsync(fileno(f))==0;if(fclose(f))ok=false;if(!ok||rename(nextp,seed))r=MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;}}
    if(r){wipe(next,sizeof(next));return r;}f=fopen(seed,"rb");if(!f){wipe(next,sizeof(next));return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;}uint8_t check[64];ok=fread(check,1,sizeof(check),f)==sizeof(check)&&fgetc(f)==EOF;fclose(f);ok=ok&&!memcmp(check,next,sizeof(next));wipe(check,sizeof(check));wipe(next,sizeof(next));if(!ok||remove(used))return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;return 0;
}
static int bio_send(void *ctx,const unsigned char *buf,size_t n){(void)ctx;int r=send(fd,buf,n,0);if(r<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return MBEDTLS_ERR_SSL_WANT_WRITE;return r<0?MBEDTLS_ERR_SSL_INTERNAL_ERROR:r;}
static int bio_recv(void *ctx,unsigned char *buf,size_t n){(void)ctx;int r=recv(fd,buf,n,0);if(r<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return MBEDTLS_ERR_SSL_WANT_READ;return r<0?MBEDTLS_ERR_SSL_INTERNAL_ERROR:r;}
static bool retry(int r){return r==MBEDTLS_ERR_SSL_WANT_READ||r==MBEDTLS_ERR_SSL_WANT_WRITE;}
static bool http_content_length(char *headers,unsigned *length){
    char *line=strstr(headers,"\r\n");if(!line)return false;line+=2;
    while(*line&&strncmp(line,"\r\n",2)){char *end=strstr(line,"\r\n");if(!end)return false;
        if((size_t)(end-line)>=15&&!strncasecmp(line,"Content-Length:",15)){char *p=line+15;while(p<end&&(*p==' '||*p=='\t'))p++;if(p==end||*p<'0'||*p>'9')return false;unsigned n=0;while(p<end&&*p>='0'&&*p<='9'){if(n>1000000)return false;n=n*10u+(unsigned)(*p++-'0');}while(p<end&&(*p==' '||*p=='\t'))p++;if(p!=end)return false;*length=n;return true;}line=end+2;
    }return false;
}
static void request_start(bool begin){
    close_transport();received=sent=0;response[0]=request[0]=0;
    char body[PAIR_BODY_CAP],path[64];request_is_begin=begin;request_is_gallery=false;request_is_favorite=false;
    if(begin){snprintf(body,sizeof(body),"{\"label\":\"Nintendo %s\",\"requestedScopes\":[\"gallery:read\",\"favorites:write\",\"artworks:submit\"]}",owner&&owner->platform&&strstr(owner->platform,"DSi")?"DSi":"DS");strcpy(path,"/api/device/pair/begin");}
    else{snprintf(body,sizeof(body),"{\"deviceCode\":\"%s\"}",device_code);strcpy(path,"/api/device/pair/poll");}
    body_size=strlen(body);request_size=(size_t)snprintf(request,sizeof(request),"POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nAccept: application/json\r\nConnection: close\r\nContent-Length: %u\r\n\r\n%s",path,PAIR_HOST,(unsigned)body_size,body);
    wipe(body,sizeof(body));if(request_size>=sizeof(request)){close_transport();notice("PAIRING REQUEST TOO LARGE");return;}
    deadline=time(NULL)+75;if(wifi_ready){if(!start_dns())transport_error("DNS SETUP FAILED");}else{stage=P_WIFI;notice("WAITING FOR WI-FI / DHCP");}
}
static bool gallery_request(const char *path,unsigned kind,unsigned index){if(stage!=P_IDLE)return false;if((kind==3||kind==4)&&!token_format(access_token))return false;close_transport();received=sent=0;response[0]=request[0]=0;request_is_gallery=true;request_is_favorite=false;request_is_thumbnail=kind==2;gallery_thumbnail_index=index;gallery_request_kind=kind;request_is_begin=false;request_size=(size_t)snprintf(request,sizeof(request),"GET %s HTTP/1.1\r\nHost: %s\r\nAccept: image/png, application/json\r\n%s%s%sConnection: close\r\n\r\n",path,PAIR_HOST,(kind>=3?"Authorization: Bearer ":""),(kind>=3?access_token:""),(kind>=3?"\r\n":""));if(request_size>=sizeof(request)){close_transport();return false;}if(!wifi_ready){if(!Wifi_CheckInit()&&!Wifi_InitDefault(INIT_ONLY|WIFI_ATTEMPT_DSI_MODE))return false;Wifi_AutoConnect();}deadline=time(NULL)+75;if(wifi_ready){if(!start_dns())return false;}else stage=P_WIFI;owner->community_loading=true;snprintf(owner->community_status,sizeof(owner->community_status),"CONNECTING TO ONLINE GALLERY");return true;}
static bool gallery_thumb_request(PdApp *app,unsigned index){if(index>=app->community_page.count)return false;const char *id=app->community_page.items[index].id;for(const char *p=id;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'))return false;char path[96];snprintf(path,sizeof(path),"/thumbs/%s.png",id);return gallery_request(path,2,index);}
static bool save_gallery_thumb(PdApp *app,unsigned index,const unsigned char *data,size_t size){if(index>=app->community_page.count||!data||!size)return false;char dir[PD_PATH],target[PD_PATH],temp[PD_PATH],name[80];if(!pd_join(dir,app->directory,"community-cache")||(!pd_exists(dir)&&!pd_mkdir(dir)))return false;snprintf(name,sizeof(name),"%s.png",app->community_page.items[index].id);if(!pd_join(target,dir,name))return false;snprintf(name,sizeof(name),"%s.png.tmp",app->community_page.items[index].id);if(!pd_join(temp,dir,name))return false;FILE *f=fopen(temp,"wb");if(!f)return false;bool ok=fwrite(data,1,size,f)==size&&fflush(f)==0;if(fclose(f))ok=false;if(ok){remove(target);if(rename(temp,target))ok=false;}if(!ok){remove(temp);return false;}char error[64];if(!pd_preview_file_profile(target,app->file_preview,1,error)){remove(target);return false;}for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++)app->gallery_thumbs[index][y*32+x]=app->file_preview[(y*2)*64+x*2];app->gallery_ready[index]=true;return true;}
static void clear_gallery_cache(PdApp *app){char dir[PD_PATH];PdListing items;if(!pd_join(dir,app->directory,"community-cache"))return;if(!pd_exists(dir))pd_mkdir(dir);if(!pd_list(dir,&items,app->community_status))return;for(unsigned i=0;i<items.count;i++){const char *n=items.entries[i].name;size_t z=strlen(n);if(items.entries[i].directory||z<5)continue;size_t cut=!strcmp(n+z-4,".png")?4:(z>=8&&!strcmp(n+z-8,".png.tmp")?8:0);if(!cut)continue;bool slug=true;for(size_t k=0;k<z-cut;k++)if(!((n[k]>='a'&&n[k]<='z')||(n[k]>='0'&&n[k]<='9')||n[k]=='-'))slug=false;if(slug){char p[PD_PATH];if(pd_join(p,dir,n))remove(p);}}}
static void pd_gallery_list(PdApp *app,unsigned mode,unsigned offset){clear_gallery_cache(app);char path[96];unsigned kind=0;if(mode==1){kind=1;snprintf(path,sizeof(path),"/api/gallery?limit=6&offset=%u&compact=1",offset);}else if(mode==2){kind=3;snprintf(path,sizeof(path),"/api/device/favorites?limit=6&offset=%u&compact=1",offset);}else if(mode==3){kind=4;snprintf(path,sizeof(path),"/api/device/uploads?offset=%u",offset);}else return;if(!gallery_request(path,kind,UINT32_MAX)){app->community_loading=false;snprintf(app->community_status,sizeof(app->community_status),token_format(access_token)?"NETWORK BUSY; RETRY":"PAIR TO ACCESS THIS PAGE");app->redraw=true;}}
static void pd_gallery_favorite(PdApp *app,unsigned item,bool favorite){if(item>=app->community_page.count||!token_format(access_token)){app->community_action_busy=false;snprintf(app->community_status,sizeof(app->community_status),"PAIR WITH FAVORITES SCOPE FIRST");app->redraw=true;return;}const char *id=app->community_page.items[item].id;for(const char *p=id;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-')){app->community_action_busy=false;return;}if(stage!=P_IDLE){app->community_action_busy=false;snprintf(app->community_status,sizeof(app->community_status),"NETWORK BUSY; RETRY");return;}if(!wifi_ready){if(!Wifi_CheckInit()&&!Wifi_InitDefault(INIT_ONLY|WIFI_ATTEMPT_DSI_MODE)){app->community_action_busy=false;snprintf(app->community_status,sizeof(app->community_status),"WI-FI INITIALIZATION FAILED");return;}Wifi_AutoConnect();}close_transport();received=sent=0;response[0]=request[0]=0;char body[24];snprintf(body,sizeof(body),"{\"favorite\":%s}",favorite?"true":"false");request_size=(size_t)snprintf(request,sizeof(request),"POST /api/device/favorites/%s HTTP/1.1\r\nHost: %s\r\nAuthorization: Bearer %s\r\nContent-Type: application/json\r\nAccept: application/json\r\nConnection: close\r\nContent-Length: %u\r\n\r\n%s",id,PAIR_HOST,access_token,(unsigned)strlen(body),body);request_is_gallery=false;request_is_favorite=true;gallery_thumbnail_index=item;request_is_begin=false;if(request_size>=sizeof(request)){app->community_action_busy=false;close_transport();return;}deadline=time(NULL)+75;if(wifi_ready){if(!start_dns()){app->community_action_busy=false;close_transport();snprintf(app->community_status,sizeof(app->community_status),"DNS SETUP FAILED");return;}}else stage=P_WIFI;snprintf(app->community_status,sizeof(app->community_status),"UPDATING FAVORITE");}
#ifdef PD_PRIVATE_BUILD
static bool load_webhook_path(char out[384]){
    char config[PD_PATH],url[512]={0};if(!make_path(config,sizeof(config),"discord-webhook.txt"))return false;
    FILE *f=fopen(config,"rb");
    if(!f){if(pd_exists(config))return false;int n=snprintf(out,384,"%s",PD_DISCORD_WEBHOOK_PATH);return n>0&&n<384;}
    bool ok=fgets(url,sizeof(url),f)!=NULL&&!ferror(f);if(fgetc(f)!=EOF)ok=false;fclose(f);
    size_t n=strlen(url);while(n&&(url[n-1]=='\r'||url[n-1]=='\n'||url[n-1]==' '||url[n-1]=='\t'))url[--n]=0;
    const char *prefix="https://discord.com/api/webhooks/";size_t plen=strlen(prefix);
    if(n<=plen+30||strncmp(url,prefix,plen))ok=false;
    const char *p=url+plen,*slash=strchr(p,'/');if(!slash||slash==p||!slash[1])ok=false;
    if(ok){for(const char *q=p;*q;q++)if(!((*q>='0'&&*q<='9')||(*q>='A'&&*q<='Z')||(*q>='a'&&*q<='z')||*q=='_'||*q=='-'||*q=='/')){ok=false;break;}
        if(strchr(slash+1,'/'))ok=false;if(strlen(slash+1)<20)ok=false;}
    if(ok){for(const char *q=p;q<slash;q++)if(*q<'0'||*q>'9')ok=false;}
    if(ok){int m=snprintf(out,384,"%s?wait=true",url+strlen("https://discord.com"));ok=m>0&&m<384;}
    wipe(url,sizeof(url));return ok;
}
static void share_start(PdApp *app,bool visible){
    if(stage!=P_IDLE){snprintf(app->notice,sizeof(app->notice),"NETWORK BUSY; TRY AGAIN");app->redraw=true;return;}
    owner=app;if(!app->directory[0]){snprintf(app->notice,sizeof(app->notice),"SAVED SD CARD REQUIRED");app->redraw=true;return;}
    if(snprintf(private_dir,sizeof(private_dir),"%s/private",app->directory)>=(int)sizeof(private_dir)||!pd_exists(private_dir))pd_mkdir(private_dir);
    if(!load_webhook_path(webhook_request_path)){snprintf(app->notice,sizeof(app->notice),"WEBHOOK OVERRIDE INVALID");app->redraw=true;return;}
    if(!make_path(share_png_path,sizeof(share_png_path),"share.png")){snprintf(app->notice,sizeof(app->notice),"SPRITE PATH TOO LONG");app->redraw=true;return;}
    char p[PD_PATH],error[64]={0};if(snprintf(p,sizeof(p),"%s.tmp",share_png_path)<(int)sizeof(p))remove(p);if(snprintf(p,sizeof(p),"%s.bak",share_png_path)<(int)sizeof(p))remove(p);remove(share_png_path);
    if(!pd_write_file(&app->doc,share_png_path,visible?2:3,false,error)){share_png_path[0]=0;wipe(webhook_request_path,sizeof(webhook_request_path));snprintf(app->notice,sizeof(app->notice),"%.63s",error[0]?error:"SPRITE EXPORT FAILED");app->redraw=true;return;}
    share_file=fopen(share_png_path,"rb");if(!share_file||fseek(share_file,0,SEEK_END)){close_transport();snprintf(app->notice,sizeof(app->notice),"SPRITE FILE COULD NOT BE READ");app->redraw=true;return;}long length=ftell(share_file);if(length<=0||length>2*1024*1024||fseek(share_file,0,SEEK_SET)){close_transport();snprintf(app->notice,sizeof(app->notice),"SPRITE EXCEEDS UPLOAD LIMIT");app->redraw=true;return;}upload_png_size=(size_t)length;
    static const char prefix[]="--PDShareBoundary7db\r\nContent-Disposition: form-data; name=\"payload_json\"\r\nContent-Type: application/json\r\n\r\n{\"content\":\"Pixel Doodle sprite\"}\r\n--PDShareBoundary7db\r\nContent-Disposition: form-data; name=\"files[0]\"; filename=\"PixelDoodle.png\"\r\nContent-Type: image/png\r\n\r\n";
    static const char suffix[]="\r\n--PDShareBoundary7db--\r\n";snprintf(upload_prefix,sizeof(upload_prefix),"%s",prefix);snprintf(upload_suffix,sizeof(upload_suffix),"%s",suffix);upload_prefix_size=strlen(upload_prefix);upload_suffix_size=strlen(upload_suffix);upload_prefix_sent=upload_suffix_sent=upload_png_loaded=upload_chunk_size=upload_chunk_sent=0;
    request_is_share=true;share_visible=visible;request_is_gallery=request_is_favorite=request_is_begin=false;snprintf(remote_host,sizeof(remote_host),"discord.com");received=sent=0;response[0]=request[0]=0;
    size_t content_length=upload_prefix_size+upload_png_size+upload_suffix_size;request_size=(size_t)snprintf(request,sizeof(request),"POST %s HTTP/1.1\r\nHost: discord.com\r\nAccept: application/json\r\nContent-Type: multipart/form-data; boundary=PDShareBoundary7db\r\nConnection: close\r\nContent-Length: %lu\r\n\r\n",webhook_request_path,(unsigned long)content_length);wipe(webhook_request_path,sizeof(webhook_request_path));if(request_size>=sizeof(request)){transport_error("SPRITE REQUEST TOO LARGE");return;}
    deadline=time(NULL)+120;if(wifi_ready){if(!start_dns())transport_error("DISCORD DNS SETUP FAILED");}else{stage=P_WIFI_INIT;notice("STARTING WI-FI");}
}
static void share_tick(PdApp *app){if(request_is_share)pd_pairing_tick(app);}
#endif
static void transport_error(const char *message){bool gallery=request_is_gallery,favorite=request_is_favorite,share=request_is_share;close_transport();if(owner){if(share){snprintf(owner->notice,sizeof(owner->notice),"%.63s",message);owner->redraw=true;}else if(gallery){owner->community_loading=false;snprintf(owner->community_status,sizeof(owner->community_status),"%.60s",message);owner->redraw=true;}else if(favorite){owner->community_action_busy=false;snprintf(owner->community_status,sizeof(owner->community_status),"%.60s",message);owner->redraw=true;}else{owner->pairing_state=3;memset(device_code,0,sizeof(device_code));notice(message);}}}
static void response_ready(unsigned status){
    char *body=response+body_at;body[body_size]=0;char value[160];bool begin=request_is_begin;
    if(status!=200){if(request_is_share){char text[64];snprintf(text,sizeof(text),"DISCORD REJECTED SPRITE (HTTP %u)",status);transport_error(text);return;}if(request_is_gallery||request_is_favorite){if(status==401){credential_clear();owner->pairing_state=0;notice("GRANT EXPIRED OR REVOKED; PAIR AGAIN");}else if(status==403){owner->pairing_state=5;notice("PAIR AGAIN TO APPROVE THESE SCOPES");}}transport_error(status==404?"DEVICE PAIRING NEEDS A SERVER UPDATE":status==401?"GRANT EXPIRED OR REVOKED; PAIR AGAIN":status==403?"GRANT NEEDS MORE SCOPES; RE-PAIR":"SECURE PAIRING REQUEST FAILED");return;}
    if(request_is_share){snprintf(owner->notice,sizeof(owner->notice),"%s",share_visible?"VISIBLE SPRITE SENT TO DISCORD":"ACTIVE LAYER SENT TO DISCORD");owner->redraw=true;close_transport();return;}
    if(request_is_favorite){unsigned i=gallery_thumbnail_index;owner->community_action_busy=false;if(i>=owner->community_page.count){transport_error("FAVORITE ITEM CHANGED");return;}if(strstr(body,"\"favorite\":true")){owner->community_page.items[i].favorite=true;snprintf(owner->community_status,sizeof(owner->community_status),"ADDED TO FAVORITES");}else if(strstr(body,"\"favorite\":false")){owner->community_page.items[i].favorite=false;snprintf(owner->community_status,sizeof(owner->community_status),"REMOVED FROM FAVORITES");}else{transport_error("INVALID FAVORITE RESPONSE");return;}close_transport();if(owner->community_mode==2)pd_gallery_list(owner,2,owner->gallery_page*PD_COMMUNITY_PAGE);owner->redraw=true;return;}
    if(request_is_gallery){if(request_is_thumbnail){unsigned i=gallery_thumbnail_index;if(!save_gallery_thumb(owner,i,(unsigned char*)body,body_size)){snprintf(owner->community_status,sizeof(owner->community_status),"THUMBNAIL UNAVAILABLE");}close_transport();if(i+1<owner->community_page.count&&!gallery_thumb_request(owner,i+1))snprintf(owner->community_status,sizeof(owner->community_status),"THUMBNAIL QUEUE BUSY");owner->redraw=true;return;}if(!pd_community_parse_page(body,body_size,&owner->community_page)){transport_error("INVALID COMMUNITY RESPONSE");return;}memset(owner->gallery_ready,0,sizeof(owner->gallery_ready));owner->community_loading=false;owner->community_loaded=true;owner->community_status[0]=0;owner->selected=-1;owner->preview_attempted=false;close_transport();if(gallery_request_kind!=4&&owner->community_page.count&&!gallery_thumb_request(owner,0))snprintf(owner->community_status,sizeof(owner->community_status),"THUMBNAIL QUEUE BUSY");owner->redraw=true;return;}
    if(begin){
        if(!json_string(body,"deviceCode",device_code,sizeof(device_code))||!token_format(device_code)||!json_string(body,"userCode",value,sizeof(value))||strlen(value)!=9||!json_string(body,"verificationUri",value,sizeof(value))||strcmp(value,"https://pixels.7db.pw/#account")){transport_error("INVALID PAIRING RESPONSE");return;}
        json_string(body,"userCode",owner->pairing_user_code,sizeof(owner->pairing_user_code));owner->pairing_state=1;poll_seconds=json_number(body,"interval",5);if(poll_seconds<5)poll_seconds=5;if(poll_seconds>60)poll_seconds=60;due=owner->now_ms+poll_seconds*1000;notice("CODE EXPIRES IN 10 MINUTES; APPROVE ON WEBSITE");
    }else if(json_string(body,"error",value,sizeof(value))){
        if(!strcmp(value,"authorization_pending")){poll_seconds=json_number(body,"interval",5);if(poll_seconds<5)poll_seconds=5;if(poll_seconds>60)poll_seconds=60;due=owner->now_ms+poll_seconds*1000;notice("WAITING FOR WEBSITE APPROVAL");}
        else if(!strcmp(value,"slow_down")){poll_seconds=json_number(body,"interval",poll_seconds+5);if(poll_seconds<5)poll_seconds=5;if(poll_seconds>60)poll_seconds=60;due=owner->now_ms+poll_seconds*1000;notice("WAITING FOR WEBSITE APPROVAL");}
        else{owner->pairing_state=3;memset(device_code,0,sizeof(device_code));notice(!strcmp(value,"access_denied")?"DEVICE REQUEST WAS DENIED":"PAIRING CODE EXPIRED; START AGAIN");}
    }else{
        if(!json_string(body,"accessToken",access_token,sizeof(access_token))||!token_format(access_token)||!approved_scopes(body)||!credential_write(access_token)){transport_error("DEVICE GRANT INVALID OR STORAGE FAILED");wipe(access_token,sizeof(access_token));return;}
        owner->pairing_state=2;memset(device_code,0,sizeof(device_code));notice("CONNECTED; REVOKE ACCESS ON THE WEBSITE");
    }
    close_transport();
}
void pd_pairing_init(PdApp *app){
    owner=app;wifi_ready=false;app->online_begin=pd_pairing_begin;app->online_tick=pd_pairing_tick;
    app->online_list=pd_gallery_list;
    app->online_favorite=pd_gallery_favorite;
#ifdef PD_PRIVATE_BUILD
    app->share_current=share_start;app->share_tick=share_tick;
#endif
    if(!app->directory[0]){app->online_begin=NULL;app->online_tick=NULL;app->pairing_state=4;notice("SAVED SD CARD NEEDED FOR SECURE PAIRING");return;}
    snprintf(private_dir,sizeof(private_dir),"%s/private",app->directory);
    struct stat st;if(stat(private_dir,&st)!=0){app->online_begin=NULL;app->online_tick=NULL;app->pairing_state=4;notice("PIXEL DOODLE PRIVATE FOLDER NEEDED");return;}
    if(credential_load()){app->pairing_state=5;notice("SAVED GRANT MAY EXPIRE OR BE REVOKED");}
}
void pd_pairing_begin(PdApp *app){
    if(stage!=P_IDLE)return;owner=app;memset(device_code,0,sizeof(device_code));app->pairing_user_code[0]=0;app->pairing_state=6;
    if(!wifi_ready){if(!Wifi_CheckInit()&&!Wifi_InitDefault(INIT_ONLY|WIFI_ATTEMPT_DSI_MODE)){transport_error("WI-FI INITIALIZATION FAILED");return;}Wifi_AutoConnect();}
    request_start(true);
}
void pd_pairing_tick(PdApp *app){
    owner=app;if(stage==P_IDLE){if(app->pairing_state==1&&device_code[0]&&(int32_t)(app->now_ms-due)>=0)request_start(false);return;}
    if(time(NULL)>deadline){transport_error("PAIRING NETWORK TIMED OUT; RETRY");return;}
    int r;
    switch(stage){
    case P_WIFI_INIT:{
        /* Let DSWiFi switch a DSi into TWL mode so Advanced Setup APs load.
           The library falls back to DS mode on DS hardware. */
        if(!Wifi_CheckInit()&&!Wifi_InitDefault(INIT_ONLY|WIFI_ATTEMPT_DSI_MODE)){transport_error("WI-FI INITIALIZATION FAILED");return;}
        int configs=Wifi_GetData(WIFIGETDATA_NUMWFCAPS,0,NULL);
        if(configs<=0){transport_error("NO SAVED WI-FI CONNECTION");return;}
        Wifi_AutoConnect();stage=P_WIFI;
        char status[64];snprintf(status,sizeof(status),"WAITING FOR WI-FI / DHCP (%d APS)",configs);notice(status);break;}
    case P_WIFI:r=Wifi_AssocStatus();if(r==ASSOCSTATUS_CANNOTCONNECT){transport_error("SAVED WI-FI CONNECTION FAILED");return;}if(r==ASSOCSTATUS_ASSOCIATED&&Wifi_GetIP()){wifi_ready=true;if(!start_dns())transport_error("DNS SETUP FAILED");}break;
    case P_DNS:dns_tick();break;
    case P_TCP:{fd_set w;FD_ZERO(&w);FD_SET(fd,&w);struct timeval tv={0};r=select(fd+1,NULL,&w,NULL,&tv);if(r<0){transport_error("SERVER CONNECTION FAILED");return;}if(r){int e=0;socklen_t n=sizeof(e);if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&e,&n)<0||e){transport_error("SERVER CONNECTION FAILED");return;}stage=P_TLS_SETUP;}break;}
    case P_TLS_SETUP:
        if(time(NULL)<1780272000){transport_error("SET CONSOLE DATE AND TIME");return;}
        mbedtls_ssl_init(&ssl);mbedtls_ssl_config_init(&config);mbedtls_x509_crt_init(&ca);mbedtls_entropy_init(&entropy);mbedtls_ctr_drbg_init(&rng);contexts=true;
        r=seed_rng();if(r){transport_error("TLS SEED MISSING OR INVALID");return;}
        r=mbedtls_x509_crt_parse(&ca,cacert_bin,cacert_bin_size);if(r){transport_error("TLS CERTIFICATE LOAD FAILED");return;}
        r=mbedtls_ssl_config_defaults(&config,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);if(r){transport_error("TLS CONFIGURATION FAILED");return;}
        mbedtls_ssl_conf_authmode(&config,MBEDTLS_SSL_VERIFY_REQUIRED);mbedtls_ssl_conf_ca_chain(&config,&ca,NULL);mbedtls_ssl_conf_rng(&config,mbedtls_ctr_drbg_random,&rng);r=mbedtls_ssl_setup(&ssl,&config);if(!r)r=mbedtls_ssl_set_hostname(&ssl,remote_host[0]?remote_host:PAIR_HOST);if(r){transport_error("TLS CONFIGURATION FAILED");return;}mbedtls_ssl_set_bio(&ssl,NULL,bio_send,bio_recv,NULL);stage=P_TLS;notice(remote_host[0]?"VERIFYING DISCORD CERTIFICATE":"VERIFYING PIXELS.7DB.PW CERTIFICATE");break;
    case P_TLS:r=mbedtls_ssl_handshake(&ssl);if(retry(r))break;if(r||mbedtls_ssl_get_verify_result(&ssl)){transport_error("HTTPS CERTIFICATE CHECK FAILED");return;}stage=P_WRITE;break;
    case P_WRITE:r=mbedtls_ssl_write(&ssl,(const unsigned char *)request+sent,request_size-sent);if(retry(r))break;if(r<=0){transport_error("HTTPS REQUEST SEND FAILED");return;}sent+=(size_t)r;if(sent==request_size){received=0;stage=request_is_share?P_BODY:P_READ;}break;
    case P_BODY:{const unsigned char *data=NULL;size_t available=0;if(upload_prefix_sent<upload_prefix_size){data=(const unsigned char *)upload_prefix+upload_prefix_sent;available=upload_prefix_size-upload_prefix_sent;}else if(upload_chunk_sent<upload_chunk_size){data=upload_chunk+upload_chunk_sent;available=upload_chunk_size-upload_chunk_sent;}else if(upload_png_loaded<upload_png_size){upload_chunk_size=fread(upload_chunk,1,sizeof(upload_chunk),share_file);upload_chunk_sent=0;if(!upload_chunk_size){transport_error("SPRITE FILE READ FAILED");return;}upload_png_loaded+=upload_chunk_size;data=upload_chunk;available=upload_chunk_size;}else if(upload_suffix_sent<upload_suffix_size){data=(const unsigned char *)upload_suffix+upload_suffix_sent;available=upload_suffix_size-upload_suffix_sent;}else{received=0;stage=P_READ;break;}r=mbedtls_ssl_write(&ssl,data,available);if(retry(r))break;if(r<=0){transport_error("SPRITE UPLOAD FAILED");return;}if(upload_prefix_sent<upload_prefix_size)upload_prefix_sent+=(size_t)r;else if(upload_chunk_sent<upload_chunk_size)upload_chunk_sent+=(size_t)r;else upload_suffix_sent+=(size_t)r;break;}
    case P_READ:r=mbedtls_ssl_read(&ssl,(unsigned char *)response+received,sizeof(response)-1-received);if(retry(r))break;if(r<=0){transport_error("HTTPS RESPONSE ENDED EARLY");return;}received+=(size_t)r;response[received]=0;{char *split=strstr(response,"\r\n\r\n");if(split){unsigned status=0,length=0;char *line=strstr(response,"HTTP/1.");if(!line||sscanf(line,"HTTP/1.%*u %u",&status)!=1){transport_error("INVALID HTTPS RESPONSE");return;}body_at=(size_t)(split+4-response);if(request_is_share){body_size=0;response_ready(status);return;}if(!http_content_length(response,&length)||length>=sizeof(response)-body_at){transport_error("UNSUPPORTED HTTPS RESPONSE LENGTH");return;}body_size=length;if(received>=body_at+body_size)response_ready(status);}else if(received>=sizeof(response)-1)transport_error("HTTPS HEADERS TOO LARGE");}break;
    default:close_transport();break;
    }
}
void pd_pairing_exit(void){close_transport();if(Wifi_CheckInit())Wifi_DisconnectAP();wifi_ready=false;wipe(device_code,sizeof(device_code));wipe(access_token,sizeof(access_token));owner=NULL;}
const char *pd_pairing_access_token(void){return access_token[0]?access_token:NULL;}
